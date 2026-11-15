#!/usr/bin/env bash
#
# test/gates/g7_negative.sh - prove the secret scan rejects a key-shaped string.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/src"
printf 'key = AKIAABCDEFGHIJKLMNOP\n' >"$tmp/src/leak.txt"
REPO="$tmp" G7_ONLY=scan bash "$repo/tools/check.sh" g7 >"$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out" >&2; echo "FAIL g7-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'possible secret' "$tmp/out" \
    || { cat "$tmp/out" >&2; echo "FAIL g7-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected scan/secret: %s\n' "$(head -n 1 "$tmp/out")"
printf 'ok g7-negative (1 rejected case)\n'
