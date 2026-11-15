#!/usr/bin/env bash
#
# tools/gates/g4.sh - G4, documents are executable and fresh.
#
#   g4-size      every project document <= 300 lines (AGENTS.md block excluded,
#                DOC-S8; docs/spec/ is the mandated full copy of the master spec
#                and is exempt for the same reason)
#   g4-snippets  every docs/snippets/*.ct compiles; ones with .expected.out run;
#                README/tutorial code blocks are bound to a snippet file and
#                match it byte for byte; snippets may not touch the impl chain
#   g4-anchors   every relative link and #fragment resolves
#   g4-applies   user-facing docs declare `applies-to` equal to the release anchor
#   g4-readme    complex-module README structure (DOC-S5) + docs/tutorial/NN-*
#   g4-version   version landing, PH mapping and CHANGELOG agree (V-6)
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g4_main() {
    repo="${REPO:-$pkg}"
    readme="${README_MD:-$repo/README.md}"
    snippets="${SNIPPET_DIR:-$repo/docs/snippets}"
    docs="${DOCS_DIR:-$repo/docs}"
    ctpkg="${CT_PKG:-$repo/ct.pkg}"
    changelog="${CHANGELOG_MD:-$repo/CHANGELOG.md}"
    agents="${AGENTS_MD:-$repo/AGENTS.md}"
    ct_bin="${CT_BIN:-/Users/samxx/.local/bin/ct}"
    only="${G4_ONLY:-}"

    g4_tmp="$(mktemp -d)"
    trap 'rm -rf "$g4_tmp"' EXIT

    [ -f "$readme" ] || fail "missing README: $readme"
    [ -f "$ctpkg" ] || fail "missing ct.pkg: $ctpkg"

    n_docs=0
    n_snip=0
    n_blocks=0

    if [ -z "$only" ] || [ "$only" = size ]; then
        g4_size "$repo" "$readme" "$agents"
        n_docs=$g4_n_docs
    fi
    if [ -z "$only" ] || [ "$only" = compile ]; then
        g4_snippets "$repo" "$snippets" "$ct_bin"
        n_snip="$g4_n_snip"
    fi
    if [ -z "$only" ] || [ "$only" = blocks ]; then
        g4_blocks "$repo" "$readme" "$docs"
        n_blocks="$g4_n_blocks"
    fi
    if [ -z "$only" ] || [ "$only" = anchors ]; then
        g4_anchors "$repo" "$readme" "$docs"
    fi
    if [ -z "$only" ] || [ "$only" = applies ]; then
        g4_applies "$repo" "$readme" "$docs" "$ctpkg"
    fi
    if [ -z "$only" ] || [ "$only" = readme ]; then
        g4_readme "$repo" "$readme" "$docs"
    fi
    if [ -z "$only" ] || [ "$only" = version ]; then
        g4_version "$repo" "$readme" "$ctpkg" "$changelog" "$agents"
    fi

    printf 'ok g4 (%d docs <= 300 lines, %d snippets compiled, %d blocks bound, anchors/applies/readme/version ok)\n' \
        "$n_docs" "$n_snip" "$n_blocks"
}

# ---------------------------------------------------------------- g4-size ---

