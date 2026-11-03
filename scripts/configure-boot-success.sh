#!/usr/bin/env bash
# Install the opt-in service; firmware/slot confirmation belongs to each device.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-boot-success.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-boot-success.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -x "$target_dir/usr/bin/python3"
test -x "$target_dir/usr/bin/qbootctl"
mkdir -p "$log_dir"
rm -f "$log_dir/boot-success-sources.sha256"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-boot-success"
fi
install -d -o 0 -g 0 -m 0755 "$target_dir/usr/local/libexec" "$target_dir/etc/systemd/system"
install -o 0 -g 0 -m 0755 "$project_dir/config/boot-success/negroni-boot-success" "$target_dir/usr/local/libexec/negroni-boot-success"
install -o 0 -g 0 -m 0644 "$project_dir/config/systemd/negroni-boot-success.service" "$target_dir/etc/systemd/system/negroni-boot-success.service"
systemctl --root="$target_dir" enable negroni-boot-success.service
[[ $(systemctl --root="$target_dir" is-enabled negroni-boot-success.service) == enabled ]]
# Deliberately do not create or overwrite /etc/negroni-boot-success.json.
cd "$project_dir"
sha256sum config/boot-success/negroni-boot-success config/systemd/negroni-boot-success.service \
    scripts/configure-boot-success.sh scripts/update-boot-success-image.sh > "$log_dir/boot-success-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/boot-success-sources.sha256"
