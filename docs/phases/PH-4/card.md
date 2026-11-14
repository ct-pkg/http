# PH-4 卡：同步执行与响应读取面

- 编号：PH-4
- phase 目录：`docs/phases/PH-4/`
- version-minor：4
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：f30c0cdce07227ff6d11e417d618a4eb6cb97fb32161279cb036ce9080f64bf6
- tasks-sha256：04cee1b1a5dfdc088097ed09c03d12e9ce2d0d31c728ba861db034ac74da6429

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-4（与目录 `docs/phases/PH-4/` 一致） |
| 对应 roadmap | [phase 4](../planning/ct-http-0.1.0-roadmap.md#phase-4--同步执行与响应读取面) |
| 对应 feature | F-16、F-04 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 不为 | 本卡不裁定 S-01…S-05；不加响应头读取（S-01）、不加 `method()`（S-02）、不加 `max_redirects`（S-03）、不做跨线程取消（S-04）、不公开 `HTTP_ERR_*` 枚举（S-05） |

## 1. 目标

`send()` 从固定值 stub 换成真 `curl_easy_perform`：请求真的打到可控的本地端点，响应面给出
`status` / `body` / `body_len` / `total_bytes`，响应仍由调用方显式释放，`take_body` 之后
body 的所有权只在一处。

第三人复述：用这个库写的请求会连到 `test/fixtures/p4-server.c` 起的本地服务端（127.0.0.1
随机端口），服务端计数 3 则证明真的发了三次请求；404/500 的状态码与 body 逐字节来自网络；
`/binary` 的 3 字节（含 NUL）按 `body_len` 读取而不是 `strlen`；`total_bytes` 等于 write
回调累计字节；`take_body` 之后 `body_len==0`，再 `http_response_free` 加 `free(body)` 在
ASAN 下 0 报错；连不上时 `http_shim_last_curl_code()` 为 7，总超时为 28。

## 2. 范围

### 2.1 做什么

- `src/http/http.shim.c`：`real_send` 取代 `stub_send`；write 回调动态缓冲；`CURLINFO_RESPONSE_CODE` 取状态；失败也返回响应对象（status 0）；`local_error` 非 0 时不 `perform`。
- `src/http/http.shim.h`：响应新增 `body_len` / `total_bytes`；读取函数与 `take_body`；测试钩子 `http_shim_last_curl_code`。
- `src/http/http.m.ct`：`ResponseView` 增加 `body_len` / `total_bytes` / `take_body`。
- `test/fixtures/p4-server.h` / `p4-server.c`：fork 子进程、bind 127.0.0.1:0、共享请求计数，端点 `/200`、`/404`、`/500`、`/binary`、`/split`、`/hang`。
- `test/fixtures/p4-*`：六个夹具 + expected。
- 既有夹具适配：`phase1_abi.c` 用 fake ops 保持输出；`phase1_chain.ct`、`phase1_var.ct`、`p2-after-send.ct`、`p3-chain.ct` 改用本地服务端，**输出逐字节不变**。
- `test/run.sh`、`tools/check.sh` 的 `g3`、`README.md`、`docs/session-map.md`。
- 红证写入 `docs/reviews/PH-4/T<n>.md`（复数目录，新增；不动未跟踪的 `docs/review/`）。

### 2.2 不做什么

- **不加响应头读取**（S-01）。即使 header 回调顺手可加也先不加，登记为「待裁定，实现成本 S（约 1 个任务：header 回调 + 存储 + 查询）」。
- 不做 `method()`（S-02）、`max_redirects`（S-03）、跨线程取消（S-04）、`HTTP_ERR_*` 枚举（S-05）。
- 不做流式（`send_stream`）、不做取消、不做重定向策略（只沿用默认不跟随）、不做真实 idle 超时。
- 不公开错误面：`http_shim_last_curl_code()` 是测试钩子，不是调用方可用的错误模型（PH-5 决定）。
- 不改 `docs/requirements.md`、`docs/design.md`；不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.c` | `real_send`、write 回调缓冲、状态码、失败也返回响应、`local_error` 短路 |
| `src/http/http.shim.h` | `HttpResponse.body_len` / `total_bytes`、三个读取函数、`http_shim_last_curl_code` |
| `src/http/http.m.ct` | `ResponseView` 三个新方法 |
| `test/fixtures/p4-server.h` + `p4-server.c` | 本地可控 HTTP 服务端与请求计数 |
| `test/fixtures/p4-status.c` + `.expected.out` | 200/404/500 状态与 body；服务端收到 3 次 |
| `test/fixtures/p4-binary.c` + `.expected.out` | 3 字节含 NUL，按长度读取 |
| `test/fixtures/p4-split.c` + `.expected.out` | 服务端两次写出，客户端累计 11 字节 |
| `test/fixtures/p4-take-body.c` + `.expected.out` | ASAN 正例：取走、释放响应、再释放 body |
| `test/fixtures/p4-take-body-bad.c` | ASAN 负例：未取走就释放响应，再释放旧指针必须 double-free |
| `test/fixtures/p4-refused.c` + `.expected.out` | 拒绝连接，码 7 |
| `test/fixtures/p4-timeout.c` + `.expected.out` | 总超时，码 28 |
| 既有四个夹具 | 适配真执行，输出不变 |
| `test/run.sh` | `run_cc_server` / `run_ct_server` / `run_asan_positive` 与接线 |
| `tools/check.sh` | `g3` 纳入全部 `p4-*` |
| `README.md`、`docs/session-map.md` | 响应面、超时边界、响应头待裁定；PH-4 登记 |
| `docs/reviews/PH-4/T1.md` … `T5.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 本地服务端夹具、`p4-status.c`、既有夹具适配、`real_send` + write 缓冲 + status/body | `test(http): PH4-T1 红` + `feat(http): PH4-T1 真执行` | `git revert` 实现提交后 `send()` 回到固定 200/`{"users":3}`，`p4-status` 的 404/500 与 `requests 3` 变红 | `bash test/run.sh p4-status` | M | E-P4-1 |
| T2 | `body_len` / `total_bytes` 字段与读取、`p4-binary.c`、`p4-split.c` | `test(http): PH4-T2 红` + `feat(http): PH4-T2 字节口径` | `git revert` 实现提交后读取函数消失，两个夹具构建失败 | `bash test/run.sh p4-binary && bash test/run.sh p4-split` | M | E-P4-2 |
| T3 | `http_response_take_body` 置空字段、CT `.take_body()`、`p4-take-body.c` 与负例 | `test(http): PH4-T3 红` + `feat(http): PH4-T3 单所有权` | `git revert` 实现提交后 `take_body` 消失，正例构建失败；负例仍 double-free 红 | `bash test/run.sh p4-take-body && bash test/run.sh p4-take-body-bad` | M | E-P4-3 |
| T4 | `http_shim_last_curl_code` 钩子、`p4-refused.c`、`p4-timeout.c` | `test(http): PH4-T4 红` + `feat(http): PH4-T4 失败码可观察` | `git revert` 实现提交后钩子消失，两个夹具构建失败 | `bash test/run.sh p4-refused && bash test/run.sh p4-timeout` | S | E-P4-4 |
| T5 | README 响应面/超时边界/响应头待裁定、`g3` 接线、`docs/session-map.md`、收口文档 | `test(docs): PH4-T5 红` + `docs: PH4-T5 响应面与收口` | `git revert` 文档提交后 README 回到 PH-3 口径，不影响构建 | `bash tools/check.sh g3 && grep -q 'take_body' README.md` | S | E-P4-5 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P4-1 | `send()` 真执行：可控本地端点收到 3 次请求，状态码与 body 逐字节来自网络；源码里确有 `perform` 调用 | `bash test/run.sh p4-status` 退出 0，输出 `200 {"users":3}`、`404 not found`、`500 boom`、`requests 3`；`grep -c 'curl_easy_perform' src/http/http.shim.c` 输出 `1` |
| E-P4-2 | 响应体按 write 回调累计，内嵌 NUL 不截断，`body_len` 与 `total_bytes` 同源 | `bash test/run.sh p4-binary` 退出 0，输出 `len 3 total 3 bytes 97 0 98`；`bash test/run.sh p4-split` 退出 0，输出 `len 11 total 11 body {"users":3}` |
| E-P4-3 | `take_body` 后原对象不持有 body：取走后 `body_len==0`，释放响应再释放 body 在 ASAN 下 0 报错；未取走就释放再释放旧指针必须 double-free | `bash test/run.sh p4-take-body` 退出 0，输出 `taken 1 len 0 again 1 first 123 total 11`；`bash test/run.sh p4-take-body-bad` 退出 0，ASAN 报 `double-free` |
| E-P4-4 | 执行失败可从钩子观察：拒绝连接为 7，总超时为 28；三种超时同码、调用方拿不到区分信息这一事实写进收口 | `bash test/run.sh p4-refused` 退出 0，输出 `status 0 code 7`；`bash test/run.sh p4-timeout` 退出 0，输出 `status 0 code 28` |
| E-P4-5 | 既有夹具在真执行下输出逐字节不变；门禁全绿；README 写明响应面、超时边界与响应头待裁定 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'take_body' README.md` 退出 0；`grep -q '响应头' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。
红证写进 `docs/reviews/PH-4/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha> 后同命令退出 0
```

- `baseline` 是实现提交的父提交；红由「测试提交的内容 + 无实现」产生，不使用转绿后的产物冒充。
- T1 的红是行为红：`p4-status` 能构建，但 stub 对 `/404` `/500` 也回 200、`requests 0`，diff 不匹配。
- T2/T4 的红是构建红：夹具引用尚不存在的 `http_response_body_len` / `http_shim_last_curl_code`，`cc -Werror` 失败。
- T3 的红是构建红：`ct build` 报 `no method 'take_body'`，C 夹具报未声明函数；负例在红状态下仍必须 double-free（否则 ASAN 断言无效）。
- T5 的红是 `grep -q 'take_body' README.md` 失败。
- 真执行的证据必须是服务端侧计数或 shim 钩子值，不能只看返回值是否等于 200。

## 7. 依赖

- `ct v0.44.1`（`/Users/samxx/.local/bin/ct`）；`--cflags` 可加 `-I test/fixtures`，`--ldflags` 可直接带 `.c`（已实测 `helper.c -lcurl` 通过）。
- 系统 libcurl 8.7.1（`-lcurl`）；本 phase 只用到 `CURLOPT_WRITEFUNCTION` / `WRITEDATA` / `CURLINFO_RESPONSE_CODE`。
- 前置：PH-3 收口 `ee4209d`；`bash tools/check.sh g0`、`g1`、`g3` 在开工前为绿。
- 本地服务端只用 POSIX socket / fork / mmap，不访问外网，不占用固定端口。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 真执行打破四个既有夹具的 stub 假设（`phase1_abi`、`phase1_chain`、`phase1_var`、`p2-after-send`、`p3-chain`） | 切 `perform` 后 g3 红 | 测试提交里先适配：`phase1_abi` 装 fake ops；其余指向本地服务端；输出逐字节不变，适配写进收口 §4 |
| 本地服务端端口与并发 | 端口冲突或连接被拒 | `bind 127.0.0.1:0` 取随机端口，端口经 `getsockname` 回传；每个夹具各起各的子进程 |
| 服务端请求计数在子进程里，父进程看不到 | `requests 0` 恒成立，判据失效 | 计数放 `mmap(MAP_SHARED｜MAP_ANONYMOUS)` |
| write 回调 realloc 失败 | 传输中断，错误码 23 | 回调返回 0 让 libcurl 中止；本 phase 只记录，错误面留 PH-5 |
| `total_bytes` 口径含糊 | 与 `body_len` 口径不一致 | 写死为 write 回调累计字节（解码后 body）；F-16 与本卡同口径 |
| 响应头顺手就能加 | 有人在本 phase 加 `header()` | 明令不加；登记「待裁定，实现成本 S（约 1 个任务：header 回调 + 存储 + 查询）」 |
| 三种超时同码 28，调用方无法区分 | 调用点只看到 28 | 本 phase 只用钩子记录 28；区分要靠 shim 自己计时与阶段标记（`CURLINFO_CONNECT_TIME_T`、write 首字节时间戳、进度回调空档），留 PH-6；curl 不提供「哪个超时触发」的字段 |
| `p4-split` 依赖 libcurl 分两次回调 | 客户端一次收完，仍为 11 字节 | 判据只断言累计 11 字节与 body 内容，不断言回调次数 |
| ASAN 与 fork 子进程 | 泄漏报告混入断言 | 正例/负例都设 `ASAN_OPTIONS=detect_leaks=0`，正例断言 rc==0 且无 `AddressSanitizer` |

## 9. 收口动作

1. 逐条跑 E-P4-1…E-P4-5，把命令、原始输出、退出码贴进 `docs/phases/PH-4/closeout.md`。
2. 对照 §4 任务表，逐个确认「红证提交 → 实现提交」相邻且 `baseline` 是父提交。
3. 检查 §3 交付物清单每个路径都已入库，无 `build/` 产物或临时文件混入。
4. 偏差只写进收口文档，不改本卡 §4/§5。
5. 收口文档单列两件事：①响应头「待裁定，实现成本 S」，本 phase 未加；②三种超时同码 `CURLE_OPERATION_TIMEDOUT`，调用方拿不到区分信息，以及区分它们需要什么（curl 侧只有阶段计时信息，没有「哪个超时触发」字段，需 shim 自己计时）。
6. 版本：保持 `0.1.0`，不移动 MINOR；不 push、不打 tag。
7. 报告一行当前事实：最小请求 `ct build` 与运行各返回什么、服务端是否收到请求（贴 `p4_server_requests` 与 `grep -c curl_easy_perform` 的真实值）。
