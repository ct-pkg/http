# PH-10 收口

- 日期：2026-11-15
- 卡片：[card.md](card.md)（判据与任务已冻结）
- 基线：`67c5b2f`（逐任务红证见 `docs/reviews/PH-10/T1.md`…`T9.md`）
- 结论：**E-P10-1 通过**（2026-11-17 补跑 Linux 半边：容器内 `g3 rc=0`）；E-P10-2…E-P10-5 通过。
  g0–g9 十道门禁全部真跑、全部退出 0，且 g0–g9 各有副本负例。

## 1. 退出判据逐条

| ID | 结果 | 原始输出（节选） |
| --- | --- | --- |
| E-P10-1 | **通过** | macOS：`ok g3 (… 55 个夹具 …)`，`rc=0`。Linux（2026-11-17，Podman + `gcc:13`）：自举 `ct v0.61.1`，`ok g3 (…)`、`g3 rc=0`；配方见 `docs/reviews/PH-10/E-P10-1-linux.md` |
| E-P10-2 | 通过 | 十道门禁逐条 `rc=0`，见 §3 |
| E-P10-3 | 通过 | `ok g4 (120 docs <= 300 lines, 8 snippets compiled, 13 blocks bound, anchors/applies/readme/version ok)`，`rc=0`；`g4_negative.sh` 打印 `rejected snippet/impl-chain`，`rc=0` |
| E-P10-4 | 通过（卡内本地形态） | `bash test/gates/p10_install.sh` → `install ok` / `200`，`rc=0`；roadmap 裸名形态不可执行，见 §4.3 |
| E-P10-5 | 通过 | `ok g4 (… version ok)`，`rc=0`；`ct.pkg#version` 与 README 版本行均为 `0.1.0`，CHANGELOG 首条一致；`git tag` 为空，提案见 §5 |

E-P10-1 在 2026-11-15 只有 macOS 半边可执行，按「达不成的逐条列出、不计通过」整条不计通过；
2026-11-17 容器内补跑通过后改判为通过，证据与配方见 §4.1 与 `docs/reviews/PH-10/E-P10-1-linux.md`。

## 2. 任务对照

| 任务 | verify | 结果 | 红证 |
| --- | --- | --- | --- |
| T1 g2 | `bash tools/check.sh g2` | `ok g2 (19 requirements mapped, 24 ACC entries, anchors resolve)` | T1.md |
| T2 g4 | `bash tools/check.sh g4` | `ok g4 (120 docs …)` | T2.md |
| T3 g5/g6 | `bash tools/check.sh g5 && g6` | `ok g5 (60 task evidence files, 9 gate negatives exercised)`；`ok g6 (3 issues …)` | T3.md |
| T4 g7/g8/g9 | `bash tools/check.sh g7 && g8 && g9` | `ok g7 (265 files scanned … 7 sanitizer fixtures)`；`ok g8 (10 cards frozen, 6 commits in scope …)`；`ok g9 (spec 1.9.0 …)` | T4.md |
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
ok g4 (120 docs <= 300 lines, 8 snippets compiled, 13 blocks bound, anchors/applies/readme/version ok)
== g5 rc=0
ok g5 (60 task evidence files, 9 gate negatives exercised)
== g6 rc=0
note g6-pool: no docs/pool.md in this project
ok g6 (3 issues, status/evidence legal, unmet criteria traced)
== g7 rc=0
ok g7 (265 files scanned, deps consistent, 7 sanitizer fixtures)
== g8 rc=0
note g8-tag: v0.1.0 not created (deviation V-002)
ok g8 (10 cards frozen, 6 commits in scope, version/tag rules)
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

### 4.1 E-P10-1 的 Linux 半边：UNVERIFIED（待环境）

- 本机事实（2026-11-15）：
  - `uname -a` = `Darwin … arm64`；
  - `command -v podman`、`command -v docker` 均无命中（退出 1）；
  - `file /Users/samxx/.local/bin/ct` = `Mach-O 64-bit executable arm64`，**不能**在 Linux 容器里执行。
- Linux 版 `ct` 的来源：ct-lang 没有发布 Linux 预编译二进制，只发 tag；`install.sh` 从源码自举
  L0（C seed）→ L1 → L2 → ct-ast，要求 `cc` 与 `tar`。因此容器里能自己造出来，
   **不构成额外阻塞**；唯一缺的是容器运行时，由用户安装。
