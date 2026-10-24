#!/usr/bin/env bash
# Restore the pinned kernel, including commits not yet published upstream.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
branch=$(git config -f .gitmodules submodule.linux.branch)
bundle="$project_dir/patches/linux/local-$branch.bundle"
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
restore_bundle=false
if ! git -C linux cat-file -e "$pin^{commit}" 2>/dev/null; then
    restore_bundle=true
fi
# The independent rc2 head keeps the historical CPU comparison reproducible.
# It is deliberately not an ancestor of the reimplemented 7.3 branch.
while read -r bundled_pin bundled_ref; do
    if ! git -C linux cat-file -e "$bundled_pin^{commit}" 2>/dev/null; then
        restore_bundle=true
    fi
done < <(git bundle list-heads "$bundle")
if [[ $restore_bundle == true ]]; then
    # Bundle prerequisites are published commits; stop at the blank line
    # before the binary pack data. Independent heads have separate prerequisites.
    while IFS=' ' read -r entry rest; do
        [[ -n $entry ]] || break
        [[ $entry == -* ]] || continue
        base=${entry#-}
        if ! git -C linux cat-file -e "$base^{commit}" 2>/dev/null; then
            git -C linux fetch --depth 1 origin "$base"
        fi
    done < "$bundle"
    git -C linux bundle verify "$bundle"
    git -C linux fetch "$bundle" '+refs/heads/*:refs/remotes/migration/*'
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
if ! git -C linux show-ref --verify --quiet "refs/heads/$branch"; then
    git -C linux branch "$branch" "$pin"
elif [[ $(git -C linux rev-parse "$branch") != "$pin" ]] &&
     git -C linux merge-base --is-ancestor "$branch" "$pin"; then
    # submodule update detached HEAD at the pin; advance an older local build
    # branch so build.sh cannot switch back to the previous bundled kernel.
    git -C linux branch -f "$branch" "$pin"
fi
if [[ $(git -C linux rev-parse "$branch") == "$pin" ]]; then
    git -C linux checkout "$branch"
fi
git submodule update --init tools/avb tools/msm-firmware-loader
git submodule status
