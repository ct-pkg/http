# 流式响应与响应体上限（phase 7）

- applies-to: 0.2.1
- 版本：0.1.0
- 对应判据：F-18、F-03；卡片 [docs/phases/PH-7/card.md](phases/PH-7/card.md)
- 本文补充 [README.md](../README.md) 的「流式（phase 7）」，不重复错误面与超时面。

## 1. 调用示例

```text
import http::http;
#include <stdio.h>

int on_chunk(char *data, int len, void *ud)
{
    int *total = (int *)ud;
    *total += len;
    return 0;              /* 非 0 = 中止传输 */
}

int main(void)
{
    CRequest *req = "https://api.example.com/report"->post();
    req->timeout(10000);
    req->idle_timeout(2000);
    req->max_body_bytes(1 << 20);

    int total = 0;
    CResponse *res = req->send_stream(on_chunk, &total);

    printf("status=%d error=%d total=%d\n",
           res->status(), res->error(), res->total_bytes());

    res->free();
    req->free();
    return 0;
}
```

`send_stream` 返回的响应只带状态、错误与计数；`body()` 恒为 `NULL`，`body_len()` 恒为 `0`，
`take_body()` 返回 `NULL`。回调收到的 `data` 是**借用**的，只在本次调用内有效；要跨回调
保留必须自行 `malloc`/`memcpy`，库不会释放调用方的内存。

## 2. 回调中止

- 回调返回非 0：写回调返回 0，libcurl 报 `CURLE_WRITE_ERROR(23)`。
- 对外 `error()==23`、`is_cancelled()==0`；这是“回调要求停”，不是用户取消。
- 触发中止的那一片已经交给回调，计入 `total_bytes()`；`body_len()` 仍为 `0`。
- 响应与请求随后正常 `free()`；ASAN 正例 `p7-chunks`、`p7-limit` 证明没有二次释放或泄漏。

## 3. `total_bytes` 的最终口径

`total_bytes()` **只数实际交付的响应体字节**：非流式是写回调写入内置缓冲的字节，
流式是交给用户回调的字节。它不含响应头、chunked 框架、TLS 记录，也不含“服务器原本
还会发”的字节。

- 非流式成功：`body_len() == total_bytes()`。
- 流式成功：`body_len() == 0`，`total_bytes()` 是回调累计。
- 回调中止：`total_bytes()` 包含触发中止的那一片。
- 上限命中：`total_bytes() == max_body_bytes()`，不会更少，也不会更多。

这条口径与 `max_body_bytes` 绑定：库在累计到上限时**立即中断**，所以不可能同时报告
“截断前的总量”。选择的是“到上限即中止并报错”，不是“继续读完再只保留前 N 字节”。

## 4. `max_body_bytes(n)` 的行为

| 输入 | 结果 |
| --- | --- |
| `n == 0` | 不限（默认） |
| `n < 0` | 本地 `-1001`（`HTTP_LOCAL_ERR_ARG`），请求不离开进程 |
| 响应体正好 `n` 字节 | 成功，`body_len == total_bytes == n` |
| 响应体超过 `n` | 在 `n` 处中止，`error()==-1008`（`HTTP_LOCAL_ERR_BODY_LIMIT`） |
| 非流式超限 | 保留前 `n` 字节，`body_len == n`，`take_body()` 可取走这前 `n` 字节 |
| 流式超限 | `body_len == 0`，前 `n` 字节已交给回调，`total_bytes == n`，`take_body()==NULL` |

`error_detail()` 在超限时形如 `code=-1008 stage=body-limit limit=1024 host=…`；`error_msg()`
是 `body-limit: host (code -1008)`。上限只作用于响应体，不限制请求体。

`-1008` 是追加的本地码，不改 `-1001…-1007` 的任何含义，也不引入 `HTTP_ERR_*` 枚举。

## 5. 请求体与流式响应

请求体仍由 `body(data, len)` 显式给长度；`send_stream` 只是把**响应**改成回调交付。
`POSTFIELDSIZE` 在 `COPYPOSTFIELDS` 之前设置，所以 `"a\0b"` 按 3 字节发送，不会被
`strlen` 截成 1 字节。夹具 `p7-body-nul` 用 `/echo` 回显这 3 字节并逐字节比较。

