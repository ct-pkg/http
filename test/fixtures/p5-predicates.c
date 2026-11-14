#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

static void show(const char *label, struct HttpResponse *res)
{
    printf("%s %d %d %d %d\n",
           label,
           http_response_status(res),
           http_response_error(res),
           http_response_is_ok(res),
           http_response_is_cancelled(res));
    http_response_free(&res);
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *ok = http_request_new(p4_server_url("/200"));
    show("ok", http_request_send(ok));
    http_request_free(&ok);

    struct HttpRequest *missing = http_request_new(p4_server_url("/404"));
    show("http404", http_request_send(missing));
    http_request_free(&missing);

    struct HttpRequest *broken = http_request_new("http://127.0.0.1:1/p5-predicates");
    show("transport", http_request_send(broken));
    http_request_free(&broken);

    /*
     * No cancel path exists yet (phase 8). The bit is set directly on a real
     * 200 response: same status and same error as row one, yet is_ok must flip
     * to 0. That is the measured proof the two predicates cannot both be true.
     */
    struct HttpRequest *cancel_req = http_request_new(p4_server_url("/200"));
    struct HttpResponse *cancel_res = http_request_send(cancel_req);
    cancel_res->cancelled = 1;
    show("cancelled", cancel_res);
    http_request_free(&cancel_req);

    p4_server_stop();
    return 0;
}
