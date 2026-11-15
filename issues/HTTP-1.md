# HTTP-1 phase 3 的 header() 从未设置 CURLOPT_HTTPHEADER

- status: verified
- 证据: `docs/reviews/PH-10/HTTP-1-verify.md`（基线 `d0160db^` 的 shim + 当前夹具：`same auth 0`；HEAD：`same auth 1`）；`docs/phases/PH-9/closeout.md` §4.4；`docs/reviews/PH-9/T2.md`；修复提交 `d0160db`
- phase: PH-9
- fix: yes
- 发现: 2026-11-15（PH-9 的 T2 红证）

## 问题

phase 3 的 `header()` 只维护 `curl_slist`，`send()` 从未设置 `CURLOPT_HTTPHEADER`，
所以 2026-11-15 之前的任何请求都没有把请求头发到线上。

## 处置

PH-9 在逐跳跟随循环里补上 `CURLOPT_HTTPHEADER`（每跳按是否跨源选择原列表或过滤后的列表），
修复提交 `d0160db`。`p9-cross-host` 的同源行 `same auth 1 cookie 1 proxy 1` 与 `same referer 1`
是「头真的发了」的对照断言。

## 假绿复核

phase 3 卡片的 E-P3-2 只断言 `curl_slist` 内容（`p3-header-crlf` 打印
`rejected 1 1 added 0 1 first Accept: application/json`），**没有**断言线上字节，
所以 phase 3 的验收没有产生「头已发出」的假绿；假绿风险在 README 的配置面叙述与
用户预期，不在冻结判据。PH-9 收口 §4.4 已如实登记。复核结论写在
`docs/phases/PH-10/closeout.md` §4.2。

## 复核（2026-11-15）

- 复核人：主控（用户授权）
- 方法：基线 `d0160db^` 的 shim 配当前夹具（消融法；基线上没有该夹具，夹具与断言不变），
  对照 HEAD 同一条命令。
- 结果：基线同源行 `same auth 0 cookie 0 proxy 0` / `same referer 0`；HEAD
  `same auth 1 cookie 1 proxy 1` / `same referer 1`。跨源行两版均为 0（基线是空过，
  HEAD 是真实剥离），同源行是「头确实上线」的对照。
- 原始输出：`docs/reviews/PH-10/HTTP-1-verify.md`。
- 状态转换：`open` → `verified`（按 5.4，REV 视角由主控以用户授权代签）。

## 未闭合项

`fix: yes` 但 `0.1.0` 号段按 `deviation: V-001` 不移动 PATCH；这是版本号历史的已知落差，
在发布 0.2.0 时随 `g4-version` 首次对账一并收敛。
