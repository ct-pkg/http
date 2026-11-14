#ifndef HTTP_SHIM_H
#define HTTP_SHIM_H

/*
 * Public C surface. The CT mapping unit (http.m.ct) only calls http_* here;
 * libcurl stays inside http.shim.c, so no curl type or symbol leaks into a
 * CT-generated object. Phase 2 owns the easy-handle lifetime and explicit
 * release; phase 3 adds request configuration, phase 4 real execution.
 */

#include <stdlib.h>

struct HttpRequest;
struct HttpResponse;

/*
 * Runtime replacement seam: the request carries its ops table as a field, so
 * a test or phase 4 can install a different send without touching the CT
 * call point or the mapping unit.
 */
struct HttpRequestOps {
    struct HttpResponse *(*send)(struct HttpRequest *r);
};

struct HttpRequest {
    char *url;        /* borrowed until phase 3 takes ownership */
    int headers;
    int timeout_ms;
    void *easy;       /* CURL *; opaque so this header stays curl-free */
    const struct HttpRequestOps *ops;
};

struct HttpResponse {
    int status;
    char *body;       /* owned by the response */
};

/* Construction and configuration. */
struct HttpRequest *http_request_new(const char *url);
struct HttpRequest *http_request_header(struct HttpRequest *r,
                                        const char *name,
                                        const char *value);
struct HttpRequest *http_request_timeout(struct HttpRequest *r, int ms);

/* Execution (still a fixed-value stub until phase 4). */
struct HttpResponse *http_request_send(struct HttpRequest *r);

/* Replace the ops table; NULL or a NULL ops field falls back to the default. */
void http_request_set_ops(struct HttpRequest *r, const struct HttpRequestOps *ops);

/* Read-only response view. */
int http_response_status(struct HttpResponse *r);
char *http_response_body(struct HttpResponse *r);

/*
 * Explicit release. Nothing runs curl_easy_cleanup on scope exit: the caller
 * owns every request and response and must release it exactly once.
 *
 *   http_request_free(&r)   slot form: nulls the caller's pointer, idempotent
 *   http_request_release()  terminal form used by the CT `.free()` method
 */
void http_request_free(struct HttpRequest **slot);
void http_request_release(struct HttpRequest *r);
void http_response_free(struct HttpResponse **slot);
void http_response_release(struct HttpResponse *r);

/* Test hooks: process-local counters, reset by starting a new process. */
int http_shim_easy_init_count(void);
int http_shim_easy_cleanup_count(void);
int http_shim_response_free_count(void);

#endif
