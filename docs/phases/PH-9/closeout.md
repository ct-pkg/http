# PH-9 收口对照：重定向与安全策略

- 卡片：[docs/phases/PH-9/card.md](card.md)
- 判定：E-P9-2…E-P9-8 通过；**E-P9-1 不计通过**（冻结锚点算术错误，行为已满足），见 §4。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=ec52cf53…1268`、`tasks-sha256=d8becb2f…3f7a`。
- 提交：本相单提交（`feat(http): PH9 重定向与安全策略`）。

## 1. 退出判据

| ID | 命令 | 原始输出 / 结果 | 判定 |
| --- | --- | --- | --- |
| E-P9-1 | `bash test/run.sh p9-same-host` | `one status 200 error 0 requests 2 body_ok 1`；`two status 200 error 0 requests 5 body_ok 1` | **不计通过**：冻结锚点要求第二条 `requests 4`，而「2 跳」链必然是 3 个请求（302、302、200），累计 5；行为（200/body_ok）满足，数字锚点不可能成立 |
| E-P9-2 | `bash test/run.sh p9-hops10` | `status 200 error 0 requests 11 body_ok 1` | 通过 |
| E-P9-3 | `bash test/run.sh p9-hops11` | `status 302 error 47 requests 11 hop11 0 ok 0` | 通过 |
| E-P9-4 | `bash test/run.sh p9-cross-host` | `cross auth 0 cookie 0 proxy 0 status 200`；`cross referer 0`；`port auth 0 cookie 0 proxy 0 status 200`；`same auth 1 cookie 1 proxy 1 status 200`；`same referer 1` | 通过（host 轴用 `localhost` 别名替代不可 bind 的 `127.0.0.2`，见 §4） |
| E-P9-5 | `bash test/run.sh p9-loop` | `error 47 requests 11 status 302` | 通过 |
| E-P9-6 | `bash test/run.sh p9-downgrade` | `error -1009 status 0 plaintext 0` | 通过 |
| E-P9-7 | `bash test/run.sh p9-default` | `status 302 error 0 ok 0 requests 1` | 通过 |
| E-P9-8 | `bash tools/check.sh g0 && g1 && g3` | 见 §5 门禁末行 | 通过 |
| E-P9-8 | `grep -q '10 跳' README.md` / `grep -q 'Proxy-Authorization' README.md` | 均命中 | 通过 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 2. 任务与红证

本相沿用「一相一提交」：红证用**基线 shim（`c3ffc37`）+ 新夹具**的消融运行取得，
不是伪造的「先红后绿」提交。红证见 `docs/reviews/PH-9/T1.md` … `T5.md`。

| 任务 | 红证摘要 | 实现后 |
| --- | --- | --- |
| T1 跟随循环 + 10 跳 + 方法改写 | 基线 `p9-hops11`：`status 200 error 0 requests 12 hop11 1 ok 1` | `302/47/11/0/0` |
| T2 跨源剥头 | 基线 `p9-cross-host`：同源行 `same auth 0 cookie 0 proxy 0`（基线根本没发头） | 同源全 1、跨源全 0 |
| T3 循环有界 | 基线 `p9-loop`：`error 47 requests 31 status 302` | `error 47 requests 11 status 302` |
| T4 降级拒绝 | 基线 `p9-downgrade`：`error 0 status 200 plaintext 1` | `error -1009 status 0 plaintext 0` |
| T5 默认回归 + 文档 | 基线 `grep '10 跳' README.md` 退出 1；`p9-default` 基线即绿 | grep 命中；`p9-default` 保持 |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`HTTP_LOCAL_ERR_REDIRECT(-1009)`、`HTTP_MAX_REDIRECTS 10` |
| `src/http/http.shim.c` | 入库；内部跟随循环、10 跳上限、方法改写、中间 body 丢弃、跨源剥头、降级拒绝 |
| `test/fixtures/p9-server.c` / `.h` | 入库；回环服务器、链长、按路径计数、回显头、循环、跨实例 |
| `test/fixtures/p9-same-host.c` … `p9-default.c` + expected | 入库；七个夹具 |
| `test/fixtures/p9-tls-redirect.py` | 入库；降级夹具的 TLS 起点 |
| `test/run.sh`、`tools/check.sh` | 入库；七个夹具并入 `g3`，TLS 依赖缺失时退出 2 |
| `README.md`、`docs/session-map.md` | 入库 |
| `docs/reviews/PH-9/T1.md` … `T5.md` | 入库 |

## 4. 偏差与未满足项

### 4.1 E-P9-1 的数字锚点不成立（不计通过）

冻结判据写的是「请求数分别为 2 与 4」。实际：

- 1 跳链 `/redirect/1 → /final`：2 个请求；
- 2 跳链 `/redirect/2 → /redirect/1 → /final`：3 个请求；
- 夹具按累计数打印，所以第二条是 `2 + 3 = 5`。

「2 跳」至少要 3 个请求，`4` 在算术上无法达成（除非把第二例改成两个独立的 1 跳请求，
那就不再是 2 跳用例）。按纪律：**不改判据、不降标准**，本条记为不通过，等待裁定。
行为层面的两条断言（`status 200`、`body_ok 1`）均满足。

建议（未执行，需用户授权改冻结判据）：把锚点改为 `two … requests 5`（累计）或
「第二条为 3 个请求」。

### 4.2 跨源主机轴的地址替换

卡片 E-P9-4 写的是 `127.0.0.1 → 127.0.0.2`。本机 macOS 不允许 bind `127.0.0.2`
（`OSError: [Errno 49] Can't assign requested address`，已实测）。夹具改为：

