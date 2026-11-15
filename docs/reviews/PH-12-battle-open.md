# PH-12 开卡 battle · 主张方：支持开卡（S-01 响应头读取 + S-02 `method()`）

- 日期：2026-11-15
- 性质：两席 battle 的主张方书面意见；另一席为反对方，本文不对其结论作预设。
- 范围：只读核对 + 论证 + 判据草案。**不修改实现、不改 `ct.pkg`、不提交**（另一席同仓在跑）。
- 基线：`a00caa8`（v0.2.0 收口）；工作树已有他人未跟踪的 `docs/review/*.md`，本文不触碰。
- 结论所依据的事实均给文件与行号；行号对应上述基线。

## 0. 结论

1. 应当开 PH-12，把 S-01（响应头读取）与 S-02（`method()`）作为一个相交付。
2. 两者的阻碍不是技术，而是 `docs/requirements.md` 里三条**0.1.0 范围裁剪**性质的非目标
   （R-04:73、R-10:139、R-11:149）。处置方式是**改写口径**，不是限定形态后假装不冲突。
3. 版本按 V-1 + `deviation: V-001`（`d = 9`，AGENTS:288）换算为 **`0.3.0`**，
   卡片字段 `- version-minor：12`；`0.12.0` 与 `0.2.1` 都违规（§4）。

## 1. 为什么现在开 PH-12 是对的

### 1.1 用户价值：这两项不是锦上添花，是 HTTP 客户端的下限

- **S-01 响应头**。没有它，调用方拿不到 `Content-Type`（决定 charset 与 SSE 判定）、
  `X-RateLimit-*` / `Retry-After`（限流退避）、`Location`（默认不跟随时的跳转目标）、
  `ETag` / `Last-Modified`（条件请求的前置信息）。本仓自己的规划文档已记录动机：
  「下游 `ct-llm-sse` 的 content-type 判定受益」（`docs/planning/ct-http-0.1.0-features.md:66`）。
  该下游未在本轮复核，作为规划期记录引用，不当作实测事实。
- **S-02 `method()`**。当前公开面只有 `post()`（`http/http.m.ct:15-17`），执行路径按
  「有 body 走 `POSTFIELDS`，无 body 走 `CURLOPT_HTTPGET`」二选一（`http/http.shim.c:987-994`）。
  也就是说：**调用方无法发出 GET / HEAD / PUT / DELETE / PATCH**，只能发 POST。
  方法不是命名问题，它决定服务端路由、缓存可行性、幂等语义、302 后的方法改写
  （R-08 已冻结「302 后 POST 变 GET」，`docs/requirements.md:112`）以及中间件处理。
  HEAD 缺失还意味着拿不到「只要元数据不要 body」的能力。

### 1.2 为什么不值得等

- **无 CT 依赖**。两项都用既有能力：`CURLOPT_HEADERFUNCTION` 已在本包使用
  （`http/http.shim.c:963`），`CURLOPT_CUSTOMREQUEST` / `CURLOPT_NOBODY` 与现有
  `curl_easy_setopt` 同一路径；映射面只是给已有 trait 加方法。不需要编译器新特性。
- **现在是成本最低的断点**。0.2.0 刚发布、十道门禁全绿、`issues/` 只剩一条环境问题
  HTTP-3（`issues/HTTP-3.md`）。此刻补口只动一次 ABI（两张结构体各追加字段），
  一次收口、一次版本移动；拖到有下游之后再做，ABI 与文档要再动一轮，且中间所有
  消费者只能拿 POST 凑 GET，或绕过 R-15 的单一边界直接写 C。
- **等待不会减少工作量**。shim 的头收集、末跳语义、上限、方法分支该写的都要写；
  晚做的唯一效果是让 0.2.0 的消费方先产生绕过写法。

### 1.3 与非目标的冲突：改写口径，理由如下

冲突位置（逐行核对）：

| 条目 | 现文 | 与哪项冲突 |
| --- | --- | --- |
| R-04:73 | 「不提供响应头读取（读取面只列状态码/body/错误/字节数）」 | S-01；且该句本属响应面，误挂在请求头条目 |
| R-10:139 | 「不提供 GET/PUT/DELETE 或自定义 method」 | S-02 |
| R-11:149 | 「不读响应头、不提供 cookies、不做 body 增量读取」 | S-01 |
| R-11:146 | 读取面断言只列 9 项，未含头 | S-01 扩读取面 |

为什么不是「限定形态」：

- S-02 无论做成 `.method("GET")` 还是专用的 `.get()`，都落在「提供 GET」这一被禁止的
  能力上；限定形态只能改小语法，改不掉冲突，R-10 仍必须改。
- S-01 无论只给 `header(name)` 还是再加枚举，都落在「读响应头」上；R-11 仍必须改。
  限制形态只应写进**语义边界**（同名多头、末跳、上限），不能用来回避改非目标。

