# ct-http 规范草案 · 可实现性评审（advocate）

> 归档：2026-11-15 由未跟踪草稿 `docs/review/spec-advocate.md` 移入
> `docs/reviews/spec-review/`，正文未改。

- 日期：2026-11-14
- 定位：只回答「草案按 CT v0.44.1 能否实现、需要怎么改写」；传输语义与既有需求的冲突见同目录
  [spec-rebuttal.md](spec-rebuttal.md)，本文不重复。
- 编译器：`/Users/samxx/.local/bin/ct` `v0.44.1`（不在 PATH）。
- 探针：全部在 `/tmp/cthttp-probe`，未改本仓源码；`ct check` 不查方法派发，以下结论一律以
  `ct build` 为准。
- 总判定：作为 v0.2 基线 **NO-GO**。9 条中只有 §1 可完全照原文落地；§3/§5/§7/§9 需小改，§2/§6/§8
  按原文不成立，§4 有条件成立。硬阻塞只有一条——`char *` 宿主 + 单表达式链；其余都有等价写法。

## 0. 判定总表

| 草案要点 | 判定 | 关键事实 |
| --- | --- | --- |
| §1 隐藏 libcurl，只暴露标量/指针 API | 可行 | CT 侧不 `#include <curl/...>`，shim + `.m.ct` 是既有路径；无语言阻塞 |
| §2 字符串字面量起手、`method/send/send_stream` 挂 `char *` | **不可行（硬）** | `char *` 不能加固有/trait 方法；字面量不能作接收者；链式也不成立（A） |
| §3 配置方法全返回 self、默认值表 | 可行，但**不能链** | 返回 `self` 合法；`s.m1().m2()` 编译失败；指针接收者必须 `->`（A/F） |
| §4 `cancel()` 跨线程 + `atomic int` | 有条件可行 | `atomic int` 可用；对象寿命不由 atomic 保护，且与「send 后自动释放」冲突（C/H） |
| §5 `HttpResponse` 字段 + 方法 | 可行，代价是布局全导出 | 公开 struct 的每个字段都进生成头，且可写；无只读字段（E） |
| §6 `send` 后自动释放、`defer { delete r; }` | **不可按原文** | 跨单元 `delete` 只 free 结构体，不执行定义单元的 Drop 体（C/D） |
| §7 回调 typedef / borrow / 非 0 中断 | 可行，闭包受限 | `pub typedef` 可导出；命名函数可用；CT 闭包不能填这个签名（G） |
| §8 12 个 `#define HTTP_*` | **不可行** | `.h.ct` 的 `#define` 不进入生成头，消费单元报未声明（B） |
| §9 实现要点与 Drop 顺序 | 大体可行，4 处语义要改 | 语言层无阻塞；GET/CUSTOMREQUEST、COPYPOSTFIELDS、LOW_SPEED、写回调超限需修正（I） |

## 1. 逐条判定

### A. 字符串宿主、字面量接收者与链式起手

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §2：`"https://api.example.com".method("GET").header("Accept","application/json").timeout(5000).send()`；省 method 时 `"url".send()` 默认 GET；三个方法「挂在 `char *` 宿主上」。 | **不成立**。`impl char * { … }` → `error: impl of an unknown trait`；`impl Http for char *` → `error: impl for undefined struct 'char'`；`"https://…".get()` → `error: no method 'get' for type 'char *'`。换成用户 struct 后，`S.make(3).get()` 与 `s.self_ret().self_ret()` 仍报 `error: method receiver must be a variable, an element, a call or a field chain`。 | 放弃 `char *` 宿主和单表达式链。用 `HttpRequest` 值对象 + `static HttpRequest *new(const char *)`；一条语句一个调用，指针接收者用 `->`。等价形态见 §3。 |

探针：`p_prim.ct`、`p_trait_ptr.ct`、`p_lit.ct`、`p_chain_static.ct`、`p_chain2.ct`（均 `build` 退出 1）。

