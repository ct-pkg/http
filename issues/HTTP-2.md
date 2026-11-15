# HTTP-2 ct install 打印的 -I vendor/http 与 src/ 布局不匹配

- status: open
- 证据: `test/gates/p10_install.sh`；`docs/phases/PH-10/closeout.md` §4.4
- phase: PH-10
- fix: no

## 问题

`ct install` 对每个 vendored 包打印一条 `-I vendor/<name>`（见 ct-lang 的 Pkg 实现与
`docs/package-guide.md`）。本包按设计 D-1 把单元放在 `src/http/`，所以消费端执行
`ct build -I vendor/http ...` 会报：

```text
ctc: cannot resolve import 'http/http'
```

显式追加 `-I vendor/http/src` 后可编译、运行，样例输出 `200`。

## 处置

0.1.0 不改目录布局（D-1 已冻结，改动会牵动全部夹具与文档）。安装验证脚本用
`-I vendor/http/src` 复跑，并在报告里单列这一差异。

## 待定

收敛方式二选一，需要用户裁定：

1. 包根增加 `http/` 入口（移动单元，改 `-I`）；或
2. `ct install` 支持每包额外 include 前缀（改编译器侧）。

在此之前，裸名 `ct get http@^0.1.0` 还需要已发布索引与 `v0.1.0` tag；本轮用户明确
不打 tag、不 push，所以该形态登记为不可执行项。
