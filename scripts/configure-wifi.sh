#!/usr/bin/env bash
# Install Wi-Fi policy into a mounted rootfs; existing connection profiles win.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-wifi.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-wifi.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -d "$target_dir/etc"
test -x "$target_dir/usr/bin/iw"
test -x "$target_dir/usr/bin/set-wireless-regdom"
test -s "$target_dir/usr/lib/firmware/regulatory.db"
test -s "$target_dir/usr/lib/firmware/regulatory.db.p7s"

mkdir -p "$log_dir"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-wifi"
fi
rm -f "$log_dir/wifi-sources.sha256" "$log_dir/wifi-verification.json"
install -d -o 0 -g 0 -m 0755 "$target_dir/etc/NetworkManager/conf.d" \
    "$target_dir/etc/conf.d" "$target_dir/etc/udev/rules.d"
install -o 0 -g 0 -m 0644 "$project_dir/config/wifi/30-negroni-wifi.conf" \
    "$target_dir/etc/NetworkManager/conf.d/30-negroni-wifi.conf"
install -o 0 -g 0 -m 0644 "$project_dir/config/wifi/wireless-regdom" \
    "$target_dir/etc/conf.d/wireless-regdom"
install -o 0 -g 0 -m 0644 "$project_dir/config/wifi/86-negroni-regulatory.rules" \
    "$target_dir/etc/udev/rules.d/86-negroni-regulatory.rules"
cd "$project_dir"
sha256sum config/wifi/* scripts/configure-wifi.sh scripts/update-wifi-image.sh \
    scripts/verify-wifi-image.py > "$log_dir/wifi-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/wifi-sources.sha256"
