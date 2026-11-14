# ct-http / S-05 主张席：0.1.0 公开错误面应纳入 `HTTP_ERR_*` 枚举

- 日期：2026-11-15
- 立场：**纳入**。
- 本文范围：只做评审与实测取证。不改 `docs/requirements.md`、`docs/design.md`、任何实现、门禁与卡片；
  可本地提交，不 push、不打 tag。
- 主张的准确边界：纳入的是**本地校验错误空间的 6 成员枚举**，`error_code` 继续保存原始 `CURLcode`。
  本文**不主张**「用枚举替换 `CURLcode` 数值」，也**不主张**照抄草案的 12 个成员。
- 编译器：`/Users/samxx/.local/bin/ct`，`ct v0.44.1`（实测 `ct --version` 输出 `ct v0.44.1`）。
- 探针目录：`/tmp/s05probe/{e,d,e2,e3,m,m2,m3}`；均为仓库外文件，未改本仓。

---

## 0. 结论摘要

| # | 判定 | 依据 |
| --- | --- | --- |
| 1 | 语言层可行：`pub enum` 能从 `.h.ct` 导出，跨单元按裸名/限定名使用，负值合法 | §1.1，`rc=0`，生成头 `enum app_HttpErr { app_Arg = (-1001) … }` |
| 2 | `#define` 不能作为跨单元公开面 | §1.2，`rc=1`，`use of undeclared identifier 'HTTP_ERR_ARG'`，生成头无该宏 |
| 3 | 最小集是 6 个成员，不是 12 个：现有实现只会产生 5 个本地码，另有 1 条分支当前无码可报 | §2 表，逐条对应 `src/http/http.shim.c` 行号 |
| 4 | `switch` 全覆盖可以变成编译期红，但前提是调用方把值收窄为 `HttpErr`；映射方法只能返回 `int` | §1.3/§1.4，`-Werror,-Wswitch` 实测 |
| 5 | 与 R-14 不冲突，但需要**修订 requirements 关闭 Q-05**；属条款变更（待确认→定稿），不是措辞澄清 | §4 |
| 6 | 成本约 90 行 / 6 个文件 + 1 个夹具；不触碰已冻结的 PH-1…PH-4 判据值；ABI 只做尾字段追加 | §5 |

---

## 1. 语言层可行性（实测）

### 1.1 `pub enum` 从 `.h.ct` 导出并跨单元使用

文件：

```ct
// /tmp/s05probe/e/app/codes.h.ct
namespace app;

pub enum HttpErr {
    Ok = 0,
    Arg = -1001,
    Header = -1002,
    Body = -1003,
    Scheme = -1004,
    Timeout = -1005,
    NoHandle = -1006
}
```

```ct
// /tmp/s05probe/e/main.ct
#include <stdio.h>
import app::codes;

int main(void)
{
    int code = Arg;          /* 裸名 */
    int q = app::Timeout;    /* 限定名 */
    switch (code) {
    case Ok:      printf("ok\n"); break;
    case Arg:     printf("arg\n"); break;
    case Header:
    case Body:
    case Scheme:
    case Timeout: printf("other %d\n", code); break;
    default:      printf("unknown %d\n", code); break;
    }
    printf("values %d %d %d\n", (int)Arg, q, (int)Ok);
    return 0;
}
```

命令与结果：

```sh
cd /tmp/s05probe/e && /Users/samxx/.local/bin/ct build main.ct -I . -o out \
    --outdir .ctb --cflags "-Wall -Wextra -Werror"
# build rc=0
./out
# arg
# values -1001 -1005 0
# run rc=0
```

生成头 `.ctb/codes.h`（原文）：

```c
enum app_HttpErr {
    app_Ok = 0,
    app_Arg = (-1001),
    app_Header = (-1002),
    app_Body = (-1003),
    app_Scheme = (-1004),
    app_Timeout = (-1005),
    app_NoHandle = (-1006)
};
```

结论：负值初值被原样保留（`= (-1001)`），成员在消费单元同时支持裸名与 `app::` 限定名，
`switch` 的 `case` 直接写成员名即可。语言规范 §4.14
（`/Users/samxx/trae-ws/ct-lang/docs/language-spec.md:804-806`）与官方夹具
（`test/modules/enum_export/app/kinds.h.ct`）与本次实测一致。

本仓也已有同类先例：`src/http/Types.h.ct:8-10` 的 `pub enum HttpDefaults`，
消费夹具 `test/fixtures/phase1_var.ct:1-2,19` 同时 `import http::http;` 与
`import http::Types;` 后直接使用裸名 `HttpTimeoutDefaultMs`。

