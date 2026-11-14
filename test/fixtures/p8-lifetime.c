#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * Lifetime contract: send() transfers nothing and releases nothing. The
 * request is still fully readable after it returns, and curl_easy_cleanup only
 * runs when the caller frees the request. ASAN must stay silent.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    char *url = p4_server_url("/200");
    struct HttpRequest *req = http_request_new(url);
    http_request_timeout(req, 5000);
    struct HttpResponse *res = http_request_send(req);

    printf("url_ok %d mode %d status %d\n",
           strcmp(req->url, url) == 0,
           req->mode,
           http_response_status(res));
    printf("cleanup_after_send %d\n", http_shim_easy_cleanup_count());

    http_response_free(&res);
    printf("cleanup_after_res_free %d responses_freed %d\n",
           http_shim_easy_cleanup_count(),
           http_shim_response_free_count());

    http_request_free(&req);
    printf("cleanup_after_req_free %d\n", http_shim_easy_cleanup_count());

    p4_server_stop();
    return 0;
}
