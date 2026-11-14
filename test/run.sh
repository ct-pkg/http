#!/usr/bin/env bash
#
# test/run.sh - run one phase fixture.
#
#   bash test/run.sh phase1_abi
#
# Each fixture is built with zero warnings, run, and compared byte-for-byte
# against <task>.expected.out. A task without a wired fixture exits 2 instead
# of reporting a vacuous pass.
#
# License: MIT (see LICENSE).

set -u

task="${1:-}"
if [ -z "$task" ]; then
    echo "usage: bash test/run.sh <task-id>" >&2
    exit 2
fi

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"
fixtures="$repo/test/fixtures"
CT_BIN="${CT_BIN:-/Users/samxx/.local/bin/ct}"
CC_BIN="${CC:-cc}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
    printf 'FAIL %s: %s\n' "$task" "$1" >&2
    exit 1
}

run_ct() {
    log="$tmp/$task.build.log"
    (
        cd "$tmp" || exit 1
        "$CT_BIN" build "$fixtures/$task.ct" -I "$repo/src" -o "$task" \
            --outdir "$tmp/$task" --cflags "-Wall -Wextra -Werror"
    ) >"$log" 2>&1 || { cat "$log" >&2; fail "ct build failed"; }
    grep -q 'warning:' "$log" && { cat "$log" >&2; fail "build emitted warning:"; }
    "$tmp/$task" >"$tmp/$task.out" 2>&1 \
        || { cat "$tmp/$task.out" >&2; fail "fixture failed"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

run_abi() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -Wall -Wextra -Werror -I "$repo/src/http" \
        "$fixtures/phase1_abi.c" -o "$tmp/phase1_abi" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc failed"; }
    "$tmp/phase1_abi" >"$tmp/phase1_abi.out" 2>&1 \
        || { cat "$tmp/phase1_abi.out" >&2; fail "abi fixture failed"; }
    diff -u "$fixtures/phase1_abi.expected.out" "$tmp/phase1_abi.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

case "$task" in
phase1_abi)
    run_abi
    ;;
phase1_chain | phase1_var)
    run_ct
    ;;
*)
    echo "no wired fixture for '$task'; nothing ran" >&2
    exit 2
    ;;
esac
