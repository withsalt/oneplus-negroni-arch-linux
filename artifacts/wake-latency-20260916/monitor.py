import glob
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import time

paths = {
    'dpms': '/sys/class/drm/card0-DSI-1/dpms',
    'gpu': '/sys/bus/platform/devices/3d00000.gpu/power/runtime_status',
    'gmu': '/sys/bus/platform/devices/3d6a000.gmu/power/runtime_status',
    'dpu': '/sys/bus/platform/devices/ae01000.display-controller/power/runtime_status',
}
for p in glob.glob('/sys/class/backlight/*/brightness'):
    paths['brightness'] = p
kwin = [p.parent for p in Path('/proc').glob('[0-9]*/comm')
        if p.read_text().strip() == 'kwin_wayland']
for p in kwin:
    paths['kwin_' + p.name] = str(p / 'wchan')

def emit(kind, **fields):
    print(json.dumps({'t': time.monotonic(), 'kind': kind, **fields}), flush=True)

fd = os.open('/dev/input/event0', os.O_RDONLY | os.O_NONBLOCK)
last = None
end = time.monotonic() + 55
start = time.monotonic()
schedule = [(3, 'off'), (15, 'on'), (27, 'off'), (39, 'on')] if '--cycle' in sys.argv else []
env = dict(os.environ, XDG_RUNTIME_DIR='/run/user/1000',
           DBUS_SESSION_BUS_ADDRESS='unix:path=/run/user/1000/bus', WAYLAND_DISPLAY='wayland-0')
last_stack = 0
emit('start')
while time.monotonic() < end:
    if schedule and time.monotonic() - start >= schedule[0][0]:
        _, mode = schedule.pop(0)
        emit('request', mode=mode)
        subprocess.Popen(['runuser', '-u', 'oneplus', '--', 'env',
                          'XDG_RUNTIME_DIR=' + env['XDG_RUNTIME_DIR'],
                          'DBUS_SESSION_BUS_ADDRESS=' + env['DBUS_SESSION_BUS_ADDRESS'],
                          'WAYLAND_DISPLAY=wayland-0', 'kscreen-doctor', '--dpms', mode],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if select.select([fd], [], [], 0)[0]:
        data = os.read(fd, 24 * 64)
        for off in range(0, len(data), 24):
            sec, usec, typ, code, val = struct.unpack('llHHi', data[off:off+24])
            emit('input', type=typ, code=code, value=val, event_time=sec+usec/1e6)
    state = {}
    for key, path in paths.items():
        try:
            state[key] = Path(path).read_text().strip()
        except OSError as e:
            state[key] = str(e)
    if state != last:
        emit('state', **state)
        last = state
    if time.monotonic() - last_stack >= 1:
        last_stack = time.monotonic()
        for p in kwin:
            for thread in (p / 'task').glob('*'):
                try:
                    if ') D' in (thread / 'stat').read_text():
                        emit('blocked', tid=thread.name, stack=(thread / 'stack').read_text())
                except OSError:
                    pass
    time.sleep(0.05)
emit('end')
