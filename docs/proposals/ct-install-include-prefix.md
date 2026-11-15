# 提案：`ct install` 的 include 前缀约定

- 对象：ct-lang（本文件只是提案，未改 ct 仓库、未提交任何 ct-lang 变更）
- 日期：2026-11-15
- 提出方：ct-http（PH-10，`issues/HTTP-2.md`）

## 现象

`ct install` 为每个 vendored 包打印一条 `-I vendor/<包名>`，但不读取包内布局。
若包的单元不在包根（例如放在 `src/http/`），消费端照抄该命令会报：

```text
ctc: cannot resolve import 'http/http'
```

必须手工改成 `-I vendor/http/src` 才能编译。包自己的 `ct.pkg` 无法声明这个前缀：
`cflags` 只进 cc 行，不参与 `ctc` 的 import 解析。

## ct 现行为（依据）

- `ct-lang/selfhost/ct-ast/tool/pkg/Pkg.h.ct:1388-1389`：
  `printf("%s build", …)` 后对每条 lock 记录打印 ` -I vendor/%s`，没有别的 include 根。
- `Pkg.h.ct:1390-1396`：`cflags` / `ldflags` 汇总后分别作为 `--cflags` / `--ldflags` 输出，
  与 import 解析无关。
- 因此 `-I` 前缀完全由 `ct install` 的固定格式决定，包作者不可配置。

## 期望行为（三选一，交 ct-lang 裁定）

1. **包清单声明 include 根**：`ct.pkg` 增加 `include = <相对路径>`（可多条），
   `ct install` 为每条打印一个 `-I vendor/<name>/<include>`；
   未声明时默认包根，保持向后兼容。
2. **强制包根约定**：规定单元必须位于包根，`ct verify` / `ct get` 校验
   「每个 `import` 能从包根解析」，不满足即报错；不新增配置项。
3. **自动探测**：`ct install` 探测包根与 `src/`，打印实际存在的第一个包含根；
   隐式行为最少，但规则不够显式。

## 我们这边的临时对策（已实施）

- 选择方案 2 的包侧形态：把单元从 `src/http/` 迁到包根 `http/`，`src/` 删除。
  于是 `ct install` 打印的 `-I vendor/http` 直接成立，消费端无需额外前缀。
- `test/gates/p10_install.sh` 断言：安装命令包含 `-I vendor/http`、不包含
  `-I vendor/http/src`，并用该命令编译、运行样例得到 `200`。
- 记录：`docs/design.md` D-1、`docs/phases/PH-10/closeout.md`、`issues/HTTP-2.md`。

## 不做

- 不改 ct-lang 仓库，不提 PR，不提交索引行。
- 不引入第二套 include 根约定；在 ct-lang 给出正式规则前，包侧按包根布局。
