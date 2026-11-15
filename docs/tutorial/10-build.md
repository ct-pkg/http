# 10 构建、接口约定与释放

- applies-to: 0.1.0

<!-- snippet: docs/snippets/build.sh -->
```text
ct build main.ct -I . -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
```

`http.shim.c` 由 `ct build` 自动列入 cc 行；`ct build` 不读取 `ct.pkg` 的
`ldflags`，所以链接参数必须显式传 `--ldflags "-lcurl"`。

释放示例（每个对象只调一次；`.free()` 调用后不再使用该变量）：

<!-- snippet: docs/snippets/free.ct -->
```text
/*
 * docs/snippets/free.ct - one explicit free per object; .free() is terminal.
 */
import http::http;

int main(void)
{
    CRequest *r = "https://api.example.com/users"->post();
    CResponse *res = r->send();
    res->free();
    r->free();
    return 0;
}
```

## 接口约定（0.1.0 冻结）

1. **必须 `import http::http;`**。公开面是映射单元 `http/http.m.ct`；方法只随它出现。
2. **只 import 类型/常量单元不会带来方法**。只写 `import http::Types;` 时，
   `"url"->post()` 报 `no method 'post' for type 'char *'`。这条是门禁 E-P1-4 的断言。
3. **常量单元换 base**。`Types.h.ct` 与 `http.m.ct` 同目录不同 base；同 base 的
   `.h.ct` + `.m.ct` 会造成 import 歧义。
4. **指针宿主用 `->`**。`post()` 返回 `CRequest *`，`send()` 返回 `CResponse *`；
   调用点一律 `->`，不要写成 `req.header(...)`。
5. **链每跳展开成嵌套裸 C 调用**。没有 vtable、没有装箱、没有临时对象。
6. **libcurl 只在 shim**。`.m.ct` 只调 `http_*`；`curl_*` 不进入 CT 单元。
7. **释放是显式的**。CT 侧 `req->free();` / `res->free();`；C 侧
   `http_request_free(&req);` / `http_response_free(&res);` 置空后二次调用是 no-op。
   `delete` 不作为公开释放方式。
8. **回调只收命名函数**。`HttpChunkCb` 带 `void *ud`，闭包不能填该签名。
