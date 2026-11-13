# test

H1 起每个任务在这里放一份夹具，并由 `test/run.sh <task-id>` 驱动。
当前**没有夹具**：`test/run.sh` 对任何 task-id 都以退出码 2 结束，不伪装通过。

约定：

- 夹具一律 `test/fixtures/<task-id>.ct`，编译走 `ct-ast`，运行输出与
  `test/fixtures/<task-id>.expected.out` 逐字节比对；
- 每个行为任务先提交“红”夹具（编译或断言失败），实现提交再转绿；
- 网络相关夹具（H2 起）由 `test/fixture_server.c` 提供本地回环服务，不访问外网。
