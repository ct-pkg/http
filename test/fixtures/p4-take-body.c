#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * take_body moves ownership to the caller. The response is then released
 * while the taken bytes are still alive, so a forgotten null would free the
 * buffer twice: first in http_response_free, then in free(body). Run under
 * AddressSanitizer, this fixture is the proof that the field is cleared.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/200"));
    struct HttpResponse *res = http_request_send(req);

    int total = http_response_total_bytes(res);
    char *body = http_response_take_body(res);
    int len_after = http_response_body_len(res);
    char *again = http_response_take_body(res);

    printf("taken %d len %d again %d first %d total %d\n",
           body != NULL,
           len_after,
           again == NULL,
           (unsigned char)body[0],
           total);

    http_response_free(&res);
    free(body);

    http_request_free(&req);
    p4_server_stop();
    return 0;
}
