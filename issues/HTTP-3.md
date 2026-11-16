# HTTP-3 E-P10-1 的 Linux 半边不可执行

- status: open
- 证据: `docs/phases/PH-10/closeout.md` §4.1；`uname -a`；`command -v podman` / `command -v docker` 均退出 1
- phase: PH-10
- fix: no

## 问题

E-P10-1 要求 macOS 与 Linux 的全量夹具都 0 失败。本机是 macOS arm64，没有 podman/docker，
也没有 Linux 版 `ct` 二进制（`test/run.sh` 默认指向 macOS 的 `ct`），因此 Linux 半边无法执行。

## 处置

不冒充通过：E-P10-1 整条记为「不计通过」，补跑命令与环境要求写在收口 §4.1。

2026-11-15 复核：

- 容器运行时仍是唯一缺口（`command -v podman` / `docker` 仍无命中）。
- Linux 版 `ct` **不是**第二条阻塞：ct-lang 只发 tag、不发 Linux 预编译二进制，
  但 `install.sh` 能从源码自举（`cc` + `tar`），容器内可用 `CT_SOURCE_DIR=/src` 离线构建。
- 收口 §4.1 已补四步可执行清单：容器内装包、自举 `ct` 并 `export CT_BIN=/usr/local/bin/ct`、
  跑 `bash tools/check.sh g3`、留 `uname -a` / `cc --version` / `ct -v` / `ok g3 (…)` / `g3 rc=0`。
- 本条按环境缺口挂账（`status: open`），不按工作量挂账。

## 触发条件

具备容器运行时与 Linux `ct` 后，按收口 §4.1 的命令在容器内跑 `bash tools/check.sh g3`，
`uname -s` 为 `Linux` 且退出 0 时关闭本条并重判 E-P10-1。

2026-11-16 复核（Podman，仍未关闭）：

- 运行时：`podman 6.2.0`（`/opt/homebrew/bin/podman`），`podman machine init` 成功，VM 为 libkrun，
  `uname` 返回 `Linux … aarch64`；VM 数据在 `/tmp/poddata`（7.8G，一次性验证物）。
- 起机不稳：`podman machine start` 常报 `krunkit exited unexpectedly with exit code 1`，同一 shell 内重试可起；
  启动会话结束后 VM 退出，跨命令 `podman run` 报 `dial tcp 127.0.0.1:63338: connect: connection refused`。
- 镜像源：`docker.io` → `connect: connection refused`；`mirror.gcr.io` → `i/o timeout`；
  `quay.io` / `ghcr.io` 可达（401），`registry.fedoraproject.org` 返回 200。
- 已用 `quay.io/fedora/fedora:42` 起容器；容器内 `dnf install` 停滞
  （`rchar` 8 秒无增长，18 分钟未装上 `gcc`），无法自举 Linux 版 `ct`，`g3` 未执行。

结论：缺口从“没有容器运行时”变为“容器内包源不可达”，`status` 仍为 `open`；
E-P10-1 继续记“不计通过”，不把没跑的写成通过。
