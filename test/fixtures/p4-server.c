#include "p4-server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#define P4_MAP_ANON MAP_ANON
#else
#define P4_MAP_ANON MAP_ANONYMOUS
#endif

static pid_t g_server_pid = -1;
static int *g_requests = NULL;
static char g_base[96];
static char g_url[192];

static int starts_with_ci(const char *s, const char *prefix)
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

static int header_end(const char *buf, size_t len)
{
    size_t i;
    for (i = 3; i < len; i++) {
        if (buf[i - 3] == '\r' && buf[i - 2] == '\n'
            && buf[i - 1] == '\r' && buf[i] == '\n') {
            return (int)(i + 1);
        }
    }
    return -1;
}

static int content_length(const char *buf, size_t len)
{
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j + 1 < len && !(buf[j] == '\r' && buf[j + 1] == '\n')) {
            j++;
        }
        if (starts_with_ci(buf + i, "content-length:")) {
            const char *v = buf + i + 15;
            while (*v == ' ') {
                v++;
            }
            return atoi(v);
        }
        i = j + 2;
    }
    return 0;
}

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

static void route(int fd, const char *path)
{
    if (strcmp(path, "/binary") == 0) {
        static const char head[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\n";
        static const char body[3] = { 'a', '\0', 'b' };
        send_all(fd, head, sizeof head - 1);
        send_all(fd, body, sizeof body);
    } else if (strcmp(path, "/split") == 0) {
        static const char head[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n\r\n";
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 150000;
        send_all(fd, head, sizeof head - 1);
        send_all(fd, "{\"use", 5);
        select(0, NULL, NULL, NULL, &tv);
        send_all(fd, "rs\":3}", 6);
    } else if (strcmp(path, "/hang") == 0) {
        static const char resp[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
        sleep(5);
        send_all(fd, resp, sizeof resp - 1);
    } else if (strcmp(path, "/404") == 0) {
        static const char resp[] =
            "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nnot found";
        send_all(fd, resp, sizeof resp - 1);
    } else if (strcmp(path, "/500") == 0) {
        static const char resp[] =
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 4\r\nConnection: close\r\n\r\nboom";
        send_all(fd, resp, sizeof resp - 1);
    } else {
        static const char resp[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{\"users\":3}";
        send_all(fd, resp, sizeof resp - 1);
    }
}

static void serve(int fd, int *count)
{
    char buf[8192];
    size_t len = 0;
    char path[256] = "/";

    for (;;) {
        int he = header_end(buf, len);
        if (he >= 0) {
            int want = content_length(buf, (size_t)he);
            if (len >= (size_t)he + (size_t)want) {
                break;
            }
        }
        if (len == sizeof buf) {
            break;
        }
        ssize_t n = read(fd, buf + len, sizeof buf - len);
        if (n <= 0) {
            break;
        }
        len += (size_t)n;
    }

    const char *sp = (const char *)memchr(buf, ' ', len);
    if (sp != NULL) {
        const char *p2 = (const char *)memchr(sp + 1, ' ',
                                              (size_t)(buf + len - (sp + 1)));
        if (p2 != NULL) {
            size_t plen = (size_t)(p2 - (sp + 1));
            if (plen < sizeof path) {
                memcpy(path, sp + 1, plen);
                path[plen] = '\0';
            }
        }
    }

    *count = *count + 1;
    route(fd, path);
}

int p4_server_start(void)
{
    if (g_server_pid > 0) {
        return 0;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    socklen_t alen = sizeof addr;
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) != 0) {
        close(fd);
        return -1;
    }

    int *count = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                             P4_MAP_ANON | MAP_SHARED, -1, 0);
    if (count == MAP_FAILED) {
        close(fd);
        return -1;
    }
    *count = 0;

    atexit(p4_server_stop);

    pid_t pid = fork();
    if (pid < 0) {
        close(fd);
        return -1;
    }
    if (pid == 0) {
        signal(SIGPIPE, SIG_IGN);
        for (;;) {
            int c = accept(fd, NULL, NULL);
            if (c < 0) {
                break;
            }
            serve(c, count);
            close(c);
        }
        close(fd);
        _exit(0);
    }

    close(fd);
    g_server_pid = pid;
    g_requests = count;
    snprintf(g_base, sizeof g_base, "http://127.0.0.1:%d",
             (int)ntohs(addr.sin_port));
    return 0;
}

void p4_server_stop(void)
{
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGTERM);
        waitpid(g_server_pid, NULL, 0);
        g_server_pid = -1;
    }
}

char *p4_server_url(const char *path)
{
    snprintf(g_url, sizeof g_url, "%s%s", g_base, path != NULL ? path : "/");
    return g_url;
}

int p4_server_requests(void)
{
    return g_requests != NULL ? *g_requests : 0;
}
