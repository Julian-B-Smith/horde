#!/usr/bin/env python3
"""gui_sink_check -- the plugin GUI's pages build markup only from literals, and
embed under a script policy that holds (B446 W3a).

WIRED: ./verify fast

  python3 tools/gui_sink_check.py [file.html ...]     (default: src/gui/*.html)

WHY. Text that reaches the GUI page from outside (preset and corner FILE names,
names carried in a session or preset file) must arrive as text. An HTML sink
re-parses whatever it is given, so one sink fed a file name lets that name
become markup: the corner-preset dropdown was built exactly that way, while the
preset dropdown above it escaped the same data, one rule with two
implementations. This makes the rule one check instead of a convention.

SINKS: an assignment to `.innerHTML` or `.outerHTML` (`=` or `+=`), and the
markup argument of `insertAdjacentHTML(...)`, `document.write(...)` and
`document.writeln(...)`. A sink is LITERAL when its value is only string
literals joined by `+`: '...', "...", or a template literal with no `${`.
Anything else is a finding unless EXCEPTIONS names that exact statement with a
reason. An exception that matches nothing is itself a finding, so the list
cannot outlive the code it excuses.

POLICY. Each GUI page is also run through tools/embed_file.py's `with_csp`, the
transform the build applies, and the result must carry exactly one policy with
`default-src 'none'`, a script-src made only of sha256 sources that match the
page's inline scripts, and no 'unsafe-inline', 'unsafe-eval', scheme or remote
origin anywhere. Whether the policy then HOLDS at run time is
tools/gui_webview_check's business (a real WKWebView); this is its static half.

URL CHANGES (B446 D-S5). In src/gui/gui*.html there is no pushState,
replaceState, location.hash or href="#…": the Windows bridge admits messages by
the sending document's URL, macOS by main frame, and the page must not move its
own URL. Planted cases of each are flagged, and an ordinary href is not.

DROPS (B448 C3). The GUI has no drag-and-drop feature, so every shipped page
must refuse every drop: a document-level, capture-phase listener for each of
dragenter, dragover and drop, whose handler calls preventDefault(). A stray file
or link dropped on the web view can then neither navigate it away nor hand the
page a file. The three registrations are read as written (a literal
`document.addEventListener('drop', fn, true)` or `{capture: true}`; the handler
is inline or a named function in the page), comments blanked first, so a guard
that is only commented out reads as absent. Also refused anywhere in a page
(fail closed, comments included): a read of dataTransfer.files / .items /
.getData, getAsFile, webkitGetAsEntry, FileReader, and draggable="true", unless
DRAGGABLE_EXCEPTIONS names it with a reason. Limit: preventDefault is found in
the handler's text, not proven to run on every path; that is the run-time half,
gui_webview_check, which dispatches a drop in the real web view.

MUST-FAIL CONTROLS, on in-memory text every run: a planted `el.innerHTML = name`,
a planted concatenation and template with a variable, each other sink, and the
pre-fix corner-dropdown line are all flagged; literal sinks (plain, joined
across lines, an empty string) are not; `with_csp` refuses a page with an
inline handler, an external script, or a policy of its own; a page whose script
changes after embedding no longer matches its hash. The drop guard's controls are
copies of the real gui2.html with one defect planted, written to a temp dir and
scanned by the same file-level function the real pages go through: the drop
listener removed, the handler without preventDefault, a non-capture listener, a
non-document listener, the guard only in a comment, a planted dataTransfer.files
read, a planted draggable="true". If any control misreads,
the check is RED: a scanner that cannot see a planted sink proves nothing about
the tree.
"""
import base64
import hashlib
import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import embed_file  # noqa: E402  (the build's own transform, never a copy of it)

# (file name, the sink statement with whitespace collapsed, reason). Keep short.
EXCEPTIONS = [
    ("gui2.html", "el.innerHTML = rt",
     "rt is built from constant strings and Number.toFixed() output only "
     "(the per-frame coherence readout); no outside text can reach it"),
]

