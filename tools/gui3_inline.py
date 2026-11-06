#!/usr/bin/env python3
"""GUI 3 as one self-contained page, for a plug-in's web view (B628, docs/proposals/gui-on-engine.md).

  python3 tools/gui3_inline.py <out.html>

Reads docs/design/gui3.html and puts every <link rel=stylesheet> and <script src> it names inline, in place, so the page
loads nothing. The plug-in's build then hands the result to tools/embed_file.py, which adds the Content-Security-Policy
(every inline script allowed by its hash, inline styles, everything else 'none') and embeds the bytes. So this writes no
policy of its own, and it changes nothing the shell does: inside the plug-in the page finds `window.hordeNativeSend`,
bound before it loads, and talks to the engine through it (engine-stub.js, NativeTransport).

What it strips, and why: the master's Google Fonts @import. Under the policy no font may load, so the page falls back to
the system's fonts, as the legacy GUI does. Nothing else in GUI 3 reaches outside the page: every storage use is wrapped,
and `url(#…)` names in-page SVG. gui3_browser_check loads the output under embed_file's policy, on recorded frames, and
fails on any policy violation or uncaught error, so a new external reference fails there, not in a host.

`</script` inside a script is escaped, or it would end the inline block early.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DESIGN = ROOT / "docs" / "design"
IMPORT = re.compile(r"@import\s+url\([^)]*\)\s*;?")


def inline(design=DESIGN):
    page = (design / "gui3.html").read_text(encoding="utf-8")

    def css(m):
        text = IMPORT.sub("", (design / m.group(1)).read_text(encoding="utf-8"))
        return "<style>\n" + text + "\n</style>"

    def js(m):
        return "<script>\n" + (design / m.group(1)).read_text(encoding="utf-8").replace("</script", "<\\/script") + "\n</script>"

    page = re.sub(r'<link rel="stylesheet" href="([^"]+)">', css, page)
    page = re.sub(r'<script src="([^"]+)"></script>', js, page)
    left = re.findall(r'<link[^>]+href=|<script[^>]+src=', page)
    if left:   # a reference written another way would load nothing under the policy, silently
        raise ValueError("gui3_inline: references left after inlining: " + ", ".join(left[:4]))
    return page


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    out = pathlib.Path(sys.argv[1])
    text = inline()
    out.write_text(text, encoding="utf-8")
    print(f"gui3_inline: wrote {out} ({len(text.encode('utf-8'))} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
