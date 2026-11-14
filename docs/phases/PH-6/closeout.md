# PH-6 收口对照：三种超时区分与慢流保护

- 卡片：[docs/phases/PH-6/card.md](card.md)
- 判定：E-P6-1…E-P6-6 全部通过；偏差见 §4。
- 版本：`0.1.0`，MINOR 未移动；未 push、未打 tag。
- 冻结摘要（未变）：`criteria-sha256=0b8e87ac…b1812`、`tasks-sha256=849f1ef3…3b96`。

## 1. 退出判据

| ID | 命令 | 结果 | 退出码 |
| --- | --- | --- | --- |
| E-P6-1 | `bash test/run.sh p6-total` | `ok`，输出 `status 0 error 28 kind 2 phase total requests 1` | 0 |
| E-P6-2 | `bash test/run.sh p6-connect` | `ok`，输出 `error 28 kind 1 phase connect` | 0 |
| E-P6-3 | `bash test/run.sh p6-idle` | `ok`，输出 `error 28 kind 3 phase idle` | 0 |
| E-P6-4 | `bash test/run.sh p6-slow` | `ok`，输出 `status 200 body_len 5 kind 0 ok 1` | 0 |
| E-P6-5 | `bash test/run.sh p6-negative` | `ok`，输出 `idle -1005 timeout -1005 requests 0` | 0 |
| E-P6-6 | `bash tools/check.sh g0` / `g1` / `g3` | `ok g0`、`ok g1`、`ok g3 (… timeouts)` | 0 / 0 / 0 |
| E-P6-6 | `grep -q 'timeout_kind' README.md` | 命中 | 0 |
| E-P6-6（负例） | `bash test/gates/g1_negative.sh` | 拒绝 18 份篡改卡（含 PH-6 三份） | 0 |

固定条：任务清单每项可独立验收（6.6 T-6）。

## 2. 任务与提交对照

| 任务 | 红（`test`） | 实现（`feat` / `docs`） | 红证 |
| --- | --- | --- | --- |
| T1 | `b9e3b7f` | `4e2d611` | [T1.md](../../reviews/PH-6/T1.md) |
| T2 | `3749de4` | `5b9f315` | [T2.md](../../reviews/PH-6/T2.md) |
| T3 | `2f776cf` | `903fafb` | [T3.md](../../reviews/PH-6/T3.md) |
| T4 | `5377ef9` | `1efd26a` | [T4.md](../../reviews/PH-6/T4.md) |
| T5 | `42235bc` | `5b4b798` | [T5.md](../../reviews/PH-6/T5.md) |
| T6 | `a1180df` | 见 `docs: PH6-T6 超时面与收口` | [T6.md](../../reviews/PH-6/T6.md) |

每对相邻：`test` 提交的父提交即 `feat`/`docs` 的前一状态，红证 baseline 与之一致。

## 3. 交付物

| 路径 | 状态 |
| --- | --- |
| `src/http/http.shim.h` | 入库；`timeout_kind` 字段、`HTTP_TIMEOUT_*` 四宏、`http_request_idle_timeout`、`http_response_timeout_kind` |
| `src/http/http.shim.c` | 入库；连接/总时长分类、进度回调空档、idle 归一到 28、`error_detail` 追加 `phase=`、负值校验 |
| `src/http/http.m.ct` | 入库；`RequestBuilder.idle_timeout()`、`ResponseView.timeout_kind()` |
| `test/fixtures/p4-server.c` | 入库；`/stall`、`/slow` 端点 |
| `test/fixtures/p6-total.c` / `.expected.out` | 总超时 |
| `test/fixtures/p6-connect.c` / `.expected.out` | 连接超时 |
| `test/fixtures/p6-idle.c` / `.expected.out` | 空闲超时 |
| `test/fixtures/p6-slow.c` / `.expected.out` | 合法慢流不误杀 |
| `test/fixtures/p6-negative.c` / `.expected.out` | 负超时本地错误 |
| `test/run.sh`、`tools/check.sh` | 五个夹具接线并入 `g3` |
| `README.md` | 超时面调用示例、判定时点、精度、误判、慢流说明 |
| `docs/reviews/PH-6/T1.md` … `T6.md` | 入库 |

## 4. 偏差

1. **roadmap 的 `first_byte` 并入 total/idle**：roadmap 写的是
   `phase=connect|first_byte|total|idle`，落地没有单独的 `first_byte`。首字节前的时间
   已经被两个已有机制覆盖：总时长先到 → `total`；空档先到 → `idle`（空档从请求开始
   计时，首字节未到也算空档）。因此对调用方只有三种可区分的原因，不增加第四种状态。
2. **idle 的 `42` 归一到 `28`**：进度回调中止在 libcurl 侧返回 `CURLE_ABORTED_BY_CALLBACK`(42)，
   与 PH-5「中止」同码。PH-6 只在 shim 自己置 `idle_fired` 时把 `error` 归一成 28 并标
   `IDLE`；调用方主动中止（PH-7）没有 `idle_fired`，仍读 42。归一化只发生在这一条路径。
3. **慢流用例比例压缩**：真实场景是「1 字节 / 59 秒」，夹具用「1 字节 / 200 ms、共 5 字节」。
   被断言的性质相同（数据持续到达、任意间隔都小于 `idle_timeout`），压缩只为让门禁能频繁跑。
   负向对照是 `LOW_SPEED_LIMIT=1` + 窗口平均：它会把前者判死，所以实现里完全不使用
   `CURLOPT_LOW_SPEED_*`。
