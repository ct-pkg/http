# PH-11 卡：0.2.0 发布相（编译器下限、applies-to、标签口径）

- 编号：PH-11
- phase 目录：`docs/phases/PH-11/`
- version-minor：11
- 规范版本：1.10.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：已收口（2026-11-15 打 `v0.2.0`；2026-11-16 以 `v0.2.1` 重发含 `publisher`/`ns-root` 的清单，tag 指向 `1121bbc`）
- criteria-sha256：554150b31eb3966d6cbf7799491cea46f9649b06413178960b0d62e0c1cfba90
- tasks-sha256：799fd6d01a067f49d4e2a1a89a08ee0e4d14d8d4203dfd080093277fcc130884

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-11（与目录 `docs/phases/PH-11/` 一致） |
| 对应 roadmap | [phase 11](../planning/ct-http-0.1.0-roadmap.md#phase-11--020-发布相) |
| 对应 feature | 无新增功能面；只做发布口径 |
| 版本 | `0.2.0`（MINOR=2，按 `deviation: V-001` 的 `d = 9` 换算为 PH-11） |
| 基线 | `937c05d`（`docs/phases/PH-11/base`） |
| 不为 | 不改 `http/` 运行时行为；不改门禁阈值或映射；不删规则凑绿；不移动已发布的 `v0.1.0` |

### 0.1 方法论核对（开卡前完成，结论写死在这里）

- 核对对象：`/Users/samxx/trae-ws/.ai-engineering/methodology/ai-coding-workflow.md`（1.10.0）。
- §6.2「phase 交付流程 / 开卡必填」（第 515–526 行）列出的必填字段是：编号、目标、范围、
  交付物清单、任务清单、退出判据、依赖、规范版本、version-minor、已知风险。
  **全文没有「每个 phase 必须带产品增量」或「不允许纯发布相」的条款。**
- §6.7 V-1（第 666、671 行）：MINOR 与 phase 一一对应，登记了偏离的按偏移换算；
  §6.7 V-7（第 677 行）：phase 收口时建 annotated tag 并推送，分支推送仍须用户指令。
- 结论：**只含发布内容的相合法**。本相有交付物（清单、版本落点、标签）与可核判据，
  不是空壳卡；因此走路线 1（`0.2.0`），不退回路线 2（`0.1.1`）。

## 1. 目标

把 0.1.0 之后积压的三件发布工作收成一次可核对的发版：编译器下限抬到有实测依据的值、
全部用户面文档的 `applies-to` 与新版本对齐、标签口径收口为「annotated tag 指向收口提交」。

第三人复述：`ct.pkg` 写 `requires = ct >= 0.53.1` 且旧下限与旧能力理由全清；
19 个用户面文档的 `applies-to` 全为 `0.2.0`；`ct.pkg#version`、README、CHANGELOG 都是 `0.2.0`；
`v0.2.0` 是 annotated tag 且指向 HEAD、可达；`g0`…`g9` 逐条退出 0。

## 2. 范围

### 2.1 做什么

- `ct.pkg`：`requires` 抬到 `ct >= 0.53.1`，注释改成实测能力依据；`version` 抬到 `0.2.0`。
- 能力文本同步：`docs/design.md`、`docs/requirements.md`、`docs/phases/H1.md`、`README.md`
  里的 `requires` 说明与旧下限引用。
- 19 个用户面文档的 `- applies-to:` 全量改为 `0.2.0`。
- 版本落点与口径：`CHANGELOG.md` 新增 `[0.2.0]` 条目；`AGENTS.md` 的 `deviation: V-001`
  记录收敛（版本冻结解除、偏移永久保留 `d = 9`）；`docs/session-map.md` 加 PH-11 行。
- 收口：`docs/phases/PH-11/closeout.md` 与 `docs/reviews/PH-11/T0.md`…`T5.md`。
- 标签：annotated `v0.2.0` 指向收口提交，按 V-7 四项校验后推送。

### 2.2 不做什么

- 不改 `http/*.ct`、`http/*.h`、`http/*.c` 的任何行为；不新增公开面。
- 不改任何门禁脚本、判定阈值、提交类别映射；不删负例；不用 `--no-verify`。
- 不移动、不删除已发布的 `v0.1.0`；不 `git reset --hard`、不丢 stash。
- 不改 `.ai-engineering` 主规范。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `docs/phases/PH-11/card.md` | 本卡（判据与任务冻结） |
| `docs/phases/PH-11/base` | 基线提交 `937c05d` |
| `ct.pkg` | `requires = ct >= 0.53.1`、`version = 0.2.0`、能力注释 |
| `README.md` | 版本行、`applies-to`、依赖行、当前 phase 卡指向 PH-11 |
| `CHANGELOG.md` | `[0.2.0] - 2026-11-15` 条目 |
| `AGENTS.md` | `deviation: V-001` 的收敛记录（不新增偏离） |
| `docs/design.md`、`docs/requirements.md`、`docs/phases/H1.md` | 编译器下限的能力依据同步 |
| 19 个用户面 `*.md` | `- applies-to: 0.2.0` |
| `docs/phases/PH-11/closeout.md` | 逐条判据、门禁原始输出、tag 四项校验 |
| `docs/reviews/PH-11/T0.md`…`T5.md` | 逐任务红证（基线 `937c05d`） |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T0 | 本卡与 `base`（判据/任务冻结） | `docs: PH11-T0 开卡` | 删卡后 `g4-version` 找不到 PH-11 映射 | `bash tools/check.sh g1` | S | E-P11-3 |
| T1 | `ct.pkg` 下限与四处能力文本 | `docs: PH11-T1 编译器下限` | 回退文本后旧下限引用重新出现 | `grep -c 'requires = ct >= 0.53.1' ct.pkg` 与 `CT_BIN=/tmp/ct-bin-0531/ct bash test/gates/p10_install.sh` | S | E-P11-1 |
| T2 | 19 个 `applies-to` 同步 | `docs: PH11-T2 applies-to 同步` | 回退 19 行后 `g4-applies` 红 | `bash tools/check.sh g4` | S | E-P11-2 |
| T3 | 版本落点 `0.2.0` 与 V-001 收敛 | `docs: PH11-T3 版本落点` | 回退三处版本后 `g4-version`/`g8-version` 红 | `G4_ONLY=version bash tools/check.sh g4` 与 `G8_ONLY=version bash tools/check.sh g8` | S | E-P11-3 |
| T4 | 收口文档、逐任务红证、会话地图 | `docs: PH11-T4 收口与红证` | 删收口与红证后 `g5` 缺证据 | `bash tools/check.sh g5` | S | E-P11-5 |
| T5 | annotated tag `v0.2.0` 指向收口提交 | `docs: PH11-T5 tag 记录`（tag 不在提交内） | 删本地 tag 后 `g8-tag` 红 | `git cat-file -t v0.2.0` 与 `bash tools/check.sh g8` | S | E-P11-4 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P11-1 | `ct.pkg` 的 `requires` 为 `ct >= 0.53.1`，四处能力文本同步，旧下限无残留 | `grep -c 'requires = ct >= 0.53.1' ct.pkg` 输出 `1`；`grep -rn 'ct >= 0.34.0\|ct v0.44.1' ct.pkg docs/design.md docs/requirements.md docs/phases/H1.md README.md` 命中 0 |
| E-P11-2 | 用户面文档 `applies-to` 全等于发布锚 `0.2.0` | `grep -rh '^- applies-to:' README.md docs --include='*.md' \| sort -u` 只输出 `- applies-to: 0.2.0` 一行；`bash tools/check.sh g4` 退出 0 |
| E-P11-3 | 版本落点一致且按偏离换算对上 PH-11：`ct.pkg#version`、README 版本行、CHANGELOG 首条均为 `0.2.0` | `G4_ONLY=version bash tools/check.sh g4` 退出 0；`G8_ONLY=version bash tools/check.sh g8` 退出 0 |
| E-P11-4 | `v0.2.0` 是 annotated tag，指向收口提交，且该提交是 HEAD 祖先 | `git cat-file -t v0.2.0` 输出 `tag`；`git rev-parse v0.2.0^{commit}` 等于 `git rev-parse HEAD`；`git merge-base --is-ancestor v0.2.0 HEAD` 退出 0 |
| E-P11-5 | 十道门禁真跑并退出 0，不得靠放宽阈值变绿 | `for g in g0 g1 g2 g3 g4 g5 g6 g7 g8 g9; do bash tools/check.sh $g \|\| exit 1; done` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

统一基线：`937c05d`（本卡开卡前的最后一次提交，见 `docs/phases/PH-11/base`）。
每个任务在 `docs/reviews/PH-11/T<n>.md` 记录 `baseline:`、verify 命令、原始输出、退出码与转绿输出。

- 文档类任务的「红」：基线命中数为 0 或目标文件不存在（`grep`/`test` 退出 1）。
- 门禁类任务的「红」：基线 `g4-version` / `g8-tag` 退出 1 并打印原始报错。
- 下限类任务的「红」另附能力对照：`ct v0.49.0` 的 `ct install` 打印 `ct-ast build`，
  `p10_install.sh` 退出 1；`ct v0.53.1` 打印 `ct build`，退出 0。
- 全部输出必须来自实际复跑，不允许改写、截断或拼接。

## 7. 依赖

- `ct v0.53.1`：本机由 `ct-lang` 的 `v0.53.1` 源码自举到 `/tmp/ct-bin-0531/ct`
  （`CT_SOURCE_DIR=/tmp/ct-lang-v0531 sh install.sh -p /tmp/ct-bin-0531`，退出 0）。
- 本机另有 `ct v0.53.0`（`/Users/samxx/.local/bin/ct`）与 `v0.49.0` / `v0.44.2` 存档，
  用于能力对照。
- 系统 `cc`、系统 libcurl、`python3`（安装样例回环服务）。
- 主规范：`/Users/samxx/trae-ws/.ai-engineering/methodology/ai-coding-workflow.md`（1.10.0）。
- 前置：PH-10 收口 `937c05d`；`g0`、`g1`、`g2`、`g3`、`g5`、`g6`、`g7`、`g9` 开工前为绿，
  `g4`/`g8` 仅因 `v0.1.0` 不指向 HEAD 而红（收口 §4.7 挂账）。

### 7.1 下限为什么是 0.53.1

- 当前最新发布：`ct-lang` 的 `v0.53.1`（2026-11-15，`git tag --sort=-v:refname | head -1`）。
- 本包用到的能力是**统一的 `ct` 入口**：`ct install` 必须打印 `ct build -I vendor/http …`，
  消费端按该行构建（`test/gates/p10_install.sh` 断言 `^ct build ` 与 `-I vendor/http`）。
  实测 `v0.49.0` 仍打印 `ct-ast build …`，安装样例退出 1；`v0.53.1` 打印 `ct build …`，
  `install ok` / `200`，退出 0。
- 旧理由 `impl T { static T *new(); }`（0.34.0，Phase 34）**已不成立**：本包公开面是
  `pub trait` + `map trait` + namespace 单元，`grep -rn 'impl ' http/` 命中 0。
  所以旧注释与 `docs/design.md` 的 0.34.0 依据一并改掉，不用旧能力冒充新下限。
- `0.53.1` 新增的 LANG-1/LANG-2（`.h.ct` 内 `defer`、namespace 内静态固有方法）本包不使用，
  不作为下限理由；下限理由是上面的安装接口能力与实测组合。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 本机默认 `ct` 是 0.53.0，低于新下限 | `ct get` 报 `requires` 不满足 | 安装样例固定用 `CT_BIN=/tmp/ct-bin-0531/ct`；其余门禁在 0.53.0 上仍可跑 |
| `v0.2.0` 打下后 HEAD 再前进会复现 `g4-version`/`g8-tag` 红 | tag 后又有提交 | 收口提交即 tag 目标；tag 之后不再提交，确需提交则重开下一相 |
| 收口提交与 tag 对象号互相引用 | tag message 含提交 sha | 先提交、后打 tag；证据文件在 tag 后回填输出，再重建本地未发布 tag |
| push 凭据不可用 | `git push` 非零退出 | 原文贴回，不换地址、不重试到死、不写成已完成 |
| 历史 `applies-to` 遗漏 | `g4-applies` 报某文件不匹配 | 按报错文件逐个补齐，不含糊跳过 |

## 9. 收口动作

1. 逐条跑 E-P11-1…E-P11-5，把命令、原始输出、退出码贴进 `docs/phases/PH-11/closeout.md`。
2. 确认 `docs/reviews/PH-11/T0.md`…`T5.md` 的 `baseline: 937c05d` 是 HEAD 祖先。
3. 贴十道门禁的 `ok gN` 与 `g4`/`g8` 转绿输出；负例仍由 `g5` 统一跑。
4. 打 annotated `v0.2.0`，校验类型、指向、祖先、可推送四项；推送分支与 tag。
5. push 失败时把确切报错原文写进收口与报告，不静默跳过、不改远端地址。
6. 报告一行：版本、提交号、tag 对象号、push 结果、未达成项（若有）。
