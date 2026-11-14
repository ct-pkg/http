# PH-1 卡：接口冻结与可编译链

- 编号：PH-1
- phase 目录：`docs/phases/PH-1/`
- version-minor：1
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：ce1d4d0e795d78e45e1408229f2b20fa03bd251141f7004a5c93586fcea2be26
- tasks-sha256：0ed8221c8ffa9341548df4643adf02952d7a21a8c14814f0685d24c9992115c0

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-1（与目录 `docs/phases/PH-1/` 一致） |
| 对应 roadmap | [phase 1](../planning/ct-http-0.1.0-roadmap.md#phase-1--接口冻结与可编译链) |
| 对应 feature | F-12、F-13、F-22（最小门禁部分） |
| 版本 | 0.1.0；本 phase 收口时 `version-minor` 保持 1，PATCH 由 fix 数决定，不写进卡 |
| 不为 | 本卡不裁定 S-01…S-05；受这些项影响的能力一律不进本 phase |

## 1. 目标

把公开面、C 结构体布局、三段静态映射和样例冻结成一条能编译、能运行的最小链路；不接网络、不接真 libcurl。

第三人复述：调用方 `import http::http;` 后，能写 `"url"->post()->header(…)->timeout(…)->send()->status()`，`ct build` 零警告通过，运行时输出固定值。

## 2. 范围

### 2.1 做什么

- `src/http/http.shim.h`：`struct HttpRequest` / `struct HttpResponse` 布局 + 固定值 stub。
- `src/http/http.m.ct`：`pub type CRequest/CResponse`、三个静态 trait、三段 `map trait … for type …`，接收者分别为 `char *`、`CRequest *`、`CResponse *`。
- `src/http/Types.h.ct`：独立 base 的常量单元，供“只 import 类型单元”的负例使用。
- `test/fixtures/`：链式正例、变量落点正例、只 import 类型单元的负例、shim ABI 夹具。
- `tools/check.sh` 的 `g1`（卡片十项 + 判据冻结）与 `g3`（构建 + 运行 + 负例）。
- `test/gates/g1_negative.sh`：g1 的负例自测，必须能让 g1 真的 FAIL。
- `README.md` 的接口约定（含“只 import `.h.ct` 没有方法”）。

### 2.2 不做什么

- 不发请求、不链接 libcurl、不实现释放语义、不做 ops 运行时替换。
- 不引入 `method()`（S-02）、响应头读取（S-01）、`max_redirects`（S-03）、跨线程取消（S-04）、`HTTP_ERR_*`（S-05）。
- 不改 `docs/requirements.md`、`docs/design.md`。
- 不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | 两个 struct 布局 + `http_request_*` / `http_response_*` 固定值 stub |
| `src/http/http.m.ct` | 公开映射单元：三个 trait + 三段 `map trait` |
| `src/http/Types.h.ct` | 独立 base 的常量单元（`pub enum HttpDefaults`） |
| `test/fixtures/phase1_chain.ct` | 四跳链正例 |
| `test/fixtures/phase1_chain.expected.out` | 链式正例期望输出 |
| `test/fixtures/phase1_var.ct` | 变量落点正例，同时 import 两个单元 |
| `test/fixtures/phase1_var.expected.out` | 变量落点期望输出 |
| `test/fixtures/phase1_no_mapping.ct` | 只 `import http::Types;` 的负例 |
| `test/fixtures/phase1_abi.c` | shim 布局与 stub 的 C 断言夹具 |
| `test/fixtures/phase1_abi.expected.out` | ABI 夹具期望输出 |
| `test/gates/g1_negative.sh` | g1 负例自测（篡改判据 / effort=L / 缺段） |
| `test/run.sh` | 四个 phase 1 夹具的构建、运行、比对 |
| `tools/check.sh` | `g1` 与 `g3` 实现 |
| `README.md` | 接口约定与门禁说明 |
| `docs/session-map.md` | PH-1 会话登记 |
| `docs/phases/PH-1/red/T1.md` … `T6.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | `tools/check.sh` 的 `g1` 分支与 `test/gates/g1_negative.sh` | `test(gate): PH1-T1 红` + `feat(gate): PH1-T1 g1 卡片门禁` | `git revert` 实现提交后删除 `g1)` 分支与自测脚本，`g0` 不受影响 | `bash tools/check.sh g1 && bash test/gates/g1_negative.sh` | M | E-P1-1 |
| T2 | `src/http/http.shim.h` 与 `test/fixtures/phase1_abi.c` | `test(http): PH1-T2 红` + `feat(http): PH1-T2 shim 布局` | `git revert` 实现提交后删除 shim 头，其余夹具不被引用 | `bash test/run.sh phase1_abi` | S | E-P1-6 |
| T3 | `src/http/http.m.ct`、`src/http/Types.h.ct` 与链式/变量夹具 | `test(http): PH1-T3 红` + `feat(http): PH1-T3 静态映射链` | `git revert` 实现提交后删除两个 CT 单元，shim 与门禁不受影响 | `bash test/run.sh phase1_chain && bash test/run.sh phase1_var` | M | E-P1-3 |
| T4 | `tools/check.sh` 的 `g3` 正例分支 | `test(gate): PH1-T4 红` + `feat(gate): PH1-T4 g3 正例` | `git revert` 实现提交后 `g3` 回到退出 2，其余门禁不变 | `bash tools/check.sh g3` | S | E-P1-2 |
| T5 | `test/fixtures/phase1_no_mapping.ct` 与 `g3` 负例断言 | `test(http): PH1-T5 红` + `feat(gate): PH1-T5 g3 负例` | `git revert` 实现提交后负例夹具不再被 `g3` 调用 | `bash test/run.sh phase1_no_mapping` | S | E-P1-4 |
| T6 | `README.md` 接口约定与 `docs/session-map.md` | `test(docs): PH1-T6 红` + `docs: PH1-T6 接口约定` | `git revert` 文档提交后 README 回到旧口径，不影响构建 | `bash tools/check.sh g0 && grep -q 'import http::http' README.md` | S | E-P1-5 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P1-1 | 卡片十项齐全、任务表七列齐全、判据摘要匹配；篡改判据后门禁必须 FAIL | `bash tools/check.sh g1` 退出 0；`bash test/gates/g1_negative.sh` 退出 0，且其内部篡改跑出 `FAIL g1` 与退出 1 |
| E-P1-2 | 正例构建零警告、运行输出逐字节匹配；`g3` 汇总退出 0 | `bash tools/check.sh g3` 退出 0；日志无 `warning:`；`diff` 无输出 |
| E-P1-3 | 四跳链与变量落点两种形式都 `ct build` 通过 | `bash test/run.sh phase1_chain` 与 `bash test/run.sh phase1_var` 均退出 0 |
| E-P1-4 | 只 import 类型单元时方法不存在，错误文案含指定串 | `bash test/run.sh phase1_no_mapping` 退出 0，且其内部 build 退出非 0 并含 `no method 'post' for type 'char *'` |
| E-P1-5 | 规约纳管门禁保持绿，README 写有唯一正确的 import 方式 | `bash tools/check.sh g0` 退出 0；`grep -q 'import http::http' README.md` 退出 0 |
| E-P1-6 | shim 布局与固定值 stub 可被 C 断言夹具验证 | `bash test/run.sh phase1_abi` 退出 0（`cc -Wall -Wextra -Werror` 零警告） |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。红证写进 `docs/phases/PH-1/red/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha> 后同命令退出 0
```

- 红证必须用 `baseline` 提交复跑，不能拿工作树当前产物冒充。
- `g1` 的负例不是“未实现退出 2”，而是 `test/gates/g1_negative.sh` 对三份篡改卡的拒绝；该脚本自身在实现前必须红。
- `g3` 的负例是 `phase1_no_mapping.ct` 的真实编译失败，不能只靠人工目测。
- 纯文档任务 T6 的“红”是 `grep` 断言失败（README 尚未写接口约定），不是“没有代码改动”。

## 7. 依赖

- 编译器：`ct v0.44.1`（`/Users/samxx/.local/bin/ct`），需支持 `map trait … for type …` 与指针宿主 `->`。
- 系统 C 编译器：`cc`，需支持 `-std=c99 -Wall -Wextra -Werror`。
- 前置：无 phase；roadmap 与 feature 清单已在 `2e9c889` 提交。
- 门禁：`tools/check.sh g0` 必须先绿（当前已绿）。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 目录与 namespace 口径未收口 | 把包根从 `src/` 改成 `http/` 时 `import http::http` 解析变化 | 本 phase 固定 `src/http/` + `namespace http;`；收口时只挪路径，不改映射 |
| 指针宿主误写成点调用 | 夹具出现 `r.status()` 仍能编译 | `g3` 用 `-Werror`；后续加 `-Werror=incompatible-pointer-types` 负例 |
| 只 import `.h.ct` 被误当成可用 | 使用者报 `no method 'post' for type 'char *'` | README 与所有样例统一写 `import http::http;`；本 phase 用 E-P1-4 钉住 |
| 固定值 stub 被误认为真请求 | 运行输出恰好 `200` 但没有任何网络 | 样例输出与 README 明写 `stub`；真请求在 phase 4 才出现 |
| 卡片摘要回填漂移 | `g1` 报 `criteria-sha256 mismatch` | 只改判据段就重算摘要；任何判据变更视为关闭本 phase 并新开卡 |

## 9. 收口动作

1. 逐条跑 E-P1-1…E-P1-6，把命令、原始输出、退出码贴进 `docs/phases/PH-1/closeout.md`。
2. 对照 §4 任务表，逐个确认“红证提交 → 实现提交”相邻且 `baseline` 是父提交。
3. 检查 §3 交付物清单每个路径都已入库，无 `build/` 产物或临时文件混入。
4. 偏差只写进收口文档，不改本卡 §4/§5；判据如需变更，关闭 PH-1 并新开卡。
5. 版本：保持 `0.1.0`，不移动 MINOR；不 push、不打 tag。
6. 报告一行当前事实：最小调用用 `ct build` 与运行各返回什么、是否真发请求（本 phase 仍是固定值 stub）。
