#include "http.shim.h"

#include <stdio.h>

/*
 * A refused connection is a transport failure, not an HTTP response: status
 * stays 0 and the CURLcode (7 = CURLE_COULDNT_CONNECT) is observable through
 * the shim hook. This is the failure side of "really performed".
 */
int main(void)
{
    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/refused");
    struct HttpResponse *res = http_request_send(req);

    printf("status %d code %d\n",
           http_response_status(res),
           http_shim_last_curl_code());

    http_response_free(&res);
    http_request_free(&req);
    return 0;
}