# (file name, the matched text, reason): a draggable="true" element a page may keep.
# None today: the GUI has no drag-and-drop feature (B448 C3). Keep short.
DRAGGABLE_EXCEPTIONS = []

_ASSIGN = re.compile(r"\.(innerHTML|outerHTML)\s*(\+?=)(?!=)")
_CALL = re.compile(r"(\binsertAdjacentHTML|\bdocument\s*\.\s*write(?:ln)?)\s*\(")

# SAME-DOCUMENT URL CHANGES (B446 D-S5). The bridge admits messages only from the
# embedded page. On Windows that is a match on the sending document's URL
# (src/gui/embedded_page_policy.h; a #fragment is tolerated, nothing else); macOS
# checks the main frame (hypersaw_gui.mm). A page that rewrites its own URL with
# pushState/replaceState, or moves it with location.hash or an href="#…" link,
# would make the Windows check depend on margins it should not need. The pages
# need none of these, so none may appear in src/gui/gui*.html, comments included
# (fail closed: a mention is cheaper to reword than to argue about).
_URL_CHANGE = re.compile(r"\b(?:pushState|replaceState)\b|\blocation\s*\.\s*hash\b|\bhref\s*=\s*[\"']#")


def url_changes(name, src):
    """['name:line: match'] for every same-document URL change in `src`."""
    return [f"{name}:{src.count(chr(10), 0, m.start()) + 1}: {m.group(0)}"
            for m in _URL_CHANGE.finditer(src)]


def _skip_string(src, i):
    """Index just past the string literal starting at src[i] (a quote or a
    backtick). For a template, also report whether it interpolates."""
    q = src[i]
    j = i + 1
    interp = False
    depth = 0
    while j < len(src):
        c = src[j]
        if c == "\\":
            j += 2
            continue
        if q == "`" and depth == 0 and src.startswith("${", j):
            interp = True
            depth = 1
            j += 2
            continue
        if depth:
            if c in "'\"`":
                j, _ = _skip_string(src, j)
                continue
            depth += {"{": 1, "}": -1}.get(c, 0)
            j += 1
            continue
        if c == q:
            return j + 1, interp
        if c == "\n" and q != "`":
            return j, interp          # unterminated: stop at the line end
        j += 1
    return j, interp


def _expression(src, i, stop_at_comma):
    """The JS expression starting at src[i]: up to `;`, an unmatched closer, a
    top-level `,` (call arguments), or a line break that does not continue the
    expression. Returns (text, end)."""
    j = i
    depth = 0
    while j < len(src):
        c = src[j]
        if c in "'\"`":
            j, _ = _skip_string(src, j)
            continue
        if src.startswith("//", j):
            nl = src.find("\n", j)
            j = len(src) if nl < 0 else nl
            continue
        if src.startswith("/*", j):
            end = src.find("*/", j + 2)
            j = len(src) if end < 0 else end + 2
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (c == ";" or (c == "," and stop_at_comma)):
            break
        elif depth == 0 and c == "\n":
            before = src[i:j].rstrip()
            after = src[j:].lstrip()
            if not (before.endswith(("+", "(", ",", "?", ":", "=")) or after.startswith(("+", "?", ":", "."))):
                break
        j += 1
    return src[i:j].strip(), j


def is_literal(expr):
    """Only string literals (no `${`) joined by `+`, with whitespace and comments."""
    i = 0
    want_operand = True
    seen = False
    while i < len(expr):
        c = expr[i]
        if c.isspace():
            i += 1
        elif expr.startswith("//", i):
            nl = expr.find("\n", i)
            i = len(expr) if nl < 0 else nl
        elif expr.startswith("/*", i):
            end = expr.find("*/", i + 2)
            i = len(expr) if end < 0 else end + 2
        elif want_operand and c in "'\"`":
            i, interp = _skip_string(expr, i)
            if interp:
                return False
            want_operand = False
            seen = True
        elif not want_operand and c == "+":
            want_operand = True
            i += 1
        else:
            return False
    return seen and not want_operand


