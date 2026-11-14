#include "http.shim.h"

#include <stdio.h>

/*
 * A header name or value containing CR/LF would let a caller append a second
 * header line. Both are rejected locally and never reach the slist.
 */
int main(void)
{
    struct HttpRequest *bad_value = http_request_new("https://example.test/v");
    struct HttpRequest *bad_name = http_request_new("https://example.test/n");
    struct HttpRequest *good = http_request_new("https://example.test/o");
    if (bad_value == NULL || bad_name == NULL || good == NULL) return 1;

    bad_value = http_request_header(bad_value, "X", "a\r\nB: c");
    bad_name = http_request_header(bad_name, "Y\n", "b");
    good = http_request_header(good, "Accept", "application/json");

    printf("rejected %d %d added %d %d first %s\n",
           bad_value->local_error == HTTP_LOCAL_ERR_HEADER,
           bad_name->local_error == HTTP_LOCAL_ERR_HEADER,
           bad_value->headers,
           good->headers,
           http_shim_header_line(good, 0));

    http_request_free(&bad_value);
    http_request_free(&bad_name);
    http_request_free(&good);
    return 0;
}
