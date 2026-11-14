# http 需求说明（需求层，未立项）

- 日期：2026-11-13
- 状态：需求梳理稿；未立项，不进入设计、实现、排期与评审。
- 仓库：`ct-pkg/http`（本地 `/Users/samxx/trae-ws/ct-http`），当前只有骨架与设计假设，没有实现。
- 输入：`README.md`、`docs/design.md`、`docs/phases/H1.md`、`docs/phases/H2-H6.md`、`AGENTS.md`、`ct.pkg`。
- 输入缺口：本仓**没有** `docs/proposal.md` 与 `docs/analysis.md`。`git ls-files docs` 只列出
  `design.md`、`phases/H1.md`、`phases/H2-H6.md`、`session-map.md`、`spec/ai-coding-workflow.md`。
  本文件把 `design.md` 的「结论/决策/阶段」当作提案，把 `design.md` §1 的语法探针与 H2–H6 的
  任务写当作评估证据；不修改任何原文。
- 核对方式：只读。当前环境没有 `ct` 命令，也没有 `selfhost/ct-ast/ct-ast` 产物，因此
  `design.md` §1 的探针未复跑；CT 能力判定以 `ct-lang` 的测试夹具与编译器源码为证据，逐条标行号。
- 格式：沿用 `ct-lang/docs/planning/requirements-pool.md` 的字段语义（id / what / why / 验收 /
  范围外 / 阻塞），压缩为「问题—判据—非目标—依赖」，不复制其长表排版。

## 1. 判据门槛与阻塞级别

### 1.1 什么算验收判据

- 必须给出命令或可观察事件：服务端访问计数、逐字节比对、错误码、sanitizer 输出、符号表检查。
- 每条至少带一条负例：实现若做错，断言必须变红。
- 「能编译」「门禁绿」不算判据；门禁只是判据的承载方式。
- 端到端网络夹具一律走本地回环，不访问外网。

### 1.2 阻塞级别

| 级别 | 定义 | 处置 |
| --- | --- | --- |
| 硬阻塞 | 现有 CT 没有等价能力，且无语法/布局绕过；必须改编译器 | 不得开工，先立语言需求 |
| 软阻塞 | 能力缺失或写法受限，但有等价写法/布局绕过；不改编译器 | 按绕法写进需求，卡片里注明 |
| 已支持 | 有测试夹具或编译器源码证明当前可用 | 直接依赖，附证据行号 |

## 2. 需求条目

### R-01 包清单与安装面

- **问题**：包必须能被 `ct get` / `ct install` 识别，声明编译器下限与链接选项，且不把 libcurl 头带进 CT 单元。
- **验收判据**：
  - `bash tools/check.sh g0` 退出 0（2026-11-13 实测：`ok g0 ... version 0.1.0`）。
  - `ct.pkg` 含 `name=http`、`version=0.1.0`、`licence=MIT`、`ldflags=-lcurl`、`requires=ct >= 0.34.0`；删掉任一键后 `ct get` 非零退出并给出缺键文案。
  - 构建产物 `otool -L`（macOS）/`ldd`（Linux）只指向系统 libcurl，没有仓库内 curl 副本。
- **非目标**：不自带/vendored curl；不提交索引行（只出提案）；不做预发布/构建元数据。
- **CT 依赖/阻塞**：`.m.ct` + `.shim.h/.c` + `ldflags`/`requires`（`ct-lang/docs/package-guide.md:57-76`、`docs/bindings-and-patterns.md:80-180`）——已支持。

### R-02 请求对象的构造与释放

- **问题**：一个请求对象持有一个 easy handle；构造、配置、释放必须一一配对，不能靠调用方猜 `free` 时机。
- **验收判据**：
  - 夹具 `test/run.sh h1-t1`：`HttpRequest.new()` 返回非空；字段默认值逐项断言；`delete` 后 shim 计数显示 `curl_easy_cleanup` 恰好 1 次，ASAN/LSAN 0 泄漏。
  - 负例：同一指针二次 `delete` 不崩溃，且第二次不再触发 `drop`（发射代码把指针置 0）。
  - 负例：只 `new` 不 `delete` 的夹具在 LSAN 下必须报泄漏，证明检测器真的开着。
