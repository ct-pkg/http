# PH-3 收口对照：请求配置与输入校验

- 卡片：[docs/phases/PH-3/card.md](card.md)
- 判定：退出判据 E-P3-1…E-P3-7 全部通过；任务清单 7 项均有「红 → 绿」相邻提交。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=7ece3103…7a7d`、`tasks-sha256=5792bca6…aa83`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P3-1 | `bash test/run.sh p3-url-own` | `ok`，夹具输出 `owned 1 url https://example.test/a` | 0 |
| E-P3-2 | `bash test/run.sh p3-header-crlf` | `ok`，输出 `rejected 1 1 added 0 1 first Accept: application/json` | 0 |
| E-P3-3 | `bash test/run.sh p3-body-nul` | `ok`，输出 `len 3 stage 2 bytes 97 0 98` 与 `badlen 1` | 0 |
| E-P3-4 | `bash test/run.sh p3-scheme` | `ok`，输出 `file -1004 ftp -1004 http 0 https 0` | 0 |
| E-P3-4（静态锚点） | `grep -c 'curl_easy_perform' src/http/http.shim.c` | `0`（无 `perform`） | 1（无匹配即预期） |
| E-P3-5 | `bash test/run.sh p3-defaults` | `ok`，输出 `defaults 0 0 0 0 ct-http/0.1.0`、`set 3000 1000 1 1 probe/1`、`neg -1005 3000` | 0 |
| E-P3-6 | `bash test/run.sh p3-chain` | `ok`，输出 `200 {"users":3}` | 0 |
| E-P3-7 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0 (... version 0.1.0)` / `ok g1 (...)` / `ok g3 (...)` | 0 / 0 / 0 |
| E-P3-7（README） | `grep -q 'connect_timeout' README.md` | 命中 | 0 |
| E-P3-7（CT 无 curl） | `grep -rn 'curl_easy_\|curl_slist_' src/http/*.m.ct src/http/*.h.ct` | 零命中 | 1（无匹配即预期） |

固定条：任务清单每项可独立验收（6.6 T-6）——已逐项见 §2。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `76a78e5` | `7625792` | [red/T1.md](red/T1.md) |
| T2 | `cb426e5` | `1dd586c` | [red/T2.md](red/T2.md) |
| T3 | `82a2a9f` | `1ed6ecc` | [red/T3.md](red/T3.md) |
| T4 | `c79003a` | `a1eb8f2` | [red/T4.md](red/T4.md) |
| T5 | `1958eb0` | `d122bab` | [red/T5.md](red/T5.md) |
| T6 | `ccd2aac` | `fd4cf7e` | [red/T6.md](red/T6.md) |
| T7 | `679719e` | `docs: PH3-T7 配置面与收口`（本提交） | [red/T7.md](red/T7.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；URL/头/body/UA 所有权字段、`HTTP_LOCAL_ERR_*`、默认 UA、五个 setter、头行钩子 |
| `src/http/http.shim.c` | 入库；深拷贝与释放、slist、CR/LF 拒绝、`POSTFIELDSIZE`→`COPYPOSTFIELDS`、协议掩码、默认值 |
| `src/http/http.m.ct` | 入库；`RequestBuilder` 增加 5 个方法 |
| `test/fixtures/p3-*.c` / `p3-chain.ct` + expected | 入库；6 个 p3 夹具 |
| `test/run.sh` | 入库；6 个 `p3-*` 接线 |
| `tools/check.sh` | 入库；`g3` 纳入全部 p3 夹具 |
| `README.md` | 入库；配置面、默认值、本地码边界、phase 3 状态 |
| `docs/phases/PH-3/red/T1.md` … `T7.md` | 入库 |

## 4. 偏差

1. **roadmap 的 E-P3-1/E-P3-3 原写「服务端收 3 字节 / 服务端计数 0」**，但 phase 3 不
   `perform`，无法起真实服务端。卡内判据改为 shim 状态锚点（`body_len`、`body_stage`、
   `local_error`、`grep curl_easy_perform` 为 0），roadmap 文本未改。
2. **`header()` 返回 `CRequest *`，无法把本地错误回传调用点**：首个本地错误存进
   `struct HttpRequest.local_error`，夹具与后续 phase 可核对；公开错误面留到 PH-5，
   README 明写「不是公开错误面」。
3. **本地码命名为 `HTTP_LOCAL_ERR_*`（-1001…-1005）**，不用 `HTTP_ERR_*` 前缀，
   避免与 S-05 待裁定的枚举抢占命名；`error_code` 语义仍保留 `CURLcode` 原值。
4. **请求体字段类型为 `char *`（非 `void *`）**：夹具要按字节核对内嵌 NUL，`void *`
   不能下标；该字段不暴露给 CT，不构成类型外泄。
5. **T4 的红是行为红**（`file`/`ftp` 返回 0），不是构建红：`local_error` 字段在 T2 已存在，
   缺少的只是 scheme 校验。
6. **`g3` 的 shell 汇总行包含 "Abort trap: 6"**（ASAN 负例被 SIGABRT 中止），
   属 PH-2 已记录的预期信号；本 phase 未改变该行为。
7. **`send()` 仍是固定值 stub**：phase 3 只做配置写入与本地校验，未接入
   `curl_easy_perform`；真执行在 PH-4。
