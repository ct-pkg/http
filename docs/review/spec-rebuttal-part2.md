# ct-http 包规范草案 · 反证评审（续 2）

- 本篇接 [spec-rebuttal.md](spec-rebuttal.md) §5，含 §5–§8。
- 拆分：2026-11-15 按小节拆分入库，正文逐字未改；同内容副本见 `docs/reviews/spec-review/`。

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

