#include "http.shim.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

/* Process-local state: each fixture runs in its own process. */
static int g_easy_init_count = 0;
static int g_easy_cleanup_count = 0;
static int g_response_free_count = 0;
static int g_curl_global_ready = 0;
static int g_last_curl_code = 0;
static int g_injected_curl_code = 0;

static struct HttpResponse *real_send(struct HttpRequest *r);
static const struct HttpRequestOps http_default_ops = { real_send };

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

/* Case-insensitive prefix test; no POSIX strncasecmp needed. */
static int prefix_ci(const char *s, const char *prefix)
{
    while (*prefix != '\0') {
        char a = *s;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (a != *prefix) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

static int has_http_scheme(const char *url)
{
    return prefix_ci(url, "http://") || prefix_ci(url, "https://");
}

struct HttpRequest *http_request_new(const char *url)
{
    if (url == NULL) {
        return NULL;
    }
    struct HttpRequest *r = (struct HttpRequest *)malloc(sizeof(struct HttpRequest));
    if (r == NULL) {
        return NULL;
    }
    r->url = dup_string(url);
    if (r->url == NULL) {
        free(r);
        return NULL;
    }
    ensure_curl_global();
    r->headers = 0;
    r->timeout_ms = 0;
    r->headers_slist = NULL;
    r->local_error = 0;
    r->body = NULL;
    r->body_len = 0;
    r->body_stage = 0;
    r->connect_timeout_ms = 0;
    r->insecure = 0;
    r->follow_redirects = 0;
    r->user_agent = dup_string(HTTP_DEFAULT_USER_AGENT);
    if (!has_http_scheme(r->url)) {
        r->local_error = HTTP_LOCAL_ERR_SCHEME;
    }
    r->easy = curl_easy_init();
    if (r->easy != NULL) {
        /* Belt and braces with the local scheme check: never leave the process. */
        curl_easy_setopt((CURL *)r->easy, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt((CURL *)r->easy, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
        if (r->user_agent != NULL) {
            curl_easy_setopt((CURL *)r->easy, CURLOPT_USERAGENT, r->user_agent);
        }
        g_easy_init_count = g_easy_init_count + 1;
    } else if (r->local_error == 0) {
        /* No handle means no transfer: report it now, not as a silent status 0. */
        r->local_error = HTTP_LOCAL_ERR_NO_HANDLE;
    }
    r->ops = &http_default_ops;
    return r;
}

static int has_crlf(const char *s)
{
    return s != NULL && (strchr(s, '\r') != NULL || strchr(s, '\n') != NULL);
}

/* "name: value", owned by the caller until it lands in the slist. */
static char *join_header(const char *name, const char *value)
{
    size_t nl = strlen(name);
    size_t vl = strlen(value);
    char *line = (char *)malloc(nl + 2 + vl + 1);
    if (line == NULL) {
        return NULL;
    }
    memcpy(line, name, nl);
    line[nl] = ':';
    line[nl + 1] = ' ';
    memcpy(line + nl + 2, value, vl + 1);
    return line;
}

struct HttpRequest *http_request_header(struct HttpRequest *r,
                                        const char *name,
                                        const char *value)
{
    if (r == NULL) {
        return NULL;
    }
    /* A CR/LF would let the caller append a second header line. */
    if (name == NULL || value == NULL || has_crlf(name) || has_crlf(value)) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_HEADER;
        }
        return r;
    }

    char *line = join_header(name, value);
    if (line == NULL) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_HEADER;
        }
        return r;
    }

    struct curl_slist *next = curl_slist_append((struct curl_slist *)r->headers_slist, line);
    free(line);
    if (next == NULL) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_HEADER;
        }
        return r;
    }

    r->headers_slist = next;
    r->headers = r->headers + 1;
    return r;
}

struct HttpRequest *http_request_timeout(struct HttpRequest *r, int ms)
{
    if (r == NULL) {
        return NULL;
    }
    if (ms < 0) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_TIMEOUT;
        }
        return r;
    }
    r->timeout_ms = ms;
    if (r->easy != NULL) {
        curl_easy_setopt((CURL *)r->easy, CURLOPT_TIMEOUT_MS, (long)ms);
    }
    return r;
}

struct HttpRequest *http_request_connect_timeout(struct HttpRequest *r, int ms)
{
    if (r == NULL) {
        return NULL;
    }
    if (ms < 0) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_TIMEOUT;
        }
        return r;
    }
    r->connect_timeout_ms = ms;
    if (r->easy != NULL) {
        curl_easy_setopt((CURL *)r->easy, CURLOPT_CONNECTTIMEOUT_MS, (long)ms);
    }
    return r;
}

