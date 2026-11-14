# ct-http 0.1.0 roadmap

- 日期：2026-11-15
- 版本：0.1.0
- 状态：规划稿。phase 1 起按本文件施工；本文件不修改 `docs/requirements.md` 与 `docs/design.md`。
- 仓库：`/Users/samxx/trae-ws/ct-http`
- 基线：`ct v0.44.1`（`/Users/samxx/.local/bin/ct`），macOS arm64，系统 cc / 系统 libcurl。

## 0. 阅读约定

**编号是阅读顺序，不是执行顺序。** phase 号按「先有接口、再有内核、再有能力、最后收口」排；实际执行以 §3 的依赖与并行组为准。

**判据门槛**（每条退出判据必须满足）：

- 有命令或可核对锚点；不接受「能编译」「门禁绿」单独作为判据。
- 至少一条负例：实现做错时断言必须变红。
- 端到端网络夹具一律走本地回环，不访问外网。
- 门禁未接线时退出 2，不得返回 0。

**待裁定项**：响应头读取、`method()`、默认 `max_redirects(10)`、跨线程取消、`HTTP_ERR_*` 枚举。逐项影响面见 [ct-http-0.1.0-features.md](ct-http-0.1.0-features.md) §3。凡受这些项影响的 phase，在 §1 标注「gated」。

## 1. 接口约定（0.1.0 冻结）

以下形态已在 `ct v0.44.1` 实测通过，不再论证可行性：

1. **公开面是 `.m.ct` 映射单元**，不是 `.h.ct`、不是普通 `.ct`。使用者必须 `import` 承载映射的 `.m.ct`（本包为 `import http::http;`），方法才存在。
2. **只 import 类型/常量单元（`.h.ct`）不会带来方法**。实测报 `no method 'post' for type 'char *'`。这条必须写进 README 与所有样例。
3. **`.m.ct` 不能 import `.h.ct`**，所以 C 结构体布局放在同名 `.shim.h`，CT 侧用 `pub type CRequest = struct HttpRequest;` 这类别名承接；映射单元自身不产生函数、不写 `impl`、不写 `Drop`。
4. **同目录同 base 的 `.h.ct` 与 `.m.ct` 会 import 歧义**。常量/枚举单元必须换 base（如 `Types.h.ct`），使用时同时 import 两个单元。
5. **三段宿主，全部静态映射**：
   - `char *` 宿主只做工厂（`.post()`），不存状态；
   - `CRequest *` 宿主承载 `.header()` / `.timeout()` 等配置，返回同一个宿主指针；
   - `CResponse *` 宿主只做只读视图（`.status()` 等）。
   每一跳展开成嵌套裸 C 调用，无 vtable、无装箱、无临时对象。
6. **指针宿主用 `->`，值宿主用 `.`**。本包统一返回 struct 指针，调用点一律 `->`；`r.status()` 这类点调用在指针上会编译通过但传错地址，门禁必须用 `-Werror=incompatible-pointer-types` 拦。
7. **运行时替换实现**：`struct HttpRequest` / `struct HttpResponse` 内放 ops 函数指针表；测试夹具替换 ops，不改 CT 调用点，也不改 `.m.ct`。
8. **显式释放**：`http_request_free()` / `http_response_free()` 是唯一释放入口。`send()` 不释放请求对象；跨单元 `delete` 不执行定义单元的 Drop 体，因此不用 `delete` 作为公开释放方式。
9. **libcurl 只在 `.shim.h` / `.shim.c`**；`.m.ct` 的 recipe 只调 `http_*` shim 函数，CT 单元不出现 `curl_*` 符号。
10. **入口动词**：0.1.0 基线用固定 `.post()`（与 R-10 现行「只保留 post」口径一致）。`method()` 属待裁定项 S-02，纳入时新增 `.method("GET")` 入口，不改已冻结的方法名。
11. **回调**：`HttpChunkCb` 声明在 `.shim.h`；只接受命名函数 + `void *ud`，闭包不能填该签名；`ud` 由调用方保活。
12. **常量**：数值单一来源在 `.shim.h`；CT 可见常量放独立 base 的 `.h.ct`，数值一致性由 g3 的 C 断言核对。

## 2. 阶段总览

