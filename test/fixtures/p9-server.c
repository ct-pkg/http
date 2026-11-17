/* Strict ISO mode (-std=c99) hides MAP_ANONYMOUS on glibc. macOS keeps its
 * default namespace: the macros would hide MAP_ANON / INADDR_LOOPBACK. */
#ifndef __APPLE__
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "p9-server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#define P9_MAP_ANON MAP_ANON
#else
#define P9_MAP_ANON MAP_ANONYMOUS
#endif

#define P9_MAX_HOPS 32

struct P9Shared {
    int total;
    int final_hits;
    int hops[P9_MAX_HOPS];
    int chain;
    char peer[192];
};

struct P9Server {
    pid_t pid;
    int port;
    struct P9Shared *shared;
    char base[96];
    char url[160];
};

static void send_all(int fd, const char *data, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, data, len);
        if (n <= 0) {
            return;
        }
        data += n;
        len -= (size_t)n;
    }
}

static void send_status(int fd, int code, const char *location,
                        const char *body, size_t body_len)
{
    char head[512];
    int n;
    if (location != NULL) {
        n = snprintf(head, sizeof head,
                     "HTTP/1.1 %d Moved\r\nLocation: %s\r\n"
                     "Content-Length: 0\r\nConnection: close\r\n\r\n",
                     code, location);
        send_all(fd, head, (size_t)n);
        return;
    }
    n = snprintf(head, sizeof head,
                 "HTTP/1.1 %d OK\r\nContent-Length: %d\r\n"
                 "Connection: close\r\n\r\n",
                 code, (int)body_len);
    send_all(fd, head, (size_t)n);
    if (body_len > 0) {
        send_all(fd, body, body_len);
    }
}

static void handle(int fd, struct P9Shared *sh, const char *self_base)
{
    char req[8192];
    ssize_t n = read(fd, req, sizeof req - 1);
    char loc[320];
    int hop;

    if (n <= 0) {
        return;
    }
    req[n] = '\0';
    sh->total = sh->total + 1;

    if (strncmp(req, "GET ", 4) != 0 && strncmp(req, "POST ", 5) != 0) {
        send_status(fd, 400, NULL, "bad", 3);
        return;
    }

    if (sscanf(req, "%*s /hop/%d", &hop) == 1 && hop >= 0 && hop < P9_MAX_HOPS) {
        sh->hops[hop] = sh->hops[hop] + 1;
        if (hop < sh->chain) {
            snprintf(loc, sizeof loc, "/hop/%d", hop + 1);
            send_status(fd, 302, loc, NULL, 0);
        } else {
            send_status(fd, 200, NULL, "done", 4);
        }
        return;
    }

    if (sscanf(req, "%*s /redirect/%d", &hop) == 1) {
        if (hop > 1) {
            snprintf(loc, sizeof loc, "/redirect/%d", hop - 1);
        } else {
            snprintf(loc, sizeof loc, "/final");
        }
        send_status(fd, 302, loc, NULL, 0);
        return;
    }

    if (strstr(req, " /final ") != NULL) {
        sh->final_hits = sh->final_hits + 1;
        send_status(fd, 200, NULL, "done", 4);
        return;
    }

    if (strstr(req, " /loop/a ") != NULL) {
        send_status(fd, 302, "/loop/b", NULL, 0);
        return;
    }
    if (strstr(req, " /loop/b ") != NULL) {
        send_status(fd, 302, "/loop/a", NULL, 0);
        return;
    }

    if (strstr(req, " /to-localhost ") != NULL) {
        /* Same port, different host name: exercises the host half of origin. */
        const char *colon = strrchr(self_base, ':');
        snprintf(loc, sizeof loc, "http://localhost:%s/echo-headers",
                 colon != NULL ? colon + 1 : "80");
        send_status(fd, 302, loc, NULL, 0);
        return;
    }

    if (strstr(req, " /to-peer ") != NULL) {
        if (sh->peer[0] == '\0') {
            send_status(fd, 500, NULL, "no-peer", 7);
            return;
        }
        snprintf(loc, sizeof loc, "%s/echo-headers", sh->peer);
        send_status(fd, 302, loc, NULL, 0);
        return;
    }

    if (strstr(req, " /echo-headers ") != NULL) {
        /* Echo the raw request bytes so the caller can look for header names. */
        send_status(fd, 200, NULL, req, strlen(req));
        return;
    }

    send_status(fd, 200, NULL, "{\"ok\":1}", 8);
    (void)self_base;
}

struct P9Server *p9_server_start(const char *bind_addr)
{
    struct P9Server *s;
    struct sockaddr_in addr;
    socklen_t alen;
    int fd;
    int one = 1;

    s = (struct P9Server *)calloc(1, sizeof *s);
    if (s == NULL) {
        return NULL;
    }
    s->shared = (struct P9Shared *)mmap(NULL, sizeof(struct P9Shared),
                                        PROT_READ | PROT_WRITE,
                                        P9_MAP_ANON | MAP_SHARED, -1, 0);
    if (s->shared == MAP_FAILED) {
        free(s);
        return NULL;
    }
    memset(s->shared, 0, sizeof(struct P9Shared));
    s->shared->chain = 10;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        free(s);
        return NULL;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        close(fd);
        free(s);
        return NULL;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        free(s);
        return NULL;
    }
    alen = sizeof addr;
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) != 0
        || listen(fd, 16) != 0) {
        close(fd);
        free(s);
        return NULL;
    }
    s->port = (int)ntohs(addr.sin_port);
    snprintf(s->base, sizeof s->base, "http://%s:%d", bind_addr, s->port);

    s->pid = fork();
    if (s->pid < 0) {
        close(fd);
        free(s);
        return NULL;
    }
    if (s->pid == 0) {
        signal(SIGPIPE, SIG_IGN);
        for (;;) {
            int c = accept(fd, NULL, NULL);
            if (c < 0) {
                break;
            }
            handle(c, s->shared, s->base);
            close(c);
        }
        close(fd);
        _exit(0);
    }
    close(fd);
    return s;
}

void p9_server_stop(struct P9Server *s)
{
    if (s == NULL) {
        return;
    }
    if (s->pid > 0) {
        kill(s->pid, SIGTERM);
        waitpid(s->pid, NULL, 0);
    }
    munmap(s->shared, sizeof(struct P9Shared));
    free(s);
}

const char *p9_server_base(struct P9Server *s)
{
    return s != NULL ? s->base : "";
}

const char *p9_server_url(struct P9Server *s, const char *path)
{
    if (s == NULL) {
        return "";
    }
    snprintf(s->url, sizeof s->url, "%s%s", s->base, path);
    return s->url;
}

void p9_server_set_chain(struct P9Server *s, int chain)
{
    if (s != NULL) {
        s->shared->chain = chain;
    }
}

void p9_server_set_peer(struct P9Server *s, const char *peer_base)
{
    if (s == NULL || peer_base == NULL) {
        return;
    }
    snprintf(s->shared->peer, sizeof s->shared->peer, "%s", peer_base);
}

int p9_server_requests(struct P9Server *s)
{
    return s != NULL ? s->shared->total : -1;
}

int p9_server_hop_requests(struct P9Server *s, int hop)
{
    if (s == NULL || hop < 0 || hop >= P9_MAX_HOPS) {
        return -1;
    }
    return s->shared->hops[hop];
}

int p9_server_final_requests(struct P9Server *s)
{
    return s != NULL ? s->shared->final_hits : -1;
}
