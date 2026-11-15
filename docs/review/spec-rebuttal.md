> 来源说明（2026-11-15 入库）：本文为原始草稿（655 行），正文未改。副本按小节边界拆为
> `docs/reviews/spec-review/spec-rebuttal.md`（前段）、`spec-rebuttal-part2.md`、`spec-rebuttal-part3.md`；
> 副本只加了归档与接续说明，去掉这些说明后与原件正文一致。

# ct-http 包规范草案 · 反证评审

- 日期：2026-11-14
- 评审对象：外部撰写的 `ct-http` 包规范草案（下称「草案」），按其 10 条要点编号引用为 §1–§10。
- 评审基线：`docs/requirements.md`（R-01～R-19、M-01～M-09、Q-01～Q-13）、`docs/design.md`（D-1～D-6）、
  `docs/phases/H1.md`、`docs/phases/H2-H6.md`、`ct.pkg`、`README.md`；跨包契约取
  `ct-sse/docs/requirements.md` 与 `ct-llm-sse/docs/requirements.md`；语言能力取 `ct-lang` 源码与夹具。
- 草案文本不在本仓，§n 与要点的对应：§1 隐藏 libcurl/API 面，§2 字符串链起手，§3 配置方法与默认值，
  §4 `cancel()`/atomic/锁，§5 `HttpResponse` 读取面，§6 生命周期，§7 流式回调，§8 十二个错误码，
  §9 实现要点与 Drop 顺序，§10 边界声明。
- 判定三档：**确有问题**（按现有文字会产出错误或不可移植的行为，或与已冻结需求直接冲突）、
  **说的不够清晰**（文字可被两种以上实现同时满足，结果不同）、**只需补例子**（文字本身站得住，缺夹具/片段）。
- 引用行号一律指本仓或本机 `ct-lang` / `ct-sse` / `ct-llm-sse` 检出的当前文件。

## 0. 结论

草案的**传输内核方向**（隐藏 libcurl、安全默认、同步/流式两条路径）与既有设计一致；
**公开面**则大面积改写了已冻结需求：字符串链起手、`method()`、响应头读取、`HttpRequest` 自动释放、
跨线程取消、错误码枚举化五项都是**范围变更**，不是措辞问题。其中字符串链与 `HttpRequest` 自动释放
两项在现有编译器与现有判据下**无法同时成立**（I-1、F-1、F-4）。

- 确有问题：37 条
- 说的不够清晰：16 条
- 只需补例子：3 条
- 必须闭合才能开工：见 §12，共 16 条（多条合并同类问题）

---

## 1. 取消语义与生命周期

### A-1 `send()` 自动释放与 `cancel()` 跨线程互斥，构成必现的 use-after-free

| 栏 | 内容 |
| --- | --- |
| 问题 | §4 允许另一线程调 `cancel()`；§6 说 `send` 后内部自动释放 `HttpRequest`。两条同时成立时，`send()` 返回后仍可能到达的 `cancel()` 落在已释放对象上；`send()` 进行中释放又会被 libcurl 明确禁止（同一 handle 不得并发使用）。草案也没定义在 `send()` **之前**调用 `cancel()` 是粘滞（后续 `send` 直接返回取消）还是被静默忽略。 |
| 最短反例 | `HttpRequest *q = "http://slow/".method("GET");` 线程 A `q.send();`，线程 B 在 A 返回后 `q.cancel();` —— B 持有悬空指针。 |
| 依据 | 草案 §4、§6；`docs/requirements.md:52`（R-02 非目标「不做跨线程共享句柄」）、`:166`（R-13 非目标「不支持跨线程取消」）、`:196-198`（R-16 六路径不得重复释放）。 |
| 为什么无法唯一定义 | 「自动释放」没有给调用方任何可观察的释放点：无法知道 `cancel()` 是否还在窗口内；`atomic int` 只保护标志位，不保护对象寿命。实现者只能二选一（不释放 = 泄漏；释放 = 竞态），两种都与草案文字冲突。 |
| 建议改法 | 恢复 `HttpRequest` 显式 `delete`（R-02/R-16）；取消改用独立 token（`HttpCancel *`，引用计数或调用方持有），或限定「`cancel()` 仅在同一线程的进度/ chunk 回调内有效」，并把跨线程整体移出本期。 |

### A-2 「生命周期由调用方加锁保证」在 C 里不是可依赖的契约

| 栏 | 内容 |
| --- | --- |
| 问题 | 锁要生效，双方必须锁同一把锁；草案没有公开任何锁对象，且释放发生在库内部的 `send()` 里。调用方若在 `send()` 全程持锁，`cancel()` 被阻塞到请求结束；若不持锁，就与内部 `free` 竞态。 |
| 最短反例 | 线程 A：`lock(m); r = q.send(); unlock(m);` 线程 B：`q.cancel();`。B 不在该锁协议内，A 的锁对 B 无约束。 |
| 依据 | 草案 §4；`docs/requirements.md:265`（Q-12 线程安全口径未定）。 |
| 为什么无法唯一定义 | 「由调用方保证」把不确定性转嫁给调用方，但 API 没有给出保证手段；同一段文字可被实现为「文档免责」或「内部加锁」，行为不同。 |
| 建议改法 | 库内提供显式生命周期原语（引用计数、`cancel` token 或 `http_request_ref/unref`），并在 `ct.pkg` 与 README 写明「单对象单线程」或「多线程安全」二选一的口径。 |

### A-3 `is_cancelled()` 与 `is_ok()` 可能同时为真

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 定义 `is_ok = 2xx && error_code == OK`，`is_cancelled` 只看标志位。取消与完成竞态时（最后一字节已收、`perform` 尚未返回、进度回调不再触发），状态码 200、`error_code == OK`、`_cancelled == 1`，两个判定同时成立。 |
| 最短反例 | 服务器发完 200 body 的同一毫秒调用 `cancel()` → `is_ok()==1 && is_cancelled()==1`。 |
| 依据 | 草案 §4、§5；`docs/requirements.md:164`（取消时 `is_cancelled()==1`、`is_ok()==0`）、`:165`（回调中止不得置 cancelled）、`docs/phases/H2-H6.md:33`（H4-T2「与 `is_ok()` 互斥」）。 |
| 为什么无法唯一定义 | 「标志被置位」与「取消实际中断了传输」是两件事，草案只定义前者；实现者用前者判定就会与 R-13/H4-T2 的互斥判据冲突。 |
| 建议改法 | 增加 `cancel_effective`：仅当进度回调因取消返回非 0 才置位；`is_cancelled()` 读它，且 `is_cancelled()==1` 蕴含 `is_ok()==0`、`error_code` 为取消码。 |

### A-4 草案没有给出任何能拿到 `HttpRequest *` 的可编译形式

| 栏 | 内容 |
| --- | --- |
| 问题 | §2 的链从字符串起手、`send()` 结束；§4 却要求用户对 `HttpRequest` 调 `cancel()`。链式表达式里没有任何一步把请求句柄交还给用户；若把 `"u".method("GET")` 的返回值绑到变量，第二跳 `.header()` 又落到 `impl` 方法链（见 I-1，编译失败）。 |
| 最短反例 | `"http://slow/".method("GET").send();` 之后无处取 `q`；`HttpRequest *q = "http://slow/".method("GET"); q.cancel();` 的 `q` 从何而来、谁释放均未定义。 |
| 依据 | 草案 §2、§4、§6；`docs/requirements.md:244`（M-07：取消时机不可达）、`:257`（Q-04）。 |
| 为什么无法唯一定义 | 公开面同时存在「字符串即请求」与「请求对象可取消」两个模型，草案没有桥接语法；实现者只能自造第三种持有方式。 |
| 建议改法 | 明确 `HttpRequest` 是显式对象：`HttpRequest *q = HttpRequest.new();`，字符串只作 `q.url(...)` 的参数；`cancel()` 挂在 `q` 上，并给出跨线程的完整可编译样例。 |

