#!/usr/bin/env python3
"""parity_floor_check -- the golden-parity gates cannot read green on a shrunken corpus.

WIRED: ./verify full

  python3 tools/parity_floor_check.py <abs-build-dir>      (verify full passes build-release)
  python3 tools/parity_floor_check.py --stream <abs-build-dir> <gate> <stream-file>
                                                           (verify full, once per stream gate)

WHY (B455 H1, docs/audits/2026-10-10-repo-audit.md; added under ADR-180 §1). parity_check,
filter_check, notch_check, spectra_check, swarmalator_check, time_check, station_check and
subosc_check loop over a manifest the golden generator writes and went red only when a
failure was COUNTED. An empty manifest, or a deleted scenario block, counted none and read
GREEN while the gate compared nothing. Each gate now pins a floor, the count it holds today:

  - seven gates carry theirs in their own source (kMinScenarios, via tools/scenario_floor.h);
  - parity_check and h2_swarm_parity_check carry theirs HERE (PARITY_FLOOR), because
    tools/parity_check.cpp is the pinned source of horde 2's lift (h2/cores/swarm/lift-ledger.json
    holds its git blob, and tools/h2_swarm_parity_check.cpp is rebuilt from it byte for byte), so
    editing either would turn h2_lift_check red and need an h2/ re-lift. The floor is read off the
    gate's own summary line ("N/M scenarios", M being the corpus size);
  - swarm48_check and h2_swarm48_check (one body, tools/swarm_sr_parity.h, over
    build-golden/sr48000) are floored HERE too (SWARM48_FLOOR), read off their summary line
    ("N scenarios at 48 kHz"). That body is shared, unlifted and unprotected, so an in-source
    floor was possible; the script keeps the floors that guard a verbatim pair in one place.

A count above a floor is reported, not failed: the floor ratchets up in the PR that adds
scenarios, and lowering it is a gate-weakening event.

MUST-FAIL CONTROLS (LIBRARY L0032; the style of h2_engine_parity_check's TRUNC), every run,
against symlinked scratch copies of the real corpus, so no golden is touched:
  POSITIVE  the unmodified corpus reads GREEN (a judge that is always red proves nothing);
  EMPTY     an empty manifest reads RED;
  DROP      the manifest with its LAST scenario removed reads RED.
A RED control must be red BECAUSE OF THE FLOOR: its output has to carry the floor's own words
("below the pinned floor"), so a gate that fell over for some other reason cannot pass as a
control (the detector must not share the assumption it tests). A control that reads the
wrong colour makes this check red.

THE TWO horde 2 STREAM GATES (--stream; B455 H1 follow-up, ADR-206 item 1). h2_engine_parity_check
and h2_scalpel_parity_check read a binary stream (tools/h2_*_stream.h), not a manifest, and carry
their floor in their own source (kMinScenarios via tools/scenario_floor.h). The header and END
counts they check agree with the rows read, so a stream with a scenario consistently dropped passed
until the floor existed. The stream takes 25-40 s to render, so ./verify renders it ONCE, runs the
gate on it (that run IS the POSITIVE control: the untouched stream, green), then calls --stream with
the same file. Controls here, run on scratch files derived from the real stream:
  DROP         the stream minus its LAST scenario, header and END rewritten to agree (the shape that
               passed before the floor) reads RED, and the floor's line is the ONLY FAIL line, naming
               N-1 scenarios and a floor of N (so nothing else turned it red);
  HEADER-ONLY  a header and an END 0, no scenario, reads RED by the floor;
  EMPTY        a zero-byte file reads RED;
  ABSENT       a path that does not exist reads RED.
The stream is walked exactly as the gate's own reader does (LIBM: n*3 float64; DATA: frames*2 float64
closes a scenario), not guessed.
"""
import os
import re
import subprocess
import sys
import tempfile

PARITY_FLOOR = 156   # scenarios in build-golden/manifest.tsv today (gen_goldens.mjs)
SWARM48_FLOOR = 171  # scenarios in build-golden/sr48000/manifest.tsv today (gen_goldens_sr.mjs)

