# ct-http / S-05 主张席（续）：冲突、成本与验收锚点

- 本篇接 [S-05-advocate.md](S-05-advocate.md) §3。

## 4. 与 requirements 的冲突及解决路径

### 4.1 现状原文（逐条）

| 位置 | 原文要点 |
| --- | --- |
| `docs/requirements.md:169-177` | R-14：三类不能混；表驱动 `error_code` 等于 DNS 6 / 拒绝 7 / 超时 28 / TLS 60 / 中止 42 / 写 23；4xx/5xx `error_code==0`；未知码保留原值 |
| `docs/requirements.md:178` | R-14 非目标：「不把 libcurl 的 enum 全量导出」 |
| `docs/requirements.md:179` | 「本地错误码空间待确认（Q-05）」 |
| `docs/requirements.md:246` | M-09 建议「定义本地错误空间（建议负数），公开常量并逐条文档化」，状态「仍需提案方确认」 |
| `docs/requirements.md:258` | Q-05：本地参数错误用什么码空间？是否允许与 `CURLcode` 混用？ |
| `docs/planning/ct-http-0.1.0-features.md:93-99` | S-05：提案要求 12 个宏/枚举；「枚举化会改写这套数值」；两条影响面分支 |
| `docs/planning/ct-http-0.1.0-roadmap.md:51` | phase 5 标 `gated`，依赖 S-05 |
| `docs/phases/PH-3/card.md:19,40-41`、`docs/phases/PH-4/card.md:19,49,51` | 两卡均写「不公开 `HTTP_ERR_*`」，错误面留 PH-5 |

### 4.2 纳入后的改法（建议措辞，本文不代改）

1. R-14 增加一条验收判据：「本地参数错误以 `pub enum HttpErr` 公开，取值为 -1001…-1006；
   `error_code` 仍等于原始 `CURLcode`，未知码保留」。R-14 原有六行表驱动**一字不改**。
2. R-14 的「本地错误码空间待确认（Q-05）」改为「已定：见 `HttpErr`」。
3. Q-05 关闭；M-09 状态改「已定」。
4. `features.md` §3 的 S-05 裁定为「纳入（窄形态：本地空间枚举，`CURLcode` 原值不动）」，
   并解除 roadmap `:51` 的 `gated`。
5. README `README.md:145-147` 的「待裁定」清单删掉 `HTTP_ERR_*`，换成交付说明。

### 4.3 定性：条款变更，不是措辞澄清

- 关闭 Q-05 是**把待确认项定稿**，改变了 requirements 的规范状态，所以是条款变更；
  但**不改任何已冻结判据的数值**，也不放宽门禁。
- 如果主张的是「枚举替换 `CURLcode` 数值」，那才与 `docs/requirements.md:173` 的表驱动直接冲突，
  且会让 E-P4-4 的 `code 7` / `code 28`（`docs/phases/PH-4/card.md:92`）变红。本文明确不主张该形态。

### 4.4 谁让谁（按仓库口径优先级）

- `AGENTS.md:4-5`：加载顺序为「本节 > 主规范 > 生成块 > 本项目其余章节；冲突取更严者」，
  且「本项目只能加严；放宽必须写 `deviation:` 并由用户签字」。
- 因此 requirements 是 R-14 的基线，design/roadmap/phase 卡只能更严，不能反向放宽。
  本方案不触碰 R-14 的 `error_code` 表，所以**不需要 requirements 让步**；需要的是
  **由用户/REQ 在 requirements 里把 Q-05 定稿**（本文只给措辞建议）。
- `features.md:59` 写明「本节只陈述选项与影响面，不替用户选择」；S-05 在裁定前不得进入 phase 卡
  （`features.md:157`）。所以流程上是：先裁定 → 改 requirements → 再进 PH-5 卡。
  在裁定前实现枚举，属于抢跑，本文不主张。

---

## 5. 成本