g4_size() {
    repo="$1"
    readme="$2"
    agents="$3"
    g4_n_docs=0

    list="$g4_tmp/docs.list"
    ( cd "$repo" && git ls-files '*.md' 2>/dev/null ) | grep -v '^docs/spec/' >"$list" || true
    printf '%s\n' "$readme" >>"$list"

    while IFS= read -r f; do
        [ -n "$f" ] || continue
        case "$f" in /*) abs="$f" ;; *) abs="$repo/$f" ;; esac
        [ -f "$abs" ] || continue
        case "$abs" in
        */docs/spec/*) continue ;;
        esac
        if [ "$(basename "$abs")" = AGENTS.md ]; then
            n="$(awk '
                /<!-- spec:begin/ { skip = 1 }
                !skip { n++ }
                /<!-- spec:end/ { skip = 0 }
                END { print n + 0 }
            ' "$abs")"
        else
            n="$(wc -l <"$abs" | tr -d ' ')"
        fi
        [ "$n" -le 300 ] || fail "g4-size: $abs has $n lines (limit 300, DOC-S2)"
        g4_n_docs=$((g4_n_docs + 1))
    done <"$list"
}

# ------------------------------------------------------------ g4-snippets ---

g4_snippets() {
    repo="$1"
    dir="$2"
    ct_bin="$3"
    [ -d "$dir" ] || fail "g4-snippets: missing directory: $dir"
    n=0

    for f in "$dir"/*.ct; do
        [ -f "$f" ] || continue
        name="$(basename "$f" .ct)"
        log="$g4_tmp/$name.build.log"
        ( cd "$g4_tmp" &&
          "$ct_bin" build "$f" -I "$repo" -I "$repo/test/fixtures" \
              -o "$name" --outdir "$g4_tmp/b-$name" \
              --cflags "-I $repo/test/fixtures -Wall -Wextra -Werror" \
              --ldflags "$repo/test/fixtures/p4-server.c -lcurl" ) >"$log" 2>&1 \
            || { cat "$log" >&2; fail "g4-snippets: $name failed to compile"; }
        grep -q 'warning:' "$log" && { cat "$log" >&2; fail "g4-snippets: $name emitted a warning"; }

        if grep -nE 'http_shim_|curl_easy_|curl_slist_|#include[[:space:]]*"[^"]*\.shim\.h"' "$f" >"$g4_tmp/impl" 2>&1; then
            cat "$g4_tmp/impl" >&2
            fail "g4-snippets: impl-chain reference in $name (docs must stay on the public face)"
        fi

        expected="$dir/$name.expected.out"
        if [ -f "$expected" ]; then
            "$g4_tmp/$name" >"$g4_tmp/$name.out" 2>&1 \
                || { cat "$g4_tmp/$name.out" >&2; fail "g4-snippets: $name failed at runtime"; }
            diff -u "$expected" "$g4_tmp/$name.out" \
                || fail "g4-snippets: $name output differs from $expected"
        fi
        n=$((n + 1))
    done
    [ "$n" -gt 0 ] || fail "g4-snippets: no .ct snippet found in $dir"
    g4_n_snip="$n"
}

# -------------------------------------------------------------- g4-blocks ---
# Every fenced block in README / docs/tutorial must be introduced by
# `<!-- snippet: <path> -->` and match that file byte for byte.

g4_blocks() {
    repo="$1"
    readme="$2"
    docs="$3"
    n=0

    list="$g4_tmp/blocks.list"
    printf '%s\n' "$readme" >"$list"
    for f in "$docs"/tutorial/*.md; do
        [ -f "$f" ] && printf '%s\n' "$f" >>"$list"
    done

    while IFS= read -r doc; do
        [ -n "$doc" ] || continue
        rm -rf "$g4_tmp/blocks"
        mkdir -p "$g4_tmp/blocks"
        awk -v dir="$g4_tmp/blocks" '
            /^<!-- snippet: / {
                m = $0
                sub(/^<!-- snippet: /, "", m)
                sub(/[ \t]*-->[ \t]*$/, "", m)
                pending = m
                have = 1
                next
            }
            /^```/ {
                if (!infence) { infence = 1; buf = ""; mark = pending; pending = ""; next }
                infence = 0
                n++
                print (have ? mark : "") > (dir "/" n ".marker")
                printf "%s", buf > (dir "/" n ".body")
                have = 0
                next
            }
            infence { buf = buf $0 "\n" }
            END { print n + 0 > (dir "/count") }
        ' "$doc"

        total="$(cat "$g4_tmp/blocks/count" 2>/dev/null || echo 0)"
        i=1
        while [ "$i" -le "$total" ]; do
            mark="$(cat "$g4_tmp/blocks/$i.marker" 2>/dev/null || true)"
            [ -n "$mark" ] \
                || fail "g4-snippets: $doc block #$i has no '<!-- snippet: ... -->' marker"
            case "$mark" in /*) target="$mark" ;; *) target="$repo/$mark" ;; esac
            [ -f "$target" ] || fail "g4-snippets: $doc block #$i points at a missing file: $mark"
            diff -u "$target" "$g4_tmp/blocks/$i.body" >"$g4_tmp/block.diff" 2>&1 \
                || { cat "$g4_tmp/block.diff" >&2; fail "g4-snippets: $doc block #$i differs from $mark"; }
            n=$((n + 1))
            i=$((i + 1))
        done
    done <"$list"
    g4_n_blocks="$n"
}

# ------------------------------------------------------------- g4-anchors ---

g4_anchors() {
    repo="$1"
    readme="$2"
    docs="$3"
    n=0

    list="$g4_tmp/anchors.list"
    printf '%s\n' "$readme" >"$list"
    for f in "$docs"/tutorial/*.md "$docs/design.md" "$docs/requirements.md" \
             "$docs/streaming.md" "$docs/errata.md" "$docs/session-map.md" "$docs"/planning/*.md; do
        [ -f "$f" ] && printf '%s\n' "$f" >>"$list"
    done

    while IFS= read -r doc; do
        [ -n "$doc" ] || continue
        grep -oE '\]\([^)]+\)' "$doc" | sed 's/^](//; s/)$//' |
        while IFS= read -r target; do
            case "$target" in
            http://*|https://*|mailto:*|\#*) frag="${target#\#}" ; file="" ;;
            *) file="${target%%#*}" ; frag="" ; case "$target" in *\#*) frag="${target#*\#}" ;; esac ;;
            esac

            if [ -n "$file" ]; then
                case "$file" in
                /*) abs="$file" ;;
                *) abs="$(cd "$(dirname "$doc")" && pwd)/$file" ;;
                esac
                [ -f "$abs" ] || { printf 'missing link target: %s -> %s\n' "$doc" "$target" >&2; exit 1; }
                case "$abs" in *.md) ;; *) continue ;; esac
            else
                abs="$doc"
            fi

            [ -n "$frag" ] || continue
            ok=0
            while IFS= read -r h; do
                s="$(printf '%s' "$h" | perl -CS -pe 's/[^\p{L}\p{N} -]//g; s/ /-/g; $_ = lc')"
                [ "$s" = "$frag" ] && ok=1 && break
            done <<EOF
$(grep -E '^#{1,6} ' "$abs" | sed 's/^#\+[[:space:]]*//')
EOF
            [ "$ok" -eq 1 ] || { printf 'missing anchor: %s -> %s\n' "$doc" "$target" >&2; exit 1; }
        done || fail "g4-anchors: broken link or anchor"
    done <"$list"
    return 0
}

