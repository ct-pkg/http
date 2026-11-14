# PH-8 卡：取消与生命周期

- 编号：PH-8
- phase 目录：`docs/phases/PH-8/`
- version-minor：8
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：b2dd6134f9df9c5a75151d3116c7aad5c921d9305d7d54bc9a644da4a1f57520
- tasks-sha256：f6c46c1c65197713f13e764511a566dc2f83cdc2505017ced75dc25d7cb6289d

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-8（与目录 `docs/phases/PH-8/` 一致） |
| 对应 roadmap | [phase 8](../planning/ct-http-0.1.0-roadmap.md#phase-8--取消与生命周期) |
| 对应 feature | F-19、F-11 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 已裁定 | S-04：**不纳入跨线程取消**，F-11 走方向 A；裁决见 `docs/reviews/PH-8/S-04-ruling.md` |
| 不为 | 不做跨线程 / 异步 / 信号取消；不做取消 token 或引用计数；不引入 pthread/TSAN；不改 `docs/requirements.md` / `docs/design.md` |

## 1. 目标

同线程内可确定性取消：`http_request_cancel(r)`（CT `req->cancel()`）置一个粘性标志，
传输在三个判定点（连接前、写回调交付前、进度回调）读它；只有取消**真正中断**传输时
才置 `is_cancelled()==1` 并对外报 `42`。`send()` 与 `send_stream()` 都不释放请求，
调用方在 `send()` 返回后仍可读请求字段，再显式 `free`；旧的「send 自动释放」路径不存在。

第三人复述：`/slow4` 慢响应中回调内 cancel，3 秒内返回 `42`、`is_cancelled==1`、`is_ok==0`、
中止那一片不交付；未取消路径两个入口都 `is_cancelled==0`；`send()` 返回后请求的
`url`/`mode` 可读、`easy_cleanup` 仍为 0，free 后为 1，ASAN 0 报告；负例在 ASAN 下被
判 `heap-use-after-free`；CT 侧 `req->cancel()` 可达，连接前取消读 `42` 且服务端计数 0。

## 2. 范围

### 2.1 做什么

- `HttpRequest` 增加 `int cancel_requested`（同线程普通字段，非 atomic）；
  `void http_request_cancel(struct HttpRequest *r)`，NULL 是 no-op。
- CT 映射：`RequestBuilder` 增加 `void cancel()`，映射到 `http_request_cancel`。
- 取消判定点与优先级（与 [S-04 裁决](../../reviews/PH-8/S-04-ruling.md) §2 一致）：
  1. 传输开始前：本地校验与模式锁之后，标志已置位则不连接、直接 `42`；
  2. 每次写回调在交付字节前先查标志，命中则中止且该片不交付、不计入 `total_bytes`；
  3. 每次进度回调（约 1 秒一次）先查标志，再判空闲。
  优先级：取消 `42/cancelled=1` > 回调中止 `23/cancelled=0` > 上限 `-1008` >
  空闲 `28/3` > 传输码。
- 进度回调从「仅在 `idle_timeout>0` 时安装」改为**始终安装**（`NOPROGRESS=0`），
  否则没有 idle 配置时取消无法在停顿流上生效。
- 生命周期：`send()` / `send_stream()` 不释放请求、不 `curl_easy_cleanup`；
  README 写明显式释放契约（`.free()` 终态一次；C 侧槽位形式可二次调用）。
- 已收数据归属：非流式保留已收前缀、`take_body()` 可取；流式已交付片归调用方、
  `body()==NULL`、`take_body()==NULL`。
- 夹具：`p8-cancel`（回调内取消）、`p8-not-cancelled`（两入口）、`p8-lifetime`（ASAN 正例）、
  `p8-lifetime-bad`（ASAN 负例：旧设计式的 free 后读）、`p8-chain`（CT `.cancel()` 连接前取消）。
- README「取消与生命周期（phase 8）」；`docs/streaming.md` 增取消与空闲/回调中止的叠加；
  `tools/check.sh g3` 接线；`docs/session-map.md`；红证进 `docs/reviews/PH-8/`；
  S-04 主张 / 反证 / 裁决三份文档。

### 2.2 不做什么

- 不做跨线程取消、异步取消、信号触发（R-13 非目标；S-04 已裁定不纳入）。
- 不做取消 token、引用计数、`http_request_ref/unref`；不引入 pthread 夹具与 TSAN。
- 不做「取消后复用同一请求重试」：标志粘性，重试要新建请求（README 写明）。
- 不做 DNS 阶段抢占式中止：解析返回前观察不到标志，属已知边界，写进文档与卡。
- 不新增本地错误码：取消沿用 `CURLE_ABORTED_BY_CALLBACK(42)`，不占用 `-1001…-1008`。
- 不改 `docs/requirements.md` / `docs/design.md`：R-13 已覆盖本次落地的同线程口径。
- 不 push、不打 tag；本 phase 一个本地提交。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | `cancel_requested` 字段、`http_request_cancel`、取消语义与同线程限制注释、`cancelled` 注释更新 |
| `src/http/http.shim.c` | 取消标志、三个判定点、始终安装进度回调、`42/cancelled=1` 记账、优先级 |
| `src/http/http.m.ct` | `RequestBuilder.cancel()` 映射 |
| `test/fixtures/p8-cancel.c` | `/slow4` 回调内取消，`42/cancelled 1/ok 0`、`lt3` |
| `test/fixtures/p8-not-cancelled.c` | `send()` 与 `send_stream()` 两条未取消路径 |
| `test/fixtures/p8-lifetime.c` | `send()` 后读请求字段、free 后计数，ASAN 正例 |
| `test/fixtures/p8-lifetime-bad.c` | free 后读请求，ASAN 负例（`heap-use-after-free`） |
| `test/fixtures/p8-chain.ct` | CT `.cancel()` 连接前取消、服务端计数 0 |
| `test/run.sh`、`tools/check.sh` | 五个夹具接线并入 `g3`，新增 UAF 负例运行器 |
| `README.md`、`docs/streaming.md` | 取消语义、精度、归属、显式释放契约、同线程限制 |
| `docs/reviews/PH-8/S-04-advocate.md`、`S-04-rebuttal.md`、`S-04-ruling.md` | 主张 / 反证 / 裁决 |
| `docs/reviews/PH-8/T1.md` … `T5.md` | 每任务红证与转绿记录 |
| `docs/phases/PH-8/closeout.md`、`docs/session-map.md` | 收口与 phase 登记 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 取消标志 + 三判定点 + 始终安装进度回调 + `42/cancelled=1`；`p8-cancel` | `feat(http): PH8 取消与生命周期`（整相一提交） | revert 后无 `http_request_cancel`，`p8-cancel` 构建失败；取消退化为已被回调中止的 23 | `bash test/run.sh p8-cancel` | M | E-P8-1 |
| T2 | 未取消真值：`send()` / `send_stream()` 均 `cancelled 0`；`p8-not-cancelled` | 同上 | revert 后不装进度回调，语义未变但取消无判定点；未取消断言仍绿，回归守卫 | `bash test/run.sh p8-not-cancelled` | S | E-P8-2 |
| T3 | 生命周期：`send()` 后请求可读、free 计数正确；`p8-lifetime` + `p8-lifetime-bad` | 同上 | revert 后若 send 内部释放，正例 ASAN 红；负例证明检测器开着 | `bash test/run.sh p8-lifetime && bash test/run.sh p8-lifetime-bad` | M | E-P8-3 |
| T4 | CT `cancel()` 映射；`p8-chain` 连接前取消 | 同上 | revert 后 `req->cancel()` 无方法，CT 构建失败 | `bash test/run.sh p8-chain` | S | E-P8-4 |
| T5 | README / `docs/streaming.md` / `g3` / session-map / 收口 / S-04 台账 | 同上 | revert 后 README 无 `cancel()`，E-P8-5 的 grep 红 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` | S | E-P8-5 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P8-1 | `/slow4` 回调内取消：3 秒内返回，`error()==42`、`is_cancelled()==1`、`is_ok()==0`，中止片不交付 | `bash test/run.sh p8-cancel` 退出 0，输出 `error 42 cancelled 1 ok 0 chunks 1 total 1 status 200 lt3 1` |
| E-P8-2 | 未取消路径两入口均 `is_cancelled()==0`、成功路径 `is_ok()==1` | `bash test/run.sh p8-not-cancelled` 退出 0，输出 `send cancelled 0 ok 1 error 0`、`stream cancelled 0 ok 1 error 0` |
| E-P8-3 | `send()` 后请求字段可读、`easy_cleanup` 在 free 前为 0 后为 1，ASAN 正例 0 报告；负例在 ASAN 下报 `heap-use-after-free` | `bash test/run.sh p8-lifetime && bash test/run.sh p8-lifetime-bad` 均退出 0 |
| E-P8-4 | CT 侧 `req->cancel()` 可达；连接前取消读 `42`、`is_cancelled()==1`、服务端计数 0 | `bash test/run.sh p8-chain` 退出 0，输出 `cancel_error 42 cancelled 1 ok 0 requests 0` |
| E-P8-5 | 门禁全绿；README 写明取消与显式释放契约；S-04 裁决入库 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'cancel()' README.md` 与 `grep -q 'S-04' docs/reviews/PH-8/S-04-ruling.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

本 phase 按 2026-11-15 指令「一相一提交」，红证不再靠相邻红/绿提交，而是**在实现前
用同一个工作区跑夹具并原样记录输出**，再实现后复跑。红证写进
`docs/reviews/PH-8/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <实现后同一命令的输出与退出码>
```

- T1 的红是构建红：`http_request_cancel` 尚不存在，`p8-cancel` 链接失败。
- T2 的红是**回归守卫**：未取消路径在基线上就应为 `cancelled 0`；不伪造红，收口注明。
- T3 的红是负例红：`p8-lifetime` 在基线上即为绿（`send()` 从未释放请求），
  `p8-lifetime-bad` 在基线与实现后都应被 ASAN 判 `heap-use-after-free`——它证明检测器
  真的开着，而不是新行为。
- T4 的红是构建红：`req->cancel()` 无方法。
- T5 的红是 `grep -q 'cancel()' README.md` 失败。
- 本 phase 的取消结论全部来自本地回环真实网络；不使用注入钩子。

## 7. 依赖

- 依赖 CT 登记版本 `v0.44.2`（已发版、tag 已推）；本机门禁实际执行的二进制为
  `ct v0.44.1`，两者差异不影响本 phase 的语法面（未使用 0.44.2 新特性）。
- 系统 libcurl 8.7.1（`CURLOPT_XFERINFOFUNCTION`、`CURLOPT_NOPROGRESS`）。
- 前置：PH-7 收口 `290fa25`；`g0`、`g1`、`g3` 开工前为绿。
- `test/fixtures/p4-server.h` 的 `/slow4`（1 秒 1 字节 ×4）与 `/200`。
- 测试时长目标 <15 秒；取消用例的等待上限由进度回调周期决定，实测约 1 秒。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 取消精度受进度回调周期限制 | `p8-cancel` 实耗约 1 秒 | 判据只断言 `<3s`，不断言毫秒；文档写明「连续有数据时下一次写回调即停，停顿流约 1 秒」 |
| 始终安装进度回调可能影响历史夹具耗时 | `g3` 时长上升 | 进度回调只在传输期间每秒一次，短请求无感知；收口给出 `g3` 实测 |
| 取消与回调中止语义混淆 | 同一片既 cancel 又返回非 0 | 写回调先查标志再调用户回调；优先级写死为取消 > 23，夹具只覆盖单一触发 |
| 取消与 idle 同时触发 | 停顿流上先置取消 | 进度回调先查取消、再判空闲，`timeout_kind` 保持 0 |
| 连接前取消的 `status` 为 0 | `p8-chain` 读到 `status 0` | 对外只断言 `error 42 / cancelled 1 / requests 0`；文档写明未收到响应头 |
| ASAN 负例被当成「实现错误」 | `p8-lifetime-bad` 非 0 退出 | 运行器单独命名（`run_asan_negative_uaf_server`），只看 `heap-use-after-free` 与 rc≠0 |
| 跨线程误用 | 调用方在别的线程调 cancel | README 与头注释写明「同线程」；不做 atomic，不承诺线程安全 |

## 9. 收口动作

1. 逐条跑 E-P8-1…E-P8-5，把命令、原始输出、退出码贴进 `docs/phases/PH-8/closeout.md`。
2. 对照 §4 确认五个任务都有交付物与 verify；红证文档齐全。
3. 检查 §3 每个路径入库，无 `build/` 产物。
4. 偏差只写进收口，不改本卡 §4/§5。
5. 收口单列：取消语义与三判定点、精度与 DNS 边界、优先级、部分数据归属、
   显式释放契约、S-04 裁决摘要、PH-8 套件时长。
6. 版本保持 `0.1.0`；不 push、不打 tag。
7. 报告给出：判据逐条结果、门禁末行、提交号、下一相。
