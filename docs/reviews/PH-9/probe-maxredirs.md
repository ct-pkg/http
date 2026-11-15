# S-03 探针：`CURLOPT_MAXREDIRS` 未设置时的默认行为

- 日期：2026-11-15
- 库：`libcurl/8.7.1 (SecureTransport) LibreSSL/3.3.6 zlib/1.2.12 nghttp2/1.69.0`
- 平台：macOS arm64，系统 cc
- 探针源码：[probe-maxredirs.c](probe-maxredirs.c)

## 1. 命令

```sh
cc -std=c99 -Wall -Wextra -Werror docs/reviews/PH-9/probe-maxredirs.c \
   -lcurl -o /tmp/probe-maxredirs
/tmp/probe-maxredirs 15
/tmp/probe-maxredirs 60
```

服务端在本地回环起一条链：`/hop/0` → … → `/hop/<chain>`，除最后一跳外均回 `302`
与相对 `Location`，最后一跳回 `200`；子进程在共享内存里数实际服务的请求数。

## 2. 原始输出

```text
$ /tmp/probe-maxredirs 15
libcurl libcurl/8.7.1 (SecureTransport) LibreSSL/3.3.6 zlib/1.2.12 nghttp2/1.69.0 chain=15
default rc=0 code=200 redirects=14 requests=15
maxredirs=10 rc=47 code=302 redirects=10 requests=11
maxredirs=12 rc=47 code=302 redirects=12 requests=13
default_again rc=0 code=200 redirects=14 requests=15
rc=0

$ /tmp/probe-maxredirs 60
libcurl libcurl/8.7.1 (SecureTransport) LibreSSL/3.3.6 zlib/1.2.12 nghttp2/1.69.0 chain=60
default rc=47 code=302 redirects=30 requests=31
maxredirs=10 rc=47 code=302 redirects=10 requests=11
maxredirs=12 rc=47 code=302 redirects=12 requests=13
default_again rc=47 code=302 redirects=30 requests=31
rc=0
```

两次运行的退出码均为 `0`（探针自身成功；`rc=47` 是被测场景的 `CURLcode`）。

## 3. 结论

1. **未设置时不是 10 跳。** 15 跳链被完整跟随：`rc=0`、`code=200`、`redirects=14`、
   服务端请求 15 次。
2. **未设置时的实际上限是 30 跳。** 60 跳链在 `redirects=30`、`requests=31` 处停住，
   `rc=47`（`CURLE_TOO_MANY_REDIRECTS`）；第 31 个 302 的目标没有请求。
   这说明 8.7.1 的“未设置”等价于库内固定上限 30，而不是无限。
3. **显式 10 的行为可测且精确。** `redirects=10`、`requests=11`、`rc=47`：
   收到第 11 个 302 后不再跟随，它的目标请求不发。
4. **对卡片的含义**：R-08 的“沿用 libcurl 默认”实际等价于“30 跳”，而 30 是依赖内部
   常量；把它写进契约等于把上限交给依赖版本决定，且跨 libcurl 版本不保证稳定。
   自定一个可测值（10）才能让判据与文档说同一件事。
