#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * The response must come from a real exchange. The server keeps the count in
 * shared memory, so `requests 3` is evidence that three connections were
 * accepted and served, not that send() returned a plausible value.
 */
static void probe(const char *path)
{
    struct HttpRequest *req = http_request_new(p4_server_url(path));
    if (req == NULL) {
        printf("alloc failed\n");
        return;
    }
    struct HttpResponse *res = http_request_send(req);
    printf("%d %s\n", http_response_status(res), http_response_body(res));
    http_response_free(&res);
    http_request_free(&req);
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    probe("/200");
    probe("/404");
    probe("/500");
    printf("requests %d\n", p4_server_requests());

    p4_server_stop();
    return 0;
}