- 状态：`E-P10-1` 的 Linux 半边在 2026-11-17 通过容器补跑（`g3 rc=0`），整条改判 **通过**；
  配方与原始输出见 `docs/reviews/PH-10/E-P10-1-linux.md`。以下四步保留作历史与复用。

容器运行时到位后，按下列四步执行（在仓库根，主机 shell）：

**第 1 步：起容器并装包**（`debian:bookworm`；Apple silicon 用 `--platform linux/arm64`）

```console
podman run --rm -it --platform linux/arm64 \
  -v "$PWD":/w -v /Users/samxx/trae-ws/ct-lang:/src:ro -w /w \
  docker.io/library/debian:bookworm bash
```

容器内：

```console
apt-get update
apt-get install -y --no-install-recommends \
  build-essential libcurl4-openssl-dev python3 openssl perl git binutils
```

- 用途：`build-essential`（gcc/ASAN）、`libcurl4-openssl-dev`（包的唯一链接依赖）、
  `python3`（本地回环 server 与 `p10_install.sh`）、`openssl`（`p9-downgrade` 证书）、
  `perl`（`shasum`，门禁算摘要用）、`git`（g5/g8 读历史）、`binutils`（`nm`，`p2-nm` 用）。

**第 2 步：在容器内自举 Linux 版 `ct` 并指定 `CT_BIN`**

```console
CT_SOURCE_DIR=/src sh /src/install.sh -p /usr/local/bin
/usr/local/bin/ct -v          # 期望：ct v0.49.0（随 /src 检出的 tag）
export CT_BIN=/usr/local/bin/ct
```

- `CT_SOURCE_DIR=/src` 走本地检出、不下载；`-p /usr/local/bin` 让 `ct` 进 PATH 旁的固定位置。
- `CT_BIN` 是 `tools/check.sh` 与 `test/run.sh` 的覆盖点；不设会回落到 macOS 的
  `/Users/samxx/.local/bin/ct`，在容器里必然失败。
- 若 `install.sh` 报 `CC not found`，先 `apt-get install -y build-essential`（第 1 步已含）。

**第 3 步：跑判据**

```console
cd /w
bash tools/check.sh g3                      # E-P10-1 的容器内判据
for g in g0 g1 g2 g4 g5 g6 g7 g8 g9; do     # 可选：Linux 上顺带复核其余门禁
    bash tools/check.sh "$g" || echo "FAIL $g"
done
```

**第 4 步：留原始输出**（追加到 `docs/reviews/PH-10/E-P10-1-linux.md`，未跑时该文件不建）

```console
{
  uname -a
  cc --version | head -1
  /usr/local/bin/ct -v
  bash tools/check.sh g3
  echo "g3 rc=$?"
} 2>&1 | tee /tmp/p10-linux-g3.log
```

**2026-11-16 复核（Podman）**：容器运行时已装上（`podman 6.2.0`，libkrun VM），但仍未跑成，原因从“无容器”变成“包源不可达”：

- `podman machine init` 成功（VM 落在 `/tmp/poddata`，7.8G），`uname` 拿到 `Linux … aarch64`。
- `podman machine start` 常报 `Error: krunkit exited unexpectedly with exit code 1`，需在同一 shell 内重试；
  VM 随启动会话退出，跨命令再 `podman run` 报 `dial tcp 127.0.0.1:63338: connect: connection refused`。
- 镜像源：`docker.io` → `connection refused`；`mirror.gcr.io` → `i/o timeout`；
  `quay.io` / `ghcr.io` 可达（401 为鉴权语义），`registry.fedoraproject.org` 返回 200。
- 用 `quay.io/fedora/fedora:42` 已能起容器；但 `dnf install` 下载停滞（`/proc/<pid>/io` 的 `rchar` 8 秒无增长，18 分钟未装到 `gcc`）。

结论：E-P10-1 的 Linux 半边已于 2026-11-17 补跑通过，本条关闭（`issues/HTTP-3.md` → `verified`）。

**2026-11-17 补跑（通过）**：

- VM：`podman 6.2.0`，`podman info` 返回 `linux`，跨命令可用；容器 `docker.io/library/gcc:13`。
- 自举：`CT_SOURCE_DIR=/src sh /src/install.sh -p /usr/local/bin` → `ct v0.61.1`。
- 结果：`bash tools/check.sh g3` 输出 55 行 `ok`，末行 `g3 rc=0`。
- 顺带修掉三处夹具在严格 C99 下缺特性测试宏的问题（`p4-server.c`、`p9-server.c`、`p9-downgrade.c`），
  macOS 与 Linux 复跑均为 0 失败。

