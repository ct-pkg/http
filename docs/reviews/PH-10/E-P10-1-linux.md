# E-P10-1 Linux 半边复跑记录（2026-11-17）

本文件是 E-P10-1 在 Linux 容器内的可复跑配方与原始结论。macOS 半边的结果见收口 §4.1。

- 结论：**通过**。容器内 `bash tools/check.sh g3` 退出 0，55 项全 `ok`；macOS 半边同日复跑亦 `rc=0`。
- 主机：macOS arm64；`podman 6.2.0`（libkrun VM），VM 内 `Linux 7.2.11-200.fc44.aarch64 … aarch64`。
- 容器：`docker.io/library/gcc:13`（Debian 系，`cc` gcc 13，自带 `libcurl` 开发头与 ASAN 运行库）。
- 编译器版本行：`cc (GCC) 13.5.0`（`gcc:13` 镜像，`-lcurl` 可用）。
- Linux `ct`：从 ct-lang 源码自举，`ct v0.61.1`。

## 1. 镜像选择（为什么不是 ct-ci）

本机预置的 `localhost/ct-ci:24.04`、`localhost/ct-ci:22.04`、`localhost/ubuntu-gcc:22.04` **都没有
`curl/curl.h`**，`cc -lcurl` 会报 `fatal error: curl/curl.h: No such file or directory`。
`docker.io/library/gcc:13` 的头文件在 `/usr/include/aarch64-linux-gnu/curl/curl.h`，可直接编译。

```console
$ podman run --rm --platform linux/arm64 docker.io/library/gcc:13 \
    bash -lc 'printf "#include <curl/curl.h>\nint main(void){curl_global_init(1);return 0;}\n" >/tmp/t.c; cc /tmp/t.c -lcurl -o /tmp/t && echo curl-build-ok'
curl-build-ok
```

## 2. 配方（可照抄）

宿主机当前目录为本仓根，ct-lang 工作树在 `/Users/samxx/trae-ws/ct-lang`（**只读挂载**，不写不改）：

```console
$ podman machine start                       # 若 VM 未起；启动后先确认
$ podman info --format '{{.Host.OS}}'        # 期望输出 linux

$ podman run --rm --platform linux/arm64 \
    -v /Users/samxx/trae-ws/ct-http:/w \
    -v /Users/samxx/trae-ws/ct-lang:/src:ro \
    -w /w docker.io/library/gcc:13 \
    bash -lc '
      set -e
      CT_SOURCE_DIR=/src sh /src/install.sh -p /usr/local/bin
      export CT_BIN=/usr/local/bin/ct
      "$CT_BIN" -v
      bash tools/check.sh g3
      echo "g3 rc=$?"
    '
```

要点：

- `CT_SOURCE_DIR=/src` 是 `install.sh` 的离线自举入口；`/src` 必须只读挂载。
- `CT_BIN` 指向容器内的自举产物；不 export 会用镜像里不存在的 `ct`。
- 镜像与源码都不落在宿主工作树里；运行期产物只在容器内 `/tmp`。
- 单次全量约 2–3 分钟（自举约 40 秒，其余为夹具）。

## 3. 原始结论

```console
$ /usr/local/bin/ct -v
ct v0.61.1

$ bash tools/check.sh g3
ok phase1_abi
ok phase1_chain
…（共 55 行 ok）
ok g3 (abi, chain, variable landing, negative import, shim lifecycle, release,
       response ownership, ops, request config, response bytes, take_body,
       failure codes, error model, predicates, timeouts, streaming,
       cancellation, lifetime, redirects, credentials, downgrade)
g3 rc=0
```

ASAN 相关用例（`p8-lifetime`、`p8-lifetime-bad`）在容器内同样通过，说明 `gcc:13` 的
sanitizer 运行库可用；本仓未额外设置 `ASAN_OPTIONS`。

## 4. Linux 暴露出的夹具问题（已修）

`ct build` 用 `-std=c99` 调 `cc`；glibc 在严格 ISO 模式下不声明 `MAP_ANONYMOUS` 与 `kill(2)`，
`-Werror=implicit-function-declaration` 直接把夹具判红。三个夹具在文件首部补了特性测试宏：

- `test/fixtures/p4-server.c`
- `test/fixtures/p9-server.c`
- `test/fixtures/p9-downgrade.c`

```c
#ifndef __APPLE__
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
```

macOS 半边不受影响：`__APPLE__` 分支跳过这些宏，保留 `MAP_ANON` 与 `INADDR_LOOPBACK`；
修改后 macOS 的 `g3` 复跑仍为 `rc=0`（2026-11-17 复核）。

## 5. 给 ct-fsguard R-17（LSan）复用的注意事项

- 同一镜像即可：`docker.io/library/gcc:13` 自带 `libasan`/`liblsan`；
  跑泄漏检测时用 `ASAN_OPTIONS=detect_leaks=1`（macOS 上需 `symbolize=0`，Linux 不需要）。
- 挂载方式保持只读：`-v <repo>:/w -v <ct-lang>:/src:ro`；不要在容器内 `git` 任何写操作。
- 若宿主机是 Apple Silicon，务必带 `--platform linux/arm64`，否则会因架构不匹配走模拟而极慢。
- VM 行为：本机 `podman machine start` 在沙箱内执行会因 `~/.config/containers` 不可写而失败
  （`creating lockfile for VM: … permission denied`），需在无沙箱环境执行；
  VM 起来后可跨命令复用，`podman info` 应返回 `linux`。
- 包源现状（2026-11-17）：`docker.io` 可用（`gcc:13` 已在本机），
  `mirror.gcr.io` 超时，`quay.io` / `ghcr.io` 返回 401（可达），
  `registry.fedoraproject.org` 200。Fedora 镜像内 `dnf` 下载曾长时间停滞，不建议作为首选。
