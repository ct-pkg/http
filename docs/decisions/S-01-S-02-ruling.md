# S-01 / S-02 需求层裁定（2026-11-15）

- applies-to: 0.2.0
- 性质：**纯文档**。不占相、不 bump 版本、不打 tag、不改实现（V-4：纯文档不 +PATCH）。
- 签字：负责人 2026-11-15（三条裁定，逐条见下）。
- 范围：把 `docs/planning/ct-http-0.1.0-features.md` §3 的 S-02 与 S-01 从「待裁定」改成「已裁定」，
  并改写 `docs/requirements.md` 的 R-04/R-10/R-11 非目标。**不**开卡、不写任务清单。
- 证据链：[PH-12 开卡 battle · 主张方](../reviews/PH-12-battle-open.md)、
  [PH-12 开卡 battle · 反方](../reviews/battle-ph12-s01-s02-con.md)；两篇为裁定前的事实核对与对辩，
  结论以本文件为准。

## 1. S-02 `method()`：新增入口，不动 `post()`

### 1.1 决定

- 新增 `CRequest *method(char *name)`，挂在 `RequestBuilder` 面上，返回接收者。
- `post()` 原样保留：它仍是字符串起手的工厂（`char * -> CRequest *`），行为不变。

### 1.2 为什么是「新增」而不是「替换」

现有 `post()` 只能发 POST，**发不出 GET/HEAD**——这是能力缺口，不是命名不顺手。
`method()` 填补该缺口；把 `post()` 改名或替换会改动已发布 `0.2.0` 的公开面，
属破坏性变更（要 MAJOR 由用户裁定），与本轮「新增」的裁定相反。

### 1.3 共存语义（写死，不留待定）

1. **优先级**：`post()` 只在创建对象时把方法初始化为 `POST`（默认值）；`method()` 是该对象上的
   设置器，后调用覆盖先调用。不存在「`post()` 覆盖 `method()`」的路径——`post()` 没有
   `CRequest *post()` 形态，不能在同一对象上二次调用。
   判据：`url->post()->method("GET")->send()` 发出 `GET`；`url->post()->send()` 发出 `POST`；
   连续 `method("GET")->method("HEAD")` 以 `HEAD` 发出（后写入胜出）。
2. **空串**：`method("")` **不算**回到 POST，按本地参数错误处理（负数码，具体码在实现相定），
   请求不发出、服务端计数 0。要回 POST 必须显式 `method("POST")`。
   理由：空方法名不是合法 HTTP token；隐式回退会把调用方 bug 变成静默成功，和 R-04
   「不得静默改写」的既有口径一致。
3. **非法字符**：含空格、CR、LF 或其他非 token 字节 → 同样本地负码，请求不发出
   （防请求行注入，与 R-04 的 CR/LF 拒绝同口径）。
4. **大小写**：方法名按传入字节原样发出，不做自动大写、不做归一；判据用 `GET`/`POST`/`HEAD`。

### 1.4 非目标

- 不替换、不改名、不删除 `post()`；不新增 `get()` / `head()` 等专用入口。
- 不改 `send()` / `post_stream()` 签名；不做并发请求；不做重试。

### 1.5 影响面与依赖

- 需求：R-10 非目标改写（见 §3）。feature：F-13/F-15 增加方法字段与 setter。
- CT 依赖：无新增。shim 走 `CURLOPT_CUSTOMREQUEST`（现有 `curl_easy_setopt` 通道），
  对 `POST` 保持现有 `CURLOPT_POST`/`POSTFIELDSIZE` 路径。

## 2. S-01：收窄为 `content_type()` 窄接口

### 2.1 决定

`HttpResponse` 只新增一个只读查询 `char *content_type()`；**不提供** `header(name)` / `header_all()`。

### 2.2 契约

1. **只看最终响应**：重定向链里只取最后一个响应，与 `status()` 同源。
2. **缺失返回 `NULL`**；存在则返回完整头值（不裁剪、不解析 `charset` 等参数）。
3. **头名匹配 ASCII 大小写不敏感**：`Content-Type` 与 `content-type` 等价。
4. **借用指针**：有效期到该响应对象 `free()`；调用方不得释放或改写。
5. 不改 `body()` / `take_body()` / 流式接口的任何语义。

