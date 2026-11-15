# http

`libcurl` easy handle 的 HTTP 客户端包。对外是链式接口：字符串起手，请求对象承载配置，
响应对象只读。

- 版本：0.1.0（phase 9：重定向与安全策略）
- 许可：MIT（`.ct` / shim / 测试）+ curl 原许可（链接的系统 libcurl）
- 平台：macOS 与 Linux 的系统 libcurl
- 规划：[0.1.0 roadmap](docs/planning/ct-http-0.1.0-roadmap.md)、
  [feature 划分](docs/planning/ct-http-0.1.0-features.md)
- 当前 phase 卡：[PH-9](docs/phases/PH-9/card.md)
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
| `max_body_bytes(n)` | 响应体上限；累计到 `n` 立即中止并报 `-1008` | 不限（`0`） |
| `body(data, len)` | 上传体；按 `len` 深拷贝，先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | 无 |
| `user_agent(ua)` | User-Agent | `ct-http/0.1.0` |
| `insecure(on)` | `1` 时同时关 `VERIFYPEER` 与 `VERIFYHOST` | `0`（校验开） |
| `follow_redirects(on)` | 是否跟随重定向；`1` 时最多跟随 10 次重定向（固定，不可配） | `0`（不跟随） |
| `cancel()` | 置粘性取消标志；只允许同线程调用（见「取消与生命周期」） | 未置位 |

- 只接受 `http://` 与 `https://`（大小写不敏感）；其余 scheme 在工厂返回时就带本地
  负数码，easy handle 另设 `CURLOPT_PROTOCOLS_STR=http,https` 兜底。
- 本地校验码是 `http.shim.h` 的宏（`HTTP_LOCAL_ERR_*`，-1001…-1009），**是公开错误面**
  的一部分：0 表示无错，正数是 `CURLcode` 原值，负数是本地拒绝。S-05 已裁定不引入
  `HTTP_ERR_*` 枚举——错误集还会增长，枚举会让每次扩展都变成对所有全写 `switch`
  的源码破坏。
- `phase 4` 起 `send()` 真的调用 `curl_easy_perform`；本地校验失败的请求不会离开进程。

## 响应面（phase 4）

响应对象只读，释放仍是显式的：

| 方法 | 作用 |
| --- | --- |
| `status()` | HTTP 状态码；传输失败时为 `0` |
| `body()` | 响应体首地址；NUL 结尾，长度只认 `body_len()`；流式恒为 `NULL` |
| `body_len()` | 响应体字节数，内嵌 NUL 不截断；流式恒为 `0` |
| `total_bytes()` | 实际交付给缓冲或回调的响应体字节数；上限命中时等于上限 |
| `error()` | 0 = 无错；正数 = `CURLcode` 原值；负数 = 本地拒绝 |
| `error_msg()` | `"<阶段>: <主机> (code N)"`，无错时为空串 |
| `error_detail()` | `"code=N stage=S host=H"`，同一输入可复现；超时多 `phase=`，上限多 `limit=` |
| `is_ok()` | 2xx 且 `error()==0` 且未取消 |
| `is_cancelled()` | 取消位；只有 `cancel()` 真正中断传输才为 `1`，此时 `error()==42` |
| `take_body()` | 取走 body：响应侧置空、`body_len()` 归零，调用方负责 `free` 一次；流式返回 `NULL` |
| `free()` | 释放响应；`take_body()` 之后不再持有 body |

- **`take_body` 是所有权转移，不是借用**：取走后原响应的 `free()` 不会重复释放
  （ASAN 正例 `p4-take-body`，负例 `p4-take-body-bad` 证明断言不是空转）。
- **超时边界**：libcurl 只给一个 `CURLE_OPERATION_TIMEDOUT`(28)；连接 / 总时长 /
  空闲的区分由 shim 自己记录，见下方「超时（phase 6）」。
- **流式边界**：`send_stream` 不填响应体；字节只经回调交付，所有权与上限见
  「流式（phase 7）」。
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
        printf("cancelled\n");            /* error()==42, phase 8 */
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
  | 取消 | 200 | 42 | 0 | 1 |

  `is_ok()` 的判定里包含 `!cancelled`，所以两个谓词不可能同时为真；这不是调用方约定，
  是构造保证。取消行由 `p8-cancel` 实测（慢响应回调内 `cancel()`）；`p5-predicates`
  另有一条同一 `status/error` 下直接置位的行（`200 0 0 1`），单独证明
  `is_ok` 的取消项会翻转。

