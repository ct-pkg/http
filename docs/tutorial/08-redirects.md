# 08 重定向与安全策略

- applies-to: 0.2.1

默认不跟随：`follow_redirects(0)` 时 3xx 原样返回（`status()==302`、`error()==0`、
`is_ok()==0`），只发 1 个请求。

`follow_redirects(1)` 后由 shim 自己逐跳跟随（`CURLOPT_FOLLOWLOCATION` 始终为 0）：

<!-- snippet: docs/snippets/redirect.ct -->
```text
/*
 * docs/snippets/redirect.ct - the fixed 10-redirect ceiling and the downgrade
 * refusal are visible through error().
 */
import http::http;

#include <stdio.h>

int main(void)
{
    CRequest *req = "https://api.example.com/v1/items"->post();
    req->timeout(10000);
    req->follow_redirects(1);
    CResponse *res = req->send();
    if (res->error() == 47) {
        printf("too many redirects (limit 10)\n");
    } else if (res->error() == -1009) {
        printf("refused https -> http downgrade\n");
    }
    res->free();
    req->free();
    return 0;
}
```

- **上限固定 10 次重定向，不可配置**（S-03；裁决见 `docs/reviews/PH-9/S-03-ruling.md`）。
  第 11 个 3xx 不再跟随：`error()==47`、`status()` 为最后一个 3xx。
- **计数单位是重定向次数，不是请求数**：10 个 302 的链成功，共 11 个请求；
  11 个 302 时第 11 跳的目标不会被请求，总请求仍是 11。
- **方法改写与 libcurl 一致**：`301`/`302`/`303` 把 POST 改成 GET 并丢弃请求体；
  `307`/`308` 保留方法与请求体。
- **中间 3xx 的 body 丢弃**：只有最终响应进入 `body()`；`send_stream` 不会把中间
  跳转的 body 交给回调。
- **跨源剥离**：scheme / host / port 任一变化即视为跨源，下一跳不发送
  `Authorization`、`Cookie`、`Proxy-Authorization`、`Referer`；一旦跨过源，
  后续跳也不再带回去。同源保留。
- **拒绝降级**：`https → http` 在发出下一跳前本地拒绝，`error()==-1009`
  （`HTTP_LOCAL_ERR_REDIRECT`）、`status()==0`、`error_detail` 带 `stage=redirect`，
  明文服务端收不到请求。`http → https` 允许。
- **循环**不单独检测，由 10 跳上限兜底：A→B→A 在 11 个请求内以 `47` 结束。
- **夹具**：`p9-same-host`、`p9-hops10`、`p9-hops11`、`p9-cross-host`、`p9-loop`、
  `p9-downgrade`（真实 TLS 起点）、`p9-default`。
