# 包身份与安装路径流程待办

- applies-to: 0.2.0
- 登记日期：2026-11-15（2026-11-15 按能力现状重判）
- 说明：本页只记本包自己的待办。每条写清「为什么需要 / 要做什么 / 前置条件 / 验收方式 / 当前定性」；
  不引用外部阶段号或版本号。

## 0. 本轮烟雾测试（2026-11-15，新工具链）

本机工具链：`/Users/samxx/.local/bin/ct`。`~/.local/bin` 不在 PATH，以下全部用显式路径调用。
测试在 `/tmp` 的仓库副本里做，工作区 `ct.pkg` 未参与、未提交。

| # | 实验 | 原文输出 | 结论 |
| --- | --- | --- | --- |
| 1 | `ct get <副本>` 后的布局 | `vendored 1 package(s)`；`vendor/http` 存在，同时 `~/.ct/pkg/ct/http/0.2.0` 已落盘 | 兼容副本仍生成；store 按发布方/包名/版本分层 |
| 2 | `cat ct.lock` | `# ct-lock 2` / `ct/http 0.2.0 dir /tmp/sm1 - . sha256:3d68…` | 发布方 `ct`、单元根 `.` 都在 lock 里 |
| 3 | 删掉 `publisher`/`ns-root` 后再 `ct get` | `vendored 1 package(s)`，rc=0；lock 退回单行 `http 0.2.0 dir …` | `ct get` 不强制这两项；强制点在 `ct pkg check` |
| 4 | `ct pkg check <dir>` | 有字段：`ok: 4 criteria`；缺字段：`'/tmp/sm2/ct.pkg' is missing 'publisher'`、`… is missing 'ns-root'` | 子命令存在，按目录校验四项 |
| 5 | 不写 `-I` 直接 `ct build` | `'cc' -std=c99 -I '/tmp/sm1/http' …`；产物运行输出 `200 {"users":3}` | import 可解析，include 根自动补齐 |

补充证据（消费端 `ct install`）：

```console
$ ct install
ct build -I /Users/samxx/.ct/pkg/ct/http/0.2.0 --ldflags '-lcurl' quickstart.ct -o build
# lock-format: v2
```

结论：**store、lock v2、发布方与单元根、构建自动补 include 根都已具备**；
`publisher`/`ns-root` 的强制点在包校验（`pkg check`），不在 `get`。

## 1. 总表

| # | 任务 | 本轮定性 | 依据 | 状态 |
| --- | --- | --- | --- | --- |
| 1 | 包身份字段 | **行为已验收** | store 落盘、lock v2 含发布方与单元根、`pkg check` 通过；两行仍未提交 | 未提交（按要求） |
| 2 | 安装测试改断 store 路径 | **可开工** | `ct install` 已打印 store 根，断言可改 | 未实施 |
| 3 | 去手算 include 路径 | **可开工** | 无 `-I` 构建成功并输出 200 | 未实施 |
| 4 | 阻塞清单与发布说明收口 | **可开工** | 纯文档，不依赖工具链 | 进行中：清单 3/5 |

## 2. 逐条

### 1) 包身份字段

- **为什么需要**：包清单只有 `name` / `version` / `ldflags` / `requires` 时，装到哪个 store、
  由谁发布都表达不了；消费端无法按发布者校验，索引也无法把包归到发布方名下。
- **要做什么**：`ct.pkg` 增加 `publisher = ct` 与 `ns-root = .`。
- **前置条件**：无（字段语义已具备）。工作区已有这两行。
- **验收方式**：
  - `grep -c '^publisher = ct$' ct.pkg` 输出 `1`；`grep -c '^ns-root = \.$' ct.pkg` 输出 `1`；
  - `ct get <本地目录 spec>` 退出 0，且包落在 store 目录；
  - `ct install` 打印的 `-I` 指向 store。
- **当前定性**：行为已验收。`ct pkg check .` 输出 `ok: 4 criteria`；`ct install` 打印
  `-I /Users/samxx/.ct/pkg/ct/http/0.2.0`；lock 为 `# ct-lock 2` 且含发布方与单元根。
  两条 `grep` 判据要等这两行提交后才算数（本轮按要求不提交）。

### 2) 安装测试改断 store 路径

- **为什么需要**：`test/gates/p10_install.sh` 把 include 根写死成 `-I vendor/http`。
  安装根换成 store 后，这条断言要么假绿、要么无谓失败，不能反映真实布局。
- **要做什么**：断言 `ct install` 打印 `ct build`，并断言 `-I` 指向 store 目录；删掉 `vendor/` 断言。
- **前置条件**：本机能观察到 store 布局（`ct get` 落 store、`ct install` 打印 store 根）。
- **验收方式**：
  - `grep -c 'vendor/' test/gates/p10_install.sh` 输出 `0`；
  - `bash test/gates/p10_install.sh` 退出 0，输出 `install ok` 与 `200`；
  - `test -d <store 目录>` 退出 0。
