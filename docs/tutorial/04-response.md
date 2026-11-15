# 04 响应面

- applies-to: 0.1.0

响应对象只读，释放仍是显式的：

| 方法 | 作用 |
| --- | --- |
| `status()` | HTTP 状态码；传输失败时为 `0` |
| `body()` | 响应体首地址；NUL 结尾，长度只认 `body_len()`；流式恒为 `NULL` |
| `body_len()` | 响应体字节数，内嵌 NUL 不截断；流式恒为 `0` |
| `total_bytes()` | 实际交付给缓冲或回调的响应体字节数；上限命中时等于上限 |
| `error()` | 0 = 无错；正数 = `CURLcode` 原值；负数 = 本地拒绝 |
| `error_msg()` | `"<阶段>: <主机> (code N)"`，无错时为空串 |
| `error_detail()` | `"code=N stage=S host=H"`，同一输入可复现；超时多 `phase=`，上限多 `limit=` |
| `is_ok()` | 2xx 且 `error()==0` 且未取消 |
| `is_cancelled()` | 取消位；只有 `cancel()` 真正中断传输才为 `1`，此时 `error()==42` |
| `take_body()` | 取走 body：响应侧置空、`body_len()` 归零，调用方负责 `free` 一次；流式返回 `NULL` |
| `free()` | 释放响应；`take_body()` 之后不再持有 body |

- **`take_body` 是所有权转移，不是借用**：取走后原响应的 `free()` 不会重复释放
  （ASAN 正例 `p4-take-body`，负例 `p4-take-body-bad` 证明断言不是空转）。
- **响应头读取不在 0.1.0**：属待裁定项（S-01）。
- **失败码**：`http_shim_last_curl_code()` 只是测试钩子；调用方读 `error()`。
- body 内嵌 NUL 不截断，长度只认 `body_len()`；逐字节夹具见 `p4-binary`。