4. **连接超时依赖 TEST-NET-1**：`192.0.2.1:81` 是 RFC 5737 保留地址，本机路由下 SYN 被丢弃，
   connect 真实超时。若某环境直接回 `ENETUNREACH`(7)，`p6-connect` 会明确变红而不是静默通过；
   本条没有用注入钩子兜底。
5. **时间断言只到「秒级」**：判据不断言毫秒，因为进度回调粒度约 1 秒。实测
   `idle_timeout(500)` 在 ~1.0–1.1 秒中止，与回调周期一致。

## 5. 判定时点、精度与可能误判

- **时点**：`curl_easy_perform` 返回后一次性判定；传输中途不做分类。
  - `transport == 28` 且 `CURLINFO_CONNECT_TIME_T == 0` 且配置了 `connect_timeout` → CONNECT；
  - 其余 `transport == 28` → TOTAL；
  - `xfer.idle_fired` → IDLE，并把 `error` 写成 28。
- **精度**：连接 / 总时长由 libcurl 计时，亚毫秒；空闲由 `CURLOPT_XFERINFOFUNCTION`
  周期驱动，实测约 1 秒一次，因此 `idle_timeout(ms)` 的实际误差上限约为一个回调周期
  （`ms` 小于 1 秒时仍约 1 秒触发）。
- **误判**：
  - CONNECT 依赖 `CURLINFO_CONNECT_TIME_T == 0`。如果 libcurl 未填该信息（极端版本差异），
    连接超时会落到 TOTAL；不会反向把 TOTAL 误报成 CONNECT。
  - TOTAL 不区分「首字节前用尽」和「传输一半用尽」；两者对外语义都是总时长，符合预期。
  - IDLE 只在 shim 自己的回调置 `idle_fired` 时成立；调用方中止不会被误标为 IDLE。

## 6. 真实网络与注入的边界

**本 phase 的超时结论全部是真实网络验证，没有使用 `http_shim_inject_curl_code`。**

| 用例 | 触发方式 | 验证类型 |
| --- | --- | --- |
| `p6-total` | 本地回环 `/hang` 接受连接不响应 | 真实网络 |
| `p6-connect` | TEST-NET-1 `192.0.2.1:81`，SYN 无响应 | 真实网络 |
| `p6-idle` | 本地回环 `/stall` 发 1 字节后静默 | 真实网络 |
| `p6-slow` | 本地回环 `/slow` 每 200 ms 1 字节 | 真实网络 |
| `p6-negative` | 本地回环 `/200`，负值在本地拦截 | 真实网络（服务端计数 0） |

PH-5 的 6/60/42/23 仍只能用注入钩子驱动，那一组结论属于注入验证；PH-6 没有扩大注入面。

## 7. 执行时长

单机 macOS arm64，每例含一次 `ct build`：

| 用例 | 墙钟 |
| --- | --- |
| `p6-total` | 1.05 s |
| `p6-connect` | 1.00 s |
| `p6-idle` | 1.79 s |
| `p6-slow` | 1.52 s |
| `p6-negative` | 0.84 s |
| **PH-6 五例合计** | **6.20 s** |
| `g3` 全量（含全部历史夹具） | 26.57 s |

没有用例等待 5–10 秒：总超时 300 ms、连接超时 300 ms、空闲回调约 1 秒、慢流夹具
1 秒。整套 PH-6 可以在每次改动后直接跑。

## 8. 调用点形态

区分结果走 `timeout_kind()`，`error()` 不变（仍是 28）；没有新增本地码，因此不会与
`-1001…-1006` 冲突。

```text
import http::http;
#include <stdio.h>

int main(void)
{
    CRequest *req = "https://api.example.com/users"->post();
    req->timeout(3000);
    req->connect_timeout(1000);
    req->idle_timeout(1500);

    CResponse *res = req->send();

    if (res->error() == 28) {
        switch (res->timeout_kind()) {
        case 1:  printf("connect timeout\n"); break;
        case 2:  printf("total timeout\n");   break;
        case 3:  printf("idle timeout\n");    break;
        default: printf("timeout, kind unknown\n");
        }
    }

    res->free();
    req->free();
    return 0;
}
```

- 成功路径：`timeout_kind() == 0`（`HTTP_TIMEOUT_NONE`），与是否配置过超时无关。
- 非超时的传输错误：`error()` 为其它 `CURLcode`，`timeout_kind()` 同样为 0。
- `error_detail()` 在超时时含 `phase=connect|total|idle`，可用于日志，不需要额外查询。

## 9. requirements 与 design

未改 `docs/requirements.md`、`docs/design.md`。R-14 的「三类错误不混」与 F-02 的三种超时
区分在本 phase 落地，没有条款需要改；不存在「不改就落不了地」的冲突。

## 10. 当前事实

`timeout_kind()` 把同一个 `CURLE_OPERATION_TIMEDOUT`(28) 分成 CONNECT(1)/TOTAL(2)/IDLE(3)，
成功路径返回 0；1 字节/200 ms 的合法慢流在 `idle_timeout(500)` 下完整读完 5 字节
（`kind 0 ok 1`），不再被低速窗口误杀；PH-6 五例合计 6.20 s，`g3` 全量 26.57 s。
