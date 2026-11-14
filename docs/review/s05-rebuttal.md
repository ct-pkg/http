# S-05 反证席：ct-http 0.1.0 不应把 `HTTP_ERR_*` 枚举纳入公开错误面

- 日期：2026-11-15
- 席位：反证（结论「不纳入」）
- 议题：仅一项——公开错误面是否应纳入 `HTTP_ERR_*` 枚举。
- 对手稿：[S-05-advocate.md](S-05-advocate.md)（主张席，已把主张收窄为「6 成员本地码枚举，
  `error_code` 继续保存原始 `CURLcode`」）。
- 基线：`fa41b4f`（工作树）。`docs/requirements.md`、`docs/design.md`、`docs/phases/H1.md`、
  `ct.pkg` 有另一会话未提交的 `requires` 行改动；本评审只读，未触碰任何既有文件。
- 编译器：`/Users/samxx/.local/bin/ct`，`ct -v` 输出 `ct v0.44.1`，退出码 0。
- 探针：`/tmp/s05-probe/`（仓库外）；命令、原始输出与退出码见 §4、§9。
- 范围：只读评审；不改 `docs/requirements.md`、`docs/design.md` 与任何实现；结论只落本文件。

## 0. 判定摘要

| 形态 | 判定 | 一句话理由 |
| --- | --- | --- |
| 原 S-05：12 个 `HTTP_ERR_*`，替换 `error_code` 的值域 | **否** | 直接违反 R-14 的三条判据；若本地码另起编号，还会让 PH-3 的 `-1004/-1005` golden 变红。 |
| 主张席窄版：6 个本地码枚举，`error_code` 原值不动 | **0.1.0 仍不纳入** | 语言可行、R-14 不冲突，但它的可观察性收益来自「暴露 `local_error()`」这一独立改动，枚举本身只多给 `-Wswitch`；而 `-Wswitch` 会把「追加成员」变成对采用者的编译破坏。 |
| 不纳入是否可用 | **可用** | `local_error()` 返回 `int` + shim 头宏 + `error_name()`，已在 CT 0.44.1 上构建运行通过（§1、§9 探针 G2）。 |
| 是否要先改需求 | **要**，只要纳入的是枚举（哪怕窄版） | 关闭 Q-05、改 R-11 读取面都是条款变更；必须先取得用户授权，不能塞进 PH-5 实现。 |

一句话：**枚举不是「能看见本地错误」的必要条件；把 `local_error()` 暴露出来就够了，枚举是它的可选语法糖。**

## 1. 不纳入也能用：一个可落地的错误面

### 1.1 公开面

沿用 PH-4 的响应对象，只加读取方法，不加新类型：

```text
CResponse *res = url->post()->send();

res->status();          /* HTTP 码；传输失败为 0 */
res->error_code();      /* 0 = 无传输错误；>0 = CURLcode 原值（含未知码） */
res->local_error();     /* 0 = 无本地拒绝；<0 = -1001…-1006 */
res->local_name();      /* 稳定短名：arg / header / scheme / timeout / no_handle */
res->error_name();      /* 传输侧短名：couldnt_connect / operation_timedout ... */
res->error_detail();    /* 可复现文案："phase=connect host=127.0.0.1" */
res->is_ok();           /* 2xx && error_code == 0 */
res->is_http_error();   /* error_code == 0 && status >= 400 */
res->is_transport_error(); /* error_code > 0 */
res->is_local_error();  /* local_error < 0 */
res->is_cancelled();    /* R-13 的取消位 */
```

数值空间按 R-14 与 M-09 的既定口径：

| 区间 | 含义 | 来源 |
| --- | --- | --- |
| `0` | 无错误；HTTP 4xx/5xx 也在此列 | R-14「HTTP 4xx/5xx：`error_code==0`」 |
| `>0` | `CURLcode` 原值，未知码原样透传 | R-14 表驱动 + 「未知码保留原值」 |
| `-1001…-1005` | 本地参数/校验错误，PH-3 已冻结 | `src/http/http.shim.h:48-52`、`p3-scheme.expected.out`、`p3-defaults.expected.out` |
| `-1006` | `curl_easy_init` 失败（主张席要求补上的分支） | `src/http/http.shim.c:87-88,339-341` |

