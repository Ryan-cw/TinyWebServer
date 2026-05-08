#!/usr/bin/env python3
"""TinyWebServer Python implementation for serving the existing root directory."""

from __future__ import annotations

import argparse
import functools
import http.server
import pathlib
import socketserver


class ReusableThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


class WelcomeHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path == "/":
            self.path = "/welcome.html"
        super().do_GET()

    def do_HEAD(self) -> None:
        if self.path == "/":
            self.path = "/welcome.html"
        super().do_HEAD()


def main() -> None:
    parser = argparse.ArgumentParser(description="Python TinyWebServer static server")
    parser.add_argument("--port", type=int, default=9006, help="HTTP listen port")
    parser.add_argument("--root", default="root", help="static file root")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    handler = functools.partial(WelcomeHandler, directory=str(root))

    with ReusableThreadingHTTPServer(("", args.port), handler) as server:
        print(f"Python TinyWebServer listening on http://127.0.0.1:{args.port}/ serving {root}")
        server.serve_forever()


if __name__ == "__main__":
    main()