| phase | 一句话目标 | 对应 feature | 依赖 | 并行组 | gated |
| --- | --- | --- | --- | --- | --- |
| 1 | 冻结公开面：字符串起手链能编译并跑出固定值 | F-12、F-13、F-22（最小门禁） | 无 | A | 否 |
| 2 | C shim 内核与显式释放落地，cleanup 恰好一次 | F-10、F-14 | phase 1 | B | 否 |
| 3 | 请求配置面与输入校验：body 二进制安全、头拒绝 CR/LF、只允许 http(s) | F-15、F-01、F-06、F-07 | phase 2 | C | 否 |
| 4 | `send()` 同步执行与响应读取面，`take_body` 单所有权 | F-16、F-04 | phase 3 | D | S-01 影响读取面 |
| 5 | 错误模型与谓词真值表：三类错误不混，`is_ok`/`is_cancelled` 互斥 | F-17、F-05 | phase 4 | E | S-05 |
| 6 | 三种超时可区分：连接 / 总时长 / 空闲 | F-02 | phase 5 | E | 否 |
| 7 | 流式执行与慢流保护：分片、回调中止、真 idle | F-18、F-03 | phase 5 | E | 否 |
| 8 | 取消与生命周期：同线程取消；消灭自动释放 UAF | F-19、F-11 | phase 7 | F | S-04 |
| 9 | 重定向与安全策略：跨主机剥离凭据、拒绝 https→http | F-08、F-09 | phase 4 | E | S-03 |
| 10 | 跨平台、sanitizer、门禁与发布材料收口 | F-21、F-23、F-22（全量） | 全部 | G | S-01/S-02 影响文档 |

**并行组**：A 无前置；B 只依赖 A；C 只依赖 B；D 只依赖 C；E 组（phase 5/6/7/9）在 phase 4 收口后可换序或并行，但共享 shim 文件时建议单写者串行；F 依赖 E 中的 7；G 最后。**phase 6 与 phase 7 的错误码断言都要求 phase 5 先冻结错误空间。**

## 3. 阶段详述

### phase 1 — 接口冻结与可编译链

- **目标**：把公开面、C 结构体布局、三段静态映射和样例冻结成可编译、可运行的最小链路；不接网络、不接真 curl。
- **范围（做）**：
  - `src/http/http.shim.h`：`struct HttpRequest` / `struct HttpResponse` 布局 + 固定值 stub 函数。
  - `src/http/http.m.ct`：`pub type CRequest/CResponse`、三个静态 trait、三段 `map trait … for type …`。
  - `src/http/Types.h.ct`：独立 base 的常量单元，供「只 import 类型单元」负例使用。
  - `test/fixtures/phase1_chain.ct` + expected；`phase1_var.ct` + expected；`phase1_no_mapping.ct`（负例）；`phase1_abi.c` + expected。
  - `tools/check.sh` 最小 `g1`（卡片十项 + 判据冻结摘要）与 `g3`（构建 + 运行 + 负例）；`test/gates/g1_negative.sh`。
- **范围（不做）**：不发请求、不链接 libcurl、不实现释放语义、不引入 `method()` / 响应头 / `max_redirects` / 跨线程取消 / `HTTP_ERR_*`。
- **退出判据**：
  - `E-P1-1`：`bash tools/check.sh g1` 退出 0；`bash test/gates/g1_negative.sh` 退出 0，且其内部篡改卡跑出 `FAIL g1` 与退出 1（负例留证）。
  - `E-P1-2`：`bash tools/check.sh g3` 退出 0；`ct build` 无 `warning:`；运行输出与 expected 逐字节相等。
  - `E-P1-3`：链 `"url"->post()->header(…)->timeout(…)->send()->status()` 与变量落点形式（`CRequest *r = …; r->header(…);`）均 `ct build` 退出 0。
  - `E-P1-4`：只 `import http::Types;` 的负例构建退出非 0，错误文案含 `no method 'post' for type 'char *'`。
  - `E-P1-5`：`bash tools/check.sh g0` 退出 0。
  - `E-P1-6`：`bash test/run.sh phase1_abi` 退出 0（`cc -Wall -Wextra -Werror` 零警告）。
