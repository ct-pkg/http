# 05 错误面与谓词真值表

- applies-to: 0.1.0

三种结果分开，判定顺序固定：

<!-- snippet: docs/snippets/errors.ct -->
```text
/*
 * docs/snippets/errors.ct - three outcomes, fixed decision order.
 */
import http::http;

#include <stdio.h>

int main(void)
{
    CResponse *res = "https://api.example.com/users"->post()->send();

    if (res->is_cancelled()) {
        printf("cancelled\n");            /* error()==42, phase 8 */
    } else if (res->error() != 0) {
        printf("transport %d: %s\n",      /* positive = raw CURLcode */
               res->error(), res->error_msg());
    } else if (res->is_ok()) {
        printf("%d %s\n", res->status(), res->body());
    } else {
        printf("http %d %s\n",            /* error==0, status is 4xx/5xx */
               res->status(), res->body());
    }

    res->free();
    return 0;
}
```

- **本地拒绝**是负数码：`-1001` ARG、`-1002` HEADER、`-1003` BODY、`-1004` SCHEME、
  `-1005` TIMEOUT、`-1006` NO_HANDLE、`-1007` MODE、`-1008` BODY_LIMIT、`-1009` REDIRECT。
- **传输错误**是正的 `CURLcode` 原值，未知码也原样保留（例如注入 `9999` 读到 `9999`），
  不归零、不改写。阶段只影响 `error_msg()` 文案：`resolve` / `connect` / `timeout` /
  `tls` / `write` / `cancelled` / `redirect` / `transfer`。
- **HTTP 4xx/5xx 不是传输错误**：`error()==0`，`is_ok()==0`，靠 `status()` 区分。

谓词真值表（实测于 `p5-predicates`）：

| 状态 | status | error | is_ok | is_cancelled |
| --- | --- | --- | --- | --- |
| 成功 | 200 | 0 | 1 | 0 |
| HTTP 错误 | 404 | 0 | 0 | 0 |
| 传输错误 | 0 | 7 | 0 | 0 |
| 取消 | 200 | 42 | 0 | 1 |

`is_ok()` 的判定里包含 `!cancelled`，所以两个谓词不可能同时为真；这不是调用方约定，
是构造保证。取消行由 `p8-cancel` 实测（慢响应回调内 `cancel()`）；`p5-predicates`
另有一条同一 `status/error` 下直接置位的行（`200 0 0 1`），单独证明 `is_ok` 的取消项会翻转。