def find_sinks(src):
    """[(line, statement, value_expression)] for every sink in `src`."""
    out = []
    for m in _ASSIGN.finditer(src):
        value, _ = _expression(src, m.end(), stop_at_comma=False)
        start = src.rfind("\n", 0, m.start()) + 1
        target = src[start:m.start()].strip().split()[-1] if src[start:m.start()].strip() else ""
        stmt = " ".join(f"{target}.{m.group(1)} {m.group(2)} {value}".split())
        out.append((src.count("\n", 0, m.start()) + 1, stmt, value))
    for m in _CALL.finditer(src):
        args, _ = _expression(src, m.end(), stop_at_comma=False)
        # the markup is the LAST argument (insertAdjacentHTML(position, markup))
        parts, k = [], 0
        while k < len(args):
            a, k = _expression(args, k, stop_at_comma=True)
            parts.append(a)
            k += 1
        value = parts[-1] if parts else ""
        stmt = " ".join(f"{m.group(1)}({args})".split())
        out.append((src.count("\n", 0, m.start()) + 1, stmt, value))
    return sorted(out)


def findings(name, src, exceptions):
    """Non-literal sinks not excused, plus the exceptions they leave unused."""
    bad, used = [], set()
    for line, stmt, value in find_sinks(src):
        if is_literal(value):
            continue
        hit = [e for e in exceptions if e[0] == name and e[1] == stmt]
        if hit:
            used.add(hit[0][1])
            continue
        bad.append(f"{name}:{line}: {stmt}")
    return bad, used


def policy_problems(name, html):
    """Problems with the policy the build would embed for `html`."""
    try:
        out = embed_file.with_csp(html)
    except ValueError as e:
        return [f"{name}: the build refuses this page: {e}"]
    metas = re.findall(r'<meta http-equiv="Content-Security-Policy" content="([^"]*)">', out)
    if len(metas) != 1:
        return [f"{name}: {len(metas)} policy metas after embedding, want exactly 1"]
    csp = metas[0]
    probs = []
    directives = {d.split()[0]: d.split()[1:] for d in (x.strip() for x in csp.split(";")) if d}
    if directives.get("default-src") != ["'none'"]:
        probs.append(f"{name}: default-src is {directives.get('default-src')}, want 'none'")
    if re.search(r"'unsafe-eval'|https?:|\*|data:|blob:|file:", csp):
        probs.append(f"{name}: the policy allows a scheme, origin or eval: {csp}")
    src = directives.get("script-src", [])
    if "'unsafe-inline'" in src or not all(s.startswith("'sha256-") for s in src):
        probs.append(f"{name}: script-src must be sha256 sources only: {src}")
    want = set()
    for m in embed_file._SCRIPT_RE.finditer(out):
        body = m.group(2).replace("\r\n", "\n").replace("\r", "\n")
        want.add("'sha256-" + base64.b64encode(hashlib.sha256(body.encode()).digest()).decode() + "'")
    if set(src) != want:
        probs.append(f"{name}: script-src hashes {sorted(src)} do not match the page's scripts {sorted(want)}")
    return probs


# DROPS (B448 C3): the three events a page must cancel, and what a page must not do.
_DROP_EVENTS = ("dragenter", "dragover", "drop")
_CAPTURE_ARG = re.compile(r"true|\{[^{}]*\bcapture\s*:\s*true\b[^{}]*\}")
_IDENT = re.compile(r"[A-Za-z_$][\w$]*")
_DT_READ = re.compile(r"\bdataTransfer\s*\??\s*(?:\.|\[\s*['\"])\s*(?:files|items|getData)\b"
                      r"|\b(?:getAsFile|getAsFileSystemHandle|webkitGetAsEntry|FileReader)\b")
# (?<![\w-]) so data-draggable and a class name are not draggable.
_DRAGGABLE = re.compile(r"(?<![\w-])draggable\s*=\s*[\"']?\s*true\b[\"']?|\.draggable\s*=\s*true\b"
                        r"|setAttribute\s*\(\s*['\"]draggable['\"]\s*,\s*['\"]?\s*true")


