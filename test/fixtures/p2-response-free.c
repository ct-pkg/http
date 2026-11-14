#include "http.shim.h"

#include <stdio.h>

/* The response owns its body; the slot form makes a second free a no-op. */
int main(void)
{
    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/p2-response-free");
    if (req == NULL) return 1;

    struct HttpResponse *res = http_request_send(req);
    if (res == NULL) return 2;

    http_response_free(&res);
    http_response_free(&res);

    printf("%d %d\n", res == NULL, http_shim_response_free_count());
    http_request_free(&req);
    return 0;
}
