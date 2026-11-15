#!/usr/bin/env bash
#
# tools/gates/g7.sh - G7, mechanical security.
#
#   scan      no committed secret material (keys, cloud tokens, literal passwords)
#   deps      every -l flag in ct.pkg is used by the build; libcurl is the only
#             native dependency and stays in the shim; nothing vendored
#   asan      the sanitizer fixtures stay green (the -bad ones must abort)
#
# Sourced by tools/check.sh; uses $pkg, $fail and $gate.
#
# License: MIT (see LICENSE).

g7_main() {
    repo="${REPO:-$pkg}"
    only="${G7_ONLY:-}"
    n_scanned=0
    n_san=0

    if [ -z "$only" ] || [ "$only" = scan ]; then
        g7_scan "$repo"
        n_scanned="$g7_n_scanned"
    fi
    if [ -z "$only" ] || [ "$only" = deps ]; then
        g7_deps "$repo"
    fi
    if [ -z "$only" ] || [ "$only" = asan ]; then
        for t in p2-double-free p4-take-body p4-take-body-bad p7-chunks p7-limit \
                 p8-lifetime p8-lifetime-bad; do
            out="$g7_tmp/$t.log"
            ASAN_OPTIONS=detect_leaks=1 bash "$repo/test/run.sh" "$t" >"$out" 2>&1 \
                || { cat "$out" >&2; fail "g7: sanitizer fixture $t failed"; }
            n_san=$((n_san + 1))
        done
    fi

    printf 'ok g7 (%d files scanned, deps consistent, %d sanitizer fixtures)\n' "$n_scanned" "$n_san"
}

# g7_scan <root> - prints the number of files inspected.
g7_scan() {
    root="$1"
    pattern='-----BEGIN [A-Z ]*PRIVATE KEY-----|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{36}|xox[baprs]-[A-Za-z0-9-]+|password[[:space:]]*=[[:space:]]*[^[:space:]]+'
    n=0
    if git -C "$root" rev-parse --git-dir >/dev/null 2>&1; then
        files="$(git -C "$root" ls-files)"
    else
        files="$(cd "$root" && find . -type f | sed 's|^\./||')"
    fi
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        case "$f" in
        tools/gates/g7.sh|test/gates/g7_negative.sh) continue ;;
        esac
        [ -f "$root/$f" ] || continue
        hit="$(grep -I -n -E -m 1 -- "$pattern" "$root/$f" 2>/dev/null || true)"
        [ -z "$hit" ] || fail "g7: possible secret in $f: $hit"
        n=$((n + 1))
    done <<EOF
$files
EOF
    g7_n_scanned="$n"
}

# g7_deps <repo>
g7_deps() {
    repo="$1"
    [ -f "$repo/ct.pkg" ] || fail "g7: missing ct.pkg"
    [ -f "$repo/http/http.shim.c" ] || fail "g7: missing http/http.shim.c"

    ldflags="$(awk -F'=' '/^ldflags[[:space:]]*=/ { sub(/^[^=]*=[[:space:]]*/, ""); print; exit }' "$repo/ct.pkg")"
    for flag in $ldflags; do
        case "$flag" in
        -l*)
            grep -qF -- "$flag" "$repo/test/run.sh" \
                || fail "g7: ct.pkg ldflags '$flag' is not used by the test build" ;;
        esac
    done

    grep -q '#include <curl/curl.h>' "$repo/http/http.shim.c" \
        || fail "g7: libcurl must stay behind http/http.shim.c"

    vendored="$(git -C "$repo" ls-files 2>/dev/null | grep -E '^(vendor/|curl/)|(^|/)curl\.(c|h)$' || true)"
    [ -z "$vendored" ] || fail "g7: vendored libcurl is not allowed: $vendored"
}

g7_tmp="$(mktemp -d)"
trap 'rm -rf "$g7_tmp"' EXIT
g7_main
