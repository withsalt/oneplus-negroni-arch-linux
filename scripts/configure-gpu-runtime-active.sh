#!/usr/bin/env bash
# Configure an already mounted rootfs to keep the A730 GPU out of runtime suspend.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=${1:?Usage: configure-gpu-runtime-active.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
log_dir=${2:?Usage: configure-gpu-runtime-active.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
[[ $EUID == 0 && $target_dir != / ]] || exit 1

unit_name=gpu-runtime-active.service
unit_source="$project_dir/config/systemd/$unit_name"
unit_target="$target_dir/etc/systemd/system/$unit_name"

test -x "$target_dir/usr/bin/sh"
test -f "$unit_source"
install -d -o 0 -g 0 -m 0755 "$target_dir/etc/systemd/system"
mkdir -p "$log_dir"
install -o 0 -g 0 -m 0644 "$unit_source" "$unit_target"
chroot "$target_dir" systemctl enable "$unit_name"
chroot "$target_dir" systemctl is-enabled "$unit_name" | grep -Fxq enabled
grep -Fxq 'WantedBy=graphical.target' "$unit_target"
grep -Fxq 'Before=display-manager.service' "$unit_target"
grep -Fxq "ExecStart=/usr/bin/sh -c 'printf on > /sys/bus/platform/devices/3d00000.gpu/power/control'" "$unit_target"
printf '%s\n' \
    'GPU runtime PM policy: on' \
    'Device: /sys/bus/platform/devices/3d00000.gpu' \
    'Service: gpu-runtime-active.service (enabled for graphical.target)' \
    > "$log_dir/gpu-runtime-pm-config.txt"
touch "$log_dir/gpu-runtime-pm.complete"
