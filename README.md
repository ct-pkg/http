# http

`libcurl` easy handle 的 HTTP 客户端包。对外是链式接口：字符串起手，请求对象承载配置，
响应对象只读。

- 版本：0.1.0（phase 6：三种超时区分与慢流保护）
- 许可：MIT（`.ct` / shim / 测试）+ curl 原许可（链接的系统 libcurl）
- 平台：macOS 与 Linux 的系统 libcurl
- 规划：[0.1.0 roadmap](docs/planning/ct-http-0.1.0-roadmap.md)、
  [feature 划分](docs/planning/ct-http-0.1.0-features.md)
- 当前 phase 卡：[PH-6](docs/phases/PH-6/card.md)
- 设计稿：[docs/design.md](docs/design.md)

> **`send()` 真的发请求**：phase 4 起 shim 调用 `curl_easy_perform`，状态码与 body
> 来自网络；连不上或超时返回 `status == 0` 的响应对象，释放路径不变。

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

    printf("%d\n", status);   /* 200 from the server */
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
| `idle_timeout(ms)` | 数据空档上限（首字节前也算空档）；负值本地拒绝 | 不设（`0`） |
| `body(data, len)` | 上传体；按 `len` 深拷贝，先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | 无 |
| `user_agent(ua)` | User-Agent | `ct-http/0.1.0` |
| `insecure(on)` | `1` 时同时关 `VERIFYPEER` 与 `VERIFYHOST` | `0`（校验开） |
| `follow_redirects(on)` | 是否跟随重定向 | `0`（不跟随） |

- 只接受 `http://` 与 `https://`（大小写不敏感）；其余 scheme 在工厂返回时就带本地
  负数码，easy handle 另设 `CURLOPT_PROTOCOLS_STR=http,https` 兜底。
- 本地校验码是 `http.shim.h` 的宏（`HTTP_LOCAL_ERR_*`，-1001…-1006），**是公开错误面**
  的一部分：0 表示无错，正数是 `CURLcode` 原值，负数是本地拒绝。S-05 已裁定不引入
  `HTTP_ERR_*` 枚举——错误集还会增长，枚举会让每次扩展都变成对所有全写 `switch`
  的源码破坏。
- `phase 4` 起 `send()` 真的调用 `curl_easy_perform`；本地校验失败的请求不会离开进程。

## 响应面（phase 4）

响应对象只读，释放仍是显式的：

| 方法 | 作用 |
| --- | --- |
| `status()` | HTTP 状态码；传输失败时为 `0` |
| `body()` | 响应体首地址；NUL 结尾，但长度只认 `body_len()` |
| `body_len()` | 响应体字节数，内嵌 NUL 不截断 |
| `total_bytes()` | write 回调累计字节，与 `body_len()` 同源 |
| `error()` | 0 = 无错；正数 = `CURLcode` 原值；负数 = 本地拒绝 |
| `error_msg()` | `"<阶段>: <主机> (code N)"`，无错时为空串 |
| `error_detail()` | `"code=N stage=S host=H"`，同一输入可复现 |
| `is_ok()` | 2xx 且 `error()==0` 且未取消 |
| `is_cancelled()` | 取消位；phase 5 没有取消路径，恒为 `0` |
| `take_body()` | 取走 body：响应侧置空、`body_len()` 归零，调用方负责 `free` 一次 |
| `free()` | 释放响应；`take_body()` 之后不再持有 body |

- **`take_body` 是所有权转移，不是借用**：取走后原响应的 `free()` 不会重复释放
  （ASAN 正例 `p4-take-body`，负例 `p4-take-body-bad` 证明断言不是空转）。
- **超时边界**：libcurl 只给一个 `CURLE_OPERATION_TIMEDOUT`(28)；连接 / 总时长 /
  空闲的区分由 shim 自己记录，见下方「超时（phase 6）」。
- **失败码**：`http_shim_last_curl_code()` 只是测试钩子；调用方读 `error()`。
- **响应头读取不在 phase 4**：属待裁定项（S-01），即使实现成本只有约 1 个任务
  （header 回调 + 存储 + 查询）也未加入。

## 错误面（phase 5）

三种结果分开，判定顺序固定：

```text
import http::http;
#include <stdio.h>

int main(void)
{
    CResponse *res = "https://api.example.com/users"->post()->send();

    if (res->is_cancelled()) {
        printf("cancelled\n");            /* phase 8 才会有这条路径 */
    } else if (res->error() != 0) {
        printf("transport %d: %s\n",      /* 正数 = CURLcode */
               res->error(), res->error_msg());
    } else if (res->is_ok()) {
        printf("%d %s\n", res->status(), res->body());
    } else {
        printf("http %d %s\n",            /* error==0，status 是 4xx/5xx */
               res->status(), res->body());
    }

    res->free();
    return 0;
}
```

- **本地拒绝**是负数码：`-1001` ARG、`-1002` HEADER、`-1003` BODY、`-1004` SCHEME、
  `-1005` TIMEOUT、`-1006` NO_HANDLE。`NO_HANDLE` 表示没有 easy handle（创建失败或
  send 时缺失），以前会静默成 `status 0`，现在会报出来。
