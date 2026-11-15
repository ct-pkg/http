#!/usr/bin/env bash
#
# test/gates/g8_negative.sh - prove g8-version rejects a landing mismatch.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

sed 's/^version[[:space:]]*=.*/version = 9.9.9/' "$repo/ct.pkg" >"$tmp/ct.pkg"
cp "$repo/README.md" "$tmp/README.md"
cp "$repo/CHANGELOG.md" "$tmp/CHANGELOG.md"

G8_ONLY=version CT_PKG="$tmp/ct.pkg" README_MD="$tmp/README.md" \
    CHANGELOG_MD="$tmp/CHANGELOG.md" \
    bash "$repo/tools/check.sh" g8 >"$tmp/out" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out" >&2; echo "FAIL g8-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'g8-version' "$tmp/out" \
    || { cat "$tmp/out" >&2; echo "FAIL g8-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected version/mismatch: %s\n' "$(head -n 1 "$tmp/out")"
printf 'ok g8-negative (1 rejected case)\n'