- **非目标**：不做引用计数、不做跨线程共享句柄、不暴露 `CURL *`。
- **CT 依赖/阻塞**：静态固有方法 `Type.new()`（`ct-lang/test/ast/229_inherent_impl/prog.ct:14,19`）、`impl Drop` + `use Drop`（`test/ast/63_own_return_field/prog.ct:9-15,38`）——已支持。

### R-03 URL 设置与所有权

- **问题**：URL 在配置与执行之间必须稳定；调用方缓冲区释放后不能变成悬空指针。
- **验收判据**：
  - 夹具把 URL 指向本地回环 `/a?b=1`；`url()` 后立即覆写并释放源缓冲区，`post()` 时服务端仍收到 `/a?b=1`（日志逐字节比对）。
  - 负例：空 URL 在发起连接前返回本地错误，服务端计数为 0。
  - 负例：超过文档上限的 URL 返回明确错误（或按已定口径截断并置错误位），不得静默改写成另一个可连接 URL。
- **非目标**：不做 URL 解析/拼接（`urlapi`）、不做 query 编码、不做 IDN。
- **CT 依赖/阻塞**：C 指针 + `<string.h>` 直用（`ct-lang/docs/package-guide.md` §1 允许单元直接 include C 头）——已支持。

### R-04 请求头列表

- **问题**：多个请求头要有序发送，重复名不能被吞；slist 生命周期跟随请求对象。
- **验收判据**：
  - 夹具回显请求头；`header("X-A","1")` + `header("X-A","2")` 后服务端按调用序收到两条 `X-A`。
  - `delete` 后 LSAN 0 泄漏，shim 计数显示 `curl_slist_free_all` 恰好 1 次。
  - 负例：超长 header（超过文档上限）返回明确错误，不得截断成另一条合法头。
- **非目标**：不提供响应头读取（读取面只列状态码/body/错误/字节数）；不做默认头或自动 Content-Type。
- **CT 依赖/阻塞**：`curl_slist_*` 经 shim 调用（`ct-lang/docs/bindings-and-patterns.md:80-180`）——已支持。
- **待确认**：是否在 `header()` 拒绝含 CR/LF 的名字或值（Q-10）。

### R-05 请求体

- **问题**：按显式长度发送原始字节，允许二进制与内嵌 NUL；缓冲区生命周期必须有确定语义。
- **验收判据**：
  - 夹具读原始 body；`body("a\0b", 3)` 服务端收到 3 字节且逐字节相等；`len=0` 收到 0 字节。
  - 负例：`len<0` 返回本地错误，服务端计数 0。
  - 生命周期：按 Q-02 定口径后补一条——配置后覆写/释放源缓冲区再 `post()`，要么发送深拷贝的原值，要么在 `body()` 当场报错；两种都不得读已释放内存（ASAN 必须红）。
- **非目标**：不做 form/multipart/mime、不做自动 Content-Type、不从文件读 body。
- **CT 依赖/阻塞**：C 指针 + 长度、`memcpy`——已支持；语义待确认（Q-02）。

### R-06 总超时与连接超时

- **问题**：同步请求必须有时间上限，避免无限等待。
- **验收判据**：
  - 夹具 `/delay?ms=2000`；`timeout(1)` 的 `post()` 在 1–3 秒内返回，`error_code == CURLE_OPERATION_TIMEDOUT(28)`、`is_ok()==0`。
  - `connect_timeout(n)` 的端到端判据需可控丢包环境；默认套件至少断言该值被写入 easy handle（shim 捕获参数），并在 README 标注未覆盖的端到端场景（Q-13）。
  - 负例：负数超时返回本地错误，服务端计数 0。