为什么这不是「推翻自己」：

- 这三条非目标写在 2026-11-13 的 0.1.0 需求梳理稿里，同一份文件把 S-01/S-02 登记为
  「待裁定项，不替用户选择」（`docs/planning/ct-http-0.1.0-features.md:58-76`）；
  roadmap 的接口冻结条款第 10 条更明确写了「`method()` 属待裁定项 S-02，**纳入时新增
  `.method(...)` 入口，不改已冻结的方法名**」（`docs/planning/ct-http-0.1.0-roadmap.md:40`）。
  也就是说，重开路径是当时就写下的，PH-12 是在执行它，不是推翻它。
- 真正「推翻自己」的做法是：实现照做，文档仍写「不支持」，让判据与制品对不上。
  本文主张的正是相反方向——同一相内改口径、补判据、补负例，并在 requirements 末尾
  用「修订记录」保留旧文原文，不静默删除。
- 0.1.0/0.2.0 的对外契约不被追溯改写：新增能力进 `0.3.0`，属 MINOR 增量。

## 2. 影响面

### 2.1 落点：数据与 libcurl 在 shim，语法只在入口

| 层 | 文件 | 改动 |
| --- | --- | --- |
| C 边界 | `http/http.shim.h` | 两张结构体**尾部追加**字段；新增 3 个头读取函数、1 个 `method` 配置函数；新增头上限常量与 1 个本地错误码 |
| C 实现 | `http/http.shim.c` | `header_cb` 按行缓存（`http.shim.c:825-871` 现仅取 status/Location）；每 hop 复位，末跳落库；超限中止；按 method 选择 `HTTPGET` / `NOBODY` / `CUSTOMREQUEST` |
| 映射 | `http/http.m.ct` | `RequestBuilder` 加 `method()`；`ResponseView` 加 `header()` / `header_count()` / `header_at(i)`。只加映射行，不写逻辑 |
| 常量 | `http/Types.h.ct` | 若 CT 侧需可见的上限/错误码常量，按 roadmap 第 12 条「数值单一来源在 `.shim.h`」补，并由 g3 的 C 断言对齐 |

不建议把读取逻辑写进 `.m.ct`：该单元的职责是映射，既有 12 个方法全是单行展开
（`http/http.m.ct:52-85`），破坏这条会让 R-15 的 grep 判据失去意义。

### 2.2 ABI：触及，但是追加式，且冻结断言不变

- `struct HttpRequest` / `struct HttpResponse` 的字段序被 `test/fixtures/phase1_abi.c:7-16`
  当作公开 ABI 断言（`url` 与 `status` 在偏移 0，`body` 在 `status` 之后）。本相只允许
  **尾部追加**（如 `method`、头缓冲指针/计数），不移除、不重排、不改既有字段类型。
  该夹具不修改，且必须继续通过。
- 包是源码分发（`ct build -I vendor/http`），不存在预编译二进制的跨版本 ABI 消费者；
  因此 ABI 风险被限制在本包自己的 C 夹具内，可通过「旧断言不改 + 新增字段只在尾部」闭合。
- trait 面：`RequestBuilder` / `ResponseView` 是 `pub trait`，给它们加方法对调用方是
  源码兼容；对假设的第三方实现者不兼容。解法是文档显式声明「这两个 trait 是包的映射
  契约，不是扩展点」，与 R-15 的「只 import 映射单元」约定一致。若实施时发现多 trait
  映射到同一宿主类型有歧义，退回「新增独立 trait + 独立 `map trait`」，不影响本结论。

### 2.3 必须一次定死的语义（否则判据不可二值）

- 只暴露**末跳**响应头：重定向的中间响应头不落库；每个 status line 复位缓冲
  （现有手动逐跳结构 `http.shim.c:965-1019` 天然支持）。
- 同名头按到达序保留；`header(name)` 返回第一条值，大小写不敏感；不合并成逗号串。
- 值去掉行尾 CRLF，保留值内空格；空值返回空串而不是 NULL；缺失返回 NULL。
- 上限：头数 ≤ 64、总字节 ≤ 8 KiB；超限在 body 交付前中止，返回本地错误
  （建议 `-1010`，与现有 `-1001…-1009` 连续，`http/http.shim.h:65-74`），
  `is_ok()==0`，`error_detail` 带 `stage=header`。
- 流式不受影响：`send_stream` 下 body 仍为 NULL/0，头照常可读。
- `method`：默认 POST（与现状一致）；`post()` 仍只是工厂，不改名；`method()` 是覆盖，
  后调用覆盖先调用；只接受 1–16 个字母，统一大写，其余本地拒绝（`-1001`）且不建连。
- GET/HEAD + 非空 body：本地拒绝（建议在 send 时判，`-1001`），不建连、服务端计数 0。

