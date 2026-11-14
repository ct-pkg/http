# http

`libcurl` easy handle 的 HTTP 客户端包。对外是链式接口：字符串起手，请求对象承载配置，
响应对象只读。

- 版本：0.1.0（phase 3：请求配置与本地校验；`send()` 仍是固定值 stub）
- 许可：MIT（`.ct` / shim / 测试）+ curl 原许可（链接的系统 libcurl）
- 平台：macOS 与 Linux 的系统 libcurl
- 规划：[0.1.0 roadmap](docs/planning/ct-http-0.1.0-roadmap.md)、
  [feature 划分](docs/planning/ct-http-0.1.0-features.md)
- 当前 phase 卡：[PH-3](docs/phases/PH-3/card.md)
- 设计稿：[docs/design.md](docs/design.md)

> **`send()` 还不发网络**：shim 返回固定的 `200` 与一段 JSON。
> 真实请求在 phase 4 接入；phase 2 已让 `curl_easy_init` / `curl_easy_cleanup`
> 真实发生，phase 3 已让配置写进 easy handle 并做本地校验。

## 用法

```text
import http::http;

#include <stdio.h>

int main(void)
{
    int status = "https://api.example.com/users"
                     ->post()
                     ->header("Accept", "application/json")
                     ->timeout(3000)
                     ->send()
                     ->status();

    printf("%d\n", status);   /* phase 1 stub: 200 */
    return 0;
}
```

变量落点同样成立：

```text
import http::http;

CRequest *req = "https://api.example.com/users"->post();
req->header("Accept", "application/json");
req->timeout(3000);
CResponse *res = req->send();
int status = res->status();
res->free();
req->free();
```

## 配置面（phase 3）

所有 setter 返回同一个 `CRequest *`，链式与变量落点两种写法等价：

| 方法 | 作用 | 默认值 |
| --- | --- | --- |
| `header(name, value)` | 请求头；名或值含 CR/LF 时本地拒绝，不进 slist | 无 |
| `timeout(ms)` | 总超时；负值本地拒绝 | 不设（`0`） |
| `connect_timeout(ms)` | 连接超时；负值本地拒绝 | 不设（`0`） |
| `body(data, len)` | 上传体；按 `len` 深拷贝，先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | 无 |
| `user_agent(ua)` | User-Agent | `ct-http/0.1.0` |
| `insecure(on)` | `1` 时同时关 `VERIFYPEER` 与 `VERIFYHOST` | `0`（校验开） |
| `follow_redirects(on)` | 是否跟随重定向 | `0`（不跟随） |

- 只接受 `http://` 与 `https://`（大小写不敏感）；其余 scheme 在工厂返回时就带本地
  负数码，easy handle 另设 `CURLOPT_PROTOCOLS_STR=http,https` 兜底。
- 本地校验码目前是 shim 内部字段（`HTTP_LOCAL_ERR_*`，-1001…-1005），
  **不是公开错误面**；公开错误码在 phase 5 裁定。
- `phase 3` 不调用 `curl_easy_perform`：`send()` 仍返回固定值。

## 构建

```sh
ct build main.ct -I src -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
```

`http.shim.c` 由 `ct build` 自动列入 cc 行；`ct build` 不读取 `ct.pkg` 的
`ldflags`，所以链接参数必须显式传 `--ldflags "-lcurl"`。

释放示例（每个对象只调一次；`.free()` 调用后不再使用该变量）：

```text
import http::http;

CRequest *r = "https://api.example.com/users"->post();
CResponse *res = r->send();
res->free();
r->free();
```

## 接口约定（0.1.0 冻结）

1. **必须 `import http::http;`**。公开面是映射单元 `src/http/http.m.ct`；方法只随它出现。
2. **只 import 类型/常量单元不会带来方法**。只写 `import http::Types;` 时，
   `"url"->post()` 报 `no method 'post' for type 'char *'`。这条是门禁 E-P1-4 的断言。
3. **常量单元换 base**。`Types.h.ct` 与 `http.m.ct` 同目录不同 base；同 base 的
   `.h.ct` + `.m.ct` 会造成 import 歧义。
4. **指针宿主用 `->`**。`post()` 返回 `CRequest *`，`send()` 返回 `CResponse *`；
   调用点一律 `->`，不要写成 `req.header(...)`。
5. **链每跳展开成嵌套裸 C 调用**。没有 vtable、没有装箱、没有临时对象；生成 C 可直接核对。
6. **libcurl 只在 shim**。`.m.ct` 只调 `http_*`；`curl_*` 不进入 CT 单元（phase 2 起由
   `http.shim.c` 承担）。
7. **释放是显式的**。`send()` 不释放请求，也不会自动调用 `curl_easy_cleanup`：
   - CT 侧用 `req->free();` / `res->free();`，各调一次；`.free()` 是终态，调用后
     原变量不再使用。
   - C 侧用槽位形式 `http_request_free(&req);` / `http_response_free(&res);`，
     置空后二次调用是 no-op。
   - `delete` 不作为公开释放方式。
8. **回调只收命名函数**。`HttpChunkCb` 带 `void *ud`，闭包不能填该签名（phase 7 落地）。

## 定位与边界

只做“发一个请求并读回结果”这条最短路径：

- 进：URL、请求头、body、超时、connect 超时、User-Agent、重定向开关、TLS 校验开关；
  `post()`；`post_stream(cb, ud)`；`cancel()`；状态码、body、错误码、错误文案、总字节数。
- 不进：连接池、重试、代理、白名单；`multi`（异步状态机）、`share`、`mime`、`form`、
  WebSocket、`urlapi`；JSON 编解码（用 `json` 包）。
- 安全默认：TLS 校验开、重定向关。

以下五项仍在范围裁定中，未裁定前不进入实现：响应头读取、`method()`、默认
`max_redirects(10)`、跨线程取消、`HTTP_ERR_*` 枚举。影响面见
[feature 划分 §3](docs/planning/ct-http-0.1.0-features.md#3-范围待裁定项-s-01s-05)。

## 安装（phase 10 之后才可用）

```text
ct get http@^0.1.0
ct install
```

## 门禁

```sh
bash tools/check.sh g0     # 规约纳管：spec 块 + 实例化表 + session-map + 版本落点
bash tools/check.sh g1     # 卡片十项 + 任务七列 + 判据/任务摘要冻结
bash tools/check.sh g3     # 夹具构建 + 运行 + 负例 + shim 生命周期/释放 + 配置面
bash test/gates/g1_negative.sh   # 证明 g1 会拒绝篡改卡
```

未实现的子命令以退出码 2 明确失败，不伪装通过。
