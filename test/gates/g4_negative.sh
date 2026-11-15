#!/usr/bin/env bash
#
# test/gates/g4_negative.sh - prove g4 rejects a bloated README and a snippet
# that reaches into the implementation chain.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# 1. g4-size: 301 lines must fail
i=0
: >"$tmp/README.md"
while [ "$i" -lt 301 ]; do
    echo "line $i" >>"$tmp/README.md"
    i=$((i + 1))
done
G4_ONLY=size README_MD="$tmp/README.md" bash "$repo/tools/check.sh" g4 >"$tmp/out1" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out1" >&2; echo "FAIL g4-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'g4-size' "$tmp/out1" \
    || { cat "$tmp/out1" >&2; echo "FAIL g4-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected size/301: %s\n' "$(head -n 1 "$tmp/out1")"

# 2. g4-snippets: a snippet that includes the shim and calls an impl symbol
mkdir -p "$tmp/snips"
cat >"$tmp/snips/impl.ct" <<EOF
import http::http;
#include <stdio.h>
#include "$repo/src/http/http.shim.h"

int main(void)
{
    struct HttpRequest *r = http_request_new("http://127.0.0.1/");
    printf("%s\n", http_shim_header_line(r, 0));
    http_request_release(r);
    return 0;
}
EOF
G4_ONLY=compile SNIPPET_DIR="$tmp/snips" bash "$repo/tools/check.sh" g4 >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out2" >&2; echo "FAIL g4-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'impl-chain' "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g4-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected snippet/impl-chain: %s\n' "$(tail -n 1 "$tmp/out2")"

printf 'ok g4-negative (2 rejected cases)\n'
