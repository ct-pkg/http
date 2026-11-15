# ct-http 0.1.0 feature 划分

- applies-to: 0.1.0
- 日期：2026-11-15
- 版本：0.1.0
- 对应 roadmap：[ct-http-0.1.0-roadmap.md](ct-http-0.1.0-roadmap.md)
- 说明：feature 是「可独立验收的交付物」；phase 是施工顺序。两者互相索引：每个 feature 标了所属 phase，每个 phase 标了覆盖的 feature。

## 1. 编号与规模

- `F-nn`：0.1.0 内要交付的 feature。`F-01`…`F-11` 是**不修就不能开工**的阻塞项，逐条独立，不合并。
- `S-nn`：范围待裁定项。未裁定前不算 feature，不排进 phase（见 §3）。
- 规模：`S` 一次会话内可收口；`M` 一个提交周期内可收口；`L` 不允许直接出现，必须再拆。本清单全部为 S/M。

## 2. Feature 清单

### 2.1 必修阻塞（F-01…F-11）

| ID | 一句话价值 | 覆盖 R-* | 验收判据（至少一条负例） | 依赖 | 可独立交付 | 规模 | phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F-01 | body 按显式长度发送，内嵌 NUL 不被截断 | R-05、R-10 | `body("a\0b",3)` 服务端逐字节收 3 字节；shim 捕获 `POSTFIELDSIZE==3` 且先于 `COPYPOSTFIELDS`。负例：去掉 `POSTFIELDSIZE` → 只收 1 字节，夹具红 | F-15、F-16 | 是 | S | 3 |
| F-02 | 连接 / 总时长 / 空闲三类超时可区分 | R-06、R-14、R-13 | `error_detail` 分别含 `phase=connect` / `phase=total` / `phase=idle`；端到端各一条。负例：三处都只映射成 28 且无阶段 → 红 | F-15、F-16、F-17 | 是 | M | 6 |
| F-03 | 慢而持续的流不被 `LOW_SPEED_LIMIT` 误杀 | R-06、R-12 | 每 2 秒 1 字节持续 6 秒，`idle_timeout(3)` 不中止；真 3 秒空档才中止。负例：`LOW_SPEED_LIMIT=1` 杀掉慢流 → 红 | F-02、F-18 | 是 | M | 7 |
| F-04 | `take_body` 后所有权单一，不再二次释放 | R-11、R-16 | `take_body` 成功后 `body==NULL`、`body_len==0`；`http_response_free` + `free(body)` ASAN 0 报错。负例：不清空字段 → double-free 红 | F-10、F-16 | 是 | S | 4 |
| F-05 | `is_ok` 与 `is_cancelled` 永不同时为真 | R-10、R-11、R-13 | 四态矩阵：ok / 404 / 传输错误 / 取消；取消态 `is_cancelled==1 && is_ok==0` 且 `error_code==42`。负例：完成后置取消位 → 矩阵红 | F-16、F-19 | 是 | S | 5 |
| F-06 | 请求头不能注入 CR/LF | R-04（Q-10 收紧为拒绝）、R-14 | `header("X","a\r\nB: c")` 与名字含 `\n` 均返回本地错误，slist 不增。负例：透传后服务端收到两条头 → 红 | F-15、F-17 | 是 | S | 3 |
| F-07 | 只允许 http/https，`file://` 不能读本地文件 | R-03、R-15 | `file:///etc/passwd`、`ftp://…` 连接前本地错误，服务端计数 0；http/https 正常。负例：不设协议掩码 → 文件内容被读回 → 红 | F-15 | 是 | S | 3 |
| F-08 | 重定向跨主机不转发 `Authorization` / `Cookie` | R-08、R-04 | A→B 302 后 B 回显头无凭据；同主机 302 保留。负例：沿用 libcurl 默认全转发 → 红 | F-15、F-16 | 是 | M | 9 |
| F-09 | 默认拒绝 https→http 降级 | R-08、R-09 | https 302 到 http 时返回错误，明文服务端计数 0；显式开关（若裁定提供）才放行。负例：跟随成功走明文 → 红 | F-08 | 是 | S | 9 |
| F-10 | 释放显式、cleanup 恰好一次，`send()` 不自动释放 | R-02（改写）、R-15、R-16 | `http_request_free` 后 `easy_cleanup==1`；`send()` 后计数仍 0；双 free 安全。负例：`send()` 内自动 free → ASAN use-after-free 红 | F-12、F-14 | 是 | M | 2 |
| F-11 | 消灭「自动释放 + 跨线程取消」的 UAF，必须改设计 | R-02、R-13、R-16 | 设计记录二选一并落地；方向 A：`send()` 后请求有效、cancel 仅回调内；方向 B：refcount/token + TSAN。负例：原设计（send 释放 + 他线程 cancel）ASAN 红 | F-10、F-19 | 否（受 S-04） | M | 8 |

