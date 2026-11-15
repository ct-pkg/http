# http

`libcurl` easy handle 的 HTTP 客户端包。对外是链式接口：字符串起手，请求对象承载配置，
响应对象只读。

- applies-to: 0.2.0
- 版本：0.1.0
- 许可：MIT（`.ct` / shim / 测试）+ curl 原许可（链接的系统 libcurl）
- 平台：macOS 与 Linux 的系统 libcurl
- 规划：[0.1.0 roadmap](docs/planning/ct-http-0.1.0-roadmap.md)、
  [feature 划分](docs/planning/ct-http-0.1.0-features.md)
- 当前 phase 卡：[PH-10](docs/phases/PH-10/card.md)
- 设计稿：[docs/design.md](docs/design.md)

## 定位与边界

只做「发一个请求并读回结果」这条最短路径。

- 进：URL、请求头、body、总/连接/空闲超时、响应体上限、User-Agent、重定向开关、TLS 校验开关；
  `post()`；`send_stream(cb, ud)`；`cancel()`；状态码、body、错误码、错误文案、总字节数。
- 不进：连接池、重试、代理、白名单；`multi`（异步状态机）、`share`、`mime`、`form`、
  WebSocket、`urlapi`；JSON 编解码（用 `json` 包）。
- 安全默认：TLS 校验开、重定向关。

## 依赖与安装

- `ct v0.53.1`（包清单 `requires` 的下限；统一 `ct` 入口，`ct install` 打印 `ct build`）、
  系统 `cc`、系统 libcurl（`-lcurl`）。
- `http.shim.c` 由 `ct build` 自动列入 cc 行；`ct build` 不读 `ct.pkg` 的 `ldflags`，
  所以链接参数必须显式传 `--ldflags "-lcurl"`。

<!-- snippet: docs/snippets/build.sh -->
```text
ct build main.ct -I . -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
```

消费端（发布索引后）：

<!-- snippet: docs/snippets/install.sh -->
```text
ct get http@^0.1.0
ct install
```

本机没有已发布索引与 tag，`ct get http@^0.1.0`（裸名）不可执行；本地目录 spec 可跑通：
包根布局是 `http/`，`ct install` 打印的 `-I vendor/http` 可直接使用。可复跑的本地形态见
`test/gates/p10_install.sh`；`ct install` 的 include 前缀约定作为提案记录在
[`docs/proposals/ct-install-include-prefix.md`](docs/proposals/ct-install-include-prefix.md)。

## 最小可运行示例

第一个例子由门禁编译并运行（`docs/snippets/quickstart.ct`，输出 `200 {"users":3}`）：

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

真实使用把 `p4_server_url(...)` 换成自己的 URL；`import http::http;` 不可省略，
只 import 类型/常量单元不会带来方法。

## API 索引

请求（`CRequest *`，setter 返回同一个指针）：

| 方法 | 一行说明 |
| --- | --- |
| `header(name, value)` | 请求头；名或值含 CR/LF 时本地拒绝 |
| `timeout(ms)` | 总超时；负值本地拒绝 |
| `connect_timeout(ms)` | 连接超时；负值本地拒绝 |
| `idle_timeout(ms)` | 数据空档上限；负值本地拒绝 |
| `max_body_bytes(n)` | 响应体上限；命中报 `-1008` |
| `body(data, len)` | 上传体；按显式长度深拷贝 |
| `user_agent(ua)` | User-Agent；默认 `ct-http/0.1.0` |
| `insecure(on)` | `1` 时同时关 `VERIFYPEER` 与 `VERIFYHOST` |
| `follow_redirects(on)` | `1` 时最多跟随 10 次重定向（固定，不可配） |
| `send()` / `send_stream(cb, ud)` | 执行；二者互斥 |
| `cancel()` | 同线程粘性取消 |
| `free()` | 释放请求；`.free()` 是终态 |

响应（`CResponse *`，只读）：

| 方法 | 一行说明 |
| --- | --- |
| `status()` | HTTP 状态码；传输失败为 `0` |
| `body()` / `body_len()` | 响应体首地址与字节数；流式恒为 `NULL` / `0` |
| `total_bytes()` | 实际交付给缓冲或回调的字节数 |
| `error()` | 0 = 无错；正数 = `CURLcode`；负数 = 本地拒绝 |
| `error_msg()` / `error_detail()` | 稳定文案与可复现细节 |
| `is_ok()` / `is_cancelled()` | 互斥谓词；`is_ok()` 含 `!cancelled` |
| `timeout_kind()` | `0` 非超时 / `1` 连接 / `2` 总时长 / `3` 空闲 |
| `take_body()` | 所有权转移；调用方 `free` 一次 |
| `free()` | 释放响应 |

本地拒绝码是 `HTTP_LOCAL_ERR_*`（`-1001`…`-1009`），**是公开错误面的一部分**。

## 教程

| 主题 | 文件 |
| --- | --- |
| 概念模型（三段宿主、所有权、错误模型） | [01-concepts.md](docs/tutorial/01-concepts.md) |
| 起手链与变量落点 | [02-quickstart.md](docs/tutorial/02-quickstart.md) |
| 配置面与默认值 | [03-config.md](docs/tutorial/03-config.md) |
| 响应面 | [04-response.md](docs/tutorial/04-response.md) |
| 错误面与谓词真值表 | [05-errors.md](docs/tutorial/05-errors.md) |
| 流式与慢流保护 | [06-streaming.md](docs/tutorial/06-streaming.md)、[docs/streaming.md](docs/streaming.md) |
| 取消与生命周期 | [07-cancel.md](docs/tutorial/07-cancel.md) |
| 重定向与安全策略 | [08-redirects.md](docs/tutorial/08-redirects.md) |
| 三种超时 | [09-timeouts.md](docs/tutorial/09-timeouts.md) |
| 构建、接口约定与释放 | [10-build.md](docs/tutorial/10-build.md) |

## 限制

- 跨线程取消不在 0.1.0（S-04）；DNS 解析阶段不可取消。
- 响应头读取、`method()`、`HTTP_ERR_*` 枚举仍是待裁定项（S-01/S-02/S-05），未实现。
- 重定向上限固定 10 次、不开放配置（S-03）；循环不单独检测，由该上限兜底。
- 空闲超时的精度约 1 秒（进度回调粒度），配小于 1 秒的值不保证按值中止。
- 无连接池、重试、代理、allow-list；每个请求一个 easy handle。

## 版本

`0.1.0`。落点：`ct.pkg#version` 与本文「版本」行，同一提交内必须一致。
phase 收口与发布时的四项对账由 `bash tools/check.sh g4`（`g4-version`）执行。

## 门禁

全部十道门禁由 `bash tools/check.sh gN` 提供；任何未实现的子命令退出 2，不伪装通过。

<!-- snippet: docs/snippets/gates.sh -->
```text
bash tools/check.sh g0     # spec pin, landing sections, session map, version
bash tools/check.sh g1     # phase cards, frozen digests, errata
bash tools/check.sh g2     # R-* -> ACC-* acceptance map
bash tools/check.sh g3     # every fixture, zero warnings
bash tools/check.sh g4     # snippets, anchors, applies-to, size, version
bash tools/check.sh g5     # red evidence + gate negatives
bash tools/check.sh g6     # issue ledger
bash tools/check.sh g7     # secrets, deps, sanitizers
bash tools/check.sh g8     # phase freeze, commit scope, tag
bash tools/check.sh g9     # spec pin against the master
```
