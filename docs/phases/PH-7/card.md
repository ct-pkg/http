# PH-7 卡：流式执行与慢流保护

- 编号：PH-7
- phase 目录：`docs/phases/PH-7/`
- version-minor：7
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：fc17f63e07069d49e946cea62d118fddb9e8b1d579b6101ecdce8e1f20680c27
- tasks-sha256：000a21ce46329763aab434e857ab403f7e20c86be94e8b89daeac62866d47b61

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-7（与目录 `docs/phases/PH-7/` 一致） |
| 对应 roadmap | [phase 7](../planning/ct-http-0.1.0-roadmap.md#phase-7--流式执行与慢流保护) |
| 对应 feature | F-18、F-03 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 已裁定 | S-05：不引入枚举；本地错误继续用负数码，`HTTP_LOCAL_ERR_STATE=-1007`、`HTTP_LOCAL_ERR_BODY_LIMIT=-1008` |
| 不为 | 不做 `CURLOPT_LOW_SPEED_*`；不做异步/多路复用；不做断点续传；不改 `docs/requirements.md` / `docs/design.md` |

## 1. 目标

`send_stream(cb, ud)` 把响应体按到达顺序交给命名回调，不整包落内存；回调返回非 0 即中止，
对外是 `error()==23`、`is_cancelled()==0`。同时把 `total_bytes` 的口径一次定死，并补上
`max_body_bytes(n)`：**累计到上限立即中止并报 `-1008`**，`total_bytes` 等于上限，非流式保留
已收前缀，流式已交付的片归调用方。合法慢流仍只靠 `idle_timeout` 的空档判定约束，不引入
低速窗口。

第三人复述：1 MB 流的回调至少 2 片、拼接逐字节相等；首片返回非 0 读 `23`；带 `\0` 的
3 字节请求体经 `send_stream` 完整到达；无 `Content-Length` 的 chunked 响应能收全；
`max_body_bytes(1024)` 下非流式 `body_len==total_bytes==1024` 且 `error==-1008`；
流式同限下 `body_len==0`、`total_bytes==1024`；流式 idle 中止读 `28/3`，已交付的 1 字节
仍在回调侧、`take_body` 返回 NULL；混用 `send()` 与 `send_stream()` 读 `-1007`。

## 2. 范围

### 2.1 做什么

- `HttpChunkCb`（`int (*)(char *data, int len, void *ud)`）+ `http_request_send_stream(r, cb, ud)`；
  CT `.send_stream(cb, ud)`。回调拿到的 `data` 只在本次调用内有效，跨回调保留必须自行复制。
- 回调返回非 0 → 写回调返回 0 → `CURLE_WRITE_ERROR(23)`；`is_cancelled` 保持 0。
- `http_request_max_body_bytes(r, n)` + CT `.max_body_bytes(n)`；`n==0` 不限，`n<0` 本地 `-1001`。
- `HTTP_LOCAL_ERR_BODY_LIMIT (-1008)`：达到上限立即中止；`total_bytes` 与保留的 `body_len`
  都等于上限；流式 `body_len==0`、`total_bytes==上限`。
- `HTTP_LOCAL_ERR_STATE (-1007)`：同一请求对象混用 `send()` / `send_stream()`；在连接前返回。
  `send_stream(r, NULL, ud)` 读 `-1001`，也不发请求。
- `total_bytes` 口径写死：write 回调（非流式）或用户回调（流式）**实际交付**的响应体字节数，
  解码后计数；不含 chunked 框架、TLS、响应头，也不含未交付的字节；达到上限时等于上限，
  不报告服务器“原本会发多少”。非流式成功时 `body_len==total_bytes`；流式恒 `body_len==0`、
  `body()==NULL`、`take_body()==NULL`。
- 流式 idle：沿用 PH-6 的 `XferState`；`/stall` 中 `idle_timeout(500)` 中止后读 `28/3`，
  已交付的字节留在回调侧，`total_bytes` 等于已交付字节；非流式保留已收前缀，`take_body`
  可取到该前缀。
- `test/fixtures/p4-server.c` 增加 `/big`（1 MB）、`/echo`（回显请求体）、`/chunked`
  （无 `Content-Length` 的分块响应）、`/slow4`（1 秒 1 字节 ×4）、`/stall2`（1 字节后停 2 秒）。
- 夹具 `p7-chunks`、`p7-abort`、`p7-body-nul`、`p7-chunked`、`p7-limit`、`p7-idle`、`p7-slow`、
  `p7-mutex`；`p7-chunks` 与 `p7-limit` 用 ASAN 正例运行。
- README 的流式面、`total_bytes` 口径、PH-6 遗留两条（IDLE 精度约 1 秒；没有独立低速下限）；
  `tools/check.sh` 的 `g3`；`docs/session-map.md`；红证进 `docs/reviews/PH-7/`。

### 2.2 不做什么

- 不做 chunked 上传 API：0.1.0 的请求体仍是显式 `body(data,len)`；本 phase 证明它经
  `send_stream` 发送时 `POSTFIELDSIZE` 生效、`\0` 不截断。若要求流式上传协议，需另立需求。
- 不用 `CURLOPT_LOW_SPEED_LIMIT` / `LOW_SPEED_TIME`。
- 不做异步、多路复用、断点续传、回调内重入、跨线程。
- 不新增与 `-1001…-1006` 冲突的本地码；`-1007`、`-1008` 是追加，不改旧值。
- 不改 `docs/requirements.md`、`docs/design.md`。R-12 的「与 body 同时设置」按「响应累积
  模式与流式模式互斥」解释；**请求体上传与流式响应共存**，因为本 phase 的 `\0` 用例要求
  这一点。若评审要求按 R-12 字面禁止请求体 + 流式响应，停下报告，不自改条款。
- 不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | `HttpChunkCb`、`max_body_bytes`、`send_stream`、`-1007`/`-1008`、流式读取说明 |
| `src/http/http.shim.c` | 流式写回调、回调中止 23、上限中止 -1008、`total_bytes` 交付口径、`-1007` 模式锁 |
| `src/http/http.m.ct` | `RequestBuilder.send_stream(cb, ud)`、`max_body_bytes(n)` |
| `test/fixtures/p4-server.c` | `/big`、`/echo`、`/chunked`、`/slow4`、`/stall2` |
| `test/fixtures/p7-chunks.c` | 1 MB ≥2 片、拼接逐字节、ASAN |
| `test/fixtures/p7-abort.c` | 首片返回非 0 → 23、`is_cancelled==0` |
| `test/fixtures/p7-body-nul.c` | `body("a\0b",3)` 经 `send_stream` 完整到达 |
| `test/fixtures/p7-chunked.c` | 无 `Content-Length` 的 chunked 响应收全 |
| `test/fixtures/p7-limit.c` | `max_body_bytes` 非流式/流式两种形态、ASAN |
| `test/fixtures/p7-idle.c` | 流式 idle 28/3 + 非流式部分保留 |
| `test/fixtures/p7-slow.c` | 慢而持续的流不被误杀 |
| `test/fixtures/p7-mutex.c` | NULL 回调 -1001、混用模式 -1007 |
| `test/run.sh`、`tools/check.sh` | 八个夹具接线并入 `g3` |
| `README.md`、`docs/session-map.md` | 流式面、`total_bytes`、PH-6 遗留、PH-7 登记 |
| `docs/reviews/PH-7/T1.md` … `T6.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | `send_stream` + `HttpChunkCb` + 分片交付；`p7-chunks`、`p7-body-nul`、`p7-chunked` | `test(http): PH7-T1 红` + `feat(http): PH7-T1 流式分片` | revert 后 `send_stream` 不存在，三个夹具构建失败 | `bash test/run.sh p7-chunks && bash test/run.sh p7-body-nul && bash test/run.sh p7-chunked` | M | E-P7-1 |
| T2 | 回调中止 → 23、`is_cancelled==0`；`p7-abort` | `test(http): PH7-T2 红` + `feat(http): PH7-T2 回调中止` | revert 后回调返回值被忽略，传输读完，断言红 | `bash test/run.sh p7-abort` | S | E-P7-4 |
| T3 | `max_body_bytes` + `total_bytes` 上限口径 + `-1008`；`p7-limit` | `test(http): PH7-T3 红` + `feat(http): PH7-T3 响应体上限` | revert 后无上限，`/big` 读完 1 MB，断言红 | `bash test/run.sh p7-limit` | M | E-P7-5 |
| T4 | 流式 idle 中止与部分保留；`p7-idle` | `test(http): PH7-T4 红` + `feat(http): PH7-T4 流式空闲` | revert 后流式不记空档，`/stall2` 走完，`error 18`，断言红 | `bash test/run.sh p7-idle` | M | E-P7-6 |
| T5 | NULL 回调 `-1001`、模式混用 `-1007`；`p7-mutex` | `test(http): PH7-T5 红` + `feat(http): PH7-T5 流式状态` | revert 后 NULL 回调不报错、混用不拦，断言红 | `bash test/run.sh p7-mutex` | S | E-P7-7 |
| T6 | README 流式/`total_bytes`/PH-6 遗留、`g3` 接线、session-map、收口 | `test(docs): PH7-T6 红` + `docs: PH7-T6 流式面与收口` | revert 后 README 没有 `send_stream`，断言红 | `bash tools/check.sh g3 && grep -q 'send_stream' README.md` | S | E-P7-8 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P7-1 | 1 MB 流至少 2 片、拼接逐字节相等，流式响应 `body_len==0`、`body()==NULL`，ASAN 无报告 | `bash test/run.sh p7-chunks` 退出 0，输出 `chunks_ge2 1 bytes_ok 1 total 1048576 body_len 0 body_null 1 error 0 status 200` |
| E-P7-2 | 带 `\0` 的 3 字节请求体经 `send_stream` 完整到达，响应也按 3 字节收全 | `bash test/run.sh p7-body-nul` 退出 0，输出 `sent 3 bytes_ok 1 body_len 0 total 3 error 0` |
| E-P7-3 | 无 `Content-Length` 的 chunked 响应收全且不卡死 | `bash test/run.sh p7-chunked` 退出 0，输出 `status 200 error 0 total 14 text alphabetagamma body_len 0` |
| E-P7-4 | 回调首片返回非 0 → `error()==23`、`is_cancelled()==0`、响应不持有 body | `bash test/run.sh p7-abort` 退出 0，输出 `error 23 cancelled 0 body_len 0 body_null 1 total_gt0 1 chunks 1` |
| E-P7-5 | `max_body_bytes(1024)`：非流式 `error -1008`、`body_len 1024`、`total_bytes 1024`、`take_body` 可取前缀；流式 `body_len 0`、`total_bytes 1024`；ASAN 无报告 | `bash test/run.sh p7-limit` 退出 0，输出两行 `nstream error -1008 body_len 1024 total 1024 take 1024 ok 0`、`stream error -1008 body_len 0 total 1024 cb 1024 ok 0` |
| E-P7-6 | 流式 idle 中止读 `28/3`，已交付字节归回调、`take_body()==NULL`；非流式保留已收前缀 | `bash test/run.sh p7-idle` 退出 0，输出 `stream error 28 kind 3 body_len 0 total 1 got 1 take_null 1`、`partial error 28 kind 3 body_len 1 total 1 take 1` |
| E-P7-7 | NULL 回调在读 `-1001` 且不发请求；混用 `send`/`send_stream` 读 `-1007` 且第二次不发请求 | `bash test/run.sh p7-mutex` 退出 0，输出 `nullcb -1001 requests 0`、`mixed -1007 requests 1` |
| E-P7-8 | 门禁全绿；README 写明流式、`total_bytes`、PH-6 的 IDLE 精度与无低速下限 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'send_stream' README.md` 与 `grep -q 'max_body_bytes' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。
红证写进 `docs/reviews/PH-7/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha 或提交名> 后同命令退出 0
```

- T1 的红是构建红：`send_stream`/`HttpChunkCb` 尚不存在。
- T2 的红是行为红：T1 的回调返回值被忽略，`p7-abort` 读完 1 MB。
- T3 的红是构建/行为红：`max_body_bytes` 不存在；即使手动截断也没有 `-1008`。
- T4 的红是行为红：流式路径不更新 `XferState`，`/stall2` 走完，`error 18`。
- T5 的红是行为红：NULL 回调未被拦截、混用模式未拦。
- T6 的红是 `grep -q 'send_stream' README.md` 失败。
- 本 phase 的流式结论全部来自本地回环真实网络；不使用注入钩子。

## 7. 依赖

- `ct v0.44.1`、系统 libcurl 8.7.1（chunked 解码、`CURLOPT_WRITEFUNCTION`）。
- 前置：PH-6 收口 `9e78023`；`g0`、`g1`、`g3` 开工前为绿。
- `test/fixtures/p4-server.h` 的本地服务端；`--ldflags` 带 `p4-server.c -lcurl`。
- 测试时长目标 <15 秒。慢流用例把 F-03 的「2 秒/字节 ×6 秒」等比压缩为
  「1 秒/字节 ×4 秒」，`idle_timeout(2000)` 仍远大于数据间隔，断言性质不变；收口给出实测。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| chunked 响应在 libcurl 侧已解码，夹具可能“天然绿” | `p7-chunked` 在 T1 基线即通过 | 仍保留为 E-P7-3 的证据，收口注明它验证的是“不依赖 Content-Length”，不是新增解码逻辑 |
| 1 MB 流可能被 libcurl 合并成 1 片 | `chunks_ge2 0` | `/big` 在前 512 KB 后插入 50 ms 间隔，强制至少两次 write 回调 |
| 回调中止与上限同时触发 | 同一片既超限又返回非 0 | 优先级写死：回调中止（23）> 上限（-1008）> idle（28）；夹具只覆盖单一触发 |
| 流式 idle 的回调粒度约 1 秒 | `idle_timeout(500)` 约 1 秒才中止 | 判据只断言 `28/3` 与字节数，不断言毫秒 |
| `/stall2` 子进程在睡 | 下一个请求排队 | 子进程被 `p4_server_stop` SIGTERM 回收；p7-idle 顺序执行，预计 <4 秒 |
| 上限截断的 `int` 转换 | 上限大于 1 MB 或负数 | 夹具只用 1024；`n<0` 本地 `-1001`；`total_bytes` 仍为 `int` |
| R-12 的 body/stream 互斥与 `\0` 用例冲突 | 评审按字面要求禁止请求体 + 流式响应 | 本卡按响应模式解释并写进 §2.2；若必须按字面，停下报告 |

## 9. 收口动作

1. 逐条跑 E-P7-1…E-P7-8，把命令、原始输出、退出码贴进 `docs/phases/PH-7/closeout.md`。
2. 对照 §4 确认「红证提交 → 实现提交」相邻且 baseline 是父提交。
3. 检查 §3 每个路径入库，无 `build/` 产物。
4. 偏差只写进收口，不改本卡 §4/§5。
5. 收口单列：`total_bytes` 最终口径、上限语义、回调所有权、`\0` 实测、chunked 结果、
   真实网络说明、PH-7 套件时长、PH-6 遗留两条的落点。
6. 版本保持 `0.1.0`；不 push、不打 tag。
7. 报告给出：`total_bytes` 最终口径、含 `\0` 载荷实测、chunked 用例结果、PH-7 套件时长。
