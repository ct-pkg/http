> 来源说明（2026-11-15 入库）：本文为原始草稿（561 行），正文未改。副本按小节边界拆为
> `docs/reviews/spec-review/ct-http-0.1.0-blockers.md`（§0–§1）、`-part2.md`、`-part3.md`；
> 副本只加了归档与接续说明，去掉这些说明后与原件正文一致。

# ct-http 0.1.0 阻塞项决策材料

> 读者：外部评审者。本文只陈述事实与选项，不给出推荐，所有「结论」栏留白。
> 文中每个判断都附命令与退出码；命令均在 CT v0.44.1 上执行，探针文件放在 `/tmp/ctprobe*`，不属于仓库。

---

## §0 背景

### 0.1 环境

| 项 | 值 |
| --- | --- |
| 编译器 | `ct v0.44.1`（`ct -v` 输出 `ct v0.44.1`） |
| 平台 | macOS arm64，系统 `cc`（Apple clang） |
| 探针位置 | `/tmp/ctprobe`、`/tmp/ctprobe2` |
| 构建命令 | `ct build <dir>/src/main.ct -o <dir>/out`（内部先生成 C，再调 `cc`） |

### 0.2 CT 的三种源文件单元

外部评审者需要先知道这三个概念，否则后文无法读：

| 单元 | 用途 | 关键规则（0.44.1 实测） |
| --- | --- | --- |
| `.ct` | 普通源文件，可含 `trait` / `struct` / `impl` / `main` | 不能被 `import` |
| `.h.ct` | 接口单元，发布 `pub` 声明 | 只有它能被 `import`；可含 `trait` / `struct` / `impl` |
| `.m.ct` | 映射单元，专门承载 `map` 绑定 | 只有它能写 `type host` 映射；其中的 `trait` 是「静态 trait」 |

`type host`（类型宿主）映射的写法：

```c
map trait Entry for type char * {
    method(s, verb) -> http_entry_new(s, verb);
}
```

作用是让 `"字符串".method("GET")` 这种写法在编译期展开成一次裸 C 调用。

CT 的「接口值」是胖指针：

```c
struct HttpRequest_iface { void *self; struct HttpRequest_vtbl *vt; };
```

把实现了 `HttpRequest` 的 struct 赋给 `HttpRequest` 类型的变量，叫装箱。

### 0.3 ct-http 想要的使用者形态

一句话链式：

```c
int status = "https://api.example.com/users"
                 .method("GET")                       /* 字符串起手，工厂 */
                 .header("Accept", "application/json")
                 .timeout(5000)
                 .send()                              /* 收口，拿响应 */
                 .status();
```

拆开是四段：起点是字符串，不是构造器；工厂产出由 trait 定义的对象类型，不是 struct；配置方法返回同一接口类型；收口拿到响应接口。

### 0.4 本文要回答的问题

在 CT 0.44.1 上，0.3 的形态能不能成立；不成立的话卡在哪几处；每一处有哪些候选出路及其代价。

**结论（留白）：**

1. 该形态在 0.44.1 上是否成立？
2. 若成立，需要几条前提？
3. 若不成立，阻塞项共几个？
4. 是否接受「逐跳落变量」的降级形态？

---

## §1 已成立的能力清单

本节全部为实测，命令统一为 `ct build … -o …`，`rc` 为 `ct` 进程退出码。

### 1.1 `.h.ct` 自由函数工厂返回接口值，落变量后单跳调用

```c
// http.h.ct
pub HttpRequest http_entry(char *url, char *verb) {
    HttpRequestImpl x;
    x.headers = 0;
    return x;
}
```

```c
// main.ct
HttpRequest r = http::http_entry("https://api.example.com/users", "GET");
r.header("Accept", "application/json");
return r.send();
```

```sh
ct build /tmp/ctprobe/p3/src/main.ct -o /tmp/ctprobe/p3/out
# rc=0；运行输出 201
```

### 1.2 接口值落变量后，接口方法单跳调用

```c
HttpRequest r = x;
r.header("Accept", "application/json");     /* 语句位置 */
```

