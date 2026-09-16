#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=${1:?Usage: configure-charge-limit.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
log_dir=${2:?Usage: configure-charge-limit.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
[[ $EUID == 0 && $target_dir != / ]] || exit 1
test -x "$target_dir/usr/bin/python3"

install -d -o 0 -g 0 -m 0755 "$target_dir/etc/systemd/system"
install -d -o 0 -g 0 -m 0755 "$target_dir/usr/local/libexec"
mkdir -p "$log_dir"
install -o 0 -g 0 -m 0644 "$project_dir/config/systemd/negroni-charge-limit.service" "$target_dir/etc/systemd/system/negroni-charge-limit.service"
install -o 0 -g 0 -m 0755 "$project_dir/config/charge-limit/negroni-charge-limit" "$target_dir/usr/local/libexec/negroni-charge-limit"
install -o 0 -g 0 -m 0644 "$project_dir/config/charge-limit/negroni-charge-limit.conf" "$target_dir/etc/negroni-charge-limit.conf"

systemctl --root="$target_dir" disable negroni-charge-limit.service
unit_state=$(systemctl --root="$target_dir" is-enabled negroni-charge-limit.service || true)
[[ $unit_state == disabled ]]
grep -Fxq 'START_THRESHOLD=75' "$target_dir/etc/negroni-charge-limit.conf"
grep -Fxq 'END_THRESHOLD=80' "$target_dir/etc/negroni-charge-limit.conf"
echo 'Negroni charge limit: installed, disabled by default' > "$log_dir/charge-limit-config.txt"
echo 'Default start threshold: 75%' >> "$log_dir/charge-limit-config.txt"
echo 'Default end threshold: 80%' >> "$log_dir/charge-limit-config.txt"
echo 'Enable: systemctl enable --now negroni-charge-limit.service' >> "$log_dir/charge-limit-config.txt"
echo 'Disable: systemctl disable --now negroni-charge-limit.service' >> "$log_dir/charge-limit-config.txt"
touch "$log_dir/charge-limit-config.complete"
cd "$project_dir"
sha256sum config/charge-limit/negroni-charge-limit config/charge-limit/negroni-charge-limit.conf config/systemd/negroni-charge-limit.service scripts/configure-charge-limit.sh > "$log_dir/charge-limit-sources.sha256"
