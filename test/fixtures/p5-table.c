#include "http.shim.h"

#include <stdio.h>
#include <string.h>

/*
 * Transport codes pass through unchanged. Code 7 is produced by a real
 * refused connection; the rest are injected because they cannot be reproduced
 * offline and deterministically (DNS, TLS, abort, write failure).
 */
static int probe(int injected, int verbose)
{
    http_shim_inject_curl_code(injected);

    struct HttpRequest *req = http_request_new(
        injected == 0 ? "http://127.0.0.1:1/p5-table"
                      : "http://127.0.0.1:1/p5-injected");
    struct HttpResponse *res = http_request_send(req);
    int code = http_response_error(res);

    if (verbose) {
        const char *msg = http_response_error_msg(res);
        const char *first = http_response_error_detail(res);
        const char *second = http_response_error_detail(res);
        printf("msg %s\n", msg);
        printf("detail %s\n", first);
        printf("repeat %d\n", strcmp(first, second) == 0);
    }

    http_response_free(&res);
    http_request_free(&req);
    http_shim_inject_curl_code(0);
    return code;
}

int main(void)
{
    printf("real %d\n", probe(0, 0));
    printf("inject %d\n", probe(6, 0));
    printf("inject %d\n", probe(28, 0));
    printf("inject %d\n", probe(60, 0));
    printf("inject %d\n", probe(42, 0));
    printf("inject %d\n", probe(23, 0));
    probe(0, 1);
    return 0;
}
