# HTTP-1 复核：请求头是否真的上线

- 基线：`d0160db^`（`d0160db` 是 PH-9 实现提交，其父提交为修复前状态）
- 夹具：`test/fixtures/p9-cross-host.c` + `test/fixtures/p9-server.c`（同源行是对照）
- 结论：复核成立。修复前同源行 `same auth 0 cookie 0 proxy 0`，修复后 `same auth 1 cookie 1 proxy 1`。

## 说明：基线不含该夹具

`p9-cross-host.c` 是 `d0160db` 新增的文件，`d0160db^` 上没有它。复核采用 PH-9 T2 记录的
消融法：**用 `d0160db^` 的 shim** 配**当前夹具**，只把实现换回基线，夹具与断言不变。
这不是用当前代码复跑，基线 shim 来自 `git archive d0160db^`。

## 红：基线 shim（修复前）

```console
$ git archive d0160db^ | tar -x -C /tmp/http1-base
$ cc -std=c99 -Wall -Wextra -Werror -I /tmp/http1-base/src/http -I test/fixtures \
      test/fixtures/p9-cross-host.c test/fixtures/p9-server.c \
      /tmp/http1-base/src/http/http.shim.c -lcurl -o /tmp/http1-base/p9-cross-host
$ /tmp/http1-base/p9-cross-host
cross auth 0 cookie 0 proxy 0 status 200
cross referer 0
port auth 0 cookie 0 proxy 0 status 200
same auth 0 cookie 0 proxy 0 status 200
same referer 0
退出码: 0
```

三条 `cross` 断言是空过（头根本没发），真正红的是同源两行：`auth/cookie/proxy = 0`、
`referer = 0`。

## 绿：HEAD（修复后）

布局迁移（HTTP-2）后包含根是仓库根的 `http/`；下面这条命令在迁移后重跑，输出与迁移前一致。

```console
$ cc -std=c99 -Wall -Wextra -Werror -I http -I test/fixtures \
      test/fixtures/p9-cross-host.c test/fixtures/p9-server.c \
      http/http.shim.c -lcurl -o /tmp/http1-head
$ /tmp/http1-head
cross auth 0 cookie 0 proxy 0 status 200
cross referer 0
port auth 0 cookie 0 proxy 0 status 200
same auth 1 cookie 1 proxy 1 status 200
same referer 1
退出码: 0

$ bash test/run.sh p9-cross-host
ok p9-cross-host
退出码: 0
```

## 复核判定

| 断言 | 基线 | HEAD | 期望 |
| --- | --- | --- | --- |
| 同源保留（host 轴对照） | `same auth 0 cookie 0 proxy 0` | `same auth 1 cookie 1 proxy 1` | HEAD 为 1 |
| 同源保留（Referer） | `same referer 0` | `same referer 1` | HEAD 为 1 |
| 跨源剥离 | 全 0（空过） | 全 0 | 全 0 |
| 请求实际发出 | 否 | 是（同源行变 1 证明） | 是 |

- 复核人：主控（用户授权）
- 日期：2026-11-15
- 结论：HTTP-1 关闭，状态转 `verified`；不改冻结判据。
