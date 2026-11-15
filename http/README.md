# http

源码单元。包根即包含根：`ct build -I .`（或消费端 `ct install` 打印的 `-I vendor/http`）
能解析 `import http::http;`。

| 单元 | 内容 |
| --- | --- |
| `http.m.ct` | 公开映射单元：`pub type CRequest/CResponse`、三个静态 trait、三段 `map trait … for type …` |
| `http.shim.h` | C 边界：`struct HttpRequest` / `HttpResponse`、本地错误码、`HTTP_MAX_REDIRECTS` |
| `http.shim.c` | libcurl 调用集中在此：easy handle、回调、逐跳跟随、错误归一 |
| `Types.h.ct` | 独立 base 的常量单元，供「只 import 类型单元」负例使用 |

- 布局：2026-11-15 起用包根 `http/`（原 `src/http/` 已迁走），理由见
  [`docs/design.md`](../docs/design.md) D-1 与 `issues/HTTP-2.md`。
- 只有 `http.shim.c` 允许出现 `curl_*`；CT 单元只调 `http_*`。
- License: MIT（见仓根 `LICENSE`）。