名字从哪来：`http.shim.h` 已经有一组宏（`HTTP_LOCAL_ERR_ARG/HEADER/BODY/SCHEME/TIMEOUT`）。
CT 消费单元的生成 C 会自动 `#include "http.shim.h"`（§4.3 实测），所以这些名字在 CT 源码里
可直接写；再加 `HTTP_LOCAL_ERR_NO_HANDLE` 即可覆盖 `-1006`。

### 1.2 调用方实际怎么分支

下面是探针 G2 的真实输出（构建 rc=0，运行 rc=0）。调用代码与主张席
[S-05-advocate.md:224-247](S-05-advocate.md#L224-L247) 的 `switch` 形态几乎一样，只是
`local_error()` 返回 `int`、`case` 用 shim 头里的宏：

```text
local none status 200
local scheme
local header
local -9999 local:unknown
```

```text
int local = res->local_error();
switch (local) {
case 0:                       /* 无本地拒绝；成功与否仍看 is_ok() */
    break;
case HTTP_LOCAL_ERR_ARG:
case HTTP_LOCAL_ERR_HEADER:
case HTTP_LOCAL_ERR_BODY:
    /* 调用点参数错，不是网络问题，不需要重试 */
    break;
case HTTP_LOCAL_ERR_SCHEME:
    /* 连接前被拒：服务端计数必为 0 */
    break;
case HTTP_LOCAL_ERR_TIMEOUT:
case HTTP_LOCAL_ERR_NO_HANDLE:
    break;
default:
    /* 未来新增本地码：旧调用方落到这里，不会静默当成功 */
    log(res->local_name());
}
```

需要精确数字的只有 R-14 已写死的 6 个传输码（6/7/28/60/42/23）。在这些位置，数字**就是判据本身**，
换成枚举反而要再证明「枚举成员等于 6/7/28/…」。

### 1.3 为什么这不是「让调用方自己看文档」

- 谓词 `is_http_error / is_transport_error / is_local_error / is_ok / is_cancelled` 是可执行代码，
  不是表格。
- `local_name()` / `error_name()` 返回稳定短名，可直接进日志、指标标签或 `strcmp` 分支。
- 名字常量在 `http.shim.h`，CT 与 C 两侧都能写；不需要第二个 import。
- 未知本地码有 `default` 分支与 `local:unknown`，不会把新码误判成成功。

### 1.4 备选方案为什么不选

- **错误详情结构体**：会把 `local_error`、`error_detail()` 已有信息再包一层，新增公开布局、
  生命周期与释放入口；R-11 的读取面已经是 6 项，不需要第七个容器。
- **`{value, ok}` 的 error 串**：C 没有元组，落地就是 out 参数或新 struct；且会改掉
  `send()` 的收口形态与 PH-4 的判据，代价大于收益。
- **只用文档表**：不接受。本文的替代是「谓词 + 名字函数 + shim 宏 + 表」；表只是人读的索引，
  机器判据在谓词、名字函数与 golden 上。

## 2. 纳入的代价（对主张席窄版）

### 2.1 可观察性收益与枚举无关

主张席 §3.1 的问题是真的：今天 CT 调用方拿不到任何本地错误原因
（[S-05-advocate.md:214-220](S-05-advocate.md#L214-L220)）。但这个问题的修法是
**把 `local_error` 暴露到响应上**，不是枚举：

- 新访问器 `local_error()` 返回 `int`；
- `real_send` 把请求的 `local_error` 抄进响应；
- 加一个 `local_name()`（或宏名）给调用方拼写。

这三步在探针 G2 里完成，构建/运行 rc=0，**没有枚举**。枚举在这之上只多给一件事：
把值收窄成 `HttpErr` 后，`switch` 漏成员会在 `-Werror,-Wswitch` 下变红（主张席 §1.4）。
这是唯一的增量收益；其余收益（名字、可区分 file:// 与连接拒绝、跨包可 import）都可以由
访问器 + 宏 + 名字函数拿到。

### 2.2 「新增成员」的性质：ABI 不是问题，`-Wswitch` 采用者是

- 只加枚举常量：无符号、无布局、无 ABI 影响。已编译消费者保留旧值。
- **但对采用 `-Wswitch` 的用户，追加成员是源码破坏**。实测（探针 F）：

```text
3 成员 + 全覆盖 switch: build rc=0, run rc=0（输出 arg）
加第 4 个成员，同一消费单元: build rc=1
  main.ct:5:13: error: enumeration value 'types_HttpErrBody' not handled in switch
                [-Werror,-Wswitch]
```

也就是说，主张席卖的那个「穷举有牙」，恰好把「追加成员便宜」变成了
「对采用者编译失败」。按常规 semver，这是 source-breaking，至少 MINOR；
按本仓 `AGENTS.md:109-110`，未发布时整体版本随 phase 移动、不升 MAJOR，但
「追加成员 = 免费」这个前提对 `-Wswitch` 用户不成立。
这正是本议题要回答的「0.1.0 之后新增成员是什么性质变更」：**不是 ABI 破坏，而是
对采用穷举检查的调用方的编译破坏；不用 `-Wswitch` 的用户则无感。**

### 2.3 与已冻结的 PH-1…PH-4

| 位置 | 原文/事实 | 窄版枚举的关系 |
| --- | --- | --- |
| PH-1 卡 | 「不引入 … `HTTP_ERR_*`（S-05）」（`docs/phases/PH-1/card.md:42`） | 改冻结范围；判据本身不红。 |
| PH-3 golden | `p3-scheme.expected.out`：`file -1004 ftp -1004 http 0 https 0`；`p3-defaults.expected.out`：`neg -1005 3000` | 窄版保持 `-1001…-1006`，golden **不变**；这是它比原 12 版温和的地方。 |
| PH-4 卡 | 「不公开 `HTTP_ERR_*` 枚举（S-05）」（`docs/phases/PH-4/card.md:19,49`） | 同 PH-1，改冻结范围。 |
| PH-4 E-P4-4 | `p4-refused` 输出 `status 0 code 7`、`p4-timeout` 输出 `status 0 code 28`，走的是 `http_shim_last_curl_code()`（`docs/phases/PH-4/card.md:92`） | 窄版不动 `error_code`，所以这些门禁仍绿；但「门禁绿」不能证明新读取面符合 R-11，必须新增断言。 |
| PH-4 收口 | 「`http_shim_last_curl_code()` 只是测试钩子，不是公开错误面」（`docs/phases/PH-4/closeout.md:60-61`） | 同上，需要新判据。 |

结论：窄版**不会**让 PH-1…PH-4 的命令变红（除非改 `-1001…-1005`），但它改了两张卡的冻结范围，
并且现有门禁无法证明 `local_error()` 的语义。用「门禁全绿」给窄版背书是不成立的。

### 2.4 跨包对齐

- `ct-json` 0.1.0 的错误码是单文件内的 `#define JSON_ERR_EOF 1` … `JSON_ERR_DEPTH 10`
  （`ct-json/json.ct:44-53`），README 明确它是「single CT file」；不是可 import 的跨单元公开枚举，
  没有「同一套编号」可对。
- `ct-llm-sse` 的 R-A-04 是「非 2xx 直接 `ERROR(HTTP_STATUS)`；2xx + content-type 再分流」
  （`ct-llm-sse/docs/requirements.md:82`），它需要的是状态码与 content-type，**不是本地错误码**。
- `ct-token` 的 requirements 没有 HTTP 错误码面。
- 反向证据：`ct-fsguard` 选了 `pub enum FsErrCode` 并冻结 10 个值
  （`ct-fsguard/docs/requirements.md:43-52`）。但它的错误码是 API 主返回值，且没有
  「原始 `CURLcode` 必须原样保留」这种外部契约；与本包不同形。

主张席说「枚举是把跨包判断变成可 import 的唯一低摩擦方式」
（[S-05-advocate.md:255-256](S-05-advocate.md#L255-L256)）。事实是：只要下游 import 了
`http::http`，它的生成 C 就会包含 `http.shim.h`，宏名同样可见（§4.3）。枚举的独有点仍只剩
`-Wswitch`；而今天没有任何下游包声明需要按 6 个本地码分支。

## 3. 与 requirements 的冲突

### 3.1 原文

- **R-13**（`docs/requirements.md:159-167`）：
  「`error_code==CURLE_ABORTED_BY_CALLBACK(42)`，`is_cancelled()==1`、`is_ok()==0`」；
  非目标「不支持跨线程取消、异步取消、信号触发」。
- **R-14**（`docs/requirements.md:169-179`）：
  「表驱动：DNS 失败 6、连接拒绝 7、超时 28、TLS 校验 60、回调中止 42、写错误 23；
  `error_code` 等于上表」；「HTTP 4xx/5xx：`error_code==0`」；
  「未知 `CURLcode`：`error_code` 保留原值，不归零」；
  非目标「不把 libcurl 的 enum 全量导出」；「本地错误码空间待确认（Q-05）」。
- **R-11**（`docs/requirements.md:138-146`）：
  读取面断言 `error_code`、`error_msg`、`error_detail()`、`is_ok()`、`is_cancelled()`；
  负例「404 却 `error_code!=0` 必须变红」。
- **R-10**（`docs/requirements.md:127-136`）：
  「404/500 时 `is_ok()==0` 且 `error_code==0`」。
- **R-12**（`docs/requirements.md:148-157`）：
  回调第 1 片返回非 0 → `CURLE_WRITE_ERROR(23)`，`is_cancelled()==0`。
- **R-06**（`docs/requirements.md:86-94`）：
  `timeout(1)` 后 `error_code == CURLE_OPERATION_TIMEDOUT(28)`。

### 3.2 两种纳入各要改哪些条

| 形态 | 需要改的条款 | 性质 |
| --- | --- | --- |
| 原 12 版：`error_code` 的值域换成 `HTTP_ERR_*` | R-14 的 6 个码、4xx/5xx==0、未知码保留；R-13 的 42；R-10 的 404/500==0；R-12 的 23；R-06 的 28；R-11 的读取面；PH-3 的 `-1004/-1005` golden | **判据改写**，不是措辞修订 |
| 主张席窄版：本地码枚举，`error_code` 原值 | R-14 增加「本地错误以 `HttpErr` 公开，取值 -1001…-1006」；关闭 Q-05；M-09 改「已定」；R-11 读取面增加 `local_error()` | 条款变更；R-14 的六行传输表**不动** |

窄版确实避开了 R-14 的表驱动，这一点主张席说得对。但它不是「无需求变更」：
Q-05（`docs/requirements.md:258`）与 M-09（`:246`）仍要从「待确认」变成「已定」，
R-11 的读取面多一项。这些都必须先改需求，不能在 PH-5 实现里边写边定。

### 3.3 是否越权

- `docs/requirements.md:287`：「不裁决 Q-01～Q-13；除 Q-01 外，每条在立项评审时定稿，
  定稿前对应条目的判据保持「待确认」。」Q-05 正是本地码空间与 `CURLcode` 是否混用。
- `docs/planning/ct-http-0.1.0-features.md:103`：「`docs/requirements.md` 本轮不改，
  等用户裁定后再改。」S-05 在同文件 `:93-99` 是待裁定项。
- `docs/phases/PH-4/card.md:52`：「不改 `docs/requirements.md`、`docs/design.md`」。

所以窄版的正确定序与主张席 §4.4 相同：**先裁定 → 改 requirements → 再进 PH-5 卡**。
在裁定前写枚举，就是抢跑。本文不指控主张席抢跑；本文的结论是：**既然必须先走一轮
需求变更，就更要把「值不值得为 `-Wswitch` 走这一轮」算清楚。**

## 4. `#define` 与 `pub enum` 的实测对照（CT 0.44.1）

统一命令形如：

```sh
/Users/samxx/.local/bin/ct build main.ct -o app --outdir build
```

### 4.1 `pub enum` 在 `.h.ct`

```text
$ ct build main.ct -o app --outdir build
'cc' -std=c99 -I 'build' 'build/main.c' 'build/Types.c' '-o' 'app'
rc=0
```

生成头 `build/Types.h`：

```c
enum types_HttpErr {
    types_HttpOk = 0,
    types_HttpErrArg = (-1001),
    types_HttpErrTimeout = (-1005)
};
```

消费单元 `build/main.c` 里出现 `#include "Types.h"`，取值运行 rc=0。
**结论：`pub enum` 成员进生成头，可跨单元使用；但名字被命名空间改写**
（本包 namespace 为 `http`，对应 `http_HTTP_ERR_*`，不是提案里的裸 `HTTP_ERR_*`）。

### 4.2 `#define` 在 `.h.ct`

```text
$ ct build main.ct -o app --outdir build
'cc' -std=c99 -I 'build' 'build/main.c' 'build/Errs.c' '-o' 'app'
main.ct:3:14: error: use of undeclared identifier 'HTTP_ERR_ARG'
1 error generated.
rc=1
```

生成头 `build/Errs.h` 只有 `pub enum ErrsDummy`；`#define` 落在 `build/Errs.c`：

```c
#define HTTP_ERR_ARG (-1001)
```

消费单元的生成 C 只包含 `Errs.h`，所以 C 编译器看不到宏。
**结论：`.h.ct` 里的 `#define` 不是公开面；与主张席 §1.2 的复测一致。**

### 4.3 但 `#define` 放在 `http.shim.h` 时可用

CT 消费单元的生成 C 会自动 `#include "http.shim.h"`（实测 `build/main.c` 第 3 行），
所以 shim 头里的宏对 CT 调用方可见：

```text
$ ct build main.ct -I <repo>/src -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
rc=0
$ ./app
ct-http/0.1.0 -1004
rc=0
```

**结论：若目标只是「调用方在 CT 源码里写 `HTTP_ERR_*` 这个名字」，不需要 `pub enum`；
`http.shim.h` 里的宏已经能做到，而且不需要第二个 import。** 代价是宏不受 CT 语义检查、
占用全局宏命名空间、没有命名空间隔离；但它没有 C 侧名字改写的问题。
主张席 §1.2 只测了 `.h.ct` 里的 `#define`，没有测 shim 头这条路径。

### 4.4 `pub enum` 不能放在 `.m.ct`

```text
$ ct build main.ct -I . -o app --outdir build
./api/api.m.ct:7:5: error: a .m.ct unit cannot declare a type
ctc: 1 error(s) generated
rc=1
```

本包公开面是 `.m.ct`（`docs/planning/ct-http-0.1.0-roadmap.md:26`），所以枚举只能放进
另一个 `.h.ct` 单元（如现有的 `Types.h.ct`），消费者要**同时 import 两个单元**。
主张席 §1.3 也测到了同一结论。

### 4.5 第二 import 陷阱与名字改写

只 `import api::api;`、使用 `Types.h.ct` 里的 `HTTP_ERR_ARG`：

```text
main.ct:5:39: error: use of undeclared identifier 'HTTP_ERR_ARG'
rc=1
```

同时 `import api::api;` 与 `import api::Types;`：

```text
rc=0
$ ./app
7 -1001
```

这正是 README 已警告的第一条接口陷阱（`README.md:117-121`：只 import 映射单元才有方法，
只 import 类型单元没有方法）。枚举把它扩大成第二份：**方法要一个 import，错误名要另一个 import，
少一个就在 C 编译期报未声明**。而 `http.shim.h` 的宏没有这个陷阱。

### 4.6 `-Wswitch`：唯一的枚举增量收益，也是追加成员的破坏点

探针 F（`/tmp/s05-probe/final/switch`）：

```text
3 成员 + 全覆盖 switch，-Wall -Wextra -Werror:
  build rc=0, run rc=0（输出 arg）
加第 4 个成员，消费单元一字未改:
  build rc=1
  error: enumeration value 'types_HttpErrBody' not handled in switch [-Werror,-Wswitch]
```

这证明两件事：①主张席的「穷举有牙」是真的（漏成员编译失败）；②**追加成员对采用者不是免费的**，
它会让已通过编译的消费单元在下一次构建时失败。主张席把 0.1.0 现在做列为低成本，
但没有把「追加成员对 `-Wswitch` 采用者的反应」算进去。

### 4.7 差异汇总

| 写法 | 生成头可见 | 跨单元可用 | C 侧名字 | 第二 import | `-Wswitch` | 实测 rc |
| --- | --- | --- | --- | --- | --- | --- |
| `pub enum` in `.h.ct` | 是 | 是 | 被 mangling（`http_HTTP_ERR_*`） | 需要 | 有（收窄后） | 0（A） |
| `#define` in `.h.ct` | 否 | 否 | 不出现 | — | 无 | 1（B1/B2） |
| `#define` in `.shim.h` | 不需要（自动 include） | 是（含下游包） | 原样 `HTTP_ERR_*` | 不需要 | 无 | 0（C） |
| `pub enum` in `.m.ct` | 不允许 | 不允许 | — | — | — | 1（D） |

这张表是本议题的事实基础：**「不纳入枚举」不等于「放弃命名常量」；枚举也不是唯一能让
`HTTP_ERR_*` 出现在 CT 源码里的手段。枚举的独有点只有 `-Wswitch`。**

## 5. 成本

### 5.1 改动面（窄版，按主张席 §5 的量级）

| 项 | 改动 | 量级 |
| --- | --- | --- |
| 新 `src/http/Errors.h.ct` | 6 成员 `pub enum HttpErr` | ~14 行 |
| `http.shim.h` | 响应尾字段 `local_error` + 访问器；`HTTP_LOCAL_ERR_NO_HANDLE` | ~4 行 |
| `http.shim.c` | 抄 `local_error` 进响应；`easy==NULL` 记 `-1006`；访问器 | ~6 行 |
| `http.m.ct` | `local_error()` + map recipe | ~2 行 |
| g3 一致性断言 | 逐成员断言 `HTTP_LOCAL_ERR_* == HTTP_ERR_*` | ~8 行 |
| 新夹具 | `p5-local-err.ct` + expected + run.sh/check.sh 接线 | ~30 行 |
| README | 错误面段落、删待裁定项 | ~10 行 |
| 合计 | 6 个既有文件 + 1 新单元 + 1 新夹具 | **~90 行** |

90 行本身不是问题；问题是它换来的公共契约：第二个 import、两套编号（shim 宏 + 枚举）、
两个错误轴（`local_error` 与 `error_code`），以及 §4.6 的追加破坏。

### 5.2 ABI

- 只加枚举常量：无符号、无布局、无 ABI 影响。
- 加 `local_error()` 访问器：新增 shim 导出符号，属追加；C 消费者重链即可。
- 把 `local_error` 存进 `struct HttpResponse`：**改结构体布局**。`http.shim.h:56-61` 是公开 C 布局，
  PH-3 夹具直接读 `local_error` / `headers`（`test/fixtures/p3-header-crlf.c:19-25`）；
  尾部追加源码兼容，但所有 C 消费者与夹具要重编。0.1.0 未发布，不触发 MAJOR，但这是实打实的改动面。

### 5.3 是否要在 PH-5 之前先改需求文档

- 窄版：**要**。Q-05 不定稿，`local_error` 的数值空间就没有需求依据；R-11 不修，读取面
  从 6 项到 7 项就是无据扩张。
- 原 12 版：**更要**。R-14 的六行表驱动会被直接改写。
- 结论：无论哪条，都不能在 PH-5 实现里边写边定；要么先拿需求修订授权，要么 0.1.0 不做。

## 6. 最强正方论点（窄版 steelman）与正面回应

### 6.1 正方最强版本

> 今天的 CT 调用方拿不到任何本地错误原因（主张席 §3.1，属实）。把 `local_error` 暴露成
> `HttpErr` 枚举后，调用方可以写全覆盖 `switch`，漏一个成员就在 `-Werror,-Wswitch` 下变红
> （主张席 §1.4 实测 rc=1）。这比「裸 int + 文档表」多一道编译期保证。数值保持
> `-1001…-1006`，R-14 的传输表一字不动，PH-3 golden 不变；成本约 90 行，ABI 只做尾字段追加。
> 0.1.0 尚未发布，现在是最便宜的时候；等发布后再加，才要背兼容包袱。

这版不是稻草人：它准确、克制，而且主动排除了「替换 `CURLcode`」这个错形态。

### 6.2 回应

1. **可观察性收益与枚举无关**：主张席 §3.1 的问题由「暴露 `local_error()`」解决，
   而访问器本身返回 `int`（主张席 §1.3 也承认映射单元写不出 `HttpErr` 类型）。
   探针 G2 用 `int` + shim 宏完成了同样的 `switch`，构建/运行 rc=0。枚举在这之上只多给
   `-Wswitch`，不是可观察性。
2. **`-Wswitch` 的收益是 opt-in 且脆弱的**：访问器返回 `int`，调用方必须手动收窄成 `HttpErr`；
   不收窄就没有检查（主张席 §1.4 末尾自己写了这条）。C 的 enum 不是强类型，错误 cast 可以骗过
   编译器；`default` 分支仍然是未知码的必需品，所以「穷举」并不封闭。
3. **`-Wswitch` 把追加变成破坏**：探针 F 证明，加第 4 个成员会让已编译通过的消费单元
   rc=1。主张席说「追加成员便宜」，对不用 `-Wswitch` 的人成立，对用它的人不成立。
   这正好回答本议题的版本问题：**新增成员的代价不是 ABI，而是对采用者的源码破坏。**
4. **名字不需要枚举**：shim 头宏已经给出裸 `HTTP_ERR_*`，且随 `http::http` 的生成 C 传递给
   下游包（§4.3）；枚举反而给 C 侧 mangled 名字（`http_HTTP_ERR_*`），还要加一条 g3 断言
   去绑定两套表。两套表比一套宏更容易漂移。
5. **6 个本地码不改变调用方动作**：ARG/HEADER/BODY/SCHEME/TIMEOUT/NO_HANDLE 全都意味着
   「连接前被拒、修参数、不要重试」。为一个单一动作做 6 路穷举，是仪式，不是能力。
   真正需要不同动作的是传输码（超时 vs DNS vs TLS），而 R-14 已经用原始码覆盖。
6. **`NO_HANDLE` 的行为修复独立于枚举**：把 `easy==NULL` 记成 `-1006`、
   在 `local_name()` 里给出 `no_handle`，就满足「不吞错」；不需要先冻结一个公开枚举。
7. **跨包收益是假设**：今天没有任何下游包声明要按 6 个本地码分支；`ct-llm-sse` 要的是
   状态码与 content-type（`ct-llm-sse/docs/requirements.md:82`）。为一个假设中的消费者
   先冻结公开契约，顺序不对。

## 7. 对我不利的实测与退路

必须写明：

- 如果产品要求的是「**CT 原生的具名枚举类型**」，不纳入做不到：`.h.ct` 的 `#define` 不可导出
  （§4.2，rc=1），shim 宏虽然可见但没有类型，也没有 `-Wswitch`（§4.6）。此时我这侧在
  「类型化」这一点上退到主张席的窄版，不能硬说「宏一样好」。
- 如果团队把「漏一个本地码要编译失败」当作硬需求，那么 `-Wswitch` 是真实收益，窄版是对的选择；
  但必须接受 §4.6 的追加破坏，并在文档里写清「新增成员会让采用者重新编译失败」。
- 旁证：`ct-fsguard` 选了 `pub enum FsErrCode` 并冻结数值（`ct-fsguard/docs/requirements.md:43-52`）。
  这支持「枚举在别的包可行」，但不支持「http 必须现在做」：它的错误码是主返回值，
  没有 R-14 这种「原始 `CURLcode` 逐项相等」的外部契约。

**退到哪里**：若用户裁定必须纳入，取主张席的窄版，不取原 12 版——
保持 `error_code` 原始 `CURLcode`，枚举只覆盖 `-1001…-1006`，先关闭 Q-05、修 R-11，
再把「追加成员对 `-Wswitch` 采用者是源码破坏」写进 README 与版本说明。

## 8. 结论与最小裁定建议

- **S-05 反证席判定：NO**。0.1.0 不把 `HTTP_ERR_*` 枚举纳入公开错误面；
  原 12 版（替换 `error_code`）明确拒绝，窄版（6 成员本地枚举）也不建议在 0.1.0 做。
- 0.1.0 采用 §1 的形态：`error_code` 原值 + `local_error()` 负数码（`-1001…-1006`）+
  谓词 + `local_name()` / `error_name()` / `error_detail()`；裸名字用 `http.shim.h` 的宏，
  不用 `.h.ct` 的 `#define`。
- 如果以后确实需要枚举：作为**独立的、需求授权后的变更**进入，先关闭 Q-05、改 R-11，
  再开卡；不要塞进 PH-5 实现。采用时同步登记「追加成员会让 `-Wswitch` 采用者编译失败」。
- 变更门槛建议：只有当某个真实下游需要「按 6 个本地码分支」或「漏码必须编译失败」时，
  才值得为枚举走一轮需求变更；在那之前，`int` + 宏 + 名字函数已经覆盖全部现有判据。

## 9. 探针索引

全部在 `/tmp/s05-probe/`，不属于仓库。统一用 `/Users/samxx/.local/bin/ct`（v0.44.1）。

| 探针 | 内容 | 命令 | rc |
| --- | --- | --- | --- |
| A `/tmp/s05-probe/final/enum` | `pub enum` 在 `.h.ct`，跨单元取值 | `ct build main.ct -o app --outdir build` | 0；运行 0 |
| B1 `/tmp/s05-probe/final/define` | 仅 `#define` 在 `.h.ct` | 同上 | 1（`use of undeclared identifier 'HTTP_ERR_ARG'`） |
| B2 同上 | `#define` + `pub enum` 占位 | 同上 | 1（同文案） |
| C `/tmp/s05-probe/final/macro` | `http.shim.h` 的宏从 CT 使用 | `ct build main.ct -I <repo>/src … --ldflags "-lcurl"` | 0；运行输出 `ct-http/0.1.0 -1004`，rc=0 |
| D `/tmp/s05-probe/final/mtype` | `pub enum` 放 `.m.ct` | `ct build main.ct -I . -o app --outdir build` | 1（`a .m.ct unit cannot declare a type`） |
| E `/tmp/s05-probe/final/trap` | 枚举在独立 `.h.ct`：单 import vs 双 import | 同上 | 单 import 1；双 import 0，运行输出 `7 -1001` |
| F `/tmp/s05-probe/final/switch` | `-Wswitch` 全覆盖；加成员后同一消费单元 | `ct build main.ct -I . -o app --outdir build --cflags "-Wall -Wextra -Werror"` | 3 成员 0；加第 4 成员 1（`-Werror,-Wswitch`） |
| G2 `/tmp/s05-probe/final/altlocal` | 不含枚举的 `local_error()` + shim 宏 + `switch` | 同上 | 0；运行 0（4 行分类输出） |
| H `/tmp/s05-probe/alt` | 不含枚举的谓词 + `error_name` + `error_detail` | 同上 | 0；运行 0（6 行分类输出） |

证据日志：`/tmp/s05-probe/final/evidence.log`（A/B/C/D/E）。