- **当前定性**：**可开工**，阻塞已解除。`ct install` 现在打印
  `-I /Users/samxx/.ct/pkg/ct/http/0.2.0`，把断言改成 store 路径能真绿；脚本尚未改。
- **现状**：脚本仍有 5 处 `vendor/` 断言（第 13、77、78、79、85 行）。

### 3) 去手算 include 路径

- **为什么需要**：`test/`、`tools/` 里手拼 `-I vendor/`，路径形态一变就要全改，且容易与
  安装输出不一致；构建既然默认读 lock，就不该再由脚本手算。
- **要做什么**：删除手写路径，改用安装输出或 lock 提供的 include 根。
- **前置条件**：同 2。
- **验收方式**：
  - `grep -rn -- '-I vendor/' test tools` 零命中（退出 1）；
  - 全仓零命中是目标；历史卡片与评审记录里保留的旧契约原文是否一并改写见 §5；
  - `bash tools/check.sh g0 … g9` 结果不受影响。
- **当前定性**：**可开工**。不写 `-I` 的 `ct build` 已能解析 `import`，cc 行自动带
  `-I '/tmp/sm1/http'`，产物运行输出 `200 {"users":3}`；脚本尚未改。

### 4) 阻塞清单与发布说明收口

- **为什么需要**：0.1.0 的阻塞项与对外发布口径必须有本仓唯一落点，读者才不会两头找。
- **要做什么**：`issues/` 覆盖 HTTP-1…HTTP-5，每条有状态、证据与结论；对外发布说明在本仓固定落点。
- **前置条件**：无。
- **验收方式**：
  - `ls issues/HTTP-{1,2,3,4,5}.md` 五件齐全；
  - 每件含 `status`、证据与结论；
  - 对外发布说明有固定文件（建议 `docs/release-notes.md`）。
- **当前定性**：**可立即开工**。本仓有 [HTTP-1](../../issues/HTTP-1.md)、
  [HTTP-2](../../issues/HTTP-2.md)、[HTTP-3](../../issues/HTTP-3.md)；
  **HTTP-4、HTTP-5 本仓尚无该条正文，需自行补齐**。

## 3. 工作区待决项：`ct.pkg`

- **现状**：`git status --porcelain` 输出 ` M ct.pkg`；diff 只有两行新增：
  `publisher = ct`、`ns-root = .`。`version`、`requires`、`ldflags` 均未改。
- **本轮处置**：不提交、不丢弃，原样留在工作区。
- **建议**：第 1 条行为已验证；建议与第 2、3 条同一相落地、一次验收。若先单独落字段，
  `ct pkg check .` 与 lock 的 `# ct-lock 2` 也能当场验证，不会再出现“字段无效”的假象。
- **风险**：长期挂 diff 容易被误提交；若 owner 同意，可先 `git stash` 存档。

## 4. 需要 owner 拍板

1. **HTTP-4 / HTTP-5 正文**：本仓没有原文，由谁、何时补齐？
2. **第 3 条的验收口径**：全仓 `-I vendor/` 零命中，还是只要求 `test/` + `tools/` 零命中？
   历史卡片与评审记录里的旧契约原文是否保留。
3. **`ct.pkg` 两行的处置**：继续挂工作区，还是先 stash 存档。
4. **对外发布说明的落点**：新建 `docs/release-notes.md`，还是并入 README 的「版本」段。

## 5. 本仓的分期判断（重判后）

分期由本仓自己定，依据只有两条：**能不能现在验收**、**会不会返工**。

- **第 1–3 条仍合并成一个相**：三条共用同一次验收、同一批文件（`ct.pkg` + `p10_install.sh`），
  拆开会出现“字段已加但无人读”的中间态，且三次触同一批文件必返工。
- **该相的入口条件已满足**：烟雾测试里 store 落盘、lock v2 含发布方与单元根、
  `ct install` 打印 store 根三项都真跑到了（§0），可以开卡并按真判据验收。
- **拆步顺序**：1 → 2 → 3。先加字段，再改安装断言，最后清手算路径；2、3 依赖新的 include 根稳定。
- **完成度**：4 条里 0 条落地；第 4 条清单 3/5，发布说明未建。
- **推不推**：第 4 条**现在就推**；第 1–3 条**已解除阻塞、可以开相**，但不单独占号，
  相位号与版本号届时按 `AGENTS.md` 6.7 现算。

## 6. 与当前相位的关系

这四条**不并入 PH-12**（`method()` + `content_type()`）：那是产品增量，这四条是安装契约对齐，
两者验收路径不同。PH-12 的登记见 [roadmap](ct-http-0.1.0-roadmap.md) §5；
本页只做登记，不改 `ct.pkg`、不打 tag、不 bump。