### 1.2 `#define` 不能进生成头（既知事实的复测）

文件：

```ct
// /tmp/s05probe/d/app/defs.h.ct
namespace app;

#define HTTP_ERR_ARG (-1001)

pub enum HttpErr {
    Ok = 0,
    Arg = -1001
}
```

```ct
// /tmp/s05probe/d/main.ct
#include <stdio.h>
import app::defs;

int main(void) { printf("%d\n", HTTP_ERR_ARG); return 0; }
```

命令与结果：

```sh
cd /tmp/s05probe/d && /Users/samxx/.local/bin/ct build main.ct -I . -o out \
    --outdir .ctbt --cflags "-Wall -Wextra -Werror"
# build rc=1
# main.ct:4:20: error: use of undeclared identifier 'HTTP_ERR_ARG'
```

同一个 `.h.ct` 生成的头 `.ctbt/defs.h` 只有 `enum app_HttpErr { app_Ok = 0, app_Arg = (-1001) };`，
**没有** `HTTP_ERR_ARG`。也就是说：同一文件里，`pub enum` 的成员到了消费单元，`#define` 没有。

### 1.3 两个硬边界（决定了枚举必须单独成单元、访问器只能返回 `int`）

| 假设 | 实测 | 结果 |
| --- | --- | --- |
| `.m.ct` 可以 `import` `.h.ct` | `/tmp/s05probe/m`：在 `http.m.ct` 副本顶部加 `import http::Types;` 后 `ct build` | `rc=1`，`error: a .m.ct unit can only import another .m.ct unit` |
| `.m.ct` 可以声明 `pub enum` / `pub type X = …` | `/tmp/s05probe/m2`：把 `pub enum HttpErr` 写进 `api.m.ct` | `rc=1`，`error: a .m.ct unit cannot declare a type` |

推论：枚举只能像 `Types.h.ct` 一样坐在独立 base 的 `.h.ct` 里；映射单元的 trait 签名写不出
`HttpErr`，所以公开访问器的返回类型只能是 `int`，调用方在本地收窄一次：

```ct
HttpErr local = res->local_error();   /* int -> enum，实测零警告 */
```

### 1.4 `switch` 全覆盖：编译期红实测

收窄后，`switch` 缺成员会直接让 `cc` 在 `-Wall -Wextra -Werror` 下失败。完整形态探针
（`/tmp/s05probe/m3`：映射单元 + 独立 `Errors.h.ct` + 双 `import`）：

```sh
cd /tmp/s05probe/m3 && /Users/samxx/.local/bin/ct build main.ct -I . -o out \
    --outdir .ctb --cflags "-Wall -Wextra -Werror"
# build rc=0
./out
# local -1001 status 0
```

漏 `case` 的同一调用点（`main4.ct` 故意漏 `HTTP_ERR_NO_HANDLE` 与 `HTTP_ERR_TIMEOUT`）：

```sh
cd /tmp/s05probe/m3 && /Users/samxx/.local/bin/ct build main4.ct -I . -o out4 \
    --outdir .ctb4 --cflags "-Wall -Wextra -Werror"
# build rc=1
# main4.ct:6:13: error: enumeration values 'api_HTTP_ERR_NO_HANDLE' and
#   'api_HTTP_ERR_TIMEOUT' not handled in switch [-Werror,-Wswitch]
```

另测：`int` 运行时值赋给 `HttpErr`（`/tmp/s05probe/e3`）`rc=0`，说明「shim 返回 int、调用方收窄」
不会引入警告。反过来，若调用方把值留在 `int` 上再 `switch`，`-Wswitch` 不触发——这条收益
**依赖调用点显式收窄**，必须写进文档，不能含糊成「switch 一定完备」。

---

## 2. 最小枚举集：从现有分支反推，不是从草案抄

现有实现里会写 `local_error` 的分支（逐条读码）：

