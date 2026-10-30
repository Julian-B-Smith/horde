#!/usr/bin/env python3
"""design_vendor — the one definition of "vendoring" the design master (ADR-336, B595).

The chain of custody's third hop: the design master (docs/design-system/master/, the
Design System artifact's project/ tree) becomes the vendored copy the labs and the GUI 3
shell load (docs/design/arp-face/bundle.js, bundle.css, tokens.css). Until B595 that
transform lived only in prose (docs/design/arp-face/README.md), so "is the vendored copy
the master's?" could only be answered by hand. This file IS the transform, so
tools/design_chain_check.py can ask `vendored == vendor(master)` byte for byte.

THE TRANSFORM (reconstructed from the arp-face README and verified against the tree on
2026-10-30: the master's bundle.js and bundle.css at 1793374583-b6a5 equal the vendored
files minus their first line, byte for byte, and tokens_css() below regenerates the
vendored tokens.css line for line):
  - bundle.js, bundle.css: ONE header line is prepended, nothing else changes:
        /* vendored from the horde Design System artifact, version <v>, <date>; do not edit, re-vendor */
  - tokens.css: generated from the master's tokens.json (the artifact makes its own
    tokens.css at publish and never saves it, so the repo cannot copy one). Rules, as the
    arp-face README states them: `--<name>`; sources color.tokens, then spacing, radius,
    shadow, size, stroke, opacity, duration, screen, then type.families as --font-sans and
    --font-mono; a plain string goes in the bare :root only; a {light, dark} value puts
    light in :root and dark in BOTH dark blocks; a value exactly `{alias}` is var(--alias);
    the dark blocks are `@media (prefers-color-scheme: dark)` on :root:not([data-theme="light"])
    and :root[data-theme="dark"], in token order.

The stamp carries a DATE as well as the version. The date is an input, never read from the
clock (doctrine: no wall-clock reads in what a check compares), so the check passes the
date the vendored header already carries and only the version and bytes are judged.

Usage (the publish procedure in docs/design/CLAUDE.md and docs/design-system/README.md):
    python3 tools/design_vendor.py record-master --version <published id> --date YYYY-MM-DD
        rewrites docs/design-system/master/VERSION (the version, the date, a sha256 per file)
        from the tree as it stands; run it once the artifact is published from the merged tree.
    python3 tools/design_vendor.py vendor --date YYYY-MM-DD --out docs/design/arp-face
        writes the three vendored files, stamped with the version VERSION names.
Never hand-edit the vendored files or VERSION; re-run these.
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MASTER = ROOT / "docs/design-system/master"
VENDORED = ROOT / "docs/design/arp-face"

# The three vendored files and where each comes from in the master tree.
FILES = ("bundle.js", "bundle.css", "tokens.css")

HEADER = "/* vendored from the horde Design System artifact, version {v}, {d}; do not edit, re-vendor{x} */\n"
TOKENS_SUFFIX = " (generated from its tokens.json)"
# Parses either header form back into (version, date). The version is the artifact's
# `<digits>-<hex>` id; anything else in that slot is a malformed stamp, not a version.
HEADER_RE = re.compile(r"^/\* vendored from the horde Design System artifact, version "
                       r"(\d+-[0-9a-f]+), (\d{4}-\d{2}-\d{2}); do not edit, re-vendor"
                       r"( \(generated from its tokens\.json\))? \*/\n")

TOKEN_GROUPS = ("spacing", "radius", "shadow", "size", "stroke", "opacity", "duration", "screen")


def header(name, version, date):
    return HEADER.format(v=version, d=date, x=TOKENS_SUFFIX if name == "tokens.css" else "")


def parse_stamp(data):
    """(version, date, body) of a vendored file, or None when line 1 is not a stamp."""
    text = data.decode("utf-8")
    m = HEADER_RE.match(text)
    if not m:
        return None
    return m.group(1), m.group(2), text[m.end():].encode("utf-8")


def _css_value(v):
    # "Exactly {alias}" only: a brace inside a longer value is left alone, as the README says.
    m = re.fullmatch(r"\{([A-Za-z0-9_-]+)\}", v)
    return f"var(--{m.group(1)})" if m else v


def tokens_css_body(tokens):
    """tokens.css minus its header line, from a parsed tokens.json."""
    toks = list(tokens["color"]["tokens"])
    for g in TOKEN_GROUPS:
        toks += tokens[g]["tokens"]
    root, dark = [], []
    for t in toks:
        v = t["value"]
        if isinstance(v, dict):
            if set(v) != {"light", "dark"}:
                raise ValueError(f"token {t['name']}: a themed value must have exactly light and dark")
            root.append((t["name"], _css_value(v["light"])))
            dark.append((t["name"], _css_value(v["dark"])))
        else:
            root.append((t["name"], _css_value(v)))
    fam = tokens["type"]["families"]
    root += [("font-sans", fam["sans"]), ("font-mono", fam["mono"])]
    out = [":root {"] + [f"  --{n}: {v};" for n, v in root] + ["}"]
    out += ["@media (prefers-color-scheme: dark)" + " {", '  :root:not([data-theme="light"]) {']
    out += [f"    --{n}: {v};" for n, v in dark] + ["  }", "}"]
    out += [':root[data-theme="dark"] {'] + [f"  --{n}: {v};" for n, v in dark] + ["}"]
    return ("\n".join(out) + "\n").encode("utf-8")


def bodies(master):
    """{file: bytes} the master yields for each vendored file, before stamping."""
    master = pathlib.Path(master)
    return {
        "bundle.js": (master / "components/bundle.js").read_bytes(),
        "bundle.css": (master / "components/bundle.css").read_bytes(),
        "tokens.css": tokens_css_body(json.loads((master / "tokens.json").read_text(encoding="utf-8"))),
    }


def vendor_one(name, body, version, date):
    return header(name, version, date).encode("utf-8") + body


def vendor(master, version, date):
    """{file: bytes} exactly as they should sit in docs/design/arp-face/."""
    return {n: vendor_one(n, b, version, date) for n, b in bodies(master).items()}


def master_version(master=MASTER):
    """The version named in the master's VERSION file (`version: <id>`)."""
    m = re.search(r"^version: (\S+)$", (pathlib.Path(master) / "VERSION").read_text(), re.M)
    if not m:
        raise SystemExit("design_vendor: no `version:` line in docs/design-system/master/VERSION")
    return m.group(1)


