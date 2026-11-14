# PH-5 收口对照：错误模型与谓词真值表

- 卡片：[docs/phases/PH-5/card.md](card.md)
- 判定：E-P5-1…E-P5-5 全部通过；T3 无独立红证，偏差见 §4。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=bde4120a…f2ef`、`tasks-sha256=f0b2b5e2…b740`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P5-1 | `bash test/run.sh p5-local` | `ok`，输出 `arg -1001`、`header -1002`、`body -1003`、`scheme -1004`、`timeout -1005`、`no_handle -1006` | 0 |
| E-P5-2 | `bash test/run.sh p5-table` | `ok`，输出 `real 7`、`inject 6/28/60/42/23`、`msg connect: 127.0.0.1:1 (code 7)`、`detail code=7 stage=connect host=127.0.0.1:1`、`repeat 1` | 0 |
| E-P5-2 | `bash test/run.sh p5-http-error` | `ok`，输出 `status 500 error 0 ok 0` | 0 |
| E-P5-3 | `bash test/run.sh p5-unknown` | `ok`，输出 `error 9999`、`detail code=9999 stage=transfer host=127.0.0.1:1` | 0 |
| E-P5-4 | `bash test/run.sh p5-predicates` | `ok`，输出 `ok 200 0 1 0`、`http404 404 0 0 0`、`transport 0 7 0 0`、`cancelled 200 0 0 1` | 0 |
| E-P5-5 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0`、`ok g1`、`ok g3 (... failure codes, error model, predicates)` | 0 / 0 / 0 |
| E-P5-5 | `grep -q 'is_cancelled' README.md` | 命中 | 0 |
| E-P5-5（负例） | `bash test/gates/g1_negative.sh` | 拒绝 15 份篡改卡（含 PH-5 三份） | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）——T3 的偏差见 §4。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `a088442` | `3e3ac8f` | [T1.md](../../reviews/PH-5/T1.md) |
| T2 | `df023cb` | `d4aa296` | [T2.md](../../reviews/PH-5/T2.md) |
| T3 | `f2c0129`（基线即绿） | 无实现提交 | [T3.md](../../reviews/PH-5/T3.md) |
| T4 | `e19e189` | `d4ebe1a` | [T4.md](../../reviews/PH-5/T4.md) |
| T5 | `6ee682c` | `T5-DOCS-SHA` | [T5.md](../../reviews/PH-5/T5.md) |

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`NO_HANDLE`、`error` / `error_msg` / `error_detail` / `cancelled`、五个读取函数、注入钩子 |
| `src/http/http.shim.c` | 入库；三类错误填充、阶段映射、detail 生成、NO_HANDLE 可观察、谓词互斥 |
| `src/http/http.m.ct` | 入库；`ResponseView` 增加 `error` / `error_msg` / `error_detail` / `is_ok` / `is_cancelled` |
| `test/fixtures/p5-local.c` | 六个本地码 |
| `test/fixtures/p5-table.c` | 真实 7 与注入 6/28/60/42/23、msg、detail、repeat |
| `test/fixtures/p5-http-error.c` | 500 不算传输错误 |
| `test/fixtures/p5-unknown.c` | 9999 原值保留 |
| `test/fixtures/p5-predicates.c` | 四态矩阵 |
| `test/fixtures/phase1_abi.c`、`p2-ops.c` | fake 响应 `memset`，新增指针字段安全 |
| `test/run.sh`、`tools/check.sh` | 五个夹具接线并入 `g3` |
| `README.md` | 错误面示例、真值表、S-05 裁定 |
| `docs/reviews/PH-5/T1.md` … `T5.md` | 入库 |

## 4. 偏差

1. **T3 没有红**：E-P5-3（未知码原值保留）在 T2 的「原值透传」实现里天然成立，
   `p5-unknown` 在 T2 基线上第一次运行就是绿的。没有先把实现改成「未知码归零」再
   修回来制造假红；T3 只提交补充断言，没有实现提交。卡 §4 的「红 → 绿」对 T3 不成立。
