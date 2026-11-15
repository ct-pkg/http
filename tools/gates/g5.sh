#!/usr/bin/env bash
#
# tools/gates/g5.sh - G5, tests prove themselves.
#
# Each phase task must have red evidence with `baseline: <sha>`, that sha must
# be a real ancestor of HEAD, and the record must carry raw output plus an exit
# code. Each gate must ship a negative self-test that actually rejects a broken
# copy (a gate that cannot go red is not a gate).
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g5_main() {
    repo="${REPO:-$pkg}"
    phases="${PHASES_DIR:-$repo/docs/phases}"
    reviews="${REVIEWS_DIR:-$repo/docs/reviews}"
    only="${G5_ONLY:-}"

    n_tasks=0
    n_evidence=0

    if [ -z "$only" ] || [ "$only" = evidence ]; then
        for card in "$phases"/PH-*/card.md; do
            [ -f "$card" ] || continue
            ph="$(basename "$(dirname "$card")")"
            tasks="$(awk '
                /^## 4\. 任务清单/ { inb = 1; next }
                inb && /^## / { exit }
                inb && /^\| T[0-9]+ / { split($0, a, "|"); gsub(/^[ \t]+|[ \t]+$/, "", a[2]); print a[2] }
            ' "$card")"
            for t in $tasks; do
                n_tasks=$((n_tasks + 1))
                ev=""
                for cand in "$phases/$ph/red/$t.md" "$reviews/$ph/$t.md"; do
                    [ -f "$cand" ] && ev="$cand" && break
                done
                [ -n "$ev" ] || fail "g5: $ph/$t has no red evidence (red/$t.md or reviews/$ph/$t.md)"
                g5_check_evidence "$repo" "$ev" "$ph/$t"
                n_evidence=$((n_evidence + 1))
            done
        done
        [ "$n_tasks" -gt 0 ] || fail "g5: no phase task found"
    fi

    if [ -z "$only" ] || [ "$only" = gates ]; then
        # g5 itself is proven by test/gates/g5_negative.sh from the closeout;
        # every other gate runs its own negative here.
        for g in g0 g1 g2 g3 g4 g6 g7 g8 g9; do
            script="$repo/test/gates/${g}_negative.sh"
            [ -f "$script" ] || fail "g5: missing negative self-test: test/gates/${g}_negative.sh"
            out="$g4_tmp_g5/${g}.out"
            mkdir -p "$g4_tmp_g5"
            bash "$script" >"$out" 2>&1 || { cat "$out" >&2; fail "g5: ${g}_negative.sh did not exit 0"; }
            grep -q 'rejected' "$out" || { cat "$out" >&2; fail "g5: ${g}_negative.sh printed no 'rejected' line"; }
        done
        [ -f "$repo/test/gates/g5_negative.sh" ] || fail "g5: missing test/gates/g5_negative.sh"
    fi

    printf 'ok g5 (%d task evidence files, %d gate negatives exercised)\n' "$n_evidence" 9
}

# g5_check_evidence <repo> <file> <label>
g5_check_evidence() {
    repo="$1"
    file="$2"
    label="$3"

    sha="$(sed -n 's/.*baseline[：:][[:space:]]*`\{0,1\}\([0-9a-f]\{7,40\}\).*/\1/p' "$file" | head -n 1)"
    [ -n "$sha" ] || fail "g5: $label: no 'baseline: <sha>' field"
    git -C "$repo" cat-file -e "$sha^{commit}" 2>/dev/null \
        || fail "g5: $label: baseline $sha is not a commit"
    git -C "$repo" merge-base --is-ancestor "$sha" HEAD 2>/dev/null \
        || fail "g5: $label: baseline $sha is not an ancestor of HEAD"

    [ "$(grep -c '^```' "$file")" -ge 2 ] \
        || fail "g5: $label: no fenced raw output"
    grep -qE '退出码|rc[=：]' "$file" || fail "g5: $label: no exit code in the record"
    grep -qE 'green|转绿|rc[=：][[:space:]]*0' "$file" \
        || fail "g5: $label: no green record (green/转绿 or rc=0)"
}

g4_tmp_g5="$(mktemp -d)"
trap 'rm -rf "$g4_tmp_g5"' EXIT
g5_main
