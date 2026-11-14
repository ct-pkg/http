#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * A 500 is an HTTP answer, not a transport failure: status carries it while
 * error stays 0. Treating it as an error would make "retry" and "the server
 * said no" indistinguishable.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/500"));
    struct HttpResponse *res = http_request_send(req);

    printf("status %d error %d ok %d\n",
           http_response_status(res),
           http_response_error(res),
           http_response_is_ok(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
