#!/usr/bin/env bash
#
# test/gates/g0_negative.sh - prove g0 can reject a broken landing.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

sed 's/^version[[:space:]]*=.*/version = 9.9.9/' "$repo/ct.pkg" >"$tmp/ct.pkg"
grep -q '^version = 9.9.9' "$tmp/ct.pkg" || { echo "FAIL g0-negative: tamper failed" >&2; exit 1; }

CT_PKG="$tmp/ct.pkg" README_MD="$repo/README.md" \
    bash "$repo/tools/check.sh" g0 >"$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out" >&2; echo "FAIL g0-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'version landing differs' "$tmp/out" \
    || { cat "$tmp/out" >&2; echo "FAIL g0-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected version-landing: %s\n' "$(head -n 1 "$tmp/out")"
printf 'ok g0-negative (1 rejected case)\n'