struct HttpRequest *http_request_user_agent(struct HttpRequest *r, const char *ua)
{
    if (r == NULL) {
        return NULL;
    }
    if (ua == NULL) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_ARG;
        }
        return r;
    }
    char *copy = dup_string(ua);
    if (copy == NULL) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_ARG;
        }
        return r;
    }
    free(r->user_agent);
    r->user_agent = copy;
    if (r->easy != NULL) {
        curl_easy_setopt((CURL *)r->easy, CURLOPT_USERAGENT, r->user_agent);
    }
    return r;
}

struct HttpRequest *http_request_insecure(struct HttpRequest *r, int on)
{
    if (r == NULL) {
        return NULL;
    }
    r->insecure = on != 0;
    if (r->easy != NULL) {
        /* Both switches: VERIFYHOST alone still rejects a self-signed peer. */
        curl_easy_setopt((CURL *)r->easy, CURLOPT_SSL_VERIFYPEER, r->insecure ? 0L : 1L);
        curl_easy_setopt((CURL *)r->easy, CURLOPT_SSL_VERIFYHOST, r->insecure ? 0L : 2L);
    }
    return r;
}

struct HttpRequest *http_request_follow_redirects(struct HttpRequest *r, int on)
{
    if (r == NULL) {
        return NULL;
    }
    r->follow_redirects = on != 0;
    if (r->easy != NULL) {
        curl_easy_setopt((CURL *)r->easy, CURLOPT_FOLLOWLOCATION,
                         r->follow_redirects ? 1L : 0L);
    }
    return r;
}

struct HttpRequest *http_request_body(struct HttpRequest *r,
                                      const char *data,
                                      int len)
{
    if (r == NULL) {
        return NULL;
    }
    if (data == NULL || len < 0) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_BODY;
        }
        return r;
    }

    size_t n = (size_t)len;
    char *copy = (char *)malloc(n + 1);
    if (copy == NULL) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_BODY;
        }
        return r;
    }
    if (n > 0) {
        memcpy(copy, data, n);
    }
    copy[n] = '\0';

    free(r->body);
    r->body = copy;
    r->body_len = len;

    /* Size first: without it libcurl would stop at the first NUL. */
    if (r->easy != NULL) {
        curl_easy_setopt((CURL *)r->easy, CURLOPT_POSTFIELDSIZE, (long)len);
        r->body_stage = (r->body_stage == 0) ? 1 : 3;
        curl_easy_setopt((CURL *)r->easy, CURLOPT_COPYPOSTFIELDS, copy);
        r->body_stage = (r->body_stage == 1) ? 2 : 3;
    }
    return r;
}

/* scheme://authority, no path or query: the message must not leak call data. */
static void host_part(const char *url, char *out, size_t cap)
{
    if (cap == 0) {
        return;
    }
    const char *p = strstr(url, "://");
    p = p != NULL ? p + 3 : url;
    size_t i = 0;
    while (p[i] != '\0' && p[i] != '/' && p[i] != '?' && p[i] != '#'
           && i + 1 < cap) {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
}

static const char *error_stage(int code)
{
    if (code < 0) {
        return "local";
    }
    switch (code) {
    case 6:
        return "resolve";
    case 7:
        return "connect";
    case 28:
        return "timeout";
    case 35:
    case 60:
        return "tls";
    case 23:
    case 55:
        return "write";
    case 42:
        return "cancelled";
    default:
        return "transfer";
    }
}

static char *make_error_msg(const char *url, int code)
{
    char host[128];
    char buf[256];
    host_part(url, host, sizeof host);
    snprintf(buf, sizeof buf, "%s: %s (code %d)", error_stage(code), host, code);
    return dup_string(buf);
}

/* Same inputs give the same string; nothing here is time or address based. */
static char *make_error_detail(const char *url, int code)
{
    char host[128];
    char buf[320];
    host_part(url, host, sizeof host);
    snprintf(buf, sizeof buf, "code=%d stage=%s host=%s", code,
             error_stage(code), host);
    return dup_string(buf);
}

/* Growable response buffer filled by the write callback. */
struct BodyBuf {
    char *data;
    size_t len;
    size_t cap;
};

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    struct BodyBuf *b = (struct BodyBuf *)ud;
    size_t n = size * nmemb;
    if (n == 0) {
        return 0;
    }
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap != 0 ? b->cap : 128;
        while (cap < b->len + n + 1) {
            cap = cap * 2;
        }
        char *grown = (char *)realloc(b->data, cap);
        if (grown == NULL) {
            return 0; /* aborts the transfer with CURLE_WRITE_ERROR */
        }
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, ptr, n);
    b->len = b->len + n;
    b->data[b->len] = '\0';
    return n;
}

