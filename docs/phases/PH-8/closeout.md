# PH-8 收口对照：取消与生命周期

- 卡片：[docs/phases/PH-8/card.md](card.md)
- 判定：E-P8-1…E-P8-5 全部通过；偏差见 §4。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=b2dd6134…7520`、`tasks-sha256=f6c46c1c…289d`。
- 提交：本相单提交（`feat(http): PH8 取消与生命周期`），未 push、未打 tag。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P8-1 | `bash test/run.sh p8-cancel` | `ok`，输出 `error 42 cancelled 1 ok 0 chunks 1 total 1 status 200 lt3 1` | 0 |
| E-P8-2 | `bash test/run.sh p8-not-cancelled` | `ok`，输出 `send cancelled 0 ok 1 error 0`、`stream cancelled 0 ok 1 error 0` | 0 |
| E-P8-3 | `bash test/run.sh p8-lifetime` | `ok p8-lifetime (ASAN clean)`，输出 `url_ok 1 mode 1 status 200` / `cleanup_after_send 0` / `cleanup_after_res_free 0 responses_freed 1` / `cleanup_after_req_free 1` | 0 |
| E-P8-3（负例） | `bash test/run.sh p8-lifetime-bad` | `ok p8-lifetime-bad (ASAN rejected the stale request pointer)`，ASAN 报 `heap-use-after-free` | 0 |
| E-P8-4 | `bash test/run.sh p8-chain` | `ok`，输出 `cancel_error 42 cancelled 1 ok 0 requests 0` | 0 |
| E-P8-5 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0 …`、`ok g1 …`、`ok g3 (… cancellation, lifetime)` | 0 / 0 / 0 |
| E-P8-5 | `grep -q 'cancel()' README.md` / `grep -q 'S-04' docs/reviews/PH-8/S-04-ruling.md` | 均命中 | 0 |
| E-P8-5（负例） | `bash test/gates/g1_negative.sh` | `ok g1-negative (24 tampered cards rejected)`，含 PH-8 三份篡改卡 | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 2. 任务、提交与红证

本相按 2026-11-15 指令「一相一提交」：红证不再是相邻红/绿提交，而是实现前在同一工作区
跑夹具并原样记录，实现后复跑。baseline 为 `96794e6`。

| 任务 | 红证 | 红的形式 | 实现后 |
| --- | --- | --- | --- |
| T1 | [T1.md](../../reviews/PH-8/T1.md) | 构建红：`http_request_cancel` 未声明 | `ok p8-cancel` |
| T2 | [T2.md](../../reviews/PH-8/T2.md) | 回归守卫：基线即绿，不伪造红 | `ok p8-not-cancelled` |
| T3 | [T3.md](../../reviews/PH-8/T3.md) | 回归守卫 + 负例：正例基线即绿；负例被 ASAN 拒绝 | 两例均 `ok` |
| T4 | [T4.md](../../reviews/PH-8/T4.md) | 构建红：`no method 'cancel'` | `ok p8-chain` |
| T5 | [T5.md](../../reviews/PH-8/T5.md) | `grep -q 'cancel()' README.md` 退出 1 | 退出 0 |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`cancel_requested`、`http_request_cancel`、同线程与 DNS 边界注释 |
| `src/http/http.shim.c` | 入库；三判定点、始终安装进度回调、`42/cancelled=1`、优先级 |
| `src/http/http.m.ct` | 入库；`RequestBuilder.cancel()` |
| `test/fixtures/p8-cancel.c` … `p8-chain.ct` + expected | 入库；五个夹具 |
| `test/run.sh`、`tools/check.sh` | 入库；五个夹具并入 `g3`，新增 UAF 负例运行器 |
| `README.md`、`docs/streaming.md` | 入库；取消与生命周期、显式释放契约、同线程限制 |
| `docs/reviews/PH-8/S-04-advocate.md`、`S-04-rebuttal.md`、`S-04-ruling.md` | 入库 |
| `docs/reviews/PH-8/T1.md` … `T5.md` | 入库 |
| `docs/session-map.md` | 入库；PH-8 标记已收口 |

## 4. 偏差

- **单提交替代逐任务红/绿**：按 2026-11-15 指令，本相只做一个本地提交；红证改为
  「实现前记录、实现后复跑」，见 §2。判据与断言未放宽。
- **T2 / T3 正例在基线即绿**：未取消路径与 `send()` 后请求可读都是既有行为，属回归守卫。
  没有伪造红；T1/T4 的构建红与 T3 的 ASAN 负例承担「断言不是空转」的证明。
- **`p8-cancel` 实测 0.76 s**：取消在第一次进度回调或第二次写回调即生效，未等到 3 秒上限。
  判据只断言 `<3s`，不锁定精度。