### 2.3 非目标（逐条对应规格评审问题）

| 编号 | 问题 | 本裁定的处理 |
| --- | --- | --- |
| B-4 | 中间响应头与最终响应头混在同一查询面 | 非目标：不提供多响应维度查询；`content_type()` 只读最终响应 |
| B-6 | 响应头名大小写未归一 | 非目标：只对 `Content-Type` 这一个头名做大小写不敏感匹配，不承诺通用头名归一，也不承诺保留原始拼写 |
| B-7 | `header_all` 生命周期/顺序/重复项未定义 | 非目标：不定义 `header_all`；`content_type()` 是借用只读指针，单值 |
| B-8 | 响应头没有上限 | 非目标：不新增头预算/超限错误码；`content_type()` 不缓存全部头 |
| F-3 | 与 R-04/R-11 非目标正面冲突 | 按本裁定改写 R-04/R-11 非目标（§3），只在改写范围内开放这一个查询 |

### 2.4 影响面与依赖

- 需求：R-11 读取面增加 1 项（6 → 7），R-04 非目标改写。feature：F-16。
- 夹具（实现相落）：① 全小写 `content-type` 能被查到；② 302→200 只取最终响应的类型；
  ③ 缺失时返回 `NULL`。
- CT 依赖：`CURLINFO_CONTENT_TYPE` 经 `curl_easy_getinfo` shim——已支持。

## 3. 三处非目标的改写

| 条目 | 改写后的非目标 |
| --- | --- |
| R-04 | 不提供通用响应头读取（`header()`/`header_all()`）；响应侧只提供 R-11 的 `content_type()` 窄接口；不做默认头或自动 Content-Type |
| R-10 | 不提供 `get()`/`put()`/`delete()` 专用入口；非 POST 方法经 `method()` 新增入口发出；不做并发请求 |
| R-11 | 不读通用响应头、不提供 cookies、不做 body 增量读取（`post_stream` 另条）；`content_type()` 是读取面唯一新增项 |

旧文原文逐字保留在本文件 §7，未删除任何规则、未降低任何阈值。

## 4. roadmap 回写

- 已在 `docs/planning/ct-http-0.1.0-roadmap.md` 追加 §5「下一相（已裁定，待开卡）」：
  只登记「要开的一相做 S-02 + `content_type()`」。
- **相位号与版本号不在本裁定内确定**：开卡时按 `docs/phases/PH-<n>/` 与 `AGENTS.md` 6.7 V-1
  （`deviation: V-001` 的 `d = 9`）另算。本裁定不 bump、不打 tag。

## 5. 不在本裁定内的项

- S-03：已按「固定 10 次、不开放配置」实现（README「限制」段），无需再裁。
- S-04 跨线程取消：等 CT 侧 `_Atomic`/c11 口径（当前 `ct build` 仍以 `-std=c99` 调 cc）。
- S-05 `HTTP_ERR_*` 枚举：未裁定；若采用 `Type::Member` 取值形态，受 CT PH-57（LANG-15）限制。
- B-5（头回调签名）等只属于完整 `header_all()` 方案，本裁定不涉及。

## 6. 下一步

实现相开卡时需重新冻结任务与判据，至少覆盖：
`method()` 的默认/覆盖/空串/非法字符四条判据，`content_type()` 的三条夹具，
以及「`post()` 行为零变化」的回归判据。开卡前不写卡、不改代码。

## 7. 附：改写前原文（逐字）

- R-04 非目标：`不提供响应头读取（读取面只列状态码/body/错误/字节数）；不做默认头或自动 Content-Type。`
- R-10 非目标：`不提供 GET/PUT/DELETE 或自定义 method；不做并发请求。`
- R-11 非目标：`不读响应头、不提供 cookies、不做 body 增量读取（post_stream 另条）。`
