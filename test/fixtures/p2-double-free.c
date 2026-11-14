#include "http.shim.h"

#include <stdio.h>

/* The slot form is idempotent: the second call sees NULL and does nothing. */
int main(void)
{
    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/p2-double-free");
    if (req == NULL) return 1;

    http_request_free(&req);
    http_request_free(&req);

    printf("%d %d %d\n",
           req == NULL,
           http_shim_easy_init_count(),
           http_shim_easy_cleanup_count());
    return 0;
}