def _blank_comments(js):
    """`js` with every // and /* */ comment replaced by spaces (newlines kept), so a
    guard that only exists in a comment is not read as present. Strings are skipped
    whole, so a // inside one is not a comment."""
    out, i = [], 0
    while i < len(js):
        if js.startswith("//", i):
            nl = js.find("\n", i)
            end = len(js) if nl < 0 else nl
        elif js.startswith("/*", i):
            close = js.find("*/", i + 2)
            end = len(js) if close < 0 else close + 2
        elif js[i] in "'\"`":
            end, _ = _skip_string(js, i)
            out.append(js[i:end])
            i = end
            continue
        else:
            out.append(js[i])
            i += 1
            continue
        out.append(re.sub(r"[^\n]", " ", js[i:end]))
        i = end
    return "".join(out)


def _call_args(text):
    """The top-level arguments of the call whose `(` has just been consumed."""
    args_text, _ = _expression(text, 0, stop_at_comma=False)
    parts, k = [], 0
    while k < len(args_text):
        a, k = _expression(args_text, k, stop_at_comma=True)
        parts.append(a)
        k += 1
    return parts


def _handler_text(js, handler):
    """The source of a listener argument: itself when inline, else the page's
    `function name(...) {...}` or `const name = ...` definition; None if absent."""
    if not _IDENT.fullmatch(handler):
        return handler
    d = re.search(r"\bfunction\s+" + re.escape(handler) + r"\s*\(", js)
    if d:
        return _expression(js, d.end() - 1, stop_at_comma=False)[0]
    d = re.search(r"\b(?:const|let|var)\s+" + re.escape(handler) + r"\s*=(?!=)", js)
    return _expression(js, d.end(), stop_at_comma=False)[0] if d else None


def drop_guard_problems(name, src, exceptions=DRAGGABLE_EXCEPTIONS):
    """Problems with how `src` (a GUI page) handles drags and drops; [] when it
    refuses every drop, reads no dropped data and has no draggable element."""
    js = _blank_comments("\n".join(m.group(2) for m in embed_file._SCRIPT_RE.finditer(src)))
    probs = []
    for ev in _DROP_EVENTS:
        why = f"no document-level listener for '{ev}'"
        for m in re.finditer(r"\bdocument\s*\.\s*addEventListener\s*\(", js):
            args = _call_args(js[m.end():])
            if len(args) < 2 or args[0] not in (f"'{ev}'", f'"{ev}"'):
                continue
            if len(args) < 3 or not _CAPTURE_ARG.fullmatch(args[2]):
                why = f"the '{ev}' listener is not capture-phase"
                continue
            body = _handler_text(js, args[1])
            if body is None or not re.search(r"\.\s*preventDefault\s*\(\s*\)", body):
                why = f"the '{ev}' handler does not call preventDefault()"
                continue
            why = None
            break
        if why:
            probs.append(f"{name}: {why}")
    for m in _DT_READ.finditer(src):
        probs.append(f"{name}:{src.count(chr(10), 0, m.start()) + 1}: reads dropped data: {m.group(0)}")
    for m in _DRAGGABLE.finditer(src):
        if not any(e[0] == name and e[1] == m.group(0) for e in exceptions):
            probs.append(f"{name}:{src.count(chr(10), 0, m.start()) + 1}: draggable element: {m.group(0)}")
    return probs


