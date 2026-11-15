# 09 三种超时

- applies-to: 0.2.0

`timeout_kind()` 是纯查询，不额外分配；成功路径恒为 `0`：

<!-- snippet: docs/snippets/timeout.ct -->
```text
/*
 * docs/snippets/timeout.ct - one CURLcode(28), three shim-side kinds.
 */
import http::http;

#include <stdio.h>

int main(void)
{
    CRequest *req = "https://api.example.com/users"->post();
    req->timeout(3000);          /* total */
    req->connect_timeout(1000);  /* connect */
    req->idle_timeout(1500);     /* data gap */

    CResponse *res = req->send();

    if (res->error() == 28) {                 /* CURLE_OPERATION_TIMEDOUT */
        switch (res->timeout_kind()) {
        case 1:  printf("connect timeout\n"); break;
        case 2:  printf("total timeout\n");   break;
        case 3:  printf("idle timeout\n");    break;
        default: printf("timeout, kind unknown\n");
        }
    }

    res->free();
    req->free();
    return 0;
}
```

| `timeout_kind()` | 含义 | 判定依据 |
| --- | --- | --- |
| `0` | 非超时（成功或别的错误） | 恒为 0；不依赖有没有超时配置 |
| `1` | 连接超时 | `transport==28` 且 `CURLINFO_CONNECT_TIME_T==0`，并且配置了 `connect_timeout` |
| `2` | 总时长超时 | `transport==28` 且不是连接超时（含首字节前就耗尽总时长） |
| `3` | 空闲超时 | 进度回调发现空档超过 `idle_timeout`，中止后把 `42` 归一成 `28` |

- **判定时点**：全部在 `curl_easy_perform` 返回之后一次算清，不在传输中途猜。
- **精度**：连接 / 总时长是 libcurl 自己的计时，亚毫秒级；空闲是进度回调粒度，
  实测约 1 秒一次，因此 `idle_timeout(500)` 实际在 ~1.0–1.1 秒中止。
  **配小于 1 秒的值不保证按值中止**。
- **没有独立低速下限**：未设 `idle_timeout` 时，持续有数据但整体极慢的流不会被中止。
- **合法慢流不会被误杀**：夹具 `p6-slow` 每 200 ms 一字节、`idle_timeout(500)`，
  结果 `status 200 body_len 5 kind 0 ok 1`。
- **负值**：`idle_timeout(ms<0)` 与 `timeout(ms<0)` 一样是本地 `-1005`，请求不离开进程。
- **全部真实网络**：本主题夹具走本地回环或 TEST-NET-1，没有注入 `CURLcode`。
