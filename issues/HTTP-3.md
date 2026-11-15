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
