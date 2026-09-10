#!/usr/bin/env python3
"""gen_lab_index — rebuild docs/design/index.html from the labs themselves.

Each card's text is the lab's own <title> and .tagline, read at generation time.
Writing descriptions into the index by hand would be a second copy of what every
lab already says about itself, and it would drift the first time a lab changed —
the same reason the GUI derives its controls from the presentation table and the
bend graphs are drawn by the shipped core rather than a JS twin.
"""
import html
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
D = ROOT / "docs/design"


def labs():
    for f in sorted(D.glob("*.html")):
        if f.name == "index.html":
            continue
        t = f.read_text(errors="ignore")
        m = re.search(r"<title>(.*?)</title>", t, re.S)
        title = html.unescape(m.group(1).strip()) if m else f.stem
        tag = re.search(r'class="tagline"[^>]*>(.*?)</div>', t, re.S)
        desc = re.sub(r"<[^>]+>", "", tag.group(1)) if tag else ""
        yield f.name, title, html.unescape(re.sub(r"\s+", " ", desc)).strip()[:260]


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
</style></head><body>
<h1>horde — design labs</h1>
<div class="sub">Every bench in <code>docs/design/</code>. Each card's text is the lab's
own tagline, read from the file at generation time — not a description written here,
which would be a second copy free to drift. Regenerate with
<code>python3 tools/gen_lab_index.py</code>.</div>
<div class="gui"><b>The instrument:</b>
  <a href="../../src/gui/gui2.html">gui2 (in development)</a>
  <a href="../../src/gui/gui.html">gui1 (shipped default)</a></div>
<div class="grid">
"""

FOOT = """</div>
</body></html>
"""


def card(name, title, desc):
    p = html.escape(desc) if desc else "<span class=none>no tagline</span>"
    return (f'  <a class="lab" href="{name}">\n'
            f"    <h2>{html.escape(title)}</h2>\n"
            f'    <div class="file">{name}</div>\n'
            f"    <p>{p}</p>\n"
            f"  </a>\n")


def main():
    entries = list(labs())
    out = HEAD + "".join(card(*e) for e in entries) + FOOT
    (D / "index.html").open("w", encoding="utf-8", newline="\n").write(out)
    print(f"gen_lab_index: wrote docs/design/index.html with {len(entries)} lab(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
