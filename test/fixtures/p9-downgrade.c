#include "http.shim.h"
#include "p9-server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int start_tls(const char *script, const char *certdir, const char *target,
                     int *port_out, pid_t *pid_out)
{
    int fds[2];
    pid_t pid;
    char buf[128];
    ssize_t n;
    int port = 0;

    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        execlp("python3", "python3", script, "0", certdir, target, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    n = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);
    if (n <= 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return -1;
    }
    buf[n] = '\0';
    if (sscanf(buf, "PORT %d", &port) != 1 || port <= 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return -1;
    }
    *port_out = port;
    *pid_out = pid;
    return 0;
}

/* https origin redirects to plain http: the follow must be refused locally. */
int main(int argc, char **argv)
{
    struct P9Server *plain;
    char target[192];
    char url[96];
    int tls_port = 0;
    pid_t tls_pid = -1;
    struct HttpRequest *req;
    struct HttpResponse *res;

    if (argc < 3) {
        printf("usage: p9-downgrade <certdir> <tls-script>\n");
        return 2;
    }
    plain = p9_server_start("127.0.0.1");
    if (plain == NULL) {
        printf("server failed\n");
        return 1;
    }
    snprintf(target, sizeof target, "%s/final", p9_server_base(plain));
    if (start_tls(argv[2], argv[1], target, &tls_port, &tls_pid) != 0) {
        printf("tls server failed\n");
        p9_server_stop(plain);
        return 1;
    }

    snprintf(url, sizeof url, "https://127.0.0.1:%d/", tls_port);
    req = http_request_new(url);
    http_request_timeout(req, 10000);
    http_request_insecure(req, 1);
    http_request_follow_redirects(req, 1);
    res = http_request_send(req);

    printf("error %d status %d plaintext %d\n",
           http_response_error(res), http_response_status(res),
           p9_server_requests(plain));

    http_response_free(&res);
    http_request_free(&req);
    kill(tls_pid, SIGTERM);
    waitpid(tls_pid, NULL, 0);
    p9_server_stop(plain);
    return 0;
}
