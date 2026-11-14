# PH-5 卡：错误模型与谓词真值表

- 编号：PH-5
- phase 目录：`docs/phases/PH-5/`
- version-minor：5
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：bde4120ad6601d7b47f00f48501b11a687c0f26f36efd2b6a03c5edfc1a2f2ef
- tasks-sha256：f0b2b5e2a8bbe086c1a87e69e4738d13bf005631b2b1ef81300ef97da836b740

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-5（与目录 `docs/phases/PH-5/` 一致） |
| 对应 roadmap | [phase 5](../planning/ct-http-0.1.0-roadmap.md#phase-5--错误模型与谓词真值表) |
| 对应 feature | F-17、F-05 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 已裁定 | S-05：**不纳入** `HTTP_ERR_*` 枚举；错误面用 `int` + `http.shim.h` 命名常量 |
| 不为 | 不引入枚举；不改 `docs/requirements.md` / `docs/design.md`；不做 `method()` / 响应头 / `max_redirects` / 跨线程取消 / 流式 |

## 1. 目标

把三类错误拆开并能被调用方读出：传输错误 = `CURLcode` 原值（正数），HTTP 4xx/5xx =
`error_code == 0`，本地参数错误 = 负数码；`error_msg` 带主机与阶段，`error_detail` 可复现。
`is_ok()` 与 `is_cancelled()` 由构造保证互斥，并用四态矩阵实测回答。

第三人复述：本地错误六类（ARG / HEADER / BODY / SCHEME / TIMEOUT / NO_HANDLE）各自能读出
`-1001…-1006`；连接拒绝读 7，注入 6/28/60/42/23 逐项相等且不被归类；500 时
`error_code==0`、`is_ok()==0`；未知码 9999 原样保留；200 注入取消位后
`is_ok()==0 && is_cancelled()==1`，同一份 200/0 数据下两者不可能同时为 1。

## 2. 范围

### 2.1 做什么

- `src/http/http.shim.h`：`HTTP_LOCAL_ERR_NO_HANDLE`；响应新增 `error` / `error_msg` / `error_detail` / `cancelled`；读取函数 `http_response_error` / `error_msg` / `error_detail` / `is_ok` / `is_cancelled`；测试钩子 `http_shim_inject_curl_code`。
- `src/http/http.shim.c`：`real_send` 填错误字段；`NO_HANDLE` 不再静默；阶段映射；detail 固定格式；释放回收两个字符串。
- `src/http/http.m.ct`：`ResponseView` 增加五个方法。
- 夹具：`p5-local`、`p5-table`、`p5-http-error`、`p5-unknown`、`p5-predicates`。
- 既有 fake 响应夹具（`phase1_abi.c`、`p2-ops.c`）加 `memset`，保证新指针字段不会喂给 `free`。
- `test/run.sh`、`tools/check.sh` 的 `g3`、`README.md`、`docs/session-map.md`。
- 红证写入 `docs/reviews/PH-5/T<n>.md`。

### 2.2 不做什么

- **不加 `HTTP_ERR_*` 枚举**（S-05 已裁定不纳入）。常量就是 `HTTP_LOCAL_ERR_*` 宏。
- 不做取消功能（S-04 / phase 8）：`cancel()` 不存在，`is_cancelled()` 只能由响应字段证明。
- 不做重试分类、不做本地化文案、不全量导出 libcurl enum。
- 不做流式、不读响应头、不做 `method()` / `max_redirects`。
- 不改 `docs/requirements.md`、`docs/design.md`；若落地必须改 R-11 / R-14，停下报告冲突与影响面。
- 不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | 六个本地码、响应错误字段、五个读取函数、注入钩子 |
| `src/http/http.shim.c` | 错误填充、阶段映射、detail 生成、NO_HANDLE 可观察 |
| `src/http/http.m.ct` | `ResponseView` 五个新方法 |
| `test/fixtures/p5-local.c` + expected | 六个本地码 |
| `test/fixtures/p5-table.c` + expected | 真实 7 与注入 6/28/60/42/23 |
| `test/fixtures/p5-http-error.c` + expected | 500 不算传输错误 |
| `test/fixtures/p5-unknown.c` + expected | 9999 原值保留 |
| `test/fixtures/p5-predicates.c` + expected | 四态矩阵 |
| `test/fixtures/phase1_abi.c`、`p2-ops.c` | fake 响应零初始化 |
| `test/run.sh`、`tools/check.sh` | 五个夹具接线并入 `g3` |
| `README.md`、`docs/session-map.md` | 错误面示例、真值表、PH-5 登记 |
| `docs/reviews/PH-5/T1.md` … `T5.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 响应 `error` 字段、六个本地码、NO_HANDLE 可观察、`p5-local.c`、fake 响应零初始化 | `test(http): PH5-T1 红` + `feat(http): PH5-T1 本地错误面` | revert 实现提交后 `http_response_error` 消失，夹具构建失败 | `bash test/run.sh p5-local` | M | E-P5-1 |
| T2 | 传输错误映射、`error_msg` / `error_detail`、注入钩子、`p5-table.c`、`p5-http-error.c` | `test(http): PH5-T2 红` + `feat(http): PH5-T2 三类不混` | revert 实现提交后两个夹具构建失败 | `bash test/run.sh p5-table && bash test/run.sh p5-http-error` | M | E-P5-2 |
| T3 | 未知码保留、`p5-unknown.c` | `test(http): PH5-T3 红` + `feat(http): PH5-T3 未知码` | revert 实现提交后夹具读不到未知码 | `bash test/run.sh p5-unknown` | S | E-P5-3 |
| T4 | `is_ok` / `is_cancelled` 互斥、取消位、`p5-predicates.c`、CT 五个方法 | `test(http): PH5-T4 红` + `feat(http): PH5-T4 谓词矩阵` | revert 实现提交后谓词消失，夹具构建失败 | `bash test/run.sh p5-predicates` | M | E-P5-4 |
| T5 | README 错误面与真值表、`g3` 接线、`docs/session-map.md`、收口 | `test(docs): PH5-T5 红` + `docs: PH5-T5 错误面与收口` | revert 文档提交后 README 回到 PH-4 口径 | `bash tools/check.sh g3 && grep -q 'is_cancelled' README.md` | S | E-P5-5 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P5-1 | 本地错误六类各自可读：ARG / HEADER / BODY / SCHEME / TIMEOUT / NO_HANDLE 对应 -1001…-1006，且 NO_HANDLE 不再静默 | `bash test/run.sh p5-local` 退出 0，输出 `arg -1001`、`header -1002`、`body -1003`、`scheme -1004`、`timeout -1005`、`no_handle -1006` |
| E-P5-2 | 三类错误不混：拒绝连接读 7，注入 6/28/60/42/23 逐项相等；500 时 `error_code==0`、`is_ok()==0`；`error_msg` 非空且含主机与阶段 | `bash test/run.sh p5-table` 退出 0，输出 `real 7`、`inject 6`、`inject 28`、`inject 60`、`inject 42`、`inject 23`；`bash test/run.sh p5-http-error` 退出 0，输出 `status 500 error 0 ok 0` |
| E-P5-3 | 未知 `CURLcode` 原值保留，不归零 | `bash test/run.sh p5-unknown` 退出 0，输出 `error 9999` |
| E-P5-4 | 四态矩阵实测；`is_ok` 与 `is_cancelled` 不可能同时为真，且由 `is_ok` 的判定条件保证 | `bash test/run.sh p5-predicates` 退出 0，输出 `ok 200 0 1 0`、`http404 404 0 0 0`、`transport 0 7 0 0`、`cancelled 200 0 0 1`（列序 status error is_ok is_cancelled） |
| E-P5-5 | 门禁全绿；README 写明错误面与真值表结论 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'is_cancelled' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。
红证写进 `docs/reviews/PH-5/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha 或提交名> 后同命令退出 0
```

- T1/T2/T3/T4 的红是构建红：夹具引用尚不存在的读取函数或钩子，`cc -Werror` 失败。
- T5 的红是 `grep -q 'is_cancelled' README.md` 失败。
- 判据里的每条数字都必须是夹具打印出来的，不允许在文档里「推算」。
- 注入钩子只用于无法离线稳定复现的码（6/60/42/23）；7 走真实拒绝，28 在 PH-4 已有端到端证据。

## 7. 依赖

- `ct v0.44.1`、系统 libcurl 8.7.1。
- 前置：PH-4 收口 `99f0881`；`g0`、`g1`、`g3` 开工前为绿。
- `test/fixtures/p4-server.h` 的本地服务端（`/200`、`/404`、`/500`、`/hang`）。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 注入钩子只能证明映射表 | 6/60/42/23 不是 libcurl 真实产生 | 真实 7（拒绝）与 28（超时）分别由本 phase 与 PH-4 端到端证明；注入只补离线不可复现的码，收口登记偏差 |
| 取消路径不存在，取消态是「造」出来的 | `is_cancelled` 恒 0，矩阵像空转 | 夹具直接置响应 `cancelled` 位，证明 `is_ok` 的判定条件本身排除取消；真实取消行为留 PH-8 |
| fake 响应夹具不初始化新字段 | `free` 收到野指针，ASAN 报错 | `phase1_abi.c`、`p2-ops.c` 在红提交里先 `memset` |
| `error_msg` 文案带主机，可能含敏感信息 | 日志出现完整 URL | 只写 scheme://host:port，不写 query；格式固定、不做本地化 |
| 未知码与本地负码空间冲突 | 本地码必须为负，CURLcode 为正 | 本地码全部 `-1001…-1006`，判定用 `error < 0` 区分 |
| 落地需改 R-11 / R-14 | 判据要求与条款冲突 | 停下报告，不自行改 requirements |
| `-Wswitch` 类负债 | 有人想把码改成枚举 | 本卡与 README 明确写死 int + 宏，S-05 已裁定 |

## 9. 收口动作

1. 逐条跑 E-P5-1…E-P5-5，把命令、原始输出、退出码贴进 `docs/phases/PH-5/closeout.md`。
2. 对照 §4 确认「红证提交 → 实现提交」相邻且 baseline 是父提交。
3. 检查 §3 每个路径入库，无 `build/` 产物。
4. 偏差只写进收口，不改本卡 §4/§5。
5. 收口单列：错误面最终调用示例、四态真值表实测结果、注入钩子的边界、`is_ok`/`is_cancelled` 的结论（改语义还是改文档）。
6. 版本保持 `0.1.0`；不 push、不打 tag。
7. 报告一行当前事实：最小请求 `ct build` 与运行各返回什么、错误面读到什么。