### 2.4 门禁要加的负例

| 门禁 | 新增内容 |
| --- | --- |
| g2 | `docs/acceptance-map.md` 为 S-01/S-02 各加 ACC 行，锚到新夹具 |
| g3 | 新夹具入 `tools/check.sh` 的 g3 清单；`test/run.sh` 增 `p12-*` 分支与新服务端 |
| g3-negative | 沿用现有两条自证（错 expected 必须红、未接线必须 rc=2），不放宽 |
| g4 | `docs/snippets/headers.ct`、`method.ct` 正例；`docs/snippets/headers-set.ct` 负例（写响应头必须编译失败） |
| g5 | 每个任务的红证文件；门禁自证仍 9 条 |
| g7 | 响应头缓冲的 ASAN/LSAN 正例；二次 `free()` 负例 |
| g8 | 版本移动与提交范围；不改阈值、不删负例 |

新增负例（必须真的变红）：缺失头返回非 NULL、同名头只留一条、中间跳头泄漏到末跳、
第 65 个头未被拒绝、`method("GE T")` 仍发出请求、GET 带 body 仍建连。

### 2.5 明确不碰

`ct.pkg` 的 `requires` 与 `ldflags`；`curl_*` 进 CT 单元；重试/代理/连接池；
响应头解析成结构化字段（`Content-Type` 参数、cookie 解析）；trailers；`HTTP_ERR_*` 枚举（S-05）。

## 3. PH-12 判据草案要点（二值可核）

| ID | 判据（断言必须能被负例打红） | 命令锚点 |
| --- | --- | --- |
| E-P12-1 | 末跳头的读取面存在且只读：`header_count()` 等于服务端发出的头数，`header("Content-Type")` 逐字节等于 `application/json; charset=utf-8`，`header("X-Missing")==NULL`；`nm` 无写入口 | `bash test/run.sh p12-headers` + `bash test/run.sh p12-nm` |
| E-P12-2 | 同名头按到达序保留两条，`header("x-trace")` 返回第一条（大小写不敏感），`header_at(0/1)` 顺序正确 | `bash test/run.sh p12-duplicate` |
| E-P12-3 | 值去 CRLF、保留内部空格、空值返回空串；越界/负索引返回 NULL | `bash test/run.sh p12-values` |
| E-P12-4 | 65 个头或 >8 KiB：body 前中止，`error()==-1010`、`is_ok()==0`、`error_detail` 含 `stage=header`，服务端 body 交付为 0 | `bash test/run.sh p12-cap` |
| E-P12-5 | 跟随重定向时只有末跳头可见（`X-Final` 在、`X-Hop` 不在），状态 200；流式下头仍可读且 `body()==NULL` | `bash test/run.sh p12-hop` 与 `bash test/run.sh p12-stream-headers` |
| E-P12-6 | 默认方法为 POST；`method("GET")`/`method("DELETE")`/`method("put")` 依次被服务端记录为 GET/DELETE/PUT；未调用 `method` 的既有夹具输出不变 | `bash test/run.sh p12-method` 与 `bash test/run.sh p4-status` |
| E-P12-7 | 非法方法（空串、含空格、含 CRLF、超 16 字符）返回 `-1001`，服务端请求计数 0；GET/HEAD 带 body 同样不建连 | `bash test/run.sh p12-method-bad` |
| E-P12-8 | 302 后：默认 POST 仍变 GET（R-08 既有断言不变）；GET 保持 GET；DELETE 的实际行为按 libcurl 记录并写进文档 | `bash test/run.sh p9-hops10` 与 `bash test/run.sh p12-method-redirect` |
| E-P12-9 | 生命周期：头存储随 `free()` 释放，ASAN/LSAN 0 报告，二次 `free()` 不崩 | `bash test/run.sh p12-lifetime` |
| E-P12-10 | 文档与版本：R-04/R-10/R-11 口径已改且带修订记录；README API/限制/版本同步；`applies-to` 全为 `0.3.0`；`g0`…`g9` 全绿 | `bash tools/check.sh g4` 与 `for g in g0 … g9` |

任务切分建议（每项一个提交、可单独回滚，均覆盖上表判据）：T0 卡片与基线；T1 shim 头存储
与上限；T2 入口头读取三方法；T3 末跳/流式语义；T4 method 字段与校验；T5 重定向方法语义
与 R-08 回归；T6 requirements/README/snippets/acceptance-map；T7 收口与版本、tag。

## 4. 版本号：按 V-1 与 `d = 9` 实际换算

核对依据：

- V-1：`PH-<n>` 交付时 `MINOR == n`，**登记了偏离的按 `d` 换算**，且相对上一相恰好 +1、
  不许多跳（`AGENTS.md:115`）。
