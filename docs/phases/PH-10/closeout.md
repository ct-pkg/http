# PH-10 收口

- 日期：2026-11-15
- 卡片：[card.md](card.md)（判据与任务已冻结）
- 基线：`67c5b2f`（逐任务红证见 `docs/reviews/PH-10/T1.md`…`T9.md`）
- 结论：**E-P10-1 不计通过**（Linux 半边不可执行）；E-P10-2…E-P10-5 通过。
  g0–g9 十道门禁全部真跑、全部退出 0，且 g0–g9 各有副本负例。

## 1. 退出判据逐条

| ID | 结果 | 原始输出（节选） |
| --- | --- | --- |
| E-P10-1 | **不计通过** | macOS：`ok g3 (… 55 个夹具 …)`，`rc=0`。Linux：`command -v podman` / `command -v docker` 均 `rc=1`，无容器；缺 Linux `ct` 二进制。见 §4.1 |
| E-P10-2 | 通过 | 十道门禁逐条 `rc=0`，见 §3 |
| E-P10-3 | 通过 | `ok g4 (109 docs <= 300 lines, 8 snippets compiled, 13 blocks bound, anchors/applies/readme/version ok)`，`rc=0`；`g4_negative.sh` 打印 `rejected snippet/impl-chain`，`rc=0` |
| E-P10-4 | 通过（卡内本地形态） | `bash test/gates/p10_install.sh` → `install ok` / `200`，`rc=0`；roadmap 裸名形态不可执行，见 §4.3 |
| E-P10-5 | 通过 | `ok g4 (… version ok)`，`rc=0`；`ct.pkg#version` 与 README 版本行均为 `0.1.0`，CHANGELOG 首条一致；`git tag` 为空，提案见 §5 |

E-P10-1 的 macOS 半边达成、Linux 半边不可执行，按「达不成的逐条列出、不计通过」整条不计通过；
不把「没跑」写成「跳过即通过」。

## 2. 任务对照

| 任务 | verify | 结果 | 红证 |
| --- | --- | --- | --- |
| T1 g2 | `bash tools/check.sh g2` | `ok g2 (19 requirements mapped, 24 ACC entries, anchors resolve)` | T1.md |
| T2 g4 | `bash tools/check.sh g4` | `ok g4 (109 docs …)` | T2.md |
| T3 g5/g6 | `bash tools/check.sh g5 && g6` | `ok g5 (60 task evidence files, 9 gate negatives exercised)`；`ok g6 (2 issues …)` | T3.md |
| T4 g7/g8/g9 | `bash tools/check.sh g7 && g8 && g9` | `ok g7 (254 files scanned … 7 sanitizer fixtures)`；`ok g8 (10 cards frozen, 5 commits in scope …)`；`ok g9 (spec 1.9.0 …)` | T4.md |
| T5 README 分拆 | `test -f docs/tutorial/01-concepts.md && bash tools/check.sh g4` | `rc=0` | T5.md |
| T6 台账与发布 | `bash tools/check.sh g6` | `ok g6 (2 issues …)` | T6.md |
| T7 安装 | `bash test/gates/p10_install.sh` | `install ok` / `200`，`rc=0` | T7.md |
| T8 跨平台 | `bash tools/check.sh g3` | macOS `rc=0`；Linux 登记为不可执行项 | T8.md |
| T9 收口 | `bash tools/check.sh g4 && bash tools/check.sh g8` | 两条 `rc=0` | T9.md |

固定条「任务清单每项可独立验收（6.6 T-6）」：九项各有 verify 与红证，无未验收项；
T8 的 Linux 半边按 §4.1 记为不可执行，不冒充过关。

## 3. 门禁原始输出

```console
== g0 rc=0
ok g0 (spec block, 8 rows, routing, deviations, session map, version 0.1.0)
== g1 rc=0
ok g1 (card items, task columns, frozen criteria/tasks digests, errata)
== g2 rc=0
ok g2 (19 requirements mapped, 24 ACC entries, anchors resolve)
== g3 rc=0
ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release, response ownership, ops, request config, response bytes, take_body, failure codes, error model, predicates, timeouts, streaming, cancellation, lifetime, redirects, credentials, downgrade)
== g4 rc=0
note g4-version: PATCH accounting deferred by deviation V-001 (0.1.0 number line)
ok g4 (109 docs <= 300 lines, 8 snippets compiled, 13 blocks bound, anchors/applies/readme/version ok)
== g5 rc=0
ok g5 (60 task evidence files, 9 gate negatives exercised)
== g6 rc=0
note g6-pool: no docs/pool.md in this project
ok g6 (2 issues, status/evidence legal, unmet criteria traced)
== g7 rc=0
ok g7 (254 files scanned, deps consistent, 7 sanitizer fixtures)
== g8 rc=0
note g8-tag: v0.1.0 not created (deviation V-002)
ok g8 (10 cards frozen, 5 commits in scope, version/tag rules)
== g9 rc=0
ok g9 (spec 1.9.0, sha256 matches master)
```

门禁负例（副本篡改，全部 `rejected`）：

```console
$ bash test/gates/g0_negative.sh  → rejected version-landing      rc=0
$ bash test/gates/g1_negative.sh  → 29 tampered cases rejected     rc=0
$ bash test/gates/g2_negative.sh  → rejected map/missing, map/anchor rc=0
$ bash test/gates/g3_negative.sh  → rejected fixture/output, /unwired rc=0
$ bash test/gates/g4_negative.sh  → rejected size/301, snippet/impl-chain rc=0
$ bash test/gates/g5_negative.sh  → rejected evidence/missing, /baseline rc=0
$ bash test/gates/g6_negative.sh  → rejected issue/fixed-status, /illegal-status rc=0
$ bash test/gates/g7_negative.sh  → rejected scan/secret            rc=0
$ bash test/gates/g8_negative.sh  → rejected version/mismatch       rc=0
$ bash test/gates/g9_negative.sh  → rejected pin/forged             rc=0
```

