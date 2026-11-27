"""Run under WSL/Linux: python3 apps/tests/test_sync_apps.py."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "sync_apps.sh"
MODULES = "Discovery SMP ipcweb light log mqtt tdaemon webserver common third_party frp watchdog swupdattool tk8710app".split()


class SyncAppsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rk3506-sync-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "本地 app"
        self.sdk = self.root / "sdk"
        self.destination = self.sdk / "external/rk3506app"
        self.destination.mkdir(parents=True)
        (self.sdk / "build.sh").touch()
        for module in MODULES:
            (self.source / module).mkdir(parents=True)
            (self.destination / module).mkdir()
        for relative in ("mqtt/src/main.c", "webserver/src/main.c", "tk8710app/tk8710_gw"):
            self.write(self.source / relative, "new data")

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def run_sync(self, *options):
        return subprocess.run(
            ["bash", str(SCRIPT), *options, str(self.source), str(self.sdk)],
            capture_output=True, text=True,
        )

    def test_contents_deletions_backups_and_exclusions(self):
        old = self.destination / "mqtt/src/main.c"
        self.write(old, "old data")
        stamp = old.stat().st_mtime_ns
        os.utime(self.source / "mqtt/src/main.c", ns=(stamp, stamp))
        self.write(self.destination / "mqtt/src/deleted.c", "removed source")
        self.write(self.destination / "mqtt/build_arm/mqtt_bridge", "keep build")
        self.write(self.source / "mqtt/build_arm/mqtt_bridge", "Windows build")
        self.write(self.source / "mqtt/docs/board.log", "do not copy")
        self.write(self.source / "webserver/inc/web_resources.h", "old generated header")
        self.write(self.destination / "webserver/inc/web_resources.h", "keep generated header")
        self.write(self.destination / "build_apps.sh", "keep entrypoint")
        self.write(self.source / "build_apps.sh", "do not overwrite")
        result = self.run_sync()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(old.read_text(), "new data")
        self.assertFalse((self.destination / "mqtt/src/deleted.c").exists())
        self.assertEqual((self.destination / "mqtt/build_arm/mqtt_bridge").read_text(), "keep build")
        self.assertFalse((self.destination / "mqtt/docs/board.log").exists())
        self.assertEqual((self.destination / "webserver/inc/web_resources.h").read_text(), "keep generated header")
        self.assertEqual((self.destination / "build_apps.sh").read_text(), "keep entrypoint")
        backup = next((self.sdk / "output").glob("app-sync-backup-*"))
        self.assertEqual((backup / "files/mqtt/src/main.c").read_text(), "old data")
        self.assertEqual((backup / "files/mqtt/src/deleted.c").read_text(), "removed source")
        self.assertEqual(self.run_sync().returncode, 0)

    def test_preview_does_not_change_or_create_files(self):
        result = self.run_sync("--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.sdk / "output").exists())
        self.assertFalse((self.destination / "mqtt/src/main.c").exists())

    def test_missing_module_fails_before_any_copy(self):
        (self.source / "watchdog").rmdir()
        result = self.run_sync()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("watchdog", result.stderr)
        self.assertFalse((self.destination / "mqtt/src/main.c").exists())
        self.assertFalse((self.sdk / "output").exists())

    def test_source_symlink_is_rejected_before_copy(self):
        (self.source / "mqtt/src/link.c").symlink_to("main.c")
        self.assertNotEqual(self.run_sync().returncode, 0)
        self.assertFalse((self.sdk / "output").exists())

    def test_destination_symlink_is_rejected(self):
        (self.destination / "mqtt").rmdir()
        (self.destination / "mqtt").symlink_to(self.source / "mqtt", target_is_directory=True)
        self.assertNotEqual(self.run_sync().returncode, 0)
        self.assertFalse((self.sdk / "output").exists())

    def test_explicit_sdk_source_is_supported(self):
        result = subprocess.run(
            ["bash", str(SCRIPT), str(self.destination), str(self.sdk)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.sdk / "output").exists())

    def test_stage_entrypoint_stops_on_missing_windows_source(self):
        shutil.copyfile(SCRIPT.parent / "build_apps.sh", self.destination / "build_apps.sh")
        shutil.copyfile(SCRIPT, self.destination / "sync_apps.sh")
        env = dict(os.environ, SDK_ROOT=str(self.sdk),
                   RK3506_APP_SOURCE=str(self.root / "missing source"))
        result = subprocess.run(
            ["bash", "external/rk3506app/build_apps.sh", "--stage"],
            cwd=self.sdk, env=env, capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source unavailable", result.stderr)
        self.assertFalse((self.sdk / "output").exists())


if __name__ == "__main__":
    unittest.main()
