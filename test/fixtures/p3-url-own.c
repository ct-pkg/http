#include "http.shim.h"

#include <stdio.h>
#include <string.h>

/*
 * The request owns its URL: overwriting the caller's buffer after the factory
 * returns must not change what the request will send.
 */
int main(void)
{
    char buf[64];
    strcpy(buf, "https://example.test/a");

    struct HttpRequest *req = http_request_new(buf);
    if (req == NULL) return 1;

    memset(buf, 'X', sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    printf("owned %d url %s\n", req->url != buf, req->url);

    http_request_free(&req);
    return 0;
}