- **非目标**：不做重试、不做总耗时预算、不做超时继承。
- **CT 依赖/阻塞**：`curl_easy_setopt` 经 shim——已支持；`timeout(0)` 语义待确认（Q-07）。

### R-07 User-Agent

- **问题**：调用方可覆盖 UA；未设置时的默认值必须确定、可断言。
- **验收判据**：
  - 夹具回显 `User-Agent`；不调用 setter 时收到的值等于文档写死的默认值（Q-06 定后写死）。
  - 调用 `user_agent("x/1")` 时逐字节相等。
  - 负例：默认值断言在实现改成空 UA 时必须变红。
- **非目标**：不解析 UA、不按平台自动拼装。
- **CT 依赖/阻塞**：已支持；默认值待确认（Q-06）。

### R-08 重定向策略

- **问题**：默认不跟随重定向；跟随时行为必须可解释。
- **验收判据**：
  - 夹具 `/302 -> /ok`；默认 `post()` 后服务端访问计数 1，`status_code==302`、`is_ok()==0`。
  - `follow_redirects(1)` 后计数 2，最终 `status_code==200`、body 为 `ok`；第二跳方法沿用 libcurl 默认（302 后 POST 变 GET），服务端记录方法并断言。
  - 负例：把默认改成跟随会让第一条断言变红。
- **非目标**：不解析 `Location`、不做跳转白名单、不设自定义跳转上限（沿用 libcurl 默认）。
- **CT 依赖/阻塞**：已支持。

### R-09 TLS 校验策略

- **问题**：默认校验证书与主机名；关闭校验必须显式。
- **验收判据**：
  - 本地 TLS 夹具用自签证书：默认 `post()` 返回传输错误（`CURLE_PEER_FAILED_VERIFICATION(60)` 或 `CURLE_SSL_CONNECT_ERROR(35)`，文档二选一并固定），`is_ok()==0`。
  - `insecure(1)` 后同一夹具返回 200。
  - 主机名不匹配（证书 SAN 不含 `127.0.0.1`）默认失败，`insecure(1)` 后成功。
  - 负例：默认值断言在实现改成不校验时必须变红。
- **非目标**：不提供自定义 CA、客户端证书、TLS 版本选择（提案未列）。
- **CT 依赖/阻塞**：已支持。

### R-10 `post()` 同步执行

- **问题**：一次调用完成“设置请求—执行—读结果”；HTTP 错误与传输错误必须分开。
- **验收判据**：
  - 夹具 200/404/500：`status_code`、`body`/`body_len`、`total_bytes` 逐项断言；200 时 `is_ok()==1`，404/500 时 `is_ok()==0` 且 `error_code==0`。
  - 二进制 body（含 NUL）用 `body_len` 比较，不用 `strlen`。
  - 3xx 在默认不跟随下 `is_ok()==0`（Q-11 定稿后写死）。
  - 负例：把 500 当成功会让断言变红。
- **非目标**：不提供 GET/PUT/DELETE 或自定义 method；不做并发请求。
- **CT 依赖/阻塞**：write callback + `getinfo` 经 shim——已支持；耗时字段是否暴露待确认（Q-08）。

### R-11 `HttpResponse` 读取面

- **问题**：结果对象只读、字段稳定，调用方能判断 ok / HTTP 错误 / 传输错误 / 取消。
- **验收判据**：
  - 四种状态各一条夹具：ok、404、传输错误、取消；断言 `status_code`、`body`、`body_len`、`error_code`、`error_msg`、`total_bytes`、`is_ok()`、`is_cancelled()`、`error_detail()`。
  - `grep` 公开头无 setter；`nm` 生成对象无写入口符号。
  - 负例：ok 状态却 `is_ok()==0`、或 404 却 `error_code!=0` 必须变红。
- **非目标**：不读响应头、不提供 cookies、不做 body 增量读取（`post_stream` 另条）。
- **CT 依赖/阻塞**：已支持；释放责任待确认（Q-03）。

### R-12 `post_stream(cb, ud)`

