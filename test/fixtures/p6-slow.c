#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * A legitimate slow stream: one byte every 200 ms. The idle limit is 500 ms,
 * so a correct gap detector watching real data never fires. LOW_SPEED_LIMIT
 * style window averaging would; this fixture is the guard against that.
 *
 * The 200 ms cadence stands in for "1 byte every 59 s" so the suite stays fast;
 * the property under test (steady data, no gap above the limit) is the same.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/slow"));
    http_request_idle_timeout(req, 500);
    http_request_timeout(req, 5000);

    struct HttpResponse *res = http_request_send(req);
    printf("status %d body_len %d kind %d ok %d\n",
           http_response_status(res),
           http_response_body_len(res),
           http_response_timeout_kind(res),
           http_response_is_ok(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