## 4. 未满足与不可执行项（逐条，不计通过）

### 4.1 E-P10-1 的 Linux 半边：不可执行

- 本机：`uname -a` = `Darwin … arm64`；`command -v podman` 与 `command -v docker` 均退出 1。
- 缺两样环境：①容器运行时（podman 或 docker）；②Linux 版 `ct` 二进制
  （`test/run.sh` 默认 `CT_BIN=/Users/samxx/.local/bin/ct`，macOS Mach-O 不能在容器里执行）。
- 有环境后的补跑命令（在仓库根执行）：

```console
podman run --rm -v "$PWD":/w -w /w \
  -e CT_BIN=/usr/local/bin/ct \
  docker.io/library/debian:bookworm \
  bash -lc 'apt-get update && apt-get install -y build-essential libcurl4-openssl-dev \
           && uname -a && bash tools/check.sh g3'
```

- 通过判据：容器内 `uname -s` 输出 `Linux`，`bash tools/check.sh g3` 退出 0。
- 在此之前 E-P10-1 不计通过。

### 4.2 PH-3 `header()` 假绿复核

- 事实：`d0160db` 之前，`header()` 只维护 `curl_slist`，`send()` 从未设置
  `CURLOPT_HTTPHEADER`，请求头没有上线；PH-9 的 T2 红证（基线同源行
  `same auth 0 cookie 0 proxy 0`）是直接证据。
- 逐卡复核（`grep '请求头\|header(' docs/phases/PH-*/card.md`）：
  - PH-1 只断言链能编译、运行输出固定值，不涉及线上字节。
  - PH-3 的 E-P3-2 只断言 `curl_slist` 内容与 CR/LF 拒绝
    （`p3-header-crlf` → `rejected 1 1 added 0 1 first Accept: application/json`），
    **没有**断言线上收到该头。
  - PH-4…PH-8 的卡片没有「请求头已发出」类断言；`p4`…`p8` 夹具也不读回显头。
- 结论：**没有冻结判据「通过了但实际没生效」**，因此没有需要推翻的 phase 结论。
  假绿出在文档与用户预期层：旧 README 的配置面让人以为 `header()` 已生效。该叙述已在
  PH-10 重写（README「配置面」→ `docs/tutorial/03-config.md`，并指向重定向篇的线上验证）。
- 已立案：`issues/HTTP-1.md`（status `open`，处置段记 `d0160db`，待 REV 复核为 `verified`）。

### 4.3 E-P10-4 的裸名形态：不可执行

- roadmap 原命令 `ct get http@^0.1.0 && ct install` 需要已发布索引与 `v0.1.0` tag；
  本轮按用户指令不打 tag、不 push，索引也不存在，故该形态不可执行。
- 已执行的替代形态（卡内定义）：本地目录 spec
  `ct get /Users/samxx/trae-ws/ct-http` → `ct install` → `ct verify` → 构建样例 → 回环 `200`。
- 差异：`ct install` 打印 `-I vendor/http`，而本包按设计 D-1 把单元放在 `src/http/`，
  消费端需 `-I vendor/http/src`；已立 `issues/HTTP-2.md`，未在本轮改目录布局。

### 4.4 ASAN 符号化挂起（环境修正，非判据放宽）

- 现象：本机 `AddressSanitizer` 在符号化阶段挂起（`p2-double-free-bad` 等负例进程不退出），
  导致 g3/g7 无法完成；`ASAN_OPTIONS=…:symbolize=0` 后正常 `ABORTING` 并退出。
- 另一处：负例报告名视是否先读失效指针而不同，本机为 `heap-use-after-free`，原断言只认
  `double-free`。改为 `double-free|use-after-free`，**仍要求进程非 0 退出**，检测强度不变。
- 影响：这是测试运行方式与报告名匹配的修正，不放宽任何判据；修正提交见 §6。

### 4.5 tag 与推送：按用户指令不执行

- `git tag` 为空；`g8-tag` 打印 `v0.1.0 not created (deviation V-002)`。
- 该偏离在 `AGENTS.md` §0.3 有日期、条款、理由、收敛动作与用户指令出处；不是「跳过」。

## 5. tag 提案（不执行）

```console
git tag -a v0.1.0 -m "PH-10 <收口提交 sha 前 7 位>" <收口提交>
git cat-file -t v0.1.0                      # 期望 tag（annotated）
git rev-parse v0.1.0^{commit}               # 期望等于收口提交
git push origin v0.1.0                      # 用户授权后
```

四项（名称带 `v`、annotated、指向收口提交、提交可在远端追溯）全过才推；本轮四项都不做。

## 6. 本轮提交

| 提交 | 内容 |
| --- | --- |
| `912ca1c` | PH-10 开卡（判据/任务冻结，g8 基线文件） |
| `226a242` | g2、g4–g9 实现与副本负例；`test/run.sh` 的 ASAN 运行修正 |
| `19ff65c` | README 分拆、`docs/tutorial/`、`docs/snippets/`、`applies-to` |
| `51b9f7e` | `issues/`、`CHANGELOG.md`、`docs/acceptance-map.md`、AGENTS 落点与偏离 |
| `f091a73` | `test/gates/p10_install.sh`（清洁目录安装） |
| 本提交 | 收口文档与 `docs/reviews/PH-10/T1…T9.md` |

上游勘误提交（PH-9 收口修正）：`docs/errata.md` 的 `ERR-001`，PH-9 卡片原文与
「不计通过」记录均未改动。
