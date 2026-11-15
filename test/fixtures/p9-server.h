#ifndef CT_HTTP_P9_SERVER_H
#define CT_HTTP_P9_SERVER_H

/* Loopback fixture server for PH-9 redirects. One instance = one child. */
struct P9Server;

/* bind_addr: "127.0.0.1" or "127.0.0.2"; port is picked by the kernel. */
struct P9Server *p9_server_start(const char *bind_addr);
void p9_server_stop(struct P9Server *s);

/* "http://addr:port" and "http://addr:port<path>" (per-instance buffers). */
const char *p9_server_base(struct P9Server *s);
const char *p9_server_url(struct P9Server *s, const char *path);

/* Number of 302s in the /hop chain: /hop/0..<chain-1> redirect, /hop/<chain> is 200. */
void p9_server_set_chain(struct P9Server *s, int chain);

/* Base URL of the peer, used by /to-peer. Safe to call after both started. */
void p9_server_set_peer(struct P9Server *s, const char *peer_base);

int p9_server_requests(struct P9Server *s);
int p9_server_hop_requests(struct P9Server *s, int hop);
int p9_server_final_requests(struct P9Server *s);

#endif
