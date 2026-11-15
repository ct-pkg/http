# 03 配置面与默认值

- applies-to: 0.1.0

所有 setter 返回同一个 `CRequest *`，链式与变量落点两种写法等价。

| 方法 | 作用 | 默认值 |
| --- | --- | --- |
| `header(name, value)` | 请求头；名或值含 CR/LF 时本地拒绝，不进 slist | 无 |
| `timeout(ms)` | 总超时；负值本地拒绝 | 不设（`0`） |
| `connect_timeout(ms)` | 连接超时；负值本地拒绝 | 不设（`0`） |
| `idle_timeout(ms)` | 数据空档上限（首字节前也算空档）；负值本地拒绝 | 不设（`0`） |
| `max_body_bytes(n)` | 响应体上限；累计到 `n` 立即中止并报 `-1008` | 不限（`0`） |
| `body(data, len)` | 上传体；按 `len` 深拷贝，先 `POSTFIELDSIZE` 后 `COPYPOSTFIELDS` | 无 |
| `user_agent(ua)` | User-Agent | `ct-http/0.1.0` |
| `insecure(on)` | `1` 时同时关 `VERIFYPEER` 与 `VERIFYHOST` | `0`（校验开） |
| `follow_redirects(on)` | 是否跟随重定向；`1` 时最多跟随 10 次重定向（固定，不可配） | `0`（不跟随） |
| `cancel()` | 置粘性取消标志；只允许同线程调用 | 未置位 |

- 只接受 `http://` 与 `https://`（大小写不敏感）；其余 scheme 在工厂返回时就带本地
  负数码，easy handle 另设 `CURLOPT_PROTOCOLS_STR=http,https` 兜底。
- 头名或头值含 CR/LF 时本地拒绝（`-1002`），头计数不增。
- 请求头真的发到线上：跨源重定向时按头名剥离敏感头，见
  [08-redirects.md](08-redirects.md)。
- 本地校验码是 `HTTP_LOCAL_ERR_*`（`-1001`…`-1009`），是公开错误面的一部分。
