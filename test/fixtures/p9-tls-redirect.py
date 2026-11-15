import http.server
import ssl
import sys

# Minimal HTTPS origin for p9-downgrade: every request gets a 302 to argv[3]
# (a plain http URL). argv[1] is the port (0 = kernel picks), argv[2] is the
# directory holding cert.pem and key.pem.


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(302)
        self.send_header("Location", sys.argv[3])
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, *args):
        pass


def main():
    srv = http.server.HTTPServer(("127.0.0.1", int(sys.argv[1])), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[2] + "/cert.pem", sys.argv[2] + "/key.pem")
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    print("PORT %d" % srv.server_port, flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
