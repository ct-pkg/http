# 07 取消与生命周期

- applies-to: 0.2.0

取消只在同一线程内生效，标志是普通 `int`，不做 atomic：

<!-- snippet: docs/snippets/cancel.ct -->
```text
/*
 * docs/snippets/cancel.ct - same-thread cancellation from inside the callback.
 */
import http::http;

#include <stdio.h>

CRequest *g_req;

int on_chunk(char *data, int len, void *ud)
{
    (void)data; (void)ud;
    if (len > 0) {
        g_req->cancel();     /* set the flag, do not abort this callback */
    }
    return 0;                /* a non-zero return is a callback abort (23) */
}

int main(void)
{
    g_req = "https://api.example.com/report"->post();
    g_req->timeout(10000);
    CResponse *res = g_req->send_stream(on_chunk, NULL);

    printf("error=%d cancelled=%d ok=%d\n",
           res->error(), res->is_cancelled(), res->is_ok());

    res->free();
    g_req->free();
    return 0;
}
```

- **判定点三处**：传输开始前、每次写回调交付字节前、每次进度回调（约 1 秒一次）。
  连续有数据的流在下一次写回调就停；停顿流要等下一次进度回调，约 1 秒。
  **DNS 解析阶段不可取消**：解析返回前观察不到标志。
- **对外形态**：取消中止时 `error()==42`（`CURLE_ABORTED_BY_CALLBACK`）、
  `is_cancelled()==1`、`is_ok()==0`、`timeout_kind()==0`。
- **优先级**：取消 `42` > 回调中止 `23` > 上限 `-1008` > 空闲 `28/3` > 传输码。
- **粘性标志**：`cancel()` 之后该请求不再发起新传输（连接前就返回 `42`，服务端计数 0，
  `status()` 为 0）。要重试请新建请求对象。
- **跨线程取消不在 0.1.0**（S-04 裁决：`docs/reviews/PH-8/S-04-ruling.md`）。
- **部分数据归属**：非流式保留已收前缀，`body_len()==total_bytes()`，`take_body()` 可取；
  流式已交付片归调用方，`body()==NULL`、`body_len()==0`、`take_body()==NULL`。
- **显式释放契约**：`send()` / `send_stream()` 从不释放请求，也不调用
  `curl_easy_cleanup`；`send()` 返回后请求字段仍可读。调用方按
  `res->free(); req->free();` 各释放一次。
- **夹具**：`p8-cancel`、`p8-not-cancelled`、`p8-lifetime`（ASAN 正例）、
  `p8-lifetime-bad`（ASAN 负例）、`p8-chain`。
