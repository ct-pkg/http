#include "http.shim.h"

#include <stdio.h>
#include <string.h>

/*
 * Replace the request's ops table at runtime: the CT call point
 * (`->send()`) and the mapping unit stay untouched, only the struct field
 * changes. This is the seam phase 4 uses to install the real perform().
 */
static int g_fake_calls = 0;

static struct HttpResponse *fake_send(struct HttpRequest *r)
{
    (void)r;
    g_fake_calls = g_fake_calls + 1;

    struct HttpResponse *res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    if (res == NULL) return NULL;

    const char *text = "teapot";
    size_t n = strlen(text) + 1;
    res->body = (char *)malloc(n);
    if (res->body != NULL) {
        memcpy(res->body, text, n);
    }
    res->status = 418;
    return res;
}

static const struct HttpRequestOps fake_ops = { fake_send };

int main(void)
{
    struct HttpRequest *req = http_request_new("http://127.0.0.1:1/p2-ops");
    if (req == NULL) return 1;

    http_request_set_ops(req, &fake_ops);

    struct HttpResponse *res = http_request_send(req);
    if (res == NULL) return 2;

    printf("%d %s %d\n",
           http_response_status(res),
           http_response_body(res),
           g_fake_calls);

    http_response_free(&res);
    http_request_free(&req);
    return 0;
}