| 项 | 改动 | 量级 |
| --- | --- | --- |
| 新文件 `src/http/Errors.h.ct` | `namespace http;` + 6 成员 `pub enum HttpErr` + 注释 | ~14 行 |
| `src/http/http.shim.h` | `struct HttpResponse` 尾部加 `int local_error;`；加 `int http_response_local_error(struct HttpResponse *);` | ~3 行 |
| `src/http/http.shim.c` | `real_send` 把 `r->local_error` 抄进响应；`easy==NULL` 分支记 `NO_HANDLE`；新增访问器 1 行 | ~6 行 |
| `src/http/http.m.ct` | `ResponseView` 加 `int local_error();` + 1 条 map recipe | ~2 行 |
| 一致性断言（g3） | CT 夹具里 `#include "http.shim.h"`，逐成员断言 `HTTP_LOCAL_ERR_* == HTTP_ERR_*` | ~8 行 |
| 新夹具 | `test/fixtures/p5-local-err.ct` + `.expected.out` + `test/run.sh` 一个 case + `tools/check.sh` g3 一行 | ~30 行 |
| `README.md` | 错误面段落 + 调用示例；删待裁定项一行 | ~10 行 |
| 合计 | 6 个已存在文件 + 1 新单元 + 1 新夹具 | **~90 行，一个 S/M 任务对** |

### 5.1 对已冻结判据的影响

- **E-P1-1…E-P1-6**：无影响。`phase1_abi.c:9-16` 只断言 `url/headers/timeout_ms/status/body`
  的偏移，尾字段不移动任何既有偏移。
- **E-P3-1…E-P3-7**：无影响。值是同一批 `-1001…-1005`；`p3-header-crlf.c:21-22`、
  `p3-body-nul.c:28` 若随宏改名同步改一行，输出逐字节不变；E-P3-4 的 `-1004` 不变。
- **E-P4-1…E-P4-5**：无影响。`7` / `28` 仍在 `CURLcode` 空间；g3 只多一行夹具调用，
  属 PH-5 的任务范围，不属「改冻结判据」。
- 唯一行为变化：`easy==NULL` 从「静默 status 0」变成 `NO_HANDLE` 可观察，且只经新访问器可见，
  既有夹具不读该值。

### 5.2 ABI

- `struct HttpResponse` 尾部加 4 字节，`status/body/body_len/total_bytes` 偏移全不变；
  CT 调用方从不自己分配该结构体，只有 shim 分配，所以无源码级破坏。
- 未发布（`docs/requirements.md:226-230` 的 tag 只提案；当前无 `v0.1.0` tag），
  不构成对已发布 ABI 的破坏。
- 两个手工构造响应的夹具（`test/fixtures/phase1_abi.c:26-36`、`test/fixtures/p2-ops.c:18-28`）
  不设新字段；它们今天也没设 `body_len` / `total_bytes`（PH-4 新增字段），g3 仍绿，所以
  **不是红点**；补一行 `res->local_error = 0;` 属卫生项，可与实现同提交。

---

## 6. 最强反方论点与正面回应

### 反方（最强的一条）

> 0.1.0 的公开读取面连 `error_code()` 都还没有，PH-5 的错误模型与谓词真值表（F-17/F-05）
> 也尚未定稿。此时先冻结一个 6 成员负数码枚举，等于让一个没设计完的错误模型先长出一半公开契约。
> 更麻烦的是 `-Wswitch` 的收益恰恰依赖「成员集合稳定」：一旦 PH-6/PH-7 需要新增
> `STATE` / `BODY_LIMIT` / 超时细分成员，所有不带 `default` 的调用方 `switch` 会集体编译变红。
> 对一个还没有外部调用方的 0.1.0，这是用将来的兼容成本换当下的命名收益；而且
> `#define` 已在 shim 里工作，数字也已被判据钉住，枚举是纯装饰。

### 回应

1. **成员不是猜测，是已实现分支的清单**。§2 每个成员都对应 `src/http/http.shim.c` 的具体行；
   其中 3 个值已写进冻结判据的输出（E-P3-2/-3/-4）。枚举不是新增承诺，而是把判据里已经冻结的
   数字搬到可拼写的位置。真正「没设计完」的部分（`STATE`/`BODY_LIMIT`/取消分类）本文明确不纳入。
