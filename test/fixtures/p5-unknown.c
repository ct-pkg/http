#include "http.shim.h"

#include <stdio.h>

/*
 * An unknown CURLcode must survive: 9999 is not in the mapped table, and
 * zeroing it would turn "something went wrong" into "no error at all".
 */
int main(void)
{
    http_shim_inject_curl_code(9999);

    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/p5-unknown");
    struct HttpResponse *res = http_request_send(req);

    printf("error %d\n", http_response_error(res));
    printf("detail %s\n", http_response_error_detail(res));

    http_response_free(&res);
    http_request_free(&req);
    http_shim_inject_curl_code(0);
    return 0;
}