- **传输错误**是正的 `CURLcode` 原值，未知码也原样保留（例如注入 `9999` 读到 `9999`），
  不归零、不改写。阶段只影响 `error_msg()` 文案：`resolve` / `connect` / `timeout` /
  `tls` / `write` / `cancelled` / `transfer`。
- **HTTP 4xx/5xx 不是传输错误**：`error()==0`，`is_ok()==0`，靠 `status()` 区分。
- **谓词真值表**（实测于 `p5-predicates`）：

  | 状态 | status | error | is_ok | is_cancelled |
  | --- | --- | --- | --- | --- |
  | 成功 | 200 | 0 | 1 | 0 |
  | HTTP 错误 | 404 | 0 | 0 | 0 |
  | 传输错误 | 0 | 7 | 0 | 0 |
  | 取消 | 200 | 0 | 0 | 1 |

  `is_ok()` 的判定里包含 `!cancelled`，所以两个谓词不可能同时为真；这不是调用方约定，
  是构造保证。取消后 `send()` 的返回值要等 phase 8 的 `cancel()` 落地才能实测；本 phase
  只能证明同一份 `status/error` 下置位取消会让 `is_ok()` 翻转。

## 超时（phase 6）

`timeout_kind()` 是纯查询，不额外分配；成功路径恒为 `0`：

```text
import http::http;
#include <stdio.h>

int main(void)
{
    CRequest *req = "https://api.example.com/users"->post();
    req->timeout(3000);          /* 总时长 */
    req->connect_timeout(1000);  /* 连接 */
    req->idle_timeout(1500);     /* 数据空档 */

    CResponse *res = req->send();

    if (res->error() == 28) {                 /* CURLE_OPERATION_TIMEDOUT */
        switch (res->timeout_kind()) {
        case 1:  printf("connect timeout\n"); break;
        case 2:  printf("total timeout\n");   break;
        case 3:  printf("idle timeout\n");    break;
        default: printf("timeout, kind unknown\n");
        }
    }

    res->free();
    req->free();
    return 0;
}
```

| `timeout_kind()` | 含义 | 判定依据 |
| --- | --- | --- |
| `0` | 非超时（成功或别的错误） | 恒为 0；不依赖有没有超时配置 |
| `1` | 连接超时 | `transport==28` 且 `CURLINFO_CONNECT_TIME_T==0`，并且配置了 `connect_timeout` |
| `2` | 总时长超时 | `transport==28` 且不是连接超时（含首字节前就耗尽总时长） |
| `3` | 空闲超时 | 进度回调发现空档超过 `idle_timeout`，中止后把 `42` 归一成 `28` |

- **判定时点**：全部在 `curl_easy_perform` 返回之后一次算清，不在传输中途猜。
  连接超时用 `CURLINFO_CONNECT_TIME_T`（微秒）作为「握手没完成」的硬证据。
- **精度**：连接 / 总时长是 libcurl 自己的计时，亚毫秒级；空闲是进度回调粒度，
  实测约 **1 秒**一次，因此 `idle_timeout(500)` 实际在 ~1.0–1.1 秒中止，
  误差上限约为一个回调周期。
- **可能误判**：`timeout_kind==2` 是「28 且非连接超时」的兜底，它不区分「首字节前
  耗尽总时长」和「传了一半耗尽总时长」——两者都是总时长，对外语义一致；对
  `CURLINFO_CONNECT_TIME_T==0` 但实际已连上的极端情况（libcurl 未填该信息）会落到
  2，不会错报成 1。
- **合法慢流不会被误杀**：没有使用 `LOW_SPEED_LIMIT` / `LOW_SPEED_TIME`（那对会以
  平均速率杀「每 59 秒 1 字节」的合法流）。空档判定只看「距离上一字节多久」，
  只要数据持续到达、间隔小于 `idle_timeout` 就永不中止。夹具 `p6-slow`
  每 200 ms 一字节、`idle_timeout(500)`，结果 `status 200 body_len 5 kind 0 ok 1`。
  200 ms 节奏是「1 字节 / 59 秒」的等比压缩，断言的性质相同，但整套用例不必真等。
- **负值**：`idle_timeout(ms<0)` 与 `timeout(ms<0)` 一样是本地 `-1005`，请求不离开
  进程（`p6-negative` 的服务端计数为 0）。
- **全部真实网络**：本 phase 的五个夹具都走本地回环或 TEST-NET-1，没有使用
  `http_shim_inject_curl_code`；连接超时用不可路由的 `192.0.2.1:81` 真实触发。

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
bash tools/check.sh g3     # 夹具构建 + 运行 + 负例 + 生命周期/释放 + 配置面 + 真执行响应面 + 超时区分
bash test/gates/g1_negative.sh   # 证明 g1 会拒绝篡改卡
```

未实现的子命令以退出码 2 明确失败，不伪装通过。
