#include "http.shim.h"

#include <curl/curl.h>
#include <string.h>

/* Process-local state: each fixture runs in its own process. */
static int g_easy_init_count = 0;
static int g_easy_cleanup_count = 0;
static int g_response_free_count = 0;
static int g_curl_global_ready = 0;

static struct HttpResponse *stub_send(struct HttpRequest *r);
static const struct HttpRequestOps http_default_ops = { stub_send };

static void ensure_curl_global(void)
{
    if (!g_curl_global_ready) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_global_ready = 1;
    }
}

/* Duplicate so the response owns its body; phase 4 replaces this with the
   write callback buffer. */
static char *dup_string(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

struct HttpRequest *http_request_new(const char *url)
{
    struct HttpRequest *r = (struct HttpRequest *)malloc(sizeof(struct HttpRequest));
    if (r == NULL) {
        return NULL;
    }
    ensure_curl_global();
    r->url = (char *)url;
    r->headers = 0;
    r->timeout_ms = 0;
    r->easy = curl_easy_init();
    if (r->easy != NULL) {
        g_easy_init_count = g_easy_init_count + 1;
    }
    r->ops = &http_default_ops;
    return r;
}

struct HttpRequest *http_request_header(struct HttpRequest *r,
                                        const char *name,
                                        const char *value)
{
    if (name != NULL && value != NULL) {
        r->headers = r->headers + 1;
    }
    return r;
}

struct HttpRequest *http_request_timeout(struct HttpRequest *r, int ms)
{
    r->timeout_ms = ms;
    return r;
}

/* Stub: returns a fixed response. Phase 4 replaces this with perform(). */
static struct HttpResponse *stub_send(struct HttpRequest *r)
{
    struct HttpResponse *resp = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    if (resp == NULL) {
        return NULL;
    }
    if (r->timeout_ms < 0) {
        resp->status = 408;
        resp->body = dup_string("stub: timeout");
    } else {
        resp->status = 200;
        resp->body = dup_string("{\"users\":3}");
    }
    return resp;
}

/* Dispatch through the struct field: no vtable lookup, one indirect call. */
struct HttpResponse *http_request_send(struct HttpRequest *r)
{
    const struct HttpRequestOps *ops = r->ops != NULL ? r->ops : &http_default_ops;
    return ops->send(r);
}

void http_request_set_ops(struct HttpRequest *r, const struct HttpRequestOps *ops)
{
    if (r == NULL) {
        return;
    }
    r->ops = ops != NULL ? ops : &http_default_ops;
}

int http_response_status(struct HttpResponse *r) { return r->status; }
char *http_response_body(struct HttpResponse *r) { return r->body; }

/*
 * The one place a request is destroyed. The easy handle is cleaned up here
 * and nowhere else, so the counter proves ownership: send() never cleans up.
 */
static void request_destroy(struct HttpRequest *r)
{
    if (r == NULL) {
        return;
    }
    if (r->easy != NULL) {
        curl_easy_cleanup((CURL *)r->easy);
        g_easy_cleanup_count = g_easy_cleanup_count + 1;
        r->easy = NULL;
    }
    /* r->url stays borrowed until phase 3 copies it. */
    free(r);
}

void http_request_free(struct HttpRequest **slot)
{
    if (slot == NULL || *slot == NULL) {
        return;
    }
    struct HttpRequest *r = *slot;
    *slot = NULL;            /* null first: a second call is a no-op */
    request_destroy(r);
}

void http_request_release(struct HttpRequest *r)
{
    http_request_free(&r);
}

/* The response owns its body: release frees both the buffer and the struct. */
static void response_destroy(struct HttpResponse *r)
{
    if (r == NULL) {
        return;
    }
    free(r->body);
    r->body = NULL;
    g_response_free_count = g_response_free_count + 1;
    free(r);
}

void http_response_free(struct HttpResponse **slot)
{
    if (slot == NULL || *slot == NULL) {
        return;
    }
    struct HttpResponse *r = *slot;
    *slot = NULL;
    response_destroy(r);
}

void http_response_release(struct HttpResponse *r)
{
    http_response_free(&r);
}

int http_shim_easy_init_count(void) { return g_easy_init_count; }
int http_shim_easy_cleanup_count(void) { return g_easy_cleanup_count; }
int http_shim_response_free_count(void) { return g_response_free_count; }
