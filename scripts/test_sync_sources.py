#!/usr/bin/env python3
"""Isolated regression tests: fake SDK repos, no original SDK writes/network."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import sync_sources as sync
from sync_policy import ABSENT, eligible, safe_path, secret_findings, snapshot

class SyncTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='rk3506-sync-test-')
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        self.repo, self.app, self.sdk = (base/n for n in ('repo','local','sdk'))
        for root in (self.repo/'apps', self.app, self.sdk/'external/rk3506app',
                     self.sdk/'kernel', self.repo/'sdk/baseline'):
            root.mkdir(parents=True)
        for root in (self.repo/'apps', self.app, self.sdk/'external/rk3506app'):
            (root/'main.c').write_text('base\n')
        self.kernel=self.sdk/'kernel'
        (self.kernel/'board.dts').write_text('vendor\n')
        for args in (['init','-q'],['add','.'],['-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','baseline']):
            subprocess.run(['git','-C',str(self.kernel),*args],check=True)
        commit=sync.git(self.kernel,'rev-parse','HEAD').stdout.decode().strip()
        sync.write_json(self.repo/'sdk/baseline/projects.json',[{'path':'kernel','commit':commit}])
        sync.write_json(self.repo/'sdk/baseline/changes.json',[])
        self.state_path=self.repo/'.sync/state.json'
        self.state={'entries':{}}
        inv=self.inventory()
        for key,entry in inv.entries.items():
            self.state['entries'][key]={'accepted':True,'values':entry['values']}
        sync.write_json(self.state_path,self.state)

    def inventory(self):
        return sync.Inventory(self.repo,self.app,self.sdk,self.state,[]).collect()

    def plan(self, resolutions=None, delete=False):
        inv=self.inventory()
        rows=sync.make_plan(inv,self.state,resolutions or {},delete)
        return inv,rows

    def apply(self, inv, rows):
        return sync.apply_plan(inv,self.state,rows,self.state_path,self.repo/'.sync/run')

    def test_local_only_change(self):
        (self.app/'main.c').write_text('new\n')
        inv,rows=self.plan()
        self.assertEqual(rows[0]['source'],'local')
        self.assertEqual(self.apply(inv,rows),1)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'new\n')
        self.assertEqual((self.sdk/'external/rk3506app/main.c').read_text(),'base\n')
        self.assertEqual(self.plan()[1][0]['action'],'unchanged')

    def test_two_source_conflict(self):
        (self.app/'main.c').write_text('local\n')
        (self.sdk/'external/rk3506app/main.c').write_text('sdk\n')
        inv,rows=self.plan()
        self.assertEqual(rows[0]['action'],'conflict')
        with self.assertRaises(RuntimeError):self.apply(inv,rows)
        inv,rows=self.plan({'apps/main.c':'local'})
        self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'local\n')

    def test_repo_edit_is_not_lost(self):
        (self.repo/'apps/main.c').write_text('git edit\n')
        inv,rows=self.plan();self.assertEqual(rows[0]['action'],'repo-only')
        self.apply(inv,rows)
        (self.app/'main.c').write_text('local edit\n')
        self.assertEqual(self.plan()[1][0]['action'],'conflict')

    def test_initial_differences_need_explicit_resolution(self):
        current={'local':{'kind':'file','sha256':'a'},'sdk':{'kind':'file','sha256':'b'},'repo':{'kind':'file','sha256':'b'}}
        self.assertEqual(sync.reconcile(current)[1],'initial-conflict')
        old={'accepted':False,'values':current}
        changed=dict(current,local={'kind':'file','sha256':'c'})
        self.assertEqual(sync.reconcile(changed,old)[1],'initial-conflict')

    def test_delete_requires_flag_and_has_backup(self):
        (self.app/'main.c').unlink()
        inv,rows=self.plan()
        self.assertIn('requires-allow-delete',rows[0]['issues'])
        with self.assertRaises(RuntimeError):self.apply(inv,rows)
        inv,rows=self.plan(delete=True);self.apply(inv,rows)
        self.assertFalse((self.repo/'apps/main.c').exists())
        self.assertEqual((self.repo/'.sync/run/backup/0').read_text(),'base\n')

    def test_credentials_block_even_explicit_copy(self):
        (self.app/'config.json').write_text('{"password":"live-secret-value"}')
        inv,rows=self.plan({'apps/config.json':'local'})
        blocked=next(r for r in rows if r['path']=='apps/config.json')
        self.assertIn('literal-credential',blocked['issues'])
        self.assertNotIn('live-secret-value',json.dumps(rows))
        with self.assertRaises(RuntimeError):self.apply(inv,rows)
        self.assertFalse((self.repo/'apps/config.json').exists())

    def test_new_app_file(self):
        (self.app/'new.c').write_text('new')
        inv,rows=self.plan();self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/new.c').read_text(),'new')

    def test_sdk_new_changed_deleted_and_reverted(self):
        (self.kernel/'board.dts').write_text('custom\n')
        (self.kernel/'new.dts').write_text('extra\n')
        inv,rows=self.plan();self.apply(inv,rows)
        manifest=sync.read_json(self.repo/'sdk/baseline/changes.json')
        self.assertEqual(manifest[0]['files'],['board.dts','new.dts'])
        (self.kernel/'board.dts').unlink()
        inv,rows=self.plan(delete=True);self.apply(inv,rows)
        manifest=sync.read_json(self.repo/'sdk/baseline/changes.json')
        self.assertEqual(manifest[0]['delete'],['board.dts'])
        (self.kernel/'board.dts').write_text('vendor\n')
        inv,rows=self.plan(delete=True);self.apply(inv,rows)
        manifest=sync.read_json(self.repo/'sdk/baseline/changes.json')
        self.assertEqual(manifest[0]['delete'],[])
        self.assertNotIn('board.dts',manifest[0]['files'])

    def test_source_changed_after_scan(self):
        (self.app/'main.c').write_text('v1')
        inv,rows=self.plan()
        (self.app/'main.c').write_text('v2')
        with self.assertRaises(RuntimeError):self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'base\n')

    def test_matching_manual_overlay_is_registered(self):
        (self.kernel/'new.dts').write_text('same')
        target=self.repo/'sdk/overlay/kernel/new.dts'
        target.parent.mkdir(parents=True);target.write_text('same')
        inv,rows=self.plan();self.apply(inv,rows)
        self.assertIn('new.dts',sync.read_json(self.repo/'sdk/baseline/changes.json')[0]['files'])

    def test_no_partial_batch_when_another_file_is_blocked(self):
        (self.app/'main.c').write_text('valid update')
        (self.app/'secret.json').write_text('{"password":"do-not-copy"}')
        inv,rows=self.plan()
        with self.assertRaises(RuntimeError):self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'base\n')

    def test_symlink_preserved_in_sdk(self):
        (self.kernel/'link').symlink_to('board.dts')
        inv,rows=self.plan();self.apply(inv,rows)
        target=self.repo/'sdk/overlay/kernel/link'
        self.assertTrue(target.is_symlink())
        self.assertEqual(str(target.readlink()),'board.dts')

    def test_rollback_on_state_write_error(self):
        (self.app/'main.c').write_text('new')
        inv,rows=self.plan(); original=sync.write_json
        def fail(path,value):
            if path==self.state_path:raise OSError('simulated full disk')
            return original(path,value)
        with patch.object(sync,'write_json',side_effect=fail):
            with self.assertRaises(OSError):self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'base\n')
        self.assertEqual(sync.read_json(self.state_path)['entries']['apps/main.c']['values']['repo'],snapshot(self.repo/'apps/main.c'))

    def test_missing_app_root_not_deletion(self):
        (self.app/'main.c').unlink();self.app.rmdir()
        with self.assertRaises(RuntimeError):self.inventory()

    def test_path_and_exclusion_policy(self):
        with self.assertRaises(ValueError):safe_path(self.repo,'../escape')
        (self.repo/'linked').symlink_to(self.app,target_is_directory=True)
        with self.assertRaises(ValueError):safe_path(self.repo,'linked/file')
        self.assertFalse(eligible('build_arm/a.c'))
        self.assertFalse(eligible('config.json.bak20260101'))
        self.assertFalse(eligible('common/extra-parts/userdata/normal/tk8710_gw'))
        self.assertTrue(eligible('tk8710app/tk8710_gw'))
        self.assertEqual(secret_findings(b'{"password":"DEPLOYMENT_REQUIRED"}','config.json'),[])
        self.assertIn('private-key',secret_findings(b'-----BEGIN PRIVATE KEY-----\nabc','key.pem'))

    def test_line_endings_do_not_create_conflict(self):
        (self.app/'main.c').write_bytes(b'base\r\n')
        self.assertEqual(self.plan()[1][0]['action'],'unchanged')

    def test_scope_and_same_change_converge(self):
        (self.app/'main.c').write_text('same')
        (self.sdk/'external/rk3506app/main.c').write_text('same')
        inv,rows=self.plan();self.apply(inv,rows)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'same')
        inv=sync.Inventory(self.repo,self.app,self.sdk,self.state,['apps/other']).collect()
        self.assertEqual(inv.entries,{})

    def test_nested_git_project_is_not_a_regular_file(self):
        sub=self.kernel/'child';sub.mkdir()
        (sub/'file.c').write_text('child base')
        for args in (['init','-q'],['add','.'],['-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','base']):
            subprocess.run(['git','-C',str(sub),*args],check=True)
        subcommit=sync.git(sub,'rev-parse','HEAD').stdout.decode().strip()
        subprocess.run(['git','-C',str(self.kernel),'add','child'],check=True,capture_output=True)
        subprocess.run(['git','-C',str(self.kernel),'-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','child'],check=True)
        projects=sync.read_json(self.repo/'sdk/baseline/projects.json')
        projects[0]['commit']=sync.git(self.kernel,'rev-parse','HEAD').stdout.decode().strip()
        projects.append({'path':'kernel/child','commit':subcommit})
        sync.write_json(self.repo/'sdk/baseline/projects.json',projects)
        (sub/'file.c').write_text('changed')
        inv,rows=self.plan()
        self.assertTrue(any('Submodule pointer' in w for w in inv.warnings))
        self.assertIn('sdk/overlay/kernel/child/file.c',inv.entries)

    def test_overlay_root_cannot_alias_sdk(self):
        (self.kernel/'board.dts').write_text('change')
        overlay=self.repo/'sdk/overlay';overlay.mkdir()
        (overlay/'kernel').symlink_to(self.kernel,target_is_directory=True)
        with self.assertRaises(ValueError):self.inventory()

    def test_untracked_nested_project_trailing_slash(self):
        sub=self.kernel/'child';sub.mkdir()
        (sub/'file.c').write_text('child')
        for args in (['init','-q'],['add','.'],['-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm','base']):
            subprocess.run(['git','-C',str(sub),*args],check=True)
        projects=sync.read_json(self.repo/'sdk/baseline/projects.json')
        projects.append({'path':'kernel/child','commit':sync.git(sub,'rev-parse','HEAD').stdout.decode().strip()})
        sync.write_json(self.repo/'sdk/baseline/projects.json',projects)
        inv=self.inventory()
        self.assertTrue(any('Submodule pointer' in w for w in inv.warnings))

    def test_cli_init_preview_and_apply(self):
        self.state_path.unlink()
        args=[sys.executable,str(Path(sync.__file__)), '--repo-root',str(self.repo),
              '--app-root',str(self.app),'--sdk-root',str(self.sdk)]
        result=subprocess.run(args+['--init'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        (self.app/'main.c').write_text('CLI change')
        result=subprocess.run(args+['--select','apps/main.c'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'base\n')
        result=subprocess.run(args+['--apply','--select','apps/main.c'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((self.repo/'apps/main.c').read_text(),'CLI change')
        self.assertEqual((self.sdk/'external/rk3506app/main.c').read_text(),'base\n')

if __name__=='__main__':unittest.main(verbosity=2)
