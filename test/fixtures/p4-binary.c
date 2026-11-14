#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * The response body is bytes, not a C string: /binary sends a NUL in the
 * middle. Only body_len and total_bytes may be used for size; strlen would
 * report 1 and silently truncate.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/binary"));
    struct HttpResponse *res = http_request_send(req);
    const char *body = http_response_body(res);

    printf("len %d total %d bytes %d %d %d\n",
           http_response_body_len(res),
           http_response_total_bytes(res),
           (unsigned char)body[0],
           (unsigned char)body[1],
           (unsigned char)body[2]);

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