# ------------------------------------------------------------- g4-applies ---

g4_applies() {
    repo="$1"
    readme="$2"
    docs="$3"
    ctpkg="$4"
    v="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
    [ -n "$v" ] || fail "g4-applies: no version in $ctpkg"

    list="$g4_tmp/applies.list"
    printf '%s\n' "$readme" >"$list"
    for f in "$docs"/tutorial/*.md "$docs/design.md" "$docs/requirements.md" \
             "$docs/streaming.md" "$docs/errata.md" "$docs/session-map.md" "$docs"/planning/*.md; do
        [ -f "$f" ] && printf '%s\n' "$f" >>"$list"
    done

    while IFS= read -r f; do
        [ -n "$f" ] || continue
        got="$(sed -n 's/^- applies-to[:：][[:space:]]*\([^[:space:](（]*\).*/\1/p' "$f" | head -n 1)"
        [ -n "$got" ] || fail "g4-applies: $(basename "$f") has no '- applies-to: <anchor>' line"
        [ "$got" = "$v" ] || fail "g4-applies: $(basename "$f") applies-to=$got, release anchor=$v"
    done <"$list"
}

# -------------------------------------------------------------- g4-readme ---

g4_readme() {
    repo="$1"
    readme="$2"
    docs="$3"
    for h in '## 定位与边界' '## 依赖与安装' '## 最小可运行示例' '## API 索引' \
             '## 教程' '## 限制' '## 版本'; do
        grep -qxF "$h" "$readme" || fail "g4-readme: missing section: $h (DOC-S5)"
    done
    [ -d "$docs/tutorial" ] || fail "g4-readme: docs/tutorial/ is required for a complex module (DOC-S5)"
    [ -f "$docs/tutorial/01-concepts.md" ] \
        || fail "g4-readme: docs/tutorial/01-concepts.md is required (DOC-S6)"
    bad=""
    for f in "$docs"/tutorial/*.md; do
        [ -f "$f" ] || continue
        b="$(basename "$f")"
        printf '%s\n' "$b" | grep -qE '^[0-9]{2}-.+\.md$' || bad="$bad $b"
    done
    [ -z "$bad" ] || fail "g4-readme: tutorial names must match NN-*.md:$bad"
}

# ------------------------------------------------------------- g4-version ---

g4_version() {
    repo="$1"
    readme="$2"
    ctpkg="$3"
    changelog="$4"
    agents="$5"

    v="$(awk -F'=' '/^version[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' "$ctpkg")"
    [ -n "$v" ] || fail "g4-version: ct.pkg has no version"
    rv="$(sed -n 's/^- 版本：\([0-9][0-9.]*\).*/\1/p' "$readme" | head -n 1)"
    [ "$rv" = "$v" ] || fail "g4-version: ct.pkg=$v README=$rv"

    [ -f "$changelog" ] || fail "g4-version: missing CHANGELOG: $changelog"
    cv="$(sed -n 's/^## \[\([0-9][0-9.]*\)\].*/\1/p' "$changelog" | head -n 1)"
    [ "$cv" = "$v" ] || fail "g4-version: CHANGELOG top=$cv, ct.pkg=$v"

    major_minor="${v%.*}"
    minor="${major_minor#*.}"
    d=0
    if [ -f "$agents" ] && grep -q 'deviation: V-001' "$agents"; then
        d="$(sed -n 's/.*偏移[:：][[:space:]]*d[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$agents" | head -n 1)"
        [ -n "$d" ] || fail "g4-version: deviation V-001 has no '偏移: d = <n>' field"
    fi
    want_minor=$((minor + d))
    printf -v ph 'PH-%d' "$want_minor"
    [ -f "$repo/docs/phases/$ph/card.md" ] \
        || fail "g4-version: version $v (MINOR=$minor, d=$d) expects $ph"

    patch="${v##*.}"
    if [ "$d" -eq 0 ]; then
        fixes="$(grep -l '^fix: yes' "$repo"/issues/*.md 2>/dev/null | wc -l | tr -d ' ')"
        [ "$patch" = "$fixes" ] || fail "g4-version: PATCH=$patch but $fixes fix issue(s) are logged"
    else
        printf 'note g4-version: PATCH accounting deferred by deviation V-001 (0.1.0 number line)\n' >&2
    fi

    if git -C "$repo" rev-parse -q --verify "refs/tags/v$v" >/dev/null 2>&1; then
        tv="$(git -C "$repo" describe --tags --exact-match 2>/dev/null || true)"
        [ "$tv" = "v$v" ] || fail "g4-version: HEAD is not tagged v$v (found ${tv:-none})"
    fi
}

g4_main
