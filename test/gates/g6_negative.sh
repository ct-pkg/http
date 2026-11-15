#!/usr/bin/env bash
#
# test/gates/g6_negative.sh - prove g6 rejects a `fixed` status and an illegal
# status word.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/issues"
printf -- '- status: fixed\n- 证据: d0160db\n- phase: PH-9\n' >"$tmp/issues/HTTP-9.md"
ISSUE_DIR="$tmp/issues" bash "$repo/tools/check.sh" g6 >"$tmp/out1" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out1" >&2; echo "FAIL g6-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'must be a disposition' "$tmp/out1" \
    || { cat "$tmp/out1" >&2; echo "FAIL g6-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected issue/fixed-status: %s\n' "$(head -n 1 "$tmp/out1")"

printf -- '- status: merged\n- 证据: d0160db\n- phase: PH-9\n' >"$tmp/issues/HTTP-9.md"
ISSUE_DIR="$tmp/issues" bash "$repo/tools/check.sh" g6 >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out2" >&2; echo "FAIL g6-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'illegal status' "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g6-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected issue/illegal-status: %s\n' "$(head -n 1 "$tmp/out2")"

printf 'ok g6-negative (2 rejected cases)\n'
