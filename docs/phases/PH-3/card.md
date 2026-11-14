# PH-3 卡：请求配置与输入校验

- 编号：PH-3
- phase 目录：`docs/phases/PH-3/`
- version-minor：3
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：7ece31030a3ea4b698c2848b81a6bca48a198cb97c714fe43f7adc9463e27a7d
- tasks-sha256：5792bca67161d830e569a0743c59c36f0fe63aa625adca4a02f83839d04faa83

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-3（与目录 `docs/phases/PH-3/` 一致） |
| 对应 roadmap | [phase 3](../planning/ct-http-0.1.0-roadmap.md#phase-3--请求配置与输入校验) |
| 对应 feature | F-15、F-01、F-06、F-07 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 不为 | 本卡不裁定 S-01…S-05；不 `perform`、不发网络、不做 `method()` / `max_redirects` / 响应头 / 跨线程取消 / `HTTP_ERR_*` |

## 1. 目标

让请求的 URL、头、body、超时、UA、TLS 与重定向开关写进 C 结构体并做本地校验：URL 由请求自持、头拒绝 CR/LF、body 按显式长度先设 `POSTFIELDSIZE` 再 `COPYPOSTFIELDS`、只接受 http(s)。

第三人复述：覆写调用方 URL 缓冲区后请求仍持有原 URL；`header("X","a\r\nB: c")` 被本地拒绝且头计数不增；`body("a\0b",3)` 存下 3 字节且 `stage==2`；`file://` / `ftp://` 在连接前拿到本地负数码；不调 setter 时 `insecure==0`、`follow_redirects==0`、UA 等于写死值。`send()` 仍是固定值 stub。

## 2. 范围

### 2.1 做什么

- `src/http/http.shim.c`：URL 深拷贝并在 destroy 释放；头写 `curl_slist` 且拒 CR/LF；body 深拷贝 + `POSTFIELDSIZE` → `COPYPOSTFIELDS` 顺序捕获；`timeout` / `connect_timeout` / `user_agent` / `insecure` / `follow_redirects` 的 setopt；`CURLOPT_PROTOCOLS_STR` 与 `CURLOPT_REDIR_PROTOCOLS_STR` 只放 http/https；本地负数码空间。
- `src/http/http.shim.h`：新增字段与声明追加在既有字段之后；`HTTP_LOCAL_ERR_*` 宏（不使用 `HTTP_ERR_*` 前缀，避免抢占 S-05）；默认 UA 宏；头行读取测试钩子。
- `src/http/http.m.ct`：`RequestBuilder` 增加 `connect_timeout` / `body` / `user_agent` / `insecure` / `follow_redirects`；已冻结的方法名不改。
- `test/fixtures/p3-*`：五个 C 夹具 + 一个 CT 链夹具 + expected。
- `test/run.sh`、`tools/check.sh` 的 `g3`、`README.md`、`docs/session-map.md`。

### 2.2 不做什么

- 不 `curl_easy_perform`、不访问网络；`send()` 仍返回固定值（phase 4 换真执行）。
- 不做 `method()`（S-02）、`max_redirects`（S-03）、响应头读取（S-01）、跨线程取消（S-04）、`HTTP_ERR_*` 枚举（S-05）。
- 不做流式、不做取消、不做错误面公开（phase 5 之前 `local_error` 只作内部字段与测试锚点）。
- 不改 `docs/requirements.md`、`docs/design.md`；不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | 追加字段、`HTTP_LOCAL_ERR_*`、`HTTP_DEFAULT_USER_AGENT`、五个 setter、头行钩子 |
| `src/http/http.shim.c` | URL/头/body 所有权与校验、五个 setopt、协议掩码 |
| `src/http/http.m.ct` | `RequestBuilder` 五个新方法 |
| `test/fixtures/p3-url-own.c` + `.expected.out` | 覆写调用方缓冲区后请求仍持有原 URL |
| `test/fixtures/p3-header-crlf.c` + `.expected.out` | 值与名含 CR/LF 均被拒；合法头进 slist |
| `test/fixtures/p3-body-nul.c` + `.expected.out` | 3 字节含 NUL；`stage==2`；负长度本地错误 |
| `test/fixtures/p3-scheme.c` + `.expected.out` | `file` / `ftp` 负数码；`http` / `HTTPS` 为 0 |
| `test/fixtures/p3-defaults.c` + `.expected.out` | 默认值逐项 + setter 落值 + 负超时本地错误 |
| `test/fixtures/p3-chain.ct` + `.expected.out` | 五方法链 + `send()` + 双 `free()` |
| `test/run.sh` | 六个 `p3-*` 夹具接线 |
| `tools/check.sh` | `g3` 纳入六个 `p3-*` |
| `README.md`、`docs/session-map.md` | 配置面与默认值文档；PH-3 登记 |
| `docs/phases/PH-3/red/T1.md` … `T7.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | URL 深拷贝与 destroy 释放、`p3-url-own.c` | `test(http): PH3-T1 红` + `feat(http): PH3-T1 URL 自持` | `git revert` 实现提交后 `url` 回到借用，覆写缓冲区夹具变红 | `bash test/run.sh p3-url-own` | S | E-P3-1 |
| T2 | 头 slist、CR/LF 拒绝、`http_shim_header_line`、`p3-header-crlf.c` | `test(http): PH3-T2 红` + `feat(http): PH3-T2 头校验` | `git revert` 实现提交后头只计数不建 slist，CR/LF 夹具变红 | `bash test/run.sh p3-header-crlf` | M | E-P3-2 |
| T3 | body 深拷贝、`POSTFIELDSIZE` → `COPYPOSTFIELDS` 顺序、`p3-body-nul.c` | `test(http): PH3-T3 红` + `feat(http): PH3-T3 body 长度` | `git revert` 实现提交后 body setter 消失，夹具构建失败 | `bash test/run.sh p3-body-nul` | M | E-P3-3 |
| T4 | scheme 白名单、本地负数码、`p3-scheme.c` | `test(http): PH3-T4 红` + `feat(http): PH3-T4 协议白名单` | `git revert` 实现提交后 `local_error` 消失，`file://` 夹具变红 | `bash test/run.sh p3-scheme` | S | E-P3-4 |
| T5 | 五个单值 setter、默认值、`p3-defaults.c` | `test(http): PH3-T5 红` + `feat(http): PH3-T5 配置默认值` | `git revert` 实现提交后 setter 消失，默认值夹具构建失败 | `bash test/run.sh p3-defaults` | M | E-P3-5 |
| T6 | `http.m.ct` 五个新方法、`p3-chain.ct` | `test(http): PH3-T6 红` + `feat(http): PH3-T6 链路扩面` | `git revert` 实现提交后链缺 `no method 'body'`，夹具构建失败 | `bash test/run.sh p3-chain` | S | E-P3-6 |
| T7 | `README.md` 配置面、`g3` 纳入 `p3-*`、收口文档 | `test(docs): PH3-T7 红` + `docs: PH3-T7 配置面与收口` | `git revert` 文档提交后 README 回到 PH-2 口径，不影响构建 | `bash tools/check.sh g3 && grep -q 'connect_timeout' README.md` | S | E-P3-7 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P3-1 | URL 深拷贝：覆写调用方缓冲区后请求仍返回原 URL | `bash test/run.sh p3-url-own` 退出 0，输出 `owned 1 url https://example.test/a`；负例：借用时输出 `owned 0` 并红 |
| E-P3-2 | 头名或头值含 CR/LF 时本地拒绝且头计数不增；合法头进 slist 且行文本正确 | `bash test/run.sh p3-header-crlf` 退出 0，输出 `rejected 1 1 added 0 1 first Accept: application/json` |
| E-P3-3 | body 按显式长度存储，NUL 不截断，且先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | `bash test/run.sh p3-body-nul` 退出 0，输出 `len 3 stage 2 bytes 97 0 98` 与 `badlen 1` |
| E-P3-4 | 只接受 http/https；其余在连接前给本地负数码，且源码无 `curl_easy_perform` | `bash test/run.sh p3-scheme` 退出 0，输出 `file -1004 ftp -1004 http 0 https 0`；`grep -c 'curl_easy_perform' src/http/http.shim.c` 输出 `0` |
| E-P3-5 | 默认值与 setter 落值逐项可核对；负超时给本地错误且不改旧值 | `bash test/run.sh p3-defaults` 退出 0，输出 `defaults 0 0 0 0 ct-http/0.1.0`、`set 3000 1000 1 1 probe/1`、`neg -1005 3000` |
| E-P3-6 | CT 链可编译：五个新方法 + `send()` + 双 `free()` | `bash test/run.sh p3-chain` 退出 0，输出 `200 {"users":3}` |
| E-P3-7 | 门禁全绿；README 写明配置面与默认值；CT 单元不含 curl 符号 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q 'connect_timeout' README.md` 退出 0；`grep -rn 'curl_easy_\|curl_slist_' src/http/*.m.ct src/http/*.h.ct` 零命中 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。红证写进 `docs/phases/PH-3/red/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha> 后同命令退出 0
```

- 红证必须用 `baseline` 提交复跑，不能拿工作树当前产物冒充。
- T1/T3 的红是构建失败（缺少 `http_request_body` / `http_request_local_error`）或输出不匹配（`owned 0`），不是 runner 未接线。
- T2/T5 的红是 C 夹具缺少新字段/新钩子导致 `cc -Werror` 失败。
- T6 的红是 `ct build` 报 `no method 'body' for type 'CRequest *'`。
- T7 的红是 `grep -q 'connect_timeout' README.md` 失败，不是「没有代码改动」。

## 7. 依赖

- `ct v0.44.1`（`/Users/samxx/.local/bin/ct`），`.shim.c` 自动进入 cc 行。
- 系统 libcurl 8.7.1（`-lcurl`）；`CURLOPT_PROTOCOLS_STR` / `CURLOPT_REDIR_PROTOCOLS_STR` 需 ≥7.85.0，本机满足。
- 前置：PH-2 收口 `c9ba2d0`；`bash tools/check.sh g0`、`g1`、`g3` 在开工前为绿。
- `ct build` 仍不读 `ct.pkg` 的 `ldflags`，`test/run.sh` 显式传 `--ldflags "-lcurl"`。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| roadmap 的 E-P3-1/E-P3-3 写「服务端收 3 字节 / 服务端计数 0」，本 phase 不 `perform` 无法起真实服务端 | 夹具无处断言 | 判据改写成 shim 状态锚点（`body_len`、`body_stage`、`local_error`、`grep curl_easy_perform` 为 0），偏差写进收口 |
| `header()` 返回 `CRequest *`，无法把本地错误回传调用点 | 调用方看不到失败 | 请求内保存首个 `local_error`；公开错误面留到 PH-5，本 phase 只做测试锚点 |
| `local_error` 被误当成公开错误码 | 调用点开始依赖字段 | 卡与 README 明写「phase 5 之前不公开」；`HTTP_LOCAL_ERR_*` 命名避开 S-05 的 `HTTP_ERR_*` |
| `CURLOPT_PROTOCOLS` 在 8.x 被标记 deprecated，`-Werror` 可能变红 | cc 报 `-Wdeprecated-declarations` | 使用 `_STR` 变体，不使用数值宏 |
| 结构体追加字段破坏 PH-1 ABI 夹具 | `phase1_abi.c` 字段顺序断言失败 | 新字段只追加在 `ops` 之后；`phase1_abi` 仍在 `g3` 回归 |
| `send()` 仍为固定值被误认为真请求 | 输出恰好 `200` 但无网络 | 卡与 README 明写 stub；真执行在 PH-4 |

## 9. 收口动作

1. 逐条跑 E-P3-1…E-P3-7，把命令、原始输出、退出码贴进 `docs/phases/PH-3/closeout.md`。
2. 对照 §4 任务表，逐个确认「红证提交 → 实现提交」相邻且 `baseline` 是父提交。
3. 检查 §3 交付物清单每个路径都已入库，无 `build/` 产物或临时文件混入。
4. 偏差只写进收口文档，不改本卡 §4/§5；判据如需变更，关闭 PH-3 并新开卡。
5. 版本：保持 `0.1.0`，不移动 MINOR；不 push、不打 tag。
6. 报告一行当前事实：最小调用 `ct build` 与运行各返回什么、是否真发请求（本 phase 仍是固定值），以及配置面可断言项。
