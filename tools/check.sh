#!/usr/bin/env bash
#
# tools/check.sh - package gates for ct-http.
#
#   bash tools/check.sh g0    spec block, instantiation table, session map,
#                             version landing (AGENTS 6.4 / 11.5)
#   bash tools/check.sh g1    phase card: ten items, seven task columns,
#                             frozen exit-criteria / task-list digests
#   bash tools/check.sh g3    build + run the phase 1 fixtures, zero warnings
#
# Any subcommand that is not implemented exits 2 with an explicit message:
# a gate that cannot run must not look green.
#
# License: MIT (see LICENSE).

set -u

here="$(cd "$(dirname "$0")" && pwd)"
pkg="$(cd "$here/.." && pwd)"
SPEC_BIN="${SPEC_BIN:-/Users/samxx/trae-ws/.ai-engineering/bin/sync-agents.sh}"
CT_BIN="${CT_BIN:-/Users/samxx/.local/bin/ct}"

fail() {
    printf 'FAIL %s: %s\n' "$gate" "$1" >&2
    exit 1
}

# section <heading> <file>
# Print the body of a level-2 section, excluding the heading and the trailing
# blank lines. Level-3 headings stay inside the section.
section() {
    awk -v h="$1" '
        index($0, h) == 1 { inb = 1; next }
        inb && /^## / { exit }
        inb { print }
    ' "$2"
}

# digest_section <heading> <file>
# sha256 over the same body section() prints, with trailing blank lines
# stripped. This is what the card records and what g1 recomputes.
digest_section() {
    awk -v h="$1" '
        index($0, h) == 1 { inb = 1; next }
        inb && /^## / { exit }
        inb { buf[n++] = $0 }
        END {
            while (n > 0 && buf[n - 1] ~ /^[[:space:]]*$/) n--
            for (i = 0; i < n; i++) print buf[i]
        }
    ' "$2" | shasum -a 256 | awk '{print $1}'
}

