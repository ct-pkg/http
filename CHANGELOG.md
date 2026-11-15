# Changelog

本项目从 0.1.0 起记录。版本号规则见 `AGENTS.md` 6.7：MINOR 与 phase 对应、PATCH 与 fix 对应；
0.1.0 号段按 `deviation: V-001` 冻结；自 0.2.0 起解除冻结，偏移 `d = 9` 永久保留。

## [0.2.1] - 2026-11-16

清单与分发的 patch 重发（无接口改动）：

- `ct.pkg` 的 `version` 由 `0.2.0` 升至 `0.2.1`；`publisher = ct` 与 `ns-root = .` 随本次清单发布。
- 起因：`v0.2.0` tag 内的 `ct.pkg` 缺 `publisher` 与 `ns-root`；且全局 store 已存在同版本号、
  摘要不同的内容（`already exists with a different digest`），同版本无法重发，故改出 patch 版本。
- `applies-to` 与 README「版本」行同步为 `0.2.1`；许可与身份字段取值未改。

## [0.2.0] - 2026-11-15

PH-11 发布相（无新增功能面）：

- 编译器下限：`requires = ct >= 0.53.1`。依据是安装接口——`ct install` 必须打印
  `ct build -I vendor/http …`；`ct v0.49.0` 仍打印 `ct-ast build`，安装样例失败。
  旧的下限理由（`impl T { static T *new(); }`，0.34.0）已随公开面改为 `map trait` 失效。
- `applies-to` 全量同步：19 个用户面文档统一为 `0.2.0`。
- 标签口径：annotated `v0.2.0` 指向收口提交并按 V-7 四项校验；`v0.1.0` 不移动。
- 版本号随 PH-11 收口移动到 `0.2.0`（MINOR=2，`deviation: V-001` 的 `d = 9`），PATCH 归零。

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