### A-5 跨线程取消是需求层的明确非目标，且 CT 侧没有线程设施

| 栏 | 内容 |
| --- | --- |
| 问题 | 草案把跨线程取消写成主路径，但 requirements 两处写死不做；`ct-lang/lib/std` 没有任何 pthread/mutex/thread 封装，CT 单元无法自行起线程，只有 C consumer 能做到。 |
| 最短反例 | 纯 CT 程序里没有任何表达式能创建第二个线程去调 `cancel()`；判据只能由 C 夹具承载。 |
| 依据 | `docs/requirements.md:52`、`:166`、`docs/phases/H2-H6.md:34`（H4-T3）；`ct-lang/lib/std` 无线程相关命中。 |
| 为什么无法唯一定义 | 「可从另一线程调用」在 CT 包语境下是不可执行的承诺；实现者无法判断该判据是否必须由本包交付。 |
| 建议改法 | 本期删除跨线程取消；若坚持，单独立项「线程能力」并把验收判据改成 C 夹具 + 线程 sanitizer（TSAN）。 |

### A-6 进度回调未启用时 `cancel()` 永不生效

| 栏 | 内容 |
| --- | --- |
| 问题 | §4 只说要读原子位，§9 未列 `CURLOPT_NOPROGRESS=0` 与进度回调注册。libcurl 默认不调用进度回调，取消位没人读；同时 DNS 解析阶段是同步阻塞，进度回调也不会被调用。 |
| 最短反例 | 目标域名解析挂起 30s，另一线程置位取消；`post()` 仍等到超时才返回，R-13 的「3 秒内返回」失败。 |
| 依据 | 草案 §4、§9；`docs/requirements.md:163`（R-13 的 3 秒判据）。 |
| 为什么无法唯一定义 | 草案只描述「读位」，没描述「谁在何时读」；实现者可选择写回调内读位（无数据时不触发）或进度回调（需显式开启），两者在慢/无响应场景结果不同。 |
| 建议改法 | 明写 `CURLOPT_NOPROGRESS=0` + 进度回调读位；并把「取消不覆盖 DNS 阶段」写进判据，或引入异步解析器。 |

---

## 2. 流式

### B-1 `body == NULL` 时 `body_len` 与 `take_body` 无定义

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 只说流式时 `body` 为 `NULL`，未说 `body_len` 是 0、-1 还是已收字节数；`take_body(int*)` 在流式响应上返回什么、写出什么也未定义。 |
| 最短反例 | `int n = -1; char *b = r.take_body(&n);` → `b` 与 `n` 的期望值无法从草案推出。 |
| 依据 | 草案 §5、§6。 |
| 为什么无法唯一定义 | 同一句文字允许「NULL/0」「NULL/累计字节」「报错」三种实现，下游无法写可移植判断。 |
| 建议改法 | 规定流式下 `body == NULL`、`body_len == 0`、`take_body` 返回 `NULL` 并置 `*len = 0`；或直接规定流式响应调用 `take_body` 返回错误码。 |

### B-2 「`total_bytes` 是截断前总量」与「超限即中断」不可兼得

| 栏 | 内容 |
| --- | --- |
| 问题 | §3/§9 说在写回调内累计判断 `max_body_bytes`，§5 说 `total_bytes` 是截断前的总量。若到上限就中断，传输已停止，无法知道服务器原本要发多少；若继续读完只为计数，则限额失去保护内存/带宽的意义。 |
| 最短反例 | 上限 16MB，服务器声明 100MB。中断实现报 `total_bytes == 16MB`；读尽实现报 100MB 但耗时与流量不受控。两种都与「截断前总量」字面冲突或与限额目的冲突。 |
| 依据 | 草案 §3、§5、§9；`docs/phases/H2-H6.md:14`（H2-T2「溢出错误码」）。 |
| 为什么无法唯一定义 | 「总量」这个词在中断模型里没有定义域；实现者必须自行裁决，且两种裁决都能声称符合文字。 |
| 建议改法 | 定义为「已接收字节数（超限时等于上限）」，并规定超限立即中断、`error_code = BODY_TOO_LARGE`、已收部分是否保留（见 D-5）。 |

### B-3 重定向时中间响应体与最终响应体的拼接规则缺失

| 栏 | 内容 |
| --- | --- |
| 问题 | §7 只说头回调每次响应都触发，没说 `body` 在 302→200 时是只含最终体、还是把中间体也拼进去。libcurl 默认会把中间响应体也交给写回调，草案文字对两种处理都不禁止。 |
| 最短反例 | `302` 体 `"moved"`、`200` 体 `"ok"`；`r.body` 可能是 `"ok"` 或 `"movedok"`，`body_len` 随之不同。 |
| 依据 | 草案 §7；`docs/requirements.md:111`（R-08 只断言最终 `status_code` 与 body）。 |
| 为什么无法唯一定义 | 草案没有给出「响应边界」概念，实现者只能靠 libcurl 的隐式行为，而该行为不是契约。 |
| 建议改法 | 规定「非流式只保留最终响应体」（在头回调遇到新状态行时清空缓冲），或规定「按到达序拼接全部响应体」，并各给一条夹具。 |

### B-4 中间响应头与最终响应头混在同一个查询面

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 的 `header(name)` / `header_all` 没有响应维度。302 的 `Content-Type: text/html` 与 200 的 `Content-Type: text/event-stream` 同时存在时，查哪个由实现自定。§7 让用户「靠 `status_code` 判断是否最终响应」，但 `status_code` 是 `HttpResponse` 的单值，回调里没有。 |
| 最短反例 | 服务器先回 302（带 `Content-Type: text/html`）再回 200（`text/event-stream`）；`r.header("Content-Type")` 返回 html 会让下游 llm-sse 直接判成不支持类型。 |
| 依据 | 草案 §5、§7；`ct-llm-sse/docs/requirements.md:41`（C1 需要 content-type）、`:82`（R-A-04 类型判定）。 |
| 为什么无法唯一定义 | 「最终响应」在存储层没有标识；实现者可以「全部采集」「只留最后一份」「只留最后一份同名列」等，行为互不相同。 |
| 建议改法 | `header`/`header_all` 默认只看最终响应；`header_all` 增加 `int response_index` 或返回按响应分组的视图；回调签名带 `status_code` 或 `final` 标志。 |

### B-5 `HttpHeadersCb` 的签名不足以判断响应边界

| 栏 | 内容 |
| --- | --- |
| 问题 | §7 的回调是 `HttpHeadersCb(name, value, ud)`，没有状态码、响应序号或结束哨兵。libcurl 的头回调还会把状态行（`HTTP/1.1 302 Found`，无冒号）也交进来，草案「每头一次」没有覆盖这种行；`value` 是否已去掉前导空格与尾部 CRLF 也未定义。 |
| 最短反例 | 回调收到 `("Location","/next",ud)` 时，无法知道这是 302 的还是要跟随后的 200 的。 |
| 依据 | 草案 §7。 |
| 为什么无法唯一定义 | 用户被要求「靠 status_code 判断」，但该值在回调内不可见；实现者只能自行扩展签名，破坏草案冻结的 API。 |
| 建议改法 | 回调改为 `(int status_code, const char *name, const char *value, void *ud)`，或规定每段头结束发一次 `name == NULL` 的哨兵并写进契约。 |

### B-6 响应头名大小写未归一