- **问题**：大响应不整包落内存，按片交给回调；回调可中止。
- **验收判据**：
  - 夹具 1 MB 分块发送；回调收到 ≥2 片，拼接后逐字节相等，顺序与片边界不丢字节。
  - 回调第 1 片返回非 0：`post_stream` 返回 `CURLE_WRITE_ERROR(23)`，`is_cancelled()==0`，LSAN 0 泄漏。
  - 与 `send()` / `send_stream()` 的响应模式互斥：同一请求对象混用两种入口时，第二次
    调用在发起连接前返回本地状态错误（`-1007`，Q-05 已定），服务端计数 0；请求体
    `body()` 与流式响应可共存。
  - 负例：回调只收到 1 片（整包缓冲）在“≥2 片”断言下变红。
- **非目标**：不做异步/多路复用、不做断点续传、不做回调内重入。
- **CT 依赖/阻塞**：`typedef int (*HttpCB)(char*,int,void*)` + 字段调用（`ct-lang/test/ast/27_fnptr_forms/prog.ct:9-13,39-40`）——已支持。

### R-13 `cancel()` / `is_cancelled()`

- **问题**：慢请求要能主动中止，且与“回调写失败”区分。
- **验收判据**：
  - 慢夹具（至少 2 秒）中触发取消，`post()` 在 3 秒内返回，`error_code==CURLE_ABORTED_BY_CALLBACK(42)`，`is_cancelled()==1`、`is_ok()==0`。
  - 未取消路径 `is_cancelled()==0`。
  - 负例：把回调中止也置 `is_cancelled()` 会让 R-12 的断言变红。
- **非目标**：不支持跨线程取消、异步取消、信号触发；同步 API 的取消点由 Q-04 定。
- **CT 依赖/阻塞**：进度回调经 shim——已支持；触发点待确认（Q-04）。

### R-14 错误映射

- **问题**：传输错误、HTTP 错误、本地参数错误三类不能混；未知码不得吞掉。
- **验收判据**：
  - 表驱动：DNS 失败 6、连接拒绝 7、超时 28、TLS 校验 60、回调中止 42、写错误 23；`error_code` 等于上表。
  - `error_msg` 非空且含主机与阶段；`error_detail()` 对同一输入可复现。
  - HTTP 4xx/5xx：`error_code==0`、`is_ok()==0`。
  - 未知 `CURLcode`：`error_code` 保留原值，不归零。
  - 负例：500 被写成传输错误、或未知码被归零，必须变红。
- **非目标**：不做重试分类、不做本地化文案、不把 libcurl 的 enum 全量导出。
- **CT 依赖/阻塞**：已支持；本地错误码空间待确认（Q-05）。

### R-15 C shim 单一边界

- **问题**：libcurl 只出现在 shim 与 `.m.ct`；CT 单元不 `#include <curl/...>`，公开头不暴露 `CURL *`。
- **验收判据**：
  - `grep -rn '#include <curl/' src/*.ct src/*.h.ct` 0 命中。
  - `grep -rn 'curl_easy_\|curl_slist_' src/*.ct src/*.h.ct` 0 命中（只允许出现在 `.m.ct` recipe 与 `.shim.c`）。
  - 对 CT 单元生成的对象跑 `nm -u`，没有 `curl_easy_*`/`curl_slist_*` 未定义符号（符号由 shim 收口）。
  - 负例：删掉 `ldflags = -lcurl` 后链接必须失败并报 curl 符号未定义。
- **非目标**：不接其他 C 库；不做动态加载/运行时探测。
- **CT 依赖/阻塞**：`.m.ct` + shim 头自动 include（`ct-lang/docs/bindings-and-patterns.md:160-172`）——已支持。

### R-16 内存与错误路径

