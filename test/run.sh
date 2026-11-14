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
            --outdir "$tmp/build-$task" \
            --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
    ) >"$log" 2>&1 || { cat "$log" >&2; fail "ct build failed"; }
    grep -q 'warning:' "$log" && { cat "$log" >&2; fail "build emitted warning:"; }
    "$tmp/$task" >"$tmp/$task.out" 2>&1 \
        || { cat "$tmp/$task.out" >&2; fail "fixture failed"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

# Fixtures that really perform need the loopback server linked in. The server
# is a plain C file, so --ldflags can carry it next to -lcurl.
run_ct_server() {
    log="$tmp/$task.build.log"
    (
        cd "$tmp" || exit 1
        "$CT_BIN" build "$fixtures/$task.ct" -I "$repo/src" -o "$task" \
            --outdir "$tmp/build-$task" \
            --cflags "-I $fixtures -Wall -Wextra -Werror" \
            --ldflags "$fixtures/p4-server.c -lcurl"
    ) >"$log" 2>&1 || { cat "$log" >&2; fail "ct build failed"; }
    grep -q 'warning:' "$log" && { cat "$log" >&2; fail "build emitted warning:"; }
    "$tmp/$task" >"$tmp/$task.out" 2>&1 \
        || { cat "$tmp/$task.out" >&2; fail "fixture failed"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

run_cc_server() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -Wall -Wextra -Werror -I "$repo/src/http" -I "$fixtures" \
        "$fixtures/$task.c" "$fixtures/p4-server.c" "$repo/src/http/http.shim.c" \
        -lcurl -o "$tmp/$task" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc failed"; }
    "$tmp/$task" >"$tmp/$task.out" 2>&1 \
        || { cat "$tmp/$task.out" >&2; fail "fixture failed"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

# A positive fixture must finish under ASAN with no report at all.
run_asan_positive_server() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -g -fsanitize=address -Wall -Wextra -Werror \
        -I "$repo/src/http" -I "$fixtures" \
        "$fixtures/$task.c" "$fixtures/p4-server.c" "$repo/src/http/http.shim.c" \
        -lcurl -o "$tmp/$task" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc -fsanitize=address failed"; }
    ASAN_OPTIONS=detect_leaks=0 "$tmp/$task" >"$tmp/$task.out" 2>&1
    rc=$?
    [ "$rc" -eq 0 ] || { cat "$tmp/$task.out" >&2; fail "sanitizer aborted a legal program"; }
    grep -q 'AddressSanitizer' "$tmp/$task.out" \
        && { cat "$tmp/$task.out" >&2; fail "sanitizer emitted a report"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s (ASAN clean)\n' "$task"
}

run_asan_negative_server() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -g -fsanitize=address -Wall -Wextra -Werror \
        -I "$repo/src/http" -I "$fixtures" \
        "$fixtures/$task.c" "$fixtures/p4-server.c" "$repo/src/http/http.shim.c" \
        -lcurl -o "$tmp/$task" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc -fsanitize=address failed"; }
    ASAN_OPTIONS=detect_leaks=0 "$tmp/$task" >"$tmp/$task.out" 2>&1
    rc=$?
    [ "$rc" -ne 0 ] || { cat "$tmp/$task.out" >&2; fail "sanitizer did not abort"; }
    grep -q 'double-free' "$tmp/$task.out" \
        || { cat "$tmp/$task.out" >&2; fail "no double-free report"; }
    printf 'ok %s (ASAN rejected the stale pointer)\n' "$task"
}

# The CT object must not reference curl: only http.shim.c may.
run_nm() {
    emit="$tmp/emit-$task"
    mkdir -p "$emit"
    log="$tmp/$task.build.log"
    (
        cd "$tmp" || exit 1
        "$CT_BIN" build "$fixtures/p2-cleanup-once.ct" -I "$repo/src" \
            --emit-c -o "$emit"
    ) >"$log" 2>&1 || { cat "$log" >&2; fail "ct build --emit-c failed"; }
    "$CC_BIN" -std=c99 -Wall -Wextra -Werror -I "$repo/src/http" -I "$emit" \
        -c "$emit/p2-cleanup-once.c" -o "$tmp/$task.o" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc -c failed"; }
    if nm -u "$tmp/$task.o" | grep -q '_curl_'; then
        nm -u "$tmp/$task.o" >&2
        fail "CT object references curl symbols"
    fi
    printf 'ok %s (no curl symbols in the CT object)\n' "$task"
}

run_cc() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -Wall -Wextra -Werror -I "$repo/src/http" \
        "$fixtures/$task.c" "$repo/src/http/http.shim.c" -lcurl \
        -o "$tmp/$task" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc failed"; }
    "$tmp/$task" >"$tmp/$task.out" 2>&1 \
        || { cat "$tmp/$task.out" >&2; fail "fixture failed"; }
    diff -u "$fixtures/$task.expected.out" "$tmp/$task.out" \
        || fail "output mismatch"
    printf 'ok %s\n' "$task"
}

# A negative fixture must abort under ASAN with a double-free report.
run_asan_negative() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -g -fsanitize=address -Wall -Wextra -Werror \
        -I "$repo/src/http" \
        "$fixtures/$task.c" "$repo/src/http/http.shim.c" -lcurl \
        -o "$tmp/$task" >"$log" 2>&1 \
        || { cat "$log" >&2; fail "cc -fsanitize=address failed"; }
    ASAN_OPTIONS=detect_leaks=0 "$tmp/$task" >"$tmp/$task.out" 2>&1
    rc=$?
    [ "$rc" -ne 0 ] || { cat "$tmp/$task.out" >&2; fail "sanitizer did not abort"; }
    grep -q 'double-free' "$tmp/$task.out" \
        || { cat "$tmp/$task.out" >&2; fail "no double-free report"; }
    printf 'ok %s (ASAN rejected the stale pointer)\n' "$task"
}

run_negative() {
    log="$tmp/$task.build.log"
    (
        cd "$tmp" || exit 1
        "$CT_BIN" build "$fixtures/$task.ct" -I "$repo/src" -o "$task" \
            --outdir "$tmp/build-$task" --cflags "-Wall -Wextra -Werror"
    ) >"$log" 2>&1
    rc=$?
    [ "$rc" -ne 0 ] || { cat "$log" >&2; fail "build unexpectedly succeeded"; }
    grep -qF "no method 'post' for type 'char *'" "$log" \
        || { cat "$log" >&2; fail "build failed without the expected 'no method' diagnostic"; }
    printf 'ok %s (rejected with the expected diagnostic)\n' "$task"
}

run_abi() {
    log="$tmp/$task.build.log"
    "$CC_BIN" -std=c99 -Wall -Wextra -Werror -I "$repo/src/http" \
        "$fixtures/phase1_abi.c" "$repo/src/http/http.shim.c" -lcurl \
        -o "$tmp/phase1_abi" >"$log" 2>&1 \
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
    run_ct_server
    ;;
p2-cleanup-once)
    run_ct
    ;;
p2-nm)
    run_nm
    ;;
