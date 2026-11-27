#!/usr/bin/env python3
"""Compare original App/SDK with this Git checkout; copy only on explicit --apply.

Run under WSL/Linux. Sources are read-only. No git add/commit/push is performed.
"""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from sync_policy import ABSENT, DELETED, eligible, safe_path, snapshot, canonical, fingerprint, secret_findings

ROOT = Path(__file__).resolve().parents[1]

def git(repo, *args, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], capture_output=True)
    if check and result.returncode:
        raise RuntimeError(f'Git operation failed in {repo}: {args[0]} (exit {result.returncode})')
    return result

def read_json(path, default=None):
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else default

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix='.sync-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)

def walk(root):
    for base, dirs, names in os.walk(root, followlinks=False):
        dirs[:] = [d for d in dirs if not (Path(base)/d).is_symlink() and
                   eligible((Path(base)/d).relative_to(root).as_posix())]
        for name in names:
            path = Path(base)/name
            relative = path.relative_to(root).as_posix()
            if eligible(relative): yield relative

def selected(key, selectors):
    return not selectors or any(key == s or key.startswith(s.rstrip('/')+'/') for s in selectors)

def app_names(root, selectors):
    if not selectors or 'apps' in selectors:
        return set(walk(root))
    names = set()
    for selector in selectors:
        if not selector.startswith('apps/'): continue
        rel = selector[5:]
        target = safe_path(root, rel)
        if target.is_symlink() or target.is_file():
            if eligible(rel): names.add(rel)
        elif target.is_dir():
            names.update(rel+'/'+name for name in walk(target) if eligible(rel+'/'+name))
    return names

def reconcile(current, previous=None, resolution=None):
    """Return proposed side and disposition; absence is a value, not an error."""
    if resolution:
        if resolution not in current:
            raise ValueError(f'Unavailable resolution side: {resolution}')
        return resolution, 'keep-repo' if resolution == 'repo' else 'resolved'
    sources = {k: v for k, v in current.items() if k != 'repo'}
    if previous is None:
        present = {k: v for k, v in sources.items() if v != ABSENT}
        if not present: return None, 'repo-only'
        values = list(present.values())
        if all(v == current['repo'] for v in values): return None, 'unchanged'
        if current['repo'] == ABSENT and all(v == values[0] for v in values):
            return next(iter(present)), 'add'
        return None, 'initial-conflict'
    if not previous['accepted']:
        present = [v for k, v in current.items() if k != 'repo' and v != ABSENT]
        if present and all(v == current['repo'] for v in present): return None, 'unchanged'
        return None, 'initial-conflict'
    old = previous['values']
    changed = {k: v for k, v in current.items() if v != old.get(k, ABSENT)}
    if not changed: return None, 'unchanged'
    values = list(changed.values())
    if not all(v == values[0] for v in values): return None, 'conflict'
    if 'repo' in changed: return None, 'repo-only' if len(changed) == 1 else 'converged'
    return next(iter(changed)), 'update'