# check_card <path>
# The ten required items (AGENTS 6.2 plus the project's red-evidence and
# closeout rows), the seven task columns, and the frozen digests.
check_card() {
    card="$1"
    [ -f "$card" ] || fail "missing card: $card"

    for h in \
        '## 0. 编号与元信息' \
        '## 1. 目标' \
        '## 2. 范围' \
        '## 3. 交付物清单' \
        '## 4. 任务清单' \
        '## 5. 退出判据（冻结）' \
        '## 6. 红证方式' \
        '## 7. 依赖' \
        '## 8. 已知风险' \
        '## 9. 收口动作'; do
        grep -qxF "$h" "$card" || fail "$card: missing card item: $h"
    done

    ph="$(sed -n 's/^- 编号：\(PH-[0-9][0-9]*\)$/\1/p' "$card" | head -n 1)"
    [ -n "$ph" ] || fail "$card: missing or malformed '- 编号：PH-<n>'"
    n="${ph#PH-}"
    grep -qxF -- "- version-minor：$n" "$card" \
        || fail "$card: version-minor must equal $n (PH-<n>)"
    grep -qE '^- 规范版本：[0-9]+\.[0-9]+\.[0-9]+' "$card" \
        || fail "$card: missing '- 规范版本：<x.y.z>'"
    grep -qE '^- criteria-sha256：[0-9a-f]{64}$' "$card" \
        || fail "$card: missing '- criteria-sha256：<64 hex>'"
    grep -qE '^- tasks-sha256：[0-9a-f]{64}$' "$card" \
        || fail "$card: missing '- tasks-sha256：<64 hex>'"

    crit="$(section '## 5. 退出判据（冻结）' "$card")"
    printf '%s\n' "$crit" | grep -qF '任务清单每项可独立验收（6.6 T-6）' \
        || fail "$card: exit criteria must include '任务清单每项可独立验收（6.6 T-6）'"
    # Criteria are namespaced by the card's own PH number: PH-2 uses E-P2-*.
    printf '%s\n' "$crit" | awk -F'|' -v pre="E-P$n-" '
        /^\| E-P[0-9]+-[0-9]+ / {
            n++
            id = $2; gsub(/^[ \t]+|[ \t]+$/, "", id)
            if (index(id, pre) != 1) {
                printf "criterion %s does not use the card prefix %s\n", id, pre > "/dev/stderr"
                bad = 1
            }
            if ($0 !~ /`/) { print "criterion without a command anchor: " $0 > "/dev/stderr"; bad = 1 }
        }
        END {
            if (n < 3) { printf "need at least 3 %s exit criteria, got %d\n", pre, n > "/dev/stderr"; bad = 1 }
            if (bad) exit 1
        }
    ' || fail "$card: exit criteria rows invalid"

    tasks="$(section '## 4. 任务清单' "$card")"
    printf '%s\n' "$tasks" | awk -F'|' -v pn="$n" '
        /^\| T[0-9]+ / {
            n++
            cnt = 0
            for (i = 2; i < NF; i++) if ($i ~ /[^ \t]/) cnt++
            if (cnt != 7) {
                printf "task row has %d non-empty columns, need 7: %s\n", cnt, $2 > "/dev/stderr"
                bad = 1
            }
            effort = $7; gsub(/^[ \t]+|[ \t]+$/, "", effort)
            if (effort != "S" && effort != "M") {
                printf "task %s effort must be S or M, got %s\n", $2, effort > "/dev/stderr"
                bad = 1
            }
            cover = $8; gsub(/^[ \t]+|[ \t]+$/, "", cover)
            if (cover !~ ("E-P" pn "-[0-9]+")) {
                printf "task %s must name the exit criterion it covers (E-P%s-<n>)\n", $2, pn > "/dev/stderr"
                bad = 1
            }
        }
        END {
            if (n < 1) { print "task table has no rows" > "/dev/stderr"; bad = 1 }
            if (bad) exit 1
        }
    ' || fail "$card: task table invalid"

    want_c="$(sed -n 's/^- criteria-sha256：\([0-9a-f]\{64\}\)$/\1/p' "$card" | head -n 1)"
    got_c="$(digest_section '## 5. 退出判据（冻结）' "$card")"
    [ "$want_c" = "$got_c" ] \
        || fail "$card: criteria-sha256 mismatch (card=$want_c actual=$got_c); exit criteria are frozen"

    want_t="$(sed -n 's/^- tasks-sha256：\([0-9a-f]\{64\}\)$/\1/p' "$card" | head -n 1)"
    got_t="$(digest_section '## 4. 任务清单' "$card")"
    [ "$want_t" = "$got_t" ] \
        || fail "$card: tasks-sha256 mismatch (card=$want_t actual=$got_t); task list is frozen"
}

gate="${1:-}"
case "$gate" in
g0)
    # 1. generated spec block + docs/spec full copy
    [ -f "$SPEC_BIN" ] || fail "spec tool not found: $SPEC_BIN"
    bash "$SPEC_BIN" --check "$pkg/AGENTS.md" || fail "sync-agents --check"

    # 2. docs/session-map.md
    [ -f "$pkg/docs/session-map.md" ] || fail "missing docs/session-map.md"

    # 3. the three landing sections, outside the generated block
    for h in '### 0.1 本项目实例化' '### 0.2 话术路由' '### 0.3 偏离项'; do
        grep -qxF "$h" "$pkg/AGENTS.md" || fail "missing landing section: $h"
    done

    # 4. eight non-empty instantiation rows (header + separator + 8 rows)
    rows="$(awk '
        /^### 0\.1 / { inb = 1; next }
        /^### 0\.2 / { inb = 0 }
        inb && /^\|/ { n++ }
        END { print n + 0 }
    ' "$pkg/AGENTS.md")"
    [ "$rows" -ge 10 ] || fail "instantiation table has $((rows - 2)) rows, need 8"

    # 5. version landing: ct.pkg#version equals the README version line
    pkg_ver="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$pkg/ct.pkg")"
    [ -n "$pkg_ver" ] || fail "ct.pkg has no version"
    readme_ver="$(sed -n 's/^- 版本：\([0-9][0-9.]*\).*/\1/p' "$pkg/README.md" | head -n 1)"
    [ "$readme_ver" = "$pkg_ver" ] \
        || fail "version landing differs: ct.pkg=$pkg_ver README=$readme_ver"

    printf 'ok g0 (spec block, 8 rows, routing, deviations, session map, version %s)\n' "$pkg_ver"
    ;;
g1)
    if [ -n "${CARD_FILE:-}" ]; then
        check_card "$CARD_FILE"
    else
        found=0
        for card in "$pkg"/docs/phases/PH-*/card.md; do
            [ -f "$card" ] || continue
            found=1
            check_card "$card"
        done
        [ "$found" -eq 1 ] || fail "no docs/phases/PH-*/card.md"
    fi
    printf 'ok g1 (card items, task columns, frozen criteria/tasks digests)\n'
    ;;
g3)
    [ -x "$CT_BIN" ] || fail "ct not found: $CT_BIN"
    bash "$pkg/test/run.sh" phase1_abi || fail "phase1_abi"
    bash "$pkg/test/run.sh" phase1_chain || fail "phase1_chain"
    bash "$pkg/test/run.sh" phase1_var || fail "phase1_var"
    bash "$pkg/test/run.sh" phase1_no_mapping || fail "phase1_no_mapping"
    bash "$pkg/test/run.sh" p2-cleanup-once || fail "p2-cleanup-once"
    bash "$pkg/test/run.sh" p2-nm || fail "p2-nm"
    bash "$pkg/test/run.sh" p2-double-free || fail "p2-double-free"
    bash "$pkg/test/run.sh" p2-after-send || fail "p2-after-send"
    bash "$pkg/test/run.sh" p2-response-free || fail "p2-response-free"
    bash "$pkg/test/run.sh" p2-ops || fail "p2-ops"
    bash "$pkg/test/run.sh" p3-url-own || fail "p3-url-own"
    bash "$pkg/test/run.sh" p3-header-crlf || fail "p3-header-crlf"
    bash "$pkg/test/run.sh" p3-body-nul || fail "p3-body-nul"
    bash "$pkg/test/run.sh" p3-scheme || fail "p3-scheme"
    bash "$pkg/test/run.sh" p3-defaults || fail "p3-defaults"
    bash "$pkg/test/run.sh" p3-chain || fail "p3-chain"
    bash "$pkg/test/run.sh" p4-status || fail "p4-status"
    bash "$pkg/test/run.sh" p4-binary || fail "p4-binary"
    bash "$pkg/test/run.sh" p4-split || fail "p4-split"
    bash "$pkg/test/run.sh" p4-take-body || fail "p4-take-body"
    bash "$pkg/test/run.sh" p4-take-body-bad || fail "p4-take-body-bad"
    bash "$pkg/test/run.sh" p4-refused || fail "p4-refused"
    bash "$pkg/test/run.sh" p4-timeout || fail "p4-timeout"
    bash "$pkg/test/run.sh" p5-local || fail "p5-local"
    bash "$pkg/test/run.sh" p5-table || fail "p5-table"
    bash "$pkg/test/run.sh" p5-http-error || fail "p5-http-error"
    bash "$pkg/test/run.sh" p5-unknown || fail "p5-unknown"
    bash "$pkg/test/run.sh" p5-predicates || fail "p5-predicates"
    printf 'ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release, response ownership, ops, request config, response bytes, take_body, failure codes, error model, predicates)\n'
    ;;
g2 | g4 | g5 | g6 | g7 | g8 | g9)
    printf '%s: not implemented yet (see docs/phases/PH-1/card.md)\n' "$gate" >&2
    exit 2
    ;;
*)
    printf 'usage: bash tools/check.sh <g0|g1|g3|...|g9>\n' >&2
    exit 2
    ;;
esac
