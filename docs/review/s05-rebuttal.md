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


续篇：[s05-rebuttal-part2.md](s05-rebuttal-part2.md)（§4–§9：语言实测对照、成本、正方 steelman 与结论）。