class Inventory:
    def __init__(self, repo, app, sdk, state, selectors, progress=None):
        self.repo, self.app, self.sdk = repo, app, sdk
        self.entries, self.warnings = {}, []
        self.manifest_path = safe_path(repo, 'sdk/baseline/changes.json')
        if self.manifest_path.is_symlink(): raise ValueError('SDK manifest must not be a symlink')
        self.manifest_raw = self.manifest_path.read_bytes()
        self.manifest = json.loads(self.manifest_raw)
        self.projects = read_json(repo/'sdk/baseline/projects.json')
        self.state = state or {'entries': {}}
        self.selectors = selectors
        self.progress = progress or (lambda message: None)

    def add(self, key, paths, values=None, project=None, name=None):
        if not selected(key, self.selectors): return
        actual = {side: snapshot(path) for side, path in paths.items()}
        current = dict(actual)
        if values: current.update(values)
        self.entries[key] = dict(paths=paths, actual=actual, values=current,
                                 project=project, name=name)

    def collect(self):
        sdk_apps = self.sdk/'external/rk3506app'
        if not self.app.is_dir() or not sdk_apps.is_dir():
            raise RuntimeError('App source directory missing; refusing to interpret it as file deletions')
        roots = {'local': self.app, 'sdk': sdk_apps,
                 'repo': safe_path(self.repo, 'apps/.anchor').parent}
        if not self.selectors or any(s == 'apps' or s.startswith('apps/') for s in self.selectors):
            names = set()
            for side, root in roots.items():
                self.progress('Scanning App paths: '+side)
                names.update(app_names(root, self.selectors))
            names.update(k[5:] for k in self.state['entries'] if k.startswith('apps/') and selected(k, self.selectors))
            for index, name in enumerate(sorted(names)):
                if index % 250 == 0: self.progress(f'Comparing App fingerprints: {index}/{len(names)}')
                if eligible(name): self.add('apps/'+name, {s: safe_path(p, name) for s, p in roots.items()})
        for project in self.projects:
            prefix = 'sdk/overlay/'+project['path']
            if self.selectors and not any(selected(prefix, [s]) or selected(s, [prefix]) for s in self.selectors): continue
            self.progress('Checking SDK baseline: '+project['path'])
            source = safe_path(self.sdk, project['path'])
            base = project['commit']
            record = next((m for m in self.manifest if m['path'] == project['path']), None)
            if record and record['commit'] != base:
                raise RuntimeError(f'Manifest/baseline mismatch: {project["path"]}')
            probe = git(source, 'cat-file', '-e', base+'^{tree}', check=False)
            if probe.returncode:
                # An unchanged unavailable vendor project is visible, never interpreted as deletion.
                if record and (record['files'] or record['delete']):
                    raise RuntimeError(f'Cannot inspect recorded SDK customization: {project["path"]}')
                warning = 'Unavailable vendor baseline: '+project['path']
                if any(s == prefix or s.startswith(prefix+'/') for s in self.selectors):
                    raise RuntimeError(warning)
                self.warnings.append(warning); continue
            names = set(git(source, 'diff', '--name-only', '-z', base).stdout.decode().split('\0'))
            names.update(git(source, 'ls-files', '--others', '--exclude-standard', '-z').stdout.decode().split('\0'))
            if record: names.update(record['files'] + record['delete'])
            names.update(k[len(prefix)+1:] for k in self.state['entries'] if k.startswith(prefix+'/'))
            # Also include manually added overlay files absent from changes.json.
            overlay = safe_path(self.repo, prefix+'/.anchor').parent
            if overlay.is_dir(): names.update(walk(overlay))
            for name in sorted({name.rstrip('/') for name in names} - {''}):
                key = prefix+'/'+name
                if not eligible(name) or not selected(key, self.selectors): continue
                src, dst = safe_path(source, name), safe_path(overlay, name)
                mode = git(source, 'ls-tree', base, '--', name).stdout
                if mode.startswith(b'160000') or (src.is_dir() and not src.is_symlink()):
                    subproject = project['path']+'/'+name
                    if not any(p['path'] == subproject for p in self.projects):
                        raise RuntimeError(f'Nested repository not recorded in projects.json: {subproject}')
                    self.warnings.append('Submodule pointer not copied; managed by subproject baseline: '+subproject)
                    continue
                src_value, dst_value = snapshot(src), snapshot(dst)
                blob = git(source, 'show', base+':'+name, check=False)
                base_value = (fingerprint(blob.stdout, 'symlink' if mode.startswith(b'120000') else 'file', name)
                              if not blob.returncode else ABSENT)
                if src_value == ABSENT and base_value != ABSENT: src_value = DELETED.copy()
                elif src_value == base_value: src_value = ABSENT.copy()
                if record and name in record['delete']:
                    if dst_value != ABSENT: raise RuntimeError(f'Overlay exists but manifest marks deletion: {key}')
                    dst_value = DELETED.copy()
                if record and name in record['files'] and dst_value == ABSENT:
                    raise RuntimeError(f'Listed overlay file missing: {key}; fix manifest or restore file first')
                self.add(key, {'sdk': src, 'repo': dst}, {'sdk': src_value, 'repo': dst_value}, project, name)
        return self

def make_plan(inventory, state, resolutions, allow_delete):
    rows = []
    for key, entry in inventory.entries.items():
        current = entry['values']
        side, action = reconcile(current, state['entries'].get(key) if state else None, resolutions.get(key))
        row = dict(path=key, action=action, source=side, values=current, issues=[])
        if side:
            value = current[side]
            if value['kind'] == 'oversize': row['issues'].append('file-over-40MiB')
            if value['kind'] == 'file':
                row['issues'] += secret_findings(entry['paths'][side].read_bytes(), key)
            if value['kind'] == 'symlink' and key.startswith('apps/'):
                row['issues'].append('app-symlink-requires-manual-review')
            if side != 'repo' and value['kind'] in {'absent', 'delete'} and current['repo'] != value and not allow_delete:
                row['issues'].append('requires-allow-delete')
        rows.append(row)
    unknown = set(resolutions) - set(inventory.entries)
    if unknown: raise ValueError('Resolution paths not in selected inventory: '+', '.join(sorted(unknown)))
    return rows