- **问题**：成功、HTTP 错误、传输错误、回调中止、取消、参数错误六条路径都不能泄漏或重复释放。
- **验收判据**：
  - 六条路径各一条夹具，在 ASAN/LSAN 下退出 0，输出无 `LeakSanitizer`/`AddressSanitizer` 报告。
  - 负例：故意去掉 `delete` 的夹具必须报泄漏（检测器自证）。
  - 负例：同一响应/请求二次 `delete` 不崩溃（H6-T2 口径）。
- **非目标**：不引入自定义分配器、不承诺 RSS 上限。
- **CT 依赖/阻塞**：`delete` + 显式 `drop`（`ct-lang/test/archive/cases/22_drop.ct:5-23`、`test/ast/63_own_return_field/prog.ct:9-15,38`）——已支持。

### R-17 平台与系统 libcurl

- **问题**：同一份源码在 macOS 与 Linux 的系统 libcurl 上都能构建运行。
- **验收判据**：
  - 两台机器（或 CI 矩阵）跑完整 `test/run.sh all`，原始输出均 0 失败。
  - `curl_version()` 打印版本的首个 token 与系统 `curl --version` 首个 token 相同（H2-T1 口径）。
  - `otool -L` / `ldd` 不含仓库内 curl 路径。
- **非目标**：不支持 Windows、不静态链接 curl、不锁 TLS 后端。
- **CT 依赖/阻塞**：环境能力；当前环境未装 `ct`，**软阻塞**，需 CI/双机证明。

### R-18 门禁与文档可执行

- **问题**：未实现的判据不能伪装通过；README 示例必须与真实语法一致。
- **验收判据**：
  - `bash tools/check.sh g0` 退出 0（已实测）；H1 收口时 `g3` 退出 0（构建 + H1 夹具全绿、零警告）。
  - `g1`/`g2`/`g4`–`g9` 未接线时退出 2，不得返回 0。
  - README 代码块来自 `docs/snippets/`，由 `g4` 编译/运行通过；含 `req.url(...).header(...)` 的片段必须被门禁拒绝（预期红）。
- **非目标**：不新增门禁编号（`AGENTS.md` §6.4）；不为过门禁放宽判据。
- **CT 依赖/阻塞**：已支持（脚本与文档门禁既有）。

### R-19 发布材料与安装验证

- **问题**：包要能被安装，版本落点一致，索引只提案不擅自发布。
- **验收判据**：
  - `ct.pkg#version` 与 README「版本」行相等（g0 已校验 0.1.0）；收口时同名 annotated tag `v0.1.0` 指向收口提交。
  - 在干净临时目录执行 `ct get http@^0.1.0 && ct install`，示例程序能请求本地夹具并得到 200。
  - 负例：把 `requires` 抬到高于本机 `ct` 的版本后，`ct get` 必须失败并打印 `requires` 文案。
- **非目标**：不自动推送分支/tag、不提交索引行（只出提案）、不做多包依赖解析。
- **CT 依赖/阻塞**：`ct.pkg` 语义（`ct-lang/docs/package-guide.md:57-76`）——已支持；tag 推送需用户单独授权（AGENTS §6.5）。

## 3. 原提案中不成立、必须先改的条目

> 本仓没有 `docs/analysis.md`；下表只列能用源码、夹具或文档内部矛盾证实的项。

