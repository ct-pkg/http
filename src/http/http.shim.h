#ifndef HTTP_SHIM_H
#define HTTP_SHIM_H

/*
 * Phase 1 shim: struct layout plus fixed-value stubs. No network and no
 * libcurl yet; phase 2 swaps the bodies for a real easy handle and adds the
 * ops table. The CT mapping unit (http.m.ct) only ever calls http_* here.
 */

#include <stdlib.h>

struct HttpRequest {
    char *url;
    int headers;
    int timeout_ms;
};

struct HttpResponse {
    int status;
    char *body;
};

/* Factory: the only place a request is constructed. */
static inline struct HttpRequest *http_request_new(const char *url)
{
    struct HttpRequest *r = (struct HttpRequest *)malloc(sizeof(struct HttpRequest));
    r->url = (char *)url;
    r->headers = 0;
    r->timeout_ms = 0;
    return r;
}

static inline struct HttpRequest *http_request_header(struct HttpRequest *r,
                                                      const char *name,
                                                      const char *value)
{
    if (name != 0 && value != 0) {
        r->headers = r->headers + 1;
    }
    return r;
}

static inline struct HttpRequest *http_request_timeout(struct HttpRequest *r, int ms)
{
    r->timeout_ms = ms;
    return r;
}

/* Stub: returns a fixed response. Phase 4 replaces this with perform(). */
static inline struct HttpResponse *http_request_send(struct HttpRequest *r)
{
    struct HttpResponse *resp = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    if (r->timeout_ms < 0) {
        resp->status = 408;
        resp->body = "stub: timeout";
    } else {
        resp->status = 200;
        resp->body = "{\"users\":3}";
    }
    return resp;
}

static inline int http_response_status(struct HttpResponse *r) { return r->status; }
static inline char *http_response_body(struct HttpResponse *r) { return r->body; }

#endif