- `deviation: V-001`：`偏移: d = 9`，收敛方式为「永久偏移」，PH-11 得 `0.2.0`
  （`AGENTS.md:284-291`；`CHANGELOG.md:4,15`）。

换算：`MINOR = n - d = 12 - 9 = 3` → **`0.3.0`**，PATCH 归零。与 0.2.0 恰好 +1，不跳号。

机械校验的两端：

- 卡片字段必须是 `- version-minor：12`（工具按字面比对 `PH-<n>`，`tools/check.sh:86-87`），
  不是 `3`。
- `g4-version` 反向算 `MINOR + d = 3 + 9 = 12`，要求存在 `docs/phases/PH-12/card.md`
  （`tools/gates/g4.sh:316-326`）。所以 T0 必须先落卡，版本才能在收口提交里移动。

反例（不采用）：`0.12.0` 是从 0.2.0 跳 10 个 MINOR，违反 V-1，且 g4 会要求 PH-21 的卡；
`0.2.1` 把新功能塞进 PATCH，违反 V-2（`AGENTS.md:116`）。若相内产生 fix，才按 V-3 逐次 +PATCH。

收口按 V-7：annotated `v0.3.0` 指向收口提交，四项校验后推 tag；分支推送仍需用户显式指令。

## 5. 正面回应最强反驳

**反驳 A：「非目标是产品决策，开卡等于推翻自己。」**
非目标的三处原文是 0.1.0 的范围裁剪，同一批文档同时写了重开路径（features §3「不替用户
选择」、roadmap 接口冻结第 10 条「纳入时新增 `.method()`」）。产品决策若是「永不做」，
不会在 roadmap 里预留纳入方式。PH-12 的处置是「改口径 + 补判据 + 保留旧文原文」，
这是按当时写下的程序办事；相反，边做边留着「不支持」的文档才是自相矛盾。

**反驳 B：「S-02 只是命名便利，收益不够一个 phase。」**
`HttpEntry` 只有 `post()`，且 shim 用「有无 body」决定 POST/GET 分支（`http.m.ct:15-17`、
`http.shim.c:987-994`）。这不是「换个名字」，而是**没有任何入口能发出 GET**。GET 与 POST 在
缓存、幂等、302 改写、服务端路由上语义不同，POST 冒充 GET 会改变线上行为。收益是能力，
不是命名；成本却很小（一个字段 + 三个 setopt 分支 + 校验），单独立相反而要重复冻结与收口。

**反驳 C：「两个 feature 塞一个相，太大。」**
两项共用同一批夹具与同一个响应对象，改动是两张结构体各追加字段、5 个 shim 入口、
4 个映射方法。拆成两相会有两次 ABI 变更、两次版本移动、两次收口，而每相的任务量都不足
以支撑独立冻结的价值。合并后仍满足「每任务一提交、可单独回滚」。

**反驳 D：「S-01 会让读取面无限膨胀，应该先只做 GET。」**
所以判据里写死了边界：只读末跳、同名按序、不做 cookie/Content-Type 参数解析、不暴露
原始头块、不读 trailers、上限固定。读取面净增 3 个方法 + 1 个错误码 + 2 个常量，是有限集。
只做 GET 而不做头，等于下游拿不到 `Content-Type`，GET 的一半价值仍然缺位。

**反驳 E：「改非目标就是放宽门禁/降判据。」**
没有删规则、没有降阈值：g2/g3/g4/g5 都是**新增**条目与负例；判据表里每条都带命令与负例，
且既有判据（如 R-08 的 302 变 GET、`p9-hops10`）保持不变。版本号也不是凑绿，而是 V-1 的
机械换算结果。

## 6. 开卡前需要用户签字的三件事

1. 改写 `docs/requirements.md` 的 R-04:73、R-10:139、R-11:149 与 R-11:146，并在文末
   「修订记录」保留旧文原文与日期。
2. 新增公开面：`method()`、`header()`/`header_count()`/`header_at()`、本地码 `-1010`、
   两个上限常量；ABI 仅尾部追加。
3. 版本 `0.3.0`（PH-12，`d = 9`），收口按 V-7 打 `v0.3.0`。

## 7. 证据索引

- `docs/requirements.md:73,112,139,146,149`
- `docs/planning/ct-http-0.1.0-features.md:58-76,134-135`
- `docs/planning/ct-http-0.1.0-roadmap.md:21,40,57,207-217`
- `http/http.m.ct:15-17,20-34,37-50,52-85`
- `http/http.shim.h:32-59,65-74,91-105`
- `http/http.shim.c:825-871,873-1020`
- `test/fixtures/phase1_abi.c:7-16`
- `tools/check.sh:86-87`；`tools/gates/g4.sh:316-326`
- `AGENTS.md:115,116,121,268,284-291`；`CHANGELOG.md:4,15`
- `issues/HTTP-3.md`（唯一 open，环境缺口）
