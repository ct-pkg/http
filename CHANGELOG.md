# Changelog

本项目从 0.1.0 起记录。版本号规则见 `AGENTS.md` 6.7：MINOR 与 phase 对应、PATCH 与 fix 对应；
本号段（0.1.0）按 `deviation: V-001` 冻结，不随 phase 或 fix 移动。

## [0.1.0] - 2026-11-15

phase 1–10 交付：

- 公开面与 C shim 内核：三段静态宿主、显式释放、`HttpRequest` / `HttpResponse`。
- 请求配置与输入校验：URL 自持、头 CR/LF 拒绝、body 二进制安全、scheme 白名单。
- 同步执行与响应读取：真 `curl_easy_perform`、`take_body` 所有权转移。
- 错误模型：本地负数码、`CURLcode` 原值、谓词真值表。
- 三种超时：连接 / 总时长 / 空闲，`timeout_kind()` 可区分。
- 流式：分片交付、回调中止、响应体上限、真空闲超时。
- 取消与生命周期：同线程粘性取消；`send()` 不自动释放。
- 重定向与安全策略：手动逐跳、固定 10 次上限、跨源剥头、拒绝 `https → http`。
- 门禁：g0–g9 全部实现；`docs/errata.md` 勘误机制；`issues/` 台账。
