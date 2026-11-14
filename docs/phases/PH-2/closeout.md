# PH-2 收口对照：C shim 内核与显式释放

- 卡片：[docs/phases/PH-2/card.md](card.md)
- 判定：退出判据 E-P2-1…E-P2-8 全部通过；任务清单 6 项均有「红 → 绿」相邻提交。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=86c135f0…4d8c`、`tasks-sha256=71129324…1929`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P2-1 | `bash tools/check.sh g1` | `ok g1 (card items, task columns, frozen criteria/tasks digests)` | 0 |
| E-P2-1（负例） | `bash test/gates/g1_negative.sh` | 两张卡各三份篡改均被拒；`ok g1-negative (6 tampered cards rejected)` | 0 |
| E-P2-2 | `bash test/run.sh p2-cleanup-once` | `ok p2-cleanup-once`，夹具输出 `1 0 1` | 0 |
| E-P2-3 | `bash test/run.sh p2-nm` | `ok p2-nm (no curl symbols in the CT object)` | 0 |
| E-P2-3（grep） | `grep -rn 'curl_easy_\|curl_slist_' src/http/*.m.ct src/http/*.h.ct` | 零命中 | 1（无匹配即预期） |
| E-P2-4 | `bash test/run.sh p2-double-free` | `ok p2-double-free`（`1 1 1`）；负例 ASAN 报 `double-free` 并中止 | 0 |
| E-P2-5 | `bash test/run.sh p2-after-send` | `ok p2-after-send`，输出 `0 200` 与 `1 1` | 0 |
| E-P2-6 | `bash test/run.sh p2-response-free` | `ok p2-response-free`，输出 `1 1` | 0 |
| E-P2-7 | `bash test/run.sh p2-ops` | `ok p2-ops`，输出 `418 teapot 1` | 0 |
| E-P2-8 | `bash tools/check.sh g0` / `grep -q 'http_request_free' README.md` | `ok g0 (... version 0.1.0)` / 命中 | 0 / 0 |
| 汇总 | `bash tools/check.sh g3` | `ok g3 (..., ops)` | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）——已逐项见 §2。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `9b455b7` | `ad7c80f` | [red/T1.md](red/T1.md) |
| T2 | `64190d5` | `6920913` | [red/T2.md](red/T2.md) |
| T3 | `1d2bc0c` | `4b40ae1` | [red/T3.md](red/T3.md) |
| T4 | `97dba3f` | `7a0e9f8` | [red/T4.md](red/T4.md) |
| T5 | `8f3f434` | `0e0a5a5` | [red/T5.md](red/T5.md) |
| T6 | `25ccc58` | `6860575` | [red/T6.md](red/T6.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；struct 布局 + 声明，无 curl 类型 |
| `src/http/http.shim.c` | 入库；easy handle 生命周期、计数器、默认 ops、槽位释放 |
| `src/http/http.m.ct` | 入库；`free()` 两个终态方法 |
| `test/fixtures/p2-*.ct` / `p2-*.c` + expected | 入库；6 个 p2 夹具 |
| `test/run.sh` | 入库；`--ldflags "-lcurl"`、`run_cc`、`run_asan_negative`、`run_nm` |
| `tools/check.sh` | 入库；`g1` 前缀泛化、`g3` 纳入全部 p2 夹具 |
| `test/gates/g1_negative.sh` | 入库；遍历所有 phase 卡 |
| `README.md`、`docs/session-map.md` | 入库 |

## 4. 偏差

1. **`ct build` 不读取 `ct.pkg` 的 `ldflags`**：实测 cc 行没有 `-lcurl`，链接报
   `_curl_easy_init` 未定义。处置：`test/run.sh` 与 README 显式传
   `--ldflags "-lcurl"`；`ct.pkg` 保持元数据不改。
2. **CT 方法无法回传置空后的指针**：映射展开成
   `http_request_release(r)`（单参），槽位置空只能作用于 shim 内部临时量。
   因此 CT 侧 `.free()` 是终态方法，幂等双释放由 C 槽位入口保证，README 已写明。
3. **E-P2-2 的 ASAN 负例依赖信号中止**：`test/run.sh` 会打印 shell 的
   `Abort trap: 6` 提示；这是负例被 ASAN 以 SIGABRT 中止的预期回报，不是失败。
4. **T6 的红锚点比 E-P2-8 更严格**：E-P2-8 的 `grep -q 'http_request_free'`
   在 PH-1 的 README 里已经满足，无法取红；T6 改用 `grep -q 'r->free()'`
   （CT 侧实际用法）作红证，判据文本未改。
5. **新增 ops 字段追加在既有字段之后**：`phase1_abi.c` 的字段顺序断言继续通过，
   但 struct 大小已变；PH-1 的 ABI 夹具只断言顺序，不断言尺寸。
