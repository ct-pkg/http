# 会话地图

| 类型 | 名称 | 负责 | 产物 |
| --- | --- | --- | --- |
| 常驻 | 需求池 | REQ | `issues/`、phase 立项 |
| 常驻 | 技术池 | ARCH | `docs/design.md`、battle 记录 |
| 常驻 | 评审 | REVIEW | 复核结论、`verified` 证据 |
| phase | `[ct-http] H<n> · <目标一句话>` | DEV / VERIFY | `docs/phases/H<n>.md` 的收口对照表 |

## phase → 会话

| phase | 目标 | 会话标题 | 状态 |
| --- | --- | --- | --- |
| H1 | 骨架、构造与 Drop、七个配置方法 | `[ct-http] H1 · 骨架与配置` | 规划中（卡见 [phases/H1.md](phases/H1.md)） |
| H2 | `post()` 同步请求 | `[ct-http] H2 · post()` | 规划中 |
| H3 | `post_stream(cb, ud)` | `[ct-http] H3 · 流式回调` | 规划中 |
| H4 | `cancel()` / `is_cancelled()` | `[ct-http] H4 · 取消` | 规划中 |
| H5 | 错误映射 | `[ct-http] H5 · 错误映射` | 规划中 |
| H6 | C shim 对接 libcurl 与收口 | `[ct-http] H6 · shim 收口` | 规划中 |

## 约定

- 一个 phase 一个会话，不复用旧会话；标题按上表。
- 任务级清单写在 phase 卡里（`docs/phases/H<n>.md`），不在本文件重复。
- 卡片冻结后任务清单不得中途修改（T-6）。
