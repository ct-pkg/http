#include "http.shim.h"
#include "p9-server.h"

#include <stdio.h>
#include <string.h>

static int body_is_done(struct HttpResponse *res)
{
    return res->body != NULL && strcmp(res->body, "done") == 0;
}

int main(void)
{
    struct P9Server *s = p9_server_start("127.0.0.1");
    if (s == NULL) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *one = http_request_new(p9_server_url(s, "/redirect/1"));
    http_request_timeout(one, 5000);
    http_request_follow_redirects(one, 1);
    struct HttpResponse *r1 = http_request_send(one);
    printf("one status %d error %d requests %d body_ok %d\n",
           http_response_status(r1), http_response_error(r1),
           p9_server_requests(s), body_is_done(r1));
    http_response_free(&r1);
    http_request_free(&one);

    struct HttpRequest *two = http_request_new(p9_server_url(s, "/redirect/2"));
    http_request_timeout(two, 5000);
    http_request_follow_redirects(two, 1);
    struct HttpResponse *r2 = http_request_send(two);
    printf("two status %d error %d requests %d body_ok %d\n",
           http_response_status(r2), http_response_error(r2),
           p9_server_requests(s), body_is_done(r2));
    http_response_free(&r2);
    http_request_free(&two);

    p9_server_stop(s);
    return 0;
}
