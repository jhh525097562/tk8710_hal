import hashlib, os, pathlib, shutil, subprocess, tempfile
here=pathlib.Path(__file__).resolve().parent
names=['mqtt_bridge','web_server','tk8710_gw']
def run_case(mode):
 with tempfile.TemporaryDirectory(prefix='swu-transaction-') as tmp:
  root=pathlib.Path(tmp);(root/'userdata').mkdir();(root/'etc').mkdir();(root/'var/run').mkdir(parents=True)
  for name in names:(root/'userdata'/name).write_text('old-'+name)
  (root/'etc/device_version.json').write_text('{"device":"keep","app_version":"old"}')
  config=b'{"gateway_id":"unchanged"}';(root/'etc/config.json').write_bytes(config)
  env=os.environ.copy();env.update(APP_UPGRADE_TEST_ROOT=tmp,PACKAGE_VERSION='test-1',REQUIRED_KB='1')
  command=['sh',str(here/'app_upgrade.sh')]
  subprocess.run(command+['preinst'],env=env,check=True)
  # A rejected second updater cannot clear the first updater's transaction lock.
  other=env.copy();other['APP_UPGRADE_OWNER']='other-updater'
  assert subprocess.run(command+['preinst'],env=other).returncode!=0
  subprocess.run(command+['recover'],env=other,check=True)
  assert (root/'var/run/app-upgrade-lock').is_dir()
  incoming=root/'userdata/.app-upgrade/transaction/incoming'
  for name in names:(incoming/name).write_text('new-'+name)
  # Fake metadata helper isolates transaction semantics from JSON-C in this test.
  (incoming/'metadata').write_text('#!/bin/sh\ncp "$1" "$2"\n')
  env['PACKAGE_HASHES']='\n'.join(hashlib.sha256((incoming/n).read_bytes()).hexdigest()+'  '+n for n in names+['metadata'])
  if mode=='corrupt':(incoming/'mqtt_bridge').write_text('bad')
  if mode=='failure':env['APP_UPGRADE_FAIL_AFTER']='web_server'
  if mode=='crash':env['APP_UPGRADE_CRASH_AFTER']='web_server'
  completed=subprocess.run(command+['postinst'],env=env)
  if mode=='ok':assert completed.returncode==0
  else:assert completed.returncode!=0
  subprocess.run(command+['recover'],env=env,check=True)
  for name in names:assert (root/'userdata'/name).read_text()==('new-' if mode=='ok' else 'old-')+name
  assert (root/'etc/config.json').read_bytes()==config
  assert not (root/'var/run/app-upgrade-maintenance').exists()
  assert not (root/'var/run/app-upgrade-lock').exists()
  print('PASS',mode,flush=True)
for mode in ['ok','corrupt','failure','crash']:run_case(mode)