- host 轴：`127.0.0.1 → localhost`（同端口、不同主机名，仍是真实 origin 变化）；
- port 轴：`127.0.0.1:PA → 127.0.0.1:PB`（同主机、不同端口）。

卡片的剥离判据（跨源 0、同源 1）与输出文本未变，覆盖的是卡片 §2.1 定义的
「host 或 port 变化」。这不是降低标准，而是把不可用的地址换成本机可用的同义形态。

### 4.3 追加 `Referer` 剥离

卡片与 roadmap 的敏感头是 `Authorization` / `Cookie` / `Proxy-Authorization`；
2026-11-15 的实现指令点名 `Referer`。实现按并集处理，四个头都剥，判据未改：

- 冻结输出的两条 `cross` / `same` 行保持原样；
- 额外增加 `cross referer 0` 与 `same referer 1` 两行作为追加断言。

### 4.4 基线固有缺陷：请求头此前从未发送

T2 红证暴露：phase 3 的 `header()` 只维护 slist，`send()` 从未设置
`CURLOPT_HTTPHEADER`。PH-9 在跟随循环里补上（每跳按是否跨源选择原列表或过滤后的列表）。
这不改变任何既有夹具的输出；`p3-*` 的断言只检查 slist 捕获，不检查线上字节。

### 4.5 `status()` 在降级拒绝时归 0

收到 302 后本地拒绝降级，`resp->status` 被显式置 0，与 `fill_local_error` 的本地
拒绝形态一致（未完成最终响应）。E-P9-6 的冻结锚点正是 `status 0`。

## 5. 门禁

```text
ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release, response ownership, ops, request config, response bytes, take_body, failure codes, error model, predicates, timeouts, streaming, cancellation, lifetime, redirects, credentials, downgrade)
G3_RC=0 G3_SECONDS=51
```

其余两条：

```text
ok g1 (card items, task columns, frozen criteria/tasks digests)
ok g0 (spec block, 8 rows, routing, deviations, session map, version 0.1.0)
ok g1-negative (27 tampered cards rejected)
```

## 6. 允许 / 不允许的变化

**允许（本相已发生）**

- `HttpRequest` 增加内部跟随状态（不新增公开字段）；
- 新增本地错误码 `-1009`（追加，不改旧值）；
- `send()` / `send_stream()` 内部从单次 `perform` 变为循环；
- 请求头第一次真正发到线上（PH-9 必需）；
- README、session-map、夹具、`g3` 扩项。

**不允许（本相未发生）**

- 不改 `follow_redirects(0)` 的任何行为（`p9-default` 守住）；
- 不改 `body` / `status` / `error` / `total_bytes` 的既有语义；
- 不改 `cancel` / 超时 / 流式分片的判据与输出；
- 不改 `docs/requirements.md` 与 `docs/design.md`（S-03 注记在上一提交）；
- 不新增任何重定向上限配置入口。

## 7. 当前事实

`follow_redirects(1)` 由 shim 逐跳跟随，上限固定 10 次重定向：10 个 302 的链成功、
共 11 个请求；11 个 302 时读 `47`、`/hop/11` 请求数 0、总请求 11。跨源（host 或 port）
剥离 `Authorization` / `Cookie` / `Proxy-Authorization` / `Referer`，同源保留；
https→http 读 `-1009` 且明文服务端 0 请求；A→B→A 在 11 个请求内以 `47` 结束；
默认不跟随行为未变。E-P9-1 的 `requests 4` 锚点不可达，其余七条通过。