```sh
ct build /tmp/ctprobe/pA1/src/main.ct -o /tmp/ctprobe/pA1/out
# rc=0
```

### 1.3 接口值指针 + `->` 连续调用

```c
HttpRequest r = x;
HttpRequest *rp = &r;
rp->header("Accept", "application/json");
return rp->send();
```

```sh
ct /tmp/ctprobe/ok5.ct -o /tmp/ctprobe/ok5.c   # rc=0
cc -std=c99 /tmp/ctprobe/ok5.c -o /tmp/ctprobe/ok5.bin   # rc=0
/tmp/ctprobe/ok5.bin                                     # rc=0，输出 201
```

### 1.4 trait 方法返回 struct 值

```c
trait StructView { Payload pick(int n); }
Payload p = x.pick(3);
```

```sh
ct /tmp/ctprobe/fail2.ct -o /tmp/ctprobe/fail2.c   # rc=0
cc -std=c99 /tmp/ctprobe/fail2.c -o /tmp/ctprobe/fail2.bin   # rc=0
```

生成的 vtable 槽：`struct Payload (*pick)(void *, int);`（带 `struct` 标签）。

### 1.5 trait 方法返回 struct 指针，落变量后单跳调用

```c
struct Payload *pp = h.locate(3);
```

```sh
ct /tmp/ctprobe/ok3.ct -o /tmp/ctprobe/ok3.c   # rc=0
cc -std=c99 /tmp/ctprobe/ok3.c -o /tmp/ctprobe/ok3.bin   # rc=0
```

### 1.6 trait 方法返回宿主裸指针，落变量后单跳调用

```c
struct HttpRequestImpl *hp = x.tap(1);
return hp->send();
```

```sh
ct /tmp/ctprobe/ok4.ct -o /tmp/ctprobe/ok4.c   # rc=0
cc -std=c99 /tmp/ctprobe/ok4.c -o /tmp/ctprobe/ok4.bin   # rc=0
/tmp/ctprobe/ok4.bin                                     # rc=0，输出 201
```

### 1.7 四行分写的前三行可执行

```c
HttpRequest req = http::http_entry("https://api.example.com/users", "GET");
req.header("A", "B");
req.timeout(5000);
```

```sh
ct build /tmp/ctprobe2/q2b/src/main.ct -o /tmp/ctprobe2/q2b/out
# rc=0
[shim] GET https://api.example.com/users
[shim] header A: B
[shim] timeout 5000
[shim] send
done
# run rc=0
```

第四行 `HttpResponse res = req.send();` 见 §3。

**结论（留白）：**

1. 这些能力是否足以支撑一个非链式的 HTTP 客户端 API？
2. 1.1 的自由函数工厂是否满足「字符串起手」？
3. 1.3 的接口指针形态是否要进入公开 API？
4. 1.4/1.5 的 struct 返回值是否要进入公开 API？

---

## §2 阻塞 A：类型宿主映射的单元限制

### 2.1 现象一：映射写进 `.h.ct` 被拒

同一份 `http.h.ct` 内先定义接口，再写映射：

```c
pub trait Entry { HttpRequest method(char *verb); }

map trait Entry for type char * {
    method(s, verb) -> http_entry_new(s, verb);
}
```

两种消费写法都未到达 `main.ct`：

```sh
ct build /tmp/ctprobe2/q1/src/main.ct -o /tmp/ctprobe2/q1/out
# rc=1
```

```text
/tmp/ctprobe2/q1/src/http/http.h.ct:39:21: error: a type host is only allowed in a .m.ct unit
```

一行链写法 `/tmp/ctprobe2/q1b/src/main.ct` 的退出码与报错原文完全相同（rc=1，同一文件同一位置）。

### 2.2 现象二：映射写进 `.m.ct` 后看不见接口

`api.m.ct`：

```c
#include "entry.shim.h"
import http::http;
namespace api;
pub trait Entry { HttpRequest method(char *verb); }
map trait Entry for type char * { method(s, verb) -> http_entry_new(s, verb); }
```

