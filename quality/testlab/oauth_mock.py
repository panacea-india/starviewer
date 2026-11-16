#!/usr/bin/env python3
"""A mock OAuth 2 provider and a bearer-token proxy, to test DICOMweb authentication.

Orthanc has no OAuth of its own, so this stands in front of it:

* POST /oauth/token  issues access tokens (client credentials grant, and refresh_token grant if
  --refresh is given). The client may authenticate with HTTP Basic or with client_id and
  client_secret in the body.
* Every other request is forwarded to Orthanc, with Orthanc's own test login added, but only if
  it carries "Authorization: Bearer <token>" for a token that was issued, has not expired and was
  not revoked. Otherwise the answer is 401 with a WWW-Authenticate header, as RFC 6750 says.
* POST /mock/revoke  invalidates every token now, to test how a client recovers from a 401.
* GET /mock/stats  returns how many tokens were issued and how many requests were accepted or
  refused, for assertions in tests.

Test use only: the secret is well known and the server listens on the loopback interface only.
"""

import argparse
import base64
import http.client
import json
import secrets
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CLIENT_ID = "starviewer-qa"
CLIENT_SECRET = "qa-only-secret"
SCOPE = "dicomweb"

state_lock = threading.Lock()
tokens = {}          # access token -> expiry time
refresh_tokens = {}  # refresh token -> True
stats = {"tokens_issued": 0, "token_requests_refused": 0, "requests_accepted": 0, "requests_refused": 0}
options = None

HOP_BY_HOP = {"connection", "keep-alive", "proxy-authenticate", "proxy-authorization", "te", "trailers",
              "transfer-encoding", "upgrade", "host", "authorization", "content-length"}


def issue_token(with_refresh):
    access = secrets.token_urlsafe(24)
    body = {"access_token": access, "token_type": "Bearer", "expires_in": options.ttl, "scope": SCOPE}
    with state_lock:
        tokens[access] = time.time() + options.ttl
        stats["tokens_issued"] += 1
        if with_refresh:
            refresh = secrets.token_urlsafe(24)
            refresh_tokens[refresh] = True
            body["refresh_token"] = refresh
    return body


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        if options.verbose:
            super().log_message(fmt, *args)

    def send_json(self, status, body, headers=None):
        data = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def handle_token(self):
        length = int(self.headers.get("Content-Length", "0"))
        form = urllib.parse.parse_qs(self.rfile.read(length).decode())
        client_id = form.get("client_id", [""])[0]
        client_secret = form.get("client_secret", [""])[0]
        authorization = self.headers.get("Authorization", "")
        if authorization.startswith("Basic "):
            try:
                client_id, _, client_secret = base64.b64decode(authorization[6:]).decode().partition(":")
                client_id = urllib.parse.unquote(client_id)
                client_secret = urllib.parse.unquote(client_secret)
            except Exception:
                pass

        if client_id != CLIENT_ID or client_secret != CLIENT_SECRET:
            with state_lock:
                stats["token_requests_refused"] += 1
            self.send_json(401, {"error": "invalid_client"}, {"WWW-Authenticate": 'Basic realm="oauth"'})
            return

        grant = form.get("grant_type", [""])[0]
        if grant == "client_credentials":
            self.send_json(200, issue_token(options.refresh))
        elif grant == "refresh_token" and options.refresh:
            token = form.get("refresh_token", [""])[0]
            with state_lock:
                known = refresh_tokens.pop(token, None)
            if known:
                self.send_json(200, issue_token(True))
            else:
                self.send_json(400, {"error": "invalid_grant"})
        else:
            self.send_json(400, {"error": "unsupported_grant_type"})

    def bearer_is_valid(self):
        authorization = self.headers.get("Authorization", "")
        if not authorization.startswith("Bearer "):
            return False
        token = authorization[7:].strip()
        with state_lock:
            expiry = tokens.get(token)
            return expiry is not None and expiry > time.time()

    def proxy(self):
        if not self.bearer_is_valid():
            with state_lock:
                stats["requests_refused"] += 1
            self.send_json(401, {"error": "invalid_token"}, {"WWW-Authenticate": 'Bearer realm="dicomweb", error="invalid_token"'})
            return
        with state_lock:
            stats["requests_accepted"] += 1

        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else None
        headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP_BY_HOP}
        orthanc = base64.b64encode(f"{options.orthanc_user}:{options.orthanc_password}".encode()).decode()
        headers["Authorization"] = "Basic " + orthanc

        upstream = http.client.HTTPConnection(options.orthanc_host, options.orthanc_port, timeout=120)
        try:
            upstream.request(self.command, self.path, body=body, headers=headers)
            response = upstream.getresponse()
            self.send_response(response.status)
            for key, value in response.getheaders():
                if key.lower() not in HOP_BY_HOP:
                    self.send_header(key, value)
            # Stream the answer without needing its length in advance
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            if self.command != "HEAD":
                while True:
                    chunk = response.read(65536)
                    if not chunk:
                        break
                    self.wfile.write(f"{len(chunk):X}\r\n".encode() + chunk + b"\r\n")
                self.wfile.write(b"0\r\n\r\n")
        except OSError as error:
            self.send_json(502, {"error": "bad_gateway", "detail": str(error)})
        finally:
            upstream.close()

    def route(self):
        path = urllib.parse.urlsplit(self.path).path
        if path == "/oauth/token" and self.command == "POST":
            self.handle_token()
        elif path == "/mock/revoke" and self.command == "POST":
            with state_lock:
                tokens.clear()
            self.send_json(200, {"revoked": True})
        elif path == "/mock/stats" and self.command == "GET":
            with state_lock:
                self.send_json(200, dict(stats))
        else:
            self.proxy()

    do_GET = do_POST = do_PUT = do_DELETE = do_HEAD = route


class QuietServer(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        # Clients that close a kept-alive connection are not errors
        pass


def main():
    global options
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=18044)
    parser.add_argument("--ttl", type=int, default=60, help="seconds a token stays valid (default 60)")
    parser.add_argument("--refresh", action="store_true", help="also issue refresh tokens")
    parser.add_argument("--orthanc-host", default="127.0.0.1")
    parser.add_argument("--orthanc-port", type=int, default=18042)
    parser.add_argument("--orthanc-user", default="qa")
    parser.add_argument("--orthanc-password", default="qa-only")
    parser.add_argument("--verbose", action="store_true")
    options = parser.parse_args()
    server = QuietServer(("127.0.0.1", options.port), Handler)
    print(f"OAuth mock on http://127.0.0.1:{options.port}/ (token endpoint /oauth/token, client {CLIENT_ID}, ttl {options.ttl}s)", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
