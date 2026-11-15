#!/usr/bin/env bash
#
# tools/check.sh - package gates for ct-http.
#
#   bash tools/check.sh g0    spec block, instantiation table, session map,
#                             version landing (AGENTS 6.4 / 11.5)
#   bash tools/check.sh g1    phase card: ten items, seven task columns,
#                             frozen exit-criteria / task-list digests, errata
#   bash tools/check.sh g2    design acceptance map (R-* -> ACC-*)
#   bash tools/check.sh g3    build + run every fixture, zero warnings
#   bash tools/check.sh g4    snippets, block binding, anchors, applies-to,
#                             300-line limit, README shape, version landing
#   bash tools/check.sh g5    per-task red evidence + gate negative self-tests
#   bash tools/check.sh g6    issue ledger, unmet criteria traced, pool
#   bash tools/check.sh g7    secret scan, dependency consistency, sanitizers
#   bash tools/check.sh g8    phase freeze, version moves, commit scope, tag
#   bash tools/check.sh g9    spec pin (version + sha256) against the master
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

# check_errata <file>
# A frozen anchor is corrected by appending to docs/errata.md, never by editing
# the frozen text. Every entry must name a real file, keep the original wording
# verbatim, and carry the seven required fields (including the authorization).
check_errata() {
    errata="$1"
    [ -f "$errata" ] || fail "missing errata file: $errata"

    n="$(grep -cE '^## ERR-[0-9]{3} ' "$errata" || true)"
    [ "${n:-0}" -ge 1 ] || fail "errata has no '## ERR-<n>' entry"

    grep -E '^## ERR-[0-9]{3} ' "$errata" | sed 's/^## \(ERR-[0-9][0-9][0-9]\).*/\1/' |
    while read -r id; do
        block="$(awk -v id="$id" '
            index($0, "## " id " ") == 1 { inb = 1; next }
            inb && /^## / { exit }
            inb { print }
        ' "$errata")"
        for field in 日期 对象 原文 修正 原因 行为证据 授权; do
            printf '%s\n' "$block" | grep -q "^- ${field}：" \
                || { printf 'errata %s: missing field %s\n' "$id" "$field" >&2; exit 1; }
        done
        obj="$(printf '%s\n' "$block" | sed -n 's/^- 对象：`\([^`]*\)`.*/\1/p' | head -n 1)"
        [ -n "$obj" ] || { printf 'errata %s: 对象 must hold a backticked path\n' "$id" >&2; exit 1; }
        [ -f "$pkg/$obj" ] || { printf 'errata %s: 对象 file not found: %s\n' "$id" "$obj" >&2; exit 1; }
        quote="$(printf '%s\n' "$block" | sed -n 's/^- 原文：`\([^`]*\)`.*/\1/p' | head -n 1)"
        [ -n "$quote" ] || { printf 'errata %s: 原文 must hold the frozen text in backticks\n' "$id" >&2; exit 1; }
        grep -qF -- "$quote" "$pkg/$obj" \
            || { printf 'errata %s: 原文 not found verbatim in %s\n' "$id" "$obj" >&2; exit 1; }
    done || fail "errata entries invalid"
}

gate="${1:-}"
case "$gate" in
g0)
    agents="${AGENTS_MD:-$pkg/AGENTS.md}"
    sm="${SESSION_MAP:-$pkg/docs/session-map.md}"
    ctpkg="${CT_PKG:-$pkg/ct.pkg}"
    readme="${README_MD:-$pkg/README.md}"

    # 1. generated spec block + docs/spec full copy
    [ -f "$SPEC_BIN" ] || fail "spec tool not found: $SPEC_BIN"
    bash "$SPEC_BIN" --check "$agents" || fail "sync-agents --check"

    # 2. docs/session-map.md
    [ -f "$sm" ] || fail "missing docs/session-map.md"

    # 3. the three landing sections, outside the generated block
    for h in '### 0.1 本项目实例化' '### 0.2 话术路由' '### 0.3 偏离项'; do
        grep -qxF "$h" "$agents" || fail "missing landing section: $h"
    done

    # 4. eight non-empty instantiation rows (header + separator + 8 rows)
    rows="$(awk '
        /^### 0\.1 / { inb = 1; next }
        /^### 0\.2 / { inb = 0 }
        inb && /^\|/ { n++ }
        END { print n + 0 }
    ' "$agents")"
    [ "$rows" -ge 10 ] || fail "instantiation table has $((rows - 2)) rows, need 8"

    # 5. version landing: ct.pkg#version equals the README version line
    pkg_ver="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
    [ -n "$pkg_ver" ] || fail "ct.pkg has no version"
    readme_ver="$(sed -n 's/^- 版本：\([0-9][0-9.]*\).*/\1/p' "$readme" | head -n 1)"
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
    check_errata "${ERRATA_FILE:-$pkg/docs/errata.md}"
    printf 'ok g1 (card items, task columns, frozen criteria/tasks digests, errata)\n'
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
    bash "$pkg/test/run.sh" p6-total || fail "p6-total"
    bash "$pkg/test/run.sh" p6-connect || fail "p6-connect"
    bash "$pkg/test/run.sh" p6-idle || fail "p6-idle"
    bash "$pkg/test/run.sh" p6-slow || fail "p6-slow"
    bash "$pkg/test/run.sh" p6-negative || fail "p6-negative"
    bash "$pkg/test/run.sh" p7-chunks || fail "p7-chunks"
    bash "$pkg/test/run.sh" p7-abort || fail "p7-abort"
    bash "$pkg/test/run.sh" p7-body-nul || fail "p7-body-nul"
    bash "$pkg/test/run.sh" p7-chunked || fail "p7-chunked"
    bash "$pkg/test/run.sh" p7-limit || fail "p7-limit"
    bash "$pkg/test/run.sh" p7-idle || fail "p7-idle"
    bash "$pkg/test/run.sh" p7-slow || fail "p7-slow"
    bash "$pkg/test/run.sh" p7-mutex || fail "p7-mutex"
    bash "$pkg/test/run.sh" p8-cancel || fail "p8-cancel"
    bash "$pkg/test/run.sh" p8-not-cancelled || fail "p8-not-cancelled"
    bash "$pkg/test/run.sh" p8-lifetime || fail "p8-lifetime"
    bash "$pkg/test/run.sh" p8-lifetime-bad || fail "p8-lifetime-bad"
    bash "$pkg/test/run.sh" p8-chain || fail "p8-chain"
    bash "$pkg/test/run.sh" p9-same-host || fail "p9-same-host"
    bash "$pkg/test/run.sh" p9-hops10 || fail "p9-hops10"
    bash "$pkg/test/run.sh" p9-hops11 || fail "p9-hops11"
    bash "$pkg/test/run.sh" p9-cross-host || fail "p9-cross-host"
    bash "$pkg/test/run.sh" p9-loop || fail "p9-loop"
    bash "$pkg/test/run.sh" p9-downgrade || fail "p9-downgrade"
    bash "$pkg/test/run.sh" p9-default || fail "p9-default"
    printf 'ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release, response ownership, ops, request config, response bytes, take_body, failure codes, error model, predicates, timeouts, streaming, cancellation, lifetime, redirects, credentials, downgrade)\n'
    ;;
g2 | g4 | g5 | g6 | g7 | g8 | g9)
    [ -f "$here/gates/$gate.sh" ] || fail "gate script not found: tools/gates/$gate.sh"
    . "$here/gates/$gate.sh"
    ;;
*)
    printf 'usage: bash tools/check.sh <g0|g1|...|g9>\n' >&2
    exit 2
    ;;
esac