| ID | 原写法（位置） | 判定与证据 | 改后口径 | 状态 |
| --- | --- | --- | --- | --- |
| M-01 | 配置写成 `req.url(...).header(...)` 单表达式链（README 目标形态的历史写法；design D-2） | 不成立：编译器对临时值的方法调用只允许出现在语句/赋值开头（`ct-lang/selfhost/ct-ast/backend/emit/Emitter.h.ct:2174`；负例门禁 `selfhost/check-ast.sh:4320-4323`） | 一条语句一个方法；配置方法仍返回 `self`，但不得串成单表达式 | 已定 |
| M-02 | 用 `HttpRequest::new()` 构造（C++ 习惯） | 不成立：`::` 走命名空间解析，报 `no function of that name in that namespace`（`selfhost/ct-ast/frontend/parse/Expr.h.ct:5553`）；可用写法见 `test/ast/229_inherent_impl/prog.ct:14,19` | 统一写 `HttpRequest.new()` | 已定 |
| M-03 | `src/` 下放单元并计划加 `namespace http;`（design D-1） | 不成立：包仓库根即包根（`docs/package-guide.md:16`）；声明 `namespace` 的单元必须坐在同名目录（`:35-52`，D13） | H1 单元不加 namespace；若要 namespace，迁到 `http/` 并在 README 固定；发布前二选一收口 | 仍需提案方确认 |
| M-04 | 「七个配置方法」（design §3、H1 §0） | 计数错：任务列了 8 个——`url` / `header` / `body` / `timeout` / `connect_timeout` / `user_agent` / `follow_redirects` / `insecure`（`docs/phases/H1.md:23-26`） | 全文统一为 8 个 | 已定 |
| M-05 | `body()` 只存指针 + 长度，`post()` 稍后执行（`docs/phases/H1.md:25`） | 判据不可判定：调用方缓冲区可先释放，行为未定义 | 二选一：`body()` 深拷贝；或契约写明缓冲区至少活到 `post()` 返回，并用 ASAN 负例证明 | 仍需提案方确认 |
| M-06 | `post()` 返回 `HttpResponse *`，未写谁释放、是否 `Drop`（`docs/design.md:12-13,98`；`docs/phases/H1.md:27`） | 判据缺口：R-11/R-16 的泄漏判据无法落地 | 定义 owner：`HttpResponse` 也 `use Drop` + `delete res`，或明确由请求对象持有到下一次请求；二者都要有二次释放负例 | 仍需提案方确认 |
| M-07 | `cancel()` 由「进度回调读位」触发，但公开回调只有 `post_stream` 的写回调（`docs/phases/H2-H6.md:32-34`） | 不可达：同步 API 禁止跨线程，公开面没有进度回调，调用方没有执行 `cancel()` 的时机 | 三选一：允许在 `HttpCB` 内调 `cancel()`；新增进度回调；v1 删除 `cancel()` 只留回调中止 | 仍需提案方确认 |
| M-08 | H2-T4 收「耗时」，但 `HttpResponse` 读取面无耗时字段（`docs/phases/H2-H6.md:16` vs `docs/phases/H1.md:27`） | 字段缺口：验收项无法断言 | 要么加 `elapsed_ms`（定义单位与起点），要么从 H2-T4 删掉耗时 | 仍需提案方确认 |
| M-09 | H3-T2 说「返回错误码」，但传输错误用 `CURLcode`，本地参数错误没有码空间（`docs/phases/H2-H6.md:24`） | 码值会撞车：0/正数已被 `CURLcode` 占用 | 定义本地错误空间（建议负数），公开常量并逐条文档化；`CURLcode` 保持原值 | 仍需提案方确认 |

## 4. 待确认

> 只列会改变验收判据或实现边界的问题；每条写「不确认会卡住什么」。

