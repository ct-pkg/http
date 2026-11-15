#!/usr/bin/env bash
#
# test/gates/g3_negative.sh - prove g3's fixtures can go red.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# 1. a wrong expected output must fail the fixture, not pass silently
cp "$repo/test/fixtures/p3-url-own.c" "$tmp/"
printf 'owned 0 url https://example.test/a\n' >"$tmp/p3-url-own.expected.out"
FIXTURES_DIR="$tmp" bash "$repo/test/run.sh" p3-url-own >"$tmp/out1" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out1" >&2; echo "FAIL g3-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'output mismatch' "$tmp/out1" \
    || { cat "$tmp/out1" >&2; echo "FAIL g3-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected fixture/output: %s\n' "$(head -n 1 "$tmp/out1")"

# 2. an unwired task must exit 2, never look green
bash "$repo/test/run.sh" no-such-fixture >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 2 ] || { cat "$tmp/out2" >&2; echo "FAIL g3-negative: expected rc=2, got $rc" >&2; exit 1; }
grep -q 'nothing ran' "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g3-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected fixture/unwired: %s\n' "$(head -n 1 "$tmp/out2")"

printf 'ok g3-negative (2 rejected cases)\n'
