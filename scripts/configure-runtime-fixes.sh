#!/usr/bin/env bash
# Apply verified user-space fixes without changing kernel/firmware partitions.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-runtime-fixes.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-runtime-fixes.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
mkdir -p "$log_dir"
rm -f "$log_dir/runtime-fixes-sources.sha256" "$log_dir/runtime-fixes-verification.json"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-runtime-fixes"
fi
python3 "$project_dir/scripts/runtime_fixes.py" "$target_dir"
sh -n "$target_dir/usr/bin/msm-firmware-loader.sh"
cd "$project_dir"
sha256sum config/firmware-loader/* config/telephony/* config/quicksettings/* config/portal-apps/* config/systemd/negroni-modem-daemon.service \
    scripts/runtime_fixes.py scripts/configure-runtime-fixes.sh \
    scripts/update-runtime-fixes-image.sh scripts/verify-runtime-fixes-image.py \
    > "$log_dir/runtime-fixes-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/runtime-fixes-sources.sha256"