### B. `.h.ct` 单元的边界

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §3～§8 把配置方法、公开 struct、12 个错误宏、回调 typedef 都放在包的公开面。 | **部分不成立**。顶层 `pub` 声明必须在 `.h.ct`；方法级 `pub` 只允许在 `.h.ct` 的 `pub struct` 固有 `impl` 方法上。trait impl 方法写 `pub` → `error: a trait impl method cannot be pub`；`.ct` 里写 `pub` 方法 → `error: a pub method needs a pub struct in a .h.ct unit`。`.h.ct` 内 `defer` → `error: defer is not allowed in a .h.ct unit`，`errdefer` → `error: errdefer is not allowed here`。`#define HTTP_OK 0` 只留在 `api.c`，生成头 `api.h` 没有它，消费单元报 `use of undeclared identifier 'HTTP_OK'`。`pub typedef int (*ChunkCb)(…)` 可导出；`pub int` 全局导出为 `extern int`；`pub enum` 导出为 `enum HTTPErr { … }`。 | 声明放 `.h.ct`，需要 `defer`/`errdefer` 的清理 helper 放 `.ct`；错误码改 `pub enum HttpErr { HTTP_OK = 0, … }`（或 `pub int` 全局），不要用 `#define` 做公开面。回调 typedef 保持 `pub typedef`。 |

探针：`p_pubin.ct`、`p_pubdrop.ct`、`h2/d.h.ct`、`h2/f.h.ct`、`h1/api.h.ct`、`h3/codes.h.ct`、`h6/errs.h.ct`。

### C. 跨单元 `delete` 与 Drop 体

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §6：`send()/send_stream()` 后内部自动释放请求侧资源；`HttpResponse` 用 `defer { delete r; }` 收尾，Drop 释放 body、响应头、`error_detail` 缓冲。 | **承诺不成立**。定义单元的 `Req_Drop_drop` 是 `static`，消费单元会生成一个空的同名 `static` 桩：`static void Req_Drop_drop(void *self) { struct Req *_self = (struct Req *)self; (void)_self; }`。`delete r` 只 `free(r)`，不执行定义单元的 Drop 体：`m1` 运行只打印 `val=43`，Drop 体里的 `LIB_DROP_BODY` 不出现；`m4` 的 `R_DROP` 同样不出现，body 无人释放。对照组：同单元 `delete` 打印 `SAME_UNIT_DROP`；在定义单元内包一层 `pub void req_free(Req *r) { delete r; }`，消费单元调用它，`LIB_DROP_BODY` 正常打印。 | 不要把 `delete` 当跨单元析构。公开 `pub void http_response_free(HttpResponse *)` / `http_request_free(HttpRequest *)`，函数体放在定义单元内，由它 `delete`/释放 body、slist、`error_detail`；消费单元用 `defer { http_response_free(res); }`。`send*` 不要顺手释放请求对象，否则调用方持有悬空指针。 |

探针：`m1`（直接 delete，无 Drop 体）、`m3`（定义单元内 free 包装，有 Drop 体）、`m4`（响应 body 的 Drop 不执行）、`p_drop_same.ct`（同单元对照）。自动释放后再 delete 的后果：`h5` 在 ASAN 下 `attempting double-free`，`h5u` 在 ASAN 下 `heap-use-after-free`，退出码均 134。

### D. `defer { delete r; }` 在消费单元

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §6：`HttpResponse` 用 `defer { delete r; }` 收尾。 | **语法可用，语义不达**。消费 `.ct` 单元里 `defer { delete r; }` 能编译，展开为 `if (r) { Req_Drop_drop(r); free(r); r = 0; }`，同一变量二次 delete 安全（指针被置 0）；但 `Req_Drop_drop` 是空桩，所以只释放结构体。`.h.ct` 内则直接禁止 defer。 | 保留 `defer`，把调用换成定义单元提供的 `http_response_free(res)`；或把 defer 限定在应用 `.ct`。若消费方本身是另一个包的 `.h.ct`，不能用 defer，必须显式调用 free。 |

探针：`m1/main.ct`（defer delete 展开）、`h2/d.h.ct`（`.h.ct` 禁止 defer）。

### E. 公开 struct 字段是否全部进头

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §5：`HttpResponse` 公开 `status_code / body / body_len / total_bytes / error_code`，方法 `is_ok / is_cancelled / error_detail / header / header_all / take_body`。 | **全部字段都会进生成头**，且没有字段级可见性：`struct Resp { int status_code; const char * body; int body_len; long long total_bytes; };`。字段可写，`const` 只约束 pointee，不阻止写字段；`take_body` 与公开 `body` 同时存在时所有权有二义。 | 接受公开字段就接受布局与可写性代价：文档写明「字段可读、不要写、不要 free」，并只保留一个释放入口。若「只读」是硬承诺，改用不透明 struct + 访问函数；但 CT 不允许方法只留原型，不透明类型只能用 `pub` 自由函数实现（见 §3 备注）。 |