# gate -> (golden dir relative to build-golden, manifest file name)
GATES = {
    "parity_check": (".", "manifest.tsv"),
    "filter_check": ("filter", "filter-manifest.tsv"),
    "notch_check": ("notch", "notch-manifest.tsv"),
    "spectra_check": ("spectra", "spectra-manifest.tsv"),
    "swarmalator_check": ("swarmalator", "swarmalator-manifest.tsv"),
    "time_check": ("time", "time-manifest.tsv"),
    "station_check": ("station", "station-manifest.tsv"),
    "subosc_check": ("subosc", "subosc-manifest.tsv"),
    "h2_swarm_parity_check": (".", "manifest.tsv"),
    "swarm48_check": ("sr48000", "manifest.tsv"),
    "h2_swarm48_check": ("sr48000", "manifest.tsv"),
}
# Gates whose floor is judged HERE: gate -> (regex whose group 1 is the scenario count, floor).
# Every other gate in GATES judges its own floor (tools/scenario_floor.h).
_PARITY_LINE = re.compile(r"parity_check: \d+/(\d+) scenarios")
SCRIPT_FLOORS = {
    "parity_check": (_PARITY_LINE, PARITY_FLOOR),
    "h2_swarm_parity_check": (_PARITY_LINE, PARITY_FLOOR),   # verbatim copy: prints "parity_check:"
    "swarm48_check": (re.compile(r"swarm48_check: (\d+) scenarios at 48 kHz"), SWARM48_FLOOR),
    "h2_swarm48_check": (re.compile(r"h2_swarm48_check: (\d+) scenarios at 48 kHz"), SWARM48_FLOOR),
}
FLOOR_WORDS = "below the pinned floor"


def run(binary, golden):
    p = subprocess.run([binary, golden], capture_output=True, text=True, timeout=300)
    return p.returncode, p.stdout + p.stderr


def verdict(gate, binary, golden):
    """(red, output). The script-floored gates are judged here; the others judge their own."""
    rc, out = run(binary, golden)
    if gate not in SCRIPT_FLOORS:
        return rc != 0, out
    pattern, floor = SCRIPT_FLOORS[gate]
    m = pattern.search(out)
    n = int(m.group(1)) if m else 0     # no summary line (a missing manifest) counts zero
    if n < floor:
        out += f"{gate}: {n} scenarios, {FLOOR_WORDS} of {floor}\n"
        return True, out
    if n > floor:
        print(f"NOTE  {gate}: {n} scenarios (floor {floor}): raise its floor in this script in the same PR")
    return rc != 0, out


def scratch_copy(src, dst, manifest, lines):
    """src's files symlinked into dst, with `manifest` replaced by `lines`."""
    for name in os.listdir(src):
        path = os.path.join(src, name)
        if name != manifest and os.path.isfile(path):
            os.symlink(path, os.path.join(dst, name))
    with open(os.path.join(dst, manifest), "w", encoding="utf-8") as f:
        f.writelines(lines)


def scenario_lines(manifest_path):
    with open(manifest_path, encoding="utf-8") as f:
        return f.readlines()


def drop_last_scenario(lines):
    """`lines` minus the last line that is a scenario (not blank, not a '#' comment)."""
    for i in range(len(lines) - 1, -1, -1):
        if lines[i].strip() and not lines[i].startswith("#"):
            return lines[:i] + lines[i + 1:]
    return lines


# gate -> (the stream's magic word, the flag that declares a file the full render)
STREAM_GATES = {
    "h2_engine_parity_check": (b"H2ENGINE", "--full-from"),
    "h2_scalpel_parity_check": (b"H2SCALPEL", "--full-from"),
}


def walk_stream(data, magic):
    """(header_line, [(start, end) byte range of each SCN block], offset of the END line).

    Mirrors tools/h2_engine_stream.h / h2_scalpel_stream.h: text lines, with LIBM carrying n*3 float64
    after its line and DATA carrying frames*2 float64 (closing the scenario)."""
    nl = data.index(b"\n")
    header = data[:nl]
    if not header.startswith(magic + b" 1 "):
        raise ValueError(f"header {header!r} is not {magic.decode()} 1 <n>")
    pos, start, scns = nl + 1, None, []
    while True:
        nl = data.index(b"\n", pos)
        words = data[pos:nl].split(b" ")
        op = words[0]
        if op == b"END":
            return header, scns, pos
        if op == b"SCN":
            start = pos
        if op == b"LIBM":
            nxt = nl + 1 + int(words[2]) * 24
        elif op == b"DATA":
            nxt = nl + 1 + int(words[1]) * 16
            if start is None:
                raise ValueError("DATA outside a scenario")
            scns.append((start, nxt))
            start = None
        else:
            nxt = nl + 1
        pos = nxt


def restream(magic, count, body):
    """A self-consistent stream: header and END both say `count`."""
    n = str(count).encode()
    return magic + b" 1 " + n + b"\n" + body + b"END " + n + b"\n"


def run_stream(binary, flag, path):
    p = subprocess.run([binary, flag, path], capture_output=True, text=True, timeout=300)
    return p.returncode, p.stdout + p.stderr