| 成员（建议值） | 触发分支（原文位置） | 现有可观察性 |
| --- | --- | --- |
| `HTTP_ERR_ARG = -1001` | `src/http/http.shim.c:200-202`（`user_agent(NULL)`）、`:207-209`（拷贝失败） | 仅 `local_error` 内部字段 |
| `HTTP_ERR_HEADER = -1002` | `src/http/http.shim.c:130-133`（名/值为 NULL 或含 CR/LF）、`:138-141`（拼接分配失败）、`:147-151`（slist 追加失败） | 同上；`p3-header-crlf.c:21-22` 以宏比较 |
| `HTTP_ERR_BODY = -1003` | `src/http/http.shim.c:255-258`（`data==NULL` 或 `len<0`）、`:264-267`（拷贝分配失败） | 同上；`p3-body-nul.c:28` 以宏比较 |
| `HTTP_ERR_SCHEME = -1004` | `src/http/http.shim.c:84-86`（`!has_http_scheme`） | `p3-scheme.c`，且 E-P3-4 把 `-1004` 写进冻结判据 |
| `HTTP_ERR_TIMEOUT = -1005` | `src/http/http.shim.c:164-167`（`timeout(ms<0)`）、`:182-185`（`connect_timeout(ms<0)`） | `p3-defaults.c:34` 打印该值 |
| `HTTP_ERR_NO_HANDLE = -1006` | `src/http/http.shim.c:87-88`（`curl_easy_init()` 返回 NULL）→ `:339-341` 短路返回 `status 0` 响应 | **当前无码可报**：`local_error` 仍为 0，`g_last_curl_code` 为 0 |

为什么不是 12 个：草案的其余成员在现有实现里没有产生分支——`STATE`（`body`/`post_stream`
互斥）要到 PH-7，`BODY_LIMIT` 要到 PH-7 的流式上限，`CANCEL` 要到 PH-8，TLS 失败是
`CURLcode 60` 属传输空间而非本地空间（`docs/requirements.md:173` 已把它放进表驱动）。
把没实现的分支先写进公开枚举，是把设计猜测固化成契约。

为什么 `NO_HANDLE` 必须在：`curl_easy_init` 失败是**已经存在**的分支，只是今天静默成
`status 0`。枚举若跳过它，恰好在本仓最弱的一条错误路径上保持不可观察，违背
`docs/requirements.md:171`（「三类不能混；未知码不得吞掉」）。

不纳入成员：响应结构体分配失败（`src/http/http.shim.c:328-331`）直接返回 NULL，没有载体可挂码，
继续按「NULL 即失败」处理，本文不扩面。

---

## 3. 调用方收益（用调用代码说明）

### 3.1 今天：CT 调用方拿不到任何错误原因

公开面只有 `status()/body()/body_len()/total_bytes()/take_body()/free()`
（`src/http/http.m.ct:33-40`）。本地拒绝与传输失败都表现为 `status()==0`，
唯一能看原因的 `http_shim_last_curl_code()` 是 C 侧测试钩子，未映射进 CT
（`src/http/http.shim.h:110-111`）。调用方想区分 `file://` 被拒与连接被拒，只能读 C 夹具的
`local_error` 或把 `-1004` 抄进自己的代码。

### 3.2 纳入后：两类错误分开问，本地空间可穷举

```ct
import http::http;
import http::Errors;

CResponse *res = r->send();
HttpErr local = res->local_error();     /* 0 或负值；与传输码分开 */
switch (local) {
case HTTP_OK:
    /* 没有本地拒绝；成功与否仍看 res->status()（以及 PH-5 的 error_code()） */
    break;
case HTTP_ERR_ARG:
    break;
case HTTP_ERR_HEADER:
case HTTP_ERR_BODY:
    /* 调用点参数错，不是网络问题，不需要重试 */
    break;
case HTTP_ERR_SCHEME:
    /* 连接前就被拒：服务端计数必为 0 */
    break;
case HTTP_ERR_TIMEOUT:
case HTTP_ERR_NO_HANDLE:
    break;
}
```

具体收益：

1. **穷举有牙**：漏一个成员，构建在 `-Werror,-Wswitch` 下变红（§1.4 的 `rc=1` 原文）。
   用裸 `int` + 文档表格做不到这一点。
2. **命名稳定**：`-1004` 已经在冻结判据 E-P3-4 的输出里（`docs/phases/PH-3/card.md:81`），
   枚举只是给这个已冻结的数字一个可拼写的名字；数字与名字由一条编译期断言绑定（§5）。
3. **跨包一致**：`#define` 跨不了单元（§1.2），所以别的包想在 CT 里判断「scheme 被拒」，
   今天只能硬编码 `-1004`。枚举是把这条判断变成可 import 的唯一低摩擦方式。

诚实边界：`local_error()==0` **不等于**成功；`is_ok()` 仍是唯一成功谓词，传输失败仍在
`error_code()`（`CURLcode` 原值）。这条必须写进 README，否则枚举会制造「本地没问题＝请求没问题」
的假完备。

---

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