def apply_plan(inventory, state, rows, state_path, run):
    blockers = [r for r in rows if r['issues'] or r['action'] in {'conflict', 'initial-conflict'}]
    if blockers: raise RuntimeError(f'{len(blockers)} selected conflicts/blocked files; nothing copied')
    # Recheck all selected inputs and manifest before touching the checkout.
    for entry in inventory.entries.values():
        if any(snapshot(path) != entry['actual'][side] for side, path in entry['paths'].items()):
            raise RuntimeError('Files changed during scan; rerun')
    if inventory.manifest_path.read_bytes() != inventory.manifest_raw:
        raise RuntimeError('SDK manifest changed during scan; rerun')
    changed = [r for r in rows if r['source'] not in {None, 'repo'} and
               r['values'][r['source']] != r['values']['repo']]
    targets = [inventory.entries[r['path']]['paths']['repo'] for r in changed]
    targets += [inventory.manifest_path, state_path]
    saved = []
    for index, target in enumerate(targets):
        backup = run/'backup'/str(index)
        backup.parent.mkdir(parents=True, exist_ok=True)
        existed = target.exists() or target.is_symlink()
        if existed: shutil.copy2(target, backup, follow_symlinks=False)
        saved.append((target, backup, existed))
    write_json(run/'backup-index.json', [{'path':str(t), 'backup':str(b), 'existed':x} for t,b,x in saved])
    try:
        for row in changed:
            key, side = row['path'], row['source']
            entry = inventory.entries[key]
            dst, src, value = entry['paths']['repo'], entry['paths'][side], row['values'][side]
            dst.parent.mkdir(parents=True, exist_ok=True)
            if value['kind'] in {'delete', 'absent'}:
                if dst.exists() or dst.is_symlink(): dst.unlink()
            elif value['kind'] == 'symlink':
                link = str(src.readlink())
                if fingerprint(link.encode(), 'symlink') != value:
                    raise RuntimeError(f'Symlink changed during copy: {key}')
                if dst.exists() or dst.is_symlink(): dst.unlink()
                dst.symlink_to(link)
            else:
                data = src.read_bytes()
                # Detect a writer changing a source after the batch preflight.
                if fingerprint(data, name=src.name) != value:
                    raise RuntimeError(f'Source changed during copy: {key}')
                mode = dst.stat().st_mode & 0o777 if dst.exists() and not dst.is_symlink() else (
                    0o755 if data.startswith((b'#!', b'\x7fELF')) and '.so' not in dst.name else 0o644)
                fd, tmp = tempfile.mkstemp(prefix='.sync-', dir=dst.parent)
                try:
                    with os.fdopen(fd, 'wb') as stream: stream.write(canonical(data, dst.name))
                    os.chmod(tmp, mode); os.replace(tmp, dst)
                finally:
                    if os.path.exists(tmp): os.unlink(tmp)
            row['values']['repo'] = value
        for row in rows:
            # Unacknowledged Git-only edits must remain detectable next time.
            if row['action'] == 'repo-only': continue
            entry = inventory.entries[row['path']]
            if entry['project']:
                project, name, value = entry['project'], entry['name'], row['values']['repo']
                record = next((x for x in inventory.manifest if x['path'] == project['path']), None)
                if record is None:
                    record = dict(path=project['path'], commit=project['commit'], files=[], delete=[])
                    inventory.manifest.append(record)
                record['files'] = sorted(set(record['files']) - {name})
                record['delete'] = sorted(set(record['delete']) - {name})
                if value['kind'] == 'delete': record['delete'].append(name)
                elif value['kind'] != 'absent': record['files'].append(name)
                record['files'].sort(); record['delete'].sort()
            state['entries'][row['path']] = {'accepted': True, 'values': row['values']}
        new_manifest = sorted(inventory.manifest, key=lambda x:x['path'])
        if new_manifest != json.loads(inventory.manifest_raw): write_json(inventory.manifest_path, new_manifest)
        write_json(state_path, state)
    except BaseException:
        for target, backup, existed in reversed(saved):
            if target.exists() or target.is_symlink(): target.unlink()
            if existed: shutil.copy2(backup, target, follow_symlinks=False)
        raise
    return len(changed)