2. **注入钩子 `http_shim_inject_curl_code`**：6（DNS）、60（TLS）、42（中止）、23（写失败）
   无法在离线环境稳定复现，用钩子驱动同一套填充逻辑；7 走真实拒绝，28 在 PH-4
   `p4-timeout` 已有端到端证据。钩子只能证明映射表，不能证明 libcurl 对所有码的映射。
3. **`p5-table` 多三行断言**：判据只列了六个码，夹具额外打印 `msg` / `detail` / `repeat`，
   用来兑现「`error_msg` 含主机与阶段、`error_detail` 可复现」。判据文字未变。
4. **`error_msg` 只写 scheme://authority**：不带路径与 query，避免把调用数据写进日志；
   格式固定为 `"<阶段>: <主机> (code N)"`，不做本地化。
5. **取消路径不存在**：`is_cancelled()` 恒为 `0`（没有 `cancel()`）。取消态由夹具直接
   置响应字段证明谓词互斥；「取消后 `send()` 返回什么」要等 PH-8 实测，本 phase 不下结论。

## 5. 错误面最终形态

```text
import http::http;
#include <stdio.h>

int main(void)
{
    CResponse *res = "https://api.example.com/users"->post()->send();

    if (res->is_cancelled()) {
        printf("cancelled\n");            /* phase 8 才有这条路径 */
    } else if (res->error() != 0) {
        printf("transport %d: %s\n",      /* 正数 = CURLcode 原值 */
               res->error(), res->error_msg());
    } else if (res->is_ok()) {
        printf("%d %s\n", res->status(), res->body());
    } else {
        printf("http %d %s\n",            /* error==0，status 是 4xx/5xx */
               res->status(), res->body());
    }

    res->free();
    return 0;
}
```

- 本地拒绝：`-1001` ARG、`-1002` HEADER、`-1003` BODY、`-1004` SCHEME、`-1005` TIMEOUT、
  `-1006` NO_HANDLE（新增，不再静默成 `status 0`）。
- 传输错误：正的 `CURLcode` 原值，未知码不归零（实测 9999 → 9999）。
- HTTP 4xx/5xx：`error()==0`、`is_ok()==0`，靠 `status()` 区分。
- 没有 `HTTP_ERR_*` 枚举；常量是 `http.shim.h` 的宏，`int` 是落地形态。

### 谓词真值表（实测）

| 状态 | `status()` | `error()` | `is_ok()` | `is_cancelled()` |
| --- | --- | --- | --- | --- |
| 成功（200） | 200 | 0 | 1 | 0 |
| HTTP 错误（404） | 404 | 0 | 0 | 0 |
| 传输错误（拒绝连接） | 0 | 7 | 0 | 0 |
| 取消（同一份 200/0，置取消位） | 200 | 0 | 0 | 1 |

结论：`is_ok()` 的判定条件是 `2xx && error==0 && !cancelled`，第四行与第一行数据完全
相同、只有取消位不同，`is_ok` 就翻成 0——两个谓词**不可能同时为真**，是构造保证，
不是文档约定。这是改语义，不是改文档承认。

## 6. requirements 与 design

未改 `docs/requirements.md`、`docs/design.md`。R-11 要求的 `error_msg` / `error_detail` /
`is_ok` / `is_cancelled` 全部落地；R-14 的三类错误与未知码保留全部落地。S-05 的裁定
（不纳入枚举、用 int + 宏）是对 Q-05 的答复，不需要改条款。

## 7. 当前事实

最小请求 `ct build` 退出 0、运行退出 0，`status 200`、`error 0`、`is_ok 1`、
`is_cancelled 0`，body `{"users":3}`；真发请求由 PH-4 的服务端计数与
`grep -c 'curl_easy_perform' src/http/http.shim.c` 为 `1` 保证。
