#!/usr/bin/env python3
"""serve_labs_check -- the labs server serves the lab trees and nothing else (B446 W3b, ADR-194).

WIRED: ./verify fast

WHY. tools/serve_labs.py used to serve the whole checkout to anything that could
reach the port, with no Host check. It is now an allowlist. A guard nobody
exercises rots, so this starts the REAL handler in-process on an ephemeral port
on every `./verify fast` (about a second) and asserts the responses.

B446 W3c ADDS the Sluice opt-in (`--allow-sluice`, off by default, serves exactly the
`local/sluice` subtree and follows exactly its one link). Rows below prove, on the
same fixture and with the default server beside it as the control: without the flag
`local/sluice` is 404; with it the subtree answers 200 while `local/` elsewhere,
PRIVATE-NOTES.md and .git stay 404, a symlink planted INSIDE the subtree and a
symlinked `local/` are still refused, and the flag really arrives through the CLI.

FOUR LAYERS.
  1. A fixture tree holding the things that must never be served (a git dir, a
     local/ dir, a private notes file, symlinks that lead out of the allowed
     trees) beside the things that must be: each must-refuse row answers 404 (or
     403 for a foreign Host), each must-serve row answers 200.
  2. MUST-FAIL CONTROL: the same probe against the stock http.server handler,
     which is what the repo used to serve with. It has to read as leaking on every
     row. If the stock handler passes, the fixture has stopped modelling the
     problem and a green on layer 1 means nothing.
  3. The REAL tree: every tracked lab page, and every file those pages
     reference by relative path, answers 200 (except the one deliberate refusal,
     local/sluice, which is opt-in). A lab that starts loading something new fails
     here, loudly, instead of silently going blank in the browser.
  4. The Sluice opt-in (above).
"""
import http.client
import http.server
import functools
import os
import re
import subprocess
import sys
import tempfile
import threading
import posixpath

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import serve_labs  # noqa: E402

fails = []


