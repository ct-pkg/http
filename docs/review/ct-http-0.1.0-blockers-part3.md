# ct-http 0.1.0 阻塞项决策材料（续 3）

- 本篇接 [ct-http-0.1.0-blockers.md](ct-http-0.1.0-blockers.md) §5，含 §5–§9。
- 拆分：2026-11-15 按小节拆分入库，正文逐字未改；同内容副本见 `docs/reviews/spec-review/`。

## §5 附带发现的编译器隐患

trait 方法返回接口值时，实现体生成：

```c
return (struct HttpRequest_iface){ (void *)&_self, &HttpRequestImpl_HttpRequest_vtable };
```

`_self` 是被调函数内的局部 `void *` 副本（`self` 的强转目标），`&_self` 指向该栈帧。返回值一旦被消费，指针指向已返回的栈帧。

同一份生成物里的 vtable 槽本身合法：

```c
struct HttpRequest_iface (*header)(void *, char *, char *);
```

复现：

```sh
ct /tmp/ctprobe/ok1.ct -o /tmp/ctprobe/ok1.c   # rc=0
grep -nF "&_self" /tmp/ctprobe/ok1.c
# 63:    return (struct HttpRequest_iface){ (void *)&_self, &HttpRequestImpl_HttpRequest_vtable };
```

**结论（留白）：**

1. 该隐患是否单独立项？
2. 在 B-1 修复前，是否禁止接口方法返回接口值？
3. 是否需要在 `ct check` 增加对应诊断？
4. 该隐患是否影响 0.1.0 的发版判断？

---

## §6 API 语义问题（与编译器无关）

来自 ct-http 代码与设计文档的审查记录；本轮未逐条实测，机制如下（来源见 §9）。

| # | 问题 | 机制 | 结论（留白） |
| --- | --- | --- | --- |
| 6.1 | `send()` 自动释放 + 跨线程 `cancel()` 的 UAF | `send()` 成功后释放句柄；另一线程持有同一指针调用 `cancel()` 时对象已释放 | |
| 6.2 | 跨单元 `delete` 不执行 Drop 体 | 消费单元的 `delete` 只释放内存；`Drop` 里的 `curl_easy_cleanup` 不执行，必须显式 `free()` | |
| 6.3 | 三种超时都只返回 `CURLE_OPERATION_TIMEDOUT(28)` | 连接、首字节、总时长三类超时都归一到 28，调用方无法区分 | |
| 6.4 | `LOW_SPEED_LIMIT=1` 误杀慢流 | 每秒 1 字节的低速阈值会把正常慢速响应判为超时 | |
| 6.5 | `COPYPOSTFIELDS` 漏 `POSTFIELDSIZE` | 未设置长度时 libcurl 按 `strlen` 处理，body 中的 `\0` 截断 | |
| 6.6 | `take_body` 后 Drop 双释放 | body 所有权已交出，`Drop` 仍按原指针释放 | |
| 6.7 | `is_ok` 与 `is_cancelled` 可同时为真 | 两个谓词的状态来源不同，未互斥 | |
| 6.8 | `header()` 不拒 CR/LF | 名称/值未做换行校验，可注入额外头 | |
| 6.9 | 无 scheme 限制 | 接受 `file:///etc/passwd` 等非 HTTP scheme | |
| 6.10 | 重定向跨主机转发 `Authorization`、https→http 降级 | 重定向时未按主机/协议过滤敏感头，也未阻止降级 | |

**结论（留白）：**

1. 哪些项进入 0.1.0 的必修范围？
2. 6.2 是改设计规避，还是等编译器支持？
3. 6.10 是否按 libcurl 的 `CURLOPT_UNRESTRICTED_AUTH` 语义对齐？
4. 这些项是否需要有独立的安全评审？

---

## §7 范围变更冲突

提案新增 5 项，与 requirements 的既有边界冲突。逐条：

