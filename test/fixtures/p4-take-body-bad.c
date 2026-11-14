#include "http.shim.h"
#include "p4-server.h"

#include <stdlib.h>

/*
 * Negative fixture: model the un-nulled take. Keep the body pointer, release
 * the response (which frees the buffer), then free the stale pointer. ASAN
 * must report double-free; otherwise p4-take-body.c would pass vacuously.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/200"));
    struct HttpResponse *res = http_request_send(req);

    char *stale = http_response_body(res);
    http_response_free(&res);
    free(stale);

    http_request_free(&req);
    p4_server_stop();
    return 0;
}
