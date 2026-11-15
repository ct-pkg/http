# 06 流式与慢流保护

- applies-to: 0.2.0

`send_stream(cb, ud)` 按到达顺序交付响应体，不回填 `body()`：

<!-- snippet: docs/snippets/stream.ct -->
```text
/*
 * docs/snippets/stream.ct - bytes arrive through the callback, not body().
 */
import http::http;

#include <stdio.h>

struct Sink {
    int chunks;
    int bytes;
};

int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    (void)data;
    if (len > 0) {
        s->chunks += 1;
        s->bytes += len;
    }
    return 0;
}

int main(void)
{
    struct Sink sink = { 0, 0 };
    CRequest *req = "https://api.example.com/report"->post();
    CResponse *res = req->max_body_bytes(1048576)->send_stream(on_chunk, &sink);

    printf("status=%d chunks=%d bytes=%d body_null=%d\n",
           res->status(), sink.chunks, sink.bytes, res->body() == NULL);

    res->free();
    req->free();
    return 0;
}
```

- 回调拿到的 `data` 只在本次调用内有效；跨回调保留必须自行复制，库不释放调用方内存。
- 返回非 0 → `error()==23`、`is_cancelled()==0`；无回调读 `-1001`，混用
  `send`/`send_stream` 读 `-1007`。
- `max_body_bytes(n)`：达到 `n` 立即中止并报 `-1008`；`total_bytes()` 等于 `n`，
  流式已交付前缀归回调。
- 请求体与流式响应可共存；chunked / 无 `Content-Length` 由 libcurl 解码后交付。
- 流式读中 `idle_timeout` 生效：中止读 `28/3`，已交付字节归回调，`take_body()` 为 `NULL`。
- 回调内可以直接 `req->cancel()` 取消（与返回非 0 不同），见
  [07-cancel.md](07-cancel.md)。
- 完整的所有权说明与更多夹具见 [docs/streaming.md](../streaming.md)。