def stream_main(build, gate, stream):
    """The must-fail controls for one stream gate; the untouched stream's green run is verify's own."""
    binary = os.path.join(build, gate)
    magic, flag = STREAM_GATES[gate]
    if not (os.path.isfile(binary) and os.path.isfile(stream)):
        print(f"FAIL  {gate}: binary or stream missing")
        return 1
    with open(stream, "rb") as f:
        data = f.read()
    header, scns, end_at = walk_stream(data, magic)
    n = len(scns)
    if header.split(b" ")[2] != str(n).encode() or data[end_at:] != b"END " + str(n).encode() + b"\n":
        print(f"FAIL  {gate}: the real stream is not self-consistent (header {header!r}, {n} scenarios, "
              f"tail {data[end_at:]!r}); the DROP control would prove nothing")
        return 1
    body_at = data.index(b"\n") + 1
    # (label, file content or None for a path that does not exist, red by the floor?, scenarios it holds)
    controls = (
        ("DROP        the stream minus its last scenario, header and END rewritten to agree",
         restream(magic, n - 1, data[body_at:scns[-1][0]]), True, n - 1),
        ("HEADER-ONLY a header and an END 0, no scenario", restream(magic, 0, b""), True, 0),
        ("EMPTY       a zero-byte file", b"", False, None),
        ("ABSENT      a path that does not exist", None, False, None),
    )
    fails = []
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "stream.bin")
        for label, content, by_floor, held in controls:
            if content is not None:
                with open(path, "wb") as f:
                    f.write(content)
            elif os.path.exists(path):
                os.remove(path)
            rc, out = run_stream(binary, flag, path)
            floor = re.search(r"FAIL \S+: (\d+) scenarios, " + re.escape(FLOOR_WORDS) + r" of (\d+)", out)
            fail_lines = [ln for ln in out.splitlines() if ln.startswith("FAIL")]
            ok = rc != 0
            why = f"exit {rc}"
            if by_floor:
                # red BECAUSE OF THE FLOOR, naming both numbers (held scenarios, the pinned floor = today's count)
                ok = ok and floor is not None and int(floor.group(1)) == held and int(floor.group(2)) == n
                why += ", " + (floor.group(0) if floor else "NO floor line")
                if held:   # a stream with rows: nothing but the floor may be red
                    ok = ok and len(fail_lines) == 1 and FLOOR_WORDS in fail_lines[0]
                    if len(fail_lines) != 1:
                        why += f", {len(fail_lines)} FAIL lines (want only the floor's)"
            print(f"{'PASS' if ok else 'FAIL'}  {gate:<24} {label} (read {'RED' if rc else 'GREEN'}; {why})")
            if not ok:
                fails.append(f"{gate}: {label.split()[0]}")
                print(out[-2000:], file=sys.stderr)
    if fails:
        print("parity_floor_check --stream: RED -- " + "; ".join(fails), file=sys.stderr)
        return 1
    print(f"parity_floor_check --stream: GREEN ({gate}: {n} scenarios; {len(controls)} controls red; "
          f"the untouched stream is the gate's own green run)")
    return 0


def main():
    if len(sys.argv) == 5 and sys.argv[1] == "--stream" and sys.argv[3] in STREAM_GATES:
        return stream_main(sys.argv[2], sys.argv[3], sys.argv[4])
    if len(sys.argv) != 2:
        print("usage: parity_floor_check.py <abs-build-dir> | --stream <abs-build-dir> <gate> <stream-file>",
              file=sys.stderr)
        return 64
    build = sys.argv[1]
    golden_root = os.path.join(os.path.dirname(build.rstrip("/")), "build-golden")
    fails = []
    for gate, (sub, manifest) in GATES.items():
        binary = os.path.join(build, gate)
        src = os.path.normpath(os.path.join(golden_root, sub))
        real = os.path.join(src, manifest)
        if not (os.path.isfile(binary) and os.path.isfile(real)):
            fails.append(f"{gate}: missing {binary if not os.path.isfile(binary) else real}")
            print(f"FAIL  {gate}: binary or manifest missing")
            continue
        lines = scenario_lines(real)
        for label, want_red, body in (
            ("POSITIVE the untouched corpus reads GREEN", False, lines),
            ("EMPTY    an empty manifest reads RED", True, []),
            ("DROP     the manifest minus its last scenario reads RED", True, drop_last_scenario(lines)),
        ):
            with tempfile.TemporaryDirectory() as tmp:
                scratch_copy(src, tmp, manifest, body)
                red, out = verdict(gate, binary, tmp)
            because_floor = FLOOR_WORDS in out
            ok = (red == want_red) and (because_floor if want_red else True)
            tail = out.strip().splitlines()[-1] if out.strip() else "(no output)"
            colour = "RED" if red else "GREEN"
            print(f"{'PASS' if ok else 'FAIL'}  {gate:<18} {label} (read {colour}"
                  f"{', by the floor' if because_floor and red else ''}): {tail}")
            if not ok:
                fails.append(f"{gate}: {label.split()[0]} read {colour}"
                             f"{'' if because_floor or not want_red else ' but NOT by the floor'}")
    if fails:
        print("parity_floor_check: RED -- " + "; ".join(fails), file=sys.stderr)
        return 1
    print(f"parity_floor_check: GREEN ({len(GATES)} gates x 3 controls; {len(SCRIPT_FLOORS)} floored "
          f"here, {len(GATES) - len(SCRIPT_FLOORS)} in their own source)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
