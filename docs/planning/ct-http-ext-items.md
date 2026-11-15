# 包外移入事项认领：EXT-HTTP-01…04

- applies-to: 0.2.0
- 认领日期：2026-11-15
- 来源：CT 仓移出的包外事项清单（§10 S4/S5 与「外部报告复核」来源行）。
  本文件**只引用能力前置**（C1/C2/C3），不引用任何 CT 版本号。
- 状态：4 条均已认领（文档层）；**落地 0 条**，因为 C1/C2 都还没落地。

## 1. 能力前置（本仓口径）

| 编号 | 能力 | 本仓依赖点 |
| --- | --- | --- |
| C1 | `ct get` 把包落到 store，并读取 `publisher` | 包身份与 store 路径 |
| C2 | `ns-root` 进入 `-I` | 安装后 include 根不再是 `vendor/<name>` |
| C3 | `ct build` 默认读 lock | 消费端不再手算 include 路径 |

## 2. 认领总表

| ID | 来源编号 | 要做什么 | 前置 | 落地状态 |
| --- | --- | --- | --- | --- |
| EXT-HTTP-01 | §10 S4 | `ct.pkg` 补 `publisher` 与 `ns-root` | C1、C2 | 阻塞（工作区已有两行，见 §4） |
| EXT-HTTP-02 | §10 S4 | `test/gates/p10_install.sh` 改断言 store 路径 | C2 | 阻塞 |
| EXT-HTTP-03 | §10 S5 | `test/`、`tools/` 去手算 `-I vendor/` | C2（建议等 C3） | 阻塞 |
| EXT-HTTP-04 | 外部报告复核来源行 | HTTP-1…HTTP-5 阻塞清单与对外发布说明归本仓 | 无（文档责任） | 部分（缺 HTTP-4/5 正文） |

## 3. 逐条

### EXT-HTTP-01 `publisher` 与 `ns-root`

- **要做什么**：`ct.pkg` 增加 `publisher = ct` 与 `ns-root = .`，与 store 布局对齐。
- **前置**：C1（`ct get` 落 store 且认 `publisher`）、C2（`ns-root` 进 `-I`）。
- **验收（前置满足后）**：
  - `grep -c '^publisher = ct$' ct.pkg` 输出 `1`；`grep -c '^ns-root = \.$' ct.pkg` 输出 `1`；
  - `ct get <本地目录 spec>` 退出 0，且 store 目录存在；
  - `ct install` 打印的 `-I` 指向 store，不含 `vendor/`。
- **当前**：不落地。工作区已有这两行但未提交（见 §4）。

### EXT-HTTP-02 `p10_install.sh` 断言 store 路径

- **要做什么**：把「打印 `ct build` + `-I vendor/http`」改为「打印 `ct build` + store 路径」，
  并删掉 `vendor/` 断言。
- **前置**：C2。
- **验收（前置满足后）**：
  - `grep -c 'vendor/' test/gates/p10_install.sh` 输出 `0`；
  - `bash test/gates/p10_install.sh` 退出 0，输出 `install ok` 与 `200`；
  - 断言目录存在：`test -d <store 路径>` 退出 0。
- **当前**：阻塞。脚本仍硬断言 `-I vendor/http`（5 处，见 EXT-HTTP-03 的清单）。

### EXT-HTTP-03 去手算 include 路径

- **要做什么**：`test/`、`tools/` 不再手写 `-I vendor/`，改为用 `ct install` 打印的行或 lock。
- **前置**：C2；建议同时等 C3，避免再用输出字符串拼路径。
- **验收（前置满足后）**：
  - `grep -rn -- '-I vendor/' test tools` 零命中（`rc=1`）；
  - 全仓零命中是目标；但历史卡片、评审记录与提案里保留了旧契约原文，是否一并改写见 §5；
  - `bash tools/check.sh g0 … g9` 不受影响。
- **当前**：阻塞。`test/tools` 现有 5 处，全部在 `test/gates/p10_install.sh`
  （第 13、77、78、79、85 行）。

### EXT-HTTP-04 0.1.0 阻塞清单与对外发布说明

- **要做什么**：HTTP-1…HTTP-5 的阻塞清单、状态与证据由本仓维护；对外发布说明也归本仓；
  CT 侧只保留分诊，不复制报告正文。
- **前置**：无（属文档责任）。但 HTTP-4/HTTP-5 的正文目前不在本仓。
- **验收（收编后）**：
  - `ls issues/HTTP-{1,2,3,4,5}.md` 五件齐全，每件含 `status` / 证据 / 结论；
  - 对外发布说明在本仓有固定落点（建议 `docs/release-notes.md`）；
  - 本仓文档不再把报告正文外包给 CT 侧。
- **当前**：部分。本仓有 [HTTP-1](../../issues/HTTP-1.md)、[HTTP-2](../../issues/HTTP-2.md)、
  [HTTP-3](../../issues/HTTP-3.md)；HTTP-4/HTTP-5 缺失（不能读本仓以外目录，见 §5）。

## 4. 工作区待决项：`ct.pkg`

- **现状**：`git status --porcelain` 输出 ` M ct.pkg`；diff 只有两行新增：
  `publisher = ct`、`ns-root = .`。`version`、`requires`、`ldflags` 均未改。
- **本次处置**：按要求**不提交、不丢弃**，原样留在工作区。
- **建议**：等 C1/C2 落地后，用一次独立提交把这两行与 EXT-HTTP-02/03 的脚本改动一起落地，
  并在落地前用 store 路径实测；在那之前任何提交都不带 `ct.pkg`。
- **风险**：工作区长期带这份 diff 容易被误提交；若 owner 同意，可先 `git stash` 存档
  （本次指令未授权，故未动）。

## 5. 需要 owner 拍板

1. **HTTP-4/HTTP-5 的正文**：本仓不能读 CT 目录。请转交正文，或明确记为「未收编」并关闭该项。
2. **EXT-HTTP-03 的验收口径**：全仓 `-I vendor/` 零命中，还是只要求 `test/` + `tools/` 零命中？
   历史卡片与评审记录里的旧契约原文是否保留。
3. **`ct.pkg` 两行的处置**：继续挂工作区，还是先 stash 存档（建议后者，需授权）。
4. **对外发布说明的落点**：新增 `docs/release-notes.md`，还是并入 README 的「版本」段。

## 6. 与当前相位的关系

这四条**不并入 PH-12**（`method()` + `content_type()`）：C1/C2 未满足，提前做会重写一遍
include 根与 lock 契约。PH-12 的登记见 [roadmap](../planning/ct-http-0.1.0-roadmap.md) §5；
本文件只做认领与状态登记，不改 `ct.pkg`、不打 tag、不 bump。
