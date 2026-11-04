#!/usr/bin/env python3
"""Check the exact managed runtime fixes in an unmounted ext4 image."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from runtime_fixes import AUTOSTART, BAD_MODEM, FILES, MANIFEST, PROJECT, QMLDIR, autostart_fixed, qmldir_fixed


DEPENDENCIES = ('/usr/lib/modem-daemon', '/usr/bin/systemctl', '/usr/lib/kde-telephony-daemon',
                '/usr/lib/kactivitymanagerd', '/usr/bin/xembedsniproxy',
                '/usr/bin/gmenudbusmenuproxy', '/usr/lib/org_kde_powerdevil')


def verify(image_dir, log_dir):
    image = image_dir/'root.img'
    log_dir.mkdir(parents=True, exist_ok=True)
    report = log_dir/'runtime-fixes-verification.json'
    report.unlink(missing_ok=True)
    def debugfs(command, missing=False):
        r = subprocess.run(['debugfs', '-R', command, str(image)], capture_output=True, check=True)
        errors = r.stderr.decode().splitlines()[1:]
        if missing:
            if not errors or 'File not found' not in '\n'.join(errors):
                raise RuntimeError('Obsolete modem activation declaration still exists')
        elif errors:
            raise RuntimeError('\n'.join(errors))
        return r.stdout
    hashes = {}
    for target, (source, mode) in FILES.items():
        installed = debugfs('cat '+target)
        if installed != (PROJECT/source).read_bytes():
            raise RuntimeError('Image/source mismatch: '+target)
        metadata = debugfs('stat '+target).decode()
        if not re.search(rf'Type:\s+regular\s+Mode:\s+0{mode:o}', metadata) or not re.search(r'User:\s+0\s+Group:\s+0', metadata):
            raise RuntimeError('Invalid owner or mode: '+target)
        hashes[target] = hashlib.sha256(installed).hexdigest()
    for target in (QMLDIR, AUTOSTART):
        metadata = debugfs('stat '+target).decode()
        if (not re.search(r'Type:\s+regular\s+Mode:\s+0644', metadata)
                or not re.search(r'User:\s+0\s+Group:\s+0', metadata)):
            raise RuntimeError('Invalid owner or mode: '+target)
        hashes[target] = hashlib.sha256(debugfs('cat '+target)).hexdigest()
    for target in DEPENDENCIES:
        metadata = debugfs('stat '+target).decode()
        mode = re.search(r'Type:\s+regular\s+Mode:\s+([0-7]+)', metadata)
        if not mode or not int(mode[1], 8) & 0o111:
            raise RuntimeError('Missing executable dependency: '+target)
    module = debugfs('cat '+QMLDIR)
    if qmldir_fixed(module) != module:
        raise RuntimeError('QML resource preference bypasses managed source')
    source = debugfs('cat '+AUTOSTART)
    if autostart_fixed(source) != source:
        raise RuntimeError('Autostart must use the shared modem systemd service')
    debugfs('stat '+BAD_MODEM, missing=True)
    if json.loads(debugfs('cat '+MANIFEST)) != hashes:
        raise RuntimeError('Managed file manifest does not match installed content')
    result = {'image': str(image), 'files_sha256': hashes, 'hardware_tested': False,
              'modem_activation': 'three real names share one systemd user service'}
    report.write_text(json.dumps(result, indent=2)+'\n')
    print('Firmware, Quick Settings and desktop integration fixes verified')
    return result


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: verify-runtime-fixes-image.py IMAGE_DIRECTORY LOG_DIRECTORY')
    verify(*map(Path, sys.argv[1:]))