同一请求对象上：

- `send()` 与 `send_stream()` 只能选一种；混用第二次读 `-1007`
  （`HTTP_LOCAL_ERR_STATE`），不发起连接。
- `send_stream(r, NULL, ud)` 读 `-1001`，同样不发起连接。
- 请求体 + 流式响应可以共存，这不是模式冲突。

## 6. 没有 `Content-Length` 的响应

chunked 或连接关闭即结束的响应由 libcurl 解码，写回调看到的是解码后的 body 字节。
`/chunked` 夹具发 `Transfer-Encoding: chunked`、三个分块、最后 `0\r\n\r\n` 并关闭；
`p7-chunked` 收到 `alphabetagamma`（14 字节），`status 200`、`error 0`，不等待一个
永远不会出现的长度。

## 7. 与超时叠加

流式读中 `idle_timeout` 仍然生效：

- 中止后对外 `error()==28`、`timeout_kind()==3`。
- 已交付给回调的字节留在调用方；响应不持有 body，`body_len()==0`、`take_body()==NULL`。
- `total_bytes()` 等于已交付字节，不是服务器声明的总长度。
- 非流式同一场景保留已收前缀，`body_len()==total_bytes()==已收字节`，`take_body()` 可取。

`idle_timeout` 的精度约 1 秒（见 README 的「超时（phase 6）」）；放弃
`LOW_SPEED_LIMIT` 后没有独立的低速下限，持续有数据但整体极慢的流只在设置
`idle_timeout` 时受约束。

## 8. 所有权小结

| 对象 | 归属 | 释放方 |
| --- | --- | --- |
| 回调里的 `data` | 库/网络栈临时缓冲 | 库；回调不得 `free` |
| 调用方保留的副本 | 调用方 | 调用方 |
| 非流式 `body()` | 响应 | `http_response_free()` 或 `take_body()` 之后由调用方 |
| 流式响应 `body()` | 不存在（`NULL`） | 无需释放 |
| 超限时非流式前缀 | 响应 | 同 `body()` |
| `error_msg()` / `error_detail()` | 响应 | `http_response_free()` |

ASAN 覆盖：`p7-chunks`（回调复制 1 MiB 后释放自己的缓冲）、`p7-limit`（`take_body`
取走 1024 字节后释放）。`p7-idle` 的两条路径由普通构建验证逻辑，所有权路径与上述
两个 ASAN 用例同源。

## 9. 取消与流式叠加（phase 8）

`req->cancel()` 置一个粘性标志，流式路径在**每次写回调交付字节之前**与**每次进度回调**
读取它；命中即中止，对外 `error()==42`、`is_cancelled()==1`、`is_ok()==0`、
`timeout_kind()==0`。

- 与回调中止的区别：回调返回非 0 是 `23`、`is_cancelled()==0`。写回调先查取消标志
  再调用用户回调，所以同一片既取消又返回非 0 时记作取消。
- 与 `idle_timeout` 叠加：进度回调先查取消、再判空档；两者同时满足时记作取消
  （`timeout_kind()==0`，不是 `3`）。
- 已交付给回调的片留在调用方，`total_bytes()` 是已交付字节；中止那一片不交付、
  不计数。响应不持有 body：`body_len()==0`、`take_body()==NULL`。
- 连接前取消：不发起连接，`status()==0`、`error()==42`、`is_cancelled()==1`，
  服务端计数为 0（`p8-chain`）。
- 精度：连续有数据的流在下一次写回调即停；停顿流等下一次进度回调，约 1 秒。
  DNS 解析阶段不可取消。
- 同线程：标志是普通 `int`，`cancel()` 不得与 `free()` 竞争；跨线程取消不在 0.1.0
  范围（S-04 裁决）。

夹具：`p8-cancel`（回调内取消）、`p8-not-cancelled`（未取消两入口）、
`p8-lifetime` / `p8-lifetime-bad`（send 后请求仍有效；负例证明 ASAN 会拒绝 UAF）。