- **依赖**：无。
- **风险**：namespace 目录与 D-1（`src/` vs `http/`）尚未收口，本 phase 采用 `src/http/` + `namespace http;`，偏差写进卡尾；如果后续把包根改成 `http/`，只挪路径，不改映射。
- **对应 feature**：F-12、F-13、F-22（最小门禁部分）。

### phase 2 — C shim 内核与显式释放

- **目标**：真 shim 接管 easy handle 与响应内存；`http_request_free` / `http_response_free` 是唯一释放入口，cleanup 恰好一次。
- **范围（做）**：`http.shim.c`；`curl_easy_init` / `curl_easy_cleanup`；ops 函数指针表；shim 计数器；双释放置空；`ct.pkg` 的 `-lcurl` 在门禁里真实生效。
- **不做**：不 `perform`、不发网络；不实现 body/header 的实际 setopt（phase 3）。
- **退出判据**：
  - `E-P2-1`：`test/run.sh p2-cleanup-once`：`http_request_free` 后 shim 计数 `easy_cleanup==1`。
  - `E-P2-2`：`test/run.sh p2-double-free`：同一指针二次 free 退出 0，计数器仍为 1（负例：不置空 → ASAN double-free 红）。
  - `E-P2-3`：`test/run.sh p2-after-send`：`send()` 后请求仍可读，cleanup 计数为 0（证明没有自动释放）。
  - `E-P2-4`：`grep -rn 'curl_easy_\|curl_slist_' src/*.m.ct src/*.h.ct` 零命中；`nm -u` CT 对象无 curl 符号。
- **依赖**：phase 1。
- **风险**：macOS clang 的 ASAN/LSAN 需要 `--cflags/-ldflags` 透传；Linux 未在本机验证，留给 phase 10。
- **对应 feature**：F-10、F-14。

### phase 3 — 请求配置与输入校验

- **目标**：所有配置写进 shim 结构体并做本地校验；body 二进制安全、头拒绝 CR/LF、只允许 http(s)。
- **范围（做）**：URL 深拷贝；`header()` 追加 slist 并拒绝 CR/LF；`body()` 先设 `POSTFIELDSIZE` 再 `COPYPOSTFIELDS`；`timeout` / `connect_timeout`；`user_agent`；`insecure`；`follow_redirects`；`CURLOPT_PROTOCOLS` / `REDIR_PROTOCOLS` 仅 HTTP/HTTPS；本地负数码空间（不含 `HTTP_ERR_*` 枚举）。
- **不做**：不 `perform`；不做 `method()`；不做 `max_redirects`；不读响应头。
- **退出判据**：
  - `E-P3-1`：`test/run.sh p3-body-nul`：`body("a\0b", 3)` 的 `POSTFIELDSIZE==3`，服务端收 3 字节。
  - `E-P3-2`：`test/run.sh p3-header-crlf`：`header("X","a\r\nB: c")` 返回本地错误，slist 计数不增（负例：透传 → 红）。
  - `E-P3-3`：`test/run.sh p3-scheme`：`file:///etc/passwd` 与 `ftp://…` 连接前返回本地错误，服务端计数 0。
  - `E-P3-4`：默认值夹具：不调 setter 时 `insecure==0`、`follow_redirects==0`、UA 等于写死值。
- **依赖**：phase 2。
- **风险**：本地错误码空间与 S-05 有关；本 phase 先用负数码并文档化，若 S-05 纳入枚举，只加映射不改调用点。
- **对应 feature**：F-15、F-01、F-06、F-07。

### phase 4 — 同步执行与响应读取面

- **目标**：`send()` 完成一次真实回环请求并填响应；body 所有权单一，`take_body` 后不再二次释放。
- **范围（做）**：`curl_easy_perform`；write 回调缓冲；`getinfo` 状态码/字节数；响应冻结；`http_response_free`；`take_body` 置空字段。
- **不做**：不做流式、不做取消、不做重定向策略（只沿用默认不跟随）、不读响应头。
- **退出判据**：
  - `E-P4-1`：`test/run.sh p4-status`：本地 200/404/500 三例，`status_code` 与 `body` 逐字节断言。
  - `E-P4-2`：`test/run.sh p4-binary`：响应含 `\0`，用 `body_len` 比较而不是 `strlen`。
  - `E-P4-3`：`test/run.sh p4-take-body`：`take_body` 后 `http_response_free` + `free(body)`，ASAN 0 报错（负例：不清空 → double-free 红）。
  - `E-P4-4`：`test/run.sh p4-total-bytes`：`total_bytes` 等于 write 回调累计字节。