探针：`h1/api.h.ct`（字段与 typedef 导出）、`h4/api2.h.ct`（方法原型导出）、`h7/op.h.ct`（`pub struct Opaque;` 前向声明可进头）。

### F. 指针接收者用 `.` 还是 `->`

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §5 写 `r->is_ok()`、`r->header(…)`；§2/§3 又在 `HttpRequest *` 上写 `req.header(…)`、在字面量上写 `.method(…)`。 | **指针必须 `->`，值用 `.`**。`S *p; p->get()` 正确；`p.get()` 能编过但把 `&p` 传给 `S_get`：clang 警告 `incompatible pointer types passing 'struct S **' to parameter of type 'struct S *'`，运行首值变成垃圾（实测 `1803985496 7 3`）。跨单元同样：`Resp2 *r; r.is_ok()` 打印 `0`，`r->is_ok()` 打印 `1`。`ct build` 对这类错误只给警告、退出 0。 | 规则写死：值接收者 `.`，指针接收者 `->`。草案的 `r->is_ok()` / `r->header(…)` 是**对的**，不要改成 `r.`；但 `"url".method(…)` 与 `req.header(…)`（`req` 为指针）必须改。门禁加 `-Werror=incompatible-pointer-types` 或 grep 生成 C 的 `&r`。 |

探针：`p_recv.ct`（点/箭头对照）、`h4/main_dot.ct`（跨单元点调用错误）、`h4/main.ct`（箭头正确）。

### G. 回调 typedef 导出与闭包限制

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §7：`typedef int (*HttpChunkCb)(const char *data,int len,void *ud)` 与 `HttpHeadersCb`；chunk 是 borrow；回调返回非 0 主动中断。 | **typedef 可导出，命名函数可用；闭包不行**。`pub typedef` 在生成头里是 `typedef int (*ChunkCb)(const char *, int, void *);`；传命名函数 + `ud` 运行正常（`h1/main_ok.ct` 打印 `status=200 rc=0`）。把 `(const char *d, int n, void *u) => n + base` 传给该 typedef 参数报 `error: a closure argument must fill a '(fn, env)' parameter pair`；闭包也不能存字段/作返回值。把形参写成 `int (*cb)(void *env, const char *data, int len)` 且与 `void *env` 相邻、调用时不另传 `ud`，闭包可用（`h9/main_pair.ct` 返回 `42`）。 | 保留 typedef 就必须在 README 写「只接受命名函数，`ud` 由调用方保活，回调不得保存」。若想要闭包糖，改成 env 在首位、且不用 typedef 隐藏那一对参数；这会改变 C 侧签名，需同时改 `.shim` 与测试。 |

探针：`h1/main_ok.ct`（命名函数）、`h1/main_closure.ct`（typedef + 闭包失败）、`h9/main_pair.ct`（env 首位 + 闭包成功）。

### H. `cancel()`、atomic 与对象寿命

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| §4：`.cancel()` 可从另一线程调用，`_cancelled` 用 `atomic int`，生命周期由调用方加锁保证。 | **语言层可行，契约层不闭合**。`atomic int` 降为 `_Atomic(int)`，`ct_atomic_store/load` 编译运行退出 0，`-Wall -Wextra` 无警告；但实测 `ct build` 仍以 `-std=c99` 调 `cc`（语言规范 §4.59 说存在原子单元时用 c11，本机 v0.44.1 未体现），Linux/GCC 下需自证。更重要的是：atomic 只保护标志位，不保护对象寿命；若 `send()` 结束时释放请求对象，另一线程的 `cancel()` 就落在已释放内存上。 | 二选一：①本期只允许同线程取消，跨线程删除；②保留跨线程，但 `send*` 不释放请求对象，调用方必须在 `cancel()` 返回后再 `http_request_free`，并把「调用方持锁/join」写成契约。`atomic int` 可保留，配 `ct_atomic_*`。 |

探针：`p_atomic2.ct`、`p_atomic3.ct`（编译运行均 0）；`h5/h5u` 证明自动释放 + 继续使用/删除的后果。

### I. §9 实现要点

