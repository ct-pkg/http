#!/usr/bin/env bash
#
# tools/gates/g2.sh - G2, design verifiability.
#
# Every requirement in docs/requirements.md must map to at least one ACC-*
# entry in docs/acceptance-map.md, and every entry must carry an anchor this
# repository can actually run or find. A mapping table nobody checks is worse
# than no table at all.
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g2_main() {
    req="${REQ_FILE:-$pkg/docs/requirements.md}"
    map="${MAP_FILE:-$pkg/docs/acceptance-map.md}"
    run_sh="${RUN_SH:-$pkg/test/run.sh}"

    [ -f "$req" ] || fail "missing requirements: $req"
    [ -f "$map" ] || fail "missing acceptance map: $map"
    [ -f "$run_sh" ] || fail "missing test runner: $run_sh"

    # Case labels explain what `bash test/run.sh <task>` can actually run.
    tasks="$(sed -n 's|^\([a-z0-9_| -]*\))$|\1|p' "$run_sh" | tr '|' '\n' |
        sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | grep -v '^\*$' | sort -u)"

    grep -E '^### R-[0-9]{2} ' "$req" | sed 's/^### \(R-[0-9][0-9]\).*/\1/' >"$tmp_g2_req"

    n_rows=0
    while IFS= read -r row; do
        [ -n "$row" ] || continue
        n_rows=$((n_rows + 1))
        id="$(printf '%s\n' "$row" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2}')"
        rid="$(printf '%s\n' "$row" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/, "", $3); print $3}')"
        anchor="$(printf '%s\n' "$row" | awk -F'|' '{print $4}')"
        cover="$(printf '%s\n' "$row" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/, "", $5); print $5}')"

        printf '%s\n' "$id" | grep -qE '^ACC-[0-9]{2}$' || fail "bad ACC id: $id"
        printf '%s\n' "$ids_seen" | grep -qxF "$id" && fail "duplicate ACC id: $id"
        ids_seen="$ids_seen
$id"
        grep -qxF "$rid" "$tmp_g2_req" || fail "$id: unknown requirement $rid"
        [ -n "$cover" ] || fail "$id: empty coverage column"

        cmd="$(printf '%s\n' "$anchor" | sed -n 's/^[^`]*`\([^`]*\)`.*$/\1/p' | head -n 1)"
        [ -n "$cmd" ] || fail "$id: anchor has no backticked command"
        g2_check_anchor "$id" "$cmd" "$tasks" || exit 1
    done <<EOF
$(grep -E '^\| ACC-[0-9]{2} ' "$map")
EOF

    [ "$n_rows" -gt 0 ] || fail "acceptance map has no ACC-* rows"
    while IFS= read -r rid; do
        [ -n "$rid" ] || continue
        grep -qE "^\| ACC-[0-9]{2} \|[[:space:]]*$rid[[:space:]]*\|" "$map" \
            || fail "requirement $rid has no ACC-* entry"
    done <"$tmp_g2_req"

    printf 'ok g2 (%d requirements mapped, %d ACC entries, anchors resolve)\n' \
        "$(wc -l <"$tmp_g2_req" | tr -d ' ')" "$n_rows"
}

# g2_check_anchor <acc> <command> <known tasks>
# Only anchors with a mechanical meaning are accepted: a wired fixture, a gate
# subcommand, or an existing script. Free-hand prose is not an anchor.
g2_check_anchor() {
    acc="$1"
    cmd="$2"
    tasks="$3"

    case "$cmd" in
    "bash test/run.sh "*)
        task="${cmd#bash test/run.sh }"
        task="${task%% *}"
        if ! printf '%s\n' "$tasks" | grep -qxF "$task"; then
            printf 'FAIL %s: %s: fixture not wired in test/run.sh: %s\n' "$gate" "$acc" "$task" >&2
            return 1
        fi
        ;;
    "bash tools/check.sh "g[0-9])
        g="${cmd#bash tools/check.sh }"
        g="${g%% *}"
        case "$g" in
        g0|g1|g2|g3|g4|g5|g6|g7|g8|g9) ;;
        *) printf 'FAIL %s: %s: unknown gate %s\n' "$gate" "$acc" "$g" >&2; return 1 ;;
        esac
        ;;
    "bash test/gates/"*)
        script="${cmd#bash }"
        script="${script%% *}"
        if [ ! -f "$pkg/$script" ]; then
            printf 'FAIL %s: %s: script not found: %s\n' "$gate" "$acc" "$script" >&2
            return 1
        fi
        ;;
    *)
        printf 'FAIL %s: %s: anchor is not mechanical (use test/run.sh, tools/check.sh or test/gates/): %s\n' \
            "$gate" "$acc" "$cmd" >&2
        return 1
        ;;
    esac
    return 0
}

tmp_g2_req="$(mktemp)"
trap 'rm -f "$tmp_g2_req"' EXIT
ids_seen=""
g2_main
