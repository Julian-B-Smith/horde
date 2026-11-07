#!/usr/bin/env python3
"""gen_random_roles_data -- the parameter snapshot the randomization roles lab reads (B644).

Origin: horde lead session, 2026-11-07, B644. The human asked for a lab listing every engine
parameter, nested by dependency, where they can say how Random treats each one.

WHAT IT WRITES. One JSON object, embedded in docs/design/random-roles-lab.html between the
`RANDOM-ROLES-DATA` markers, so the page needs no server logic and no fetch. One row per line on
purpose: tools/engine_words_check.py reads line by line, and a one-line dump would put the Detune
law row beside the Spread law's "swarm" option, which reads as a selection it is not.

WHAT IT READS, and why each one.
  docs/design/gui3/params.json       the manifest rows (SPEC-ENGINE section 5 as gen_gui3_controls
                                     reads it): key, class, rate, law, range, options.
  docs/design/gui3/layout.json       `live_when`: PA-8's live-when table, machine-readable, with
                                     {o} and {b} placeholders. It is the only machine-readable copy;
                                     the spec's table is prose.
  specs/SPEC-ENGINE.md               D.2's morph settings (context rows, never drawn), and the PA-8
                                     rows layout.json does not transcribe, which are written below
                                     as SPEC_RULES with their wording quoted, so the gap is visible.
  engine2/structure/engine-structure.json   only its hash, so the page can say which tree it
                                     was cut from; its blocks are coarser than the manifest's.

THE NESTING. Each row gets a home (a source, a blade, a section of an oscillator) and, inside it,
a parent: the key its live-when condition hangs on. For an `all` condition the last clause in the
same block wins (the most specific: `shape` hangs on `wave`, not on `mode`); for `any` the first
(`width` hangs on `unison`, not on `panMotion`, which would be a cycle). A candidate outside the
row's home is ignored, so a blade spread that is live only at U > 1 sits under its blade, with
"U > 1" in its condition text, not under the oscillator's unison row.

Deterministic: no clock, sorted where order is not the manifest's. `--check` exits 1 when the
embedded block differs from what this would write (not wired into ./verify: a lab's snapshot
going stale is a fact the page states, by hash, not a failure).
"""
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PARAMS = ROOT / "docs/design/gui3/params.json"
LAYOUT = ROOT / "docs/design/gui3/layout.json"
SPEC = ROOT / "specs/SPEC-ENGINE.md"
STRUCT = ROOT / "engine2/structure/engine-structure.json"
LAB = ROOT / "docs/design/random-roles-lab.html"
BEGIN, END = "/* RANDOM-ROLES-DATA:BEGIN */", "/* RANDOM-ROLES-DATA:END */"

OSCS, BLADES = ("osc1", "osc2"), ("blade1", "blade2")
CARRIER = ["Sync", "FM reset", "FM free", "Ring"]