- **依赖**：phase 3。
- **风险**：本地夹具服务器的端口占用与并发；`total_bytes` 口径必须在本 phase 写死（解码后 body 字节）。
- **对应 feature**：F-16、F-04。**gated：S-01**（若纳入响应头读取，本 phase 的响应面与夹具扩项）。

### phase 5 — 错误模型与谓词真值表

- **目标**：传输错误、HTTP 错误、本地参数错误三类不混；`is_ok` 与 `is_cancelled` 永远互斥。
- **范围（做）**：`CURLcode` 原值透传；本地错误负数码；`error_msg` / `error_detail`；4xx/5xx `error_code==0`；未知码保留；取消态真值表。
- **不做**：不做 `HTTP_ERR_*` 枚举（S-05 未定）；不做重试分类。
- **退出判据**：
  - `E-P5-1`：`test/run.sh p5-table`：DNS 6、拒绝 7、超时 28、TLS 60、中止 42、写 23 逐项相等。
  - `E-P5-2`：`test/run.sh p5-http-error`：500 时 `error_code==0`、`is_ok()==0`。
  - `E-P5-3`：`test/run.sh p5-unknown`：注入未知码，`error_code` 保留原值。
  - `E-P5-4`：`test/run.sh p5-predicates`：ok/404/传输错误/取消四态矩阵；取消态 `is_cancelled==1 && is_ok==0`。
- **依赖**：phase 4。
- **风险**：DNS 失败夹具不能访问外网；优先用 `127.0.0.1` 的拒绝路径与可控解析失败；S-05 纳入时本 phase 返工映射表。
- **对应 feature**：F-17、F-05。**gated：S-05**。

### phase 6 — 三种超时区分

- **目标**：连接超时、总超时、空闲超时不再都只给一个 `CURLcode 28`，调用方能从结果里区分。
- **范围（做）**：shim 记录阶段与时间戳；`error_detail` 带 `phase=connect|first_byte|total|idle`；`idle_timeout` 的真实语义（与 phase 7 共用进度回调）。
- **不做**：不做异步解析器；DNS 阶段不可取消的边界写进文档。
- **退出判据**：
  - `E-P6-1`：`test/run.sh p6-connect`：不可达地址 + `connect_timeout` → `error_detail` 含 `phase=connect`。
  - `E-P6-2`：`test/run.sh p6-total`：`/delay` + `timeout` → `phase=total`。
  - `E-P6-3`：`test/run.sh p6-negative`：负超时返回本地错误，服务端计数 0。
- **依赖**：phase 5。
- **风险**：e2e 计时抖动；核心断言放在 shim 捕获的阶段字段，端到端只做一次。
- **对应 feature**：F-02。

### phase 7 — 流式执行与慢流保护

- **目标**：大响应分片交付、回调可中止；慢而持续的流不被误杀。
- **范围（做）**：`send_stream(cb, ud)`；`HttpChunkCb`；回调中止 → 23 且 `is_cancelled==0`；`body` 与 stream 互斥；真 idle 由进度回调时间戳实现，不用 `LOW_SPEED_LIMIT=1`。
- **不做**：不做异步/多路复用、断点续传、回调内重入。
- **退出判据**：
  - `E-P7-1`：`test/run.sh p7-chunks`：1 MB 至少 2 片，拼接逐字节相等。
  - `E-P7-2`：`test/run.sh p7-abort`：首片返回非 0，`error_code==23`、`is_cancelled==0`。
  - `E-P7-3`：`test/run.sh p7-slow`：每 2 秒 1 字节持续 6 秒，`idle_timeout(3)` 不中止（负例：`LOW_SPEED_LIMIT=1` → 红）。
  - `E-P7-4`：`test/run.sh p7-互斥`：同时设 `body` 与 stream，连接前本地错误。
- **依赖**：phase 5。
- **风险**：慢流夹具耗时较长；回调 typedef 在 shim 头，闭包不可用，文档必须写「命名函数」。
- **对应 feature**：F-18、F-03。

### phase 8 — 取消与生命周期

