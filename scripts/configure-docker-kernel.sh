#!/usr/bin/env bash
# Apply the Docker requirements to both new and existing kernel configurations.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
output_dir=${1:-linux/output}
mkdir -p "$output_dir"
output_dir=$(realpath "$output_dir")
cd linux
make_args=(O="$output_dir" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-)
if [[ ! -f "$output_dir/.config" ]]; then
    make "${make_args[@]}" defconfig
    make "${make_args[@]}" sm8450.config
fi
KCONFIG_CONFIG="$output_dir/.config" bash scripts/kconfig/merge_config.sh \
    -m -O "$output_dir" "$output_dir/.config" "$project_dir/config/kernel/docker.config"
make "${make_args[@]}" olddefconfig
python3 "$project_dir/scripts/verify-docker-kernel.py" --config-only "$output_dir/.config"
