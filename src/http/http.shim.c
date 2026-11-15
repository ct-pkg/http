#include "http.shim.h"

#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

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
    r->idle_timeout_ms = 0;
    r->max_body_bytes = 0;
    r->mode = 0;
    r->user_agent = dup_string(HTTP_DEFAULT_USER_AGENT);
    r->cancel_requested = 0;
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

/*
 * Idle is data-gap based, not average-speed based: a stream that keeps sending
 * slowly must never be killed for being slow. A negative ms is a local error
 * here, same code as a negative total timeout.
 */
struct HttpRequest *http_request_idle_timeout(struct HttpRequest *r, int ms)
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
    r->idle_timeout_ms = ms;
    return r;
}

struct HttpRequest *http_request_max_body_bytes(struct HttpRequest *r, int n)
{
    if (r == NULL) {
        return NULL;
    }
    if (n < 0) {
        if (r->local_error == 0) {
            r->local_error = HTTP_LOCAL_ERR_ARG;
        }
        return r;
    }
    r->max_body_bytes = n;
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
        /* Following is ours: libcurl must not follow and hide the hops. */
        curl_easy_setopt((CURL *)r->easy, CURLOPT_FOLLOWLOCATION, 0L);
    }
    return r;
}

/* Sticky by design: a cancelled request is not reusable, rebuild it to retry. */
void http_request_cancel(struct HttpRequest *r)
{
    if (r != NULL) {
        r->cancel_requested = 1;
    }
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
    if (code == HTTP_LOCAL_ERR_STATE) {
        return "state";
    }
    if (code == HTTP_LOCAL_ERR_BODY_LIMIT) {
        return "body-limit";
    }
    if (code == HTTP_LOCAL_ERR_REDIRECT) {
        return "redirect";
    }
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
static char *make_error_detail(const char *url, int code, int kind, int limit)
{
    char host[128];
    char buf[384];
    host_part(url, host, sizeof host);
    if (code == HTTP_LOCAL_ERR_BODY_LIMIT) {
        snprintf(buf, sizeof buf, "code=%d stage=%s limit=%d host=%s", code,
                 error_stage(code), limit, host);
    } else if (kind == HTTP_TIMEOUT_NONE) {
        snprintf(buf, sizeof buf, "code=%d stage=%s host=%s", code,
                 error_stage(code), host);
    } else {
        static const char *names[] = { "none", "connect", "total", "idle" };
        snprintf(buf, sizeof buf, "code=%d stage=%s phase=%s host=%s", code,
                 error_stage(code), names[kind], host);
    }
    return dup_string(buf);
}

/* Fill an already-allocated response with a local rejection. */
static void fill_local_error(struct HttpResponse *resp, const char *url,
                             int code, int with_body)
{
    resp->error = code;
    if (with_body) {
        resp->body = dup_string("");
    }
    resp->error_msg = make_error_msg(url != NULL ? url : "", code);
    resp->error_detail =
        make_error_detail(url != NULL ? url : "", code, HTTP_TIMEOUT_NONE, 0);
}

/*
 * Cancellation before any byte arrives: status stays 0 because no response
 * header was read, but error/cancelled are exactly the mid-transfer shape.
 */
static void fill_cancel_error(struct HttpResponse *resp, const char *url,
                              int with_body)
{
    resp->error = (int)CURLE_ABORTED_BY_CALLBACK;
    resp->cancelled = 1;
    resp->timeout_kind = HTTP_TIMEOUT_NONE;
    if (with_body) {
        resp->body = dup_string("");
    }
    resp->error_msg = make_error_msg(url != NULL ? url : "", resp->error);
    resp->error_detail = make_error_detail(url != NULL ? url : "", resp->error,
                                           HTTP_TIMEOUT_NONE, 0);
}

/* Wall clock in microseconds: short-lived transfers, only differences matter. */
static int64_t now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

/* Growable response buffer filled by the write callback. */
struct XferState {
    int64_t last_us;       /* last time a data byte arrived, or the send start */
    int idle_ms;
    int first_byte;
    int idle_fired;
    const int *cancel_flag; /* -> HttpRequest.cancel_requested; NULL never */
    int cancel_fired;       /* the flag, not the idle gap, stopped the transfer */
    int aborted;            /* the user callback refused a chunk (23), not a cancel */
};

struct BodyBuf {
    char *data;
    size_t len;
    size_t cap;
    size_t max;
    int limit_fired;
    struct XferState *xfer;
};

/* One write sink for both entry points; discard marks an intermediate 3xx. */
struct HopSink {
    int streaming;
    struct BodyBuf *buf; /* non-stream only */
    HttpChunkCb cb;      /* stream only */
    void *ud;
    size_t delivered;
    size_t max;
    int limit_fired;
    struct XferState *xfer;
    int discard; /* 1 while this hop is a redirect we are about to follow */
};

/* 0 on success, -1 when realloc fails (libcurl then reports 23). */
static int body_append(struct BodyBuf *b, const char *ptr, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap != 0 ? b->cap : 128;
        char *grown;
        while (cap < b->len + n + 1) {
            cap = cap * 2;
        }
        grown = (char *)realloc(b->data, cap);
        if (grown == NULL) {
            return -1;
        }
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, ptr, n);
    b->len = b->len + n;
    b->data[b->len] = '\0';
    return 0;
}

