#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * A negative limit is a local error, not "no limit": the request must never
 * leave the process. Both the idle and the total setter share the -1005 code.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *idle = http_request_new(p4_server_url("/200"));
    http_request_idle_timeout(idle, -1);
    struct HttpResponse *idle_res = http_request_send(idle);

    struct HttpRequest *total = http_request_new(p4_server_url("/200"));
    http_request_timeout(total, -1);
    struct HttpResponse *total_res = http_request_send(total);

    printf("idle %d timeout %d requests %d\n",
           http_response_error(idle_res),
           http_response_error(total_res),
           p4_server_requests());

    http_response_free(&idle_res);
    http_response_free(&total_res);
    http_request_free(&idle);
    http_request_free(&total);
    p4_server_stop();
    return 0;
}
