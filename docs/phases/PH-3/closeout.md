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
| T7 | `679719e` | `9826ff5` | [red/T7.md](red/T7.md) |

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

## 5. 补充说明：判据改锚的时点核实（2026-11-15）

§4.1 写的「判据改为 shim 状态锚点」容易被读成「冻结之后改判据」。核实结论：**不是**。
冻结那一刻卡片 §5 已经是 shim 锚点，之后一字未改。

### 5.1 证据

`docs/phases/PH-3/card.md` 只有一次提交，即冻结提交：

```console
$ git log --oneline -- docs/phases/PH-3/card.md
1b65139 docs(phases): PH-3 卡与判据冻结
```

判据摘要用 `tools/check.sh` 同一套 `digest_section` 重算，冻结提交与当下完全一致：

```console
$ git show 1b65139:docs/phases/PH-3/card.md | <digest_section '## 5. 退出判据（冻结）'>
7ece31030a3ea4b698c2848b81a6bca48a198cb97c714fe43f7adc9463e27a7d
$ <digest_section '## 5. 退出判据（冻结）'> docs/phases/PH-3/card.md
7ece31030a3ea4b698c2848b81a6bca48a198cb97c714fe43f7adc9463e27a7d

$ git diff --stat 1b65139..HEAD -- docs/phases/PH-3/card.md
（空）
```

冻结提交里的原文（`git show 1b65139:docs/phases/PH-3/card.md`）：

```text
| E-P3-1 | URL 深拷贝：覆写调用方缓冲区后请求仍返回原 URL | `bash test/run.sh p3-url-own` 退出 0，输出 `owned 1 url https://example.test/a`；负例：借用时输出 `owned 0` 并红 |
| E-P3-3 | body 按显式长度存储，NUL 不截断，且先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | `bash test/run.sh p3-body-nul` 退出 0，输出 `len 3 stage 2 bytes 97 0 98` 与 `badlen 1` |
| E-P3-4 | 只接受 http/https；其余在连接前给本地负数码，且源码无 `curl_easy_perform` | `bash test/run.sh p3-scheme` 退出 0，输出 `file -1004 ftp -1004 http 0 https 0`；`grep -c 'curl_easy_perform' src/http/http.shim.c` 输出 `0` |
```

三条都在冻结时就是可执行的 shim / 本地锚点，没有任何「服务端收到几字节」的措辞。

### 5.2 与 roadmap 的差异源于编号不同，不是改锚

`docs/planning/ct-http-0.1.0-roadmap.md`（未冻结，最后一次改动在 `22ed9df`）
的 phase 3 写的是：

```text
- `E-P3-1`：`test/run.sh p3-body-nul`：`body("a\0b", 3)` 的 `POSTFIELDSIZE==3`，服务端收 3 字节。
- `E-P3-3`：`test/run.sh p3-scheme`：`file:///etc/passwd` 与 `ftp://…` 连接前返回本地错误，服务端计数 0。
```

两份文件的 `E-P3-*` 编号指向不同对象：roadmap 的 E-P3-1 是 body，卡片的 E-P3-1 是
URL 自持；roadmap 的 E-P3-3 是 scheme，卡片的 E-P3-3 是 body。卡片不是把 roadmap 的
判据「改锚」，而是立卡时按「本 phase 不 `perform`」这一事实重写了可执行锚点，并把
roadmap 的「服务端」观察降级为 shim 状态观察。

### 5.3 处置

- 属「冻结前就是 shim 锚点」这一种，不触发 §6.2 冻结违规，不需要重开 PH-3。
- roadmap 措辞乐观（在 phase 3 不 `perform` 的前提下要求服务端观察），本补充说明即为
  记录；roadmap 是否改写等裁定，本轮不动。
- `docs/phases/PH-3/card.md` 保持 `1b65139` 原文，§4.1 也不重写，冲突以本节为准。
