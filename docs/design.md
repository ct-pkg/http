# http 模块设计（libcurl easy handle，OO）

- applies-to: 0.1.0
- 日期：2026-11-13
- 状态：骨架期设计。**本仓尚未实现任何 libcurl 调用**；H1 只做骨架、配置与 Drop。
- 目标包：`http`（仓库 `ct-pkg/http`，本地检出 `/Users/samxx/trae-ws/ct-http`）
- 规范：`AGENTS.md`（主规范 1.9.0 生成块）+ `docs/package-guide.md`（ct-lang 仓）

---

## 0. 结论摘要

1. **对外两个对象**：`HttpRequest`（构造 / 配置 / 执行 / `Drop` 清理）与
   `HttpResponse`（只读结果）。不做连接池、重试、代理、白名单。
2. **安全默认**：TLS 校验开、重定向关；`insecure(1)` / `follow_redirects(1)` 必须显式打开。
3. **构造用静态固有方法**：`HttpRequest.new()`。注意是 `.`，**不是** `HttpRequest::new()`
   （后者是命名空间限定调用，实测编译失败，见 D-4）。
4. **配置方法返回接收者 `HttpRequest *`，但单表达式链不成立**：当前编译器只允许
   “对临时值的方法调用出现在语句或赋值开头”，`req.url(...).header(...)` 会报错。
   因此配置写成一条语句一个方法（D-2）。
5. **清理走 `Drop`**：`struct HttpRequest { … } use Drop;` + `impl Drop for HttpRequest`，
   `delete req;` 触发（D-3）。
6. **libcurl 只出现在 C shim 里**：CT 侧不 `#include <curl/curl.h>`；H6 把调用集中到
   `src/*.shim.c` + `.m.ct` 映射，`ct.pkg` 带 `ldflags = -lcurl`。

---

## 1. 语法假设核验（2026-11-13，只读探针）

探针全部在 `/tmp/http-probes`，编译器为当次 HEAD 自举的 `ct-ast`
（`ct-lang` 的 L0→L1→L2→ct-ast 链，无仓库改动）。

| # | 假设 | 结论 | 证据 |
| --- | --- | --- | --- |
| 1 | 静态方法 `static HttpRequest *new()` 与 `Type.new()` 调用 | **成立**（写法是 `Type.new()`，不是 `Type::new()`） | `ct-lang/test/ast/229_inherent_impl/prog.ct:14`（`static Counter make(int v)`）、`:20`（`Counter.make(5)`）；探针 `p1.ct` 编过、运行返回 0。`p1b.ct`（`Req::new()`）报 `error: no function of that name in that namespace` |
| 2 | `impl Drop for T` 的写法 | **成立**，且必须配 `use Drop` | `ct-lang/test/ast/63_own_return_field/prog.ct:9-15`（`} use Drop;` + `impl Drop for Box { void drop(void) }`）；`ct-lang/test/archive/cases/22_drop.ct:5-17`；调用点是 `delete p;`（63:38、22:38）；探针 `p2.ct` 编过（唯一输出是 `potential leak` 警告，因为探针用 `free` 而非 `delete`） |
| 3 | 方法返回 `self` 做链式配置 | **返回 `self` 成立；单表达式链不成立** | `ct-lang/lib/std/Str.h.ct:131-136`（`Str retain()` + `self->…`）；探针 `p3a.ct`（`p = r.url("x")`）编过、`p->n == 1`。`p3i.ct`（`p = mk().url("x").header(…)`）报 `error: a method call on a temporary is only allowed at the start of a statement or of an assignment`；规则源码在 `ct-lang/selfhost/ct-ast/backend/emit/Emitter.h.ct:2174`，反例进真门禁：`ct-lang/selfhost/check-ast.sh:4321` |
| 4 | `typedef int (*HttpCB)(char*, int, void*)` + 结构体字段 + `->field` 调用 | **成立** | `ct-lang/test/ast/27_fnptr_forms/prog.ct:9-22`（`typedef int (*BinOp)(int,int);`、字段 `BinOp add;`、`c.add(2,3)` / `p->add(2,3)`）；探针 `p4.ct`（`HttpCB` + `void *ud`，`r->cb(p,n,r->ud)`）编过并返回 0 |

