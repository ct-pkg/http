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

# A manifest without the S1 identity keys must be refused by ct pkg check.
mkdir -p "$tmp/bad"
grep -vE '^(publisher|ns-root)[[:space:]]*=' "$repo/ct.pkg" >"$tmp/bad/ct.pkg"
grep -q '^name = http' "$tmp/bad/ct.pkg" \
    || { echo "FAIL g0-negative: manifest strip failed" >&2; exit 1; }

CT_PKG_DIR="$tmp/bad" bash "$repo/tools/check.sh" pkgcheck >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out2" >&2; echo "FAIL g0-negative: expected rc=1 on pkg check, got $rc" >&2; exit 1; }
grep -q "is missing 'publisher'" "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g0-negative: wrong pkg diagnostic" >&2; exit 1; }
printf '  rejected pkg-manifest: %s\n' "$(grep "is missing 'publisher'" "$tmp/out2" | head -n 1)"
printf 'ok g0-negative (2 rejected cases)\n'
