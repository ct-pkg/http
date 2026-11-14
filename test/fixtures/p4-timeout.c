#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * /hang sleeps past the client's 300 ms total timeout, so the transfer ends
 * with CURLE_OPERATION_TIMEDOUT (28). The same code is what a connect timeout
 * would produce; that indistinguishability is recorded in the phase closeout,
 * not papered over here.
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
    printf("status %d code %d\n",
           http_response_status(res),
           http_shim_last_curl_code());

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
