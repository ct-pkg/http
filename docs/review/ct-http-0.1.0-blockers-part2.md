# ct-http 0.1.0 阻塞项决策材料（续 2）

- 本篇接 [ct-http-0.1.0-blockers.md](ct-http-0.1.0-blockers.md) §2，含 §2–§4。
- 拆分：2026-11-15 按小节拆分入库，正文逐字未改；同内容副本见 `docs/reviews/spec-review/`。

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

