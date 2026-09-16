#!/usr/bin/env bash
# Install editable Plasma power defaults into an already mounted rootfs.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-screen-defaults.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-screen-defaults.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -d "$target_dir/etc"

# Invalidate the pack marker before changing the image, including on retries.
mkdir -p "$log_dir"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-screen-defaults"
fi
install -d -o 0 -g 0 -m 0755 "$target_dir/etc/xdg"
install -o 0 -g 0 -m 0644 "$project_dir/config/powerdevil/powerdevilrc" "$target_dir/etc/xdg/powerdevilrc"
cmp "$project_dir/config/powerdevil/powerdevilrc" "$target_dir/etc/xdg/powerdevilrc"
cd "$project_dir"
sha256sum config/powerdevil/powerdevilrc scripts/configure-screen-defaults.sh \
    scripts/update-screen-defaults-image.sh scripts/verify-screen-defaults-image.py \
    > "$log_dir/screen-defaults-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/screen-defaults-sources.sha256"
