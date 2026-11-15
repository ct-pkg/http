# 会话地图

- applies-to: 0.1.0

| 类型 | 名称 | 负责 | 产物 |
| --- | --- | --- | --- |
| 常驻 | 需求池 | REQ | `issues/`、phase 立项 |
| 常驻 | 技术池 | ARCH | `docs/design.md`、battle 记录 |
| 常驻 | 评审 | REVIEW | 复核结论、`verified` 证据 |
| phase | `[ct-http] PH-<n> · <目标一句话>` | DEV / VERIFY | `docs/phases/PH-<n>/card.md` 与收口文档 |

## phase → 会话

| phase | 目标 | 会话标题 | 状态 |
| --- | --- | --- | --- |
| PH-1 | 接口冻结与可编译链 | `[ct-http] PH-1 · 接口冻结` | 已收口（`e6ce6c0`） |
| PH-2 | C shim 内核与显式释放 | `[ct-http] PH-2 · shim 与释放` | 已收口（`c9ba2d0`） |
| PH-3 | 请求配置与输入校验 | `[ct-http] PH-3 · 配置与校验` | 已收口（`9826ff5`） |
| PH-4 | 同步执行与响应读取面 | `[ct-http] PH-4 · 同步执行` | 已收口（`c0cf8f4`） |
| PH-5 | 错误模型与谓词真值表 | `[ct-http] PH-5 · 错误模型` | 已收口（`1afcd39`） |
| PH-6 | 三种超时区分 | `[ct-http] PH-6 · 超时` | 已收口（`9e78023`） |
| PH-7 | 流式执行与慢流保护 | `[ct-http] PH-7 · 流式` | 已收口（`290fa25`） |
| PH-8 | 取消与生命周期 | `[ct-http] PH-8 · 取消` | 已收口（PH-8 单提交） |
| PH-9 | 重定向与安全策略 | `[ct-http] PH-9 · 重定向` | 已收口（PH-9 单提交） |
| PH-10 | 跨平台、sanitizer、门禁与发布收口 | `[ct-http] PH-10 · 收口` | 开工（2026-11-15） |

## 约定

- 一个 phase 一个会话，不复用旧会话；标题按上表。
- 任务级清单写在 phase 卡里（`docs/phases/PH-<n>/card.md`），不在本文件重复。
- 卡片冻结后任务清单与退出判据不得中途修改（T-6）；偏差写进收口文档。
