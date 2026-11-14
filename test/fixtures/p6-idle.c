#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * /stall sends one byte and then goes quiet. The idle limit is 500 ms; the
 * progress callback notices the gap about once a second and aborts. The raw
 * code would be 42 (aborted by callback), but for the caller this is a
 * timeout: error is normalised to 28 and the kind says IDLE.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/stall"));
    http_request_idle_timeout(req, 500);
    http_request_timeout(req, 5000);

    struct HttpResponse *res = http_request_send(req);
    const char *detail = http_response_error_detail(res);

    printf("error %d kind %d phase %s\n",
           http_response_error(res),
           http_response_timeout_kind(res),
           strstr(detail, "phase=idle") != NULL ? "idle" : "none");

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