| # | 提案要求 | 需求要求 | 冲突点 | 谁让谁（留白） |
| --- | --- | --- | --- | --- |
| 7.1 | 响应头读取：`header(name)` / `header_all()` | R-04 把「响应头读取」列为 non-goal | 新增公开 API 与 non-goal 直接相反 | |
| 7.2 | 新增 `method()` | R-10 把「GET/PUT/DELETE 或自定义 method」列为 non-goal，只保留 `post()` | 新增 API 与 non-goal 直接相反 | |
| 7.3 | 默认 `max_redirects(10)` | R-08 要求「不设自定义跳转上限，沿用 libcurl 默认」 | 默认值从 libcurl 默认改为 10 | |
| 7.4 | 跨线程取消 | R-13 把「不支持跨线程取消」列为 non-goal | 能力与 non-goal 直接相反 | |
| 7.5 | `HTTP_ERR_*` 枚举 | R-14 要求错误映射保留原始 `CURLcode` 表，并保留未知码 | 枚举与「保留原始码 + 未知码透传」的语义不完全重合 | |

**结论（留白）：**

1. 5 项是否整体进入 0.1.0？
2. 若进入，R-04/R-08/R-10/R-13 的 non-goal 是否同步改写？
3. 7.5 的枚举是否保留原始 `CURLcode` 字段？
4. 范围变更是否触发版本号调整？

---

## §8 汇总表

| 阻塞项 | 影响什么用法 | 是否可绕 | 绕的代价 |
| --- | --- | --- | --- |
| §2-A 类型宿主映射的单元限制 | `"url".method("GET")` 这一跳 | 可绕开「字符串起手」，不可绕出同一形态 | 改用 `.h.ct` 自由函数工厂：丢掉字符串起手；调用点写成 `http::http_entry("url","GET")` |
| §3-B 接口方法返回接口值不能装箱 | `HttpResponse res = req.send();` 与 `.send().status()` | 可绕开「拿响应接口」 | `send()` 返回值只能丢弃；改返回 struct 指针则使用者接触具体类型，失去响应侧多实现 |
| §4-C 临时值不能作接收者 | 一行链 `...header(...).timeout(...)` | 可绕开「一行」 | 每跳一条语句，逐跳落变量；方法返回值不再有链式用途 |
| §5 生成 `&_self` 悬垂 | 任何接口方法返回接口值并被消费 | 不可绕（一旦消费即悬垂） | 在修复前不使用「接口方法返回接口值」的返回值 |
| §6 API 语义问题 | 资源生命周期、超时区分、body 完整性、头注入、scheme、重定向安全 | 部分可在设计层绕开 | 见 §6 各行；6.2 依赖显式 `free()`，6.6 需改所有权约定 |

**结论（留白）：**

1. 0.1.0 是否以「三处阻塞全解」为发版条件？
2. 若只解 §2，产品形态是什么？
3. 若只解 §3，产品形态是什么？
4. 是否接受 0.1.0 只提供逐跳落变量的 API？

---

## §9 证据分级

| 节 | 实测证据 | 文档/代码审查（本轮未实测） |
| --- | --- | --- |
| §1 | 7 条（1.1–1.7，均含命令与 rc） | 0 |
| §2 | 6 条（q1、q1b、p1 两种 import 拼写、p1b、q4、q5） | 「不存在同时满足的位置」由这 5 条排除得出，属实测归纳 |
| §3 | 4 条（q2、q2b、fail1、p3b；含 vtable 槽与生成 C） | 0 |
| §4 | 3 条（fail5、fail6、fail3）+ 1 条代理测量（p3b） | 0 |
| §5 | 1 条（ok1 生成 C 片段） | 0 |
| §6 | 0 | 10 条（来自 ct-http 代码与设计/评审记录的既有结论） |
| §7 | 0 | 5 条（提案与 requirements 文本对照） |
| §8 | 汇总自 §2–§6 | 0 |

§6、§7 无命令与退出码：它们不是编译器行为，没有可执行的判定入口；来源为 ct-http 仓库内的设计文档与评审记录。
