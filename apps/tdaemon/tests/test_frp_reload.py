"""Local fake FRP process test: no connection to any tunnel server."""
import os, pathlib, subprocess, tempfile, time

apps=pathlib.Path(__file__).resolve().parents[2]
def wait_for(test):
    for _ in range(100):
        if test(): return
        time.sleep(.1)
    raise AssertionError('timed out')

with tempfile.TemporaryDirectory(prefix='frp-reload-') as tmp:
    root=pathlib.Path(tmp)
    (root/'frpc.c').write_text('#include <unistd.h>\nint main(void){for(;;)pause();}\n')
    subprocess.run(['gcc',str(root/'frpc.c'),'-o',str(root/'frpc')],check=True)
    (root/'frpc.toml').write_text('name = "DEVICE_ID_STRING"\n')
    config=root/'config.json'
    config.write_text('{\n "gateway_id": "1111111111111111"\n}\n')
    env=os.environ.copy();env.update(FRP_DIR=tmp,GATEWAY_CONFIG=str(config),FRPC_LOG=str(root/'log'),LOCK_DIR=str(root/'lock'),RESTART_DELAY='1')
    watchdog=subprocess.Popen(['sh',str(apps/'frp/start_frpc.sh')],env=env)
    def child():
        try:return pathlib.Path(f'/proc/{watchdog.pid}/task/{watchdog.pid}/children').read_text().strip().split()
        except FileNotFoundError:return []
    try:
        generated=root/'frpc.generated.toml'
        wait_for(lambda: generated.exists() and '1111111111111111' in generated.read_text() and child())
        old=child()[0]
        config.write_text('{\n "gateway_id": "2222222222222222"\n}\n')
        subprocess.run(['sh','-c','. "$1"; log() { :; }; proc_reload_frp_config "$2"','test',str(apps/'tdaemon/tdaemon_proc.sh'),str(root/'frpc')],check=True)
        wait_for(lambda: '2222222222222222' in generated.read_text() and child() and child()[0]!=old)
        assert watchdog.poll() is None
        assert not pathlib.Path('/proc/'+old).exists()
        print('PASS: FRP child restarted; new gateway_id generated; watchdog retained; old child reaped')
    finally:
        watchdog.terminate()
        watchdog.wait(timeout=10)