| 栏 | 内容 |
| --- | --- |
| 问题 | HTTP 头名大小写不敏感，HTTP/2 一律小写。草案只说 `header(name)`，未说查找是否忽略大小写、`header_all` 是否保留原始拼写。 |
| 最短反例 | 服务器返回 `content-type: text/event-stream`（全小写），调用方查 `header("Content-Type")`；按字节比较的实现返回 NULL，下游判为 UNSUPPORTED_TYPE。 |
| 依据 | 草案 §5；`ct-llm-sse/docs/requirements.md:41`、`:82`；对照 `ct-sse/docs/requirements.md:38`（SSE 字段名**大小写敏感**是显式约定，HTTP 头不是）。 |
| 为什么无法唯一定义 | 「header(名字)」这一句同时允许大小写敏感与不敏感两种实现，且下游无法区分。 |
| 建议改法 | 查找一律 ASCII 大小写不敏感；`header_all` 保留服务器原始拼写；补一条全小写响应的夹具。 |

### B-7 `header_all` 与响应头生命周期未定义

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 只写「`header_all`（borrow）」「borrow，活到 delete」，未给原型、顺序、重复项规则、是否可在 `send_stream` 返回后查询、是否包含中间响应。 |
| 最短反例 | 两个 `Set-Cookie`；`header("Set-Cookie")` 返回第一条还是最后一条，`header_all` 是否两者都在，草案无法推出。 |
| 依据 | 草案 §5、§6。 |
| 为什么无法唯一定义 | 返回类型与所有权都没冻结，实现者可以返回 `char **`、结构体数组或拼接串；调用方无法写稳定代码。 |
| 建议改法 | 冻结原型（如 `int header_count(); const char *header_name_at(int); const char *header_value_at(int);`），规定按到达序、保留重复、借用期到 `delete`。 |

### B-8 响应头没有上限，`max_body_bytes` 管不到

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 只给 body 上限；§10 的边界声明也未提头。恶意或故障服务器可发海量头，内存在头回调阶段无界增长。 |
| 最短反例 | 服务器发 10 万个 `X: y` 头，body 为 0；`max_body_bytes` 不触发，进程内存耗尽。 |
| 依据 | 草案 §3、§10。 |
| 为什么无法唯一定义 | 草案没有任何头侧预算，实现者无法判断超限应报错、截断还是忽略。 |
| 建议改法 | 增加 `max_header_bytes` 与 `max_header_count`（或写死常量），超限报 `HEADER_TOO_LARGE` 并中断。 |

---

## 3. 超时

### C-1 三个超时命中同一个 `CURLcode`，无法区分为三个错误码

| 栏 | 内容 |
| --- | --- |
| 问题 | 总超时、连接超时、低速度（idle）超时在 libcurl 里都返回 `CURLE_OPERATION_TIMEDOUT(28)`；草案 §8 却要求 `TIMEOUT` / `IDLE_TIMEOUT`（以及 `CONNECT` 的一部分）分列。仅凭返回值无法判定是哪一个；三条同时到期时谁胜出也没有规则。 |
| 最短反例 | `connect_timeout(10)` 与 `timeout(120)` 同时设置，连接阶段卡 10s 返回 28；草案要求报 `CONNECT` 还是 `TIMEOUT`？ |
| 依据 | 草案 §3、§8、§9；`docs/requirements.md:173`（R-14 表：超时 28）。 |
| 为什么无法唯一定义 | 28 是唯一出口，草案没有要求实现记录阶段或起点；两个实现者按不同规则归类，都符合文字。 |
| 建议改法 | shim 记录 `phase` 与 `elapsed`（或记录进度回调最后一次触发时间），据此归类；或在 requirements 层放弃三码，统一 `TIMEOUT` + `error_detail` 带阶段。 |

### C-2 用 `LOW_SPEED_LIMIT=1` 实现 `idle_timeout` 会误杀慢而持续的流

| 栏 | 内容 |
| --- | --- |
| 问题 | `LOW_SPEED_*` 判的是「窗口内平均速率低于阈值」，不是「连续无字节」。每 59 秒到 1 字节的流没有任何 60 秒空档，但平均约 0.017 B/s，60 秒窗口到点即被杀。 |
| 最短反例 | 心跳型 SSE 每 59s 一条注释行；`idle_timeout(60)` 在第二条到达前中断，尽管从未空闲 60s。 |
| 依据 | 草案 §3、§9。 |
| 为什么无法唯一定义 | 草案把「idle」写成一个语义词，实现却给出「平均速率」；两者在慢流上不等价，且实现者无法同时满足。 |
| 建议改法 | 用进度回调自记「上次收到字节的时间戳」实现真正的 idle；或把配置名改为 `min_speed_bytes_per_sec` 并如实描述。 |

### C-3 低速度计时覆盖连接与等待首字节阶段

| 栏 | 内容 |
| --- | --- |
| 问题 | `LOW_SPEED_*` 从传输开始就计时；服务器做长任务、60 秒后才发首字节的合法场景会被 `idle_timeout(60)` 杀掉，而草案把 `idle_timeout` 描述成「仅流式」的读空闲。 |
| 最短反例 | `/slow-compute?ms=60000` 先算 60s 再回 200；`idle_timeout(60)` 在首字节前触发。 |
| 依据 | 草案 §3、§9。 |
| 为什么无法唯一定义 | 「空闲」的起点（连接成功/首字节/上一条数据）未定义，三种起点给出三种结果。 |
| 建议改法 | 明确「收到首个 body 字节后才武装 idle 计时」，并给首字节延迟的夹具。 |

### C-4 流式 300 秒总超时对长对话是硬切断，且 `timeout(0)` 语义未定

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 给流式 300s 默认总超时。长时间对话或长生成会在此被切断；调用方想延长只能调 `timeout()`，而 0/负数的语义草案没写（requirements Q-07 也未定）。 |
| 最短反例 | 一次 6 分钟的流式回答在 300s 处收到 `TIMEOUT`，此前数据已交付一半。 |
| 依据 | 草案 §3；`docs/requirements.md:260`（Q-07「0 是不限还是立即超时」）、`:166` 的 R-13 非目标。 |
| 为什么无法唯一定义 | 默认值是产品选择，0 的语义是契约缺口；实现者把 0 实现成「立即超时」也符合现有文字。 |
| 建议改法 | 流式默认 0（不限）或明说 300s 是可关的软上限；定义 `timeout(0)`、负数与上限。 |

### C-5 `timeout` 单位冲突：草案毫秒，requirements 秒

| 栏 | 内容 |
| --- | --- |
| 问题 | §2 示例 `.timeout(5000)`、§3 默认值用秒描述（120s/300s），暗示入参毫秒；R-06 的判据是 `timeout(1)` 在 1–3 秒内返回，暗示入参秒。同一个方法名两种单位。 |
| 最短反例 | 按 R-06 写 `req.timeout(15)` 期望 15 秒；按草案实现成毫秒则为 15ms，立即超时。 |
| 依据 | 草案 §2、§3；`docs/requirements.md:90`（R-06 判据）、`docs/phases/H1.md:25`（H1-T3 存 `int`）。 |
| 为什么无法唯一定义 | 草案与既有判据各给一个单位，实现者无论选哪个都会让另一半判据变红。 |
| 建议改法 | 统一单位并改名（`timeout_ms` / `timeout_s` 二选一），同步改 R-06 判据与 H1-T3。 |

---

## 4. 响应体上限与所有权

### D-1 `take_body` 之后 Drop 再 `free(body)` 会二次释放

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 的 `take_body(int*)` 转移所有权，§6 的 Drop 又释放 `body`。若 `take_body` 不清空字段，`delete r` 时 double free；草案没要求清空。 |
| 最短反例 | `char *b = r.take_body(&n); delete r; free(b);` → 第二次释放。 |
| 依据 | 草案 §5、§6；`docs/requirements.md:198`（R-16 二次释放负例）。 |
| 为什么无法唯一定义 | 「转移所有权」与「Drop 释放 body」两条规则叠加，缺一条「置空」就无法自洽；实现者补与不补都不违反字面。 |
| 建议改法 | 规定 `take_body` 成功后把 `body` 置 `NULL`、`body_len` 置 0；加一条 ASAN 负例断言不崩。 |

