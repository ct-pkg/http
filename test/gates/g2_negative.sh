#!/usr/bin/env bash
#
# test/gates/g2_negative.sh - prove g2 rejects an incomplete or fake mapping.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cp "$repo/docs/requirements.md" "$tmp/req.md"
cp "$repo/docs/acceptance-map.md" "$tmp/map.md"

# 1. a requirement with no ACC row must be rejected
grep -v '^| ACC-24 ' "$tmp/map.md" >"$tmp/map-missing.md"
grep -q '^| ACC-24 ' "$tmp/map-missing.md" && { echo "FAIL g2-negative: tamper failed" >&2; exit 1; }
REQ_FILE="$tmp/req.md" MAP_FILE="$tmp/map-missing.md" \
    bash "$repo/tools/check.sh" g2 >"$tmp/out1" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out1" >&2; echo "FAIL g2-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'has no ACC-\* entry' "$tmp/out1" \
    || { cat "$tmp/out1" >&2; echo "FAIL g2-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected map/missing: %s\n' "$(head -n 1 "$tmp/out1")"

# 2. an anchor pointing at an unwired fixture must be rejected
sed 's|`bash test/run.sh p2-cleanup-once`|`bash test/run.sh no-such-fixture`|' \
    "$tmp/map.md" >"$tmp/map-anchor.md"
grep -q 'no-such-fixture' "$tmp/map-anchor.md" || { echo "FAIL g2-negative: tamper failed" >&2; exit 1; }
REQ_FILE="$tmp/req.md" MAP_FILE="$tmp/map-anchor.md" \
    bash "$repo/tools/check.sh" g2 >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out2" >&2; echo "FAIL g2-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'fixture not wired' "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g2-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected map/anchor: %s\n' "$(head -n 1 "$tmp/out2")"

printf 'ok g2-negative (2 rejected cases)\n'