| 草案原文 | 实测结论 | 改法 |
| --- | --- | --- |
| method → `CURLOPT_CUSTOMREQUEST`；body → `CURLOPT_COPYPOSTFIELDS`；`idle_timeout` → `LOW_SPEED_LIMIT=1` + `LOW_SPEED_TIME`；`max_body_bytes` 在 write callback 内累计；Drop 顺序 `curl_easy_cleanup` → 两个 `curl_slist_free_all` → `free(body)` → `free(error_msg)`。 | **CT 层无阻塞**，四处语义要修：① GET 也走 CUSTOMREQUEST 会绕过 libcurl 的 GET 语义，且 CUSTOMREQUEST 会跨重定向保留，跟随时方法可能错；② `COPYPOSTFIELDS` 不先设 `POSTFIELDSIZE` 时按 `strlen` 截断，二进制 body 与内嵌 NUL 会坏；③ 超限返回 0 与用户回调返回非 0 都变成 `CURLE_WRITE_ERROR(23)`，无法区分；④ `curl_easy_strerror` 返回 static，不能 `free`，只有自建的 `error_detail` 才能 free；Drop 顺序只在定义单元内有效（C）。 | GET 用 `CURLOPT_HTTPGET`，非 GET 才 CUSTOMREQUEST；`body(bytes,len)` 先设 `POSTFIELDSIZE` 再 COPYPOSTFIELDS（或自持缓冲 + POSTFIELDS）；write callback 用独立 `limit_hit` 标志映射到 `HTTP_ERR_BODY_LIMIT`，与 `HTTP_ERR_CALLBACK_ABORTED` 分开；`error_msg`/`error_detail` 的所有权写清。 |

探针：语言层无额外探针；以上为 libcurl 语义核对项，需在 H2/H3/H5 的夹具里各加一条负例。

## 2. 按原文仍可行的部分

- **隐藏 libcurl**：可行。CT 单元不 `#include <curl/...>`，`CURL *` 只出现在 `.shim.c` / `.m.ct`；`ct.pkg` 的 `ldflags = -lcurl` 既有路径成立。
- **标量配置与默认值**：可行。`timeout / connect_timeout / idle_timeout / max_body_bytes / max_redirects / insecure / user_agent` 都是 `int`/`long long`/`const char *`，没有类型层障碍。
- **配置方法返回 self**：可行。`S *set(int)` 返回 `self` 并赋值给同类型指针，跨单元可用；只是不能把返回值直接当下一跳接收者。建议把 setter 改成返回 `int` 错误码，非法参数可在调用点报出。
- **`HttpResponse` 的读取面**：可行。公开字段 + 方法在 `.h.ct` 内联实现，跨单元可用（`h4`：`r->is_ok()` 返回 1，`r->header("Content-Type")` 返回 `text/plain`）。
- **流式回调**：可行。typedef 导出、命名函数 + `ud`、非 0 中断都能落到 C shim；chunk 作为 borrow 传给回调与 CT 的 `const char *` 参数一致。

## 3. 最小可信替代 API 面（v0.2 骨架）

约束：libcurl 只在 shim；`HttpRequest`/`HttpResponse` 由调用方显式释放；setter 返回错误码；指针接收者一律 `->`；
不使用单表达式链；错误码用 `pub enum`；回调默认只接受命名函数。下面是**签名骨架**（方法体省略，落地时补；
CT 不允许方法只留原型）。

