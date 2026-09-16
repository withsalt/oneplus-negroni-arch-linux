#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
branch=${1:-}
if [[ -n $branch ]]; then
    image_dir="images/$branch"
    log_dir="logs/$branch"
    mkdir -p "$image_dir" "$log_dir"
    git -C linux fetch --depth 1 origin "$branch"
    if ! git -C linux show-ref --verify --quiet "refs/heads/$branch"; then
        git -C linux checkout -b "$branch" "origin/$branch"
    else
        git -C linux checkout "$branch"
    fi
    export IMAGE_DIR="$image_dir" LOG_DIR="$log_dir"
else
    image_dir=images
    log_dir=logs
fi
# Let Kbuild check changed sources even when a previous build completed.
bash scripts/build-kernel.sh
if [[ ! -f "$log_dir/rootfs.packages-complete" ]]; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/prepare-rootfs.sh
fi
if [[ ! -f "$log_dir/rootfs.complete" ]]; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/finalize-rootfs.sh
fi
if ! cmp -s "$log_dir/kernel-artifacts.sha256" "$log_dir/rootfs-kernel.sha256" ||
   [[ ! -f "$log_dir/charge-limit-sources.sha256" ]] ||
   ! sha256sum --check --status "$log_dir/charge-limit-sources.sha256"; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/update-kernel-image.sh
fi
if [[ ! -f "$log_dir/ssh.complete" ]]; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/enable-ssh-image.sh
fi
if [[ ! -f "$log_dir/gpu-runtime-pm.complete" ]]; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/enable-gpu-runtime-active-image.sh
fi
if [[ ! -f "$log_dir/screen-defaults-sources.sha256" ]] ||
   ! sha256sum --check --status "$log_dir/screen-defaults-sources.sha256"; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/update-screen-defaults-image.sh
fi
if ! cmp -s config/packages/common-tools.txt "$log_dir/common-tools.list"; then
    sudo env IMAGE_DIR="$image_dir" LOG_DIR="$log_dir" bash scripts/install-common-tools-image.sh
fi
if [[ ! -f "$log_dir/images.complete" ]]; then bash scripts/pack-images.sh; fi
printf '\nImages are in %s/%s\n' "$PWD" "$image_dir"