**F-11 的两个方向（必须写清后果，不替用户选）**

- **方向 A：同线程取消 + 显式释放。** `send()` 不释放请求；`cancel()` 只在 chunk / 进度回调内有效；调用方在 `send()` 返回后自行 `http_request_free`。
  - 后果：跨线程取消能力没有；调用方要自己保证线程边界；判据可以在纯 CT 内落地；无需 pthread/TSAN；R-13 的非目标保持不变。
- **方向 B：跨线程取消 + 生命周期原语。** 增加 `HttpCancel *` 或 `http_request_ref/unref`；`send()` 不释放；调用方在 `cancel()` 返回后 `unref`。
  - 后果：需要线程设施（C 夹具 + pthread）与 TSAN；CT 标准库没有线程封装，判据只能由 C 夹具承载；API 面变大；R-13 必须改写，S-04 必须纳入。

### 2.2 交付 feature（F-12…F-23）

| ID | 一句话价值 | 覆盖 R-* | 验收判据 | 依赖 | 可独立交付 | 规模 | phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F-12 | 一个 `import` 拿到链式 API，libcurl 类型不外泄 | R-01、R-15、R-18、R-19 | `import http::http;` 后 `CRequest/CResponse` 与 `.post()/.header()/.timeout()/.send()/.status()` 可用；`import http::Types;` 单独不可用；`grep '#include <curl/' src` 零命中 | 无 | 是 | M | 1、10 |
| F-13 | 字符串起手的三段链可编译，展开为嵌套裸 C 调用 | R-02、R-10 | 4 跳链 `ct build` rc=0；生成 C 无 `_iface` / `_vtbl`；变量落点形式同样 rc=0 | F-12 | 是 | S | 1 |
| F-14 | libcurl 只在 shim；测试可替换 ops 而不改调用点 | R-15、R-16 | `nm -u` CT 对象无 curl 符号；夹具替换 `struct HttpRequest` 的 ops 后链观察到的行为改变 | F-10、F-12 | 是 | M | 2 |
| F-15 | URL/头/body/超时/UA/TLS/重定向的配置面完整 | R-03…R-09 | 每个 setter 有 shim 捕获夹具；默认值逐项断言；非法值本地报错 | F-12、F-14 | 是 | M | 3 |
| F-16 | `send()` 一次完成执行并给出稳定响应面 | R-10、R-11 | 200/404/500 状态码与 body 逐字节；`total_bytes` 口径写死；公开面无可写 setter | F-15 | 是 | M | 4 |
| F-17 | 传输错误 / HTTP 错误 / 本地参数错误不混 | R-14 | 表驱动 6/7/28/60/42/23；4xx/5xx `error_code==0`；未知码保留；`error_msg` 非空 | F-16 | 是 | M | 5 |
| F-18 | 大响应分片交付，回调可中止 | R-12 | 1 MB ≥2 片且拼接逐字节相等；首片返回非 0 → 23 且 `is_cancelled==0`；与 `body` 互斥 | F-16、F-17 | 是 | M | 7 |
| F-19 | 同线程内可确定性取消 | R-13 | 慢响应回调内取消，3 秒内返回 42；未取消路径 `is_cancelled==0`；DNS 阶段不可取消写进文档 | F-18 | 是 | M | 8 |
| F-21 | macOS/Linux 系统 libcurl 同源可跑，sanitizer 干净 | R-17、R-16 | 双平台 `test/run.sh all` 0 失败；`curl_version()` 首 token 与系统一致；ASAN/LSAN 干净 | 全部功能 feature | 否 | M | 10 |
| F-22 | 每条主张有命令；文档样例可编译；坏形态被门禁拒绝 | R-18 | g0–g9 接线；`g1` 校验卡片十项与判据摘要；`g4` 编译正例、拒绝 impl-chain 负例 | F-12、F-21 | 部分（最小版 phase 1） | M | 1、10 |
| F-23 | 0.1.0 可安装、版本落点一致 | R-19、R-01 | 干净目录 `ct get http@^0.1.0 && ct install`，样例 200；`ct.pkg#version` 与 README 一致；tag 只提案 | F-21、F-22 | 否 | S | 10 |

