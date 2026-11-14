#include "http.shim.h"

#include <stdio.h>

/*
 * Only http and https may leave the process. Everything else is rejected
 * locally at the factory, before any handle could be used.
 */
static int local_error_of(const char *url)
{
    struct HttpRequest *req = http_request_new(url);
    if (req == NULL) return 0;
    int code = req->local_error;
    http_request_free(&req);
    return code;
}

int main(void)
{
    printf("file %d ftp %d http %d https %d\n",
           local_error_of("file:///etc/passwd"),
           local_error_of("ftp://example.test/x"),
           local_error_of("http://example.test/x"),
           local_error_of("HTTPS://example.test/x"));
    return 0;
}
