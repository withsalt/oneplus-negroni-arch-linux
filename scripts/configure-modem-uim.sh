#!/usr/bin/env bash
# Treat an absent SIM as an expected result, preserving modem/QMI failures.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-modem-uim.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-modem-uim.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -x "$target_dir/usr/bin/msm-modem-uim-selection.sh"
mkdir -p "$log_dir"
rm -f "$log_dir/modem-uim-sources.sha256" "$log_dir/modem-uim-verification.json"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-modem-uim"
fi
python3 "$project_dir/scripts/modem_uim.py" "$target_dir/usr/bin/msm-modem-uim-selection.sh"
bash -n "$target_dir/usr/bin/msm-modem-uim-selection.sh"
cd "$project_dir"
sha256sum scripts/modem_uim.py scripts/configure-modem-uim.sh \
    scripts/update-modem-uim-image.sh scripts/verify-modem-uim-image.py \
    > "$log_dir/modem-uim-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/modem-uim-sources.sha256"
