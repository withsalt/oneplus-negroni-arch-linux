#!/usr/bin/env python3
"""Version-checked firmware loader, Quick Settings and desktop packaging fixes."""
import hashlib
import json
import os
from pathlib import Path
import sys

MANIFEST = '/var/lib/negroni-runtime-fixes/installed-files.json'
PROJECT = Path(__file__).resolve().parents[1]
LOADER = '/usr/bin/msm-firmware-loader.sh'
BAD_MODEM = '/usr/share/dbus-1/services/org.kde.modemdaemon.service'
QML_MODULE = '/usr/lib/qt6/qml/org/kde/plasma/private/mobileshell'
QUICK_SETTINGS = QML_MODULE+'/actiondrawer/private/QuickSettings.qml'
QMLDIR = QML_MODULE+'/qmldir'
ORIGINAL_QML_SHA256 = '04efb09e0ed16082294c4c2d7c1c32cc93d4bdab3ffec9b30d0bffa8cbaa8b40'
AUTOSTART = '/etc/xdg/autostart/org.kde.modem.daemon.desktop'
ORIGINAL_LOADER_SHA256 = 'c68f181a550f662a75091af6e5600953fe950c050ff5398168c2b490977cd459'
FILES = {
    QUICK_SETTINGS: ('config/quicksettings/QuickSettings.qml', 0o644),
    LOADER: ('config/firmware-loader/msm-firmware-loader.sh', 0o755),
    '/usr/lib/systemd/user/negroni-modem-daemon.service': ('config/systemd/negroni-modem-daemon.service', 0o644),
}
for interface in ('DeviceUtils', 'UssdUtils', 'CallUtils'):
    filename = f'org.kde.telephony.{interface}.service'
    FILES['/usr/share/dbus-1/services/'+filename] = ('config/telephony/'+filename, 0o644)

for app_id in ('org.kde.ActivityManager', 'org.kde.xembedsniproxy',
               'org.kde.gmenudbusmenuproxy', 'org.kde.org_kde_powerdevil'):
    filename = app_id+'.desktop'
    FILES['/usr/local/share/applications/'+filename] = ('config/portal-apps/'+filename, 0o644)


def autostart_fixed(source):
    old = b'Exec=/usr/lib/modem-daemon\n'
    new = b'Exec=/usr/bin/systemctl --user start negroni-modem-daemon.service\n'
    if source.count(new) == 1 and old not in source:
        return source
    if source.count(old) != 1:
        raise RuntimeError('Unknown modem autostart command; review package changes first')
    return source.replace(old, new)


def qmldir_fixed(source):
    # The packaged module prefers its qrc copy, which would bypass our QML fix.
    prefer = b'prefer :/org/kde/plasma/private/mobileshell/\n'
    if b'module org.kde.plasma.private.mobileshell\n' not in source or b'ActionDrawer 254.0 ActionDrawer.qml\n' not in source:
        raise RuntimeError('Unknown mobileshell module metadata; review package first')
    if source.count(prefer) == 1:
        return source.replace(prefer, b'')
    if b'prefer ' in source:
        raise RuntimeError('Unexpected QML resource preference')
    return source


def plan(root):
    manifest = root/MANIFEST.lstrip('/')
    if manifest.is_symlink():
        raise RuntimeError('Unexpected runtime fixes manifest symlink')
    recorded = json.loads(manifest.read_text()) if manifest.exists() else {}
    if not isinstance(recorded, dict):
        raise RuntimeError('Invalid runtime fixes manifest')
    changes = {}
    for target, (source, mode) in FILES.items():
        path = root / target.lstrip('/')
        wanted = (PROJECT/source).read_bytes()
        if path.is_symlink():
            raise RuntimeError(f'Refusing unexpected symlink: {path}')
        current = path.read_bytes() if path.exists() else None
        previously_managed = (current is not None and
                              hashlib.sha256(current).hexdigest() == recorded.get(target))
        if target in (LOADER, QUICK_SETTINGS):
            original_hash = ORIGINAL_LOADER_SHA256 if target == LOADER else ORIGINAL_QML_SHA256
            if (current != wanted and not previously_managed
                    and hashlib.sha256(current or b'').hexdigest() != original_hash):
                raise RuntimeError('Unknown package file version; review before applying: '+target)
        elif current is not None and current != wanted and not previously_managed:
            raise RuntimeError(f'Unexpected existing local configuration: {path}')
        changes[target] = (wanted, mode)
    for target in (QMLDIR, AUTOSTART):
        if (root/target.lstrip('/')).is_symlink():
            raise RuntimeError('Refusing unexpected symlink: '+target)
    changes[QMLDIR] = (qmldir_fixed((root/QMLDIR.lstrip('/')).read_bytes()), 0o644)
    path = root / BAD_MODEM.lstrip('/')
    if path.exists():
        original = b'[D-BUS Service]\nName=org.kde.telephony\nExec=/usr/lib/modem-daemon\n'
        if path.is_symlink() or path.read_bytes().strip() != original.strip():
            raise RuntimeError('Unknown modem activation file; review before replacing')
        changes[BAD_MODEM] = (None, 0)
    auto = root / AUTOSTART.lstrip('/')
    changes[AUTOSTART] = (autostart_fixed(auto.read_bytes()), 0o644)
    for executable in ('/usr/lib/modem-daemon', '/usr/bin/systemctl', '/usr/lib/kde-telephony-daemon',
                       '/usr/lib/kactivitymanagerd', '/usr/bin/xembedsniproxy',
                       '/usr/bin/gmenudbusmenuproxy', '/usr/lib/org_kde_powerdevil'):
        if (not (root/executable.lstrip('/')).is_file()
                or not (root/executable.lstrip('/')).stat().st_mode & 0o111):
            raise RuntimeError('Missing dependency: '+executable)
    return changes


def apply(root):
    # Validate every input before changing any file. Keep content-addressed originals.
    changes = plan(root)
    for target, (content, mode) in changes.items():
        path = root / target.lstrip('/')
        if path.exists():
            original = path.read_bytes()
            if original == content:
                path.chmod(mode)
                if os.geteuid() == 0:
                    os.chown(path, 0, 0)
                continue
            digest = hashlib.sha256(original).hexdigest()
            backup = root/'var/lib/negroni-runtime-fixes/backups'/digest/target.lstrip('/')
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                backup.write_bytes(original)
                backup.chmod(path.stat().st_mode & 0o777)
        if content is None:
            path.unlink()
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_name(path.name+'.negroni-new')
            temporary.write_bytes(content)
            temporary.chmod(mode)
            temporary.replace(path)
        print(target)
    manifest = root/MANIFEST.lstrip('/')
    manifest.parent.mkdir(parents=True, exist_ok=True)
    hashes = {target: hashlib.sha256(content).hexdigest()
              for target, (content, _) in changes.items() if content is not None}
    manifest.write_text(json.dumps(hashes, indent=2, sort_keys=True)+'\n')
    manifest.chmod(0o644)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: runtime_fixes.py ROOTFS_DIRECTORY')
    apply(Path(sys.argv[1]).resolve(strict=True))
