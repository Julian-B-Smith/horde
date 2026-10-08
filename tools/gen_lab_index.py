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

RELEVANCE IS THE LAB'S OWN CLAIM TOO. The navigator lists the labs horde 2's plan
(docs/H2-PLAN.md) still uses; a lab says so about itself, the way it says review
state, rather than this file holding a list of names that would rot the first time
a lab was added or retired (the same reason as above):
  <meta name="lab-part" content="Engine">        a CURRENT lab, grouped under that
        plan part's heading. PARTS below owns the heading ORDER, because the order
        is the plan's, not the labs'. A current lab with no (or an unrecognised)
        lab-part lands under a final "Unsorted" heading, so a forgotten tag shows
        up as a visible miss instead of the lab silently vanishing.
  <meta name="lab-archive" content="reason">     the lab is no longer in the plan;
        it moves into the closed Archive section at the bottom, dimmed, with the
        reason shown. lab-archive wins over lab-part.
  <meta name="lab-superseded-by" ...>            also Archive (as before), with its
        link to the successor; it needs no lab-archive tag.
A lab awaiting review is pinned on top AND still listed in its own part (or the
Archive), so the part headings are a complete map of the plan and the pin is only
a to-do list. The pinned copy alone carries the pink review styling.

Deterministic and idempotent: no wall clock, no set iteration; labs are sorted by
filename and parts follow PARTS, so two runs give a byte-identical file.
"""
import html
import pathlib
import re
import subprocess
import sys

# Optional first argument: the tree to read and write. tools/labs_preview.sh runs THIS
# copy of the script (the main checkout's reviewed one) against its scratch merge, never
# the merge's own copy — a lab branch must not get to choose the code that runs (W2-06).
ROOT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1
        else pathlib.Path(__file__).parent.parent).resolve()
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


def meta(text, name):
    m = re.search(r'<meta name="%s" content="([^"]*)"' % name, text)
    return html.unescape(m.group(1)) if m else None


def labs():
    for f in tracked_labs():
        if f.parent != D or f.name == "index.html":
            continue
        t = f.read_text(errors="ignore")
        m = re.search(r"<title>(.*?)</title>", t, re.S)
        title = html.unescape(m.group(1).strip()) if m else f.stem
        tag = re.search(r'class="tagline"[^>]*>(.*?)</div>', t, re.S)
        desc = re.sub(r"<[^>]+>", "", tag.group(1)) if tag else ""
        yield {"name": f.name, "title": title,
               "desc": html.unescape(re.sub(r"\s+", " ", desc)).strip()[:260],
               "review": meta(t, "lab-review"), "sup": meta(t, "lab-superseded-by"),
               "part": meta(t, "lab-part"), "archive": meta(t, "lab-archive")}


# H2-PLAN.md's part order. A lab's lab-part must match one of these exactly, else
# it falls to "Unsorted" (see docstring).
PARTS = ["Plan and decisions", "Engine", "Pitch", "Modulation", "Morph and state",
         "Sources and routing", "FX", "Presentation", "Calibration"]


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
  h3 { font-size:12px; letter-spacing:2px; color:var(--dim); margin:22px 0 10px; }
  h3.first { margin-top:0; }
  .review-grid { margin-bottom:26px; }
  .lab.review { border-color:#f5169c; box-shadow:0 0 0 1px #f5169c inset; }
  .pill { display:inline-block; font-size:9px; letter-spacing:1px; padding:1px 6px;
          border-radius:3px; background:#f5169c; color:#fff; margin-bottom:6px; }
  .lab.superseded { opacity:.45; }
  details.archive { margin-top:30px; }
  details.archive summary { font-size:12px; letter-spacing:2px; color:var(--dim);
          cursor:pointer; margin-bottom:12px; }
  .why { color:var(--dim); font-size:10px; margin-top:4px; }
  .succ { color:var(--dim); font-size:10px; margin-top:4px; }
</style></head><body>
<h1>horde — design labs</h1>
<div class="sub">The labs horde 2's plan uses, grouped by plan part; older labs are in
the Archive at the bottom. Each card's text is the lab's own tagline, read from the
file at generation time — not a description written here, which would be a second
copy free to drift. Regenerate with <code>python3 tools/gen_lab_index.py</code>.</div>
<div class="gui"><b>The instrument:</b>
  <a href="../../src/gui/gui2.html">gui2 (in development)</a>
  <a href="../../src/gui/gui.html">gui1 (shipped default)</a></div>
"""

FOOT = "</body></html>\n"


def card(e, pinned=False, archived=False):
    p = html.escape(e["desc"]) if e["desc"] else "<span class=none>no tagline</span>"
    cls = "lab review" if pinned else "lab superseded" if archived else "lab"
    pill = (f'    <span class="pill">NEW · AWAITING REVIEW · {html.escape(e["review"])}</span>\n'
            if pinned else "")
    note = (f'    <div class="succ">superseded by {html.escape(e["sup"])}</div>\n' if e["sup"]
            else f'    <div class="why">{html.escape(e["archive"])}</div>\n' if archived else "")
    return (f'  <a class="{cls}" href="{e["name"]}">\n'
            f"{pill}"
            f'    <h2>{html.escape(e["title"])}</h2>\n'
            f'    <div class="file">{e["name"]}</div>\n'
            f"    <p>{p}</p>\n"
            f"{note}"
            f"  </a>\n")


def grid(entries, **kw):
    return '<div class="grid">\n' + "".join(card(e, **kw) for e in entries) + "</div>\n"


def main():
    entries = list(labs())
    archived = [e for e in entries if e["sup"] or e["archive"]]
    current = [e for e in entries if e not in archived]
    pinned = [e for e in entries if e["review"]]
    out = HEAD
    if pinned:
        out += (f'<h3 class="first">AWAITING YOUR REVIEW ({len(pinned)})</h3>\n'
                '<div class="grid review-grid">\n'
                + "".join(card(e, pinned=True) for e in pinned) + "</div>\n")
    sections = [(h, [e for e in current if e["part"] == h]) for h in PARTS]
    sections.append(("Unsorted", [e for e in current if e["part"] not in PARTS]))
    for heading, group in sections:
        if group:
            out += f"<h3>{html.escape(heading.upper())} ({len(group)})</h3>\n" + grid(group)
    if archived:
        out += (f'<details class="archive"><summary>Archive — {len(archived)} labs no longer '
                "in horde 2's plan</summary>\n" + grid(archived, archived=True) + "</details>\n")
    out += FOOT
    (D / "index.html").open("w", encoding="utf-8", newline="\n").write(out)
    print(f"gen_lab_index: wrote docs/design/index.html with {len(entries)} lab(s): "
          f"{len(pinned)} pinned for review, {len(current)} current "
          f"({len(sections[-1][1])} unsorted), {len(archived)} archived")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
