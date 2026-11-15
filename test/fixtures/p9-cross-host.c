#include "http.shim.h"
#include "p9-server.h"

#include <stdio.h>
#include <string.h>

static int body_has(struct HttpResponse *res, const char *needle)
{
    return res->body != NULL && strstr(res->body, needle) != NULL;
}

static struct HttpRequest *with_headers(const char *url)
{
    struct HttpRequest *r = http_request_new(url);
    http_request_timeout(r, 5000);
    http_request_follow_redirects(r, 1);
    http_request_header(r, "Authorization", "Bearer t");
    http_request_header(r, "Cookie", "s=1");
    http_request_header(r, "Proxy-Authorization", "Basic x");
    http_request_header(r, "Referer", "http://origin/secret");
    return r;
}

static void report(const char *label, struct HttpResponse *res)
{
    printf("%s auth %d cookie %d proxy %d status %d\n", label,
           body_has(res, "Authorization:"), body_has(res, "Cookie:"),
           body_has(res, "Proxy-Authorization:"), http_response_status(res));
    printf("%s referer %d\n", label, body_has(res, "Referer:"));
}

/*
 * Origin changes are tested on both axes the card defines:
 *   host: 127.0.0.1 -> localhost (same port)
 *   port: 127.0.0.1:PA -> 127.0.0.1:PB
 * 127.0.0.2 is not bindable on this macOS (errno 49), so the host axis uses
 * the localhost alias, which is the same machine under a different host name.
 */
int main(void)
{
    struct P9Server *a = p9_server_start("127.0.0.1");
    struct P9Server *b = p9_server_start("127.0.0.1");
    if (a == NULL || b == NULL) {
        printf("server failed\n");
        return 1;
    }
    p9_server_set_peer(a, p9_server_base(b));

    struct HttpRequest *cross = with_headers(p9_server_url(a, "/to-localhost"));
    struct HttpResponse *rc = http_request_send(cross);
    report("cross", rc);
    http_response_free(&rc);
    http_request_free(&cross);

    struct HttpRequest *port = with_headers(p9_server_url(a, "/to-peer"));
    struct HttpResponse *rp = http_request_send(port);
    printf("port auth %d cookie %d proxy %d status %d\n",
           body_has(rp, "Authorization:"), body_has(rp, "Cookie:"),
           body_has(rp, "Proxy-Authorization:"), http_response_status(rp));
    http_response_free(&rp);
    http_request_free(&port);

    struct HttpRequest *same = with_headers(p9_server_url(a, "/echo-headers"));
    struct HttpResponse *rs = http_request_send(same);
    report("same", rs);
    http_response_free(&rs);
    http_request_free(&same);

    p9_server_stop(b);
    p9_server_stop(a);
    return 0;
}
