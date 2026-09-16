#!/usr/bin/env bash
# Restore the pinned kernel, including commits not yet published upstream.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
bundle="$project_dir/patches/linux/local-7.3-rc2.bundle"
base=e8291a2765b80f94c047d28ca514465c7716c374
pin=$(git ls-files --stage linux | awk '$1 == "160000" { print $2 }')
[[ -n $pin ]] || { echo 'Missing linux gitlink in the parent repository.' >&2; exit 1; }
git submodule init linux
if [[ ! -e linux/.git ]]; then
    if [[ -d linux && -n $(find linux -mindepth 1 -maxdepth 1 -print -quit) ]]; then
        echo 'linux/ is nonempty and is not a Git checkout; refusing to overwrite it.' >&2
        exit 1
    fi
    git init linux
    git -C linux remote add origin "$(git config submodule.linux.url)"
fi
if ! git -C linux cat-file -e "$pin^{commit}" 2>/dev/null; then
    if ! git -C linux cat-file -e "$base^{commit}" 2>/dev/null; then
        git -C linux fetch --depth 1 origin "$base"
    fi
    git -C linux bundle verify "$bundle"
    git -C linux fetch "$bundle" refs/heads/7.3-rc2:refs/remotes/migration/7.3-rc2
fi
git -C linux cat-file -e "$pin^{commit}"
if [[ -n $(git -C linux status --porcelain) ]]; then
    echo 'linux/ has local changes; commit or stash them before initializing.' >&2
    exit 1
fi
if ! git -C linux rev-parse --verify HEAD >/dev/null 2>&1; then
    git -C linux checkout --detach "$pin"
fi
git submodule absorbgitdirs linux
git submodule update --init --no-fetch linux
if ! git -C linux show-ref --verify --quiet refs/heads/7.3-rc2; then
    git -C linux branch 7.3-rc2 "$pin"
fi
if [[ $(git -C linux rev-parse 7.3-rc2) == "$pin" ]]; then
    git -C linux checkout 7.3-rc2
fi
git submodule update --init tools/avb tools/msm-firmware-loader
git submodule status