### D-2 `error_detail` 同时被称作 borrow 和 Drop 释放对象

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 说 `error_detail` 是 borrow、活到 `delete`；§6 说 Drop 释放 `error_detail`；§9 的 Drop 顺序里写的是 `free(error_msg)`。三处措辞指向不同所有权。若它借用的是请求侧错误缓冲，而请求在 `send()` 后已自动释放（§6），则响应上的 borrow 一开始就悬空。 |
| 最短反例 | `HttpResponse *r = q.send();`（q 已自动释放）→ `r.error_detail()` 读到已释放的 `CURLOPT_ERRORBUFFER` 内存。 |
| 依据 | 草案 §5、§6、§9；`docs/requirements.md:142`（R-11 要求 `error_msg` 字段与 `error_detail()`）。 |
| 为什么无法唯一定义 | 一个字段不可能既借用又被 Drop 释放；且借用源的寿命随 §6 的自动释放规则改变。 |
| 建议改法 | 响应自持错误文本（`send()` 返回时深拷贝），Drop 释放自有副本；`error_detail` 返回其借用视图；同时恢复 R-11 的 `error_msg` 字段。 |

### D-3 16MB / 64MB 两个上限缺理由，且分类维度错位

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 给非流式 16MB、流式 64MB。流式并不把 body 留在内存，理论上只需计数；非流式才需要内存预算。数值来源与「为什么流式反而更宽」都没写。 |
| 最短反例 | 流式请求收到 65MB 时中断，但此时内存占用并不高；用户已收到并处理了 64MB，中断只带来状态不一致（见 B-2）。 |
| 依据 | 草案 §3、§9。 |
| 为什么无法唯一定义 | 上限究竟是「内存保护」还是「流量/滥用保护」未定，两者对应的默认值与中断语义不同。 |
| 建议改法 | 说明每个上限的目的；流式上限改名 `max_stream_bytes` 并明确它是计数/反滥用，不保证内存。 |

### D-4 `total_bytes` 的口径未定

| 栏 | 内容 |
| --- | --- |
| 问题 | §5 只给字段名。它统计 write 回调收到的字节、wire 上的字节（含 chunked 框架、TLS 前？）、还是解码后字节；是否含中间 3xx 响应体，都没有定义。 |
| 最短反例 | gzip 响应：`body_len == 100`，wire 300；`total_bytes` 期望 100 还是 300？ |
| 依据 | 草案 §5、§9；`docs/requirements.md:131`（R-10 断言 `total_bytes`）。 |
| 为什么无法唯一定义 | 「总字节数」在 libcurl 有多个 `CURLINFO_SIZE_*` 对应物，草案没指定用哪一个。 |
| 建议改法 | 定义为「write 回调累计收到的 body 字节（解码前/后写明一种）」，并给 gzip 夹具。 |

### D-5 超限后 `body` 是否保留已收部分未定

| 栏 | 内容 |
| --- | --- |
| 问题 | §9 说在写回调内累计判断；超限后已收字节是保留（`take_body` 能取到前缀）还是全部丢弃，草案没写。这与 B-2 的 `total_bytes` 语义互相牵连。 |
| 最短反例 | 上限 16MB，收到 16MB+1；`r.body_len` 是 16MB 还是 0？ |
| 依据 | 草案 §5、§9。 |
| 为什么无法唯一定义 | 保留则 `take_body` 返回半截数据且无标记；丢弃则用户丢失全部；两种都不违背文字。 |
| 建议改法 | 二选一写死：推荐保留并置 `BODY_TOO_LARGE`，`take_body` 返回已收前缀、`body_len` 等于 `total_bytes`。 |

---

## 5. 错误分类

### E-1 没有 `CURLE_*` → `HTTP_ERR_*` 映射表，且枚举化与 R-14 直接冲突

| 栏 | 内容 |
| --- | --- |
| 问题 | §8 只列 12 个宏名，没有数值、没有映射规则。R-14 要求 `error_code` 直接等于 `CURLcode`（DNS 6、连接拒绝 7、超时 28、TLS 60、中止 42、写错误 23），未知码保留原值。草案的 `HTTP_ERR_CONNECT` 等与这套数值不兼容。 |
| 最短反例 | DNS 失败（6）与连接拒绝（7）在草案里都可归 `HTTP_ERR_CONNECT`；R-14 的表驱动判据要求分别等于 6 和 7，必红。 |
| 依据 | 草案 §8；`docs/requirements.md:173-176`（R-14 表驱动 + 未知码保留）、`:246`（M-09 本地码空间）、`:258`（Q-05）。 |
| 为什么无法唯一定义 | 「12 个宏」与「保留原始 CURLcode」是两套错误模型；没有映射表，实现者无法同时满足，也无法判断 `OTHER` 是否要携带原值。 |
| 建议改法 | 保留 `error_code = CURLcode` 原值（含未知），另加 `error_kind`（枚举）；或给出 12×N 完整映射表并把 R-14 作为范围变更重写。 |

### E-2 `WRITE` 与 `CALLBACK_ABORTED` 无法区分

| 栏 | 内容 |
| --- | --- |
| 问题 | §7 说用户 chunk 回调返回非 0 主动中断，§8 却同时给 `CALLBACK_ABORTED` 与 `WRITE`。二者都经写回调路径，libcurl 统一报 `CURLE_WRITE_ERROR(23)`；内部缓冲写失败（OOM、短路）也报 23。 |
| 最短反例 | 用户回调第一片返回 1，与库内 malloc 失败，外部都看到 23；草案要求前者 `CALLBACK_ABORTED`、后者 `WRITE`。 |
| 依据 | 草案 §7、§8；`docs/requirements.md:153`（R-12：回调中止返回 `CURLE_WRITE_ERROR(23)` 且 `is_cancelled()==0`）、`docs/phases/H2-H6.md:25`（H3-T3）。 |
| 为什么无法唯一定义 | 只有库侧额外标志才能区分，而草案既没规定标志，也没规定优先级；同时 R-12 已把 23 写进判据，草案另立新码与之冲突。 |
| 建议改法 | 库侧维护 `user_aborted` / `internal_write_failed` 两个标志，在写回调返回处分别置位；最终错误码仍需与 R-12 对齐（23），或明确修改 R-12。 |

### E-3 `OOM` 的来源与可恢复性未定义

| 栏 | 内容 |
| --- | --- |
| 问题 | §8 列了 `OOM`，但 write 回调内 malloc 失败只能返回 0，libcurl 会报 23；OOM 不会以独立 code 浮出。草案没说 OOM 由哪一层产生、是否可重试、已收 body 是否保留。 |
| 最短反例 | 16MB 缓冲扩容失败；调用方看到 23 还是 `OOM`？重试一次是否安全？ |
| 依据 | 草案 §8、§9。 |
| 为什么无法唯一定义 | 「OOM」不是传输层可观察事件；草案把它与传输码并列，实现者只能自定产生点。 |
| 建议改法 | 定义为本地终止错误：清空 body、`error_code = OOM`、不可重试；并说明它覆盖不了回调路径内的 OOM（那仍是 23）。 |

### E-4 `OTHER` 的出现条件缺失

| 栏 | 内容 |
| --- | --- |
| 问题 | §8 列 `OTHER` 但没说什么情况归它。`CURLE_PARTIAL_FILE(18)`、`CURLE_GOT_NOTHING(52)`、`CURLE_RECV_ERROR(56)` 按草案既可映射为具体码也可归 `OTHER`。 |
| 最短反例 | Content-Length 与实际不符（18）→ `OTHER` 还是 `WRITE`？两张表都能自圆其说。 |
| 依据 | 草案 §8；`docs/requirements.md:176`（未知码保留）。 |
| 为什么无法唯一定义 | 映射边界没冻结，同一 CURLcode 可落在多个宏里。 |
| 建议改法 | 给出至少 DNS/连接/超时/TLS/中止/写/截断/空响应 8 个样例的映射行，并规定「未列出的保留原值」。 |