# Oscillator sections: which suffix lives where. Anything unlisted lands in "other", which the
# page's self-check reports, so a new spec row shows up instead of vanishing.
OSC_SECTIONS = [
    ("pitch", "Pitch", ["octave", "semitone", "pitch", "fine", "pitchMode"]),
    ("out", "Level, place and envelope", ["level", "pan", "mute", "solo", "env", "stream"]),
    ("swarm", "Swarm", ["unison", "seed", "distribution", "detuneLaw", "detune", "detuneRange", "harmonicReach",
                        "anchor", "coupling", "phaseLag", "pivot", "onsetLock", "dissolve", "driftDepth", "driftRate",
                        "inertia", "phase", "phaseRandom", "retrigger", "onsetScatter", "timingCorrection", "width",
                        "panImage", "panInvert", "panMotion", "bandLimit", "motionGrain", "baseWave", "spreadLaw",
                        "spreadDrift", "quantize"]),
    ("blades", "Blades", []),
]
CARRIER_LOOP = ["crossMod", "feedback", "loopDrive"]
BOTH_BLADES = ["stacking", "upperHears", "collisionPitch", "collisionBite"]
SUB_SECTIONS = [
    ("shape", "Shape", ["wave", "pulseWidth", "softness", "bumpHarmonic", "bumpAmount", "bumpPhase",
                        "bump2Harmonic", "bump2Amount", "bump2Phase"]),
    ("pitch", "Pitch", ["octave", "semitones", "pitch", "fine", "keytrack", "startPhase", "pitchMode"]),
    ("drive", "Drive and erosion", ["order", "erode", "drive", "crunch", "character", "tone"]),
    ("voice", "Voicing", ["voiceMode", "priority"]),
    ("out", "Level, place and envelope", ["level", "pan", "mute", "solo", "env", "stream"]),
]
NOISE_SECTIONS = [
    ("source", "Source", ["type", "density", "rate", "length", "lfsrKeytrack", "colour", "pitch", "mode",
                          "sampleLoop", "sampleKeytrack", "sampleStart", "sampleRandom"]),
    ("stages", "Stages", ["resonator", "feedback", "whisper", "vowel", "wind", "gust"]),
    ("filter", "Filter", ["filter", "cutoff", "resonance", "keytrack"]),
    ("out", "Level, place and envelope", ["level", "spread", "pan", "mute", "solo", "env", "stream", "pitchMode"]),
]
ENV_NAMES = {
    "env1": "ENV 1, the amp envelope every source follows by default",
    "env2": "ENV 2",
    "env3": "ENV 3",
    "env4": "ENV 4, the pitch envelope by convention (ADR-374 r5, GUI-55 r5)",
}

# PA-8 rows that layout.json's live_when does not carry, quoted from SPEC-ENGINE's table.
SPEC_RULES = {
    "blade": "every blade row: that blade is on (PA-8)",
    "both": "upperHears, stacking, collisionPitch, collisionBite: both blades are on (PA-8)",
    "source": "a parameter not listed is live whenever its source is on (PA-8)",
    "noise": "every noise row but spread, pan, mute, solo and env: the noise is on, or the sub is on with erode > 0 (PA-8, SN-14)",
    "env": "every env row: an on source follows that envelope (PA-8)",
}


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()[:16]


def expand(obj, o, b):
    s = json.dumps(obj, ensure_ascii=False).replace("{o}", o[3:]).replace("{b}", b[5:] if b else "{b}")
    return json.loads(s)


def live_when_map(layout, keys):
    """key -> structured condition, from layout.json's live_when, placeholders expanded."""
    out = {}
    for row in layout["live_when"]["rows"]:
        for pat in row["keys"]:
            combos = [(o, b) for o in OSCS for b in BLADES] if "{b}" in pat else \
                     [(o, None) for o in OSCS] if "{o}" in pat else [(None, None)]
            for o, b in combos:
                k = pat
                if o:
                    k = k.replace("{o}", o[3:])
                if b:
                    k = k.replace("{b}", b[5:])
                if k not in keys:
                    continue      # a morph row (D.2), or a pattern that does not apply to this block
                cond = row["when"]
                if o:
                    cond = expand(cond, o, b)
                if k in out:
                    out[k] = {"all": [out[k], cond]}
                else:
                    out[k] = cond
    return out


def candidate(cond):
    """the key a condition hangs on: last clause of `all`, first of `any`."""
    if "key" in cond:
        return [cond["key"]]
    if "all" in cond:
        return [k for c in reversed(cond["all"]) for k in candidate(c)]
    if "any" in cond:
        return [k for c in cond["any"] for k in candidate(c)]
    return []


