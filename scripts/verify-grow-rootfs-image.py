#!/usr/bin/env python3
"""Verify first-boot expansion is installed and armed in an offline ext4 image."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
UNIT = 'negroni-grow-rootfs.service'
MARKER = '/var/lib/negroni-grow-rootfs/done.json'
DEPENDENCIES = ('python3', 'findmnt', 'lsblk', 'blockdev', 'dumpe2fs', 'resize2fs')


def verify(image_dir, log_dir):
    image = image_dir / 'root.img'
    log_dir.mkdir(parents=True, exist_ok=True)
    report = log_dir / 'grow-rootfs-verification.json'
    report.unlink(missing_ok=True)

    def debugfs(command, missing_ok=False):
        result = subprocess.run(['debugfs', '-R', command, str(image)],
                                capture_output=True, check=True)
        errors = result.stderr.decode().splitlines()[1:]
        if errors:
            if missing_ok and all('File not found by ext2_lookup' in error for error in errors):
                return None
            raise RuntimeError('\n'.join(errors))
        return result.stdout

    hashes = {}
    files = {
        '/usr/local/libexec/negroni-grow-rootfs': ('config/grow-rootfs/negroni-grow-rootfs', '0755'),
        f'/etc/systemd/system/{UNIT}': (f'config/systemd/{UNIT}', '0644'),
    }
    for target, (source, mode) in files.items():
        installed = debugfs(f'cat {target}')
        if installed != (PROJECT / source).read_bytes():
            raise RuntimeError(f'image/source mismatch: {target}')
        metadata = debugfs(f'stat {target}').decode()
        if (not re.search(rf'Type:\s+regular\s+Mode:\s+{mode}', metadata) or
                not re.search(r'User:\s+0\s+Group:\s+0', metadata)):
            raise RuntimeError(f'{target} must be regular, root-owned, mode {mode}')
        hashes[target] = hashlib.sha256(installed).hexdigest()
    link = f'/etc/systemd/system/multi-user.target.wants/{UNIT}'
    metadata = debugfs(f'stat {link}').decode()
    match = re.search(r'Fast link dest: "([^"]+)"', metadata)
    if ('Type: symlink' not in metadata or not match or
            match[1] not in (f'/etc/systemd/system/{UNIT}', f'../{UNIT}')):
        raise RuntimeError('Root expansion service must be enabled')
    if debugfs(f'stat {MARKER}', missing_ok=True) is not None:
        raise RuntimeError('Image must not contain a root expansion completion marker')

    for name in DEPENDENCIES:
        target = PurePosixPath('/usr/bin') / name
        # Python's public interpreter name is a symlink on Arch Linux ARM.
        for _ in range(8):
            metadata = debugfs(f'stat {target}').decode()
            mode = re.search(r'Type:\s+regular\s+Mode:\s+([0-7]+)', metadata)
            if mode and int(mode[1], 8) & 0o111:
                break
            destination = re.search(r'Fast link dest: "([^"]+)"', metadata)
            if 'Type: symlink' not in metadata or not destination:
                raise RuntimeError(f'Missing executable root expansion dependency: {name}')
            target = target.parent / destination[1]
        else:
            raise RuntimeError(f'Unresolved root expansion dependency: {name}')

    result = {
        'image': str(image), 'service': UNIT, 'service_enabled': True,
        'first_boot_armed': True, 'completion_marker': MARKER,
        'files_match_sources': True, 'installed_file_sha256': hashes,
        'runtime_dependencies': list(DEPENDENCIES), 'filesystem': 'ext4',
        'partition_label': 'userdata', 'repartition': False, 'hardware_tested': False,
    }
    report.write_text(json.dumps(result, indent=2) + '\n')
    print('Root expansion image verified: enabled, dependencies present, first boot armed')
    return result


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: verify-grow-rootfs-image.py IMAGE_DIRECTORY LOG_DIRECTORY')
    verify(*map(Path, sys.argv[1:]))