def drop_guard_controls():
    """The drop guard's must-fail controls. Each plant is the REAL gui2.html with
    one defect, written to a temp dir and scanned through the same file read the
    real pages take; the real page must read clean."""
    real = (ROOT / "src/gui/gui2.html").read_text(encoding="utf-8")
    fn = "function refuseDrop(ev) {\n  ev.preventDefault();"
    line = {e: f"document.addEventListener('{e}', refuseDrop, true);" for e in _DROP_EVENTS}
    plants = [
        ("the drop listener removed", real.replace(line["drop"], "", 1)),
        ("the dragover listener removed", real.replace(line["dragover"], "", 1)),
        ("the handler without preventDefault", real.replace(fn, "function refuseDrop(ev) {\n  void ev;", 1)),
        ("a listener that is not capture-phase",
         real.replace(line["drop"], "document.addEventListener('drop', refuseDrop);", 1)),
        ("a listener on window, not document", real.replace(line["drop"], line["drop"].replace("document", "window"), 1)),
        ("a listener on an element", real.replace(line["drop"], line["drop"].replace("document", "document.body"), 1)),
        ("the guard only in a comment", real.replace(line["drop"], "// " + line["drop"], 1)),
        ("a planted dataTransfer.files read", real.replace(fn, fn + "\n  const f = ev.dataTransfer.files;", 1)),
        ("a planted dataTransfer.items read", real.replace(fn, fn + "\n  const f = ev.dataTransfer?.items;", 1)),
        ("a planted draggable=\"true\" element", real.replace("<script>", "<div draggable=\"true\"></div>\n<script>", 1)),
    ]
    out = [("drops: the real gui2.html reads clean", not drop_guard_problems("gui2.html", real))]
    with tempfile.TemporaryDirectory() as tmp:
        for i, (label, text) in enumerate(plants):
            path = pathlib.Path(tmp) / f"plant{i}.html"
            path.write_text(text, encoding="utf-8")
            # a plant that changed nothing would read clean for the wrong reason
            out.append((f"drops: refuses {label}",
                        text != real and bool(drop_guard_problems(path.name, path.read_text(encoding="utf-8")))))
    out.append(("drops: data-draggable and a .drag class are not draggable",
                not drop_guard_problems("x.html", real + '<i data-draggable="true" class="drag"></i>')))
    planted = real.replace("<script>", '<div draggable="true"></div><script>', 1)
    out.append(("drops: a named exception excuses its element",
                planted != real and not drop_guard_problems("gui2.html", planted, [("gui2.html", 'draggable="true"', "r")])))
    return out


def controls():
    """Each planted case and whether the scanner reads it correctly."""
    flagged = lambda s: bool(findings("plant.html", s, [])[0])
    cases = [
        ("plain variable", "el.innerHTML = name;", True),
        ("concatenation", "el.innerHTML = '<b>' + name + '</b>';", True),
        ("template with ${}", "el.innerHTML = `<option>${n}</option>`;", True),
        ("+= variable", "el.innerHTML += name;", True),
        ("outerHTML", "el.outerHTML = name;", True),
        ("insertAdjacentHTML", "el.insertAdjacentHTML('beforeend', name);", True),
        ("document.write", "document.write(name);", True),
        ("document.writeln", "document.writeln('<p>' + x);", True),
        ("pre-fix corner dropdown",
         "ld.innerHTML = '<option value=\"\">custom</option>' +\n"
         "          names.map(n => `<option>${n}</option>`).join('');", True),
        ("empty string", "host.innerHTML = '';", False),
        ("literal joined across lines", "box.innerHTML = '<label>W</label>' +\n    '<output>0</output>';", False),
        ("template without ${}", "row.innerHTML = `<label>Scale</label>`;", False),
        ("literal insertAdjacentHTML", "el.insertAdjacentHTML('beforeend', '<hr>');", False),
        ("comparison is not a sink", "if (el.innerHTML === name) x = 1;", False),
    ]
    out = [(f"sink: {label}", flagged(src) == want) for label, src, want in cases]
    url_cases = [
        ("history.pushState", "history.pushState({}, '', '/x');", True),
        ("history.replaceState", "history.replaceState(null, '', '#a');", True),
        ("location.hash assignment", "location.hash = 'tab2';", True),
        ("window.location.hash read", "const t = window.location.hash;", True),
        ("href=\"#…\" link", "<a href=\"#top\">top</a>", True),
        ("href='#' link", "<a href='#'>x</a>", True),
        ("an ordinary href", "<a href=\"https://example.invalid/\">x</a>", False),
        ("a variable called hash", "const hash = sha(x); location.reload;", False),
        ("a data-href attribute", "<b data-x=\"#fff\" style=\"color:#fff\">x</b>", False),
    ]
    out += [(f"url change: {label}", bool(url_changes("plant.html", src)) == want)
            for label, src, want in url_cases]
    out += drop_guard_controls()
    # an exception matches by statement and is reported when unused
    bad, used = findings("plant.html", "el.innerHTML = rt;", [("plant.html", "el.innerHTML = rt", "r")])
    out.append(("exception excuses its own statement", not bad and used == {"el.innerHTML = rt"}))
    page = "<html><head><meta charset=\"utf-8\"></head><body><script>var a = 1;</script></body></html>"
    out.append(("policy: a clean page passes", not policy_problems("ok.html", page)))
    for label, bad_page in [
        ("inline handler", page.replace("<body>", "<body><button onclick=\"go()\">x</button>")),
        ("external script", page.replace("<script>", "<script src=\"https://cdn.example/x.js\">")),
        ("page's own policy", page.replace("<head>", "<head><meta http-equiv=\"Content-Security-Policy\" content=\"x\">")),
    ]:
        out.append((f"policy: refuses {label}", bool(policy_problems("bad.html", bad_page))))
    # a script edited after its hash was taken no longer matches
    embedded = embed_file.with_csp(page)
    tampered = embedded.replace("var a = 1;", "var a = 2;")
    csp = re.search(r'content="([^"]*)"', tampered).group(1)
    body_hash = "'sha256-" + base64.b64encode(hashlib.sha256(b"var a = 2;").digest()).decode() + "'"
    out.append(("policy: a hash is bound to its script", body_hash not in csp))
    return out


