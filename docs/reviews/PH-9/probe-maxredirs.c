/*
 * PH-9 / S-03 probe: what does libcurl do when CURLOPT_MAXREDIRS is unset?
 *
 *   cc -std=c99 -Wall -Wextra -Werror docs/reviews/PH-9/probe-maxredirs.c \
 *      -lcurl -o /tmp/probe-maxredirs
 *   /tmp/probe-maxredirs 15
 *   /tmp/probe-maxredirs 60
 *
 * Serves /hop/0 .. /hop/<chain>; every hop below <chain> answers 302 with a
 * relative Location to the next one, the last answers 200. The child counts
 * every request it actually serves in shared memory, so the parent can prove
 * whether the request after redirect N was sent.
 */
#include <arpa/inet.h>
#include <curl/curl.h>
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
#define P_MAP_ANON MAP_ANON
#else
#define P_MAP_ANON MAP_ANONYMOUS
#endif

static int g_chain = 15;
static pid_t g_pid = -1;
static int *g_count;
static char g_base[96];

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

static void handle(int fd, int *count)
{
    char req[2048];
    ssize_t n = read(fd, req, sizeof req - 1);
    if (n <= 0) {
        return;
    }
    req[n] = '\0';

    int hop = -1;
    const char *p = strstr(req, "/hop/");
    if (p != NULL) {
        hop = atoi(p + 5);
    }
    *count = *count + 1;

    char head[256];
    if (hop >= 0 && hop < g_chain - 1) {
        int hn = snprintf(head, sizeof head,
                          "HTTP/1.1 302 Found\r\nLocation: /hop/%d\r\n"
                          "Content-Length: 0\r\nConnection: close\r\n\r\n",
                          hop + 1);
        send_all(fd, head, (size_t)hn);
    } else {
        static const char ok[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\ndone";
        send_all(fd, ok, sizeof ok - 1);
    }
}

static int server_start(void)
{
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
    if (listen(fd, 16) != 0) {
        close(fd);
        return -1;
    }

    int *count = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                             P_MAP_ANON | MAP_SHARED, -1, 0);
    if (count == MAP_FAILED) {
        close(fd);
        return -1;
    }
    *count = 0;

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
            handle(c, count);
            close(c);
        }
        close(fd);
        _exit(0);
    }

    close(fd);
    g_pid = pid;
    g_count = count;
    snprintf(g_base, sizeof g_base, "http://127.0.0.1:%d",
             (int)ntohs(addr.sin_port));
    return 0;
}

static void server_stop(void)
{
    if (g_pid > 0) {
        kill(g_pid, SIGTERM);
        waitpid(g_pid, NULL, 0);
        g_pid = -1;
    }
}

static size_t sink(char *ptr, size_t size, size_t nmemb, void *ud)
{
    (void)ptr;
    (void)ud;
    return size * nmemb;
}

static void run(const char *label, long maxredirs)
{
    CURL *e = curl_easy_init();
    char url[160];
    snprintf(url, sizeof url, "%s/hop/0", g_base);
    *g_count = 0;

    curl_easy_setopt(e, CURLOPT_URL, url);
    curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, sink);
    if (maxredirs > 0) {
        curl_easy_setopt(e, CURLOPT_MAXREDIRS, maxredirs);
    }

    CURLcode rc = curl_easy_perform(e);

    long code = 0;
    long redirs = 0;
    curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_getinfo(e, CURLINFO_REDIRECT_COUNT, &redirs);
    printf("%s rc=%d code=%ld redirects=%ld requests=%d\n",
           label, (int)rc, code, redirs, *g_count);
    fflush(stdout);
    curl_easy_cleanup(e);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        g_chain = atoi(argv[1]);
    }
    if (g_chain < 2) {
        printf("chain must be >= 2\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    printf("libcurl %s chain=%d\n", curl_version(), g_chain);
    if (server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    run("default", 0);
    run("maxredirs=10", 10);
    run("maxredirs=12", 12);
    run("default_again", 0);

    server_stop();
    curl_global_cleanup();
    return 0;
}
