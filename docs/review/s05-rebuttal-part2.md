# S-05 反证席（续）：语言实测、成本与结论

- 本篇接 [s05-rebuttal.md](s05-rebuttal.md) §3。

## 4. `#define` 与 `pub enum` 的实测对照（CT 0.44.1）

统一命令形如：

```sh
/Users/samxx/.local/bin/ct build main.ct -o app --outdir build
```

### 4.1 `pub enum` 在 `.h.ct`

```text
$ ct build main.ct -o app --outdir build
'cc' -std=c99 -I 'build' 'build/main.c' 'build/Types.c' '-o' 'app'
rc=0
```

生成头 `build/Types.h`：

```c
enum types_HttpErr {
    types_HttpOk = 0,
    types_HttpErrArg = (-1001),
    types_HttpErrTimeout = (-1005)
};
```

消费单元 `build/main.c` 里出现 `#include "Types.h"`，取值运行 rc=0。
**结论：`pub enum` 成员进生成头，可跨单元使用；但名字被命名空间改写**
（本包 namespace 为 `http`，对应 `http_HTTP_ERR_*`，不是提案里的裸 `HTTP_ERR_*`）。

### 4.2 `#define` 在 `.h.ct`

```text
$ ct build main.ct -o app --outdir build
'cc' -std=c99 -I 'build' 'build/main.c' 'build/Errs.c' '-o' 'app'
main.ct:3:14: error: use of undeclared identifier 'HTTP_ERR_ARG'
1 error generated.
rc=1
```

生成头 `build/Errs.h` 只有 `pub enum ErrsDummy`；`#define` 落在 `build/Errs.c`：

```c
#define HTTP_ERR_ARG (-1001)
```

消费单元的生成 C 只包含 `Errs.h`，所以 C 编译器看不到宏。
**结论：`.h.ct` 里的 `#define` 不是公开面；与主张席 §1.2 的复测一致。**

### 4.3 但 `#define` 放在 `http.shim.h` 时可用

CT 消费单元的生成 C 会自动 `#include "http.shim.h"`（实测 `build/main.c` 第 3 行），
所以 shim 头里的宏对 CT 调用方可见：

```text
$ ct build main.ct -I <repo>/src -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
rc=0
$ ./app
ct-http/0.1.0 -1004
rc=0
```

**结论：若目标只是「调用方在 CT 源码里写 `HTTP_ERR_*` 这个名字」，不需要 `pub enum`；
`http.shim.h` 里的宏已经能做到，而且不需要第二个 import。** 代价是宏不受 CT 语义检查、
占用全局宏命名空间、没有命名空间隔离；但它没有 C 侧名字改写的问题。
主张席 §1.2 只测了 `.h.ct` 里的 `#define`，没有测 shim 头这条路径。

### 4.4 `pub enum` 不能放在 `.m.ct`

```text
$ ct build main.ct -I . -o app --outdir build
./api/api.m.ct:7:5: error: a .m.ct unit cannot declare a type
ctc: 1 error(s) generated
rc=1
```

本包公开面是 `.m.ct`（`docs/planning/ct-http-0.1.0-roadmap.md:26`），所以枚举只能放进
另一个 `.h.ct` 单元（如现有的 `Types.h.ct`），消费者要**同时 import 两个单元**。
主张席 §1.3 也测到了同一结论。

### 4.5 第二 import 陷阱与名字改写

只 `import api::api;`、使用 `Types.h.ct` 里的 `HTTP_ERR_ARG`：

```text
main.ct:5:39: error: use of undeclared identifier 'HTTP_ERR_ARG'
rc=1
```

同时 `import api::api;` 与 `import api::Types;`：

```text
rc=0
$ ./app
7 -1001
```

这正是 README 已警告的第一条接口陷阱（`README.md:117-121`：只 import 映射单元才有方法，
只 import 类型单元没有方法）。枚举把它扩大成第二份：**方法要一个 import，错误名要另一个 import，
少一个就在 C 编译期报未声明**。而 `http.shim.h` 的宏没有这个陷阱。