- 必须留的片段：`uname -a`（`Linux … aarch64`）、`cc --version` 首行、`ct -v`、
  `ok g3 (…)` 汇总行、`g3 rc=0`。
- 通过判据：容器内 `uname -s` = `Linux` 且 `bash tools/check.sh g3` 退出 0；两者缺一不记通过。
- 在此之前，`E-P10-1` 的 Linux 半边保持 `UNVERIFIED`，不写「已通过」。

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
- **复核完成（2026-11-15，主控/用户授权）**：基线 `d0160db^` 的 shim 配当前夹具得
  `same auth 0 cookie 0 proxy 0` / `same referer 0`；HEAD 得 `same auth 1 cookie 1 proxy 1` /
  `same referer 1`。结论成立，`issues/HTTP-1.md` 已转 `verified`，
  原始输出见 `docs/reviews/PH-10/HTTP-1-verify.md`。

### 4.3 E-P10-4 的裸名形态：不可执行

- roadmap 原命令 `ct get http@^0.1.0 && ct install` 需要已发布索引与 `v0.1.0` tag；
  本轮按用户指令不打 tag、不 push，索引也不存在，故该形态不可执行。
- 已执行的替代形态（卡内定义）：本地目录 spec
  `ct get /Users/samxx/trae-ws/ct-http` → `ct install` → `ct verify` → 构建样例 → 回环 `200`。
- 差异：`ct install` 打印 `-I vendor/http`，而本包按设计 D-1 把单元放在 `src/http/`，
  消费端需 `-I vendor/http/src`；已立 `issues/HTTP-2.md`，未在本轮改目录布局。
- **收口更新（2026-11-15，用户裁定选①）**：布局已改为包根 `http/`（`git mv src/http http`），
  `ct install` 打印的 `-I vendor/http` 与真实包含根一致，差异消失。
  `p10_install.sh` 断言安装行含 `-I vendor/http` 且不含 `-I vendor/http/src`，样例 `200`；
  重跑输出见 `docs/reviews/PH-10/HTTP-2-verify.md`。上游前缀约定另立提案
  `docs/proposals/ct-install-include-prefix.md`（未改 ct-lang 仓库）。

### 4.4 ASAN 符号化挂起（环境修正，非判据放宽）

- 现象：本机 `AddressSanitizer` 在符号化阶段挂起（`p2-double-free-bad` 等负例进程不退出），
  导致 g3/g7 无法完成；`ASAN_OPTIONS=…:symbolize=0` 后正常 `ABORTING` 并退出。
- 另一处：负例报告名视是否先读失效指针而不同，本机为 `heap-use-after-free`，原断言只认
  `double-free`。改为 `double-free|use-after-free`，**仍要求进程非 0 退出**，检测强度不变。
- 影响：这是测试运行方式与报告名匹配的修正，不放宽任何判据；修正提交见 §6。

### 4.5 tag 与推送：2026-11-15 用户授权后已执行

- 收口当时（§4.5 原文）确实未打 tag：`git tag` 为空，`g8-tag` 打印
  `v0.1.0 not created (deviation V-002)`；偏离登记在 `AGENTS.md` §0.3。
- **2026-11-15 用户授权执行**，事实如下：
  - tag：`v0.1.0`，annotated（`git cat-file -t` = `tag`），tag 对象 `610d9f9`，
    指向提交 `b4b7b43`（打 tag 当时的 HEAD）。
  - 推送：`41ae95e..b4b7b43 main -> main`，`* [new tag] v0.1.0 -> v0.1.0`；
    远端复核 `git ls-remote origin refs/heads/main refs/tags/v0.1.0` 命中两者。
  - 推送前核对：`origin/main` 是本地 `main` 的祖先（`git merge-base --is-ancestor` 退出 0），
    无分叉；待推 120 个提交，改动只落在 `http/`、`test/`、`tools/`、`docs/`、`issues/`
    与四个根文件（`AGENTS.md`、`CHANGELOG.md`、`README.md`、`src/README.md` 的删除）。
- **与冻结判据的冲突**：E-P10-5 冻结文本写「tag 只出提案」，本次授权覆盖了该条。
  冻结原文不动（g1 的 `criteria-sha256` 不变），以本节作为授权记录。