### E-5 本地参数错误与传输码混在同一空间

| 栏 | 内容 |
| --- | --- |
| 问题 | `INVALID_URL` / `BODY_TOO_LARGE` / `OOM` / `OTHER` 是本地错误，却与传输码同列。数值未给，可能与 0、28、23 撞车；R-14 要求三类不混。 |
| 最短反例 | 若 `HTTP_ERR_TIMEOUT` 取 28、`HTTP_ERR_INVALID_URL` 也取 28，调用方无法区分本地拒绝与网络超时。 |
| 依据 | 草案 §8；`docs/requirements.md:172-176`（R-14「三类不能混」）、`:246`（M-09 建议负数空间）。 |
| 为什么无法唯一定义 | 草案没给数值与空间划分，实现者可自由取值，判据无法写死。 |
| 建议改法 | 本地错误用负数空间（或高位段），传输错误保留 `CURLcode` 原值；公开常量表并加边界断言。 |

---

## 6. 与既有 requirements 的冲突（范围变更）

### F-1 字符串链起手与 R-18 的门禁判据正相反

| 栏 | 内容 |
| --- | --- |
| 问题 | §2 的主形态是 `"https://…".method("GET").header(...).send()`；R-18 明确要求「含 `req.url(...).header(...)` 的片段必须被门禁拒绝（预期红）」，M-01 也已裁定单表达式链不成立。草案等于要求恢复被冻结否决的形态。 |
| 最短反例 | 把草案的示例放进 `docs/snippets/`，按 R-18 应被 g4 拒绝；草案却要求它能编译运行。 |
| 依据 | 草案 §2；`docs/requirements.md:218`（R-18）、`:238`（M-01）、`docs/design.md:56-69`（D-2）、`README.md:29-31`。 |
| 为什么无法唯一定义 | 两套判据互斥，实现者无法同时让「示例能跑」和「门禁拒绝该形态」成立。 |
| 建议改法 | 二选一：放弃字符串链（保留 `HttpRequest.new()` + 一条语句一个方法），或作为范围变更重写 R-18/M-01 与全部门禁，并给出真实编译夹具证明链路可编译。 |

### F-2 `method()` / `send()` / `send_stream()` 是对 R-10/R-12 的破坏性改名

| 栏 | 内容 |
| --- | --- |
| 问题 | 既有需求冻结 `post()`、`post_stream(cb, ud)`，且 R-10 的非目标写明「不提供 GET/PUT/DELETE 或自定义 method」。草案新增 `method()` 并把执行入口改名。 |
| 最短反例 | 按 R-10 写的 200/404/500 判据调用 `req.post()`；草案 API 没有 `post()`。 |
| 依据 | 草案 §1、§2；`docs/requirements.md:135`（R-10 非目标）、`:148-157`（R-12）、`docs/phases/H2-H6.md:9-26`。 |
| 为什么无法唯一定义 | 名字与能力都变了，任何实现只能满足一边。 |
| 建议改法 | 明确这是 v0.1 未发布期的改名，并一次性重写 R-10/R-12、H2/H3 全部任务与判据；或保留 `post`/`post_stream`，method 作为新增能力另条。 |

### F-3 响应头读取是 R-04/R-11 明文排除的范围

| 栏 | 内容 |
| --- | --- |
| 问题 | §1 把「响应头」列入 API，§5 给 `header`/`header_all`；R-04 非目标写「不提供响应头读取（读取面只列状态码/body/错误/字节数）」，R-11 同样排除。 |
| 最短反例 | R-11 的「grep 公开头无 setter、读取面只有 6 项」判据会被 `header_all` 打破。 |
| 依据 | 草案 §1、§5；`docs/requirements.md:72`（R-04 非目标）、`:145`（R-11 非目标）。 |
| 为什么无法唯一定义 | 需求层没有这个能力，验收面也没有对应夹具；实现者加了就无法通过 R-11 的公开面核对。 |
| 建议改法 | 作为范围新增：更新 R-04/R-11，补充响应头夹具（大小写、重复、重定向、上限），并说明下游 llm-sse 的 content-type 需求。 |

### F-4 `HttpRequest` 自动释放与 R-02/R-16 的 `delete` 判据冲突

| 栏 | 内容 |
| --- | --- |
| 问题 | §6 说请求不需要显式 `delete`，`send` 后内部释放；R-02 要求 `delete` 触发 `curl_easy_cleanup` 恰好一次、二次 `delete` 不崩，R-16 要求六条路径 ASAN 干净。自动释放会让「未 send 就放弃」的请求泄漏，也让二次 `delete` 无从谈起。 |
| 最短反例 | `HttpRequest *q = HttpRequest.new(); q.header(...);` 然后直接丢弃 → 泄漏；而 R-02 的 LSAN 判据要求 `delete` 后零泄漏。 |
| 依据 | 草案 §6；`docs/requirements.md:49-52`（R-02）、`:198-200`（R-16）、`docs/phases/H1.md:23`（H1-T1）。 |
| 为什么无法唯一定义 | 所有权模型二选一，草案选的那套恰好让已冻结的负例无法成立。 |
| 建议改法 | 恢复显式 `delete`；若确实要自动释放，则必须同时定义「未 send 的对象由谁释放」并重写 R-02/R-16。 |

### F-5 `error_code` 语义变更与 R-14 冲突

见 E-1。范围变更：错误码从「原始 `CURLcode`」改成「12 个包自定义宏」，R-14 的表驱动与「未知码保留原值」全部失效。

### F-6 跨线程取消与 R-13 非目标冲突

见 A-5。范围变更：从「不支持跨线程取消」变成「支持」，且需要新增线程能力与 TSAN 判据。

### F-7 `max_redirects(10)` 与 R-08 非目标冲突

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 新增 `max_redirects` 默认 10；R-08 非目标写「不设自定义跳转上限（沿用 libcurl 默认）」。 |
| 最短反例 | 11 跳重定向链：按 R-08 应跟随到 libcurl 默认上限，按草案在第 10 跳报错。 |
| 依据 | 草案 §3；`docs/requirements.md:113`（R-08 非目标）。 |
| 为什么无法唯一定义 | 上限数量直接改变可观察行为，且两处文字给出不同默认。 |
| 建议改法 | 删除该 setter，或把 R-08 的非目标改掉并补 10/11 跳两条夹具。 |

### F-8 草案漏掉的既有需求

| 栏 | 内容 |
| --- | --- |
| 问题 | 草案没有覆盖：R-03 的空 URL 与 URL 长度上限；R-04 的 CR/LF 拒绝与超长头；R-05 的二进制 NUL 与 `len<0`；R-06 的负超时；R-07 的默认 UA 可断言；R-09 的主机名校验（不只是证书链）；R-18 的门禁与 README 片段来源。 |
| 最短反例 | `body("a\0b", 3)`：§9 只写 `CURLOPT_COPYPOSTFIELDS`，未写 `POSTFIELDSIZE`；libcurl 默认按 `strlen` 取长，实际只发 1 字节。 |
| 依据 | 草案 §3、§9；`docs/requirements.md:59-61`（R-03）、`:69-74`（R-04）、`:80-82`（R-05）、`:91`（R-06）、`:100-102`（R-07）、`:120-122`（R-09）、`:218`（R-18）。 |
| 为什么无法唯一定义 | 这些是已冻结判据，草案没提等于默认不实现；实现者按草案做会直接让对应判据变红。 |
| 建议改法 | 逐条并入草案，并在实现要点里补 `CURLOPT_POSTFIELDSIZE`（或 `_LARGE`）。 |

### F-9 草案新增、requirements 没有的概念（范围变更清单）

