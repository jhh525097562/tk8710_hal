#!/usr/bin/env python3
"""Restore customization into a matching, clean vendor SDK. Check by default."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args]).decode().strip()

def contained(root, relative):
    rel = Path(relative)
    if rel.is_absolute() or '..' in rel.parts:
        raise ValueError(f'Unsafe relative path: {relative}')
    target = root / rel
    # Reject directory symlinks that would write outside the selected SDK.
    if not target.parent.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'Path escapes SDK: {relative}')
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sdk', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    sdk = args.sdk.resolve(strict=True)
    changes = json.loads((ROOT/'sdk/baseline/changes.json').read_text())
    operations = []
    for change in changes:
        repo = contained(sdk, change['path'])
        if git(repo, 'rev-parse', 'HEAD') != change['commit']:
            raise RuntimeError(f'Baseline mismatch: {repo}')
        if git(repo, 'status', '--porcelain', '--untracked-files=no'):
            raise RuntimeError(f'Tracked local changes: {repo}; use a clean SDK copy')
        for name in change['files']:
            source = ROOT/'sdk/overlay'/change['path']/name
            target = contained(repo, name)
            if target.exists() or target.is_symlink():
                if not git(repo, 'ls-files', '--', name):
                    raise RuntimeError(f'Untracked target would be overwritten: {target}')
            operations.append((source, target))
        for name in change['delete']:
            operations.append((None, contained(repo, name)))
    app_target = sdk/'external/rk3506app'
    if app_target.exists():
        raise RuntimeError(f'App target already exists: {app_target}; use a clean SDK copy')
    print(f'Baseline/check passed: {len(changes)} repos, {len(operations)} file operations')
    if not args.apply:
        print('No writes. Add --apply to restore into this clean SDK.')
        return
    for source, target in operations:
        if source is None:
            if target.exists() or target.is_symlink(): target.unlink()
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_symlink():
            if target.exists() or target.is_symlink(): target.unlink()
            target.symlink_to(os.readlink(source))
        else:
            if target.is_symlink(): target.unlink()
            shutil.copy2(source, target)
    shutil.copytree(ROOT/'apps', app_target, symlinks=True,
                    ignore=shutil.ignore_patterns('build_sdk', 'build_arm', '__pycache__', 'logs'))
    print('SDK customization restored. Supply private deployment configuration before packaging.')

if __name__ == '__main__':
    main()
