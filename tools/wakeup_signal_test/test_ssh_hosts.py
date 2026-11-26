from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

import paramiko
import ssh_hosts


class HostRegistrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.old_key = paramiko.RSAKey.generate(1024)
        cls.new_key = paramiko.RSAKey.generate(1024)

    def test_first_host_creates_file_and_logs_fingerprint(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "keys" / "known_hosts.local"
            journal, log = Mock(), Mock()
            client = paramiko.SSHClient()
            ssh_hosts.configure(client, path, journal, log)
            client._policy.missing_host_key(client, "192.168.100.81", self.new_key)
            self.assertTrue(paramiko.HostKeys(str(path)).check("192.168.100.81", self.new_key))
            self.assertTrue(client.get_host_keys().check("192.168.100.81", self.new_key))
            self.assertEqual(journal.emit.call_args.kwargs["fingerprint"], ssh_hosts.fingerprint(self.new_key))
            log.assert_called_once()

    def test_new_host_preserves_old_and_repeat_does_not_rewrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known_hosts.local"
            keys = paramiko.HostKeys()
            keys.add("192.168.100.78", self.old_key.get_name(), self.old_key)
            keys.save(str(path))
            policy = ssh_hosts.RegisterNewHost(path, Mock(), Mock())
            client = paramiko.SSHClient()
            policy.missing_host_key(client, "192.168.100.81", self.new_key)
            self.assertTrue(paramiko.HostKeys(str(path)).check("192.168.100.78", self.old_key))
            contents = path.read_bytes()
            policy.missing_host_key(client, "192.168.100.81", self.new_key)
            self.assertEqual(path.read_bytes(), contents)

    def test_changed_host_is_rejected_without_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known_hosts.local"
            policy = ssh_hosts.RegisterNewHost(path, Mock(), Mock())
            policy.missing_host_key(paramiko.SSHClient(), "192.168.100.81", self.old_key)
            before = path.read_bytes()
            with self.assertRaises(paramiko.BadHostKeyException):
                policy.missing_host_key(paramiko.SSHClient(), "192.168.100.81", self.new_key)
            self.assertEqual(path.read_bytes(), before)

    def test_nonstandard_ports_are_separate_identities(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known_hosts.local"
            policy = ssh_hosts.RegisterNewHost(path, Mock(), Mock())
            policy.missing_host_key(paramiko.SSHClient(), "192.168.100.81", self.old_key)
            policy.missing_host_key(paramiko.SSHClient(), "[192.168.100.81]:2222", self.new_key)
            keys = paramiko.HostKeys(str(path))
            self.assertTrue(keys.check("192.168.100.81", self.old_key))
            self.assertTrue(keys.check("[192.168.100.81]:2222", self.new_key))

    def test_failed_save_keeps_original_file_and_does_not_trust_key(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known_hosts.local"
            policy = ssh_hosts.RegisterNewHost(path, Mock(), Mock())
            policy.missing_host_key(paramiko.SSHClient(), "192.168.100.78", self.old_key)
            before = path.read_bytes()
            client = paramiko.SSHClient()
            with patch.object(ssh_hosts.os, "replace", side_effect=OSError("read only")):
                with self.assertRaises(OSError):
                    policy.missing_host_key(client, "192.168.100.81", self.new_key)
            self.assertEqual(path.read_bytes(), before)
            self.assertIsNone(client.get_host_keys().lookup("192.168.100.81"))
            self.assertEqual(list(Path(directory).iterdir()), [path])


if __name__ == "__main__":
    unittest.main()
