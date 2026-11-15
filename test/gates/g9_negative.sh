#!/usr/bin/env bash
#
# test/gates/g9_negative.sh - prove g9 rejects a forged spec pin.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

sed 's/sha256=[0-9a-f]\{64\}/sha256=0000000000000000000000000000000000000000000000000000000000000000/' \
    "$repo/AGENTS.md" >"$tmp/AGENTS.md"
grep -q 'sha256=0000000000000000000000000000000000000000000000000000000000000000' "$tmp/AGENTS.md" \
    || { echo "FAIL g9-negative: tamper failed" >&2; exit 1; }

AGENTS_MD="$tmp/AGENTS.md" bash "$repo/tools/check.sh" g9 >"$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out" >&2; echo "FAIL g9-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'spec-sha256' "$tmp/out" \
    || { cat "$tmp/out" >&2; echo "FAIL g9-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected pin/forged: %s\n' "$(head -n 1 "$tmp/out")"
printf 'ok g9-negative (1 rejected case)\n'
