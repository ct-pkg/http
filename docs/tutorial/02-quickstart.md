# 02 起手链与变量落点

- applies-to: 0.2.0

最小可运行示例（门禁编译并运行，输出 `200 {"users":3}`）：

<!-- snippet: docs/snippets/quickstart.ct -->
```text
/*
 * docs/snippets/quickstart.ct - the first example, and a gate-run one.
 * It talks to a loopback server linked from test/fixtures so the output is
 * frozen: 200 {"users":3}
 */
import http::http;

#include <stdio.h>
#include "p4-server.h"

int main(void)
{
    if (p4_server_start() != 0) return 1;
    char *url = p4_server_url("/200");

    CRequest *req = url->post();
    req->header("Accept", "application/json");
    req->timeout(3000);

    CResponse *res = req->send();
    printf("%d %s\n", res->status(), res->body());

    res->free();
    req->free();
    p4_server_stop();
    return 0;
}
```

真实使用把 `p4_server_url("/200")` 换成自己的 URL 即可。变量落点形式与链式等价：

<!-- snippet: docs/snippets/variable-landing.ct -->
```text
/*
 * docs/snippets/variable-landing.ct - the chain and the variable landing form
 * are equivalent. Compile-only: the host is a documentation placeholder.
 */
import http::http;

int main(void)
{
    CRequest *req = "https://api.example.com/users"->post();
    req->header("Accept", "application/json");
    req->timeout(3000);
    CResponse *res = req->send();
    int status = res->status();
    (void)status;
    res->free();
    req->free();
    return 0;
}
```

## 三条硬约定

1. `import http::http;` 必写；只 import 类型/常量单元不会带来方法。
2. 指针宿主用 `->`，不要写 `.status()`。
3. `.free()` 各调一次；`send()` 不释放请求。
