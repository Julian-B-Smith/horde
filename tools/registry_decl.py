#!/usr/bin/env python3
"""Emit the DECLARATION side of the conformance dump: id, key, global.

Deliberately separate from registry_dump.cpp, which emits patch_key from the
bytes state_save writes. Two independent sources is the whole point — deriving
`key` from `patch_key` (the obvious shortcut) would make FOUNDATIONS' C4 compare
a reconstruction against a string built from the same source.

STRIP COMMENTS BEFORE PARSING IDS. The first version ran \\d+ over the raw
kGlobalIds block and slurped digits out of "A12", "ADR-082" and "2026-08-11",
reporting 36 globals where there are 29 — which marked `dist` both global and
per-oscillator. registry_conformance caught it as C2 "leaf shadows an ancestor",
correctly, on real data. That was a free calibration of their tool and a real
defect in ours.
"""
import re, sys, pathlib

src = pathlib.Path(__file__).resolve().parent.parent / "src" / "hypersaw_clap.cpp"
s = src.read_text(encoding="utf-8")
params = re.search(r'static const ParamDef kParams\[\] = \{(.*?)\n\};', s, re.S).group(1)
gblk = re.search(r'constexpr clap_id kGlobalIds\[\] = \{(.*?)\};', s, re.S).group(1)
globals_ = set(re.findall(r'\b\d+\b', re.sub(r'//[^\n]*', '', gblk)))

rows = re.findall(r'\{(\d+), "([A-Za-z0-9_]+)", "', params)
if not rows:
    sys.exit("no params parsed — kParams shape changed")
print(f"# {len(rows)} params, {len(globals_)} global", file=sys.stderr)
for pid, key in rows:
    print(f"{pid}\t{key}\t{1 if pid in globals_ else 0}")
