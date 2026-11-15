# PH-10 卡：跨平台、sanitizer、门禁与发布材料收口

- 编号：PH-10
- phase 目录：`docs/phases/PH-10/`
- version-minor：10
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15，卡已冻结）
- criteria-sha256：8dec6f7592ae8b5ec4044d148f1ee7c7df3f80ddafdd109d7158ab30dcb22688
- tasks-sha256：023b8361a914993ee8e04a764c7885518aaeff6cc9c42748850a6df245cbfe86

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-10（与目录 `docs/phases/PH-10/` 一致） |
| 对应 roadmap | [phase 10](../planning/ct-http-0.1.0-roadmap.md#phase-10--跨平台sanitizer门禁与发布材料收口) |
| 对应 feature | F-21、F-23、F-22（全量） |
| 版本 | `0.1.0`；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 不为 | 不新增功能面；不裁定 S-01/S-02；不 push、不打 tag（用户 2026-11-15 指令） |

## 1. 目标

把 0.1.0 的收口工作做完：把 g2、g4、g5、g6、g7、g8、g9 从「退出码 2 的占位」变成能真跑、
能红能绿的门禁；补齐文档可执行材料（`docs/snippets/`、README 分拆、`applies-to`、CHANGELOG）、
台账（`issues/`）与验收映射（`docs/acceptance-map.md`）；在清洁临时目录里复跑安装与样例。

第三人复述：`bash tools/check.sh g2` 到 `g9` 都退出 0 且各自有拒绝坏输入的负例；
`docs/snippets/*.ct` 由 g4 编译、能跑的都跑；README ≤ 300 行且结构符合复杂模块；
`ct.pkg#version` 与 README 都是 `0.1.0`；安装样例在临时目录里输出 `200`；
macOS 全量夹具 0 失败；Linux 若本机无容器则如实登记为不可执行项。

## 2. 范围

### 2.1 做什么

- `tools/gates/g2.sh` … `g9.sh` 与 `tools/check.sh` 的分派；每个门禁配 `test/gates/g<n>_negative.sh`。
- `docs/acceptance-map.md`：R-01…R-19 → `ACC-*`，每条挂可核对命令。
- `docs/snippets/`：README 与教程里的代码块唯一来源；可运行的正例带 `.expected.out`。
- README 分拆为入口页 + `docs/tutorial/NN-*.md`（DOC-S1/S2/S5/S6）。
- `CHANGELOG.md`、`issues/HTTP-*.md`、`docs/errata.md` 的登记面。
- `test/gates/p10_install.sh`：清洁临时目录 `ct get` → `ct install` → 样例 `200`。
- PH-10 收口文档与逐任务红证（`docs/reviews/PH-10/T*.md`）。

### 2.2 不做什么

- 不改任何已冻结的退出判据或任务清单（含 PH-9）；已冻结判据如有错，只走 `docs/errata.md`。
- 不降低任何既有门禁的判定阈值；不为了让门禁变绿而改判定逻辑。
- 不新增公开 API、不改 `src/http/` 的运行时行为。
- 不 push、不打 tag、不改远端；tag 只出提案。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `tools/gates/g2.sh` … `g9.sh` | 七道门禁的实现 |
| `tools/check.sh` | 子命令分派；`g1-errata` 已在前一提交落地 |
| `test/gates/g0_negative.sh` … `g9_negative.sh` | 每道门禁的拒绝能力证明 |
| `docs/acceptance-map.md` | R-01…R-19 的 `ACC-*` 映射 |
| `docs/snippets/` | 代码块唯一来源；`.ct` 正例、`.sh` 命令块 |
| `docs/tutorial/01-concepts.md` … `10-build.md` | README 分拆后的教程 |
| `README.md` | 入口页：定位、安装、最小示例、API 索引、教程、限制、版本 |
| `CHANGELOG.md` | 版本条目（`0.1.0`） |
| `issues/HTTP-1.md`、`issues/HTTP-2.md` | phase 3 请求头缺陷；`ct install` 的 `-I` 布局缺口 |
| `test/gates/p10_install.sh` | 清洁目录安装 + 样例 200 |
| `docs/phases/PH-10/closeout.md` | 逐条判据、门禁输出、未满足项 |
| `docs/reviews/PH-10/T1.md` … `T9.md` | 逐任务红证（baseline `67c5b2f`） |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | `docs/acceptance-map.md` + `tools/gates/g2.sh` + `test/gates/g2_negative.sh` | `feat(gate): PH10-T1 验收映射` | revert 后 g2 回到退出 2，映射表无人校验 | `bash tools/check.sh g2` | M | E-P10-2 |
| T2 | `tools/gates/g4.sh` + `docs/snippets/` + `test/gates/g4_negative.sh` | `feat(gate): PH10-T2 文档可执行` | revert 后 g4 退出 2，README 402 行不再被拦 | `bash tools/check.sh g4` | M | E-P10-3 |
| T3 | `tools/gates/g5.sh`、`tools/gates/g6.sh` + 两个负例 | `feat(gate): PH10-T3 自证与台账` | revert 后 g5/g6 退出 2，缺红证与坏状态词不再被拦 | `bash tools/check.sh g5 && bash tools/check.sh g6` | M | E-P10-2 |
| T4 | `tools/gates/g7.sh`、`g8.sh`、`g9.sh` + 三个负例 | `feat(gate): PH10-T4 安全与提交` | revert 后三道门禁退回退出 2 | `bash tools/check.sh g7 && bash tools/check.sh g8 && bash tools/check.sh g9` | M | E-P10-2 |
| T5 | README 入口页 + `docs/tutorial/` + `applies-to` | `docs: PH10-T5 README 分拆与教程` | revert 后 README 回到 402 行，g4-size 红 | `test -f docs/tutorial/01-concepts.md && bash tools/check.sh g4` | M | E-P10-3 |
| T6 | `issues/`、`CHANGELOG.md`、`AGENTS.md` 落点与偏离登记 | `docs: PH10-T6 台账与发布材料` | revert 后 g6 无台账、g4-version 无 CHANGELOG | `bash tools/check.sh g6` | M | E-P10-2 |
| T7 | `test/gates/p10_install.sh` + README 安装节 | `test(gate): PH10-T7 安装路径` | revert 后安装样例不可复跑 | `bash test/gates/p10_install.sh` | S | E-P10-4 |
| T8 | macOS 全量夹具记录 + Linux 不可执行项登记 | `docs: PH10-T8 跨平台记录` | revert 后 E-P10-1 只剩断言没有记录 | `bash tools/check.sh g3` | M | E-P10-1 |
| T9 | 收口文档、逐任务红证、tag 提案 | `docs: PH10-T9 收口` | revert 后收口对照表缺失 | `bash tools/check.sh g4 && bash tools/check.sh g8` | S | E-P10-5 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P10-1 | macOS 全量夹具 0 失败；Linux 在容器内复跑，无容器则登记为不可执行项并给出补跑命令 | `bash tools/check.sh g3` 退出 0（本机 macOS）；Linux 复跑命令 `bash tools/check.sh g3`（容器内），本机 `command -v podman docker` 均无命中，登记见收口 §4.1 |
| E-P10-2 | `g0`…`g9` 全部真跑并退出 0；未实现的子命令不得返回 0 | `for g in g0 g1 g2 g3 g4 g5 g6 g7 g8 g9; do bash tools/check.sh $g; done` 每道退出 0 |
| E-P10-3 | g4 编译 `docs/snippets/*.ct` 正例，并拒绝实现链偏离（负例打印 rejected） | `bash tools/check.sh g4` 退出 0；`bash test/gates/g4_negative.sh` 退出 0 且含 `rejected snippet/impl-chain` |
| E-P10-4 | 清洁临时目录 `ct get` → `ct install` → 样例输出 `200`；裸名 `http@^0.1.0` 需已发布索引与 tag，本机以本地目录 spec 复跑并登记差异 | `bash test/gates/p10_install.sh` 退出 0，输出 `install ok` 与 `200` |
| E-P10-5 | `ct.pkg#version` 与 README 版本行均为 `0.1.0`，CHANGELOG 首条一致；tag 只出提案 | `bash tools/check.sh g4`（`g4-version`）退出 0；`git tag` 为空且收口 §5 给出 tag 提案 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

统一基线：`67c5b2f`（本卡开卡前的最后一次提交）。每个任务在 `docs/reviews/PH-10/T<n>.md`
记录：

```text
baseline: 67c5b2f
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <实现后同命令输出与退出码>
```

红证必须来自基线复跑，不用工作树当前产物冒充。门禁任务的「红」是基线对该子命令退出 2；
文档任务的「红」是基线缺文件或 g4 因超限/未绑定而失败。每道门禁另有副本负例
（`test/gates/g<n>_negative.sh`），篡改数据后必须打印 `rejected` 并退出非 0。

## 7. 依赖

- `ct v0.44.1`（`/Users/samxx/.local/bin/ct`）、系统 `cc`、系统 libcurl 8.7.1。
- 主规范：`/Users/samxx/trae-ws/.ai-engineering/methodology/ai-coding-workflow.md`（1.9.0）。
- 前置：PH-9 收口 `d0160db`；`g0`/`g1`/`g3` 开工前为绿。
- Linux 复跑需要 podman/docker：本机 `command -v podman` 与 `command -v docker` 均退出 1。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 本机无 Linux 容器 | `podman`/`docker` 不可用 | 不冒充通过：登记为不可执行项，给出容器内补跑命令与 `uname` 要求 |
| README 与两份 S-05 记录超过 300 行 | `g4-size` 报 402/403/470 | 按 DOC-S2/S3 拆成入口页、教程与续篇；不改内容含义 |
| `ct install` 打印 `-I vendor/http`，与 `src/` 布局不符 | 消费端 `cannot resolve import` | 如实立 `issues/HTTP-2.md`；本地用 `-I vendor/http/src` 复跑并登记 |
| 历史提交跨目录（整相一提交） | `g8-one-thing` 对全历史变红 | 适用范围从 `docs/phases/PH-10/base` 起算，历史不追溯（T-7 同口径） |
| 用户禁止 tag/push | 收口无 tag | 偏离项 `V-002` 登记；`g8-tag` 打印 deferred 而不是伪造 tag |
| 门禁变严暴露既有文档问题 | 早期文档链接/锚点失效 | 修文档或补锚点，不降低门禁阈值 |

## 9. 收口动作

1. 逐条跑 E-P10-1…E-P10-5，把命令、原始输出、退出码贴进 `docs/phases/PH-10/closeout.md`。
2. 对照 §4 逐个确认红证与实现相邻可追溯，`baseline` 为 `67c5b2f` 且是 `HEAD` 祖先。
3. 逐道门禁贴 `ok gN` 与对应 `gN_negative.sh` 的 `rejected` 行。
4. 未满足项逐条列在收口 §4，不计通过；不得用「跳过」冒充通过。
5. 版本：`0.1.0` 不变；tag 只出提案，不执行。
6. 报告一行当前事实：门禁覆盖到哪、哪些判据未达成、Linux 与安装路径的差异。