/* Drop an intermediate redirect body before the next hop. */
static void body_reset(struct BodyBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static size_t hop_write_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    struct HopSink *s = (struct HopSink *)ud;
    size_t n = size * nmemb;
    if (n == 0) {
        return 0;
    }
    /* Cancel before delivery: the chunk never reaches the buffer or the count. */
    if (s->xfer->cancel_flag != NULL && *s->xfer->cancel_flag) {
        s->xfer->cancel_fired = 1;
        return 0;
    }
    /* Intermediate redirect body: read and drop, do not count or deliver. */
    if (s->discard) {
        return n;
    }
    /*
     * Option A cap: deliver exactly max bytes, then stop. Equal to the cap is
     * not an overflow, so an exactly-max response still succeeds.
     */
    if (s->streaming) {
        size_t keep = n;
        if (s->max > 0 && s->delivered + n > s->max) {
            keep = s->max - s->delivered;
            s->limit_fired = 1;
        }
        if (keep > 0) {
            s->delivered = s->delivered + keep;
            s->xfer->last_us = now_us();
            s->xfer->first_byte = 1;
            if (s->cb != NULL && s->cb(ptr, (int)keep, s->ud) != 0) {
                s->xfer->aborted = 1; /* libcurl reports 23 */
                return 0;
            }
        }
        if (s->limit_fired) {
            return 0;
        }
        return n;
    }

    if (s->max > 0 && s->buf->len + n > s->max) {
        size_t keep = s->max - s->buf->len;
        s->limit_fired = 1;
        if (keep > 0) {
            if (body_append(s->buf, ptr, keep) != 0) {
                s->xfer->aborted = 1;
                return 0;
            }
            s->xfer->last_us = now_us();
            s->xfer->first_byte = 1;
        }
        return 0;
    }
    if (body_append(s->buf, ptr, n) != 0) {
        s->xfer->aborted = 1;
        return 0; /* aborts the transfer with CURLE_WRITE_ERROR */
    }
    s->xfer->last_us = now_us();
    s->xfer->first_byte = 1;
    return n;
}

/* libcurl calls this about once a second; cancel wins over the gap check. */
static int xfer_cb(void *ud, curl_off_t dlt, curl_off_t dln,
                   curl_off_t ult, curl_off_t uln)
{
    struct XferState *s = (struct XferState *)ud;
    (void)dlt;
    (void)dln;
    (void)ult;
    (void)uln;
    /* A callback abort already ended the transfer: do not relabel it as cancel. */
    if (s->aborted) {
        return 0;
    }
    if (s->cancel_flag != NULL && *s->cancel_flag) {
        s->cancel_fired = 1;
        return 1; /* aborts the transfer with CURLE_ABORTED_BY_CALLBACK */
    }
    if (s->idle_ms > 0
        && now_us() - s->last_us >= (int64_t)s->idle_ms * 1000) {
        s->idle_fired = 1;
        return 1; /* the same code; the kind is what tells them apart */
    }
    return 0;
}

