#!/usr/bin/env bash
# Install and enable the CPU policy in a mounted rootfs, without starting it.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-cpu-performance.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-cpu-performance.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -x "$target_dir/usr/bin/python3"
unit=negroni-cpu-performance.service

mkdir -p "$log_dir"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-cpu-performance"
fi
# Remove the success fingerprint before mutation so a failed install is retried.
rm -f "$log_dir/cpu-performance-sources.sha256" "$log_dir/cpu-performance-verification.json"
install -d -o 0 -g 0 -m 0755 "$target_dir/usr/local/sbin" "$target_dir/etc/systemd/system"
install -o 0 -g 0 -m 0755 "$project_dir/config/cpu-performance/negroni-cpu-performance" \
    "$target_dir/usr/local/sbin/negroni-cpu-performance"
install -o 0 -g 0 -m 0644 "$project_dir/config/systemd/$unit" "$target_dir/etc/systemd/system/$unit"
systemctl --root="$target_dir" enable "$unit"
[[ $(systemctl --root="$target_dir" is-enabled "$unit") == enabled ]]
cmp "$project_dir/config/cpu-performance/negroni-cpu-performance" "$target_dir/usr/local/sbin/negroni-cpu-performance"
cmp "$project_dir/config/systemd/$unit" "$target_dir/etc/systemd/system/$unit"
printf '%s\n' 'CPU policy: performance; hardware-supported boost: enabled' \
    'Enabled at boot: negroni-cpu-performance.service' \
    'Stop the service to restore the settings captured at service startup.' \
    > "$log_dir/cpu-performance-config.txt"
cd "$project_dir"
sha256sum config/cpu-performance/negroni-cpu-performance config/systemd/negroni-cpu-performance.service \
    scripts/configure-cpu-performance.sh scripts/update-cpu-performance-image.sh \
    scripts/verify-cpu-performance-image.py > "$log_dir/cpu-performance-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/cpu-performance-sources.sha256" \
    "$log_dir/cpu-performance-config.txt"
