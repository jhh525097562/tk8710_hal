"""Build fixed three-application CPIO SWU; run in WSL, never installs it."""
import argparse, hashlib, json, pathlib, re, shlex, shutil, subprocess, tempfile

p=argparse.ArgumentParser()
p.add_argument('--sdk',default='/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0')
p.add_argument('--version',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_.-]{1,48}',a.version):p.error('version must be 1-48 safe ASCII characters')
sdk=pathlib.Path(a.sdk).resolve();here=pathlib.Path(__file__).resolve().parent
target=pathlib.Path(a.output).resolve();target.parent.mkdir(parents=True,exist_ok=True)
if target.exists():p.error('output already exists; choose a new release filename')
host=sdk/'buildroot/output/rockchip_hd_rk3506b_iot_emmc/host'
cc=host/'bin/arm-buildroot-linux-gnueabihf-gcc'
source=sdk/'device/rockchip/common/extra-parts/userdata/normal'
names=['mqtt_bridge','web_server','tk8710_gw']
rootfs=sdk/'buildroot/output/rockchip_hd_rk3506b_iot_emmc/target'
readelf=host/'bin/arm-buildroot-linux-gnueabihf-readelf'
def dependencies(binary, seen=None):
 seen=set() if seen is None else seen
 text=subprocess.check_output([str(readelf),'-d',str(binary)],text=True)
 for lib in re.findall(r'\(NEEDED\).*\[(.*?)\]',text):
  if lib in seen:continue
  found=next((rootfs/sub/lib for sub in ['lib','usr/lib'] if (rootfs/sub/lib).is_file()),None)
  if found is None:raise ValueError('missing target library: '+lib+' required by '+str(binary))
  seen.add(lib);dependencies(found,seen)
 return sorted(seen)
with tempfile.TemporaryDirectory(prefix='gateway-swu-') as tmp:
 d=pathlib.Path(tmp)
 for name in names:
  b=(source/name).read_bytes()
  if b[:4]!=b'\x7fELF' or b[4]!=1 or b[5]!=1 or b[18:20]!=b'\x28\x00':raise ValueError('not ARM ELF: '+name)
  shutil.copy2(source/name,d/name)
 subprocess.run([str(cc),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',str(here/'metadata.c'),'-ljson-c','-o',str(d/'metadata')],check=True)
 artifacts=names+['metadata']
 needed={n:dependencies(d/n) for n in artifacts}
 hashes={n:hashlib.sha256((d/n).read_bytes()).hexdigest() for n in artifacts}
 checks='\n'.join(h+'  '+n for n,h in hashes.items())
 size=sum((d/n).stat().st_size for n in artifacts)
 preamble='#!/bin/sh\nPACKAGE_VERSION='+shlex.quote(a.version)+'\nREQUIRED_KB='+str((size*4)//1024+16384)+'\nPACKAGE_HASHES='+shlex.quote(checks)+'\n'
 (d/'install.sh').write_text(preamble+here.joinpath('app_upgrade.sh').read_text().split('\n',1)[1],encoding='utf8')
 files=',\n'.join('{ filename="'+n+'"; path="/userdata/.app-upgrade/transaction/incoming/'+n+'"; }' for n in artifacts)
 (d/'sw-description').write_text('software = { version="'+a.version+'"; stable = { copy1 = { files = (\n'+files+'\n); scripts = ({ filename="install.sh"; type="shellscript"; }); }; }; };\n')
 subprocess.run(['sh','-n',str(d/'install.sh')],check=True)
 members=['sw-description','install.sh']+artifacts
 with target.open('xb') as f:
  subprocess.run(['cpio','-o','-H','crc','--quiet'],cwd=d,input=('\n'.join(members)+'\n').encode(),stdout=f,check=True)
 manifest={'version':a.version,'members':members,'sha256':hashes,'target_dependencies':needed,'bytes':target.stat().st_size,'package_sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
 target.with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2))
print(target)
print(json.dumps(manifest,indent=2))