| ID | 问题 | 不确认会卡住什么 |
| --- | --- | --- |
| Q-01 | 本仓没有 `docs/proposal.md` 与 `docs/analysis.md`；本文件以 `design.md` 替代 | 无法逐字对照原提案；评审基线只能取 `design.md` 与任务写。若原件在别处，给路径后按原文重对齐（不改原文） |
| Q-02 | `body()` 是深拷贝还是借用调用方缓冲区？借用时有效期到哪一步？ | 卡 R-05 的生命周期判据、H1-T3 实现与 H5 的 ASAN 负例 |
| Q-03 | `HttpResponse *` 由谁释放？是否有 `Drop`？body 缓冲区随谁释放？ | 卡 R-11 读取面、R-16 泄漏判据、H6-T2 二次释放口径 |
| Q-04 | `cancel()` 在哪个回调里调？还是 v1 不提供？ | 卡 R-13 全部判据与 H4 的任务拆分 |
| Q-05 | 本地参数错误用什么码空间？是否允许与 `CURLcode` 混用？ | 卡 R-12 的 `body`/`post_stream` 互斥、R-14 的表驱动、H3-T2 |
| Q-06 | 未设 `user_agent` 时的默认值：libcurl 默认还是包标识？ | 卡 R-07 的默认值断言与 R-10 的请求回显断言 |
| Q-07 | `timeout(0)` / `connect_timeout(0)` 是「不限」还是「立即超时」？ | 卡 R-06 的 0 值负例与 README 语义 |
| Q-08 | 是否在 `HttpResponse` 暴露耗时字段？单位与起点是什么？ | 卡 R-10 与 H2-T4 的「耗时」验收 |
| Q-09 | 目录与命名空间二选一：`src/` 不加 namespace，还是迁到 `http/`？ | 卡 R-01/R-03 的落盘路径、发布检查与 H1-T2 开工 |
| Q-10 | `header()` 是否拒绝含 CR/LF 的名字或值？ | 卡 R-04 是否加一条安全负例；不加则需在文档写明调用方责任 |
| Q-11 | 默认不跟随重定向时，3xx 是否一律 `is_ok()==0`？ | 卡 R-10 的 3xx 判据与 R-11 的状态分类 |
| Q-12 | 线程安全口径：是否明确「一个请求对象只能单线程使用」？ | 卡 R-13/R-16 的非目标边界与 README 限制段 |
| Q-13 | `connect_timeout` 的端到端验收用可控网络环境，还是降级为 shim 参数捕获？ | 卡 R-06 的可执行判据；降级需在报告里标「未端到端覆盖」 |

## 5. CT 能力依赖汇总

| 能力 | 证据 | 级别 | 影响条目 |
| --- | --- | --- | --- |
| 静态固有方法 `Type.make()` | `ct-lang/test/ast/229_inherent_impl/prog.ct:14,19`；`docs/roadmap.md:49`（Phase 34，v0.34.0） | 已支持 | R-02 |
| `impl Drop` + `} use Drop;` + `delete` | `ct-lang/test/ast/63_own_return_field/prog.ct:9-15,38`；`test/archive/cases/22_drop.ct:5-23` | 已支持 | R-02、R-16 |
| 函数指针 typedef + 结构体字段调用 | `ct-lang/test/ast/27_fnptr_forms/prog.ct:9-13,39-40` | 已支持 | R-12 |
| `.m.ct` + `.shim.h/.c` + `ldflags`/`requires` | `ct-lang/docs/package-guide.md:57-76`；`docs/bindings-and-patterns.md:80-180` | 已支持 | R-01、R-15 |
| 单表达式链式方法调用 | `selfhost/ct-ast/backend/emit/Emitter.h.ct:2174`；`selfhost/check-ast.sh:4320-4323`（负例） | 软阻塞：一条语句一个方法 | R-03～R-09、R-12 |
| `Type::new()` 命名空间限定构造 | `selfhost/ct-ast/frontend/parse/Expr.h.ct:5553`（错误文案） | 软阻塞：改用 `Type.new()` | R-02 |
| `namespace` 单元目录（D13） | `ct-lang/docs/package-guide.md:35-52` | 软阻塞：不加 namespace 或迁目录 | R-01、Q-09 |
| 系统 libcurl（头与库） | 本机未装 `ct`，未做构建验证 | 软阻塞：需 CI/双机证明 | R-17、R-19 |

**没有硬阻塞**：以上需求都能用现有 CT 能力或等价写法落地，不需要新增语言关键字或语法。

## 6. 不在本文件范围

- 不修改 `docs/design.md` 与任何原文；不建 phase 卡、不排 roadmap、不写实现。
- 不 clone 或改动其他仓库；`ct-lang` 只做只读取证。
- 不裁决 Q-01～Q-13；除 Q-01 外，每条在立项评审时定稿，定稿前对应条目的判据保持「待确认」。
- 版本、tag 与推送按 `AGENTS.md` §6.5/§6.7，需用户显式指令；本文件不触发。
