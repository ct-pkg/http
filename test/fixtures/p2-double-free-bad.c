#include "http.shim.h"

/*
 * Negative fixture: bypass the slot form by keeping a stale copy. The second
 * free must trip AddressSanitizer. This is what proves the idempotent slot in
 * p2-double-free.c is doing real work instead of passing vacuously.
 */
int main(void)
{
    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/p2-double-free-bad");
    if (req == NULL) return 1;

    struct HttpRequest *stale = req;
    http_request_free(&req);
    http_request_free(&stale);

    return 0;
}