class QuietStock(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


serve_labs.LabHandler.log_message = lambda *a: None   # a green run should be one line


def check(ok, what):
    if not ok:
        fails.append(what)
        print(f"FAIL  {what}")


def request(port, path, host="auto", method="GET"):
    """One raw request. `host`: 'auto' = a legitimate Host; None = send none; else literal."""
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    # putrequest with skip_host so the Host header is exactly what the row says,
    # and the path goes out unnormalised (http.client does not touch it).
    c.putrequest(method, path, skip_host=True, skip_accept_encoding=True)
    if host == "auto":
        host = f"localhost:{port}"
    if host is not None:
        c.putheader("Host", host)
    c.endheaders()
    r = c.getresponse()
    body = r.read()
    c.close()
    return r.status, body


class Served:
    def __init__(self, server):
        self.server, self.port = server, server.server_address[1]
        self.t = threading.Thread(target=server.serve_forever, daemon=True)

    def __enter__(self):
        self.t.start()
        return self

    def __exit__(self, *a):
        self.server.shutdown()
        self.server.server_close()


def build_fixture(base):
    outside = os.path.join(base, "outside")
    root = os.path.join(base, "tree")
    for d in ("outside", "tree/docs/design", "tree/reference/sub", "tree/.git", "tree/local/security",
              "tree/local/sluice-real", "tree/tools", "tree/src/gui"):
        os.makedirs(os.path.join(base, d))
    w = lambda p, s: open(os.path.join(base, p), "w").write(s)  # noqa: E731
    w("outside/secret.txt", "SECRET")
    w("tree/docs/design/index.html", "<title>nav</title>")
    w("tree/docs/design/lab.html", "<title>lab</title>")
    w("tree/docs/design/.hidden", "dotfile")
    w("tree/reference/swarmsaw.html", "<title>ref</title>")
    w("tree/reference/sub/deep.js", "1")
    w("tree/.git/HEAD", "ref: refs/heads/main")
    w("tree/local/security/report.md", "SECRET")
    w("tree/PRIVATE-NOTES.md", "SECRET")
    w("tree/ROADMAP.md", "roadmap")
    w("tree/tools/serve_labs.py", "# not a lab file")
    w("tree/src/gui/gui2.html", "gui")
    # Symlinks that lead OUT of the allowed trees, a file link and a directory link,
    # plus the repo's real shape: local/sluice -> a private sibling's tree.
    os.symlink(os.path.join(outside, "secret.txt"), os.path.join(root, "docs/design/link.txt"))
    os.symlink(outside, os.path.join(root, "reference/linkdir"))
    os.symlink(os.path.join(base, "tree/local/sluice-real"), os.path.join(root, "local/sluice"))
    w("tree/local/sluice-real/spec.md", "SLUICE-SPEC")
    w("tree/local/sluice-real/.hidden", "dotfile")
    # Links planted INSIDE the Sluice subtree that lead out of it (a file and a directory),
    # and a hardlink-free sibling file under local/ that the opt-in must not open.
    os.symlink(os.path.join(outside, "secret.txt"), os.path.join(base, "tree/local/sluice-real/escape.txt"))
    os.symlink(outside, os.path.join(base, "tree/local/sluice-real/escdir"))
    # A second tree whose `local` ITSELF is a link: the opt-in must not follow that one.
    os.makedirs(os.path.join(base, "outside2/sluice"))
    w("outside2/sluice/spec.md", "SECRET")
    os.makedirs(os.path.join(base, "tree2/docs/design"))
    w("tree2/docs/design/index.html", "<title>nav</title>")
    os.symlink(os.path.join(base, "outside2"), os.path.join(base, "tree2/local"))
    return root


# (path, host, expected status on the HARDENED server). host 'auto' is legitimate.
ROWS = [
    ("/docs/design/index.html", "auto", 200),          # must-serve controls come first:
    ("/docs/design/lab.html?x=1", "auto", 200),        # a refusal row is only meaningful
    ("/docs/design/", "auto", 200),                    # next to a row that is served
    ("/reference/swarmsaw.html", "auto", 200),
    ("/reference/sub/deep.js", "auto", 200),
    ("/ROADMAP.md", "auto", 200),
    ("/src/gui/gui2.html", "auto", 200),
    ("/.git/HEAD", "auto", 404),
    ("/.git/config", "auto", 404),
    ("/local/security/report.md", "auto", 404),
    ("/local/security/", "auto", 404),
    ("/local/sluice/spec.md", "auto", 404),            # the symlink into a private sibling
    ("/PRIVATE-NOTES.md", "auto", 404),
    ("/tools/serve_labs.py", "auto", 404),
    ("/docs/design/link.txt", "auto", 404),            # file symlink out of an allowed root
    ("/reference/linkdir/secret.txt", "auto", 404),    # directory symlink out of an allowed root
    ("/docs/design/../../PRIVATE-NOTES.md", "auto", 404),
    ("/docs/design/%2e%2e/%2e%2e/PRIVATE-NOTES.md", "auto", 404),
    ("/docs/design/..%2f..%2fPRIVATE-NOTES.md", "auto", 404),
    ("/docs//design/index.html", "auto", 404),
    ("/docs/design/.hidden", "auto", 404),
    ("/reference/", "auto", 404),                      # no index.html: never a listing
    ("/", "auto", 302),                                # the bare root redirects to the navigator
    ("/", "evil.example:{port}", 403),                 # ...but only for a legitimate Host
    ("/docs/design/index.html", "evil.example:{port}", 403),   # DNS rebinding
    ("/docs/design/index.html", "localhost", 403),             # no port
    ("/docs/design/index.html", "localhost:1", 403),           # wrong port
    ("/docs/design/index.html", "127.0.0.1.evil.example:{port}", 403),
    ("/docs/design/index.html", None, 403),                    # no Host at all
    ("/docs/design/index.html", "127.0.0.1:{port}", 200),
]


# The same fixture served with --allow-sluice. Only the local/sluice subtree changes
# answer; every other row of ROWS must read exactly as before.
SLUICE_ROWS = [
    ("/local/sluice/spec.md", "auto", 200),               # the opt-in: the one link, followed
    ("/local/sluice/spec.md?x=1", "auto", 200),
    ("/local/sluice/spec.md", "evil.example:{port}", 403),    # the Host rule still applies
    ("/local/sluice/spec.md", None, 403),
    ("/local/sluice/", "auto", 404),                      # never a listing
    ("/local/sluice/.hidden", "auto", 404),               # dot-files, as everywhere
    ("/local/sluice/missing.md", "auto", 404),
    ("/local/sluice/escape.txt", "auto", 404),            # a file link planted inside the subtree
    ("/local/sluice/escdir/secret.txt", "auto", 404),     # a directory link planted inside it
    ("/local/sluice/../security/report.md", "auto", 404),
    ("/local/sluice/%2e%2e/security/report.md", "auto", 404),
    ("/local/sluice/..%2fsecurity%2freport.md", "auto", 404),
    ("/local/sluice-real/spec.md", "auto", 404),          # the link's target by its own name: not the subtree
    ("/local/security/report.md", "auto", 404),           # the rest of local/
    ("/local/", "auto", 404),
    ("/local", "auto", 404),
    ("/local/sluicex/spec.md", "auto", 404),              # a prefix match is not a subtree match
    ("/PRIVATE-NOTES.md", "auto", 404),
    ("/.git/HEAD", "auto", 404),
    ("/tools/serve_labs.py", "auto", 404),
    ("/docs/design/link.txt", "auto", 404),               # the other trees' symlink rule is untouched
    ("/reference/linkdir/secret.txt", "auto", 404),
    ("/docs/design/index.html", "auto", 200),
]
# `tree2/local` is a symlink: even with the flag, nothing under it is served.
SLUICE_ROWS_LINKED_LOCAL = [
    ("/local/sluice/spec.md", "auto", 404),
    ("/docs/design/index.html", "auto", 200),
]


def probe(port, rows=ROWS):
    out = []
    for path, host, _ in rows:
        h = host.format(port=port) if isinstance(host, str) and host != "auto" else host
        out.append(request(port, path, h)[0])
    return out


def free_port():
    import socket
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def cli_status(args, path, wait=10.0):
    """Start tools/serve_labs.py itself with `args` (a port is appended in place of {port}), GET `path`, stop it."""
    import time
    port = free_port()
    cmd = [sys.executable, os.path.join(ROOT, "tools/serve_labs.py")] + [a.format(port=port) for a in args]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        end = time.time() + wait
        while time.time() < end:
            try:
                return request(port, path)[0]
            except OSError:
                if p.poll() is not None:
                    return f"server exited {p.returncode}"
                time.sleep(0.05)
        return "no answer"
    finally:
        p.kill()
        p.communicate()


def main():
    with tempfile.TemporaryDirectory() as base:
        root = build_fixture(base)
        root2 = os.path.join(base, "tree2")

        # Layer 1: the hardened handler.
        srv = serve_labs.make_server(root, 0)
        check(srv.server_address[0] == "127.0.0.1", f"binds 127.0.0.1 only (got {srv.server_address[0]})")
        with Served(srv) as s:
            got = probe(s.port)
            for (path, host, want), g in zip(ROWS, got):
                check(g == want, f"{path} Host={host}: want {want}, got {g}")

        # Layer 2: the same rows against the stock handler must read as leaking.
        # Rows expected 404/403 on the hardened server and 200 on the stock one are
        # the leaks; the evidence is that a representative set of them DO answer 200.
        stock = http.server.ThreadingHTTPServer(
            ("127.0.0.1", 0), functools.partial(QuietStock, directory=root))
        with Served(stock) as s:
            got = dict(zip([(p, h) for p, h, _ in ROWS], probe(s.port)))
            for path, host in [("/.git/HEAD", "auto"), ("/local/security/report.md", "auto"),
                               ("/PRIVATE-NOTES.md", "auto"), ("/docs/design/link.txt", "auto"),
                               ("/local/sluice/spec.md", "auto"),
                               ("/docs/design/index.html", "evil.example:{port}")]:
                check(got[(path, host)] == 200,
                      f"CONTROL: stock handler should leak {path} Host={host} (got {got[(path, host)]}); "
                      "the fixture no longer models the problem")

        # Layer 4: the Sluice opt-in, on the SAME fixture. The default server beside it is the
        # control: the opt-in row that answers 200 must answer 404 without the flag.
        srv = serve_labs.make_server(root, 0, allow_sluice=True)
        with Served(srv) as s:
            got = probe(s.port, SLUICE_ROWS)
            for (path, host, want), g in zip(SLUICE_ROWS, got):
                check(g == want, f"--allow-sluice {path} Host={host}: want {want}, got {g}")
            st, body = request(s.port, "/local/sluice/spec.md")
            check(body == b"SLUICE-SPEC", f"--allow-sluice served the wrong bytes for local/sluice/spec.md: {body!r}")
            # Every ROWS row that is not about the Sluice link reads the same under the flag.
            same = [r for r in ROWS if not r[0].startswith("/local/sluice")]
            for (path, host, want), g in zip(same, probe(s.port, same)):
                check(g == want, f"--allow-sluice changed an unrelated row {path} Host={host}: want {want}, got {g}")
        srv = serve_labs.make_server(root, 0)
        with Served(srv) as s:
            for path in ("/local/sluice/spec.md", "/local/sluice/spec.md?x=1"):
                st, _ = request(s.port, path)
                check(st == 404, f"CONTROL: without the flag {path} must be 404 (got {st}); the opt-in rows prove nothing")
        srv = serve_labs.make_server(root2, 0, allow_sluice=True)
        with Served(srv) as s:
            for (path, host, want), g in zip(SLUICE_ROWS_LINKED_LOCAL, probe(s.port, SLUICE_ROWS_LINKED_LOCAL)):
                check(g == want, f"--allow-sluice with a symlinked local/ {path}: want {want}, got {g}")
        # The stock handler on the same fixture leaks the planted in-subtree links: the
        # rows above are refusals of something real.
        stock = http.server.ThreadingHTTPServer(
            ("127.0.0.1", 0), functools.partial(QuietStock, directory=root))
        with Served(stock) as s:
            for path in ("/local/sluice/escape.txt", "/local/sluice/escdir/secret.txt"):
                st, _ = request(s.port, path)
                check(st == 200, f"CONTROL: stock handler should leak {path} (got {st}); the planted link is dead")
        # The flag reaches the handler through the command line (any position), and is off without it.
        for args, want in ((["{port}", root], 404), (["--allow-sluice", "{port}", root], 200),
                           (["{port}", root, "--allow-sluice"], 200)):
            got = cli_status(args, "/local/sluice/spec.md")
            check(got == want, f"CLI serve_labs.py {' '.join(a for a in args if a != root)}: /local/sluice/spec.md want {want}, got {got}")

    # Layer 3: the real tree. Every tracked page and everything it loads by
    # relative path must still be served.
    tracked = subprocess.run(["git", "ls-files", "--", "docs/design", "reference", "src/gui"],
                             cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    pages = sorted(f for f in tracked if f.endswith(".html") and os.path.isfile(os.path.join(ROOT, f)))
    check(len(pages) > 30, f"found only {len(pages)} tracked lab pages; the listing is broken")
    refs_re = re.compile(r"""["'`(]((?:\.\./)+[A-Za-z0-9_./-]+)|(?:src|href)=["']([A-Za-z0-9_][A-Za-z0-9_./-]*)["']""")
    DELIBERATE = ("local/sluice",)   # the Sluice lab's in-place link; it shows its no-spec state
    with Served(serve_labs.make_server(ROOT, 0)) as s:
        checked = 0
        for page in pages:
            st, _ = request(s.port, "/" + page)
            check(st == 200, f"/{page}: want 200, got {st}")
            with open(os.path.join(ROOT, page), encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
            for m in refs_re.finditer(text):
                ref = m.group(1) or m.group(2)
                target = posixpath.normpath(posixpath.join(posixpath.dirname(page), ref))
                if target.startswith("..") or any(target.startswith(d) for d in DELIBERATE):
                    continue
                if not os.path.isfile(os.path.join(ROOT, target)):
                    continue   # a page naming a file that does not exist is not this gate's concern
                st, _ = request(s.port, "/" + target)
                checked += 1
                check(st == 200, f"{page} loads {target}: want 200, got {st}")
        check(checked > 20, f"only {checked} page references were checked; the scan is broken")
        st, _ = request(s.port, "/docs/design/index.html")
        check(st == 200, "the navigator /docs/design/index.html is served")

    if fails:
        print(f"\nRED -- serve_labs_check: {len(fails)} failure(s)")
        return 1
    print(f"serve_labs_check: GREEN -- {len(ROWS)} rows on the hardened handler, stock-handler control leaks, "
          f"{len(SLUICE_ROWS)} --allow-sluice rows (off by default, CLI flag proven), "
          f"{len(pages)} lab pages and {checked} loaded files served")
    return 0


if __name__ == "__main__":
    sys.exit(main())