**编译器前置条件**：第 1 条（静态固有方法）来自 Phase 34；发布版低于该 phase 的 `ct`
编不过本包，所以 `ct.pkg` 写 `requires = ct >= 0.34.0`。

---

## 2. 决策

### D-1 目录：`src/` + `test/`（与 ct-pkg 既有惯例不同，待裁定）

- 本仓按用户指定清单建：`src/`（源码单元）、`test/`（测试）、`docs/`、`tools/`。
- 既有惯例（`docs/package-guide.md` §1、`ct-lang/docs/history/design/phase18.md` 18.1、
  `ct-pkg/curl`）是“包根即包根、单元与 `ct.pkg` 并列”，单元带 `namespace` 时必须坐在
  同名目录（D13 方案 A）。curl 实际用 `curl/` + `tests/`，没有 `src/`。
- 风险：若 `src/` 里的单元声明 `namespace http;`，`ct build --ns-root` 会报目录不匹配。
  当前决定：**H1 的单元不声明 namespace**；若之后要 namespace，按 D13 迁到 `http/` 或
  放弃 `--ns-root` 校验。发布前必须收口这一条。

### D-2 配置不用单表达式链

链式语法在 `map` 宿主方法上可用（`ct-pkg/curl` 的 `chain.ct` 就是这种形态），但本包用
真实结构体 + `Drop` 管理 easy handle，不适合 `map` 展开。因此配置写成：

```text
HttpRequest *req = HttpRequest.new();
req.url("https://example.com");
req.header("Accept", "application/json");
req.timeout(15);
HttpResponse *res = req.post();
```

代价：不能写成一行“流式”风格；收益：状态与清理都归 `Drop`，没有中间临时值。

### D-3 清理归 `Drop`

`delete req;` 触发 `drop()`；H6 在 `drop()` 里执行 `curl_slist_free_all` +
`curl_easy_cleanup`，H1 先用 `free` 占位。禁止调用方手动释放内部字段。

### D-4 构造语法

`HttpRequest.new()`（静态固有方法）。`HttpRequest::new()` 是命名空间限定调用，编译失败；
不要照 C++ 习惯写。

### D-5 安全默认

`verify_ssl = 1`、`follow_redirects = 0`。`insecure(1)` 与 `follow_redirects(1)` 必须显式调用；
README 与 phase 卡都把这写成验收项（负例：默认值必须能被测试断言）。

### D-6 不做

连接池、重试、代理、域名白名单、`multi` / `share` / `mime` / `form` / WebSocket / `urlapi`。
理由：这些要么需要第二个对象或异步状态机（与同步 `post()` 语义冲突），要么是产品策略
（重试/白名单）不该由传输包决定。

---

## 3. 阶段划分

| phase | 内容 | 出口 |
| --- | --- | --- |
| H1 | 骨架；构造与 `Drop`；七个配置方法；`HttpResponse` 读取面 | 无需 libcurl 即可编译运行的配置对象 |
| H2 | `post()`：shim 绑定、响应缓冲、header slist、`perform` + `getinfo` | 本地 fixture server 上 200/404/500 端到端 |
| H3 | `post_stream(cb, ud)`：分片回调、提前中止、与 `send()` / `send_stream()` 响应模式互斥（请求体可共存） | 回调契约测试 + 分片顺序断言 |
| H4 | `cancel()` / `is_cancelled()`：进度回调置位 → `CURLE_ABORTED_BY_CALLBACK` | 取消路径可复现，且不泄漏 |
| H5 | 错误映射：`CURLcode` → `error_code` / `error_msg` / `error_detail()` | 表驱动负例全覆盖 |
| H6 | 全部 libcurl 调用收进 C shim；跨平台与 sanitizer；收口 | macOS/Linux 绿，ASAN/LSAN 干净 |

任务级清单见 `docs/phases/H1.md` 与 `docs/phases/H2-H6.md`。