## 流式（phase 7）

`send_stream(cb, ud)` 按到达顺序交付响应体，不回填 `body()`；通常写作
`req->max_body_bytes(1048576)->send_stream(on_chunk, &sink)`。

- 回调拿到的 `data` 只在本次调用内有效；跨回调保留必须自行复制，库不释放调用方内存。
- 返回非 0 → `error()==23`、`is_cancelled()==0`；无回调读 `-1001`，混用
  `send`/`send_stream` 读 `-1007`。
- `max_body_bytes(n)`：达到 `n` 立即中止并报 `-1008`；`total_bytes()` 等于 `n`，非流式保留
  前 `n` 字节并可 `take_body`，流式已交付前缀归回调。
- 请求体与流式响应可共存；chunked / 无 `Content-Length` 由 libcurl 解码后交付。
- 流式读中 `idle_timeout` 生效：中止读 `28/3`，已交付字节归回调，`take_body()` 为 `NULL`；
  非流式保留已收前缀。
- 回调内可以直接 `req->cancel()` 取消（与返回非 0 不同）：见「取消与生命周期（phase 8）」。
- 完整示例与所有权说明见 [docs/streaming.md](docs/streaming.md)。

## 取消与生命周期（phase 8）

```text
import http::http;
#include <stdio.h>

CRequest *g_req;

int on_chunk(char *data, int len, void *ud)
{
    (void)data; (void)ud;
    if (len > 0) {
        g_req->cancel();     /* 置标志，不中止当前回调 */
    }
    return 0;                /* 返回非 0 才是回调中止（23） */
}

int main(void)
{
    g_req = "https://api.example.com/report"->post();
    g_req->timeout(10000);
    CResponse *res = g_req->send_stream(on_chunk, NULL);

    printf("error=%d cancelled=%d ok=%d\n",
           res->error(), res->is_cancelled(), res->is_ok());

    res->free();
    g_req->free();
    return 0;
}
```

- **判定点三处**：传输开始前、每次写回调交付字节前、每次进度回调（约 1 秒一次）。
  连续有数据的流在下一次写回调就停；停顿流要等下一次进度回调，约 1 秒。
  **DNS 解析阶段不可取消**：解析返回前观察不到标志。
- **对外形态**：取消中止时 `error()==42`（`CURLE_ABORTED_BY_CALLBACK`）、
  `is_cancelled()==1`、`is_ok()==0`、`timeout_kind()==0`。成功路径 `is_cancelled()==0`。
- **优先级**：取消 `42` > 回调中止 `23` > 上限 `-1008` > 空闲 `28/3` > 传输码。
  写回调在调用用户回调之前先查标志，所以同一片「取消 + 返回非 0」按取消记账。
- **粘性标志**：`cancel()` 之后该请求不再发起新传输（连接前就返回 `42`，服务端计数 0，
  `status()` 为 0）。要重试请新建请求对象。
- **同线程限制**：`cancel()` 只在 `send()` 所在线程调用；标志是普通 `int`，不做 atomic。
  跨线程取消不在 0.1.0 范围（S-04 裁决：`docs/reviews/PH-8/S-04-ruling.md`）。
- **部分数据归属**：非流式保留已收前缀，`body_len()==total_bytes()`，`take_body()` 可取；
  流式已交付片归调用方，`body()==NULL`、`body_len()==0`、`take_body()==NULL`。
- **显式释放契约**：`send()` / `send_stream()` 从不释放请求，也不调用
  `curl_easy_cleanup`；`send()` 返回后请求字段仍可读。调用方按
  `res->free(); req->free();` 各释放一次，CT 侧 `.free()` 是终态。
- **夹具**：`p8-cancel`（回调内取消）、`p8-not-cancelled`（两入口未取消）、
  `p8-lifetime`（send 后读请求，ASAN 正例）、`p8-lifetime-bad`（ASAN 负例，
  证明旧设计的 free 后读会被判 `heap-use-after-free`）、`p8-chain`（CT `.cancel()`）。

## 重定向与安全策略（phase 9）

默认不跟随：`follow_redirects(0)` 时 3xx 原样返回（`status()==302`、`error()==0`、
`is_ok()==0`），只发 1 个请求。