### 4.6 `-Wswitch`：唯一的枚举增量收益，也是追加成员的破坏点

探针 F（`/tmp/s05-probe/final/switch`）：

```text
3 成员 + 全覆盖 switch，-Wall -Wextra -Werror:
  build rc=0, run rc=0（输出 arg）
加第 4 个成员，消费单元一字未改:
  build rc=1
  error: enumeration value 'types_HttpErrBody' not handled in switch [-Werror,-Wswitch]
```

这证明两件事：①主张席的「穷举有牙」是真的（漏成员编译失败）；②**追加成员对采用者不是免费的**，
它会让已通过编译的消费单元在下一次构建时失败。主张席把 0.1.0 现在做列为低成本，
但没有把「追加成员对 `-Wswitch` 采用者的反应」算进去。

### 4.7 差异汇总

| 写法 | 生成头可见 | 跨单元可用 | C 侧名字 | 第二 import | `-Wswitch` | 实测 rc |
| --- | --- | --- | --- | --- | --- | --- |
| `pub enum` in `.h.ct` | 是 | 是 | 被 mangling（`http_HTTP_ERR_*`） | 需要 | 有（收窄后） | 0（A） |
| `#define` in `.h.ct` | 否 | 否 | 不出现 | — | 无 | 1（B1/B2） |
| `#define` in `.shim.h` | 不需要（自动 include） | 是（含下游包） | 原样 `HTTP_ERR_*` | 不需要 | 无 | 0（C） |
| `pub enum` in `.m.ct` | 不允许 | 不允许 | — | — | — | 1（D） |

这张表是本议题的事实基础：**「不纳入枚举」不等于「放弃命名常量」；枚举也不是唯一能让
`HTTP_ERR_*` 出现在 CT 源码里的手段。枚举的独有点只有 `-Wswitch`。**

## 5. 成本

### 5.1 改动面（窄版，按主张席 §5 的量级）

| 项 | 改动 | 量级 |
| --- | --- | --- |
| 新 `src/http/Errors.h.ct` | 6 成员 `pub enum HttpErr` | ~14 行 |
| `http.shim.h` | 响应尾字段 `local_error` + 访问器；`HTTP_LOCAL_ERR_NO_HANDLE` | ~4 行 |
| `http.shim.c` | 抄 `local_error` 进响应；`easy==NULL` 记 `-1006`；访问器 | ~6 行 |
| `http.m.ct` | `local_error()` + map recipe | ~2 行 |
| g3 一致性断言 | 逐成员断言 `HTTP_LOCAL_ERR_* == HTTP_ERR_*` | ~8 行 |
| 新夹具 | `p5-local-err.ct` + expected + run.sh/check.sh 接线 | ~30 行 |
| README | 错误面段落、删待裁定项 | ~10 行 |
| 合计 | 6 个既有文件 + 1 新单元 + 1 新夹具 | **~90 行** |

90 行本身不是问题；问题是它换来的公共契约：第二个 import、两套编号（shim 宏 + 枚举）、
两个错误轴（`local_error` 与 `error_code`），以及 §4.6 的追加破坏。

### 5.2 ABI

- 只加枚举常量：无符号、无布局、无 ABI 影响。
- 加 `local_error()` 访问器：新增 shim 导出符号，属追加；C 消费者重链即可。
- 把 `local_error` 存进 `struct HttpResponse`：**改结构体布局**。`http.shim.h:56-61` 是公开 C 布局，
  PH-3 夹具直接读 `local_error` / `headers`（`test/fixtures/p3-header-crlf.c:19-25`）；
  尾部追加源码兼容，但所有 C 消费者与夹具要重编。0.1.0 未发布，不触发 MAJOR，但这是实打实的改动面。

### 5.3 是否要在 PH-5 之前先改需求文档

- 窄版：**要**。Q-05 不定稿，`local_error` 的数值空间就没有需求依据；R-11 不修，读取面
  从 6 项到 7 项就是无据扩张。