```ct
// http.h.ct —— 骨架签名；方法体必须写在 .h.ct 内（CT 不允许只留原型）
#include <stddef.h>

pub enum HttpErr {
    HTTP_OK = 0,
    HTTP_ERR_ARG = -100,
    HTTP_ERR_STATE = -101,
    HTTP_ERR_TLS = -102,
    HTTP_ERR_TIMEOUT = -103,
    HTTP_ERR_BODY_LIMIT = -104,
    HTTP_ERR_CALLBACK_ABORTED = -105
}

pub typedef int (*HttpChunkCb)(const char *data, int len, void *ud);
pub typedef int (*HttpHeaderCb)(const char *name, const char *value, void *ud);

pub struct HttpRequest {
    void *h;            // C shim 持有 CURL*；CT 侧不出现 curl 类型
    int last_error;
    atomic int cancelled;
}

pub struct HttpResponse {
    int status_code;
    char *body;         // 归响应所有；take_body 后置 0
    int body_len;
    long long total_bytes;
    int error_code;
    int cancelled;
    char *error_detail; // 自建文案；free 由 http_response_free 负责
    void *headers;      // C shim 持有 curl_slist*；不要碰
}

impl HttpRequest {
    pub static HttpRequest *new(const char *url) { /* shim 分配 + 默认值 */ }
    pub int method(const char *m) { /* 返回 HttpErr */ }
    pub int header(const char *name, const char *value) { /* 返回 HttpErr */ }
    pub int body(const char *bytes, int len) { /* 深拷贝；返回 HttpErr */ }
    pub int timeout_ms(int ms) { /* 120000 默认 */ }
    pub int connect_timeout_ms(int ms) { /* 10000 默认 */ }
    pub int idle_timeout_ms(int ms) { /* 仅流式，60000 默认 */ }
    pub int max_body_bytes(long long n) { /* 非流式 16MB、流式 64MB 默认 */ }
    pub int max_redirects(int n) { /* 10 默认 */ }
    pub int follow_redirects(int on) { /* 0 默认 */ }
    pub int insecure(int on) { /* 0 默认 */ }
    pub int user_agent(const char *ua) { /* 默认值写死并断言 */ }
    pub void cancel(void) { /* ct_atomic_store(&self->cancelled, 1) */ }
}

// 执行：不释放 req；调用方失败路径也要 http_request_free
pub HttpResponse *http_send(HttpRequest *req);
pub HttpResponse *http_send_stream(HttpRequest *req, HttpChunkCb cb, void *ud);

// 唯一释放入口；函数体在定义单元内 delete / 释放 body、headers、error_detail
pub void http_request_free(HttpRequest *req);
pub void http_response_free(HttpResponse *resp);

impl HttpResponse {
    pub int is_ok(void) { return self->status_code >= 200 && self->status_code < 300 && self->error_code == 0; }
    pub int is_cancelled(void) { return self->cancelled; }
    pub const char *header(const char *name) { /* shim 查 headers */ }
    pub char *take_body(int *len) { /* 取走所有权并把 body 置 0 */ }
}
```

消费形态（`.ct` 单元）：

```ct
HttpRequest *req = HttpRequest.new("https://example.com/api");
req->method("GET");
req->header("Accept", "application/json");
req->timeout_ms(5000);
HttpResponse *res = http_send(req);
defer { http_response_free(res); }
defer { http_request_free(req); }
if (res->is_ok()) { /* res->body / res->body_len */ }
```

备注：若不透明布局是硬要求，把 `HttpRequest`/`HttpResponse` 改成 `pub struct X;` 前向声明，删除 `impl` 方法，
全部改 `pub` 自由函数。`h7` 证明前向声明可进生成头；但方法不能只留原型，且消费方 `delete` 只能 free 结构体。

## 4. 探针索引

统一命令：`cd /tmp/cthttp-probe/<dir> && /Users/samxx/.local/bin/ct build <unit>.ct -o <out> --outdir .ctbuild`；
`h5`/`h5u` 另加 `--cflags "-fsanitize=address -g" --ldflags "-fsanitize=address"`。`ct check` 只在 A 的
`p_prim.ct` 上用于确认 impl 诊断，其余以 `build` 为准。

| 文件 | 用途 |
| --- | --- |
| `p_prim.ct` / `p_trait_ptr.ct` | `impl char *` 与 `impl Trait for char *` 的失败文案 |
| `p_lit.ct` / `p_chain_static.ct` / `p_chain2.ct` / `p_chain3.ct` | 字面量接收者、链式接收者的失败文案 |
| `p_recv.ct` / `h4/main_dot.ct` | `.` 与 `->` 的差异（警告 + 运行值） |
| `h1/api.h.ct` / `h1/main_ok.ct` / `h1/main_closure.ct` | typedef/字段导出、命名回调可用、闭包失败 |
| `h2/d.h.ct` / `h2/f.h.ct` | `.h.ct` 内 defer / errdefer 禁止 |
| `h3/codes.h.ct` / `h6/errs.h.ct` | `pub int` / `pub enum` 错误码导出 |
| `m1` / `m3` / `m4` / `p_drop_same.ct` | 跨单元 delete 不执行 Drop 体；定义单元内 free 包装可执行 |
| `h5` / `h5u` | 自动释放后再 delete/使用：ASAN double-free / use-after-free |
| `h7/op.h.ct` | 不透明 struct 前向声明 |
| `h8/api3.h.ct` | 返回 self 的指针方法（可赋值、不可链） |
| `h9/main_pair.ct` | env 首位函数指针 + 闭包可用 |
| `p_atomic2.ct` / `p_atomic3.ct` | `atomic int` 与 `ct_atomic_*` |
