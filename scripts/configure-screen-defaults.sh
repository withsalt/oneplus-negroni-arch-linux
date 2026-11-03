#!/usr/bin/env bash
# Install Plasma desktop defaults and the SDDM power policy in a mounted rootfs.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-screen-defaults.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-screen-defaults.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -d "$target_dir/etc"
# Fail before changing the image if an existing rootfs lacks a runtime dependency.
for dependency in dbus-run-session kwin_wayland plasma-keyboard; do
    test -x "$target_dir/usr/bin/$dependency"
done
test -x "$target_dir/usr/lib/org_kde_powerdevil"

# Invalidate the pack marker before changing the image, including on retries.
mkdir -p "$log_dir"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-screen-defaults"
fi
rm -f "$log_dir/screen-defaults-sources.sha256" "$log_dir/screen-defaults-verification.json"
install -d -o 0 -g 0 -m 0755 "$target_dir/etc/xdg"
install -o 0 -g 0 -m 0644 "$project_dir/config/powerdevil/powerdevilrc" "$target_dir/etc/xdg/powerdevilrc"
cmp "$project_dir/config/powerdevil/powerdevilrc" "$target_dir/etc/xdg/powerdevilrc"
install -d -o 0 -g 0 -m 0755 "$target_dir/etc/sddm.conf.d" \
    "$target_dir/etc/xdg/negroni-greeter" "$target_dir/etc/systemd/logind.conf.d" \
    "$target_dir/usr/local/libexec"
install -o 0 -g 0 -m 0644 "$project_dir/config/sddm/10-mobile.conf" "$target_dir/etc/sddm.conf.d/10-mobile.conf"
install -o 0 -g 0 -m 0644 "$project_dir/config/sddm/powerdevilrc" "$target_dir/etc/xdg/negroni-greeter/powerdevilrc"
install -o 0 -g 0 -m 0644 "$project_dir/config/logind/60-negroni-power-key.conf" \
    "$target_dir/etc/systemd/logind.conf.d/60-negroni-power-key.conf"
for helper in negroni-sddm-compositor negroni-sddm-power; do
    install -o 0 -g 0 -m 0755 "$project_dir/config/sddm/$helper" "$target_dir/usr/local/libexec/$helper"
done
cd "$project_dir"
sha256sum config/powerdevil/powerdevilrc config/sddm/10-mobile.conf config/sddm/powerdevilrc \
    config/sddm/negroni-sddm-compositor config/sddm/negroni-sddm-power \
    config/logind/60-negroni-power-key.conf scripts/configure-screen-defaults.sh \
    scripts/update-screen-defaults-image.sh scripts/verify-screen-defaults-image.py \
    > "$log_dir/screen-defaults-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/screen-defaults-sources.sha256"
