# ct-http 包规范草案 · 反证评审（续 3）

- 本篇接 [spec-rebuttal.md](spec-rebuttal.md) §9，含 §9–§12。
- 归档：2026-11-15 拆分归位，正文未改。

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
