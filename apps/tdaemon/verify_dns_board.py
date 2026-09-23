"""Install DNS ownership changes and verify recovery on an explicitly selected board.
Only the tdaemon interpreter is restarted; DHCP interfaces and business apps stay up.
Requires BOARD_PASSWORD. Backups are retained on the board.
"""
import argparse,hashlib,json,os,pathlib,shlex,sys,time
import paramiko
sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser();p.add_argument('--host',required=True);p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);here=pathlib.Path(__file__).resolve().parent
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy());c.connect(a.host,username='root',password=os.environ['BOARD_PASSWORD'],timeout=8)
s=c.open_sftp()
def read(path):
 with s.open(path,'rb') as f:return f.read()
def cmd(command):
 _,out,err=c.exec_command(command,timeout=40);text=out.read().decode('utf8',errors='replace');errors=err.read().decode('utf8',errors='replace')
 if out.channel.recv_exit_status():raise RuntimeError(command+'\n'+errors)
 return text
def replace_once(data,old,new):
 assert data.count(old)==1,old
 return data.replace(old,new)
paths=['/usr/bin/tdaemon.sh','/usr/bin/tdaemon_net.sh','/usr/share/udhcpc/default.script','/usr/lib/dhcpcd/dhcpcd-hooks/20-resolv.conf']
originals={p:read(p) for p in paths};updates={}
main=originals[paths[0]].decode()
main=replace_once(main,'tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_net.sh"','tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_net.sh"\ntdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_dns.sh"')
main=replace_once(main,'    cfg_apply_new\n','    cfg_apply_new\n    dns_ensure_config || true\n')
main=replace_once(main,'        cfg_reload_if_changed "$ACTIVE_CFG_PATH"','        cfg_reload_if_changed "$ACTIVE_CFG_PATH"\n        dns_ensure_config || true')
updates[paths[0]]=main.encode()
updates[paths[1]]=replace_once(originals[paths[1]].decode(),'    echo "nameserver $ACTIVE_NET_DNS_PRIMARY" > "$ACTIVE_NET_RESOLV_CONF"\n    echo "nameserver $ACTIVE_NET_DNS_SECONDARY" >> "$ACTIVE_NET_RESOLV_CONF"','    dns_ensure_config || return 1').encode()
updates[paths[2]]=replace_once(originals[paths[2]].decode(),'RESOLV_CONF="/etc/resolv.conf"','RESOLV_CONF="/etc/resolv.conf"\n[ ! -e /etc/tdaemon-dns-owner ] || RESOLV_CONF="/run/udhcpc-resolv.conf"').encode()
updates[paths[3]]=b'[ ! -e /etc/tdaemon-dns-owner ] || return 0\n'+originals[paths[3]]
updates['/usr/bin/tdaemon_dns.sh']=(here/'tdaemon_dns.sh').read_bytes()
updates['/etc/tdaemon-dns-owner']=(here/'dns-hooks/tdaemon-dns-owner').read_bytes()
backup=cmd('mktemp -d /userdata/dns-owner-backup-XXXXXX').strip()
report={'host':a.host,'backup':backup,'config_sha_before':hashlib.sha256(read('/etc/config.json')).hexdigest()}
report['original_dns']=read('/etc/resolv.conf').decode()
report['business_before']=cmd('pidof mqtt_bridge tk8710_gw || true').split()
for index,(path,data) in enumerate(updates.items()):
 cmd('if [ -f '+shlex.quote(path)+' ]; then cp -p '+shlex.quote(path)+' '+backup+'/original-'+str(index)+'; fi')
 with s.open(path+'.dns-new','wb') as f:f.write(data)
 s.chmod(path+'.dns-new',0o644 if path.endswith('tdaemon-dns-owner') or path.endswith('20-resolv.conf') else 0o755)
 if not path.endswith('tdaemon-dns-owner'):cmd('sh -n '+shlex.quote(path+'.dns-new'))
report['files']={str(i):p for i,p in enumerate(updates)}
# Publish helper and hooks before enabling the owner marker and restarting tdaemon.
for path in ['/usr/bin/tdaemon_dns.sh',paths[2],paths[3],paths[1],paths[0],'/etc/tdaemon-dns-owner']:
 cmd('mv '+shlex.quote(path+'.dns-new')+' '+shlex.quote(path))
pid=read('/var/run/tdaemon_job.pid').decode().strip();assert pid.isdigit()
assert b'/usr/bin/tdaemon.sh' in read('/proc/'+pid+'/cmdline')
cmd('kill -TERM '+pid)
for _ in range(60):
 if not cmd('test -d /proc/'+pid+' && echo alive || true').strip():break
 time.sleep(.5)
else:raise RuntimeError('old tdaemon did not exit')
cmd('/etc/init.d/S99tdaemon_job start')
for _ in range(60):
 if b'nameserver 223.5.5.5' in read('/etc/resolv.conf'):break
 time.sleep(.5)
else:raise RuntimeError('DNS not repaired after daemon startup')
good=read('/etc/resolv.conf');report['repaired_dns']=good.decode()
# A dhcpcd event must leave the resolver unchanged now. Its hook is sourced.
cmd("sh -c '. /usr/lib/dhcpcd/dhcpcd-hooks/20-resolv.conf'")
assert read('/etc/resolv.conf')==good
report['dhcpcd_hook_no_write']=True
start=time.monotonic()
try:
 with s.open('/etc/resolv.conf','wb') as f:f.write(b'# intentional DNS recovery test\n')
 for _ in range(120):
  if b'nameserver 223.5.5.5' in read('/etc/resolv.conf'):break
  time.sleep(.5)
 else:raise RuntimeError('automatic repair timed out')
 report['empty_dns_recovery_seconds']=round(time.monotonic()-start,3)
finally:
 if b'nameserver 223.5.5.5' not in read('/etc/resolv.conf'):
  with s.open('/etc/resolv.conf','wb') as f:f.write(good)
report['default_lookup']=cmd('timeout 10 nslookup amqtt.taolink-tech.com')
for _ in range(40):
 if cmd('pidof frpc || true').strip():
  time.sleep(2)
  if cmd('pidof frpc || true').strip():break
 time.sleep(.5)
report['frpc_pids']=cmd('pidof frpc || true').split()
report['frpc_tail']=cmd('tail -18 /var/log/frpc.log')
report['business_after']=cmd('pidof mqtt_bridge tk8710_gw || true').split()
report['config_sha_after']=hashlib.sha256(read('/etc/config.json')).hexdigest()
assert report['config_sha_after']==report['config_sha_before']
(a.output/'board-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(report,ensure_ascii=False,indent=2))
s.close();c.close()