```sh
ct build /tmp/ctprobe/p1/src/main.ct -o /tmp/ctprobe/p1/out
# rc=1
```

```text
ctc: cannot resolve import 'http/http'
```

换成 `import http;`：rc=1，原文 `ctc: cannot resolve import 'http'`。

不写 import、只让返回类型保持 `HttpRequest` 时，映射单元能过，消费单元在调用点失败：

```text
/tmp/ctprobe/p1b/src/main.ct:9:68: error: cannot box this expression
```

### 2.3 合起来的后果

能放 `map … for type` 的单元（`.m.ct`）看不见接口；能看见接口的单元（`.h.ct`）不许放映射。`"字符串".method(...)` 这一跳没有可放置的位置。

### 2.4 候选解法（不做推荐）

| 候选 | 做法 | 代价（事实） |
| --- | --- | --- |
| A-1 | 改编译器，允许 `.m.ct` import `.h.ct` | 映射单元要跨单元解析接口类型与 `use`/`impl` 可见性；构建图的单元种类约束、缓存键、`.m.ct` 的加载顺序都要改；现有「只能导入另一个 `.m.ct` 单元」类诊断要重定义 |
| A-2 | 允许 `.h.ct` 承载类型宿主 | `.h.ct` 目前没有 recipe 展开路径；生成的头/实现分离、shim 的 `-I`、`--emit-map` 的宿主条目、消费单元生成 C 的顺序都要扩；`a type host is only allowed in a .m.ct unit` 要删除或缩窄 |
| A-3 | 把接口定义搬到同时满足两个约束的位置 | **不存在这种位置**：`.m.ct` 里的 trait 是静态 trait，不能作接口类型（见 2.5）；普通 `.ct` 不能被 import；`.h.ct` 不许放映射 |

### 2.5 候选 A-3 的排除证据

接口 trait 写进 `.m.ct` 并在消费单元当类型用：

```sh
ct build /tmp/ctprobe2/q4/src/main.ct -o /tmp/ctprobe2/q4/out
# rc=1
```

```text
/tmp/ctprobe2/q4/src/api/api.m.ct:6:5: error: a static trait has no interface value
```

接口写在普通 `.ct` 单元并尝试 import：

```sh
ct build /tmp/ctprobe2/q5/src/main.ct -o /tmp/ctprobe2/q5/out
# rc=1
```

```text
ctc: only a .h.ct unit can be imported: rename 'plain/plain.ct' to 'plain/plain.h.ct'
```

**结论（留白）：**

1. A-1 与 A-2 是否都进入 0.1.0 的范围？
2. 若都不进入，`"字符串起手"` 是否放弃？
3. 编译器改动的最低可接受范围是什么？
4. 是否接受在 0.1.0 内保留 A-3「不存在」的结论？

---

## §3 阻塞 B：接口方法返回接口值不能装箱

### 3.1 现象

四行分写的第四行：

```c
HttpResponse res = req.send();
```

```sh
ct build /tmp/ctprobe2/q2/src/main.ct -o /tmp/ctprobe2/q2/out
# rc=1
```

```text
/tmp/ctprobe2/q2/src/main.ct:11:34: error: cannot infer the type to box
```

改成丢弃返回值的语句：

```c
req.send();
```

```sh
ct build /tmp/ctprobe2/q2b/src/main.ct -o /tmp/ctprobe2/q2b/out
# rc=0；运行 rc=0，输出见 1.7
```

### 3.2 同族方法也受影响

`header()` 返回 `HttpRequest` 本身，语句位置可以调用（§1.2），但把返回值装箱给一个变量就失败：

```text
/tmp/ctprobe/fail1.ct:29:60: error: cannot infer the type to box
```

### 3.3 对比：返回 struct / struct 指针是通的

- 返回 struct 值：§1.4，`ct` rc=0、`cc` rc=0。
- 返回 struct 指针：§1.5，`ct` rc=0、`cc` rc=0。
- 返回接口值：vtable 槽本身合法（`struct HttpRequest_iface (*header)(void *, char *, char *);`），但调用点无法把返回值装箱。

