# src

源码单元，一单元一主类型（`docs/package-guide.md` §1）：

| 单元 | 内容 | phase |
| --- | --- | --- |
| `Request.h.ct` | `struct HttpRequest` + `HttpResponse` 声明、`static HttpRequest *new()`、配置方法与 `Drop` | H1 |
| `Request.ct` | 配置方法的实现（H1 只有存值，不碰 libcurl） | H1 |
| `Curl.m.ct` | libcurl 的 `map` 映射（CT 侧唯一的 libcurl 入口） | H2 / H6 |
| `Curl.shim.h` / `Curl.shim.c` | 手写 C shim：`curl_easy_*`、`curl_slist_*` 调用集中在这里 | H2 / H6 |

当前目录为空：H1-T1 落第一个单元。`namespace http;` 尚未引入；若引入，按编译器的
D13 目录规则，单元必须迁到 `http/`（见 `docs/design.md` D-1）。

License: MIT（见仓根 `LICENSE`）。