p2-double-free)
    run_cc
    task=p2-double-free-bad
    run_asan_negative
    ;;
p2-after-send)
    run_ct_server
    ;;
p2-response-free)
    run_cc
    ;;
p2-ops)
    run_cc
    ;;
p3-url-own)
    run_cc
    ;;
p3-header-crlf)
    run_cc
    ;;
p3-body-nul)
    run_cc
    ;;
p3-scheme)
    run_cc
    ;;
p3-defaults)
    run_cc
    ;;
p3-chain)
    run_ct_server
    ;;
p4-status)
    run_cc_server
    ;;
p4-binary | p4-split)
    run_cc_server
    ;;
p4-take-body)
    run_asan_positive_server
    ;;
p4-take-body-bad)
    run_asan_negative_server
    ;;
p4-refused)
    run_cc
    ;;
p4-timeout)
    run_cc_server
    ;;
p5-local)
    run_cc
    ;;
p5-table)
    run_cc
    ;;
p5-http-error)
    run_cc_server
    ;;
p5-unknown)
    run_cc
    ;;
p5-predicates)
    run_cc_server
    ;;
p6-total)
    run_cc_server
    ;;
p6-connect)
    run_cc
    ;;
p6-slow)
    run_cc_server
    ;;
p6-idle)
    run_cc_server
    ;;
p6-negative)
    run_cc_server
    ;;
p7-chunks)
    run_asan_positive_server
    ;;
p7-body-nul | p7-chunked)
    run_cc_server
    ;;
p7-abort)
    run_cc_server
    ;;
phase1_no_mapping)
    run_negative
    ;;
*)
    echo "no wired fixture for '$task'; nothing ran" >&2
    exit 2
    ;;
esac
