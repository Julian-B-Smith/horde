#!/usr/bin/env python3
"""serve_labs — serve the design labs from a repo tree, with caching OFF.

WHY NOT `python3 -m http.server`. It sends Last-Modified and no Cache-Control,
so browsers cache heuristically — for days when the file was old — and a lab
REBUILT UNDER THE SAME FILENAME keeps showing its previous version. That is
exactly what happened on 2026-09-24: B208 rebuilt the modulator lab inside the
human's original `shape-lab-mod.html`, the navigator card pointed at the right
file, and the browser served the August copy under the new card. `no-store`
makes every load a real read of the file on disk.

WHAT IT SERVES (ADR-194 / B446 W3b, W2-05, L3-M7). Only the lab trees and the
few files the lab pages actually fetch; everything else is 404. The earlier
version served the whole checkout, including gitignored private files and
`.git/`, to anything that could reach the port. Rules, all enforced per request:
  - Host header must be `localhost:<port>` or `127.0.0.1:<port>` (403 otherwise):
    a page on another domain that re-resolves to 127.0.0.1 (DNS rebinding) sends
    its own hostname and is refused.
  - the path must be under ALLOWED_DIRS or be one of ALLOWED_FILES;
  - no `..` and no dot-file component, no symlink anywhere below the served
    root on the way to the file (a link out of an allowed tree is never followed);
  - directories are never listed (an `index.html` is served if present).
Binds 127.0.0.1 only.

ALLOWED_FILES was found by reading what the lab pages load (grep of relative
`fetch(`/`import(`/`src=` in docs/design and reference), not guessed. When a lab
starts loading something new, add it here deliberately. `local/sluice` (the
Sluice lab's in-place spec link) is deliberately NOT served; that lab shows its
no-spec state.

Usage: python3 tools/serve_labs.py [port] [root]
  port  default 8146
  root  the tree to serve; default is the repo this script lives in. The root is
        DATA: labs_preview.sh runs the MAIN checkout's copy of this script against
        its scratch merge, so a lab branch never chooses the code that runs.
"""
import http.server
import os
import pathlib
import sys
import urllib.parse

ALLOWED_DIRS = ("docs/design", "reference")
ALLOWED_FILES = frozenset({
    "docs/H2-PLAN.md", "ROADMAP.md", "DECISIONS.md",            # h2-plan-map.html
    "src/gui/gui.html", "src/gui/gui2.html",                    # the navigator's two GUI links
    "tools/patchspace/metrics.mjs", "tools/patchspace/calibrate.mjs",
    "tools/patchspace/dependency_tree.json",                    # edge-correction-lab, listening-pass
})


def allowed_relpath(url_path):
    """The repo-relative POSIX path a URL may read, or None. Pure: no filesystem."""
    # Split query/fragment by hand: urlsplit would read "//x/y" as host "x", which
    # is not how SimpleHTTPRequestHandler reads the same line.
    path = urllib.parse.unquote(url_path.split("?", 1)[0].split("#", 1)[0])
    if "\0" in path or "\\" in path or not path.startswith("/"):
        return None
    parts = path.split("/")[1:]
    if parts and parts[-1] == "":
        parts.pop()                                   # trailing slash: a directory request
    # "." and ".." are rejected rather than normalised; dot-files (.git, .env,
    # .DS_Store) are never part of a lab.
    if not parts or any(p == "" or p.startswith(".") for p in parts):
        return None
    rel = "/".join(parts)
    if rel in ALLOWED_FILES or any(rel == d or rel.startswith(d + "/") for d in ALLOWED_DIRS):
        return rel
    return None


class LabHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _gate(self):
        """True when the request may proceed; otherwise the refusal is already sent."""
        port = self.server.server_address[1]
        if self.headers.get("Host") not in (f"localhost:{port}", f"127.0.0.1:{port}"):
            self.send_error(403, "Host not allowed")
            return False
        rel = allowed_relpath(self.path)
        if rel is None:
            self.send_error(404)
            return False
        root = os.path.realpath(self.directory)
        # realpath resolves every symlink; the result equals the literal path only
        # when there was none (macOS /tmp -> /private/tmp is in `root`, which is
        # resolved first, so it is not mistaken for one).
        if os.path.realpath(os.path.join(root, *rel.split("/"))) != os.path.join(root, *rel.split("/")):
            self.send_error(404)
            return False
        return True

    def do_GET(self):
        if self._gate():
            super().do_GET()

    def do_HEAD(self):
        if self._gate():
            super().do_HEAD()

    def list_directory(self, path):
        self.send_error(404)
        return None


def make_server(root, port):
    handler = lambda *a, **k: LabHandler(*a, directory=str(root), **k)  # noqa: E731
    return http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8146
    root = pathlib.Path(sys.argv[2] if len(sys.argv) > 2
                        else pathlib.Path(__file__).parent.parent).resolve()
    with make_server(root, port) as srv:
        print(f"serve_labs: {root.name} on http://localhost:{port}/docs/design/index.html (no-store)")
        srv.serve_forever()


if __name__ == "__main__":
    main()