- **目标**：同线程内可确定性取消；`send()` 不再自动释放请求，UAF 路径消失。
- **范围（做）**：取消标志 + 进度回调读位；`is_cancelled` 只在取消真正中断时置位；`send()` 后请求仍有效；显式 free 契约写进 README。
- **不做**：默认不做跨线程取消（S-04 未定）；若 S-04 纳入，另开方向 B（引用计数/取消 token + TSAN）。
- **退出判据**：
  - `E-P8-1`：`test/run.sh p8-cancel`：慢响应中在回调内取消，3 秒内返回 42，`is_cancelled==1`、`is_ok==0`。
  - `E-P8-2`：`test/run.sh p8-not-cancelled`：未取消路径 `is_cancelled==0`。
  - `E-P8-3`：`test/run.sh p8-lifetime`：`send()` 返回后继续读请求字段、再 free，ASAN 0 报错（负例：send 内 free → use-after-free 红）。
- **依赖**：phase 7。
- **风险**：DNS 阶段不可取消；进度回调必须显式开启；S-04 决定是否保留方向 B。
- **对应 feature**：F-19、F-11。**gated：S-04**。

### phase 9 — 重定向与安全策略

- **目标**：跟随重定向时凭据不跨主机泄漏、TLS 不降级。
- **范围（做）**：`follow_redirects(1)` 显式开启；同源保留自定义头，跨源头剥离 `Authorization` / `Cookie` / `Proxy-Authorization`；默认拒绝 https→http；重定向协议掩码仅 HTTP/HTTPS。
- **不做**：不做白名单、不做自定义跳转上限（除非 S-03 纳入）。
- **退出判据**：
  - `E-P9-1`：`test/run.sh p9-same-host`：302 同主机跟随，`status_code==200`、body 正确。
  - `E-P9-2`：`test/run.sh p9-cross-host`：A→B 重定向，B 回显无 `Authorization`（负例：默认转发 → 红）。
  - `E-P9-3`：`test/run.sh p9-downgrade`：https→http 被拒，明文服务端计数 0。
  - `E-P9-4`：默认不跟随：302 时 `status_code==302`、`is_ok()==0`、访问计数 1。
- **依赖**：phase 4。
- **风险**：libcurl 的 header 转发与 `UNRESTRICTED_AUTH` 语义；S-03 改变默认跳数。
- **对应 feature**：F-08、F-09。**gated：S-03**。

### phase 10 — 跨平台、sanitizer 与发布材料

- **目标**：同一份源码在 macOS/Linux 系统 libcurl 上干净运行；门禁与文档可执行；0.1.0 收口。
- **范围（做）**：双平台 `test/run.sh all`；ASAN/LSAN；g0–g9 全量接线；`docs/snippets/` 正例与 impl-chain 负例；README 接口约定与限制段；`ct.pkg` / README 版本落点；tag 提案（不推送）。
- **不做**：不推送分支/tag；不提交索引行；不做 Windows。
- **退出判据**：
  - `E-P10-1`：macOS 与 Linux 原始输出均 0 失败。
  - `E-P10-2`：`bash tools/check.sh g0 && … && bash tools/check.sh g9` 退出 0。
  - `E-P10-3`：`g4` 编译 `docs/snippets/` 正例；含 `req.url(...).header(...)` 的 impl-chain 负例被拒。
  - `E-P10-4`：干净临时目录 `ct get http@^0.1.0 && ct install` 后样例得到 200。
  - `E-P10-5`：`ct.pkg#version` 与 README 版本行均为 `0.1.0`；tag 只出提案。
- **依赖**：全部前序 phase。
- **风险**：本机没有 Linux；双平台需 CI 或第二台机器；tag/push 需用户显式指令。
- **对应 feature**：F-21、F-23、F-22（全量）。**gated：S-01/S-02**（文档与样例随裁定更新）。

## 4. 不属于 0.1.0 的范围

- 连接池、重试、代理、域名白名单。
- `multi` / `share` / `mime` / `form` / WebSocket / `urlapi`。
- JSON 编解码、cookie 管理、自动 Content-Type。
- 自定义 CA、客户端证书、TLS 版本选择。
- Windows、静态链接 curl、vendored curl。

以上任一若要进入 0.1.0，先走需求层立项，不在本 roadmap 内改判据。
