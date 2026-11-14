#!/usr/bin/env bash
#
# test/gates/g1_negative.sh - prove that g1 can fail.
#
#   bash test/gates/g1_negative.sh
#
# A gate that cannot reject a broken card is not a gate. This self-test takes
# three copies of the live card, breaks one rule in each, and requires
# `tools/check.sh g1` to exit 1 with a `FAIL g1` line every time.
#
# License: MIT (see LICENSE).

set -u

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
card="$repo/docs/phases/PH-1/card.md"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
    printf 'FAIL g1-negative: %s\n' "$1" >&2
    exit 1
}

expect_reject() {
    label="$1"
    file="$2"
    out="$tmp/$label.out"

    CARD_FILE="$file" bash "$repo/tools/check.sh" g1 >"$out" 2>&1
    rc=$?
    if [ "$rc" -ne 1 ]; then
        cat "$out" >&2
        fail "$label: expected rc=1, got $rc"
    fi
    grep -q 'FAIL g1' "$out" || {
        cat "$out" >&2
        fail "$label: rejected without a 'FAIL g1' line"
    }
    printf '  rejected %s: %s\n' "$label" "$(head -n 1 "$out")"
}

[ -f "$card" ] || fail "missing card: $card"

# 1. change an exit criterion -> frozen digest must reject
sed 's/^| E-P1-2 |/| E-P1-2 | tampered /' "$card" >"$tmp/criteria.md"
# 2. mark a task with effort L -> schema must reject
awk -F'|' 'BEGIN { OFS = "|" } /^\| T[0-9]+ / && !done { $7 = " L "; done = 1 } { print }' \
    "$card" >"$tmp/effort.md"
# 3. drop a required card item -> the ten-item check must reject
grep -v '^## 9. 收口动作$' "$card" >"$tmp/missing.md"

grep -q '^| E-P1-2 | tampered ' "$tmp/criteria.md" || fail "could not build the criteria tamper case"
grep -q '| L |' "$tmp/effort.md" || fail "could not build the effort tamper case"
grep -q '^## 9. 收口动作$' "$tmp/missing.md" && fail "could not build the missing-item case"

expect_reject criteria "$tmp/criteria.md"
expect_reject effort "$tmp/effort.md"
expect_reject missing "$tmp/missing.md"

printf 'ok g1-negative (3 tampered cards rejected)\n'
