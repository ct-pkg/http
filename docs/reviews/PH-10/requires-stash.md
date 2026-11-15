# PH-10 遗留 `requires` stash 存档

- 存档日期：2026-11-15
- 来源：`stash@{0}: On main: requires 清理`（stash 提交 `1983edf`）
- 基线提交：`937c05d`（`docs(phases): 落库 tag 记录、Linux 四步清单与挂账`）
- 处置：**存档后 drop**；2026-11-15 之后 `git stash list` 为空
- 结论：内容已被 PH-11 的 `requires = ct >= 0.53.1` 取代，不再 apply

## 1. 涉及文件（`git stash show --stat`）

```console
$ git stash show --stat stash@{0}
 ct.pkg               | 7 ++++---
 docs/design.md       | 2 +-
 docs/phases/H1.md    | 2 +-
 docs/requirements.md | 2 +-
 4 files changed, 7 insertions(+), 6 deletions(-)
```

## 2. 为什么被 0.53.1 取代

1. **前提过期**：stash 写于 PH-10，当时的「最新发布」是 `ct v0.44.1`；PH-11 发布时的最新发布
   已是 `ct v0.53.1`，同一条「取最新发布」的口径得出不同数字。
2. **能力依据也换了**：stash 仍用「静态固有方法（`impl T { static T *new(); }`，Phase 34）」
   当理由；PH-11 核对发现本包公开面已是 `pub trait` + `map trait`，`grep -rn 'impl ' http/`
   命中 0，该理由不成立。现行理由是安装接口：`ct install` 必须打印 `ct build -I vendor/http`，
   而 `ct v0.49.0` 仍打印 `ct-ast build`，安装样例退出 1（见 `docs/reviews/PH-11/T1.md`）。
3. **判据已被覆盖**：PH-11 卡 E-P11-1 要求旧下限零残留，四处文本已由 PH-11 改动落库；
   stash 若 apply 会把 `0.44.1` 写回，直接违反该判据。

## 3. patch 全文（`git stash show -p stash@{0}`）

```diff
diff --git a/ct.pkg b/ct.pkg
index d4616fd..4a9a325 100644
--- a/ct.pkg
+++ b/ct.pkg
@@ -8,10 +8,11 @@
 # connection pooling, retries, proxying or allow-listing.
 #
 # Requires the compiler line that ships static inherent methods
-# (`impl T { static T *new(); }`, Phase 34); a released ct that predates it
-# cannot compile this package.
+# (`impl T { static T *new(); }`, Phase 34). That capability landed in
+# v0.34.0; the floor below tracks the newest ct release, so a released ct
+# below it is rejected before it can compile this package.
 name = http
 version = 0.1.0
 licence = MIT
 ldflags = -lcurl
-requires = ct >= 0.34.0
+requires = ct >= 0.44.1
diff --git a/docs/design.md b/docs/design.md
index 5cde095..8e02cd8 100644
--- a/docs/design.md
+++ b/docs/design.md
@@ -38,7 +38,7 @@
 | 4 | `typedef int (*HttpCB)(char*, int, void*)` + 结构体字段 + `->field` 调用 | **成立** | `ct-lang/test/ast/27_fnptr_forms/prog.ct:9-22`（`typedef int (*BinOp)(int,int);`、字段 `BinOp add;`、`c.add(2,3)` / `p->add(2,3)`）；探针 `p4.ct`（`HttpCB` + `void *ud`，`r->cb(p,n,r->ud)`）编过并返回 0 |
 
 **编译器前置条件**：第 1 条（静态固有方法）来自 Phase 34；发布版低于该 phase 的 `ct`
-编不过本包，所以 `ct.pkg` 写 `requires = ct >= 0.34.0`。
+编不过本包，所以 `ct.pkg` 写 `requires = ct >= 0.44.1`（能力下限为 Phase 34 的 v0.34.0，现行下限取最新发布版）。
 
 ---
 
diff --git a/docs/phases/H1.md b/docs/phases/H1.md
index 9e55a11..ddaa903 100644
--- a/docs/phases/H1.md
+++ b/docs/phases/H1.md
@@ -42,5 +42,5 @@
 - **链式不成立**（D-2）：若用户坚持单表达式链，必须改走 `map` 宿主方法（`ct-pkg/curl` 形态），
   本卡全部配置任务重写。等待裁决后再开工 H1-T2。
 - **编译器未发布**：静态固有方法在当前 HEAD 可用、发布版不可用；`ct.pkg` 已写
-  `requires = ct >= 0.34.0`。
+  `requires = ct >= 0.44.1`。
 - **目录待收口**（D-1）：单元若加 `namespace http;` 必须迁到 `http/` 目录。
diff --git a/docs/requirements.md b/docs/requirements.md
index c880c69..6259c14 100644
--- a/docs/requirements.md
+++ b/docs/requirements.md
@@ -38,7 +38,7 @@
 - **问题**：包必须能被 `ct get` / `ct install` 识别，声明编译器下限与链接选项，且不把 libcurl 头带进 CT 单元。
 - **验收判据**：
   - `bash tools/check.sh g0` 退出 0（2026-11-13 实测：`ok g0 ... version 0.1.0`）。
-  - `ct.pkg` 含 `name=http`、`version=0.1.0`、`licence=MIT`、`ldflags=-lcurl`、`requires=ct >= 0.34.0`；删掉任一键后 `ct get` 非零退出并给出缺键文案。
+  - `ct.pkg` 含 `name=http`、`version=0.1.0`、`licence=MIT`、`ldflags=-lcurl`、`requires=ct >= 0.44.1`；删掉任一键后 `ct get` 非零退出并给出缺键文案。
   - 构建产物 `otool -L`（macOS）/`ldd`（Linux）只指向系统 libcurl，没有仓库内 curl 副本。
 - **非目标**：不自带/vendored curl；不提交索引行（只出提案）；不做预发布/构建元数据。
 - **CT 依赖/阻塞**：`.m.ct` + `.shim.h/.c` + `ldflags`/`requires`（`ct-lang/docs/package-guide.md:57-76`、`docs/bindings-and-patterns.md:80-180`）——已支持。
```

## 4. 处置记录

```console
$ git stash list
stash@{0}: On main: requires 清理
$ git stash drop
Dropped refs/stash@{0} (1983edff3f243f3fca184255cc59f2f84c9134ff)
$ git stash list
（无输出，stash 列表为空）
```

原 stash 内容以本文件 §3 全文保留；如需复现，可按 patch 手工 apply 到 `937c05d` 的工作树，
但**不应**在现行树上应用（见 §2 第 3 条）。