`follow_redirects(1)` 后由 shim 自己逐跳跟随（`CURLOPT_FOLLOWLOCATION` 始终为 0），
规则如下：

- **上限固定 10 次重定向，不可配置**（S-03；裁决见 `docs/reviews/PH-9/S-03-ruling.md`）。
  第 11 个 3xx 不再跟随：`error()==47`、`status()` 为最后一个 3xx。
- **计数单位是重定向次数，不是请求数**：10 个 302 的链成功，共 11 个请求；
  11 个 302 时第 11 跳的目标不会被请求，总请求仍是 11。
- **方法改写与 libcurl 一致**：`301`/`302`/`303` 把 POST 改成 GET 并丢弃请求体；
  `307`/`308` 保留方法与请求体。
- **中间 3xx 的 body 丢弃**：只有最终响应进入 `body()`；`send_stream` 不会把中间
  跳转的 body 交给回调。
- **跨源剥离**：scheme / host / port 任一变化即视为跨源，下一跳不发送
  `Authorization`、`Cookie`、`Proxy-Authorization`、`Referer`；一旦跨过源，
  后续跳也不再带回去。同源保留。
- **拒绝降级**：`https → http` 在发出下一跳前本地拒绝，`error()==-1009`
  （`HTTP_LOCAL_ERR_REDIRECT`）、`status()==0`、`error_detail` 带 `stage=redirect`，
  明文服务端收不到请求。`http → https` 允许。
- **循环**不单独检测，由 10 跳上限兜底：A→B→A 在 11 个请求内以 `47` 结束。
- **夹具**：`p9-same-host`、`p9-hops10`、`p9-hops11`、`p9-cross-host`、`p9-loop`、
  `p9-downgrade`（真实 TLS 起点）、`p9-default`。

```text
import http::http;
#include <stdio.h>

int main(void)
{
    CRequest *req = "https://api.example.com/v1/items"->post();
    req->timeout(10000);
    req->follow_redirects(1);
    CResponse *res = req->send();
    if (res->error() == 47) {
        printf("too many redirects (limit 10)\n");
    } else if (res->error() == -1009) {
        printf("refused https -> http downgrade\n");
    }
    res->free();
    req->free();
    return 0;
}
```

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
  误差上限约为一个回调周期（IDLE 精度约 1 秒）。**配小于 1 秒的值不保证按值中止**。
- **没有独立低速下限**：放弃 `LOW_SPEED_LIMIT` 后，持续有数据但整体极慢的流只在设置了
  `idle_timeout` 时才受约束；未设置时库不会因为它“平均很慢”而中止。
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

- 进：URL、请求头、body、总/连接/空闲超时、响应体上限、User-Agent、重定向开关、TLS 校验开关；
  `post()`；`send_stream(cb, ud)`；`cancel()`；状态码、body、错误码、错误文案、总字节数。
- 不进：连接池、重试、代理、白名单；`multi`（异步状态机）、`share`、`mime`、`form`、
  WebSocket、`urlapi`；JSON 编解码（用 `json` 包）。
- 安全默认：TLS 校验开、重定向关。

以下三项仍在范围裁定中，未裁定前不进入实现：响应头读取、`method()`、
`HTTP_ERR_*` 枚举。跨线程取消已裁定不进入 0.1.0（S-04），默认重定向上限已裁定为
固定 10 次、不开放配置（S-03），0.1.0 的 `cancel()` 只允许同线程。
影响面见 [feature 划分 §3](docs/planning/ct-http-0.1.0-features.md#3-范围待裁定项-s-01s-05)。

## 安装（phase 10 之后才可用）

```text
ct get http@^0.1.0
ct install
```

## 门禁

```sh
bash tools/check.sh g0     # 规约纳管：spec 块 + 实例化表 + session-map + 版本落点
bash tools/check.sh g1     # 卡片十项 + 任务七列 + 判据/任务摘要冻结
bash tools/check.sh g3     # 夹具构建 + 运行 + 负例 + 生命周期/释放 + 配置面 + 真执行响应面 + 超时区分 + 流式 + 取消/生命周期 + 重定向/凭据/降级
bash test/gates/g1_negative.sh   # 证明 g1 会拒绝篡改卡
```

未实现的子命令以退出码 2 明确失败，不伪装通过。
