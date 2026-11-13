#!/usr/bin/env bash
#
# tools/check.sh - package gates for ct-http.
#
#   bash tools/check.sh g0    spec block, instantiation table, session map,
#                             version landing (AGENTS 6.4 / 11.5)
#
# Any subcommand that is not implemented exits 2 with an explicit message:
# a gate that cannot run must not look green.
#
# License: MIT (see LICENSE).

set -u

here="$(cd "$(dirname "$0")" && pwd)"
pkg="$(cd "$here/.." && pwd)"
SPEC_BIN="${SPEC_BIN:-/Users/samxx/trae-ws/.ai-engineering/bin/sync-agents.sh}"

fail() {
    printf 'FAIL %s: %s\n' "$gate" "$1" >&2
    exit 1
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
g3 | g1 | g2 | g4 | g5 | g6 | g7 | g8 | g9)
    printf '%s: not implemented yet (see docs/phases/H1.md H1-T6)\n' "$gate" >&2
    exit 2
    ;;
*)
    printf 'usage: bash tools/check.sh <g0|g1|...|g9>\n' >&2
    exit 2
    ;;
esac