- 原 12 版：**更要**。R-14 的六行表驱动会被直接改写。
- 结论：无论哪条，都不能在 PH-5 实现里边写边定；要么先拿需求修订授权，要么 0.1.0 不做。

## 6. 最强正方论点（窄版 steelman）与正面回应

### 6.1 正方最强版本

> 今天的 CT 调用方拿不到任何本地错误原因（主张席 §3.1，属实）。把 `local_error` 暴露成
> `HttpErr` 枚举后，调用方可以写全覆盖 `switch`，漏一个成员就在 `-Werror,-Wswitch` 下变红
> （主张席 §1.4 实测 rc=1）。这比「裸 int + 文档表」多一道编译期保证。数值保持
> `-1001…-1006`，R-14 的传输表一字不动，PH-3 golden 不变；成本约 90 行，ABI 只做尾字段追加。
> 0.1.0 尚未发布，现在是最便宜的时候；等发布后再加，才要背兼容包袱。

这版不是稻草人：它准确、克制，而且主动排除了「替换 `CURLcode`」这个错形态。

### 6.2 回应

1. **可观察性收益与枚举无关**：主张席 §3.1 的问题由「暴露 `local_error()`」解决，
   而访问器本身返回 `int`（主张席 §1.3 也承认映射单元写不出 `HttpErr` 类型）。
   探针 G2 用 `int` + shim 宏完成了同样的 `switch`，构建/运行 rc=0。枚举在这之上只多给
   `-Wswitch`，不是可观察性。
2. **`-Wswitch` 的收益是 opt-in 且脆弱的**：访问器返回 `int`，调用方必须手动收窄成 `HttpErr`；
   不收窄就没有检查（主张席 §1.4 末尾自己写了这条）。C 的 enum 不是强类型，错误 cast 可以骗过
   编译器；`default` 分支仍然是未知码的必需品，所以「穷举」并不封闭。
3. **`-Wswitch` 把追加变成破坏**：探针 F 证明，加第 4 个成员会让已编译通过的消费单元
   rc=1。主张席说「追加成员便宜」，对不用 `-Wswitch` 的人成立，对用它的人不成立。
   这正好回答本议题的版本问题：**新增成员的代价不是 ABI，而是对采用者的源码破坏。**
4. **名字不需要枚举**：shim 头宏已经给出裸 `HTTP_ERR_*`，且随 `http::http` 的生成 C 传递给
   下游包（§4.3）；枚举反而给 C 侧 mangled 名字（`http_HTTP_ERR_*`），还要加一条 g3 断言
   去绑定两套表。两套表比一套宏更容易漂移。
5. **6 个本地码不改变调用方动作**：ARG/HEADER/BODY/SCHEME/TIMEOUT/NO_HANDLE 全都意味着
   「连接前被拒、修参数、不要重试」。为一个单一动作做 6 路穷举，是仪式，不是能力。
   真正需要不同动作的是传输码（超时 vs DNS vs TLS），而 R-14 已经用原始码覆盖。
6. **`NO_HANDLE` 的行为修复独立于枚举**：把 `easy==NULL` 记成 `-1006`、
   在 `local_name()` 里给出 `no_handle`，就满足「不吞错」；不需要先冻结一个公开枚举。
7. **跨包收益是假设**：今天没有任何下游包声明要按 6 个本地码分支；`ct-llm-sse` 要的是
   状态码与 content-type（`ct-llm-sse/docs/requirements.md:82`）。为一个假设中的消费者
   先冻结公开契约，顺序不对。

## 7. 对我不利的实测与退路

必须写明：

- 如果产品要求的是「**CT 原生的具名枚举类型**」，不纳入做不到：`.h.ct` 的 `#define` 不可导出
  （§4.2，rc=1），shim 宏虽然可见但没有类型，也没有 `-Wswitch`（§4.6）。此时我这侧在
  「类型化」这一点上退到主张席的窄版，不能硬说「宏一样好」。
- 如果团队把「漏一个本地码要编译失败」当作硬需求，那么 `-Wswitch` 是真实收益，窄版是对的选择；
  但必须接受 §4.6 的追加破坏，并在文档里写清「新增成员会让采用者重新编译失败」。
