#!/usr/bin/env bash
#
# tools/gates/g8.sh - G8, commit granularity and scope.
#
#   freeze     a phase card's task list and exit criteria never change after
#              they are committed (T-6, g8-phase-freeze)
#   version    version landings agree, MINOR maps to PH with the registered
#              offset, PATCH tracks fixes (V-1..V-5, g8-version)
#   one-thing  a commit serves one purpose: docs, code or quality, never two
#   tag        an annotated v<version> tag points at the closeout commit, or a
#              signed deviation says why this run has no tag (V-7)
#   t7         a feat/fix task commit is preceded by its `test(...) red` parent
#
# Scope for one-thing and t7 is docs/phases/PH-10/base..HEAD: T-7 and 8.3 are
# not retroactive over phases that were already closed (AGENTS 6.6 适用边界).
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g8_main() {
    repo="${REPO:-$pkg}"
    phases="${PHASES_DIR:-$repo/docs/phases}"
    only="${G8_ONLY:-}"
    base="${G8_BASE:-}"
    if [ -z "$base" ] && [ -f "$phases/PH-10/base" ]; then
        base="$(tr -d '[:space:]' <"$phases/PH-10/base")"
    fi

    n_cards=0
    n_commits=0

    if [ -z "$only" ] || [ "$only" = freeze ]; then
        for card in "$phases"/PH-*/card.md; do
            [ -f "$card" ] || continue
            g8_freeze "$repo" "$card"
            n_cards=$((n_cards + 1))
        done
    fi
    if [ -z "$only" ] || [ "$only" = version ]; then
        g8_version "$repo"
    fi
    if { [ -z "$only" ] || [ "$only" = one-thing ]; } && [ -n "$base" ]; then
        g8_one_thing "$repo" "$base"
        n_commits="$g8_n_commits"
    fi
    if [ -z "$only" ] || [ "$only" = tag ]; then
        g8_tag "$repo"
    fi
    if { [ -z "$only" ] || [ "$only" = t7 ]; } && [ -n "$base" ]; then
        g8_t7 "$repo" "$base"
    fi

    printf 'ok g8 (%d cards frozen, %d commits in scope, version/tag rules)\n' "$n_cards" "$n_commits"
}

# g8_digest <file> <heading>
g8_digest() {
    awk -v h="$2" '
        index($0, h) == 1 { inb = 1; next }
        inb && /^## / { exit }
        inb { buf[n++] = $0 }
        END {
            while (n > 0 && buf[n - 1] ~ /^[[:space:]]*$/) n--
            for (i = 0; i < n; i++) print buf[i]
        }
    ' "$1" | shasum -a 256 | awk '{print $1}'
}

# g8_freeze <repo> <card>
g8_freeze() {
    repo="$1"
    card="$2"
    rel="${card#"$repo"/}"
    cur_c="$(g8_digest "$card" '## 5. 退出判据（冻结）')"
    cur_t="$(g8_digest "$card" '## 4. 任务清单')"

    for sha in $(git -C "$repo" log --format=%H -- "$rel"); do
        tmp="$g8_tmp/$sha.md"
        git -C "$repo" show "$sha:$rel" >"$tmp" 2>/dev/null || continue
        c="$(g8_digest "$tmp" '## 5. 退出判据（冻结）')"
        t="$(g8_digest "$tmp" '## 4. 任务清单')"
        if [ "$c" != "$cur_c" ] || [ "$t" != "$cur_t" ]; then
            fail "g8-phase-freeze: $rel changed its frozen sections in $sha"
        fi
    done
}

