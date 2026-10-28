"""Ensure fatal lifecycle records survive discarded daemon console output."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include "tk8710_gw_status.h"
#include "trm/trm_log.h"
int main(void) {
    if (TRM_LogConfig(TRM_LOG_WARN, 1) || GwStatusInit("state")) return 2;
    GwStatusSet(GW_STATUS_RUNNING, "business_started", 0);
    GwStatusSet(GW_STATUS_FAULT, "md_data_irq_stalled", -1);
    GwStatusSet(GW_STATUS_FAULT, "gateway_shutdown_error", 1);
    GwStatusFinish(1);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="gw-fault-log-") as temporary:
    directory = Path(temporary)
    source = directory / 'test.c'
    source.write_text(SOURCE)
    executable = directory / 'test'
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-pthread',
        '-DPLATFORM_RK3506', '-I'+str(ROOT/'inc'), '-I'+str(ROOT/'src/gateway'),
        str(source), str(ROOT/'src/gateway/tk8710_gw_status.c'),
        str(ROOT/'src/trm/trm_log.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], cwd=directory, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    logs = '\n'.join(path.read_text() for path in (directory/'8710log').glob('trm_log_*.log'))
    assert 'Gateway fault: reason=md_data_irq_stalled error=-1 pid=' in logs
    assert 'Gateway fault: reason=gateway_shutdown_error' not in logs
    status = json.loads((directory/'state/last_exit.json').read_text())
    assert status['exited'] and status['reason'] == 'md_data_irq_stalled'
    print('PASS fatal file log and last_exit.json with stdout/stderr discarded')