/*
 * One synchronous exchange. The request keeps its easy handle: send() never
 * cleans up, so the caller's explicit release stays the only owner. A failed
 * transfer still returns a response (status 0) so the release path is uniform.
 */
static struct HttpResponse *real_send(struct HttpRequest *r)
{
    struct HttpResponse *resp = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    if (resp == NULL) {
        return NULL;
    }
    resp->status = 0;
    resp->body = NULL;
    resp->body_len = 0;
    resp->total_bytes = 0;
    resp->error = 0;
    resp->error_msg = NULL;
    resp->error_detail = NULL;
    g_last_curl_code = 0;

    /* Local rejections keep their negative code; no handle is its own case. */
    if (r->local_error != 0 || r->easy == NULL) {
        int local = r->local_error != 0 ? r->local_error : HTTP_LOCAL_ERR_NO_HANDLE;
        resp->error = local;
        resp->body = dup_string("");
        resp->error_msg = make_error_msg(r->url != NULL ? r->url : "", local);
        resp->error_detail = make_error_detail(r->url != NULL ? r->url : "", local);
        return resp;
    }

    struct BodyBuf buf;
    buf.data = NULL;
    buf.len = 0;
    buf.cap = 0;

    CURL *easy = (CURL *)r->easy;
    curl_easy_setopt(easy, CURLOPT_URL, r->url);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &buf);

    int transport;
    if (g_injected_curl_code != 0) {
        transport = g_injected_curl_code; /* test seam: no transfer at all */
    } else {
        transport = (int)curl_easy_perform(easy);
    }
    g_last_curl_code = transport;

    long code = 0;
    if (g_injected_curl_code == 0) {
        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
    }
    resp->status = (int)code;
    resp->error = transport;
    resp->body = buf.data != NULL ? buf.data : dup_string("");
    resp->body_len = (int)buf.len;
    /* Written down here: total_bytes is the write-callback total, not a
       getinfo figure, so the two sizes can never disagree. */
    resp->total_bytes = (int)buf.len;
    if (transport != 0) {
        resp->error_msg = make_error_msg(r->url, transport);
        resp->error_detail = make_error_detail(r->url, transport);
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
int http_response_body_len(struct HttpResponse *r) { return r->body_len; }
int http_response_total_bytes(struct HttpResponse *r) { return r->total_bytes; }
int http_response_error(struct HttpResponse *r) { return r->error; }

const char *http_response_error_msg(struct HttpResponse *r)
{
    return r->error_msg != NULL ? r->error_msg : "";
}

const char *http_response_error_detail(struct HttpResponse *r)
{
    return r->error_detail != NULL ? r->error_detail : "";
}

int http_response_is_ok(struct HttpResponse *r)
{
    return r->status >= 200 && r->status < 300 && r->error == 0;
}

/*
 * Single ownership: the pointer leaves the response and the response forgets
 * it, so its release frees only the struct. A second take sees NULL.
 */
char *http_response_take_body(struct HttpResponse *r)
{
    if (r == NULL || r->body == NULL) {
        return NULL;
    }
    char *body = r->body;
    r->body = NULL;
    r->body_len = 0;
    return body;
}

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
    /* The request owns its URL from the factory onward. */
    free(r->url);
    r->url = NULL;
    curl_slist_free_all((struct curl_slist *)r->headers_slist);
    r->headers_slist = NULL;
    free(r->body);
    r->body = NULL;
    free(r->user_agent);
    r->user_agent = NULL;
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
    free(r->error_msg);
    r->error_msg = NULL;
    free(r->error_detail);
    r->error_detail = NULL;
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

/*
 * The raw CURLcode of the last send, 0 when nothing was performed. A test
 * hook only: the public error surface is still undecided (S-05, phase 5).
 * Note that connect and total timeouts both surface as 28 here.
 */
int http_shim_last_curl_code(void) { return g_last_curl_code; }

void http_shim_inject_curl_code(int code) { g_injected_curl_code = code; }

const char *http_shim_header_line(struct HttpRequest *r, int index)
{
    if (r == NULL || index < 0) {
        return NULL;
    }
    struct curl_slist *node = (struct curl_slist *)r->headers_slist;
    for (int i = 0; node != NULL && i < index; i++) {
        node = node->next;
    }
    return node != NULL ? node->data : NULL;
}
