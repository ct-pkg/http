#include "http.shim.h"

#include <stdio.h>
#include <string.h>

/*
 * 192.0.2.1 is TEST-NET-1: nothing routes there, so the TCP connect itself
 * never completes. With connect_timeout 300 ms and a larger total, the raw
 * code is still 28 but the kind must say CONNECT, not TOTAL.
 */
int main(void)
{
    struct HttpRequest *req = http_request_new("http://192.0.2.1:81/p6-connect");
    http_request_connect_timeout(req, 300);
    http_request_timeout(req, 2000);

    struct HttpResponse *res = http_request_send(req);
    const char *detail = http_response_error_detail(res);

    printf("error %d kind %d phase %s\n",
           http_response_error(res),
           http_response_timeout_kind(res),
           strstr(detail, "phase=connect") != NULL ? "connect" : "none");

    http_response_free(&res);
    http_request_free(&req);
    return 0;
}
