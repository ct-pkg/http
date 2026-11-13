# http

> **未立项，不作数；待提案分析、排期、立项、评审后重出。**

`libcurl` easy handle 的 HTTP 客户端包。对外两个对象：`HttpRequest`（构造、配置、执行、
`Drop` 清理）与 `HttpResponse`（只读结果）。

- 版本：0.1.0（骨架阶段，H1 尚未开工）
- 许可：MIT（`.ct` / shim / 测试）+ curl 原许可（链接的系统 libcurl）
- 平台：macOS 与 Linux 的系统 libcurl
- 设计稿：[docs/design.md](docs/design.md)
- phase 卡：[docs/phases/H1.md](docs/phases/H1.md)

> 本包目前只有仓库骨架与设计假设核验，**没有任何可编译的 HTTP 实现**。
> 下面代码是 H1–H6 的目标形态，不是现状。

```text
import http::Request;

HttpRequest *req = HttpRequest.new();
req.url("https://example.com/api");
req.header("Accept", "application/json");
req.body("{\"k\":1}", 7);
req.timeout(15);
HttpResponse *res = req.post();
if (res.is_ok()) { ... }
```

> 注意：当前编译器**不允许**把方法调用的结果当作下一个方法调用的接收者
> （见 [docs/design.md](docs/design.md) D-2），所以上面的配置是一条语句一个方法，
> 不是 `req.url(...).header(...)` 那种单表达式链。

## 定位与边界

只做“发一个请求并读回结果”这条最短路径：

- 进：easy handle 的创建 / 配置 / 执行 / 清理；URL、请求头、body、超时、
  connect 超时、User-Agent、重定向开关、TLS 校验开关；`post()`；`post_stream(cb, ud)`；
  `cancel()`；状态码、body、错误码、错误文案、总字节数。
- 不进：连接池、重试、代理、白名单；`multi`（异步状态机）、`share`、`mime`、`form`、
  WebSocket、`urlapi`；JSON 编解码（用 `json` 包）。
- 安全默认：TLS 校验开、重定向关。

## 安装（H6 之后才可用）

```text
ct get http@^0.1.0
ct install
```

## 门禁

```sh
bash tools/check.sh g0     # 规约纳管：spec 块 + 实例化表 + session-map + 版本落点
```

`g1` / `g3` / `g4` 等子命令尚未接线，调用会以退出码 2 明确失败，不伪装通过。