def main():
    params = json.loads(PARAMS.read_text(encoding="utf-8"))
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    rows = params["params"]
    keys = {r["key"] for r in rows}
    by = {r["key"]: r for r in rows}
    lw = live_when_map(layout, keys)

    nodes, home = [], {}

    def group(gid, label, parent, when=None, cond=None, note=None):
        nodes.append({"id": gid, "kind": "group", "label": label, "parent": parent, "when": when, "cond": cond, "note": note})

    def on(k):
        return {"key": k, "is": ["on"]}

    # ---- homes ------------------------------------------------------------------------------------
    group("g:voice", "Voice and output", None, note="Whole-instrument rows: no source owns them.")
    for k in ("voice.mode", "voice.polyphony", "voice.gravity", "quality", "out.level"):
        home[k] = "g:voice"

    for o in OSCS:
        n = o[3:]
        group("g:" + o, "Osc " + n, None)
        home[o + ".on"] = "g:" + o
        for sid, label, sufs in OSC_SECTIONS:
            group("g:%s.%s" % (o, sid), label, o + ".on", when="Osc %s is on" % n, cond=on(o + ".on"))
            for s in sufs:
                k = "mix." + o if s == "level" else o + "." + s
                if k in keys:
                    home[k] = "g:%s.%s" % (o, sid)
        bl = "g:%s.blades" % o
        group(bl + ".carrier", "Carrier loop", bl, when="a blade is on in Sync, FM or Ring (PA-8)",
              cond={"any": [{"all": [on("%s.%s.on" % (o, b)), {"key": "%s.%s.mode" % (o, b), "is": CARRIER}]} for b in BLADES]})
        group(bl + ".both", "Both blades", bl, when=SPEC_RULES["both"],
              cond={"all": [on("%s.%s.on" % (o, b)) for b in BLADES]})
        for s in CARRIER_LOOP:
            home["%s.%s" % (o, s)] = bl + ".carrier"
        for s in BOTH_BLADES:
            home["%s.%s" % (o, s)] = bl + ".both"
        for b in BLADES:
            bk = "%s.%s" % (o, b)
            home[bk + ".on"] = bl
            for r in rows:
                if r["block"] == bk and r["key"] != bk + ".on":
                    home[r["key"]] = bk + ".on"

    group("g:sub", "Sub", None)
    home["sub.on"] = "g:sub"
    for sid, label, sufs in SUB_SECTIONS:
        group("g:sub." + sid, label, "sub.on", when="the sub is on", cond=on("sub.on"))
        for s in sufs:
            k = "mix.sub" if s == "level" else "sub." + s
            if k in keys:
                home[k] = "g:sub." + sid

    group("g:noise", "Noise", None)
    home["noise.on"] = "g:noise"
    noise_live = {"any": [on("noise.on"), {"all": [on("sub.on"), {"key": "sub.erode", "gt": 0}]}]}
    for sid, label, sufs in NOISE_SECTIONS:
        out_sec = sid == "out"
        group("g:noise." + sid, label, "noise.on",
              when="the noise is on" if out_sec else "the noise is on, or the sub is on with erode > 0 (PA-8, SN-14)",
              cond=on("noise.on") if out_sec else noise_live)
        for s in sufs:
            k = "mix.noise" if s == "level" else "noise." + s
            if k in keys:
                home[k] = "g:noise." + sid

    group("g:env", "Envelopes", None, note="ENV 1 is every source's default; a source's `env` row picks another (RC-6.5).")
    srcs = ["osc1", "osc2", "sub", "noise"]
    for e in ("env1", "env2", "env3", "env4"):
        idx = "ENV " + e[3:]
        group("g:" + e, ENV_NAMES[e], "g:env", when=SPEC_RULES["env"],
              cond={"any": [{"all": [on(s + ".on"), {"key": s + ".env", "is": [idx]}]} for s in srcs]})
        for r in rows:
            if r["block"] == e:
                home[r["key"]] = "g:" + e

    # ---- parents inside each home ------------------------------------------------------------------
    rowsout = []
    for r in rows:
        k = r["key"]
        h = home.get(k, "g:other")
        parent = h
        cond = lw.get(k)
        if cond:
            for c in candidate(cond):
                if c != k and c in keys and home.get(c) == h:
                    parent = c
                    break
                if c != k and c in keys and c == h:   # the home row itself (a blade's or source's on switch)
                    parent = c
                    break
        # the structured condition the preview evaluates: live_when, plus the PA-8 rows it lacks
        full = [cond] if cond else []
        blk = r["block"]
        m = re.fullmatch(r"(osc\d)\.(blade\d)", blk)
        if m and not k.endswith(".on"):
            full.insert(0, on(blk + ".on"))
        src = blk.split(".")[0]
        if k.startswith("mix."):
            src = k.split(".")[1]
        if src in srcs and not k.endswith(src + ".on"):
            if src == "noise" and k.split(".")[-1] not in ("spread", "pan", "mute", "solo", "env") and not k.startswith("mix."):
                full.insert(0, noise_live)
            elif not (cond and k.startswith("mix.")):
                full.insert(0, on(src + ".on"))
        rr = {f: r.get(f) for f in ("key", "label", "block", "class", "rate", "law", "n", "options", "range_text", "unit",
                                    "lo", "hi", "default_v", "default_index", "default_text", "reserved", "advanced",
                                    "host", "see", "section", "spec") if f in r}
        rr["home"] = h
        rr["parent"] = parent
        rr["live_when"] = cond
        rr["cond"] = ({"all": full} if len(full) > 1 else full[0]) if full else None
        rowsout.append(rr)

    # ---- D.2: the morph's settings, context only ----------------------------------------------------
    spec = SPEC.read_text(encoding="utf-8")
    d2 = spec.split("### D.2 The morph settings", 1)[1].split("\n### ", 1)[0]
    morph = []
    for line in d2.split("\n"):
        mm = re.match(r"\|\s*`(morph\.[A-Za-z]+)`\s*\|([^|]*)\|([^|]*)\|([^|]*)\|(.*)\|\s*$", line)
        if mm:
            morph.append({"key": mm.group(1), "label": mm.group(2).strip(), "range_text": mm.group(3).strip(),
                          "default_text": mm.group(4).strip(), "spec": mm.group(5).strip()})

    data = {
        "format": "horde.random-roles-data/1",
        "generator": "tools/gen_random_roles_data.py",
        "sources": {
            "params": {"path": "docs/design/gui3/params.json", "sha256_16": sha(PARAMS), "format": params["format"]},
            "live_when": {"path": "docs/design/gui3/layout.json#live_when", "sha256_16": sha(LAYOUT)},
            "spec": {"path": "specs/SPEC-ENGINE.md (PA-8, D.2)", "sha256_16": sha(SPEC)},
            "structure": {"path": "engine2/structure/engine-structure.json", "sha256_16": sha(STRUCT)},
        },
        "spec_rules": SPEC_RULES,
        "manifest_count": len(rows),
        "groups": [n for n in nodes],
        "rows": rowsout,
        "morph": morph,
    }
    # one entry per line (see the docstring: engine_words_check reads lines)
    parts = ['{"format": %s,' % json.dumps(data["format"])]
    for f in ("generator", "sources", "spec_rules", "manifest_count"):
        parts.append(" %s: %s," % (json.dumps(f), json.dumps(data[f], ensure_ascii=False, sort_keys=True)))
    for f in ("groups", "rows", "morph"):
        parts.append(" %s: [" % json.dumps(f))
        items = data[f]
        for i, it in enumerate(items):
            parts.append("  " + json.dumps(it, ensure_ascii=False, sort_keys=True) + ("," if i < len(items) - 1 else ""))
        parts.append(" ]" + ("," if f != "morph" else ""))
    parts.append("}")
    block = BEGIN + "\nconst DATA = " + "\n".join(parts) + ";\n" + END

    html = LAB.read_text(encoding="utf-8")
    if BEGIN not in html or END not in html:
        sys.exit("gen_random_roles_data: the lab has no %s ... %s block" % (BEGIN, END))
    new = html.split(BEGIN, 1)[0] + block + html.split(END, 1)[1]
    if "--check" in sys.argv:
        if new != html:
            print("gen_random_roles_data: the lab's snapshot is stale; run python3 tools/gen_random_roles_data.py")
            return 1
        print("gen_random_roles_data: snapshot current (%d rows)" % len(rows))
        return 0
    LAB.write_text(new, encoding="utf-8")
    print("gen_random_roles_data: wrote %d manifest rows, %d groups, %d morph context rows into %s"
          % (len(rows), len(nodes), len(morph), LAB.relative_to(ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
