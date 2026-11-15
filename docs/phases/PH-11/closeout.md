# PH-11 收口

- 日期：2026-11-15
- 卡片：[card.md](card.md)（判据与任务已冻结）
- 基线：`937c05d`（逐任务红证见 `docs/reviews/PH-11/T0.md`…`T5.md`）
- 结论：E-P11-1…E-P11-5 全部达成；十道门禁逐条退出 0；`v0.2.0` 为 annotated tag。

## 1. 方法论核对（开卡前完成）

- 规范：`/Users/samxx/trae-ws/.ai-engineering/methodology/ai-coding-workflow.md`（1.10.0）。
- §6.2（第 515–526 行）只列必填字段，**没有**「每相必须带产品增量」或「禁止纯发布相」的条款。
- §6.7 V-1（第 666、671 行）与 V-7（第 677 行）分别管版本换算与收口 tag。
- 结论：只含发布内容的相合法，路线 1（`0.2.0`）成立，不退回 `0.1.1`。

## 2. 退出判据逐条

| ID | 结果 | 原始输出（节选） |
| --- | --- | --- |
| E-P11-1 | 达成 | `grep -c 'requires = ct >= 0.53.1' ct.pkg` → `1`，`rc=0`；旧下限 `grep` 命中 0，`rc=1`；`CT_BIN=/tmp/ct-bin-0531/ct bash test/gates/p10_install.sh` → `install ok` / `200`，`rc=0` |
| E-P11-2 | 达成 | 19 个文件 `applies-to` 全为 `0.2.0`；`sort -u` 只有一行；`ok g4 (… anchors/applies/readme/version ok)`，`rc=0` |
| E-P11-3 | 达成 | 三处落点均为 `0.2.0`；`G4_ONLY=version` 与 `G8_ONLY=version` 均 `rc=0`；`ok g0 (… version 0.2.0)` |
| E-P11-4 | 达成 | `git cat-file -t v0.2.0` → `tag`；`v0.2.0^{commit}` = HEAD；`merge-base --is-ancestor` `rc=0`，见 §3 |
| E-P11-5 | 达成 | 十道门禁逐条 `rc=0`，见 §4 |

固定条「任务清单每项可独立验收（6.6 T-6）」：六项各有 verify 与红证，无缺项。

## 3. tag 四项校验

```console
$ git cat-file -t v0.2.0
tag
$ git rev-parse v0.2.0^{commit}
1ea2972b29c46bdf48f5455d3378d0c25031178e
$ git rev-parse HEAD
1ea2972b29c46bdf48f5455d3378d0c25031178e
$ git merge-base --is-ancestor v0.2.0 HEAD
rc=0
$ git rev-parse v0.2.0
99609e5778f5b612337cc5d864dae5f414e49646
```

tag 建立后又追加了 T5 记录提交，最终 `v0.2.0` 重建到该提交；最终对象号见本文件末尾。

`v0.1.0` 未移动、未删除；tag 之后 HEAD 不再前进，下一次改动重开新相。

## 4. 门禁原始输出

以下为收口提交 `1ea2972` 上的复跑；tag 建成后的最终复跑见交付报告（`g8` 的 in-scope 计数会随提交数增长）。

```console
== g0 rc=0
ok g0 (spec block, 8 rows, routing, deviations, session map, version 0.2.0)
== g1 rc=0
ok g1 (card items, task columns, frozen criteria/tasks digests, errata)
== g2 rc=0
ok g2 (19 requirements mapped, 24 ACC entries, anchors resolve)
== g3 rc=0
ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release, response ownership, ops, request config, response bytes, take_body, failure codes, error model, predicates, timeouts, streaming, cancellation, lifetime, redirects, credentials, downgrade)
== g4 rc=0
note g4-version: PATCH accounting deferred by deviation V-001 (0.1.0 number line)
ok g4 (138 docs <= 300 lines, 8 snippets compiled, 13 blocks bound, anchors/applies/readme/version ok)
== g5 rc=0
ok g5 (66 task evidence files, 9 gate negatives exercised)
== g6 rc=0
note g6-pool: no docs/pool.md in this project
ok g6 (3 issues, status/evidence legal, unmet criteria traced)
== g7 rc=0
ok g7 (278 files scanned, deps consistent, 7 sanitizer fixtures)
== g8 rc=0
ok g8 (11 cards frozen, 20 commits in scope, version/tag rules)
== g9 rc=0
ok g9 (spec 1.10.0, sha256 matches master)
```

## 5. 口径说明

### 5.1 下限为什么是 0.53.1

- 能力：统一 `ct` 入口——`ct install` 打印 `ct build -I vendor/http …`，消费端按该行构建
  （`test/gates/p10_install.sh` 断言）。`ct v0.49.0` 打印 `ct-ast build`，样例退出 1；
  `ct v0.53.1` 得 `install ok` / `200`。
- 旧理由（`impl T { static T *new(); }`，0.34.0）已删：公开面是 `map trait`，
  `grep -rn 'impl ' http/` 命中 0。
- `0.53.1` 的 LANG-1/LANG-2 本包不使用，未拿来当理由。

### 5.2 版本号与偏离

- `0.2.0`（MINOR=2）按 `deviation: V-001` 的 `d = 9` 映射到 PH-11；冻结解除，偏移永久保留。
- PATCH=0：`0.2.0` 之后没有新立案 fix；`issues/` 里两条 `fix: yes` 属 0.1.0 号段。
- `g4-version` 的 `PATCH accounting deferred` 是门禁在 `d ≠ 0` 时的固定 note，未改门禁。

### 5.3 tag 与门禁的已知空隙

- `g8-tag` 在 tag 缺失时会接受 `deviation: V-002` 的豁免分支（`AGENTS.md` 里该条记为已解除）。
  因此单看 `ok g8` 不足以证明 tag 存在；本轮的 tag 结论以 §3 的三条直接断言为准，`v0.2.0`
  建立后 `g8-tag` 走严格分支校验（annotated + 指向 HEAD）。
- `v0.1.0` 指向 `b4b7b43`，与 `0.2.0` 的收口提交无冲突；已发布 tag 不回改。

### 5.4 工作区遗留

- `stash@{0}: requires 清理` 保存的是 PH-10 时期四处 `requires = ct >= 0.44.1` 的实验改动；
  本卡的下限（`0.53.1`）是重新论证后的结论，stash 内容已过期。**不 drop、不 apply**，原样保留备查。
- `docs/review/*.md` 三篇未跟踪草稿仍未入库，按用户口径保留在原处（已归档副本在
  `docs/reviews/spec-review/`）。

### 5.5 环境缺口

- Linux 半边仍为 `UNVERIFIED（待环境）`，与 PH-10 收口 §4.1 同一状态；`issues/HTTP-3.md`
  保持 `open`，容器运行时由用户安装后按四步清单复跑。本轮不改该结论。

## 6. 本轮提交

| 提交 | 内容 |
| --- | --- |
| `d64cb8f` | PH-11 开卡（判据/任务冻结、`base`、方法论核对结论） |
| `2824452` | 19 个 `applies-to` 同步到 `0.2.0` |
| `6653560` | 编译器下限 `ct >= 0.53.1` 与能力依据同步 |
| `16ef5bc` | 版本落点 `0.2.0`、CHANGELOG 与 V-001 收敛 |
| `1ea2972` | 收口、逐任务红证、会话地图 |
| 本提交 | tag 四项校验记录（T5），`v0.2.0` 重建到本提交 |

tag `v0.2.0` 建在本提交之上（annotated）；本提交的提交号与 tag 对象号见交付报告。
