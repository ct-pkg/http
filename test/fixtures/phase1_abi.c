#include "http.shim.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Field order is part of the public ABI: the CT mapping hands these structs
   to the stub functions by pointer. */
typedef char assert_url_first[(offsetof(struct HttpRequest, url) == 0) ? 1 : -1];
typedef char assert_headers_after_url[
    (offsetof(struct HttpRequest, headers) > offsetof(struct HttpRequest, url)) ? 1 : -1];
typedef char assert_timeout_after_headers[
    (offsetof(struct HttpRequest, timeout_ms) > offsetof(struct HttpRequest, headers)) ? 1 : -1];
typedef char assert_status_first[(offsetof(struct HttpResponse, status) == 0) ? 1 : -1];
typedef char assert_body_after_status[
    (offsetof(struct HttpResponse, body) > offsetof(struct HttpResponse, status)) ? 1 : -1];

/* This fixture owns the ABI layout, not the network. Since phase 4 send()
   really performs, the ABI probe installs a fixed ops table so its output
   stays byte-for-byte what phase 1 froze. */
static struct HttpResponse *abi_send(struct HttpRequest *r)
{
    (void)r;
    static const char text[] = "{\"users\":3}";

    struct HttpResponse *res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    if (res == NULL) return NULL;
    memset(res, 0, sizeof *res);

    res->body = (char *)malloc(sizeof text);
    if (res->body == NULL) {
        free(res);
        return NULL;
    }
    memcpy(res->body, text, sizeof text);
    res->status = 200;
    return res;
}

static const struct HttpRequestOps abi_ops = { abi_send };

int main(void)
{
    struct HttpRequest *req = http_request_new("https://api.example.com/users");
    if (req == NULL || req->url == NULL) return 1;

    req = http_request_header(req, "Accept", "application/json");
    if (req->headers != 1) return 2;

    req = http_request_timeout(req, 3000);
    if (req->timeout_ms != 3000) return 3;

    http_request_set_ops(req, &abi_ops);

    struct HttpResponse *res = http_request_send(req);
    if (res == NULL || http_response_status(res) != 200) return 4;

    printf("abi ok %d %s\n", http_response_status(res), http_response_body(res));
    return 0;
}