| 栏 | 内容 |
| --- | --- |
| 问题 | 以下概念在 R-01～R-19 中不存在，属于新增范围，草案没有单列说明：`method()`（R-10 明确排除）；响应头读取 `header`/`header_all`（R-04/R-11 排除）；`take_body` 所有权转移（R-11 只有只读面）；`max_body_bytes` 及 16MB/64MB；`idle_timeout` 与 300s 流式总超时；`connect_timeout` 默认 10s；`max_redirects`（R-08 排除）；`HTTP_ERR_*` 枚举（与 R-14 的原始码语义冲突）；跨线程 `cancel`（R-13 排除）；`HttpRequest` 自动释放（R-02/R-16 相反）。 |
| 最短反例 | 用 R-11 的公开面核对脚本跑草案头文件：多出 `header`/`header_all`/`take_body`，脚本必红；把 `HTTP_ERR_TIMEOUT` 写进 R-14 的表驱动，数值对不上。 |
| 依据 | 草案 §1–§9；`docs/requirements.md:72`、`:135`、`:145`、`:113`、`:166`、`:173-176`、`:49-52`、`:196-198`。 |
| 为什么无法唯一定义 | 需求层没有这些条目，验收面也没有夹具；实现者无法判断哪些算「本期必须」、哪些算「以后再说」。 |
| 建议改法 | 单列一张「新增概念 / 对应需求编号（新建）/ 验收判据 / 是否本期」表，逐项走立项评审；未立项的不得进 v0.1 公开面。 |

---

## 7. 与 ct-sse / ct-llm-sse 的接口契约

### G-1 头回调与首个 chunk 的先后未冻结

| 栏 | 内容 |
| --- | --- |
| 问题 | 下游要先看状态码与 content-type 才能决定 SSE 还是 JSON 路径；草案没有规定 `HttpHeadersCb` 一定先于第一个 `HttpChunkCb`。 |
| 最短反例 | 调用方在 chunk 回调里才去查 `header("Content-Type")`，若实现延迟到头回调解码后才填充响应头，则首块已被误判。 |
| 依据 | 草案 §7；`ct-llm-sse/docs/requirements.md:41`（C1 输入三件事）、`:82`（R-A-04）。 |
| 为什么无法唯一定义 | 「每次响应都触发头回调」没说相对 body 的顺序；实现者可缓冲后统一回调，也可即时回调。 |
| 建议改法 | 写进契约「同一响应的头回调先于该响应首个 chunk」，并加一条顺序断言。 |

### G-2 流式 `body == NULL` 让 JSON 兜底路径拿不到整包

| 栏 | 内容 |
| --- | --- |
| 问题 | llm-sse 在 2xx + `application/json` 时要读整包 body；草案的 `send_stream` 把 body 置 NULL，且 `take_body` 在流式上的语义又未定义（B-1）。下游要么自己拼接（可行但重复劳动），要么依赖未定义行为。 |
| 最短反例 | 服务器回 `application/json` 的完整对象；`send_stream` 回调收到字节但 `r.take_body` 无定义。 |
| 依据 | 草案 §5、§7；`ct-llm-sse/docs/requirements.md:41`、`:45`（C5 两种类型）。 |
| 为什么无法唯一定义 | 传输层没有「按 content-type 选择缓冲」的契约，下游无法判断该用哪条路径。 |
| 建议改法 | 明确 `send_stream` 下 body 一律不保留，下游自行拼接；或提供 `send_buffer` 模式；或允许 `take_body` 返回已拼接流。三者选一写死。 |

### G-3 chunk 借用与 SSE 半行保留的配合未写明

| 栏 | 内容 |
| --- | --- |
| 问题 | §7 说 chunk 是 borrow、跨回调要自行 copy；ct-sse 的解析器必须在两次 `feed` 之间保留半行，因此它自己会复制。契约本身相容，但草案与下游文档都没有把「回调返回后 chunk 失效」写成强制句。 |
| 最短反例 | 下游把 `data` 指针存进事件结构，回调返回后读 → 悬空；ct-sse 的事件字段本身也是借用（`R-B-05` 要求显式 copy）。 |
| 依据 | 草案 §7；`ct-sse/docs/requirements.md:35`（R-A-01 跨 feed 保留半行）、`:54`（R-B-05 跨回调保留必须 copy）。 |
| 为什么无法唯一定义 | 借用期边界只在草案一句话里，没有夹具，也没有和下游文档对齐；实现者可以延长寿命（复制）而不违反文字。 |
| 建议改法 | 在 README 与头注释写死「回调返回即失效」，并给一条「回调内立即 feed」的集成夹具；`HttpChunkCb` 的 `data` 加 `const`。 |

### G-4 取消/失败如何映射到下游的「fail 通知」未定义

| 栏 | 内容 |
| --- | --- |
| 问题 | llm-sse 的 `fail(code)` 需要传输层给出「失败/取消」信号；草案只给 `error_code` 与 `is_cancelled`，没有约定取消时 `error_detail` 是否为空、`status_code` 是否保留。 |
| 最短反例 | 用户在流中间取消：`status_code` 可能是 200、`error_code` 可能是 `CANCELLED`、`body` 为 NULL；下游无法判断该走 EOF 还是 fail。 |
| 依据 | 草案 §5、§8；`ct-llm-sse/docs/requirements.md:41`、`:81`（R-A-03 finish/fail 两条入口）。 |
| 为什么无法唯一定义 | 草案没有「取消后的响应状态」真值表，两个实现者会给出不同组合。 |
| 建议改法 | 冻结取消状态表：`is_cancelled()==1` 时 `is_ok()==0`、`error_code==CANCELLED`、`status_code` 保留已收到的最后值、`body==NULL`。 |

---

## 8. 安全

### H-1 未限制 URL 协议，`file://` 等本地读取可直接穿透

| 栏 | 内容 |
| --- | --- |
| 问题 | §10 只声明「不做 URL 白名单」，没有声明「只允许 http/https」。libcurl 默认允许大量 scheme（`file`、`ftp` 等）。字符串链让任何字符串都能起手发请求。 |
| 最短反例 | `"file:///etc/passwd".send()` 返回文件内容；`status_code == 0`、`error_code == OK`，调用方若只看 `error_code` 会当成成功。 |
| 依据 | 草案 §2、§10；`docs/requirements.md:62`（R-03 只排除 URL 解析/拼接，没有排除 scheme 限制）。 |
| 为什么无法唯一定义 | 「不做白名单」与「只做 HTTP」不是一回事；实现者可以选择设置协议掩码，也可以不设，行为差一个本地文件读取原语。 |
| 建议改法 | 在 shim 设置 `CURLOPT_PROTOCOLS` / `CURLOPT_REDIR_PROTOCOLS` 仅 `HTTP/HTTPS`，其它 scheme 返回 `INVALID_URL`，并加 `file://` 负例。 |

### H-2 重定向跨主机转发自定义头（含 `Authorization` / `Cookie`）

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 提供 `follow_redirects` 与 `max_redirects`，§10 没有任何头转发策略。libcurl 默认会把 `CURLOPT_HTTPHEADER` 的自定义头带到重定向目标，跨主机也一样。 |
| 最短反例 | 请求 `https://api.example.com` 带 `Authorization: Bearer …`；服务器 302 到 `https://evil.test`；凭据随头转发。 |
| 依据 | 草案 §3、§10；`docs/requirements.md:111`（R-08 的跟随判据不覆盖头转发）。 |
| 为什么无法唯一定义 | 「跟随重定向」只定义了跳数，没定义敏感头的存活规则；实现者可以全转发、全剥离或按同源剥离，都符合文字。 |
| 建议改法 | 默认只在同源（scheme+host+port）转发自定义头；跨源剥离 `Authorization`/`Cookie`/`Proxy-Authorization`；写进 README 并加夹具。 |

### H-3 `https` → `http` 降级重定向无策略

