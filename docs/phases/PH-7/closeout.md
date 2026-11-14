# PH-7 收口对照：流式执行与慢流保护

- 卡片：[docs/phases/PH-7/card.md](card.md)
- 判定：E-P7-1…E-P7-8 全部通过；偏差见 §4。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=fc17f63e…0c27`、`tasks-sha256=000a21ce…7b61`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P7-1 | `bash test/run.sh p7-chunks` | `ok`，输出 `chunks_ge2 1 bytes_ok 1 total 1048576 body_len 0 body_null 1 error 0 status 200`（ASAN clean） | 0 |
| E-P7-2 | `bash test/run.sh p7-body-nul` | `ok`，输出 `sent 3 bytes_ok 1 body_len 0 total 3 error 0` | 0 |
| E-P7-3 | `bash test/run.sh p7-chunked` | `ok`，输出 `status 200 error 0 total 14 text alphabetagamma body_len 0` | 0 |
| E-P7-4 | `bash test/run.sh p7-abort` | `ok`，输出 `error 23 cancelled 0 body_len 0 body_null 1 total_gt0 1 chunks 1` | 0 |
| E-P7-5 | `bash test/run.sh p7-limit` | `ok`，两行 `nstream error -1008 body_len 1024 total 1024 take 1024 ok 0`、`stream error -1008 body_len 0 total 1024 cb 1024 ok 0`（ASAN clean） | 0 |
| E-P7-6 | `bash test/run.sh p7-idle` | `ok`，两行 `stream error 28 kind 3 body_len 0 total 1 got 1 take_null 1`、`partial error 28 kind 3 body_len 1 total 1 take 1` | 0 |
| E-P7-7 | `bash test/run.sh p7-mutex` | `ok`，输出 `nullcb -1001 requests 0`、`mixed -1007 requests 1` | 0 |
| E-P7-8 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0`、`ok g1`、`ok g3 (… timeouts, streaming)` | 0 / 0 / 0 |
| E-P7-8 | `grep -q 'send_stream' README.md` / `max_body_bytes` | 均命中 | 0 |
| E-P7-8（负例） | `bash test/gates/g1_negative.sh` | 拒绝 21 份篡改卡（含 PH-7 三份） | 0 |
| 补充 | `bash test/run.sh p7-slow` | `ok`，输出 `status 200 error 0 total 4 got 4 chunks_ge2 1 body_len 0` | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `00dc45a` | `178719f` | [T1.md](../../reviews/PH-7/T1.md) |
| T2 | `bf230ec` | `3225545` | [T2.md](../../reviews/PH-7/T2.md) |
| T3 | `3e42a0a` | `9119ca9` | [T3.md](../../reviews/PH-7/T3.md) |
| T4 | `e748d4c` | `cd93f0a` | [T4.md](../../reviews/PH-7/T4.md) |
| T5 | `c9ef9e4` | `7027892` | [T5.md](../../reviews/PH-7/T5.md) |
| T6 | `e9dc0e2` | 见 `docs: PH7-T6 流式面与收口` | [T6.md](../../reviews/PH-7/T6.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`HttpChunkCb`、`send_stream`、`max_body_bytes`、`-1007`/`-1008`、IDLE 精度说明 |
| `src/http/http.shim.c` | 入库；流式写回调、回调中止 23、上限 -1008、交付口径、模式锁 |
| `src/http/http.m.ct` | 入库；`send_stream(cb, ud)`、`max_body_bytes(n)` |
| `test/fixtures/p4-server.c` | 入库；`/big`、`/echo`、`/chunked`、`/slow4`、`/stall2` |
| `test/fixtures/p7-chunks.c` … `p7-mutex.c` + expected | 入库；八个流式夹具 |
| `test/run.sh`、`tools/check.sh` | 入库；八个夹具并入 `g3`，摘要含 `streaming` |
| `README.md` | 入库；流式面、`total_bytes` 口径、PH-6 遗留两条 |
| `docs/streaming.md` | 入库；回调契约、上限语义、所有权、chunked、idle 叠加 |
| `docs/session-map.md` | 入库；PH-7 标记已收口 |
| `docs/reviews/PH-7/T1.md` … `T6.md` | 入库 |

## 4. 偏差

1. **`total_bytes` 选取「到上限即中止」**：评审指出的「截断前总量」与「超限即中断」不能
   同时成立；本 phase 选择后者。累计到 `max_body_bytes` 时立即停止，`total_bytes` 与
   保留的 `body_len` 都等于上限，不报告服务器原本会发的总量。若继续读完再只保留前 N 字节，
   上限既不能保护内存也不能保护带宽，且会让“总量”与“已交付量”分离。
2. **新增 `max_body_bytes` 公开 setter**：requirements 与 design 没有该条款，也没有禁止；
   这是本 phase 为落实上限语义新增的保护性配置，不改任何既有条款。若评审认为新增公开 API
   属于范围变更，需另行裁定；当前不视为 requirements 冲突。
3. **R-12 的「与 body 同时设置」按响应模式解释**：`body()` + `send_stream()` 允许共存，
   因为本 phase 的 `\0` 用例要求请求体经流式路径发送；真正互斥的是 `send()` 与
   `send_stream()`（第二次读 `-1007`）。未改 R-12 字面；若评审要求按字面禁止请求体 +
   流式响应，停下报告，不自改条款。
4. **慢流用例等比压缩**：F-03 写的是「每 2 秒 1 字节、持续 6 秒、`idle_timeout(3)`」；
   夹具用「每 1 秒 1 字节、共 4 字节、`idle_timeout(2000)`」。数据间隔仍小于空闲阈值，
   断言性质不变，整套时长压到 3.77 秒（该用例墙钟）。
5. **`p7-slow` 在 T4 基线上即为绿**：T4 的空档判定从不看平均速率，所以“慢而持续”本来就
   不会被误杀。它是回归守卫，不是新增行为；没有伪造红。红/绿证据以 T1–T5 为准。
6. **chunked 用例在 T1 基线上即通过**：libcurl 已解码 chunked，shim 的写回调不依赖
   `Content-Length`。它证明的是“无长度响应能收全且不卡死”，不是新增解码逻辑；E-P7-3
   仍作为证据保留。
7. **`p7-idle` 在 green 中重启服务端**：服务端单线程，第一段 `/stall2` 的子进程还在
   `sleep`，第二段会把等待 `accept` 的时间算成“首字节前空档”，读到 0 字节。断言未变，
   只隔离服务端生命周期；红证保留的是重启前的合并夹具输出（见 T4.md 补充）。
8. **ASAN 只覆盖所有权相关用例**：`p7-chunks` 与 `p7-limit` 走 ASAN 正例；其余用例用普通
   cc，避免整套时长翻倍。回调中止、idle 与 mutex 的所有权由代码路径同一性覆盖。

## 5. `total_bytes` 最终口径

**只数实际交付的响应体字节**：

- 非流式：写回调写入内置缓冲的字节；流式：交给用户回调的字节。
- 不含响应头、chunked 框架、TLS 记录，也不含“服务器原本还会发”的字节。
- 非流式成功：`body_len() == total_bytes()`。
- 流式成功：`body_len() == 0`，`total_bytes()` 是回调累计。
- 回调中止：`total_bytes()` 包含触发中止的那一片（它已经交给回调）。
- 上限命中：`total_bytes() == max_body_bytes()`，不会更多。
- 非流式上限命中保留前 `n` 字节，`take_body()` 可取；流式上限命中 `take_body()==NULL`。

错误优先级写死：回调中止 `23` > 上限 `-1008` > idle `28/3` > 传输码。

## 6. 含 `\0` 载荷实测

`p7-body-nul` 用 `body("a\0b", 3)` + `send_stream` 请求 `/echo`；服务端按请求体回显，
回调逐字节比较。输出 `sent 3 bytes_ok 1 body_len 0 total 3 error 0`：请求体 3 字节、
含 `0x00`，没有被 `strlen` 截成 1 字节。失败模式下（漏 `POSTFIELDSIZE`）回显会是 1 字节，
断言变红。

## 7. chunked 用例结果

`p7-chunked` 请求 `/chunked`：服务端只发 `Transfer-Encoding: chunked`，三个分块
（`alpha`、`beta`、`gamma`）后 `0\r\n\r\n` 并关闭，没有 `Content-Length`。客户端输出
`status 200 error 0 total 14 text alphabetagamma body_len 0`，未卡死。

## 8. 真实网络与注入的边界

**PH-7 的流式结论全部是本地回环真实网络，没有使用 `http_shim_inject_curl_code`。**

| 用例 | 触发方式 | 验证类型 |
| --- | --- | --- |
| `p7-chunks` | `/big` 1 MiB，512 KiB 后停 50 ms | 真实网络 + ASAN |
| `p7-abort` | `/big` 首片回调返回 1 | 真实网络 |
| `p7-body-nul` | `/echo` 回显 3 字节请求体 | 真实网络 |
| `p7-chunked` | `/chunked` 无长度、分块、关闭 | 真实网络 |
| `p7-limit` | `/big` + `max_body_bytes(1024)` | 真实网络 + ASAN |
| `p7-idle` | `/stall2` 1 字节后停 2 秒 | 真实网络 |
| `p7-slow` | `/slow4` 1 秒 1 字节 ×4 | 真实网络 |
| `p7-mutex` | `/200`，两次本地拦截 | 真实网络（计数证明未发） |

## 9. 执行时长

单机 macOS arm64，每例含一次构建：

| 用例 | 墙钟 |
| --- | --- |
| `p7-chunks` | 1.07 s |
| `p7-abort` | 0.77 s |
| `p7-body-nul` | 0.79 s |
| `p7-chunked` | 0.80 s |
| `p7-limit` | 0.96 s |
| `p7-idle` | 2.91 s |
| `p7-slow` | 3.77 s |
| `p7-mutex` | 0.72 s |
| **PH-7 八例合计** | **11.79 s** |
| `g3` 全量（含全部历史夹具） | 42.05 s |

低于 15 秒目标。慢流 3.77 秒是最大单项；`p7-idle` 2.91 秒来自两段 1 秒空档判定。

## 10. PH-6 遗留落点

1. **IDLE 精度**：README「超时（phase 6）」写明「IDLE 精度约 1 秒，配小于 1 秒的值不保证
   按值中止」；`http.shim.h` 的 `idle_timeout_ms` 与 `http_response_timeout_kind` 注释同样写明。
   `docs/streaming.md` §7 再次引用。
2. **没有独立低速下限**：README 明确「放弃 `LOW_SPEED_LIMIT` 后，持续有数据但整体极慢的流
   只在设置 `idle_timeout` 时受约束；未设置时库不会因为它平均很慢而中止」；
   `docs/streaming.md` §7 同步。

## 11. requirements 与 design

未改 `docs/requirements.md`、`docs/design.md`。`total_bytes` 与 `max_body_bytes` 的口径在
卡片、README 与 `docs/streaming.md` 定死，没有改既有条款；R-12 的 body/stream 互斥按
§4.3 解释并已报告，若评审要求按字面执行需用户裁定。

## 12. 当前事实

`send_stream(cb,ud)` 已落地：1 MiB 流 `chunks_ge2 1/bytes_ok 1`，含 `\0` 的 3 字节请求体
完整到达，无 `Content-Length` 的 chunked 响应收全 14 字节；`max_body_bytes(1024)` 在
`-1008` 处停住且 `total_bytes==1024`；流式 idle 中止读 `28/3` 且已交付字节归回调。
PH-7 八例合计 11.79 s，`g3` 42.05 s。