# g8_version <repo>
g8_version() {
    repo="$1"
    ctpkg="${CT_PKG:-$repo/ct.pkg}"
    readme="${README_MD:-$repo/README.md}"
    changelog="${CHANGELOG_MD:-$repo/CHANGELOG.md}"
    agents="${AGENTS_MD:-$repo/AGENTS.md}"

    [ -f "$ctpkg" ] || fail "g8-version: missing $ctpkg"
    v="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
    [ -n "$v" ] || fail "g8-version: ct.pkg has no version"
    rv="$(sed -n 's/^- 版本：\([0-9][0-9.]*\).*/\1/p' "$readme" | head -n 1)"
    [ "$rv" = "$v" ] || fail "g8-version: ct.pkg=$v README=$rv"
    cv="$(sed -n 's/^## \[\([0-9][0-9.]*\)\].*/\1/p' "$changelog" 2>/dev/null | head -n 1)"
    [ "$cv" = "$v" ] || fail "g8-version: CHANGELOG top=${cv:-none}, ct.pkg=$v"

    minor="$(printf '%s' "${v%.*}" | sed 's/^[0-9]*\.//')"
    d=0
    if [ -f "$agents" ] && grep -q 'deviation: V-001' "$agents"; then
        d="$(sed -n 's/.*偏移[:：][[:space:]]*d[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$agents" | head -n 1)"
        [ -n "$d" ] || fail "g8-version: deviation V-001 has no offset"
    fi
    want=$((minor + d))
    [ -f "$repo/docs/phases/PH-$want/card.md" ] \
        || fail "g8-version: version $v expects PH-$want"

    patch="${v##*.}"
    if [ "$d" -eq 0 ]; then
        fixes="$(grep -l '^fix: yes' "$repo"/issues/*.md 2>/dev/null | wc -l | tr -d ' ')"
        [ "$patch" = "$fixes" ] || fail "g8-version: PATCH=$patch but $fixes fix issue(s) logged"
    fi
}

# g8_one_thing <repo> <base>
g8_one_thing() {
    repo="$1"
    base="$2"
    n=0
    for sha in $(git -C "$repo" rev-list --no-merges "$base..HEAD"); do
        n=$((n + 1))
        cats=""
        while IFS= read -r f; do
            [ -n "$f" ] || continue
            case "$f" in
            src/*|http/*) cats="$cats code" ;;
            tools/*|test/*) cats="$cats quality" ;;
            docs/*|*.md|issues/*|ct.pkg|LICENSE) cats="$cats docs" ;;
            esac
        done <<EOF
$(git -C "$repo" show --pretty=format: --name-only "$sha" | sed '/^$/d')
EOF
        kinds="$(printf '%s\n' $cats | sort -u | sed '/^$/d' | wc -l | tr -d ' ')"
        [ "$kinds" -le 1 ] || fail "g8-one-thing: $sha mixes purposes:$cats"
    done
    g8_n_commits="$n"
}

# g8_tag <repo>
g8_tag() {
    repo="$1"
    ctpkg="${CT_PKG:-$repo/ct.pkg}"
    agents="${AGENTS_MD:-$repo/AGENTS.md}"
    v="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
    [ -n "$v" ] || fail "g8-tag: no version in ct.pkg"

    if git -C "$repo" rev-parse -q --verify "refs/tags/v$v" >/dev/null 2>&1; then
        [ "$(git -C "$repo" cat-file -t "v$v")" = tag ] \
            || fail "g8-tag: v$v is not an annotated tag"
        target="$(git -C "$repo" rev-parse "v$v^{commit}")"
        [ "$target" = "$(git -C "$repo" rev-parse HEAD)" ] \
            || fail "g8-tag: v$v points at $target, not HEAD"
        return 0
    fi
    if [ -f "$agents" ] && grep -q 'deviation: V-002' "$agents"; then
        printf 'note g8-tag: v%s not created (deviation V-002)\n' "$v" >&2
        return 0
    fi
    fail "g8-tag: missing annotated tag v$v and no deviation V-002"
}

# g8_t7 <repo> <base>
g8_t7() {
    repo="$1"
    base="$2"
    for sha in $(git -C "$repo" rev-list --no-merges "$base..HEAD"); do
        subj="$(git -C "$repo" log -1 --format=%s "$sha")"
        case "$subj" in
        feat*|fix*) ;;
        *) continue ;;
        esac
        case "$subj" in
        *PH[0-9]*-T[0-9]*) ;;
        *) continue ;;
        esac
        tid="$(printf '%s' "$subj" | sed -n 's/.*\(PH[0-9][0-9]*-T[0-9][0-9]*\).*/\1/p')"
        parent="$(git -C "$repo" rev-parse "$sha^")"
        psubj="$(git -C "$repo" log -1 --format=%s "$parent")"
        case "$psubj" in
        test\(*) ;;
        *) fail "g8-t7: $tid implementation $sha is not preceded by a test commit (parent: $psubj)" ;;
        esac
        printf '%s\n' "$psubj" | grep -qF "$tid" \
            || fail "g8-t7: parent of $sha does not mention $tid"
    done
}

g8_tmp="$(mktemp -d)"
trap 'rm -rf "$g8_tmp"' EXIT
g8_main