- **`p8-cancel` 的 `total 1`**：取消发生在第一片交付之后，第二片在交付前被标志拦下，
  所以 `total_bytes()` 恰为 1，与「中止片不交付」的口径一致。
- **进度回调改为始终安装**：`idle_timeout` 为 0 时也装 `XFERINFOFUNCTION`，否则停顿流上的
  取消没有判定点。历史夹具全部通过，`g3` 时长见 §8。

## 5. 取消语义（最终口径）

- 入口 `void http_request_cancel(struct HttpRequest *r)` / CT `req->cancel()`；NULL 为 no-op。
- 判定点：传输开始前、每次写回调交付前、每次进度回调（约 1 秒一次）。
- 对外：`error()==42`、`is_cancelled()==1`、`is_ok()==0`、`timeout_kind()==0`。
- 优先级：取消 `42` > 回调中止 `23` > 上限 `-1008` > 空闲 `28/3` > 传输码。
- 标志粘性：连接前取消不连接、`status()==0`、服务端计数 0（`p8-chain`）。
- 精度：连续有数据的流在下一次写回调停（实测 <1 秒）；停顿流等下一次进度回调，约 1 秒。
- DNS 解析阶段不可取消；同线程限制；跨线程不在 0.1.0（S-04）。

## 6. 生命周期与释放契约

- `send()` / `send_stream()` 从不释放请求、从不 `curl_easy_cleanup`；`p8-lifetime` 证明
  `send()` 后 `req->url`、`req->mode` 可读，清理计数在请求 free 前为 0、后为 1。
- 响应与请求各自释放一次：CT `res->free(); req->free();`；C 侧槽位形式可二次调用。
- 负例 `p8-lifetime-bad` 复刻旧设计下「send 后继续用请求」的访问：
  `http_request_release(req)` 之后读 `req->url`，ASAN 报 `heap-use-after-free` 并以
  非 0 退出，证明正例的 ASAN 覆盖不是空转。
- 取消时的部分数据：非流式保留已收前缀、`take_body()` 可取；流式已交付片归调用方、
  `body()==NULL`、`take_body()==NULL`。

## 7. 真实网络与注入的边界

**PH-8 的取消与生命周期结论全部来自本地回环真实网络，没有使用
`http_shim_inject_curl_code`。**

| 用例 | 触发方式 | 验证类型 |
| --- | --- | --- |
| `p8-cancel` | `/slow4` 首片回调内 `cancel()` | 真实网络 |
| `p8-not-cancelled` | `/200` 两个入口 | 真实网络 |
| `p8-lifetime` | `/200`，send 后读请求字段 | 真实网络 + ASAN |
| `p8-lifetime-bad` | `/200`，free 后读请求 | 真实网络 + ASAN 负例 |
| `p8-chain` | `/slow4`，连接前 `req->cancel()` | 真实网络（服务端计数 0） |

## 8. 执行时长

单机 macOS arm64，每例含一次构建：

| 用例 | 墙钟 |
| --- | --- |
| `p8-cancel` | 0.76 s |
| `p8-not-cancelled` | 0.72 s |
| `p8-lifetime` | 0.77 s |
| `p8-lifetime-bad` | 未单独计时：该例有意触发 SIGABRT，沙箱对 abort 子进程判 backend 失败；同一构建参数，整套 `g3` 中通过 |
| `p8-chain` | 0.76 s |
| **PH-8 五例合计** | **约 3.8 s**（四例实测 3.01 s + 负例同量级） |
| `g3` 全量（含全部历史夹具） | 43 s（`G3_SECONDS=43`，含全部 45 例） |

低于 15 秒目标。

## 9. S-04 裁决摘要

- 主张席要求纳入跨线程取消（方向 B），反证席主张只做同线程（方向 A）；
  裁决采纳反证席：**0.1.0 只做同线程取消**。
- 理由：R-13/R-02 的非目标与 Q-12 已排除跨线程；单一个 atomic 不解决 cancel 与 free
  的 UAF；CT 无线程封装，跨线程判据只能落在 C 夹具；现有判定点已覆盖「慢而持续」与
  「连上后停顿」，「首字节前无数据」属超时语义。
- 不改 `docs/requirements.md` / `docs/design.md`：R-13 已覆盖本次落地口径。
- 重新裁定的触发条件与方向 B 的成本写在裁决文档 §4。

## 10. 当前事实

`req->cancel()` 已落地：`/slow4` 回调内取消在 0.76 s 内返回 `42/cancelled 1/ok 0`，
中止片不交付（`total 1`）；未取消路径两入口 `cancelled 0`；`send()` 后请求字段可读、
清理计数在请求 free 前为 0、后为 1，ASAN 正例干净，负例被 `heap-use-after-free` 拒绝；
CT 侧连接前取消读 `42/cancelled 1` 且服务端计数 0。S-04 已裁定为同线程（方向 A）。
PH-8 为收口状态，无未决项。
