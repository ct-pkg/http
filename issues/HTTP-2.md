# HTTP-2 ct install 打印的 -I vendor/http 与包布局不匹配

- status: verified
- 证据: `test/gates/p10_install.sh`；`docs/reviews/PH-10/T7.md`；`docs/proposals/ct-install-include-prefix.md`；`docs/design.md` D-1
- phase: PH-10
- fix: yes

## 问题（2026-11-15 发现）

`ct install` 对每个 vendored 包打印一条 `-I vendor/<name>`
（`ct-lang/selfhost/ct-ast/tool/pkg/Pkg.h.ct:1388-1389`）。本包当时按设计 D-1 把单元放在
`src/http/`，消费端执行 `ct build -I vendor/http ...` 报：

```text
ctc: cannot resolve import 'http/http'
```

## 处置（2026-11-15，选①）

用户裁定采用「包根 `http/`」布局：

- `git mv src/http http`，`src/` 删除；`src/README.md` 迁为 `http/README.md`。
- 同步 `test/run.sh`、`tools/gates/g4.sh`、`tools/gates/g7.sh`、
  `test/gates/g4_negative.sh`、`test/gates/p10_install.sh`、`docs/snippets/build.sh`、
  README 与教程的 `-I`。
- 现在 `ct install` 打印的 `-I vendor/http` 就是真实包含根；
  `p10_install.sh` 断言安装行含 `-I vendor/http`、不含 `-I vendor/http/src`，样例输出 `200`。
- `ct install` 的 include 前缀约定另以提案形式记录：
  `docs/proposals/ct-install-include-prefix.md`（未改 ct-lang 仓库）。

## 复核

- 复核人：主控（用户授权）
- 证据：`bash tools/check.sh g3`、`bash tools/check.sh g4`、
  `bash test/gates/p10_install.sh` 均退出 0（输出见 `docs/reviews/PH-10/HTTP-2-verify.md`）。
- 状态转换：`open` → `verified`。
- `fix: yes` 的 PATCH 记账仍按 `deviation: V-001` 延后，不另动版本号。
