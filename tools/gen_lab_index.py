#!/usr/bin/env python3
"""gen_lab_index — rebuild docs/design/index.html from the labs themselves.

Each card's text is the lab's own <title> and .tagline, read at generation time.
Writing descriptions into the index by hand would be a second copy of what every
lab already says about itself, and it would drift the first time a lab changed —
the same reason the GUI derives its controls from the presentation table and the
bend graphs are drawn by the shipped core rather than a JS twin.

REVIEW STATE IS THE LAB'S OWN CLAIM TOO, for the same reason. A lab awaiting the
human's review carries `<meta name="lab-review" content="B207 · 2026-09-22">` and
is pinned to the top under "Awaiting your review"; a lab another lab replaced
carries `<meta name="lab-superseded-by" content="winner.html">` and is dimmed with
a link to its successor. The lead removes `lab-review` when the human rules. A
date-based "new" badge was rejected: it would read the wall clock, so the same
tree would generate a different index tomorrow, and "new" is not what matters —
"you have not looked at this yet" is.
"""
import html
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
D = ROOT / "docs/design"


def tracked_labs():
    # TRACKED FILES ONLY, never a glob of the working tree. A glob also finds
    # gitignored local-only labs — quantum-morph-lab.html is ignored because it
    # names a private sibling (ADR-014) — and the committed index then links a
    # file no clone has, and would publish its title and tagline if they carried
    # the name. `git ls-files` is the set a reader of the repo can actually open.
    out = subprocess.run(["git", "ls-files", "--", "docs/design/*.html"], cwd=ROOT,
                         capture_output=True, text=True, check=True).stdout.split()
    return sorted(ROOT / p for p in out)


def labs():
    for f in tracked_labs():
        if f.parent != D or f.name == "index.html":
            continue
        t = f.read_text(errors="ignore")
        m = re.search(r"<title>(.*?)</title>", t, re.S)
        title = html.unescape(m.group(1).strip()) if m else f.stem
        tag = re.search(r'class="tagline"[^>]*>(.*?)</div>', t, re.S)
        desc = re.sub(r"<[^>]+>", "", tag.group(1)) if tag else ""
        rv = re.search(r'<meta name="lab-review" content="([^"]*)"', t)
        sup = re.search(r'<meta name="lab-superseded-by" content="([^"]*)"', t)
        yield (f.name, title, html.unescape(re.sub(r"\s+", " ", desc)).strip()[:260],
               html.unescape(rv.group(1)) if rv else None, sup.group(1) if sup else None)


# Everything outside the card grid is static; the grid is the only derived part.
HEAD = """<!doctype html><html><head><meta charset="utf-8">
<title>horde — design labs</title>
<style>
  :root { --bg:#0b0e13; --panel:#11151f; --line:#2a3040; --dim:#7f8899;
           --text:#cdd6e4; --pull:#5ff2e0; }
  * { box-sizing:border-box; margin:0; }
  body { background:var(--bg); color:var(--text); padding:22px 26px 60px;
         font:13px/1.5 ui-monospace,Menlo,Consolas,monospace; }
  h1 { font-size:15px; letter-spacing:2px; color:var(--pull); margin-bottom:4px; }
  .sub { color:var(--dim); margin-bottom:22px; max-width:70ch; }
  .grid { display:grid; gap:12px; grid-template-columns:repeat(auto-fill,minmax(310px,1fr)); }
  .lab { display:block; background:var(--panel); border:1px solid var(--line);
          border-radius:5px; padding:12px 14px; text-decoration:none; color:inherit; }
  .lab:hover { border-color:var(--pull); }
  .lab h2 { font-size:12px; color:var(--pull); letter-spacing:1px; }
  .file { color:var(--dim); font-size:10px; margin:2px 0 7px; }
  .lab p { color:var(--text); font-size:11px; opacity:.85; }
  .none { color:var(--dim); }
  .gui { margin-bottom:22px; }
  .gui a { color:var(--pull); margin-right:16px; }
  h3 { font-size:12px; letter-spacing:2px; color:var(--dim); margin:0 0 10px; }
  .review-grid { margin-bottom:26px; }
  .lab.review { border-color:#f5169c; box-shadow:0 0 0 1px #f5169c inset; }
  .pill { display:inline-block; font-size:9px; letter-spacing:1px; padding:1px 6px;
          border-radius:3px; background:#f5169c; color:#fff; margin-bottom:6px; }
  .lab.superseded { opacity:.45; }
  .succ { color:var(--dim); font-size:10px; margin-top:4px; }
</style></head><body>
<h1>horde — design labs</h1>
<div class="sub">Every bench in <code>docs/design/</code>. Each card's text is the lab's
own tagline, read from the file at generation time — not a description written here,
which would be a second copy free to drift. Regenerate with
<code>python3 tools/gen_lab_index.py</code>.</div>
<div class="gui"><b>The instrument:</b>
  <a href="../../src/gui/gui2.html">gui2 (in development)</a>
  <a href="../../src/gui/gui.html">gui1 (shipped default)</a></div>
"""

FOOT = """</div>
</body></html>
"""


def card(name, title, desc, review, sup):
    p = html.escape(desc) if desc else "<span class=none>no tagline</span>"
    cls = "lab review" if review else "lab superseded" if sup else "lab"
    pill = f'    <span class="pill">NEW · AWAITING REVIEW · {html.escape(review)}</span>\n' if review else ""
    succ = f'    <div class="succ">superseded by {html.escape(sup)}</div>\n' if sup else ""
    return (f'  <a class="{cls}" href="{name}">\n'
            f"{pill}"
            f"    <h2>{html.escape(title)}</h2>\n"
            f'    <div class="file">{name}</div>\n'
            f"    <p>{p}</p>\n"
            f"{succ}"
            f"  </a>\n")


def main():
    entries = list(labs())
    pending = [e for e in entries if e[3]]
    out = HEAD
    if pending:
        out += (f"<h3>AWAITING YOUR REVIEW ({len(pending)})</h3>\n"
                '<div class="grid review-grid">\n' + "".join(card(*e) for e in pending) + "</div>\n"
                "<h3>ALL LABS</h3>\n")
    out += '<div class="grid">\n' + "".join(card(*e) for e in entries if not e[3]) + FOOT
    (D / "index.html").open("w", encoding="utf-8", newline="\n").write(out)
    print(f"gen_lab_index: wrote docs/design/index.html with {len(entries)} lab(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
