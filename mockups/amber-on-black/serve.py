#!/usr/bin/env python3
"""Serve the local theme mockup and the V1 reference captures."""

from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class PreviewHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(REPO_ROOT), **kwargs)

    def do_GET(self):
        if self.path in ("", "/", "/index.html"):
            self.send_response(302)
            self.send_header("Location", "/mockups/amber-on-black/")
            self.end_headers()
            return
        super().do_GET()


if __name__ == "__main__":
    server = ThreadingHTTPServer(("0.0.0.0", 8000), PreviewHandler)
    print("Amber on Black preview at http://0.0.0.0:8000/", flush=True)
    server.serve_forever()
