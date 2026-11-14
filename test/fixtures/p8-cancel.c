#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <sys/time.h>

/*
 * Cancel from inside the chunk callback. The callback does NOT return non-zero:
 * cancel() is a flag, and the transfer must stop at the next write or progress
 * tick. A correct run reports 42 with is_cancelled==1, and the chunk that was
 * withheld (the one after the cancel) never reaches the callback.
 */
static struct HttpRequest *g_req;
static int g_chunks;

static int on_chunk(char *data, int len, void *ud)
{
    (void)data;
    (void)ud;
    if (len > 0) {
        g_chunks = g_chunks + 1;
        http_request_cancel(g_req);
    }
    return 0;
}

static long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    g_req = http_request_new(p4_server_url("/slow4"));
    http_request_timeout(g_req, 10000);

    long started = now_ms();
    struct HttpResponse *res = http_request_send_stream(g_req, on_chunk, NULL);
    long elapsed = now_ms() - started;

    printf("error %d cancelled %d ok %d chunks %d total %d status %d lt3 %d\n",
           http_response_error(res),
           http_response_is_cancelled(res),
           http_response_is_ok(res),
           g_chunks,
           http_response_total_bytes(res),
           http_response_status(res),
           elapsed < 3000);

    http_response_free(&res);
    http_request_free(&g_req);
    p4_server_stop();
    return 0;
}
