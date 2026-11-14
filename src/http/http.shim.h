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
    char *url;        /* owned by the request since phase 3 */
    int headers;
    int timeout_ms;
    void *easy;       /* CURL *; opaque so this header stays curl-free */
    const struct HttpRequestOps *ops;
    void *headers_slist; /* struct curl_slist *; opaque */
    int local_error;     /* first local validation error, 0 = none */
    char *body;          /* owned copy of the upload buffer */
    int body_len;
    /* 0 untouched, 1 POSTFIELDSIZE applied, 2 COPYPOSTFIELDS applied,
       3 applied out of order (a bug the fixtures must catch). */
    int body_stage;
    int connect_timeout_ms;
    int insecure;         /* 0 = verify peer/host, the default */
    int follow_redirects; /* 0 = do not follow, the default */
    char *user_agent;     /* owned; defaults to HTTP_DEFAULT_USER_AGENT */
};

/*
 * Local validation codes are negative so they never collide with a CURLcode.
 * They are not the public error surface (see S-05); phase 5 decides that.
 */
#define HTTP_LOCAL_ERR_ARG (-1001)
#define HTTP_LOCAL_ERR_HEADER (-1002)
#define HTTP_LOCAL_ERR_BODY (-1003)
#define HTTP_LOCAL_ERR_SCHEME (-1004)
#define HTTP_LOCAL_ERR_TIMEOUT (-1005)
#define HTTP_LOCAL_ERR_NO_HANDLE (-1006)

#define HTTP_DEFAULT_USER_AGENT "ct-http/0.1.0"

struct HttpResponse {
    int status;
    char *body;       /* owned by the response; not a C string, use body_len */
    int body_len;     /* bytes in body, so an embedded NUL survives */
    int total_bytes;  /* bytes accumulated by the write callback */
    int error;        /* 0, a positive CURLcode, or a negative local code */
    char *error_msg;  /* owned; "<stage>: <host> (code N)", NULL when clean */
    char *error_detail; /* owned; "code=N stage=S host=H", reproducible */
    int cancelled;    /* set only by a real abort once phase 8 adds cancel() */
};

/* Construction and configuration. */
struct HttpRequest *http_request_new(const char *url);
struct HttpRequest *http_request_header(struct HttpRequest *r,
                                        const char *name,
                                        const char *value);
struct HttpRequest *http_request_timeout(struct HttpRequest *r, int ms);
struct HttpRequest *http_request_body(struct HttpRequest *r,
                                      const char *data,
                                      int len);
struct HttpRequest *http_request_connect_timeout(struct HttpRequest *r, int ms);
struct HttpRequest *http_request_user_agent(struct HttpRequest *r, const char *ua);
struct HttpRequest *http_request_insecure(struct HttpRequest *r, int on);
struct HttpRequest *http_request_follow_redirects(struct HttpRequest *r, int on);

/* Execution: one synchronous perform, still guarded by the local error. */
struct HttpResponse *http_request_send(struct HttpRequest *r);

/* Replace the ops table; NULL or a NULL ops field falls back to the default. */
void http_request_set_ops(struct HttpRequest *r, const struct HttpRequestOps *ops);

/* Read-only response view. */
int http_response_status(struct HttpResponse *r);
char *http_response_body(struct HttpResponse *r);
int http_response_body_len(struct HttpResponse *r);
int http_response_total_bytes(struct HttpResponse *r);

/* 0 = no error, > 0 = raw CURLcode, < 0 = local rejection (HTTP_LOCAL_ERR_*).
   An HTTP 4xx/5xx is not an error here: status carries that. */
int http_response_error(struct HttpResponse *r);

/* Human text and a deterministic detail string; both "" when error is 0. */
const char *http_response_error_msg(struct HttpResponse *r);
const char *http_response_error_detail(struct HttpResponse *r);

/* 2xx with no transport/local error and not cancelled. Never true for 3xx/4xx/5xx. */
int http_response_is_ok(struct HttpResponse *r);

/* Cancellation is a separate axis: is_ok() rules it out by construction. */
int http_response_is_cancelled(struct HttpResponse *r);

/* Move the body out: the response stops owning it (body becomes NULL and
   body_len 0), the caller must free() the returned pointer exactly once. */
char *http_response_take_body(struct HttpResponse *r);

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

/* Test hook: raw CURLcode of the last send (0 = none, 28 = timed out). */
int http_shim_last_curl_code(void);

/*
 * Test hook: make the next send report this CURLcode instead of performing.
 * Codes such as 6/60/42/23 cannot be produced offline and deterministically;
 * 7 and 28 are covered by real transfers.
 */
void http_shim_inject_curl_code(int code);

/* Test hook: the index-th accepted header as "Name: value", NULL past the end. */
const char *http_shim_header_line(struct HttpRequest *r, int index);

#endif
