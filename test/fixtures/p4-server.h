#ifndef P4_SERVER_H
#define P4_SERVER_H

/*
 * Controllable loopback HTTP server for the phase 4 fixtures.
 *
 * p4_server_start() binds 127.0.0.1:0, forks a child that serves requests and
 * returns once the port is known. The child keeps a request counter in shared
 * memory, so the parent can prove a request really left the process instead of
 * trusting the value http_request_send() happened to return.
 *
 * Routes:
 *   /200     200, body {"users":3}
 *   /404     404, body "not found"
 *   /500     500, body "boom"
 *   /binary  200, 3 bytes a NUL b
 *   /split   200, {"users":3} written in two writes with a delay
 *   /hang    sleeps 5s before the 200, so a short timeout fires first
 *   other    200, body {"users":3}
 *
 * POSIX only, no external network access, one server per fixture process.
 */

int p4_server_start(void);
void p4_server_stop(void);

/* "http://127.0.0.1:<port><path>"; returns a static buffer. */
char *p4_server_url(const char *path);

/* Requests served so far, as seen by the server child. */
int p4_server_requests(void);

#endif