- 旁证：`ct-fsguard` 选了 `pub enum FsErrCode` 并冻结数值（`ct-fsguard/docs/requirements.md:43-52`）。
  这支持「枚举在别的包可行」，但不支持「http 必须现在做」：它的错误码是主返回值，
  没有 R-14 这种「原始 `CURLcode` 逐项相等」的外部契约。

**退到哪里**：若用户裁定必须纳入，取主张席的窄版，不取原 12 版——
保持 `error_code` 原始 `CURLcode`，枚举只覆盖 `-1001…-1006`，先关闭 Q-05、修 R-11，
再把「追加成员对 `-Wswitch` 采用者是源码破坏」写进 README 与版本说明。

## 8. 结论与最小裁定建议

- **S-05 反证席判定：NO**。0.1.0 不把 `HTTP_ERR_*` 枚举纳入公开错误面；
  原 12 版（替换 `error_code`）明确拒绝，窄版（6 成员本地枚举）也不建议在 0.1.0 做。
- 0.1.0 采用 §1 的形态：`error_code` 原值 + `local_error()` 负数码（`-1001…-1006`）+
  谓词 + `local_name()` / `error_name()` / `error_detail()`；裸名字用 `http.shim.h` 的宏，
  不用 `.h.ct` 的 `#define`。
- 如果以后确实需要枚举：作为**独立的、需求授权后的变更**进入，先关闭 Q-05、改 R-11，
  再开卡；不要塞进 PH-5 实现。采用时同步登记「追加成员会让 `-Wswitch` 采用者编译失败」。
- 变更门槛建议：只有当某个真实下游需要「按 6 个本地码分支」或「漏码必须编译失败」时，
  才值得为枚举走一轮需求变更；在那之前，`int` + 宏 + 名字函数已经覆盖全部现有判据。

## 9. 探针索引

全部在 `/tmp/s05-probe/`，不属于仓库。统一用 `/Users/samxx/.local/bin/ct`（v0.44.1）。

| 探针 | 内容 | 命令 | rc |
| --- | --- | --- | --- |
| A `/tmp/s05-probe/final/enum` | `pub enum` 在 `.h.ct`，跨单元取值 | `ct build main.ct -o app --outdir build` | 0；运行 0 |
| B1 `/tmp/s05-probe/final/define` | 仅 `#define` 在 `.h.ct` | 同上 | 1（`use of undeclared identifier 'HTTP_ERR_ARG'`） |
| B2 同上 | `#define` + `pub enum` 占位 | 同上 | 1（同文案） |
| C `/tmp/s05-probe/final/macro` | `http.shim.h` 的宏从 CT 使用 | `ct build main.ct -I <repo>/src … --ldflags "-lcurl"` | 0；运行输出 `ct-http/0.1.0 -1004`，rc=0 |
| D `/tmp/s05-probe/final/mtype` | `pub enum` 放 `.m.ct` | `ct build main.ct -I . -o app --outdir build` | 1（`a .m.ct unit cannot declare a type`） |
| E `/tmp/s05-probe/final/trap` | 枚举在独立 `.h.ct`：单 import vs 双 import | 同上 | 单 import 1；双 import 0，运行输出 `7 -1001` |
| F `/tmp/s05-probe/final/switch` | `-Wswitch` 全覆盖；加成员后同一消费单元 | `ct build main.ct -I . -o app --outdir build --cflags "-Wall -Wextra -Werror"` | 3 成员 0；加第 4 成员 1（`-Werror,-Wswitch`） |
| G2 `/tmp/s05-probe/final/altlocal` | 不含枚举的 `local_error()` + shim 宏 + `switch` | 同上 | 0；运行 0（4 行分类输出） |
| H `/tmp/s05-probe/alt` | 不含枚举的谓词 + `error_name` + `error_detail` | 同上 | 0；运行 0（6 行分类输出） |

证据日志：`/tmp/s05-probe/final/evidence.log`（A/B/C/D/E）。
