#include "http.shim.h"

#include <stdio.h>

/*
 * Every local rejection must reach the response as a negative code instead of
 * disappearing behind status 0. NO_HANDLE is the case that used to be silent:
 * a request with no easy handle now reports instead of pretending to send.
 */
static void report(const char *label, struct HttpRequest *req)
{
    if (req == NULL) {
        printf("%s <null>\n", label);
        return;
    }
    struct HttpResponse *res = http_request_send(req);
    printf("%s %d\n", label, http_response_error(res));
    http_response_free(&res);
    http_request_free(&req);
}

int main(void)
{
    struct HttpRequest *no_handle =
        http_request_new("http://127.0.0.1:1/p5-local-no-handle");
    if (no_handle != NULL) {
        /* The send-time path: a missing handle is an error, not a silent 0. */
        no_handle->easy = NULL;
    }

    report("arg", http_request_user_agent(
                       http_request_new("http://127.0.0.1:1/p5-local-arg"), NULL));
    report("header", http_request_header(
                         http_request_new("http://127.0.0.1:1/p5-local-header"),
                         "X-Test", "a\r\nInjected: 1"));
    report("body", http_request_body(
                       http_request_new("http://127.0.0.1:1/p5-local-body"), NULL, 0));
    report("scheme", http_request_new("ftp://127.0.0.1/p5-local-scheme"));
    report("timeout", http_request_timeout(
                          http_request_new("http://127.0.0.1:1/p5-local-timeout"), -1));
    report("no_handle", no_handle);
    return 0;
}
