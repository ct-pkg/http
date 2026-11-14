# PH-6 卡：三种超时区分与慢流保护

- 编号：PH-6
- phase 目录：`docs/phases/PH-6/`
- version-minor：6
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：0b8e87acee6b16229969100306b0211e387f1ec6e32f9d93e226358bebbc1812
- tasks-sha256：849f1ef3307d2b7d200d6aaacd62414c850bee5e457e1e511fe9b7f520cb3b96

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-6（与目录 `docs/phases/PH-6/` 一致） |
| 对应 roadmap | [phase 6](../planning/ct-http-0.1.0-roadmap.md#phase-6--三种超时区分) |
| 对应 feature | F-02 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 已裁定 | S-05：不引入枚举，`timeout_kind` 用 `int` + 宏 |
| 不为 | 不引入 `HTTP_ERR_*`；不做流式（PH-7）；不做取消（PH-8）；不改 `docs/requirements.md` / `docs/design.md` |

## 1. 目标

`CURLE_OPERATION_TIMEDOUT`(28) 只有一个值，区分由 shim 自己造：结果里给出
`timeout_kind()` ∈ {NONE, CONNECT, TOTAL, IDLE}，`error()` 仍是 28（原值透传不变），
`error_detail()` 追加 `phase=`。空闲判定用进度回调的空档，不用 `LOW_SPEED_*`，
合法慢流不被误杀。

第三人复述：总超时用例读 `kind 2` 且 detail 含 `phase=total`；连接超时用例读 `kind 1`
且 `phase=connect`；空闲用例读 `kind 3` 且 `phase=idle`；1 字节/200ms 的慢流读完 5 字节、
`kind 0`、`is_ok 1`；`idle_timeout(-1)` 与 `timeout(-1)` 都返回 `-1005` 且服务端计数 0。

## 2. 范围

### 2.1 做什么

- `http_request_idle_timeout(r, ms)` + CT `.idle_timeout()`；负值报 `HTTP_LOCAL_ERR_TIMEOUT`。
- `http_response_timeout_kind(r)` + CT `.timeout_kind()`；宏 `HTTP_TIMEOUT_NONE/CONNECT/TOTAL/IDLE`（0/1/2/3）。
- `HttpResponse.timeout_kind`；`error_detail` 在超时时追加 ` phase=connect|total|idle`。
- 进度回调记录最后一次 activity 时间；`idle_timeout > 0` 时才安装，空档超限返回 1 中止，结果归一到 28 + `IDLE`。
- `test/fixtures/p4-server.c` 增加 `/stall`（发 1 字节后停 3 秒）与 `/slow`（1 字节/200ms，共 5 字节）。
- 夹具 `p6-total`、`p6-connect`、`p6-idle`、`p6-slow`、`p6-negative`。
- `README.md`、`tools/check.sh` 的 `g3`、`docs/session-map.md`；红证进 `docs/reviews/PH-6/`。

### 2.2 不做什么

- 不用 `CURLOPT_LOW_SPEED_LIMIT` / `LOW_SPEED_TIME`：它按窗口平均速度判定，会把
  「1 字节 / 59 秒」这类合法慢流判死。
- 不新增与 `-1001…-1006` 冲突的本地码；超时仍是一个 `28`。
- 不做流式 `send_stream`、不做取消、不做 DNS 阶段取消。
- 不改 `docs/requirements.md`、`docs/design.md`；若必须改 R-14 / R-11 才能落地，停下报告。
- 不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | `timeout_kind` 字段与四个宏、两个新函数 |
| `src/http/http.shim.c` | 阶段判定、进度回调、detail 追加 `phase=`、负值校验 |
| `src/http/http.m.ct` | `idle_timeout()`、`timeout_kind()` |
| `test/fixtures/p4-server.c` | `/stall`、`/slow` 端点 |
| `test/fixtures/p6-total.c` | 总超时 |
| `test/fixtures/p6-connect.c` | 连接超时 |
| `test/fixtures/p6-idle.c` | 空闲超时 |
| `test/fixtures/p6-slow.c` | 合法慢流不被误杀 |
| `test/fixtures/p6-negative.c` | 负超时本地错误 + 服务端计数 0 |
| `test/run.sh`、`tools/check.sh` | 五个夹具接线并入 `g3` |
| `README.md`、`docs/session-map.md` | 超时面、精度与误判、PH-6 登记 |
| `docs/reviews/PH-6/T1.md` … `T6.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | `timeout_kind` 字段/宏/读取、detail 追加 `phase=`、总超时分类、`p6-total.c` | `test(http): PH6-T1 红` + `feat(http): PH6-T1 总超时` | revert 实现提交后 `timeout_kind` 消失，夹具构建失败 | `bash test/run.sh p6-total` | M | E-P6-1 |
| T2 | 连接超时分类（连接未建立 + 配置命中）、`p6-connect.c` | `test(http): PH6-T2 红` + `feat(http): PH6-T2 连接超时` | revert 实现提交后分类落到 total，夹具断言红 | `bash test/run.sh p6-connect` | S | E-P6-2 |
| T3 | `idle_timeout()` API、进度回调记时（不中止）、`/slow` 与 `/stall` 端点、`p6-slow.c` | `test(http): PH6-T3 红` + `feat(http): PH6-T3 慢流不误杀` | revert 实现提交后 `idle_timeout` 消失，夹具构建失败 | `bash test/run.sh p6-slow` | M | E-P6-4 |
| T4 | 空档中止、idle 归一化到 28、`p6-idle.c` | `test(http): PH6-T4 红` + `feat(http): PH6-T4 空闲超时` | revert 实现提交后 `/stall` 走完 3 秒，输出 `error 18 kind 0`，断言红 | `bash test/run.sh p6-idle` | M | E-P6-3 |
| T5 | `idle_timeout` 负值校验、`p6-negative.c` | `test(http): PH6-T5 红` + `feat(http): PH6-T5 负超时` | revert 实现提交后 `idle_timeout(-1)` 仍发请求，输出 `error 0`，断言红 | `bash test/run.sh p6-negative` | S | E-P6-5 |
| T6 | README 超时面/精度/误判、`g3` 接线、`docs/session-map.md`、收口 | `test(docs): PH6-T6 红` + `docs: PH6-T6 超时面与收口` | revert 文档提交后 README 回到 PH-5 口径 | `bash tools/check.sh g3 && grep -q 'timeout_kind' README.md` | S | E-P6-6 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P6-1 | 总超时可区分：`/hang` + `timeout(300)`，`error` 仍是 28，`timeout_kind` 为 TOTAL，detail 含 `phase=total`，服务端确实收到请求 | `bash test/run.sh p6-total` 退出 0，输出 `status 0 error 28 kind 2 phase total requests 1` |
| E-P6-2 | 连接超时可区分：`192.0.2.1:81` + `connect_timeout(300)`，`timeout_kind` 为 CONNECT，detail 含 `phase=connect` | `bash test/run.sh p6-connect` 退出 0，输出 `error 28 kind 1 phase connect` |
| E-P6-3 | 空闲超时可区分：`/stall` + `idle_timeout(500)`，约 1 秒内中止，`error` 归一为 28，`kind` 为 IDLE，detail 含 `phase=idle` | `bash test/run.sh p6-idle` 退出 0，输出 `error 28 kind 3 phase idle` |
| E-P6-4 | 合法慢流不被误杀：1 字节/200ms 的 5 字节流在 `idle_timeout(500)` 下完整读完 | `bash test/run.sh p6-slow` 退出 0，输出 `status 200 body_len 5 kind 0 ok 1` |
| E-P6-5 | 负超时是本地错误且不发请求：`idle_timeout(-1)` 与 `timeout(-1)` 都读 `-1005`，服务端计数 0 | `bash test/run.sh p6-negative` 退出 0，输出 `idle -1005 timeout -1005 requests 0` |
| E-P6-6 | 门禁全绿；README 写明 `timeout_kind` 的调用形态、判定时点、精度与可能误判 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'timeout_kind' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。
红证写进 `docs/reviews/PH-6/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha 或提交名> 后同命令退出 0
```

- T1/T3 的红是构建红：读取函数或 `idle_timeout` 尚不存在。
- T2/T4/T5 的红是行为红：夹具能构建，但分类/中止/校验缺失导致输出不匹配。
- T6 的红是 `grep -q 'timeout_kind' README.md` 失败。
- 本 phase 的超时结论全部来自真实网络（本地服务端 + 保留地址），**不使用注入钩子**。

## 7. 依赖

- `ct v0.44.1`、系统 libcurl 8.7.1（`CURLINFO_CONNECT_TIME_T`、`CURLOPT_XFERINFOFUNCTION` 可用）。
- 前置：PH-5 收口 `723d00b`；`g0`、`g1`、`g3` 开工前为绿。
- `test/fixtures/p4-server.h` 的本地服务端；`192.0.2.1`（TEST-NET-1，保留地址）用于连接超时。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 连接超时依赖保留地址被丢弃 | 环境直接返回 `7`（ENETUNREACH）而不是 `28` | 夹具会明确红，不会静默通过；若换环境失败，改用注入并把结论标为注入验证（收口登记） |
| 进度回调约 1 秒一次，idle 精度有限 | `idle_timeout(500)` 实测约 1.0 秒才中止 | 判据只断言 `kind/phase`，不断言毫秒级时长；精度写进 README 与收口 |
| idle 与 total 同时可触发 | 两者都配置且相等 | 内部机制不同（idle 中止返回 42，total 返回 28），按返回码定优先级；收口写明 |
| idle 归一会掩盖真实 42 | 有人把回调中止也当 idle | 只有 shim 自己置 `idle_fired` 才归一；PH-7 的调用方中止仍读 23/42 |
| 慢流夹具压缩了节奏 | 1 字节/200ms 不是 1 字节/59 秒 | 语义等价（持续有数据），比例压缩只为测试时长；收口写明 |
| `/stall` 子进程睡 3 秒 | 夹具结束时子进程仍在睡 | `p4_server_stop` 发 `SIGTERM` 回收，不留孤儿 |
| 分类误判 | 连接已建立但 TLS 握手挂起 | `CURLINFO_CONNECT_TIME_T` 含 TLS，未完成时为 0，归 CONNECT；DNS 同样计入连接阶段 |

## 9. 收口动作

1. 逐条跑 E-P6-1…E-P6-6，把命令、原始输出、退出码贴进 `docs/phases/PH-6/closeout.md`。
2. 对照 §4 确认「红证提交 → 实现提交」相邻且 baseline 是父提交。
3. 检查 §3 每个路径入库，无 `build/` 产物。
4. 偏差只写进收口，不改本卡 §4/§5。
5. 收口单列：判定时点与精度、可能误判、慢流比例压缩、哪些结论是真实网络验证（本 phase 全部）。
6. 版本保持 `0.1.0`；不 push、不打 tag。
7. 报告给出：整套超时用例的实际执行时长、慢流用例结果、调用点示例。
