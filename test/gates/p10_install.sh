#!/usr/bin/env bash
#
# test/gates/p10_install.sh - E-P10-4: consume the package from a clean project.
#
#   bash test/gates/p10_install.sh
#
# A throwaway project declares `dep = http@^0.1.0`, vendors this repository
# (local directory spec; the bare-name form needs a published index and a tag,
# neither of which exists in this run), verifies the lock, and builds a sample
# against the vendored tree. The sample performs a real POST against a loopback
# server and must print 200.
#
# The printed `ct install` command says `-I vendor/http`. The package keeps its
# units at the repository root under `http/` (layout decision 2026-11-15), so the
# printed prefix is exactly what the consumer needs; the sample below uses it
# verbatim and fails if the install line changes. Background and the upstream
# proposal: docs/proposals/ct-install-include-prefix.md, issues/HTTP-2.md.
#
# License: MIT (see LICENSE).

set -u

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
CT_BIN="${CT_BIN:-/Users/samxx/.local/bin/ct}"
tmp="$(mktemp -d)"
srv=""
cleanup() {
    [ -n "$srv" ] && kill "$srv" 2>/dev/null
    rm -rf "$tmp"
}
trap cleanup EXIT

fail() {
    printf 'FAIL p10_install: %s\n' "$1" >&2
    exit 1
}

[ -x "$CT_BIN" ] || fail "ct not found: $CT_BIN"
command -v python3 >/dev/null 2>&1 || fail "python3 is required for the loopback server"

mkdir -p "$tmp/app"
cat >"$tmp/app/ct.pkg" <<'EOF'
name = p10-app
version = 0.0.0
dep = http@^0.1.0
EOF

cat >"$tmp/app/sample.ct" <<'EOF'
import http::http;

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *url = getenv("P10_URL");
    if (url == NULL) return 2;
    CRequest *req = url->post();
    CResponse *res = req->send();
    printf("%d\n", res->status());
    res->free();
    req->free();
    return 0;
}
EOF

( cd "$tmp/app" && "$CT_BIN" get "$repo" ) >"$tmp/get.log" 2>&1 \
    || { cat "$tmp/get.log" >&2; fail "ct get (local dir spec)"; }
grep -q 'vendored 1 package' "$tmp/get.log" \
    || { cat "$tmp/get.log" >&2; fail "ct get did not vendor the package"; }

( cd "$tmp/app" && "$CT_BIN" install ) >"$tmp/install.log" 2>&1 \
    || { cat "$tmp/install.log" >&2; fail "ct install"; }
grep -q '^ct build ' "$tmp/install.log" \
    || { cat "$tmp/install.log" >&2; fail "ct install printed no build command"; }
grep -q -- '-I vendor/http ' "$tmp/install.log" \
    || { cat "$tmp/install.log" >&2; fail "ct install did not print '-I vendor/http'"; }
grep -q -- '-I vendor/http/src' "$tmp/install.log" \
    && { cat "$tmp/install.log" >&2; fail "ct install still prints the src/ include"; }

( cd "$tmp/app" && "$CT_BIN" verify ) >"$tmp/verify.log" 2>&1 \
    || { cat "$tmp/verify.log" >&2; fail "ct verify"; }

( cd "$tmp/app" && "$CT_BIN" build sample.ct -I vendor/http -o sample \
      --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl" ) >"$tmp/build.log" 2>&1 \
    || { cat "$tmp/build.log" >&2; fail "sample build against the vendored tree"; }

python3 - "$tmp/port" <<'PY' &
import http.server, socketserver, sys

class H(http.server.BaseHTTPRequestHandler):
    def _ok(self):
        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"ok")

    do_GET = _ok
    do_POST = _ok

    def log_message(self, *a):
        pass

srv = socketserver.TCPServer(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(srv.server_address[1]))
srv.handle_request()
PY
srv=$!

i=0
while [ ! -s "$tmp/port" ] && [ "$i" -lt 50 ]; do
    i=$((i + 1))
    sleep 0.1
done
[ -s "$tmp/port" ] || fail "loopback server did not start"
port="$(cat "$tmp/port")"

out="$(cd "$tmp/app" && P10_URL="http://127.0.0.1:$port/" ./sample 2>&1)" \
    || fail "sample failed at runtime: $out"
[ "$out" = "200" ] || fail "sample printed '$out', expected 200"

wait "$srv" 2>/dev/null || true
srv=""

printf 'install ok\n'
printf '%s\n' "$out"
