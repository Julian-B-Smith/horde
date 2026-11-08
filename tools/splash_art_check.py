#!/usr/bin/env python3
"""splash_art_check — the Mindlathe splash's embedded art is the asset, byte for byte (GUI-54).

WIRED: ./verify fast (wired in the PR that creates it, GUI-54 A1, under ADR-180 §1)

The plug-in's page can load no image (default-src 'none', and gui_sink_check forbids data:), so `Horde.Splash` carries the PNG as
base64 inside the design master's bundle.js, between the markers `/* @mindlathe-png */'` and `'/* @mindlathe-png-end */`. A
re-exported asset with a stale base64, or the reverse, would ship a logo nobody reviewed. This holds the two together, in the
master and in the vendored copy the shell loads, and checks the asset is what the decoder reads: 8-bit RGB or RGBA, not interlaced,
with no chunk but IHDR, IDAT and IEND (no EXIF or text riding into a public repo).

Must-fail controls run every time: one changed byte in the asset, and an asset carrying a tEXt chunk, each read red.
"""
import base64
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSET = ROOT / "docs/design-system/master/assets/Logos/mindlathe-saturated.png"
BUNDLES = [ROOT / "docs/design-system/master/components/bundle.js", ROOT / "docs/design/arp-face/bundle.js"]
MARK = re.compile(r"/\* @mindlathe-png \*/'([A-Za-z0-9+/=]*)'/\* @mindlathe-png-end \*/")
SIG = b"\x89PNG\r\n\x1a\n"


def png_problems(data):
    if not data.startswith(SIG):
        return ["not a PNG"]
    probs, at, kinds = [], 8, []
    while at + 8 <= len(data):
        n = struct.unpack(">I", data[at:at + 4])[0]
        kinds.append(data[at + 4:at + 8].decode("latin1"))
        if kinds[-1] == "IHDR":
            w, h, depth, ctype, _, _, lace = struct.unpack(">IIBBBBB", data[at + 8:at + 21])
            if depth != 8 or ctype not in (2, 6) or lace != 0:
                probs.append(f"IHDR depth {depth}, colour type {ctype}, interlace {lace}: the decoder reads 8-bit RGB/RGBA, not interlaced")
        at += 12 + n
    extra = sorted(set(kinds) - {"IHDR", "IDAT", "IEND"})
    if extra:
        probs.append("chunks beyond IHDR/IDAT/IEND: " + ", ".join(extra))
    return probs


def problems(asset, bundles):
    probs = [f"asset: {p}" for p in png_problems(asset)]
    for name, text in bundles:
        found = MARK.findall(text)
        if len(found) != 1:
            probs.append(f"{name}: {len(found)} marked art blocks, want 1")
        elif base64.b64decode(found[0]) != asset:
            probs.append(f"{name}: the embedded art differs from {ASSET.relative_to(ROOT)}")
    return probs


def main():
    asset = ASSET.read_bytes()
    bundles = [(str(p.relative_to(ROOT)), p.read_text(encoding="utf-8")) for p in BUNDLES]
    probs = problems(asset, bundles)
    # must-fail controls: the detector has to see a one-byte drift and a smuggled text chunk
    flipped = bytearray(asset); flipped[len(flipped) // 2] ^= 1
    texted = asset[:33] + struct.pack(">I", 4) + b"tEXt" + b"a\x00bc" + b"\x00\x00\x00\x00" + asset[33:]
    if not problems(bytes(flipped), bundles):
        probs.append("CONTROL: a one-byte change to the asset read green")
    if not any("tEXt" in p for p in problems(texted, [])):
        probs.append("CONTROL: an asset with a tEXt chunk read green")
    if probs:
        print("splash_art_check: RED")
        for p in probs:
            print("  " + p)
        return 1
    print(f"splash_art_check: GREEN -- the splash art ({len(asset)} bytes) is the asset in {len(bundles)} bundles; 2 must-fail controls fired")
    return 0


if __name__ == "__main__":
    sys.exit(main())
