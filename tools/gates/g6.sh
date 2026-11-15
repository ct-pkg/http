#!/usr/bin/env bash
#
# tools/gates/g6.sh - G6, the ledger closes.
#
# Every issue has a legal status word and non-empty evidence; `fixed` never
# appears as a status (it belongs in the disposition paragraph); an unmet exit
# criterion in a closeout points at an errata entry or an issue; a version
# mismatch must be filed (V-6).
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g6_main() {
    repo="${REPO:-$pkg}"
    issues="${ISSUE_DIR:-$repo/issues}"
    phases="${PHASES_DIR:-$repo/docs/phases}"
    errata="${ERRATA_FILE:-$repo/docs/errata.md}"
    ctpkg="${CT_PKG:-$repo/ct.pkg}"
    readme="${README_MD:-$repo/README.md}"

    [ -d "$issues" ] || fail "g6: missing issue ledger: $issues"
    n_issues=0

    for f in "$issues"/*.md; do
        [ -f "$f" ] || continue
        n_issues=$((n_issues + 1))
        base="$(basename "$f")"
        st="$(sed -n 's/^- status[:：][[:space:]]*\([a-z]*\).*/\1/p' "$f" | head -n 1)"
        [ -n "$st" ] || fail "g6: $base: missing '- status:'"
        case "$st" in
        open|verified|reopened|rejected|deferred|wontfix|superseded) ;;
        fixed) fail "g6: $base: 'fixed' must be a disposition, not a status (5.4)" ;;
        *) fail "g6: $base: illegal status '$st'" ;;
        esac
        ev="$(sed -n 's/^- 证据[:：][[:space:]]*\(.*\)/\1/p' "$f" | head -n 1)"
        [ -n "$ev" ] || fail "g6: $base: empty evidence field"
        ph="$(sed -n 's/^- phase[:：][[:space:]]*\(PH-[0-9][0-9]*\).*/\1/p' "$f" | head -n 1)"
        [ -n "$ph" ] || fail "g6: $base: missing '- phase: PH-<n>'"
        [ -f "$phases/$ph/card.md" ] || fail "g6: $base: phase $ph has no card"
        case "$st" in
        rejected|wontfix|deferred|superseded)
            grep -qE '^- (理由|触发条件)[:：]' "$f" || fail "g6: $base: status $st needs a reason/trigger" ;;
        esac
    done
    [ "$n_issues" -gt 0 ] || fail "g6: the ledger is empty; an empty ledger is not a closed ledger"

    # An unmet criterion must be traceable: errata (anchor bug) or an issue.
    for co in "$phases"/PH-*/closeout.md; do
        [ -f "$co" ] || continue
        ph="$(basename "$(dirname "$co")")"
        if grep -qE '不计通过|未满足' "$co"; then
            hit=0
            [ -f "$errata" ] && grep -q "$ph" "$errata" && hit=1
            grep -lE "^- phase[:：][[:space:]]*$ph" "$issues"/*.md >/dev/null 2>&1 && hit=1
            [ "$hit" -eq 1 ] || fail "g6: $ph has an unmet criterion with no errata or issue"
        fi
    done

    # V-6: a version landing mismatch is a defect and cannot be left unfiled.
    if [ -f "$ctpkg" ] && [ -f "$readme" ]; then
        pv="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
        rv="$(sed -n 's/^- 版本：\([0-9][0-9.]*\).*/\1/p' "$readme" | head -n 1)"
        if [ "$pv" != "$rv" ]; then
            grep -lE '^# .*版本' "$issues"/*.md >/dev/null 2>&1 \
                || fail "g6: version mismatch ct.pkg=$pv README=$rv has no issue (V-6)"
        fi
    fi

    if [ -f "$repo/docs/pool.md" ]; then
        bad="$(grep -E '^\| ' "$repo/docs/pool.md" | grep -vE '\| (open|closed|deferred|rejected|superseded) \|' | grep -v '^| ---' || true)"
        [ -z "$bad" ] || fail "g6-pool: illegal pool status: $bad"
    else
        printf 'note g6-pool: no docs/pool.md in this project\n' >&2
    fi

    printf 'ok g6 (%d issues, status/evidence legal, unmet criteria traced)\n' "$n_issues"
}

g6_main
