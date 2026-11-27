"""File selection and publication checks shared by the incremental importer."""
import hashlib
from pathlib import Path, PurePosixPath
import re

ABSENT = {'kind': 'absent'}
DELETED = {'kind': 'delete'}  # SDK vendor file intentionally removed.
MAX_BYTES = 40 * 1024 * 1024
SKIP_DIRS = {'.git', '.repo', '.private', '.sync', '.trae', '.vscode',
             '__pycache__', '.venv', 'node_modules', 'CMakeFiles', 'build',
             'build_arm', 'build_sdk', 'build_rk3506', 'logs', 'remote_logs',
             'backup', 'backups', 'milestone', 'release', 'zpf'}
SKIP_SUFFIX = {'.o', '.d', '.pyc', '.log', '.img', '.rar', '.zip', '.7z',
               '.tar', '.gz', '.xz', '.png', '.jpg', '.jpeg', '.exe', '.dll'}
TEXT_SUFFIX = {'.c', '.h', '.sh', '.py', '.md', '.json', '.toml', '.conf',
               '.dts', '.dtsi', '.mk', '.txt', '.ps1', '.yaml', '.yml'}

def eligible(name):
    p = PurePosixPath(name)
    return (not any(x in SKIP_DIRS or 'backup' in x.lower() or
                    x.startswith(('deploy-20', 'board-validation-', 'protection-board-')) or
                    'validation-' in x for x in p.parts)
            and p.suffix.lower() not in SKIP_SUFFIX
            and not re.search(r'(?:[._](?:bak|back|cbak)\d*|~)$', p.name)
            and not p.name.endswith('.generated.toml')
            and not (p.name.startswith('log_') and p.suffix == '.dat')
            and not name.startswith('common/extra-parts/userdata/normal/'))

def safe_path(root, relative):
    p = PurePosixPath(relative)
    if not relative or p.is_absolute() or '..' in p.parts or '\\' in relative:
        raise ValueError(f'Unsafe relative path: {relative}')
    target = root.joinpath(*p.parts)
    # Do not traverse even an in-tree symlink: it could alias another file.
    parent = root
    for part in p.parts[:-1]:
        parent /= part
        if parent.is_symlink():
            raise ValueError(f'Symlink parent: {relative}')
    return target

def canonical(data, name):
    if b'\0' not in data and (Path(name).suffix in TEXT_SUFFIX or data.startswith(b'#!')):
        return data.replace(b'\r\n', b'\n')
    return data

def fingerprint(data, kind='file', name=''):
    return {'kind': kind, 'sha256': hashlib.sha256(canonical(data, name)).hexdigest()}

def snapshot(path):
    if path.is_symlink():
        return fingerprint(str(path.readlink()).encode(), 'symlink')
    if not path.exists():
        return ABSENT.copy()
    if not path.is_file():
        raise ValueError(f'Not a regular file: {path}')
    if path.stat().st_size > MAX_BYTES:
        return {'kind': 'oversize', 'bytes': path.stat().st_size}
    return fingerprint(path.read_bytes(), name=path.name)

def secret_findings(data, name):
    """Return rule names only: reports must never contain a secret value."""
    rules = []
    if Path(name).name in {'shadow', 'gshadow', 'id_rsa', 'id_ed25519'}:
        rules.append('private-key-or-password-database')
    if b'\0' in data:  # PEM parser strings in ELF libraries are not private keys.
        return rules
    text = data.decode('utf-8', errors='replace')
    if re.search(r'(?m)^-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----', text):
        rules.append('private-key')
    if re.search(r'\b(?:ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}|AKIA[A-Z0-9]{16})\b', text):
        rules.append('access-token')
    key = r'(?:password|passwd|web_password|auth\.token|token|wpa_passphrase|psk|AP_PASS|ROOT_PASSWORD)'
    patterns = [rf'''(?im)["']?{key}["']?\s*[:=]\s*(["'])([^\r\n"']*)\1''',
                rf'(?im)^\s*(?:wpa_passphrase|psk|auth\.token|AP_PASS|BR2_TARGET_GENERIC_ROOT_PASSWD)\s*=\s*([^\s#]+)']
    for index, pattern in enumerate(patterns):
        for match in re.finditer(pattern, text):
            value = match[2] if index == 0 else match[1].strip('"\'')
            if (value and not value.startswith(('$', '<')) and
                    value not in {'DEPLOYMENT_REQUIRED', 'REPLACE_ME', 'CHANGEME'}):
                rules.append('literal-credential'); break
    return sorted(set(rules))
