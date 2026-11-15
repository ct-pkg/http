# HTTP-2 复核：包根 `http/` 与 `ct install` 的 `-I` 对齐

- 日期：2026-11-15
- 复核人：主控（用户授权）
- 结论：成立。包根改为 `http/` 后，`ct install` 打印的 `-I vendor/http` 与真实包含根一致，
  消费端用同一串参数即可构建样例，回环请求输出 `200`。

## 1. 目录迁移清单

```console
$ git diff --cached --name-status -M
R100	src/README.md	http/README.md
R100	src/http/Types.h.ct	http/Types.h.ct
R100	src/http/http.m.ct	http/http.m.ct
R100	src/http/http.shim.c	http/http.shim.c
R100	src/http/http.shim.h	http/http.shim.h
```

`src/` 目录已删除（`git mv src/http http`、`git mv src/README.md http/README.md`、`rmdir src`）。
迁移后 `http/README.md` 重写，故状态是 `AM`（重命名 + 修改）。

## 2. 真实 `ct install` 打印（干净工程，本地目录 spec）

```console
$ ct get /Users/samxx/trae-ws/ct-http
vendored 1 package(s)
$ ct install
ct build -I vendor/http --ldflags '-lcurl' <unit>.ct -o build
$ ls vendor/http
AGENTS.md  CHANGELOG.md  ct.pkg  docs  http  issues  LICENSE  README.md  test  tools
```

打印的前缀是 `vendor/http`，vendored 树里 `http/http.m.ct` 确实存在，两者对齐。

## 3. 安装样例

```console
$ bash test/gates/p10_install.sh
install ok
200
$ echo $?
0
```

样例用打印出来的 `-I vendor/http` 原样构建；脚本断言安装行含 `-I vendor/http `、
不含 `-I vendor/http/src`。

## 4. g3（含 ABI、shim 生命周期、ASAN 负例）

```console
$ bash tools/check.sh g3
…
ok p9-downgrade
ok p9-default
ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release,
response ownership, ops, request config, response bytes, take_body, failure codes,
error model, predicates, timeouts, streaming, cancellation, lifetime, redirects,
credentials, downgrade)
$ echo $?
0
```

## 5. g4（文档上限、片段编译、块绑定、锚点、版本）

```console
$ bash tools/check.sh g4
note g4-version: PATCH accounting deferred by deviation V-001 (0.1.0 number line)
ok g4 (120 docs <= 300 lines, 8 snippets compiled, 13 blocks bound,
anchors/applies/readme/version ok)
$ echo $?
0
```

## 6. 迁移前的问题（对照）

迁移前消费端只能写 `ct build -I vendor/http/src …`，否则 `ctc: cannot resolve import 'http/http'`。
根因是 `ct` 为每个 vendored 包只打印一条 `-I vendor/<name>`
（`ct-lang/selfhost/ct-ast/tool/pkg/Pkg.h.ct:1388-1389`），不感知包内单元子目录。
上游约定已立提案 `docs/proposals/ct-install-include-prefix.md`，本仓未改 ct-lang。
