> 来源说明（2026-11-15 入库）：本文为原始草稿第一部分（§0–§1），正文未改；原件按小节拆为
> 本篇、`ct-http-0.1.0-blockers-part2.md`（§2–§4）、`ct-http-0.1.0-blockers-part3.md`（§5–§9）。
> `docs/reviews/spec-review/` 下有同内容副本；除本来源说明外，三段正文拼接与拆分前原件逐字一致（sha256 已核）。

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

