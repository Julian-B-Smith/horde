#!/usr/bin/env python3
"""serve_labs — serve the repo root for the design labs, with caching OFF.

WHY NOT `python3 -m http.server`. It sends Last-Modified and no Cache-Control,
so browsers cache heuristically — for days when the file was old — and a lab
REBUILT UNDER THE SAME FILENAME keeps showing its previous version. That is
exactly what happened on 2026-09-24: B208 rebuilt the modulator lab inside the
human's original `shape-lab-mod.html`, the navigator card pointed at the right
file, and the browser served the August copy under the new card. `no-store`
makes every load a real read of the file on disk.

Usage: python3 tools/serve_labs.py [port]   (default 8146; serves this repo)
"""
import functools
import http.server
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8146
    handler = functools.partial(NoCache, directory=str(ROOT))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as srv:
        print(f"serve_labs: {ROOT.name} on http://localhost:{port}/docs/design/index.html (no-store)")
        srv.serve_forever()


if __name__ == "__main__":
    main()