- **tag 与 HEAD 的落差**：此后 HEAD 前进到 `be58548`（工程规范 1.10.0）与本次文档收口，
  `g8-tag`（tag 必须指向 HEAD）与 `g4-version`（HEAD 必须被 `v0.1.0` 标记）都不再成立，
  两道判红。已发布 tag 不移动、不改写；挂账与收敛条件见 §4.7。

### 4.6 工作区遗留改动的归位（2026-11-15）

- **未采纳**：`ct.pkg` 的 `requires = ct >= 0.44.1`（含注释改写）、`docs/design.md`、
  `docs/requirements.md`、`docs/phases/H1.md` 里的同一处 `requires` 数字。
  - 理由一：「下限取最新发布版」这个前提已经过期。撰写时（2026-11-13）最新发布版是 v0.44.1；
    2026-11-15 `ct-lang` 最新 tag 已是 v0.49.0（`git tag --sort=-v:refname | head -1`）。
    照原样提交会把一句过期理由写进包清单。
  - 理由二：`docs/requirements.md` 把 `requires=ct >= 0.34.0` 列为验收判据，
    改数字属于改判据，不属于「归位」。
  - 判定：**弃用**——不提交、不抬下限，四处改动不代表现行事实。
  - 清理方式：`git checkout -- ct.pkg docs/design.md docs/requirements.md docs/phases/H1.md`
    （只回退这四个文件的这一处 hunk；`docs/design.md` 本轮无其它改动）。
  - 阻塞原因：该命令属破坏性操作，被权限层拒绝（工具返回「不允许重置/回退」），未绕过。
    因此四处改动目前仍留在工作区，等明确授权后再执行上面的清理。
  - 要抬下限：不算「归位」，应按当前实测环境另出判据变更（新 phase 或 errata）。
- **已归档**：`docs/review/` 下三篇未跟踪草稿收进 `docs/reviews/spec-review/`；
  两篇超过 300 行的按小节边界拆为 `part2` / `part3`，正文未改，只补来源说明；
  源草稿按「不要删」保留在原处。
- **入库**：本节、§4.1 的四步清单、§4.5 的 tag 记录与 `AGENTS.md` 的 V-002 更新，
  随本次文档收口提交入库（此前因「tag 之后的提交会让 g8 红」而暂留工作区，现已落库，
  门禁影响见 §4.7）。

### 4.7 挂账（与已发布 tag 冲突，不硬改历史）

- `g8-tag` / `g4-version`：`v0.1.0` 指向 `b4b7b43`，HEAD 已到本次文档收口提交，两道门禁判红
  （`g8-tag: … not HEAD`；`g4-version: HEAD is not tagged v0.1.0`）。这个红从 `be58548`
  （工程规范 1.10.0）起就存在，不是本次文档改动引入。
  - 不改的原因：移动已推送的 tag 等于改写发布对象，本地与远端都会分叉；用户明确不硬改历史。
  - 收敛条件：下一次发版打新 tag（0.2.0）后自然转绿。期间该红如实保留，
    不用 `--no-verify`、不改门禁逻辑来掩盖。
- 四处 `requires` 改动：判定弃用但未能执行清理（权限层拦截 `git checkout --`），
  挂「待授权清理」，命令见 §4.6。
- Linux 半边：`E-P10-1` 于 2026-11-17 在容器内补跑通过（`g3 rc=0`），`issues/HTTP-3.md` → `verified`，
  属环境缺口而非工作量缺口。

## 5. tag 提案（收口时为提案，2026-11-15 已授权执行）

```console
git tag -a v0.1.0 -m "PH-10 <收口提交 sha 前 7 位>" <收口提交>
git cat-file -t v0.1.0                      # 期望 tag（annotated）
git rev-parse v0.1.0^{commit}               # 期望等于收口提交
git push origin v0.1.0                      # 用户授权后
```

四项（名称带 `v`、annotated、指向收口提交、提交可在远端追溯）全过才推。
收口当时四项都没做；2026-11-15 用户授权后按此执行，结果见 §4.5。

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

收口提交本身也在 g8 的统计范围内，因此 §3 的数字是收口提交后的复跑值。此后追加的纯文档
提交会让 in-scope 计数增加，这部分仍判绿；但 `g8-tag` 因 HEAD 已离开 `v0.1.0` 而转红，
原因与收敛条件见 §4.7。