| 栏 | 内容 |
| --- | --- |
| 问题 | 默认 `follow_redirects(0)` 是安全的，但一旦打开，libcurl 允许跳回明文 http；草案没有任何提示或拒绝。 |
| 最短反例 | `https://a` 返回 `Location: http://a/x`；跟随成功后凭据与 body 走明文。 |
| 依据 | 草案 §3、§10；`docs/requirements.md:113`（R-08 非目标未提协议降级）。 |
| 为什么无法唯一定义 | 「跟随重定向」是否包含降级没有定义，实现者可拒绝也可放行。 |
| 建议改法 | 规定默认拒绝 https→http 降级（`REDIR_PROTOCOLS` 限定或手动检查），需显式开关才允许。 |

### H-4 `header()` 不拒绝 CR/LF，可注入额外头或请求行

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 只说同名追加；`method()` 与 URL 同样没有控制字符校验。值里带 `\r\n` 可拆出新头，method 里带控制字符可污染请求行。 |
| 最短反例 | `header("X", "a\r\nAuthorization: Bearer stolen")` → 服务端收到两条头。 |
| 依据 | 草案 §2、§3；`docs/requirements.md:74`（Q-10 未定，但需求已把「拒绝 CR/LF」列为待确认项）、`:263`。 |
| 为什么无法唯一定义 | 草案没规定拒绝还是透传；两种实现都符合现有文字，但一种有注入面。 |
| 建议改法 | `header`/URL/method 一律拒绝 `\r`、`\n`、`\0` 与控制字符，返回本地 `INVALID_ARG`；补三条负例。 |

### H-5 `insecure(1)` 的作用范围未写清

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 只有 `insecure` 一个布尔，§5 没有对应说明。R-09 要求默认同时校验证书链**与**主机名，并给出 SAN 不匹配的夹具。 |
| 最短反例 | 实现只关 `VERIFYPEER` 不关 `VERIFYHOST`：`insecure(1)` 后自签证书 200，但主机名不匹配仍失败，R-09 第二条判据红。 |
| 依据 | 草案 §3；`docs/requirements.md:120-122`（R-09 两条判据）。 |
| 为什么无法唯一定义 | 「insecure」是语义词，映射到一个还是两个 libcurl 选项未定。 |
| 建议改法 | 明写同时关 `CURLOPT_SSL_VERIFYPEER` 与 `CURLOPT_SSL_VERIFYHOST`，且对重定向后的每一跳生效。 |

### H-6 无 TLS 最低版本与 CA 来源说明（与既有非目标一致，但需写进文档）

| 栏 | 内容 |
| --- | --- |
| 问题 | 草案没有 TLS 版本下限、CA 路径、失败文案。R-09 已把「自定义 CA / 客户端证书 / TLS 版本选择」列为非目标，所以这不是范围冲突；但「依赖系统默认」在 macOS（SecureTransport/LibreSSL）与 Linux（OpenSSL）上的默认 CA 与最低版本不同，需要可断言的说明。 |
| 最短反例 | 同一自签夹具在 macOS 与 Linux 的失败码不同（60 vs 35），R-09 要求「文档二选一并固定」，草案没有任何固定。 |
| 依据 | 草案 §10；`docs/requirements.md:120`、`:124`（R-09 非目标）。 |
| 为什么无法唯一定义 | 平台差异存在但草案未指定归一方式，实现者可以原样透出也可自行归类。 |
| 建议改法 | 写明「不提供 CA/版本配置，依赖系统 libcurl」，并固定 TLS 失败归一到 `SSL`（原始码保留在 detail）；补双平台夹具。 |

---

## 9. 语法与实现可行性

### I-1 若配置方法是 `impl` 方法，字符串链在第 2 跳就编译失败

| 栏 | 内容 |
| --- | --- |
| 问题 | §2 的链要成立，每一跳的接收者都是上一跳的返回值。CT 的 `impl` 固有方法调用**不能**作接收者；只有 type-host map（`map trait … for type …`）能链。草案同时写「method/send 挂在 `char *` 宿主上」与「`HttpRequest*` 配置方法返回 self」，没有说明后者用哪种机制。 |
| 最短反例 | 若 `method()` 返回 `HttpRequest*` 且 `header()` 是 `impl` 方法：`"u".method("GET").header("A","1")` 报 `error: a method call on a temporary is only allowed at the start of a statement or of an assignment`。 |
| 依据 | 草案 §2、§3；`ct-lang/examples/13_builder_chain/src/main.ct:9-10`（「An `impl` method call cannot be a receiver」）、`ct-lang/docs/language-spec.md:561`（只有 type-host map 的链合法）、`ct-lang/selfhost/ct-ast/backend/emit/Emitter.h.ct:2326`、`ct-lang/selfhost/check-ast.sh:4321`、`docs/design.md:36`（探针 p3i）、`docs/requirements.md:238`（M-01）。 |
| 为什么无法唯一定义 | 草案没说 `HttpRequest` 的配置方法用 `impl` 还是 type-host map；用前者链必失败，用后者才能链。实现者无法从文字判断该选哪个。 |
| 建议改法 | 全链改 type-host map，并在卡里附一条真实编译探针；或放弃链，恢复「一条语句一个方法」（D-2）。 |

### I-2 全链走 `char *` type-host 时，配置状态无处安放

| 栏 | 内容 |
| --- | --- |
| 问题 | type-host map 的 recipe 只把接收者替换一次，`"url".method("GET")` 的返回值若仍是 `char *`，则后续 `header()`/`timeout()` 拿到的还是同一个 URL 字符串，没有任何对象承载已配置的头/体/超时。 |
| 最短反例 | `char *u = "http://h/a"; u.header("X","1").send();` 与 `"http://h/a".header("X","2").send();` 若按指针值做侧表键，字面量地址可能相同（实现定义）→ 两条链互相污染；若按字符串内容做键，则同一 URL 的并发请求无法区分。 |
| 依据 | 草案 §2、§3；`ct-lang/docs/language-spec.md:553-561`（recipe 只替换接收者）、`ct-lang/docs/tutorial.md:577`（`char *` 方法名全局共享）。 |
| 为什么无法唯一定义 | 草案没有定义「配置状态存在哪里」；实现者只能自造全局/线程局部侧表，键的选择会改变可观察行为。 |
| 建议改法 | 回到显式 `HttpRequest` 对象（状态在结构体里），字符串只作为 `url()` 参数；若保留链，必须定义状态载体与并发模型。 |

### I-3 `impl char *` 不存在，字符串宿主只能来自 `.m.ct` 的静态 trait

| 栏 | 内容 |
| --- | --- |
| 问题 | CT 的 `impl T { … }` 只在 `T` 是已声明 struct 时成立；`char *` 不是 struct，无法 `impl`。字符串方法只能由 `map trait … for type char *` 提供，而该 trait 必须声明在 `.m.ct`，是静态 trait，不能有默认体、不能 `impl`、不能装箱。 |
| 最短反例 | 写 `impl char * { ... }` 会在 `find_struct("char")` 失败后落到 trait 路径，报 `impl of an unknown trait` 或语法错。 |
| 依据 | 草案 §2；`ct-lang/selfhost/ct-ast/frontend/parse/Parser.h.ct:2900-2906`（inherent 必须命中已声明 struct）、`ct-lang/docs/language-spec.md:548`（`.m.ct` 的 `pub trait` 自动是静态 trait）。 |
| 为什么无法唯一定义 | 草案把「挂在 `char *` 宿主上」当成已有能力描述，但实现路径只有一种且约束很多；不写明会让实现者反复试错。 |
| 建议改法 | 写明用 `map trait … for type char *`，并给出 `.m.ct` + shim 的文件布局与一条编译探针。 |

### I-4 C99 门禁与 `_Atomic` 冲突

