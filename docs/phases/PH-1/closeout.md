# PH-1 收口对照：接口冻结与可编译链

- 卡片：[docs/phases/PH-1/card.md](card.md)
- 判定：退出判据 E-P1-1…E-P1-6 全部通过；任务清单 6 项均有“红 → 绿”相邻提交。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=ce1d4d0e…be26`、`tasks-sha256=0ed8221c…15c0`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P1-1 | `bash tools/check.sh g1` | `ok g1 (card items, task columns, frozen criteria/tasks digests)` | 0 |
| E-P1-1（负例） | `bash test/gates/g1_negative.sh` | 三份篡改卡分别被拒：`criteria-sha256 mismatch` / `effort must be S or M` / `missing card item` | 0（被拒是预期） |
| E-P1-2 | `bash tools/check.sh g3` | `ok g3 (abi, chain, variable landing, negative import)` | 0 |
| E-P1-3 | `bash test/run.sh phase1_chain` / `phase1_var` | `ok phase1_chain` / `ok phase1_var` | 0 / 0 |
| E-P1-4 | `bash test/run.sh phase1_no_mapping` | 构建非 0，诊断含 `no method 'post' for type 'char *'`；runner 自身 | 0 |
| E-P1-5 | `bash tools/check.sh g0` | `ok g0 (... version 0.1.0)` | 0 |
| E-P1-6 | `bash test/run.sh phase1_abi` | `ok phase1_abi`（`cc -std=c99 -Wall -Wextra -Werror`） | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）——已逐项见 §2。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `431c089` | `ff0b830` | [red/T1.md](red/T1.md) |
| T2 | `0116da3` | `d856374` | [red/T2.md](red/T2.md) |
| T3 | `dc70c5a` | `ccc3ae5` | [red/T3.md](red/T3.md) |
| T4 | `d22e38b` | `17edd96` | [red/T4.md](red/T4.md) |
| T5 | `cc4b541` | `0ad93c1` | [red/T5.md](red/T5.md) |
| T6 | `e4e9058` | `e1b9271` | [red/T6.md](red/T6.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；两个 struct + 固定值 stub |
| `src/http/http.m.ct` | 入库；三个 trait + 三段静态映射 |
| `src/http/Types.h.ct` | 入库；`pub enum HttpDefaults` |
| `test/fixtures/phase1_chain.ct` + expected | 入库 |
| `test/fixtures/phase1_var.ct` + expected | 入库 |
| `test/fixtures/phase1_no_mapping.ct` | 入库 |
| `test/fixtures/phase1_abi.c` + expected | 入库 |
| `test/gates/g1_negative.sh` | 入库 |
| `test/run.sh` | 入库；四个 phase 1 夹具 |
| `tools/check.sh` | 入库；g0 / g1 / g3 |
| `README.md`、`docs/session-map.md` | 入库 |

## 4. 偏差

1. **目录口径未收口**：本 phase 固定 `src/http/` + `namespace http;`，D-1（`src/` vs `http/`）
   仍未裁定；若后续改包根，只挪路径，不改映射。
2. **E-P1-6 是开卡时新增的**：roadmap 初稿只有五条判据；立卡时补 ABI 夹具一条，发生在首个
   实现提交之前，不构成判据冻结后的修改。
3. **`Types.h.ct` 用 `pub enum`**：CT 没有顶层 `const` 声明，常量用单成员枚举承载（值 0）。
4. **T3 绿提交修正 runner 输出目录冲突**：`--outdir` 与 `-o` 同名导致 ld 失败，改成
   `build-<task>`；属夹具接线修正，不改变产品行为。
5. **T1 baseline 校正**：红证初稿写 `2e9c889`，按 T-7 应为红证提交的父提交 `22ed9df`；
   复跑输出一致，见 `be5c926`。
6. **g3 未链接 libcurl**：`ct.pkg` 的 `-lcurl` 在本 phase 不生效，符合“不接网络、不接真 curl”。

## 5. 一行当前事实

用本库写最小 HTTP 调用：`ct build` 退出 0、运行退出 0，`status()` 返回 `200`、body 为
`{"users":3}`；**不真发请求**，仍是 shim 固定值，没有网络与 libcurl。

## 6. 下一 phase 入口

按 roadmap 进入 PH-2（C shim 内核与显式释放）。S-01…S-05 仍未裁定，PH-1 未引入其中任何一项；
PH-2 也不依赖它们。
