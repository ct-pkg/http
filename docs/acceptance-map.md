# 验收映射（R-* → ACC-*）

- applies-to: 0.1.0

每个需求（`docs/requirements.md` 的 `R-nn`）至少映射到一条 `ACC-*`；每条 `ACC-*` 的锚点
必须是仓库里真能跑的东西：一个已接线的夹具、一道门禁、或 `test/gates/` 下的脚本。
`bash tools/check.sh g2` 逐行核对本表。

| ACC | 要求 | 判据锚点 | 覆盖 |
| --- | --- | --- | --- |
| ACC-01 | R-01 | `bash tools/check.sh g0` | 包清单键齐全、版本落点一致、规约纳管 |
| ACC-02 | R-02 | `bash test/run.sh p2-cleanup-once` | 构造/释放配对，cleanup 恰好一次 |
| ACC-03 | R-02 | `bash test/run.sh p2-double-free` | 二次释放 no-op（ASAN 正负例） |
| ACC-04 | R-03 | `bash test/run.sh p3-url-own` | URL 深拷贝，覆写调用方缓冲仍持有原值 |
| ACC-05 | R-04 | `bash test/run.sh p3-header-crlf` | 头名/值含 CR/LF 本地拒绝，合法头进 slist |
| ACC-06 | R-04 | `bash test/run.sh p9-cross-host` | 请求头真发到线上；跨源剥离、同源保留 |
| ACC-07 | R-05 | `bash test/run.sh p3-body-nul` | body 按显式长度存储，NUL 不截断 |
| ACC-08 | R-06 | `bash test/run.sh p6-total` | 总时长超时读 `28/2` |
| ACC-09 | R-06 | `bash test/run.sh p6-connect` | 连接超时读 `28/1`，服务端计数 0 |
| ACC-10 | R-07 | `bash test/run.sh p3-defaults` | UA 默认 `ct-http/0.1.0`，setter 落值 |
| ACC-11 | R-08 | `bash test/run.sh p9-hops10` | 10 次重定向成功，共 11 个请求 |
| ACC-12 | R-08 | `bash test/run.sh p9-hops11` | 第 11 次重定向拒绝，读 `47` |
| ACC-13 | R-09 | `bash test/run.sh p9-downgrade` | `https → http` 在下一跳前本地拒绝 |
| ACC-14 | R-10 | `bash test/run.sh p4-status` | `send()` 真执行，状态与 body 来自网络 |
| ACC-15 | R-11 | `bash test/run.sh p4-take-body` | 读取面与 `take_body` 所有权转移 |
| ACC-16 | R-12 | `bash test/run.sh p7-chunks` | 流式按到达顺序交付分片 |
| ACC-17 | R-12 | `bash test/run.sh p7-abort` | 回调返回非 0 → `23`，非取消 |
| ACC-18 | R-13 | `bash test/run.sh p8-cancel` | 同线程取消：`42` / `is_cancelled 1` |
| ACC-19 | R-14 | `bash test/run.sh p5-table` | 三类错误不混，码表逐项可核 |
| ACC-20 | R-15 | `bash test/run.sh p2-nm` | C shim 单一边界，导出符号可核 |
| ACC-21 | R-16 | `bash test/run.sh p8-lifetime` | send 后读取请求字段安全（ASAN 正例） |
| ACC-22 | R-17 | `bash tools/check.sh g7` | 系统 libcurl、无 vendored、sanitizer 全绿 |
| ACC-23 | R-18 | `bash tools/check.sh g1` | 判据可核对、卡片冻结、勘误可追溯 |
| ACC-24 | R-19 | `bash test/gates/p10_install.sh` | 清洁目录安装与样例 `200` |
