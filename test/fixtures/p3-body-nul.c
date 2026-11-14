#include "http.shim.h"

#include <stdio.h>

/*
 * The upload length is explicit: a NUL inside the payload must survive, and
 * POSTFIELDSIZE has to be applied before COPYPOSTFIELDS, otherwise libcurl
 * stops at the first NUL and sends a single byte.
 */
int main(void)
{
    const char payload[4] = { 'a', '\0', 'b', '\0' };

    struct HttpRequest *req = http_request_new("https://example.test/b");
    if (req == NULL) return 1;

    req = http_request_body(req, payload, 3);
    printf("len %d stage %d bytes %d %d %d\n",
           req->body_len,
           req->body_stage,
           (unsigned char)req->body[0],
           (unsigned char)req->body[1],
           (unsigned char)req->body[2]);

    struct HttpRequest *bad = http_request_new("https://example.test/n");
    if (bad == NULL) return 2;
    bad = http_request_body(bad, payload, -1);
    printf("badlen %d\n", bad->local_error == HTTP_LOCAL_ERR_BODY);

    http_request_free(&bad);
    http_request_free(&req);
    return 0;
}
