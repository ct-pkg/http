#!/usr/bin/env bash
#
# tools/gates/g9.sh - G9, the spec pin.
#
# AGENTS.md records the master spec version and sha256 in its generated block.
# Both must match the master spec on disk; a pin that lags is drift, and a pin
# that runs ahead is a forged copy.
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g9_main() {
    agents="${AGENTS_MD:-$pkg/AGENTS.md}"
    master="${SPEC_MASTER:-/Users/samxx/trae-ws/.ai-engineering/methodology/ai-coding-workflow.md}"

    [ -f "$agents" ] || fail "g9: missing $agents"
    [ -f "$master" ] || fail "g9: master spec not found: $master"

    head="$(grep -m 1 '^<!-- spec:begin ' "$agents")"
    [ -n "$head" ] || fail "g9: no spec:begin block"
    pv="$(printf '%s\n' "$head" | sed -n 's/.*version=\([0-9][0-9.]*\).*/\1/p')"
    ps="$(printf '%s\n' "$head" | sed -n 's/.*sha256=\([0-9a-f]\{64\}\).*/\1/p')"
    [ -n "$pv" ] || fail "g9: block has no version"
    [ -n "$ps" ] || fail "g9: block has no sha256"

    mv="$(sed -n 's/^| spec-version | `\([0-9][0-9.]*\)`.*/\1/p' "$master" | head -n 1)"
    [ -n "$mv" ] || fail "g9: master spec has no spec-version row"
    ms="$(shasum -a 256 "$master" | awk '{print $1}')"

    [ "$ps" = "$ms" ] || fail "g9: spec-sha256 pin=$ps master=$ms"
    [ "$pv" = "$mv" ] || fail "g9: spec-version pin=$pv master=$mv"

    if ! awk -v a="$pv" -v b="$mv" 'BEGIN {
            split(a, x, "."); split(b, y, ".")
            for (i = 1; i <= 3; i++) if (x[i] + 0 != y[i] + 0) exit (x[i] + 0 < y[i] + 0)
            exit 0
        }'; then
        fail "g9: spec-version pin=$pv lags master=$mv"
    fi

    printf 'ok g9 (spec %s, sha256 matches master)\n' "$pv"
}

g9_main