2. **成员增长导致的编译红是设计意图，不是事故**。新增一种本地拒绝原因，意味着「所有本地拒绝
   我都处理了」的调用方判断过期了，应该被强制复核；这正是 `-Wswitch` 存在的理由。需要前向兼容的
   调用点写 `default:` 即可，这是每个调用点的显式选择，文档写清即可。
3. **「`#define` 已在工作」只对 C 侧成立**。§1.2 实测：`.h.ct` 的 `#define` 不进入生成头，
   CT 消费单元 `rc=1`。也就是说，今天跨单元的公开面**没有**任何可用的名字，调用方只能抄 `-1004`。
   枚举不是装饰，它是 CT 0.44.1 上唯一可行的公开常量形态。
4. **假完备风险靠拆分与命名消解，而不是靠不发布**。`local_error()` 只回答「本地为什么被拒」，
   `error_code()` 回答「传输为什么失败」，`is_ok()` 回答「成功没有」。三者分工写进 README 与 PH-5 卡，
   并明确 `local_error()==0` ≠ 成功，就不会出现「switch 写完就以为处理完了」。
5. **成本不对称**：现在做约 90 行，且不动任何冻结判据值；等到 PH-5 的错误面落地、README 与下游
   开始引用字面量之后再补名字，改动面更大且要同时教育已有调用方。0.1.0 没有外部调用方，
   恰恰是定名字最便宜的时刻。

剩余风险（接受并登记）：枚举成员集合在 1.0 前仍可能增加；若 PH-5 最终决定把本地错误并入统一的
`error_kind`，本文的 6 成员枚举应作为 `error_kind` 的本地子集保留，而不是回退成裸 `int`。

---

## 7. 若采纳，PH-5 卡至少要有这三条判据

1. `bash test/run.sh p5-local-err` 退出 0，输出覆盖 6 个成员各自的 `local_error` 值与
   `status()==0`；且 `grep -c 'switch'` 的夹具在漏一个 `case` 时构建非 0（红证）。
2. 一致性：CT 夹具里 `HTTP_LOCAL_ERR_* == HTTP_ERR_*` 六个静态断言编译通过（数值仅一处漂移即红）。
3. 语义：`local_error()==0` 时 `is_ok()` 不因枚举而改变；4xx/5xx 仍 `error_code()==0`；
   refused/timeout 仍为 `7/28`（复用 E-P4-4，不改数值）。

---

## 8. 证据索引与未复跑项

| 项 | 证据 |
| --- | --- |
| `pub enum` 跨单元、负值、`switch` | `/tmp/s05probe/e`，`rc=0`；生成头 `.ctb/codes.h` |
| `#define` 不出生成头 | `/tmp/s05probe/d`，`rc=1`；`.ctbt/defs.h` 无宏 |
| 枚举类型 `switch` 缺分支变红 | `/tmp/s05probe/e2`，`rc=1`（`-Wswitch`）；收窄正例 `/tmp/s05probe/e3`，`rc=0` |
| `.m.ct` 不能 import `.h.ct` | `/tmp/s05probe/m`，`rc=1` |
| `.m.ct` 不能声明类型 | `/tmp/s05probe/m2`，`rc=1` |
| 推荐形态端到端 | `/tmp/s05probe/m3`，`rc=0`，输出 `local -1001 status 0`；漏 case `rc=1` |
| 本仓同类先例 | `src/http/Types.h.ct:8-10`；`test/fixtures/phase1_var.ct:1-2,19` |
| 语言规范 | `ct-lang/docs/language-spec.md:732-768`（`switch`）、`:770-806`（`enum`） |

未复跑：`bash test/run.sh` / `tools/check.sh` 需执行仓库内脚本，本轮按只读评审约束未运行；
PH-1…PH-4 判据的影响判断来自卡面文本与夹具源码逐行核对，不是来自门禁重跑。
