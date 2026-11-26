"""Register first-seen SSH hosts locally; never replace an existing identity."""
import base64
import hashlib
import os
from pathlib import Path
import tempfile

import paramiko


def fingerprint(key):
    digest = hashlib.sha256(key.asbytes()).digest()
    return "SHA256:" + base64.b64encode(digest).decode("ascii").rstrip("=")


class RegisterNewHost(paramiko.MissingHostKeyPolicy):
    def __init__(self, path, journal, log=print):
        self.path = Path(path)
        self.journal = journal
        self.log = log

    def missing_host_key(self, client, hostname, key):
        # Reload before merging so unrelated entries saved since connect setup
        # are retained. Write a complete temporary file, then replace atomically.
        keys = paramiko.HostKeys()
        if self.path.exists():
            keys.load(str(self.path))
        existing = keys.lookup(hostname)
        if existing and not keys.check(hostname, key):
            expected = existing.get(key.get_name()) or next(iter(existing.values()))
            raise paramiko.BadHostKeyException(hostname, key, expected)
        if not existing:
            keys.add(hostname, key.get_name(), key)
            self.path.parent.mkdir(parents=True, exist_ok=True)
            fd, temporary = tempfile.mkstemp(prefix=".known_hosts_", dir=self.path.parent)
            os.close(fd)
            try:
                keys.save(temporary)
                os.replace(temporary, self.path)
            finally:
                if os.path.exists(temporary):
                    os.unlink(temporary)
            identity = fingerprint(key)
            self.journal.emit("ssh_host_registered", host=hostname, algorithm=key.get_name(),
                              fingerprint=identity, path=str(self.path))
            self.log(f"SSH 已自动登记新地址 {hostname}，{key.get_name()} {identity}，"
                     f"保存到 {self.path}")
        client.get_host_keys().add(hostname, key.get_name(), key)


def configure(client, path, journal, log=print):
    path = Path(path)
    client.load_system_host_keys()
    if path.exists():
        client.load_host_keys(str(path))
    client.set_missing_host_key_policy(RegisterNewHost(path, journal, log))
