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


续篇：[S-05-advocate-part2.md](S-05-advocate-part2.md)（§4–§8：与 requirements 的冲突、成本、反方与验收锚点）。
