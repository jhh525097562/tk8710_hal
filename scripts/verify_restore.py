#!/usr/bin/env python3
"""Test restoration in a temporary sparse copy, without changing the source SDK."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = Path(sys.argv[1]).resolve()
changes = json.loads((root/'sdk/baseline/changes.json').read_text())
with tempfile.TemporaryDirectory(prefix='rk3506-restore-check-') as tmp:
    target = Path(tmp)
    for change in changes:
        rel = change['path']
        dest = target/rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git','clone','--quiet','--shared','--no-checkout',str(sdk/rel),str(dest)],check=True)
        subprocess.run(['git','-C',str(dest),'read-tree','HEAD'],check=True)
        paths=subprocess.check_output(['git','-C',str(dest),'ls-files','-z'])
        subprocess.run(['git','-C',str(dest),'update-index','--skip-worktree','-z','--stdin'],input=paths,check=True)
        # Materialize only files that the restore procedure overwrites/deletes.
        for name in change['files']+change['delete']:
            tracked=subprocess.check_output(['git','-C',str(dest),'ls-files','--',name])
            if tracked:
                subprocess.run(['git','-C',str(dest),'checkout-index','--',name],check=True)
    subprocess.run([sys.executable,str(root/'scripts/apply_sdk.py'),str(target)],check=True)
    subprocess.run([sys.executable,str(root/'scripts/apply_sdk.py'),str(target),'--apply'],check=True)
    count=0
    for change in changes:
        for name in change['files']:
            src=root/'sdk/overlay'/change['path']/name
            dst=target/change['path']/name
            if src.is_symlink(): assert dst.is_symlink() and os.readlink(src)==os.readlink(dst)
            else: assert src.read_bytes()==dst.read_bytes(), str(dst)
            count+=1
        for name in change['delete']:
            dst=target/change['path']/name
            assert not dst.exists() and not dst.is_symlink()
    # Refuse a repeated apply into an existing customization checkout.
    repeat=subprocess.run([sys.executable,str(root/'scripts/apply_sdk.py'),str(target)],capture_output=True)
    assert repeat.returncode!=0
    print(f'PASS restore: {count} files and deletion manifest; repeated apply refused')