> F-20 不单独设 feature：重定向策略由 F-08/F-09 覆盖，避免与阻塞项重复计数。

## 3. 范围待裁定项（S-01…S-05）

> 本节只陈述选项与影响面，不替用户选择。未裁定的项不进入 phase 1；其余 phase 的卡片必须写明「按裁定结果执行」。

### S-01 响应头读取

- **提案要求什么**：`HttpResponse` 增加 `header(name)` / `header_all()`，可读响应头。
- **与 requirements 哪条冲突**：R-04 非目标「不提供响应头读取（读取面只列状态码/body/错误/字节数）」；R-11 非目标「不读响应头」，且 R-11 的公开面判据只列 6 项。
- **包含的影响面**：F-16 扩读取面；新增头存储、大小写归一、重复项、重定向中间响应、头上限（一行 fixture 不够）；phase 4 与 phase 10 扩项；下游 `ct-llm-sse` 的 content-type 判定受益；R-04/R-11 必须改写。
- **不包含的影响面**：响应面保持 6 项；F-16 与 phase 4 按现判据收口；下游只能靠状态码与 body 自行判断，`ct-llm-sse` 需要另找 content-type 来源；R-04/R-11 不动。
- **影响 phase/feature**：phase 4、phase 10；F-16（＋新增候选 feature）。**不进入 phase 1。**

### S-02 `method()`

- **提案要求什么**：链式入口 `.method("GET"|"POST"|…)`，可自定义方法。
- **与 requirements 哪条冲突**：R-10 非目标「不提供 GET/PUT/DELETE 或自定义 method」，现行只保留 `post()`。
- **包含的影响面**：F-13/F-15 增加方法字段与 `CURLOPT_CUSTOMREQUEST`/`HTTPGET` 分支；HEAD/GET 的重定向语义要单独定；phase 1 的入口从 `.post()` 改为 `.method()`；R-10 必须改写；如果同时保留 `.post()`，要定义二者优先级。
- **不包含的影响面**：入口固定 `.post()`；变量落点形式可以后续加 `.get()` 等专用方法而不动链；R-10 不动；phase 1 的冻结面最小。
- **影响 phase/feature**：phase 1（入口命名）、phase 3（配置）、phase 10（文档）；F-13、F-15。**phase 1 按「不含」施工。**

### S-03 默认 `max_redirects(10)`

- **提案要求什么**：新增 `max_redirects(n)`，默认 10。
- **与 requirements 哪条冲突**：R-08 非目标「不设自定义跳转上限（沿用 libcurl 默认）」。
- **包含的影响面**：F-15 增加 setter；F-08/F-09 的跟随路径增加跳数上限与 10/11 跳夹具；phase 9 扩项；R-08 非目标必须改写；libcurl 默认值不再是契约。
- **不包含的影响面**：沿用 libcurl 默认上限；R-08 不动；phase 9 只做凭据剥离与降级拒绝；11 跳链的行为由 libcurl 决定，夹具不写死。
- **影响 phase/feature**：phase 3、phase 9；F-15、F-08、F-09。**不进入 phase 1。**

