#!/usr/bin/env bash
#
# test/gates/g1_negative.sh - prove that g1 can fail.
#
#   bash test/gates/g1_negative.sh
#
# A gate that cannot reject a broken card is not a gate. This self-test takes
# every live phase card, breaks one rule in each of three copies, and requires
# `tools/check.sh g1` to exit 1 with a `FAIL g1` line every time.
#
# License: MIT (see LICENSE).

set -u

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
    printf 'FAIL g1-negative: %s\n' "$1" >&2
    exit 1
}

expect_reject() {
    local label="$1"
    local file="$2"
    local out
    local rc
    out="$tmp/$(printf '%s' "$label" | tr '/' '_').out"

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

found=0
total=0
for card in "$repo"/docs/phases/PH-*/card.md; do
    [ -f "$card" ] || continue
    found=1
    local_label="$(basename "$(dirname "$card")")"

    # 1. change an exit criterion -> frozen digest must reject
    awk '/^\| E-P[0-9]+-[0-9]+ / && !done { sub(/ \|[ \t]*$/, " tampered |"); done = 1 } { print }' \
        "$card" >"$tmp/$local_label-criteria.md"
    # 2. mark a task with effort L -> schema must reject
    awk -F'|' 'BEGIN { OFS = "|" } /^\| T[0-9]+ / && !done { $7 = " L "; done = 1 } { print }' \
        "$card" >"$tmp/$local_label-effort.md"
    # 3. drop a required card item -> the ten-item check must reject
    grep -v '^## 9. 收口动作$' "$card" >"$tmp/$local_label-missing.md"

    grep -q ' tampered |' "$tmp/$local_label-criteria.md" || fail "$local_label: could not build the criteria tamper case"
    grep -q '| L |' "$tmp/$local_label-effort.md" || fail "$local_label: could not build the effort tamper case"
    grep -q '^## 9. 收口动作$' "$tmp/$local_label-missing.md" && fail "$local_label: could not build the missing-item case"

    expect_reject "$local_label/criteria" "$tmp/$local_label-criteria.md"
    expect_reject "$local_label/effort" "$tmp/$local_label-effort.md"
    expect_reject "$local_label/missing" "$tmp/$local_label-missing.md"
    total=$((total + 3))
done

[ "$found" -eq 1 ] || fail "no docs/phases/PH-*/card.md"
[ "$total" -gt 0 ] || fail "no tampered card was exercised"

printf 'ok g1-negative (%d tampered cards rejected)\n' "$total"
