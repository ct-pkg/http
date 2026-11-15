# 01 概念模型

- applies-to: 0.1.0

`http` 不是「对象 + 虚函数」的库，而是三段静态宿主加一条裸 C 调用链。理解这四点，
后面每个主题都只是细节。

## 三段宿主

| 宿主 | 从哪来 | 能做什么 |
| --- | --- | --- |
| `char *` | 调用方的 URL | 只做工厂：`url->post()` |
| `CRequest *` | `post()` | 配置（setter 返回同一指针）+ 执行 + `free()` |
| `CResponse *` | `send()` / `send_stream()` | 只读视图 + `take_body()` + `free()` |

映射单元是 `src/http/http.m.ct`，所以使用方必须 `import http::http;`。
只 import `http::Types` 时方法不存在，构建会报 `no method 'post' for type 'char *'`。

## 指针宿主用 `->`

`post()` 返回 `CRequest *`，`send()` 返回 `CResponse *`，链上每一跳都是嵌套裸 C 调用。
调用点一律写 `req->header(...)`；写成 `req.header(...)` 在 CT 里可能编译通过但传错地址，
所以门禁用 `-Werror=incompatible-pointer-types` 拦。

## 所有权

- `send()` 从不释放请求，也不调用 `curl_easy_cleanup`；返回后请求字段仍可读。
- `http_request_free()` / `http_response_free()` 是唯一释放入口，第二次调用是 no-op。
- `take_body()` 是所有权转移：取走后原响应不再持有 body，调用方负责一次 `free`。
- CT 侧 `.free()` 是终态，调用后不再使用原变量。

## 错误模型

三种结果分开，判定顺序固定：

| 结果 | `error()` | `status()` | `is_ok()` | `is_cancelled()` |
| --- | --- | --- | --- | --- |
| 成功 | 0 | 2xx | 1 | 0 |
| HTTP 4xx/5xx | 0 | 4xx/5xx | 0 | 0 |
| 传输错误 | 正数（`CURLcode` 原值） | 0 | 0 | 0 |
| 本地拒绝 | 负数（`HTTP_LOCAL_ERR_*`） | 0 | 0 | 0 |
| 取消 | 42 | 原响应码或 0 | 0 | 1 |

`is_ok()` 的构造里包含 `!cancelled`，所以 `is_ok()` 与 `is_cancelled()` 不可能同时为真。