### S-04 跨线程取消

- **提案要求什么**：`cancel()` 可从另一线程调用，`atomic int` 保护标志位。
- **与 requirements 哪条冲突**：R-13 非目标「不支持跨线程取消、异步取消、信号触发」；R-02 非目标「不做跨线程共享句柄」；Q-12 线程口径未定。
- **包含的影响面**：F-11 走方向 B；F-19 扩跨线程；phase 2 生命周期、phase 8 取消都要重排；需要 C 侧 pthread 夹具与 TSAN；R-13/Q-12 必须改写；CT 标准库无线程封装，纯 CT 判据不可行。
- **不包含的影响面**：F-11 走方向 A；`cancel()` 只在同线程回调内；phase 8 可在纯 CT 内收口；无 TSAN；R-13 非目标保持。
- **影响 phase/feature**：phase 2、phase 8；F-10、F-11、F-19。**不进入 phase 1。**

### S-05 `HTTP_ERR_*` 枚举

- **提案要求什么**：公开 12 个错误码宏/枚举，按语义分类。
- **与 requirements 哪条冲突**：R-14 要求 `error_code` 等于原始 `CURLcode`（DNS 6、拒绝 7、超时 28、TLS 60、中止 42、写 23），未知码保留原值；枚举化会改写这套数值。
- **包含的影响面**：F-17 增加 `error_kind` 或完整映射表；本地错误空间与传输码分离要写死；phase 5 扩项；R-14/M-09/Q-05 必须改写；F-02 的三超时归类可以直接落在 `error_kind` 上。
- **不包含的影响面**：`error_code` 保留 `CURLcode` 原值；本地参数错误用负数码空间并文档化；未知码透传；R-14 不动；F-02 用 `error_detail` 的 `phase=` 区分。
- **影响 phase/feature**：phase 5；F-17、F-02。**不进入 phase 1。**

## 4. 与 requirements 的对账（按新形态）

> 只读结论；`docs/requirements.md` 本轮不改，等用户裁定后再改。分三档：天然满足 / 判据可保留但措辞需改 / 必须改判据。

### 4.1 天然满足（新形态下无需改判据）

| R-* | 理由 |
| --- | --- |
| R-01 包清单与安装面 | `ct.pkg` 的 `name/version/licence/ldflags/requires` 语义不变；`.m.ct` + shim 是既有安装路径。 |
| R-07 User-Agent | 仍是 `curl_easy_setopt(USERAGENT)`；默认值写死即可断言。 |
| R-15 C shim 单一边界 | 新形态更严：`.m.ct` recipe 只调 `http_*`，`curl_*` 只在 `.shim.c/.h`。仅需把 grep 目标从 `src/*.ct src/*.h.ct` 扩到 `src/*.m.ct`（判据措辞）。 |
| R-17 平台与系统 libcurl | 环境判据不变；`curl_version()` 改为 namespace 宿主映射入口。 |
| R-19 发布材料与安装验证 | 版本落点与 `ct get/install` 判据不变；shim 文件需随包安装，属打包检查。 |

### 4.2 判据可保留、措辞或入口需改

| R-* | 要改什么 | 判据本身 |
| --- | --- | --- |
| R-03 URL 所有权 | `url(u)` 改为链起手字符串；深拷贝发生在 factory 返回前 | 「覆写源缓冲区后仍收到原 URL」保留 |
| R-04 请求头 | `header()` 返回 `CRequest *`；Q-10 从「待确认」收紧为「拒绝 CR/LF」 | slist 顺序、重复名、泄漏判据保留 |
| R-05 请求体 | `body()` 返回 `CRequest *`；Q-02 定为深拷贝；补 `POSTFIELDSIZE` | 二进制 NUL、`len=0`、负长度判据保留 |
| R-09 TLS | `insecure(1)` 必须同时关 `VERIFYPEER` 与 `VERIFYHOST` | 自签失败、`insecure` 后 200、SAN 不匹配判据保留 |
| R-12 流式 | `post_stream` 改名 `send_stream`；回调 typedef 在 shim 头；只收命名函数 | 分片、拼接、回调中止、互斥判据保留 |
| R-16 内存路径 | `delete` 改为 `http_request_free` / `http_response_free`；二次释放由置空保证 | 六路径 ASAN/LSAN 判据保留 |

