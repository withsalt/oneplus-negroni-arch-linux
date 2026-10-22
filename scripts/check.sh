#!/usr/bin/env bash
# Run host checks without root privileges, image mounts or device access.
set -euo pipefail
cd "$(dirname "$0")/.."

with_kernel=false
case "${1:-}" in
    '') ;;
    --with-kernel) with_kernel=true ;;
    --help|-h)
        printf 'Usage: bash scripts/check.sh [--with-kernel]\n'
        exit 0
        ;;
    *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { printf 'Expected at most one option.\n' >&2; exit 2; }

printf 'Checking Bash syntax...\n'
bash -n build.sh
for script in scripts/*.sh; do
    bash -n "$script"
done

printf 'Checking Python syntax...\n'
python3 - <<'PY'
from pathlib import Path

paths = sorted(Path("scripts").glob("*.py")) + sorted(Path("tests").glob("*.py"))
paths.append(Path("config/charge-limit/negroni-charge-limit"))
paths.append(Path("config/cpu-performance/negroni-cpu-performance"))
for path in paths:
    compile(path.read_bytes(), str(path), "exec")
print(f"{len(paths)} Python files passed")
PY

printf 'Running charge-limit service tests...\n'
python3 -B tests/test-charge-limit-service.py

if [[ $with_kernel == true ]]; then
    for source in qcom_battmgr.c oplus_battmgr.c qcom_battmgr_charge_limit.h; do
        [[ -f "linux/drivers/power/supply/$source" ]] || {
            printf 'Missing kernel source. Run: bash scripts/init-submodules.sh\n' >&2
            exit 1
        }
    done
    check_dir=$(mktemp -d "${TMPDIR:-/tmp}/negroni-check.XXXXXX")
    trap 'rm -rf -- "$check_dir"' EXIT
    printf 'Running charge-limit policy and driver tests...\n'
    cc -std=c11 -Wall -Wextra -Werror tests/charge-limit-policy.c -o "$check_dir/charge-limit-policy"
    "$check_dir/charge-limit-policy"
    python3 -B tests/test-charge-limit-driver.py
    printf 'Checking Docker configuration in fresh and existing kernel outputs...\n'
    python3 -B tests/test-docker-config.py
fi

printf 'All requested checks passed.\n'
