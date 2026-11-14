# PH-4 收口对照：同步执行与响应读取面

- 卡片：[docs/phases/PH-4/card.md](card.md)
- 判定：退出判据 E-P4-1…E-P4-5 全部通过；任务清单 5 项均有「红 → 绿」相邻提交。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=f30c0cdc…4bf6`、`tasks-sha256=04cee1b1…6429`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P4-1 | `bash test/run.sh p4-status` | `ok`，夹具输出 `200 {"users":3}`、`404 not found`、`500 boom`、`requests 3` | 0 |
| E-P4-1（静态锚点） | `grep -c 'curl_easy_perform' src/http/http.shim.c` | `1` | 0 |
| E-P4-2 | `bash test/run.sh p4-binary` / `p4-split` | `ok`，输出 `len 3 total 3 bytes 97 0 98` 与 `len 11 total 11 body {"users":3}` | 0 / 0 |
| E-P4-3 | `bash test/run.sh p4-take-body` | `ok p4-take-body (ASAN clean)`，输出 `taken 1 len 0 again 1 first 123 total 11` | 0 |
| E-P4-3（负例） | `bash test/run.sh p4-take-body-bad` | `ok p4-take-body-bad (ASAN rejected the stale pointer)`，ASAN 报 `double-free` | 0 |
| E-P4-4 | `bash test/run.sh p4-refused` / `p4-timeout` | `ok`，输出 `status 0 code 7` 与 `status 0 code 28` | 0 / 0 |
| E-P4-5 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0 (... version 0.1.0)` / `ok g1 (...)` / `ok g3 (... response bytes, take_body, failure codes)` | 0 / 0 / 0 |
| E-P4-5（README） | `grep -q 'take_body' README.md`、`grep -q '响应头' README.md` | 均命中 | 0 / 0 |
| E-P4-5（负例） | `bash test/gates/g1_negative.sh` | `ok g1-negative (12 tampered cards rejected)`，含 PH-4 三份篡改 | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）——已逐项见 §2。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `39c588d` | `c0070f7` | [T1.md](../../reviews/PH-4/T1.md) |
| T2 | `81c5a60` | `876f317` | [T2.md](../../reviews/PH-4/T2.md) |
| T3 | `f5d665c` | `a1fb667` | [T3.md](../../reviews/PH-4/T3.md) |
| T4 | `3c5672f` | `f3ba523` | [T4.md](../../reviews/PH-4/T4.md) |
| T5 | `68e333d` | `T5-DOCS-SHA` | [T5.md](../../reviews/PH-4/T5.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`body_len` / `total_bytes`、三个读取函数、`take_body`、`http_shim_last_curl_code` |
| `src/http/http.shim.c` | 入库；`real_send`、write 回调缓冲、状态码、失败也返回响应、CURLcode 钩子 |
| `src/http/http.m.ct` | 入库；`ResponseView` 增加 `body_len` / `total_bytes` / `take_body` |
| `test/fixtures/p4-server.h` / `p4-server.c` | 入库；127.0.0.1 随机端口、共享请求计数、六个端点 |
| `test/fixtures/p4-*` + expected | 入库；status/binary/split/take-body/refused/timeout 七个夹具 |
| 既有五个夹具 | 入库；适配真执行，输出逐字节不变 |
| `test/run.sh` | 入库；`run_ct_server` / `run_cc_server` / `run_asan_positive_server` / `run_asan_negative_server` |
| `tools/check.sh` | 入库；`g3` 纳入 7 个 `p4-*` |
| `README.md` | 入库；真执行、响应面、超时边界、响应头待裁定 |
| `docs/reviews/PH-4/T1.md` … `T5.md` | 入库 |

## 4. 偏差

1. **既有夹具适配（输出不变）**：真执行打破 stub 假设。`phase1_abi.c` 装固定的
   `abi_send` ops，ABI 探针不再依赖网络；`phase1_chain.ct`、`phase1_var.ct`、
   `p2-after-send.ct`、`p3-chain.ct` 改用 `p4_server_url("/200")`。四个 CT 夹具与
   一个 C 夹具的期望输出逐字节未改，`g3` 全部通过。
2. **roadmap 的 E-P4-4（`p4-total-bytes`）并入 E-P4-2**：`total_bytes` 与 `body_len`
   同源（都取 write 回调累计字节），单列一条判据会逼出两套口径。`p4-split` 同时覆盖
   两件事：多次回调累加、NUL 无关的长度。
3. **`p4-split` 不断言回调次数**：只断言累计 11 字节。libcurl 可能把两次写合并成
   一次回调，断言次数会变成环境相关；`len 11` 已经能证明「不是只留最后一片」。
4. **`http_shim_last_curl_code()` 是测试钩子**：失败侧需要可观察的量，但公开错误面
   属 PH-5（S-05 未裁定），因此只加进程内钩子，不扩公开 API。
5. **`send()` 对 `local_error` 非 0 的请求短路**：不 `perform`，返回 `status 0` 的响应。
   PH-3 只把本地错误写进结构体，调用点在 PH-4 仍拿不到区分信息，见 §6。
6. **一个夹具耗时约 0.3 秒**：`p4-timeout` 用 `/hang` 睡 5 秒，客户端 300 ms 超时；
   服务端子进程在夹具结束时被 `SIGTERM` 回收，未留下孤儿进程。

## 5. 响应头读取：待裁定，未加入

- 本 phase **没有**实现响应头读取。即使实现成本很低（header 回调 + 存储 + 查询，
  约 1 个任务，规模 S），也按裁定前置原则没有顺手加。
- 登记：**待裁定（S-01），实现成本 S（约 1 个任务：header 回调 + 存储 + 查询）**。
- 若纳入：F-16 的响应面从 6 项扩到 7 项以上，需处理大小写归一、重复头、重定向中间
  响应与头上限；`p4-*` 夹具要加 `/headers` 端点；PH-4 的判据需重开。
- 若不纳入：响应面保持 `status` / `body` / `body_len` / `total_bytes` / `take_body` /
  `free` 六项，下游只能靠状态码与 body 自行判断。

## 6. 三种超时：同码，调用方拿不到区分信息

- 现状：连接超时（`CURLOPT_CONNECTTIMEOUT_MS`）与总超时（`CURLOPT_TIMEOUT_MS`）在
  libcurl 侧都返回 `CURLE_OPERATION_TIMEDOUT`(28)；`p4-timeout` 实测 `code 28`。
  第三种「空闲超时」在 0.1.0 还没有对应 API（roadmap phase 6/7）。
- 结论：调用方只看到 28，**无法判断是连接没建起来、还是总时长用完、还是数据流空档**。
  这不是本 phase 的疏漏，而是当前判据只记录、不区分。
- 区分它们需要什么：
  - curl 侧：没有「哪个超时触发」的字段。可用的只有阶段时间
    （`CURLINFO_CONNECT_TIME_T`、`CURLINFO_STARTTRANSFER_TIME_T`、`CURLINFO_TOTAL_TIME_T`）
    与 `CURLINFO_OS_ERRNO`，需要 shim 自己判断哪一段超了。
  - 自己计时：在 progress 回调或 write 回调里记时间戳，得到「连接完成时刻」
    与「首字节/末字节时刻」，才能把 28 拆成 `phase=connect` / `phase=total` / `phase=idle`。
- 处置：留给 PH-6；本 phase 只保证 28 可观察（钩子）且事实被记录。

## 7. 当前事实

用这个库写最小请求：`ct build` 退出 0，运行退出 0，`status 200`、body `{"users":3}`；
**真的发出了网络请求**——`p4-status` 的本地服务端计数 `requests 3`，
`grep -c 'curl_easy_perform' src/http/http.shim.c` 为 `1`（不再是 shim 固定值）。
