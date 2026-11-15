#include "http.shim.h"
#include "p9-server.h"

#include <stdio.h>

/* A -> B -> A must end at the fixed ceiling, not spin. */
int main(void)
{
    struct P9Server *s = p9_server_start("127.0.0.1");
    if (s == NULL) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p9_server_url(s, "/loop/a"));
    http_request_timeout(req, 10000);
    http_request_follow_redirects(req, 1);
    struct HttpResponse *res = http_request_send(req);

    printf("error %d requests %d status %d\n",
           http_response_error(res), p9_server_requests(s),
           http_response_status(res));

    http_response_free(&res);
    http_request_free(&req);
    p9_server_stop(s);
    return 0;
}
