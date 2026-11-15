# PH-9 卡：重定向与安全策略

- 编号：PH-9
- phase 目录：`docs/phases/PH-9/`
- version-minor：9
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15，卡已冻结）
- criteria-sha256：ec52cf53bd58908794791d5b81736e4fda234c606d7c830cb384fd4aec201268
- tasks-sha256：d8becb2f5c216ecfcca44f43b016bfb7a2483f294f159ebb88e74eefc6833f7a

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-9（与目录 `docs/phases/PH-9/` 一致） |
| 对应 roadmap | [phase 9](../planning/ct-http-0.1.0-roadmap.md#phase-9--重定向与安全策略) |
| 对应 feature | F-08、F-09 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 已裁定 | S-03：`follow_redirects(1)` 上限**固定 10 次重定向**，不暴露配置项；裁决见 `docs/reviews/PH-9/S-03-ruling.md`，实测见 `probe-maxredirs.md` |
| 不为 | 不新增 `max_redirects(n)`；不读/暴露响应头（S-01 未纳入）；不做白名单；不 push、不打 tag；不做 PH-10 的 g4–g9 |

## 1. 目标

`follow_redirects(1)` 时按可解释的规则跟随：最多 10 次重定向，跨源不转发
`Authorization` / `Cookie` / `Proxy-Authorization`，默认拒绝 https→http 降级；
`follow_redirects(0)` 的行为一字不变。所有边界用本地回环夹具锁定，请求计数由服务端
子进程证明。

第三人复述：同主机 1 跳与 2 跳都返回 200；10 跳链成功、共 11 个请求；
11 跳链读 `47`、`/hop/11` 请求数 0、共 11 个请求；A→B 跨源后 B 看不到三个敏感头，
同源保留；A→B→A 循环在 11 个请求内以 `47` 收场；https→http 读 `-1009` 且明文服务端
计数 0；默认不跟随仍是 `status 302 / error 0 / is_ok 0 / 请求 1`。

## 2. 范围

### 2.1 做什么

- **内部跟随循环**（不用 `CURLOPT_FOLLOWLOCATION`）：每跳之后读 `Location`，
  由 shim 决定是否继续、下一跳带哪些头。这样 host/scheme 的逐跳判定才有落点。
- 上限：最多 **10 次重定向**（S-03 裁决）。第 11 个 3xx 不再跟随：
  `error()==47`（`CURLE_TOO_MANY_REDIRECTS`），`status()` 为最后一个 3xx，
  目标请求不发。
- 只跟随 `301` / `302` / `303` / `307` / `308`；`Location` 支持绝对 URL 与以 `/`
  开头的相对路径（相对路径按当前 origin 解析）。
- 方法改写与库默认一致：`301`/`302`/`303` 把 POST 改成 GET 并丢弃请求体；
  `307`/`308` 保留方法与请求体。
- 中间响应的 body 丢弃，只有最终响应进入 `body()` 或流式回调；`send_stream`
  不得把中间 3xx 的 body 交给调用方。
- 跨源剥离：目标 host 或 port 与当前 origin 不同时，下一跳不发送
  `Authorization` / `Cookie` / `Proxy-Authorization`；同源保留。host 比较大小写不敏感。
- 降级拒绝：`https → http` 在发出下一跳前本地拒绝，`HTTP_LOCAL_ERR_REDIRECT (-1009)`，
  `error_detail()` 形如 `code=-1009 stage=redirect host=…`，明文服务端计数 0。
  `http → https`、同协议跳转不受影响。
- 循环：不额外做 URL 去重，由 10 次上限兜底；A→B→A 在 11 个请求内以 `47` 结束。
- 新增 `test/fixtures/p9-server.c/.h`：可绑定指定回环地址与端口自动分配、可配置链长、
  按路径计数（总计数 + `/hop/<n>` 计数）、回显请求头、循环路由、跨实例跳转。
- 夹具 `p9-same-host`、`p9-hops10`、`p9-hops11`、`p9-cross-host`、`p9-loop`、
  `p9-downgrade`、`p9-default`；`g3` 接线；README 重定向面；`docs/session-map.md`；
  红证进 `docs/reviews/PH-9/`。
- `docs/requirements.md` R-08 非目标旁的一句注记已随 S-03 裁决落地；本相不改其他条款。

### 2.2 不做什么

- 不新增 `max_redirects(n)` 或任何上限配置入口（S-03）。
- 不读、不缓存、不暴露响应头（S-01 未纳入）；`Location` 只在 shim 内部使用。
- 不做跳转白名单、不做 DNS 重绑定防护、不做 `300`/`304`/`305`/`306` 的跟随。
- 不把 `Cookie` 做成 cookie 引擎：只剥调用方显式设置的 `Cookie` 头。
- 不改 `docs/design.md`；不改 R-08 以外的规范条款。
- 不做 PH-10 的跨平台矩阵、`g4`–`g9` 与发布材料。
- 不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | `-1009` 宏、跟随语义与上限注释、内部 Location 读取说明 |
| `src/http/http.shim.c` | 跟随循环、10 跳上限、方法改写、中间 body 丢弃、跨源剥头、降级拒绝 |
| `test/fixtures/p9-server.c` / `.h` | 可绑定地址/链长/按路径计数/回显头/循环/跨实例 |
| `test/fixtures/p9-same-host.c` | 同主机 1 跳与 2 跳 |
| `test/fixtures/p9-hops10.c` | 10 跳成功、11 个请求 |
| `test/fixtures/p9-hops11.c` | 11 跳读 47、`/hop/11` 计数 0 |
| `test/fixtures/p9-cross-host.c` | 127.0.0.1 → 127.0.0.2 剥头；同源保留 |
| `test/fixtures/p9-loop.c` | A→B→A 有界结束 |
| `test/fixtures/p9-downgrade.c` | https→http 读 `-1009`，明文计数 0 |
| `test/fixtures/p9-default.c` | 默认不跟随回归 |
| `test/run.sh`、`tools/check.sh` | 七个夹具接线并入 `g3`；TLS 夹具缺依赖时退出 2 |
| `README.md`、`docs/session-map.md` | 重定向面、上限、剥头、降级、PH-9 登记 |
| `docs/reviews/PH-9/T1.md` … `T5.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 内部跟随循环 + 10 跳上限 + 方法改写 + `p9-server`；`p9-same-host`、`p9-hops10`、`p9-hops11` | `feat(http): PH9 重定向与安全策略`（整相一提交） | revert 后无跟随循环，`p9-same-host` 只读 302，`p9-hops10` 红 | `bash test/run.sh p9-same-host && bash test/run.sh p9-hops10 && bash test/run.sh p9-hops11` | M | E-P9-1、E-P9-2、E-P9-3 |
| T2 | 跨源剥头 + 同源保留；`p9-cross-host` | 同上 | revert 后三个头跨源照发，断言红 | `bash test/run.sh p9-cross-host` | M | E-P9-4 |
| T3 | 循环有界；`p9-loop` | 同上 | revert 后循环无上限，请求数超出，断言红 | `bash test/run.sh p9-loop` | S | E-P9-5 |
| T4 | https→http 降级拒绝 + TLS 夹具；`p9-downgrade` | 同上 | revert 后降级被跟随，明文计数非 0，断言红 | `bash test/run.sh p9-downgrade` | M | E-P9-6 |
| T5 | 默认不跟随回归 + README/`g3`/session-map/收口；`p9-default` | 同上 | revert 后 README 无重定向上限说明，E-P9-8 的 grep 红 | `bash test/run.sh p9-default && bash tools/check.sh g3` | S | E-P9-7、E-P9-8 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P9-1 | 同主机 1 跳与 2 跳均返回 200、body 正确，请求数分别为 2 与 4 | `bash test/run.sh p9-same-host` 退出 0，输出 `one status 200 error 0 requests 2 body_ok 1`、`two status 200 error 0 requests 4 body_ok 1` |
| E-P9-2 | 10 跳链成功：`status 200`、`error 0`、服务端共 11 个请求 | `bash test/run.sh p9-hops10` 退出 0，输出 `status 200 error 0 requests 11 body_ok 1` |
| E-P9-3 | 11 跳链读 `47`、`status 302`、总请求 11、`/hop/11` 请求数 0 | `bash test/run.sh p9-hops11` 退出 0，输出 `status 302 error 47 requests 11 hop11 0 ok 0` |
| E-P9-4 | 跨源（127.0.0.1→127.0.0.2）B 看不到三个敏感头；同源保留三个头 | `bash test/run.sh p9-cross-host` 退出 0，输出 `cross auth 0 cookie 0 proxy 0 status 200`、`same auth 1 cookie 1 proxy 1 status 200` |
| E-P9-5 | A→B→A 循环以 `47` 结束，请求数不超过 11，不卡死 | `bash test/run.sh p9-loop` 退出 0，输出 `error 47 requests 11 status 302` |
| E-P9-6 | https→http 本地拒绝：`error -1009`、明文服务端计数 0、`status 0` | `bash test/run.sh p9-downgrade` 退出 0，输出 `error -1009 status 0 plaintext 0` |
| E-P9-7 | 默认不跟随不变：`status 302`、`error 0`、`is_ok 0`、请求 1 | `bash test/run.sh p9-default` 退出 0，输出 `status 302 error 0 ok 0 requests 1` |
| E-P9-8 | 门禁全绿；README 写明 10 跳上限、跨源剥头与降级拒绝 | `bash tools/check.sh g0 && bash tools/check.sh g1 && bash tools/check.sh g3` 退出 0；`grep -q '10 跳' README.md` 与 `grep -q 'Proxy-Authorization' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

本相沿用「一相一提交」口径：实现前在同一工作区跑夹具并原样记录输出，实现后复跑；
红证写进 `docs/reviews/PH-9/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <实现后同一命令的输出与退出码>
```

- T1 的红是构建红（`p9-server` 与跟随循环尚不存在）与行为红（默认不跟随，只读 302）。
- T2 的红是行为红：剥离逻辑缺失时跨源仍转发三个头。
- T3 的红是行为红：无上限时循环请求数超过 11。
- T4 的红是行为红：降级被跟随，明文服务端计数非 0。
- T5 的红是 `grep -q '10 跳' README.md` 失败。
- 降级夹具依赖 `python3 -c 'import ssl'` 与 `openssl`；缺失时运行器退出 2，
  不得返回 0（门禁无法运行不许伪装绿）。
- 其余结论全部来自本地回环真实网络；不使用注入钩子。

## 7. 依赖

- 依赖 CT 登记版本 `v0.44.2`（已发版、tag 已推）；本机门禁实际执行的二进制为
  `ct v0.44.1`，本相不使用 0.44.2 新特性。
- 系统 libcurl 8.7.1（`CURLOPT_HEADERFUNCTION`、`CURLINFO_RESPONSE_CODE`）；
  S-03 实测见 `docs/reviews/PH-9/probe-maxredirs.md`。
- 前置：PH-8 收口 `2b8e411`；`g0`、`g1`、`g3` 开工前为绿。
- 回环地址 `127.0.0.1` 与 `127.0.0.2`（跨源夹具）；TLS 夹具依赖 `python3` 的 `ssl`
  模块与 `openssl` 生成自签证书。
- 测试时长目标 <15 秒；跳转链全部在本地回环，无真实外网。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| 手动跟随与库的 302 方法改写不一致 | 同主机 POST 用例读 405 | 夹具锁定 301/302/303→GET、307/308 保留；按 curl 文档语义实现 |
| 中间 3xx 的 body 污染流式回调 | `p9-*` 流式用例 total 偏大 | 每跳只有最终响应启用写回调；中间响应用丢弃 sink |
| `localhost` 解析到 `::1` 导致跨源夹具连不上 | 连接拒绝 7 | 跨源用 `127.0.0.1` 与 `127.0.0.2`，不用域名 |
| TLS 夹具依赖 python3/openssl | 环境缺依赖 | 运行器显式退出 2 并说明缺什么；CI 需记录该依赖 |
| 上限计数与「跳」的口径歧义 | 10/11 跳断言争议 | 按 S-03 裁决：计数单位为**重定向次数**，判据同时给出总请求数与 `/hop/11` 计数 |
| 跨源剥头漏掉端口变化 | 同 host 不同 port 仍带头 | 判定用 host+port+scheme 的 origin 比较；夹具覆盖 host 变化 |
| `-1009` 与既有本地码冲突 | `error()` 读到未知负码 | `-1001…-1008` 已占用，`-1009` 为追加，不改旧值 |
| 降级夹具的 https 服务端不可用 | `p9-downgrade` 退出 2 | 门禁失败而非跳过；收口注明依赖 |

## 9. 收口动作

1. 逐条跑 E-P9-1…E-P9-8，把命令、原始输出、退出码贴进 `docs/phases/PH-9/closeout.md`。
2. 对照 §4 确认五个任务都有交付物与 verify；红证文档齐全。
3. 检查 §3 每个路径入库，无 `build/` 产物。
4. 偏差只写进收口，不改本卡 §4/§5。
5. 收口单列：10 跳口径与请求计数、跨源判定范围、降级拒绝的错误码、方法改写表、
   中间 body 处理、TLS 夹具依赖、PH-9 套件时长。
6. 版本保持 `0.1.0`；不 push、不打 tag。
7. 报告给出：判据逐条结果、门禁末行、提交号、下一相。