@contextmanager
def lock(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as stream:
        try: fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError: raise RuntimeError('Another sync process is running')
        yield

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app-root', type=Path, default=Path('/mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app'))
    parser.add_argument('--sdk-root', type=Path, default=Path('/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0'))
    parser.add_argument('--repo-root', type=Path, default=ROOT)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--init', action='store_true', help='Record fingerprints only; do not resolve initial differences')
    group.add_argument('--apply', action='store_true', help='Apply selected non-conflicting updates with backups')
    parser.add_argument('--select', action='append', default=[], metavar='REPO_PATH', help='Repeat to select directories/files')
    parser.add_argument('--resolve', action='append', default=[], metavar='PATH=local|sdk|repo')
    parser.add_argument('--allow-delete', action='store_true')
    args = parser.parse_args()
    repo, app, sdk = (p.resolve(strict=True) for p in (args.repo_root,args.app_root,args.sdk_root))
    for source in (app, sdk):
        if repo == source or repo.is_relative_to(source) or source.is_relative_to(repo):
            raise ValueError('Source and destination roots must not overlap')
    if os.name != 'posix': parser.error('Run this script under WSL/Linux')
    for selector in args.select:
        safe_path(repo, selector)
        if not (selector == 'apps' or selector.startswith('apps/') or selector == 'sdk/overlay' or selector.startswith('sdk/overlay/')):
            parser.error('--select must be apps/... or sdk/overlay/...')
    if args.init and (args.select or args.resolve): parser.error('--init records the full inventory; no select/resolve')
    resolutions = {}
    for resolution in args.resolve:
        key, sep, side = resolution.rpartition('=')
        if not sep or side not in {'local','sdk','repo'}: parser.error('Invalid --resolve')
        safe_path(repo, key); resolutions[key] = side
    cache = repo/'.sync'
    if cache.is_symlink(): raise RuntimeError('.sync must not be a symlink')
    with lock(cache/'sync.lock'):
        state_path = cache/'state.json'
        state = read_json(state_path)
        identity = {'repo':str(repo),'app':str(app),'sdk':str(sdk)}
        if state and (state.get('version') != 1 or state.get('roots') != identity):
            raise RuntimeError('State roots/version mismatch; use the original roots or archive .sync and initialize again')
        if args.init and state: raise RuntimeError('Baseline already exists; refusing to replace it')
        if args.apply and not state: raise RuntimeError('Run --init once before applying changes')
        inventory = Inventory(repo, app, sdk, state, args.select, lambda message: print(message, flush=True)).collect()
        if args.select and not inventory.entries: raise RuntimeError('Selected path matched no eligible files')
        rows = make_plan(inventory, state, resolutions, args.allow_delete)
        run = cache/'runs'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        report = {'roots': identity, 'warnings': inventory.warnings, 'files': rows}
        write_json(run/'report.json', report)
        interesting = [r for r in rows if r['action'] != 'unchanged' or r['issues']]
        lines = ['# 增量同步报告', '', '只列路径、状态和规则，不记录文件内容或密码。', '']
        for row in interesting:
            lines.append(f'- `{row["path"]}`: {row["action"]}; source={row["source"]}; issues={",".join(row["issues"])}')
        lines += ['', '## 检查限制', ''] + ['- '+w for w in inventory.warnings]
        (run/'report.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')
        print(f'Report: {run}/report.md\nFiles: {len(rows)}; differences: {len(interesting)}')
        for row in interesting[:25]: print(row['action'],row['path'],','.join(row['issues']))
        if args.init:
            records = {key: {'accepted': row['action'] == 'unchanged', 'values': row['values']}
                       for key,row in ((r['path'],r) for r in rows)}
            write_json(state_path, {'version':1,'roots':identity,'entries':records})
            print('Baseline recorded. Initial differences remain unresolved. No source files copied.')
        elif args.apply:
            count=apply_plan(inventory,state,rows,state_path,run)
            print(f'Applied {count} files; backups: {run}/backup-index.json. No Git commit/push.')
        else:
            if state:
                print('Preview only. Baseline exists; use --apply with --select and explicit resolutions as needed.')
            else:
                print('Preview only. Run --init once before using --apply.')
        return 2 if not args.init and any(r['issues'] or r['action'] in {'conflict','initial-conflict'} for r in rows) else 0

if __name__ == '__main__':
    try: sys.exit(main())
    except (RuntimeError, ValueError, OSError) as error:
        print(f'ERROR: {error}', file=sys.stderr); sys.exit(1)