### 4.3 必须改判据（新形态下原判据不可执行或语义已变）

| R-* | 冲突点 | 建议改法（待裁定） |
| --- | --- | --- |
| R-02 构造与释放 | `HttpRequest.new()` / `delete` / `Drop` 不再是公开形态 | 构造改为 `"url".post()`；释放改为显式 free；「二次 delete」改为「二次 free 安全」 |
| R-06 超时 | 现行只有总超时/连接超时，且三者都返回 28 | 补第三种空闲超时；错误归类用 `error_detail` 的 `phase=`；单位写死毫秒或秒 |
| R-08 重定向 | 未覆盖跨主机凭据、https→http 降级；S-03 改默认上限 | 补 F-08/F-09 判据；S-03 裁定后改「不设上限」非目标 |
| R-10 `post()` | 执行入口改为 `.send()`，配置面改为链式 | 判据保留 200/404/500，入口统一改名；R-10 非目标随 S-02 调整 |
| R-11 响应读取面 | 显式 free、`take_body`、谓词真值表、S-01 响应头都会改公开面 | 先冻结 6 项读取面；`take_body` 与谓词按 F-04/F-05 写；S-01 纳入时再加 |
| R-13 取消 | 同线程取消点原先未定义，跨线程是 S-04 | 定义为「仅回调内、同线程」；S-04 纳入时改 |
| R-14 错误映射 | 本地错误码空间未定；S-05 枚举与原始码冲突 | 本地用负数码空间；`error_code` 保留原值；S-05 决定是否加 `error_kind` |
| R-18 门禁与文档 | README 现在写「链不成立」，与新形态相反 | 保留「impl-chain 必须被 g4 拒绝」；新增「三段静态映射链必须编译通过」正例 |

### 4.4 M-01…M-09 在新形态下的状态

| 编号 | 原结论 | 新形态下的状态 |
| --- | --- | --- |
| M-01 | 单表达式链不成立，一条语句一个方法 | **部分失效**：`impl` 链仍不成立；类型宿主静态映射链成立。需把结论收窄到 `impl`。 |
| M-02 | `HttpRequest::new()` 不成立，改 `HttpRequest.new()` | **失效**：构造改为 `char *` factory，不再有 `new()`。 |
| M-03 | `src/` 与 namespace 二选一 | 已收口：2026-11-15 采用包根 `http/`（PH-10，HTTP-2 选①）。 |
| M-04 | 配置方法计数为 8 | **需重数**：去掉 `url()`，增加空闲超时/上限类方法，数量变化。 |
| M-05 | `body()` 深拷贝还是借用未定 | **定为深拷贝**（F-01/F-15）。 |
| M-06 | `HttpResponse *` 谁释放未定 | **定为显式 `http_response_free`**（F-10）。 |
| M-07 | `cancel()` 触发点不可达 | **部分解决**：同线程 chunk/进度回调内可触发；跨线程仍属 S-04。 |
| M-08 | 是否暴露耗时未定 | 仍开放，不进入 0.1.0 读取面（除非另行裁定）。 |
| M-09 | 本地错误码空间未定 | **定为负数码空间**；S-05 决定是否再加枚举。 |

## 5. 拆分原则

- 任何 feature 若落地时发现需要跨两个 phase 或两个独立目的，按 AGENTS T-1/T-2 退回重切，不把 L 塞进一张卡。
- 阻塞项 F-01…F-11 各自一对红/绿提交；不得用「整个 phase 门禁绿」替代单项验收。
- 待裁定项 S-01…S-05 在裁定前只出现在本文件与 roadmap 的 gated 标注里，不写入任何 phase 卡的任务行。