def main(argv):
    files = [pathlib.Path(a) for a in argv] or sorted((ROOT / "src/gui").glob("*.html"))
    red = False
    for label, ok in controls():
        if not ok:
            print(f"gui_sink_check: CONTROL misread: {label}", file=sys.stderr)
            red = True
    if not files:
        print("gui_sink_check: no GUI pages found; a scan of nothing proves nothing", file=sys.stderr)
        return 1
    used_all, sinks = set(), 0
    for f in files:
        try:
            src = f.read_text(encoding="utf-8")
        except OSError as e:
            print(f"gui_sink_check: cannot read {f}: {e}", file=sys.stderr)
            red = True
            continue
        sinks += len(find_sinks(src))
        bad, used = findings(f.name, src, EXCEPTIONS)
        used_all |= {(f.name, u) for u in used}
        for b in bad:
            print(f"gui_sink_check: non-literal HTML sink {b}", file=sys.stderr)
            print("    build it with DOM nodes and textContent / new Option(text, value),"
                  " or add an EXCEPTION with its reason", file=sys.stderr)
            red = True
        for p in drop_guard_problems(f.name, src):
            print(f"gui_sink_check: DROPS {p}", file=sys.stderr)
            print("    every shipped page must refuse every drop (see DROPS in the file header)", file=sys.stderr)
            red = True
        for p in policy_problems(f.name, src):
            print(f"gui_sink_check: POLICY {p}", file=sys.stderr)
            red = True
        if f.name.startswith("gui"):
            for u in url_changes(f.name, src):
                print(f"gui_sink_check: same-document URL change {u}", file=sys.stderr)
                print("    the bridge admits messages by document URL on Windows; keep the page's URL fixed",
                      file=sys.stderr)
                red = True
    if not argv:   # unused exceptions are judged only on the real tree
        for name, stmt, _ in EXCEPTIONS:
            if (name, stmt) not in used_all:
                print(f"gui_sink_check: EXCEPTION matches nothing (stale): {name}: {stmt}", file=sys.stderr)
                red = True
    print(f"gui_sink_check: {'FAIL' if red else 'OK'} ({len(files)} pages, {sinks} sinks, "
          f"{len(EXCEPTIONS)} excepted, {len(controls())} controls)")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
