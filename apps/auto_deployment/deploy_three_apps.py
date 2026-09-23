"""Deploy the three local ARM builds, preserving board configuration and backups."""
import hashlib,json,pathlib,shlex,time,sys
import paramiko

root=pathlib.Path(__file__).resolve().parents[1]
files={'mqtt_bridge':root/'mqtt/build_arm/mqtt_bridge','web_server':root/'webserver/build_arm/web_server',
       'tk8710_gw':pathlib.Path('D:/CodexWorkSpace/tk8710_hal_v1.6/build_rk3506/tk8710_gw')}
run=time.strftime('deploy-%Y%m%d-%H%M%S');local=root/'auto_deployment'/run;local.mkdir()
for path in files.values():
    data=path.read_bytes();assert data[:4]==b'\x7fELF' and data[18:20]==b'\x28\x00',path
for host in sys.argv[1:]:
    c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy());c.connect(host,username='root',password=__import__("os").environ["RK3506_SSH_PASSWORD"],timeout=10)
    s=c.open_sftp();backup='/userdata/'+run;s.mkdir(backup,0o700)
    report={'host':host,'backup':backup,'hashes':{},'commands':{}}
    def cmd(x):
        _,o,e=c.exec_command(x,timeout=20);out=o.read().decode('utf8','replace');err=e.read().decode('utf8','replace');code=o.channel.recv_exit_status()
        if code:raise RuntimeError(x+': '+err+out)
        return out.strip()
    def live(name):
        pids=cmd('pidof '+name+' || true').split();result=[]
        for pid in pids:
            try:
                text=s.open('/proc/'+pid+'/stat').read().decode();state=text[text.rfind(')')+2]
                if state!='Z': result.append(pid)
            except OSError:pass
        return result
    guards=[];started=[];replaced=[];saved={}
    try:
        for name,path in files.items():
            original=s.open('/userdata/'+name,'rb').read()
            with s.open(backup+'/'+name,'wb') as f:f.write(original)
            s.chmod(backup+'/'+name,0o755)
            s.put(str(path),backup+'/'+name+'.new');s.chmod(backup+'/'+name+'.new',0o755)
            expected=hashlib.sha256(path.read_bytes()).hexdigest()
            assert hashlib.sha256(s.open(backup+'/'+name+'.new','rb').read()).hexdigest()==expected
            report['hashes'][name]=expected
            pids=live(name);assert len(pids)<=1,(name,pids)
            argv=[]
            if pids:
                assert s.readlink('/proc/'+pids[0]+'/exe')=='/userdata/'+name
                argv=s.open('/proc/'+pids[0]+'/cmdline').read().decode().strip('\0').split('\0')[1:]
            elif name=='tk8710_gw':argv=['-w','/userdata/','-g','0x2e','--driver-log-level','warn','--trm-log-level','warn']
            saved[name]=(pids,argv)
        for name in ['config.json','tdaemon.conf']:
            data=s.open('/etc/'+name,'rb').read()
            report[name+'_hash']=hashlib.sha256(data).hexdigest()
            with s.open(backup+'/'+name,'wb') as f:f.write(data)
        guards=cmd('pidof tdaemon.sh || true').split()
        if guards:cmd('kill -STOP '+' '.join(guards))
        for name in ['tk8710_gw','mqtt_bridge','web_server']:
            for pid in live(name):cmd('kill -TERM '+pid+' 2>/dev/null || true')
        for _ in range(20):
            if not any(live(n) for n in files):break
            time.sleep(1)
        for name in files:
            remaining=live(name)
            if remaining:
                for pid in remaining:assert s.readlink('/proc/'+pid+'/exe')=='/userdata/'+name
                cmd('kill -KILL '+' '.join(remaining));report.setdefault('forced_stops',{})[name]=remaining
        time.sleep(1)
        for name in files:
            s.posix_rename(backup+'/'+name+'.new','/userdata/'+name);replaced.append(name)
        for name in ['mqtt_bridge','web_server','tk8710_gw']:
            line=shlex.join(['/userdata/'+name]+saved[name][1]);report['commands'][name]=line
            pid=cmd('cd /userdata; nohup '+line+' >'+backup+'/'+name+'.log 2>&1 </dev/null & echo $!');started.append(pid)
        time.sleep(5)
        report['initial_processes']={name:live(name) for name in files}
        for name in files:
            assert report['initial_processes'][name],name+' exited'
            pid=report['initial_processes'][name][0]
            assert cmd('sha256sum /proc/'+pid+'/exe').split()[0]==report['hashes'][name]
        report['deployed']=True
    except Exception as e:
        report['error']=str(e)
        if replaced:
            for name in files:
                for pid in live(name):cmd('kill -TERM '+pid+' 2>/dev/null || true')
            time.sleep(5)
            for name in files:
                for pid in live(name):cmd('kill -KILL '+pid+' 2>/dev/null || true')
            for name in replaced:
                cmd('cp '+backup+'/'+name+' /userdata/'+name+'.rollback && mv /userdata/'+name+'.rollback /userdata/'+name)
            for name,(pids,argv) in saved.items():
                if pids:cmd('nohup '+shlex.join(['/userdata/'+name]+argv)+' >/tmp/'+name+'-rollback.log 2>&1 </dev/null &')
            report['rolled_back']=True
    finally:
        if guards:cmd('kill -CONT '+' '.join(guards)+' 2>/dev/null || true')
        for name in ['config.json','tdaemon.conf']:
            if name+'_hash' in report:assert hashlib.sha256(s.open('/etc/'+name,'rb').read()).hexdigest()==report[name+'_hash']
        (local/(host+'.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
        print(json.dumps(report),flush=True);s.close();c.close()
    if not report.get('deployed'):sys.exit(1)
print('REPORT',local)
