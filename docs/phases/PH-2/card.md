# PH-2 卡：C shim 内核与显式释放

- 编号：PH-2
- phase 目录：`docs/phases/PH-2/`
- version-minor：2
- 规范版本：1.9.0（开卡时 `AGENTS.md` 生成块头快照）
- 状态：开工（2026-11-15）
- criteria-sha256：86c135f0d9ae1dfe1ba58d24ee9b69c69156465ca5bd88b746cf9d7dcf004d8c
- tasks-sha256：711293249c49338cd20dbaa46e0ebb98bf8bcb8d60ad526c0d9ba74013861929

## 0. 编号与元信息

| 项 | 值 |
| --- | --- |
| 编号 | PH-2（与目录 `docs/phases/PH-2/` 一致） |
| 对应 roadmap | [phase 2](../planning/ct-http-0.1.0-roadmap.md#phase-2--c-shim-内核与显式释放) |
| 对应 feature | F-10、F-14 |
| 版本 | 0.1.0；`version-minor` 与 PH 号一致，PATCH 由 fix 数决定，不写进卡 |
| 不为 | 本卡不裁定 S-01…S-05；不 `perform`、不发网络、不做 header/body 的实际 setopt |

## 1. 目标

让 `curl_easy_init` / `curl_easy_cleanup` 真的发生且只发生一次，释放入口只在 shim 侧；CT 侧只多一个终态方法 `.free()`，不产生自动释放。

第三人复述：`CRequest *r = "url"->post(); r->free();` 之后 shim 的 init/cleanup 计数各为 1；`send()` 返回后请求仍可用；`http_request_free(&r)` 这类 C 槽位入口二次调用退出 0。

## 2. 范围

### 2.1 做什么

- `src/http/http.shim.c`：真 `.c` 链接单元，持有 libcurl easy handle、静态计数器、默认 ops 表。
- `src/http/http.shim.h`：只留声明与 struct 布局；`void *easy` 不暴露 curl 类型。
- `src/http/http.m.ct`：`RequestBuilder` 增加 `void free();`；`ResponseView` 增加 `void free();`。
- 显式释放：`http_request_free(struct HttpRequest **)` / `http_response_free(struct HttpResponse **)` 置空槽位；`http_request_release` / `http_response_release` 供 CT 终态方法使用。
- ops 函数指针表：`struct HttpRequestOps` + `http_request_set_ops()`；`send` 经 ops 派发。
- `test/run.sh` 的 `p2-*` 夹具：`--ldflags "-lcurl"`、C 夹具、ASAN 负例、`nm -u` 断言。
- `README.md` 的显式释放契约。

### 2.2 不做什么

- 不 `curl_easy_perform`、不访问网络；`send()` 仍返回固定值（phase 4 换真执行）。
- 不做 URL 深拷贝、header/slist、body、超时的实际 setopt（phase 3）。
- 不引入响应头读取（S-01）、`method()`（S-02）、`max_redirects`（S-03）、跨线程取消（S-04）、`HTTP_ERR_*`（S-05）。
- 不改 `docs/requirements.md`、`docs/design.md`；不 push、不打 tag。

## 3. 交付物清单

| 路径 | 内容 |
| --- | --- |
| `src/http/http.shim.h` | 两个 struct 布局 + 全部 `http_*` 声明（无 curl 类型） |
| `src/http/http.shim.c` | easy handle 生命周期、计数器、默认 ops、槽位释放 |
| `src/http/http.m.ct` | 既有三段映射 + `free()` 两个终态方法 |
| `test/fixtures/p2_cleanup_once.ct` + `.expected.out` | 创建 → `free()` → 计数 `1 0 1` |
| `test/fixtures/p2_after_send.ct` + `.expected.out` | `send()` 后计数 0；释放后 1 |
| `test/fixtures/p2_double_free.c` + `.expected.out` | 槽位二次 free 输出 `1 1 1` |
| `test/fixtures/p2_double_free_bad.c` | 绕过槽位的双释放，ASAN 必须报 `double-free` |
| `test/fixtures/p2_response_free.c` + `.expected.out` | 响应槽位二次 free 输出 `1 1` |
| `test/fixtures/p2_ops.c` + `.expected.out` | 替换 ops 后输出 `418 teapot 1` |
| `test/gates/g1_negative.sh` | 遍历所有 `PH-*/card.md`，逐卡三份篡改 |
| `test/run.sh` | `p2-cleanup-once`、`p2-nm`、`p2-double-free`、`p2-after-send`、`p2-response-free`、`p2-ops` |
| `tools/check.sh` | `g1` 判据前缀按卡号泛化；`g3` 纳入 `p2-*` |
| `README.md` | 显式释放、`import http::http`、`-lcurl` 说明 |
| `docs/session-map.md` | PH-2 会话登记 |
| `docs/phases/PH-2/red/T1.md` … `T6.md` | 每任务红证与转绿记录 |

## 4. 任务清单

| ID | 交付物 | 提交 | 回滚 | verify | effort | 覆盖 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 本卡、`tools/check.sh` 的 `g1` 前缀泛化、`test/gates/g1_negative.sh` 遍历所有卡 | `test(gate): PH2-T1 红` + `feat(gate): PH2-T1 g1 按卡号泛化` | `git revert` 实现提交后 `g1` 只认 E-P1，PH-2 卡变红；PH-1 不受影响 | `bash tools/check.sh g1 && bash test/gates/g1_negative.sh` | S | E-P2-1 |
| T2 | `http.shim.c`、真 `curl_easy_init`/`cleanup`、计数器、CT `.free()`、`p2_cleanup_once`、`p2_nm` | `test(http): PH2-T2 红` + `feat(http): PH2-T2 shim 内核` | `git revert` 实现提交后 `.free()` 消失，shim.c 删除，`g3` 回到旧夹具 | `bash test/run.sh p2-cleanup-once && bash test/run.sh p2-nm` | M | E-P2-2、E-P2-3 |
| T3 | 请求槽位释放 `http_request_free(**`、`p2_double_free.c`、`p2_double_free_bad.c` | `test(http): PH2-T3 红` + `feat(http): PH2-T3 双指针释放` | `git revert` 实现提交后槽位函数消失，CT `.free()` 仍可单次释放 | `bash test/run.sh p2-double-free` | M | E-P2-4 |
| T4 | 响应释放与 after-send：`http_response_free(**`、`res->free()`、`p2_after_send.ct`、`p2_response_free.c` | `test(http): PH2-T4 红` + `feat(http): PH2-T4 响应所有权` | `git revert` 实现提交后响应不能显式释放，after-send 夹具失败 | `bash test/run.sh p2-after-send && bash test/run.sh p2-response-free` | M | E-P2-5、E-P2-6 |
| T5 | ops 表：`struct HttpRequestOps`、`ops` 字段、`http_request_set_ops()`、`send` 派发、`p2_ops.c` | `test(http): PH2-T5 红` + `feat(http): PH2-T5 ops 替换` | `git revert` 实现提交后 `send` 回到直调，夹具编译失败 | `bash test/run.sh p2-ops` | M | E-P2-7 |
| T6 | `README.md` 释放契约与 `-lcurl` 说明、`docs/session-map.md` | `test(docs): PH2-T6 红` + `docs: PH2-T6 释放契约` | `git revert` 文档提交后 README 回到旧口径，不影响构建 | `bash tools/check.sh g0 && grep -q 'http_request_free' README.md` | S | E-P2-8 |

## 5. 退出判据（冻结）

| ID | 判据 | 核对命令或锚点 |
| --- | --- | --- |
| E-P2-1 | `g1` 按卡片自身 PH 号识别判据前缀；PH-1 与 PH-2 两张卡都过；负例逐卡拒绝 | `bash tools/check.sh g1` 退出 0；`bash test/gates/g1_negative.sh` 退出 0，输出每张卡三行 `rejected` |
| E-P2-2 | 真 easy handle：创建后 init==1，显式 `free()` 后 cleanup==1 | `bash test/run.sh p2-cleanup-once` 退出 0，输出 `1 0 1` |
| E-P2-3 | CT 生成对象不含 curl 符号；curl 只在 `.shim.c` | `bash test/run.sh p2-nm` 退出 0（`nm -u` 无 `_curl_`）；`grep -rn 'curl_easy_\|curl_slist_' src/http/*.m.ct src/http/*.h.ct` 零命中 |
| E-P2-4 | 槽位二次 free 退出 0；绕过槽位时 ASAN 必须报错 | `bash test/run.sh p2-double-free` 退出 0，输出 `1 1 1`；负例进程 rc≠0 且含 `double-free` |
| E-P2-5 | `send()` 不释放请求；响应仍可读 | `bash test/run.sh p2-after-send` 退出 0，输出 `0 200` 与 `1 1` |
| E-P2-6 | 响应释放走槽位，二次 free 安全，响应释放计数为 1 | `bash test/run.sh p2-response-free` 退出 0，输出 `1 1` |
| E-P2-7 | ops 运行时替换生效，CT 调用点与 `.m.ct` 不变 | `bash test/run.sh p2-ops` 退出 0，输出 `418 teapot 1` |
| E-P2-8 | README 写明唯一释放入口与 import 约定 | `bash tools/check.sh g0` 退出 0；`grep -q 'http_request_free' README.md` 退出 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 6. 红证方式

每个任务一对相邻提交：先 `test(...)` 提交夹具与红证，再 `feat(...)` 或 `docs:` 提交实现并转绿。红证写进 `docs/phases/PH-2/red/<task>.md`，最小字段：

```text
baseline: <实现前提交 sha>
命令: <verify 命令>
原始输出: <完整输出>
退出码: <rc>
green: <转绿提交 sha> 后同命令退出 0
```

- 红证必须用 `baseline` 提交复跑，不能拿工作树当前产物冒充。
- T1 的红是「PH-2 卡存在时旧 `g1` 报 `E-P1` 前缀不匹配」；先落卡再跑旧门禁留证。
- T2 的红是 `p2_cleanup_once` 构建失败（`no method 'free'`），不是 runner 未接线。
- T3 的红是 C 夹具找不到 `http_request_free` 槽位声明；T4/T5 同理按缺少的符号/方法取红。
- T6 的红是 `grep` 断言失败（README 尚无释放契约），不是「没有代码改动」。

## 7. 依赖

- `ct v0.44.1`（`/Users/samxx/.local/bin/ct`），`.shim.c` 自动进入 cc 行。
- 系统 libcurl 8.7.1（SDK 头 + `-lcurl`）；系统 `cc` 支持 `-fsanitize=address`。
- 前置：PH-1 收口 `e6ce6c0`；`bash tools/check.sh g0` 与 `g3` 在开工前为绿。
- 实测：`ct build` **不**读取 `ct.pkg` 的 `ldflags`；`test/run.sh` 必须显式传 `--ldflags "-lcurl"`。

## 8. 已知风险

| 风险 | 触发信号 | 处置 |
| --- | --- | --- |
| CT 方法无法回传置空后的指针 | 映射展开成 `http_request_free(&(struct HttpRequest *){r})`，槽位置空只作用于临时量 | CT 侧只提供终态 `.free()`；幂等双释放由 C 槽位入口保证，README 明写「CT 侧只调一次」 |
| `ct.pkg` 的 `-lcurl` 不生效 | cc 行无 `-lcurl`，链接报 `_curl_easy_init` 未定义 | `test/run.sh` 显式 `--ldflags "-lcurl"`；`ct.pkg` 保持元数据，不改 |
| ASAN 与 libcurl 全局状态互相干扰 | LSan 在退出时报告 curl 全局泄漏 | ASAN 负例只链 shim 的自由路径，`ASAN_OPTIONS=detect_leaks=0` |
| 计数器跨夹具串味 | 同一进程内前一个夹具的计数被读到 | 每个夹具独立进程运行，不共享计数器 |
| `send()` 仍为固定值被误认为真请求 | 输出恰好 `200` 但无网络 | 卡与 README 明写 stub；真执行在 PH-4 |
| 结构体加字段破坏 PH-1 ABI 夹具 | `phase1_abi.c` 字段顺序断言失败 | 新字段只追加在既有字段之后；`phase1_abi` 纳入 `g3` 回归 |

## 9. 收口动作

1. 逐条跑 E-P2-1…E-P2-8，把命令、原始输出、退出码贴进 `docs/phases/PH-2/closeout.md`。
2. 对照 §4 任务表，逐个确认「红证提交 → 实现提交」相邻且 `baseline` 是父提交。
3. 检查 §3 交付物清单每个路径都已入库，无 `build/` 产物或临时文件混入。
4. 偏差只写进收口文档，不改本卡 §4/§5；判据如需变更，关闭 PH-2 并新开卡。
5. 版本：保持 `0.1.0`，不移动 MINOR；不 push、不打 tag。
6. 报告一行当前事实：最小调用 `ct build` 与运行各返回什么、是否真发请求（本 phase 仍是固定值），以及 init/cleanup 计数。
