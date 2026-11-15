#include "http.shim.h"
#include "p9-server.h"

#include <stdio.h>
#include <string.h>

/* Ten 302s followed, the eleventh request is the final 200 (S-03 ceiling). */
int main(void)
{
    struct P9Server *s = p9_server_start("127.0.0.1");
    if (s == NULL) {
        printf("server failed\n");
        return 1;
    }
    p9_server_set_chain(s, 10);

    struct HttpRequest *req = http_request_new(p9_server_url(s, "/hop/0"));
    http_request_timeout(req, 10000);
    http_request_follow_redirects(req, 1);
    struct HttpResponse *res = http_request_send(req);

    printf("status %d error %d requests %d body_ok %d\n",
           http_response_status(res), http_response_error(res),
           p9_server_requests(s),
           res->body != NULL && strcmp(res->body, "done") == 0);

    http_response_free(&res);
    http_request_free(&req);
    p9_server_stop(s);
    return 0;
}