/*
 * One synchronous exchange. The request keeps its easy handle: send() never
 * cleans up, so the caller's explicit release stays the only owner. A failed
 * transfer still returns a response (status 0) so the release path is uniform.
 *
 * Redirects are followed here, not by libcurl: each hop needs its own decision
 * about credentials and scheme, which CURLOPT_FOLLOWLOCATION cannot express.
 */

struct Origin {
    char scheme[8];
    char host[128];
    int port;
};

/* ASCII-only, case-insensitive prefix test on a possibly non-NUL buffer. */
static int prefix_ci_n(const char *s, size_t n, const char *p)
{
    size_t i;
    for (i = 0; p[i] != '\0'; i++) {
        char a;
        char b;
        if (i >= n) {
            return 0;
        }
        a = s[i];
        b = p[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static void origin_parse(const char *url, struct Origin *o)
{
    const char *sep;
    const char *p;
    const char *slash;
    const char *hstart;
    const char *hend;
    size_t sl;
    size_t hl;
    size_t i;

    memset(o, 0, sizeof *o);
    sep = strstr(url, "://");
    if (sep == NULL) {
        return;
    }
    sl = (size_t)(sep - url);
    if (sl >= sizeof o->scheme) {
        sl = sizeof o->scheme - 1;
    }
    for (i = 0; i < sl; i++) {
        char ch = url[i];
        o->scheme[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
    }
    o->scheme[sl] = '\0';
    if (strcmp(o->scheme, "http") != 0 && strcmp(o->scheme, "https") != 0) {
        return;
    }

    p = sep + 3;
    slash = p;
    while (*slash != '\0' && *slash != '/' && *slash != '?' && *slash != '#') {
        slash++;
    }
    {
        const char *at = (const char *)memchr(p, '@', (size_t)(slash - p));
        if (at != NULL) {
            p = at + 1;
        }
    }
    hstart = p;
    if (*p == '[') {
        const char *rb = strchr(p, ']');
        if (rb == NULL) {
            return;
        }
        hstart = p + 1;
        hend = rb;
    } else {
        hend = p;
        while (*hend != '\0' && *hend != ':' && *hend != '/'
               && *hend != '?' && *hend != '#') {
            hend++;
        }
    }
    hl = (size_t)(hend - hstart);
    if (hl == 0 || hl >= sizeof o->host) {
        return;
    }
    for (i = 0; i < hl; i++) {
        char ch = hstart[i];
        o->host[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
    }
    o->host[hl] = '\0';
    if (*hend == ':') {
        o->port = atoi(hend + 1);
    }
    if (o->port <= 0) {
        o->port = strcmp(o->scheme, "https") == 0 ? 443 : 80;
    }
}

/* Same scheme + host + port. Unknown URLs compare equal: do not strip blindly. */
static int origin_same(const char *a, const char *b)
{
    struct Origin oa;
    struct Origin ob;
    origin_parse(a, &oa);
    origin_parse(b, &ob);
    if (oa.host[0] == '\0' || ob.host[0] == '\0') {
        return 1;
    }
    return strcmp(oa.scheme, ob.scheme) == 0
           && strcmp(oa.host, ob.host) == 0 && oa.port == ob.port;
}

static int url_is_https(const char *url)
{
    return prefix_ci_n(url, strlen(url), "https://");
}

/* Absolute Location, or a path-absolute / path-relative one resolved on base. */
static int location_resolve(const char *base, const char *loc,
                            char *out, size_t cap)
{
    struct Origin o;
    int n;
    size_t ll = strlen(loc);

    if (ll == 0) {
        return 0;
    }
    if (prefix_ci_n(loc, ll, "http://") || prefix_ci_n(loc, ll, "https://")) {
        if (ll + 1 > cap) {
            return 0;
        }
        memcpy(out, loc, ll + 1);
        return 1;
    }
    origin_parse(base, &o);
    if (o.host[0] == '\0') {
        return 0;
    }
    if (loc[0] == '/') {
        n = snprintf(out, cap, "%s://%s:%d%s", o.scheme, o.host, o.port, loc);
    } else {
        const char *sep = strstr(base, "://");
        const char *path = sep != NULL ? sep + 3 : base;
        const char *last;
        while (*path != '\0' && *path != '/') {
            path++;
        }
        last = strrchr(path, '/');
        if (last == NULL) {
            n = snprintf(out, cap, "%s://%s:%d/%s", o.scheme, o.host, o.port, loc);
        } else {
            size_t dirlen = (size_t)(last - path + 1);
            n = snprintf(out, cap, "%s://%s:%d%.*s%s", o.scheme, o.host, o.port,
                         (int)dirlen, path, loc);
        }
    }
    return n > 0 && (size_t)n < cap;
}

/* Headers that must not cross an origin boundary. */
static int header_is_sensitive(const char *line)
{
    static const char *names[] = {
        "authorization:", "cookie:", "proxy-authorization:", "referer:"
    };
    size_t i;
    size_t n = strlen(line);
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (prefix_ci_n(line, n, names[i])) {
            return 1;
        }
    }
    return 0;
}

static struct curl_slist *headers_filtered(struct curl_slist *src)
{
    struct curl_slist *out = NULL;
    struct curl_slist *node;
    for (node = src; node != NULL; node = node->next) {
        if (!header_is_sensitive(node->data)) {
            out = curl_slist_append(out, node->data);
        }
    }
    return out;
}

/* Per-hop facts the header callback collects before the body starts. */
struct HopState {
    int status;
    int follow;
    int is_redirect;
    struct HopSink *sink;
    char location[1024];
};

static size_t header_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    struct HopState *h = (struct HopState *)ud;
    size_t n = size * nmemb;

    if (n >= 5 && memcmp(ptr, "HTTP/", 5) == 0) {
        size_t i = 0;
        while (i < n && ptr[i] != ' ') {
            i++;
        }
        h->status = i < n ? atoi(ptr + i + 1) : 0;
        return n;
    }
    if (n == 2 && ptr[0] == '\r' && ptr[1] == '\n') {
        /* Headers are complete; the body has not been written yet. */
        if (h->follow && h->location[0] != '\0'
            && (h->status == 301 || h->status == 302 || h->status == 303
                || h->status == 307 || h->status == 308)) {
            h->is_redirect = 1;
            /* Set before the body starts, so a stream never sees it. */
            if (h->sink != NULL) {
                h->sink->discard = 1;
            }
        }
        return n;
    }
    if (prefix_ci_n(ptr, n, "location:")) {
        const char *v = ptr + 9;
        size_t off = 9;
        size_t vl;
        while (off < n && (*v == ' ' || *v == '\t')) {
            v++;
            off++;
        }
        vl = n - off;
        while (vl > 0 && (v[vl - 1] == '\r' || v[vl - 1] == '\n'
                          || v[vl - 1] == ' ' || v[vl - 1] == '\t')) {
            vl--;
        }
        if (vl >= sizeof h->location) {
            vl = sizeof h->location - 1;
        }
        memcpy(h->location, v, vl);
        h->location[vl] = '\0';
    }
    return n;
}

static struct HttpResponse *perform_request(struct HttpRequest *r, int streaming,
                                            HttpChunkCb cb, void *ud)
{
    struct HttpResponse *resp = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
    struct BodyBuf buf;
    struct XferState xfer;
    struct HopSink sink;
    CURL *easy;
    char current_url[2048];
    int hops = 0;
    int stripped = 0;
    int hop_body;
    int status = 0;
    int transport = 0;
    int local_redirect_error = 0;
    int too_many = 0;
    int64_t started_us;
    int64_t elapsed_us = 0;

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
    resp->cancelled = 0;
    resp->timeout_kind = HTTP_TIMEOUT_NONE;
    g_last_curl_code = 0;

    if (r->local_error != 0 || r->easy == NULL) {
        int local = r->local_error != 0 ? r->local_error : HTTP_LOCAL_ERR_NO_HANDLE;
        fill_local_error(resp, r->url, local, streaming ? 0 : 1);
        return resp;
    }
    if (streaming && cb == NULL) {
        fill_local_error(resp, r->url, HTTP_LOCAL_ERR_ARG, 0);
        return resp;
    }
    if (streaming ? (r->mode == 1) : (r->mode == 2)) {
        fill_local_error(resp, r->url, HTTP_LOCAL_ERR_STATE, streaming ? 0 : 1);
        return resp;
    }
    r->mode = streaming ? 2 : 1;
    if (r->cancel_requested) {
        fill_cancel_error(resp, r->url, streaming ? 0 : 1);
        return resp;
    }
    if (strlen(r->url) >= sizeof current_url) {
        fill_local_error(resp, r->url, HTTP_LOCAL_ERR_ARG, streaming ? 0 : 1);
        return resp;
    }
    memcpy(current_url, r->url, strlen(r->url) + 1);

    buf.data = NULL;
    buf.len = 0;
    buf.cap = 0;
    buf.max = r->max_body_bytes > 0 ? (size_t)r->max_body_bytes : 0;
    buf.limit_fired = 0;

    xfer.last_us = now_us();
    xfer.idle_ms = r->idle_timeout_ms;
    xfer.first_byte = 0;
    xfer.idle_fired = 0;
    xfer.cancel_flag = &r->cancel_requested;
    xfer.cancel_fired = 0;
    xfer.aborted = 0;
    buf.xfer = &xfer;

    sink.streaming = streaming;
    sink.buf = &buf;
    sink.cb = cb;
    sink.ud = ud;
    sink.delivered = 0;
    sink.max = buf.max;
    sink.limit_fired = 0;
    sink.xfer = &xfer;
    sink.discard = 0;

    easy = (CURL *)r->easy;
    hop_body = r->body != NULL;
    started_us = now_us();
    /* Following is done here; never let libcurl follow behind our back. */
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, xfer_cb);
    curl_easy_setopt(easy, CURLOPT_XFERINFODATA, &xfer);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, header_cb);

    for (;;) {
        struct HopState hop;
        struct curl_slist *filtered = NULL;
        long code = 0;

        memset(&hop, 0, sizeof hop);
        hop.follow = r->follow_redirects != 0;
        hop.sink = &sink;
        sink.discard = 0;
        sink.limit_fired = 0;
        xfer.last_us = now_us();

        if (stripped) {
            filtered = headers_filtered((struct curl_slist *)r->headers_slist);
        }
        curl_easy_setopt(easy, CURLOPT_URL, current_url);
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER,
                         stripped ? filtered
                                  : (struct curl_slist *)r->headers_slist);
        curl_easy_setopt(easy, CURLOPT_HEADERDATA, &hop);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, hop_write_cb);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &sink);
        if (hop_body && r->body != NULL) {
            /* Size first, then copy: a NUL inside the body must survive. */
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)r->body_len);
            curl_easy_setopt(easy, CURLOPT_COPYPOSTFIELDS, r->body);
            curl_easy_setopt(easy, CURLOPT_HTTPGET, 0L);
        } else {
            curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        }

        if (g_injected_curl_code != 0) {
            transport = g_injected_curl_code; /* test seam: no transfer at all */
        } else {
            transport = (int)curl_easy_perform(easy);
        }
        g_last_curl_code = transport;
        elapsed_us = now_us() - started_us;

        if (g_injected_curl_code == 0) {
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
        }
        status = hop.status != 0 ? hop.status : (int)code;
        if (filtered != NULL) {
            curl_slist_free_all(filtered);
        }

        if (!hop.is_redirect) {
            break;
        }
        /* The fixed ceiling counts redirects, not requests (S-03). */
        if (hops >= HTTP_MAX_REDIRECTS) {
            too_many = 1;
            break;
        }
        {
            char next_url[2048];
            if (!location_resolve(current_url, hop.location, next_url,
                                  sizeof next_url)) {
                too_many = 1; /* unusable Location: stop like a runaway chain */
                break;
            }
            if (url_is_https(current_url)
                && prefix_ci_n(next_url, strlen(next_url), "http://")) {
                local_redirect_error = 1;
                break;
            }
            if (!origin_same(current_url, next_url)) {
                stripped = 1; /* sticky: credentials never reappear later */
            }
            /* libcurl's own POST rewrite: 301/302/303 become GET, 307/308 keep. */
            if (hop.status == 301 || hop.status == 302 || hop.status == 303) {
                hop_body = 0;
            }
            hops++;
            memcpy(current_url, next_url, strlen(next_url) + 1);
            body_reset(&buf);
        }
    }

    resp->status = status;
    if (local_redirect_error) {
        /* A policy rejection is not a completed response: keep status 0. */
        resp->status = 0;
        resp->error = HTTP_LOCAL_ERR_REDIRECT;
    } else if (too_many) {
        resp->error = (int)CURLE_TOO_MANY_REDIRECTS;
    } else {
        resp->error = transport;
    }

    /*
     * A bare 28 says "timed out", never which limit. Judge at the point the
     * transfer returns: if libcurl never completed the connection and the
     * configured connect limit has elapsed, the connect phase ran out; any
     * other 28 is the total limit. An idle abort arrives as 42 from our own
     * progress callback and is reported as 28 with kind IDLE, so the timeout
     * family stays one code.
     */
    if (xfer.cancel_fired) {
        resp->error = (int)CURLE_ABORTED_BY_CALLBACK;
        resp->cancelled = 1;
        resp->timeout_kind = HTTP_TIMEOUT_NONE;
    } else if (xfer.aborted) {
        resp->error = (int)CURLE_WRITE_ERROR;
    } else if (sink.limit_fired) {
        resp->error = HTTP_LOCAL_ERR_BODY_LIMIT;
        resp->timeout_kind = HTTP_TIMEOUT_NONE;
    } else if (xfer.idle_fired) {
        resp->error = (int)CURLE_OPERATION_TIMEDOUT;
        resp->timeout_kind = HTTP_TIMEOUT_IDLE;
    } else if (transport == (int)CURLE_OPERATION_TIMEDOUT) {
        curl_off_t connect_us = 0;
        if (g_injected_curl_code == 0) {
            curl_easy_getinfo(easy, CURLINFO_CONNECT_TIME_T, &connect_us);
        }
        if (connect_us == 0 && r->connect_timeout_ms > 0
            && elapsed_us >= (int64_t)r->connect_timeout_ms * 1000) {
            resp->timeout_kind = HTTP_TIMEOUT_CONNECT;
        } else {
            resp->timeout_kind = HTTP_TIMEOUT_TOTAL;
        }
    }

    if (streaming) {
        resp->body = NULL;
        resp->body_len = 0;
        resp->total_bytes = (int)sink.delivered;
    } else {
        resp->body = buf.data != NULL ? buf.data : dup_string("");
        resp->body_len = (int)buf.len;
        /* total_bytes is the write-callback total, not a getinfo figure. */
        resp->total_bytes = (int)buf.len;
    }
    if (resp->error != 0) {
        resp->error_msg = make_error_msg(r->url, resp->error);
        resp->error_detail = make_error_detail(
            r->url, resp->error, resp->timeout_kind,
            resp->error == HTTP_LOCAL_ERR_BODY_LIMIT ? (int)buf.max : 0);
    }
    return resp;
}

static struct HttpResponse *real_send(struct HttpRequest *r)
{
    return perform_request(r, 0, NULL, NULL);
}

static struct HttpResponse *real_send_stream(struct HttpRequest *r,
                                             HttpChunkCb cb,
                                             void *ud)
{
    return perform_request(r, 1, cb, ud);
}

/* Dispatch through the struct field: no vtable lookup, one indirect call. */
struct HttpResponse *http_request_send(struct HttpRequest *r)
{
    const struct HttpRequestOps *ops = r->ops != NULL ? r->ops : &http_default_ops;
    return ops->send(r);
}

struct HttpResponse *http_request_send_stream(struct HttpRequest *r,
                                              HttpChunkCb cb,
                                              void *ud)
{
    if (r == NULL) {
        return NULL;
    }
    return real_send_stream(r, cb, ud);
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
    /* The cancellation term is what makes the two predicates mutually
       exclusive: nothing else can flip is_ok on a 200 with error 0. */
    return r->status >= 200 && r->status < 300 && r->error == 0
           && !r->cancelled;
}

int http_response_is_cancelled(struct HttpResponse *r) { return r->cancelled != 0; }
int http_response_timeout_kind(struct HttpResponse *r) { return r->timeout_kind; }

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