| 栏 | 内容 |
| --- | --- |
| 问题 | §4 用 `_cancelled` 原子整型；H1 的出口判据要求 shim 以 `-Wall -Wextra -std=c99` 零警告编译。`_Atomic` / `<stdatomic.h>` 是 C11。 |
| 最短反例 | g3 接好后，shim 里的 `_Atomic int` 在 `-std=c99` 下编译失败（或需要 `__STDC_VERSION__` 保护）。 |
| 依据 | 草案 §4；`docs/phases/H1.md:37`（E-H1-3）。 |
| 为什么无法唯一定义 | 草案没有指定 C 标准；不改门禁就得换原子实现，两者都合理但互斥。 |
| 建议改法 | 用 GCC/Clang `__atomic_*` 内建（C99 可用），或把 shim 标准提到 C11 并同步改 E-H1-3 与 AGENTS 门禁。 |

### I-5 `.m.ct` 不能放函数与状态，必须指定 C 侧载体

| 栏 | 内容 |
| --- | --- |
| 问题 | 若公开面走 type-host map，`.m.ct` 只允许 prologue、`pub trait`、`pub type`、`map`；请求状态、缓冲、atomic 都只能放在 shim 的 C 结构体里。草案 §9 的 Drop 顺序却按 CT 的 `Drop` 描述。 |
| 最短反例 | 按草案在 CT 侧写 `impl Drop for HttpRequest`，同时公开面又走 `.m.ct` 的静态 trait，两边对「谁拥有 easy handle」给出不同答案。 |
| 依据 | 草案 §6、§9；`ct-lang/docs/language-spec.md:525-530`。 |
| 为什么无法唯一定义 | 所有权归属（CT `Drop` vs C `free`）在两套机制下不同，草案没有指定。 |
| 建议改法 | 明确「对象由 C 侧 `http_request_free` 释放，CT 侧只持不透明句柄」，或明确走 CT struct + `Drop` 并放弃字符串宿主。 |

---

## 10. 只需补例子

以下三条文字本身自洽，缺的是可执行片段或夹具，补齐即可，不需要改契约：

### L-1 默认值总表与「不调 setter」断言

| 栏 | 内容 |
| --- | --- |
| 问题 | §3 的默认值分散在正文里（120s/300s/10s/16MB/64MB/10/0/0），没有一张表，也没有「不调 setter 时这些值成立」的夹具；默认 UA 除外（见 F-8）。 |
| 最短反例 | 实现把 `follow_redirects` 默认写成 1 仍能通过肉眼审阅；没有断言就没人发现。 |
| 依据 | 草案 §3；`docs/requirements.md:100-102`（R-07 要求默认值可断言）、`docs/phases/H1.md:26`（H1-T4 负例）。 |
| 为什么无法唯一定义 | 不缺定义，缺的是把定义变成可观察事件的载体。 |
| 建议改法 | 补一张「配置项 / 默认值 / 非法值 / 对应夹具」表，并给一条「全部走默认」的端到端样例。 |

### L-2 `defer { delete r; }` 的合法位置示例

| 栏 | 内容 |
| --- | --- |
| 问题 | §6 用 `defer { delete r; }` 表达响应释放，但没有说明它在 CT 里的限制：只能出现在函数体内语句位置，`.h.ct` 内禁止，`switch` 内禁止，defer 内禁止控制转移。 |
| 最短反例 | 用户把示例抄进 `.h.ct` 的 inline 函数 → `defer is not allowed in a .h.ct unit`。 |
| 依据 | 草案 §6；`ct-lang/docs/language-spec.md:420-432`。 |
| 为什么无法唯一定义 | 语义明确，只是示例没有标注适用范围。 |
| 建议改法 | 在 README 示例处补一行「只能写在普通 `.ct` 函数体」，并加一个含 `return`/`break` 的完整片段。 |

### L-3 流式回调的最短可运行片段

| 栏 | 内容 |
| --- | --- |
| 问题 | §7 只说 chunk 是 borrow、跨回调需 copy，没有给出「回调内立即消费」与「需要保留时显式 copy」两段对照代码。 |
| 最短反例 | 用户照文字实现却在回调外持有 `data`，测试才暴露悬空；没有样例就没有可复制的正确写法。 |
| 依据 | 草案 §7；`ct-sse/docs/requirements.md:35`、`:54`。 |
| 为什么无法唯一定义 | 契约完整，缺的是使用示例与一条「拷贝后回调外仍可读」的夹具。 |
| 建议改法 | 补两段片段（直接 feed / 显式 copy）与一个集成夹具。 |

---

## 11. 按现有文字解释得通、不附议的条目

以下条目看似可疑，但按草案文字或既有需求可以自洽，不作为问题计入：

1. `follow_redirects` 默认 0、`insecure` 默认 0：与 D-5、R-08、R-09 一致，安全默认正确。
2. 不做自动 Content-Type、不做 cookie、不做代理、不做重试、不做连接池、不做 body 解析：与 D-6、R-05、R-08 非目标一致。
3. `status_code == 0 表示未拿到响应`：0 不是合法 HTTP 状态码，判断可判定（但 `file://` 会绕过这一假设，见 H-1）。
4. `is_ok = 2xx && error_code == OK`：与 R-10/R-11 一致，问题只在取消竞态（A-3）。
5. chunk 是 borrow、跨回调需 copy：与 ct-sse R-A-01/R-B-05 相容，SSE 解析器本就复制半行；缺的是 `const` 与强制措辞（G-3）。
6. `error_detail` 借用、活到 `delete`：只要响应自持错误文本就自洽；矛盾只出在「Drop 释放 error_detail」的措辞（D-2）。
7. 16MB / 64MB 两个数值本身不违反任何既有需求：问题在理由缺失与 `total_bytes` 口径（D-3、B-2），不是数值。
8. 不做 URL 白名单：与 R-03 非目标一致；问题是它不等同于「不做 scheme 限制」（H-1）。

---

## 12. 必须闭合才能开工的编号清单

以下 16 条按「先定契约、再写实现」排序；每条闭合指草案文字改到唯一可实现、且能落成一条会变红的夹具：

1. **A-1 + A-2 + F-4**：`HttpRequest` 生命周期归属二选一并写死；取消不得依赖「调用方加锁」。
2. **A-4 + I-1 + I-2 + I-3**：公开面语法定稿（显式对象或 type-host map），附一条真实编译探针；字符串链若保留，必须给出可编译形式与状态载体。
3. **A-3**：`is_cancelled` / `is_ok` / `error_code` 的真值表与优先级。
4. **A-5**：跨线程取消本期去留，以及所需的线程能力与判据。
5. **B-2 + D-4 + D-5**：`total_bytes`、`max_body_bytes`、超限后 body 保留规则三者一次定死。
6. **B-4 + B-5 + B-7 + G-1**：响应头的存储维度、回调签名、查询面与「头先于 chunk」契约。
7. **B-6**：头名大小写归一（并与下游 content-type 判定对齐）。
8. **B-8**：响应头预算与超限错误码。
9. **C-1**：三个超时到错误码的归类规则（28 的唯一出口如何拆分）。
10. **C-2 + C-3**：`idle_timeout` 的定义（真 idle 还是平均速率；何时武装）。
11. **C-4 + C-5**：超时单位统一、`timeout(0)` 语义、流式默认值。
12. **E-1 + E-5 + F-5**：错误码模型二选一（原始 `CURLcode` + `kind`，或完整映射表），并给本地错误码空间。
13. **E-2**：`WRITE` / `CALLBACK_ABORTED` 的区分标志与最终码值（对齐 R-12）。
14. **F-1 + F-2 + F-3 + F-7 + F-9**：范围变更逐项裁定（字符串链、method/send 改名、响应头读取、跳数上限、新增概念清单），改需求或改草案，不得两存。
15. **H-1 + H-2 + H-3 + H-4**：协议掩码、跨源头转发、https 降级、CR/LF 校验四条安全规则写死并各配负例。
16. **I-4 + I-5**：C 标准与原子实现、对象所有权归属（CT `Drop` 还是 C `free`）定稿。

以上任一条未闭合，实现者都能写出「符合草案文字但跑不过既有判据」的版本；此时开工等于把返工写进排期。
