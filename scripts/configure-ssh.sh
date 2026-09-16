#!/usr/bin/env bash
# Configure an already mounted build rootfs; do not start its services here.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=${1:?Usage: configure-ssh.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
log_dir=${2:?Usage: configure-ssh.sh ROOTFS_DIRECTORY LOG_DIRECTORY}
[[ $EUID == 0 && $target_dir != / ]] || exit 1
chroot "$target_dir" pacman -Q openssh
chroot "$target_dir" id oneplus >/dev/null
install -d -m 0755 "$target_dir/etc/ssh/sshd_config.d"
mkdir -p "$log_dir"
install -o 0 -g 0 -m 0644 "$project_dir/config/ssh/00-negroni.conf" \
    "$target_dir/etc/ssh/sshd_config.d/00-negroni.conf"
chroot "$target_dir" systemctl enable sshd.service
# Arch's sshd unit starts sshdgenkeys first, generating device-specific keys.
test -f "$target_dir/usr/lib/systemd/system/sshdgenkeys.service"
chroot "$target_dir" systemctl is-enabled sshd.service
# sshd validates the configuration only when a host key is available. Keep
# this temporary key in /run (a tmpfs during image builds), never in /etc/ssh.
check_dir=$(mktemp -d "$target_dir/run/ssh-check.XXXXXX")
cleanup() {
    for path in "$check_dir/host_key" "$check_dir/host_key.pub"; do
        [[ ! -e $path ]] || unlink "$path"
    done
    rmdir "$check_dir"
}
trap cleanup EXIT
check_key=${check_dir#"$target_dir"}/host_key
chroot "$target_dir" ssh-keygen -q -t ed25519 -N '' -f "$check_key"
chroot "$target_dir" /usr/bin/sshd -t -h "$check_key"
chroot "$target_dir" /usr/bin/sshd -T \
    -h "$check_key" -C user=oneplus,host=localhost,addr=127.0.0.1 > "$log_dir/sshd-effective.conf"
for setting in 'port 22' 'permitrootlogin no' 'allowusers oneplus' \
    'passwordauthentication yes' 'pubkeyauthentication yes' \
    'permitemptypasswords no' 'kbdinteractiveauthentication no' 'usepam yes'; do
    grep -Fixq "$setting" "$log_dir/sshd-effective.conf"
done
# Refuse to publish an image with pre-generated, shared SSH host keys.
shopt -s nullglob
host_keys=("$target_dir"/etc/ssh/ssh_host_*_key*)
(( ${#host_keys[@]} == 0 )) || { echo 'Image contains SSH host keys.' >&2; exit 1; }
touch "$log_dir/ssh.complete"