ARTIFACT = "https://claude.ai/artifact/EGDwetDHwcvxhTbNffCzJc"
VERSION_HEAD = """# The design master's source (ADR-336 ruling 1, B595): the Design System artifact's
# project/ tree, byte-identical. Edit the master HERE, never in the artifact; merge, publish
# the artifact from the merged tree, then rewrite this file with the published version
# (python3 tools/design_vendor.py record-master --version <id> --date <d>) and re-vendor.
# tools/design_chain_check.py holds the tree to these hashes: every file listed, none extra.
"""


def master_files(master=MASTER):
    """Every file of the master tree except VERSION itself, sorted (no set iteration)."""
    master = pathlib.Path(master)
    return sorted(p.relative_to(master).as_posix() for p in master.rglob("*")
                  if p.is_file() and p != master / "VERSION")


def version_text(master, version, date):
    import hashlib
    master = pathlib.Path(master)
    files = master_files(master)
    lines = [VERSION_HEAD, f"artifact: {ARTIFACT}\n", f"version: {version}\n", f"date: {date}\n",
             f"files: {len(files)}\n"]
    lines += [f"{hashlib.sha256((master / f).read_bytes()).hexdigest()}  {f}\n" for f in files]
    return "".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    v_ = sub.add_parser("vendor", help="write the vendored files from the master at its VERSION")
    v_.add_argument("--date", required=True, help="the vendoring date for the stamp (YYYY-MM-DD)")
    v_.add_argument("--out", required=True, help="directory to write bundle.js, bundle.css, tokens.css")
    r_ = sub.add_parser("record-master", help="rewrite master/VERSION from the tree as it stands")
    r_.add_argument("--version", required=True, help="the published artifact version, e.g. 1793374583-b6a5")
    r_.add_argument("--date", required=True, help="YYYY-MM-DD")
    a = ap.parse_args()
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", a.date):
        raise SystemExit("design_vendor: --date must be YYYY-MM-DD")
    if a.cmd == "record-master":
        if not re.fullmatch(r"\d+-[0-9a-f]+", a.version):
            raise SystemExit("design_vendor: --version must be the artifact's <digits>-<hex> id")
        (MASTER / "VERSION").write_text(version_text(MASTER, a.version, a.date), encoding="utf-8")
        print(f"design_vendor: wrote docs/design-system/master/VERSION at {a.version}")
        return 0
    v = master_version()
    out = pathlib.Path(a.out)
    for n, b in vendor(MASTER, v, a.date).items():
        (out / n).write_bytes(b)
    print(f"design_vendor: wrote {', '.join(FILES)} to {a.out} at master {v}, stamped {a.date}; "
          "update the arp-face README's provenance row and the design ledger in the same PR")
    return 0


if __name__ == "__main__":
    sys.exit(main())