### 3.4 后果

四行分写可以执行，但拿不到响应对象；`send()` 只能作为丢弃返回值的语句存在，`.status()` 无处可调。

### 3.5 候选（不做推荐）

| 候选 | 做法 | 代价（事实） |
| --- | --- | --- |
| B-1 | 修装箱推断，让接口方法的接口返回值可在声明初始化位置装箱 | 需要明确「盒子的目标类型」推断规则；同类报错还出现在自由函数调用结果作接收者（`/tmp/ctprobe/p3b/src/main.ct:7:114`，rc=1，`error: cannot infer the type to box`），规则改动会同时影响该路径 |
| B-2 | `send()` 返回 struct 指针，放弃响应接口 | 使用者拿到具体类型名；响应侧无法多实现；与 §0.3 的「收口拿响应接口」不一致 |
| B-3 | `send()` 走 out 参数或回调交付结果 | `send()` 签名不再是零参；调用点需要额外变量或闭包；链式收口在该跳断开 |

**结论（留白）：**

1. B-1 是否由编译器侧解决？
2. B-2 是否可接受为 0.1.0 的公开形态？
3. B-3 的 out 参数是否与响应接口共存？
4. `send()` 是否必须返回可消费的值？

---

## §4 阻塞 C：临时值不能作接收者

### 4.1 现象

方法调用的结果再作接收者：

```c
return x.tap(1)->send();
```

```sh
ct /tmp/ctprobe/fail5.ct -o /tmp/ctprobe/fail5.c
# rc=1
```

```text
/tmp/ctprobe/fail5.ct:39:1: error: method receiver must be a variable, an element, a call or a field chain
```

连续两跳：

```c
return x.tap(1)->tap(2)->send();
```

```sh
ct /tmp/ctprobe/fail6.ct -o /tmp/ctprobe/fail6.c
# rc=1
```

```text
/tmp/ctprobe/fail6.ct:39:1: error: a method call on a temporary is only allowed at the start of a statement or of an assignment
```

固有 `impl` 返回接口值后直接接调用：

```c
return x.tuned(1).send();
```

```sh
ct /tmp/ctprobe/fail3.ct -o /tmp/ctprobe/fail3.c
# rc=1
```

```text
/tmp/ctprobe/fail3.ct:35:1: error: method receiver must be a variable, an element, a call or a field chain
```

### 4.2 关于 `"url".method("GET").header(...)`

该表达式无法在本轮取得独立报错：§2 的单元限制先触发（`a type host is only allowed in a .m.ct unit`），编译不进入该表达式。可测的最近形态是自由函数工厂结果直接接方法：

```c
HttpRequest r = http::http_entry("https://api.example.com/users", "GET").header("Accept", "application/json");
```

```sh
ct build /tmp/ctprobe/p3b/src/main.ct -o /tmp/ctprobe/p3b/out
# rc=1
```

```text
/tmp/ctprobe/p3b/src/main.ct:7:114: error: cannot infer the type to box
```

### 4.3 后果

即使 §2、§3 都解决，一行链仍然写不成；每条语句最多一次方法调用，结果是逐跳落变量。

### 4.4 候选（不做推荐）

| 候选 | 做法 | 代价（事实） |
| --- | --- | --- |
| C-1 | 放宽接收者规则，允许方法调用结果继续作接收者 | 临时值的生命周期、接口装箱目标推断、生成 C 的临时变量提升规则都要扩；现有「临时值只能在语句/赋值起点」的诊断要重定义 |
| C-2 | 维持现状，把链式定位为多行写法 | §0.3 的一行链在 0.1.0 不出现；每条语句一次调用；文档与样例要按多行写 |

**结论（留白）：**

1. C-1 是否进入 0.1.0？
2. C-2 的多行写法是否满足产品目标？
3. 多行写法下 `header` / `timeout` 的返回值是否还需要？
4. 「链式」一词是否保留在 API 描述里？

---

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
