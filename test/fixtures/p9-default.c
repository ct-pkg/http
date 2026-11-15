#include "http.shim.h"
#include "p9-server.h"

#include <stdio.h>
#include <string.h>

/* follow_redirects(0) must stay byte-for-byte the pre-phase-9 behaviour. */
int main(void)
{
    struct P9Server *s = p9_server_start("127.0.0.1");
    if (s == NULL) {
        printf("server failed\n");
        return 1;
    }
    p9_server_set_chain(s, 10);

    struct HttpRequest *req = http_request_new(p9_server_url(s, "/hop/0"));
    http_request_timeout(req, 5000);
    struct HttpResponse *res = http_request_send(req);

    printf("status %d error %d ok %d requests %d\n",
           http_response_status(res), http_response_error(res),
           http_response_is_ok(res), p9_server_requests(s));

    http_response_free(&res);
    http_request_free(&req);
    p9_server_stop(s);
    return 0;
}
