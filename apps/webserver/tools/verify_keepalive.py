import http.client, urllib.parse, time, json, pathlib, paramiko, hashlib

base=pathlib.Path(__file__).resolve().parents[1]
binary=base/'build_arm/web_server'
run='keepalive-'+time.strftime('%Y%m%d-%H%M%S')
out=base/'docs'/run;out.mkdir(parents=True)
for host in ['192.168.200.106','192.168.100.63']:
    c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy());c.connect(host,username='root',password=__import__("os").environ["RK3506_SSH_PASSWORD"],timeout=8)
    s=c.open_sftp();cfg=json.loads(s.open('/etc/config.json').read());guards=[];report={'host':host}
    def cmd(text):
        _,o,e=c.exec_command(text,timeout=15);data=o.read().decode();err=e.read().decode();assert o.channel.recv_exit_status()==0,err
        return data.strip()
    backup='/userdata/'+run; s.mkdir(backup)
    s.put(str(binary),backup+'/web_server.new');s.chmod(backup+'/web_server.new',0o755)
    cmd('cp -p /userdata/web_server '+backup+'/web_server.old')
    try:
        guards=cmd('pidof tdaemon.sh || true').split()
        if guards:cmd('kill -STOP '+' '.join(guards))
        for pid in cmd('pidof web_server').split():
            assert s.readlink('/proc/'+pid+'/exe')=='/userdata/web_server'
            cmd('kill -TERM '+pid)
        for _ in range(15):
            if not cmd('ss -lnt | grep ":8090 " || true'):break
            time.sleep(1)
        assert not cmd('ss -lnt | grep ":8090 " || true')
        s.posix_rename(backup+'/web_server.new','/userdata/web_server')
        cmd('nohup /userdata/web_server >'+backup+'/web.log 2>&1 </dev/null &')
        time.sleep(2)
        conn=http.client.HTTPConnection(host,8090,timeout=5)
        conn.request('POST','/login',urllib.parse.urlencode({'username':'admin','password':cfg.get('web_password','admin')}),{'Content-Type':'application/x-www-form-urlencoded'})
        r=conn.getresponse();cookie=r.getheader('Set-Cookie');r.read();assert r.status==302
        timings=[]
        for path in ['/status','/api/system_info','/config','/status','/network','/status','/rf','/status']*5:
            before=time.monotonic();conn.request('GET',path,headers={'Cookie':cookie});r=conn.getresponse();data=r.read()
            assert r.status==200 and data
            timings.append({'path':path,'seconds':round(time.monotonic()-before,4),'bytes':len(data)})
        conn.close();report.update(passed=True,requests=timings,max_seconds=max(x['seconds'] for x in timings),backup=backup,sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
    finally:
        if guards:cmd('kill -CONT '+' '.join(guards)+' 2>/dev/null || true')
        (out/(host+'.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
        s.close();c.close()
    print(host,'PASS',len(timings),'requests, max',report['max_seconds'],flush=True)
print(out)
