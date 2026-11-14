#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * /hang accepts and never answers. With a 300 ms total timeout and no connect
 * timeout, the connection is established and no body byte ever arrives: the
 * kind must be TOTAL, and the server must have accepted the request.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/hang"));
    http_request_timeout(req, 300);

    struct HttpResponse *res = http_request_send(req);
    const char *detail = http_response_error_detail(res);

    printf("status %d error %d kind %d phase %s requests %d\n",
           http_response_status(res),
           http_response_error(res),
           http_response_timeout_kind(res),
           strstr(detail, "phase=total") != NULL ? "total" : "none",
           p4_server_requests());

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
