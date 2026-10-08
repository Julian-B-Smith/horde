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
starts loading something new, add it here deliberately.

`local/sluice` (the Sluice lab's in-place link to a private sibling's spec) is
NOT served by default; that lab shows its no-spec state. `--allow-sluice` is the
human's explicit, per-run opt-in (ratified 2026-10-08, B446 W3c): it serves
exactly that one subtree and follows exactly one symlink, `local/sluice` itself.
Under the flag the per-request rules become:
  - `local` must be a real directory (not a link);
  - the request must stay inside the link's resolved target, and nothing BELOW
    `local/sluice` may be a symlink (a link planted in the subtree is refused);
  - everything else under `local/`, and every other rule above, is unchanged.

Usage: python3 tools/serve_labs.py [--allow-sluice] [port] [root]
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


NAVIGATOR = "/docs/design/index.html"   # where the bare root redirects
SLUICE = "local/sluice"   # the one subtree --allow-sluice opens


def in_sluice(rel):
    return rel == SLUICE or rel.startswith(SLUICE + "/")


def allowed_relpath(url_path, allow_sluice=False):
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
    if allow_sluice and in_sluice(rel):
        return rel
    return None


def sluice_ok(root, rel):
    """The --allow-sluice rule for a request inside local/sluice (root is already resolved).

    `local` must be a real directory, and below the link nothing may be a symlink:
    the link's own target is resolved once, and the requested path must equal its
    own realpath when joined onto that target.
    """
    local = os.path.join(root, "local")
    if os.path.realpath(local) != local:
        return False
    target = os.path.realpath(os.path.join(root, *SLUICE.split("/")))
    rest = rel[len(SLUICE):].strip("/")
    wanted = os.path.join(target, *rest.split("/")) if rest else target
    return os.path.realpath(wanted) == wanted


class LabHandler(http.server.SimpleHTTPRequestHandler):
    allow_sluice = False   # per class, set by make_server: one flag per server, never per request

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _gate(self):
        """True when the request may proceed; otherwise the refusal is already sent."""
        port = self.server.server_address[1]
        if self.headers.get("Host") not in (f"localhost:{port}", f"127.0.0.1:{port}"):
            self.send_error(403, "Host not allowed")
            return False
        if self.path == "/":
            # The preview pane always opens the bare root; send it to the navigator
            # rather than a 404. A redirect to a fixed in-tree path serves nothing new.
            self.send_response(302)
            self.send_header("Location", NAVIGATOR)
            self.end_headers()
            return False
        rel = allowed_relpath(self.path, self.allow_sluice)
        if rel is None:
            self.send_error(404)
            return False
        root = os.path.realpath(self.directory)
        if in_sluice(rel):
            if not sluice_ok(root, rel):
                self.send_error(404)
                return False
            return True
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


def make_server(root, port, allow_sluice=False):
    # A subclass per server, so one server's opt-in can never leak into another's
    # (the check runs a default and an opted-in server in one process).
    cls = type("LabHandlerSluice" if allow_sluice else "LabHandlerDefault", (LabHandler,),
               {"allow_sluice": bool(allow_sluice)})
    handler = lambda *a, **k: cls(*a, directory=str(root), **k)  # noqa: E731
    return http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)


def main():
    argv = sys.argv[1:]
    allow_sluice = "--allow-sluice" in argv
    argv = [a for a in argv if a != "--allow-sluice"]
    port = int(argv[0]) if len(argv) > 0 else 8146
    root = pathlib.Path(argv[1] if len(argv) > 1
                        else pathlib.Path(__file__).parent.parent).resolve()
    with make_server(root, port, allow_sluice) as srv:
        print(f"serve_labs: {root.name} on http://localhost:{port}/docs/design/index.html (no-store)")
        if allow_sluice:
            print(f"serve_labs: --allow-sluice: also serving {SLUICE}/ (one subtree, the one link, nothing else of local/)")
        srv.serve_forever()


if __name__ == "__main__":
    main()
