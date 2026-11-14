#include "http.shim.h"

#include <stdio.h>

/*
 * Every single-value setter lands in the struct, and the defaults are part of
 * the contract: TLS verification on, redirects off, a fixed user agent.
 */
int main(void)
{
    struct HttpRequest *req = http_request_new("https://example.test/d");
    if (req == NULL) return 1;

    printf("defaults %d %d %d %d %s\n",
           req->timeout_ms,
           req->connect_timeout_ms,
           req->insecure,
           req->follow_redirects,
           req->user_agent);

    req = http_request_timeout(req, 3000);
    req = http_request_connect_timeout(req, 1000);
    req = http_request_insecure(req, 1);
    req = http_request_follow_redirects(req, 1);
    req = http_request_user_agent(req, "probe/1");
    printf("set %d %d %d %d %s\n",
           req->timeout_ms,
           req->connect_timeout_ms,
           req->insecure,
           req->follow_redirects,
           req->user_agent);

    req = http_request_timeout(req, -5);
    printf("neg %d %d\n", req->local_error, req->timeout_ms);

    http_request_free(&req);
    return 0;
}
