#!/usr/bin/env python3
"""The blade archive's off-toolchain check (B706, ADR-406 ruling 5; docs/archive/BLADE-ENGINE.md §5).

  python3 -I tools/blade-archive/compare.py <recorded dir> <fresh dir> --limit <dBFS>

When a rebuild's hashes differ from the recorded lists (a newer compiler or system library can move the last bits),
this is the check that remains: for every WAV in the recorded dir, the same name in the fresh dir, the largest absolute
sample difference over the whole file, in dBFS (full scale 1.0). It prints the worst file and exits 1 if any file is
missing, a different length or format, or worse than the limit.

The limit is always given by the caller, never defaulted here: it is a tolerance, and tolerances in this repo are
registered by the human (docs/armor/tolerances.json). The archive doc names the values: -120 dBFS, the engine's tier 1
(ADR-380 A1), for the bank's float renders; for the 16-bit demo renders, where one step is 1/32768 (-90.3 dBFS), -84
(two steps), the lead's reading, not a ruling. Reads 32-bit float (format 3) and 16-bit PCM (format 1) WAVs only, as
archive_render and preview-demo-render write them. Standard library only, so it runs on any later Python 3.
"""
import array
import math
import pathlib
import struct
import sys


def samples(path):
    """(format, rate, channels, samples as floats) from a WAV written by archive_render or preview-demo-render."""
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path.name}: not a WAV")
    fmt = rate = channels = bits = None
    pos = 12
    body = None
    while pos + 8 <= len(data):
        tag, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        chunk = data[pos + 8:pos + 8 + size]
        if tag == b"fmt ":
            fmt, channels, rate, _, _, bits = struct.unpack("<HHIIHH", chunk[:16])
        elif tag == b"data":
            body = chunk
        pos += 8 + size + (size & 1)
    if body is None or fmt is None:
        raise ValueError(f"{path.name}: no fmt or data chunk")
    if fmt == 3 and bits == 32:
        a = array.array("f")
        a.frombytes(body)
        return fmt, rate, channels, list(a)
    if fmt == 1 and bits == 16:
        a = array.array("h")
        a.frombytes(body)
        return fmt, rate, channels, [x / 32768.0 for x in a]
    raise ValueError(f"{path.name}: format {fmt} at {bits} bits is not one this check reads")


def db(x):
    return 20 * math.log10(x) if x > 0 else -math.inf


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    recorded, fresh = pathlib.Path(argv[1]), pathlib.Path(argv[2])
    if len(argv) != 5 or argv[3] != "--limit":
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    limit = float(argv[4])
    files = sorted(recorded.glob("*.wav"))
    if not files:
        print(f"compare: no WAVs in {recorded}", file=sys.stderr)
        return 2
    worst, worst_name, bad = -math.inf, "", []
    for f in files:
        g = fresh / f.name
        if not g.exists():
            bad.append(f"{f.name}: missing from {fresh}")
            continue
        fa, ra, ca, a = samples(f)
        fb, rb, cb, b = samples(g)
        if (fa, ra, ca, len(a)) != (fb, rb, cb, len(b)):
            bad.append(f"{f.name}: format, rate, channels or length differ")
            continue
        d = db(max((abs(x - y) for x, y in zip(a, b)), default=0.0))
        if d > worst:
            worst, worst_name = d, f.name
        if d > limit:
            bad.append(f"{f.name}: {d:.1f} dBFS")
    print(f"compare: {len(files)} files; the largest difference {worst:.1f} dBFS ({worst_name or 'none'}); limit {limit:.1f} dBFS")
    for line in bad[:20]:
        print("  RED " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
