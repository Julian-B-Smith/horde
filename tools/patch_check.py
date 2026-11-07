#!/usr/bin/env python3
"""patch_check -- validate a dictated horde patch against the engine's parameter table (B629).

WIRED: ./verify fast (as `--selftest`, its own must-fail controls); tools/agent_manual_check.py, also in ./verify fast, runs it on every recipe.

WHY. An agent turns a musician's words into a patch (docs/manual/agent/). A patch that names a key the
engine lacks, or a value outside its row, is refused by the engine one key at a time (PA-7) and the
sound is silently not what was asked for. A row that PA-8 makes inert does nothing, so a patch that
leans on one is wrong in a way no error shows. This tool says both before the patch reaches the engine.

THE FORMAT, horde-patch/1:
    { "format": "horde-patch/1", "base": "<a bank preset name, or 'init'>",
      "values": { "<key>": <value>, ... } }
  * a continuous row takes x in its range, in the numbers the range column states (a '%' row whose range
    is 0..1 takes 0..1); a dB row with a -inf bottom also takes the string "-inf";
  * a step row with named options takes the option's name (a string) or its 0-based index (an integer);
  * a step row whose options are numbers (octave, semitone, unison, polyphony, the bump harmonics,
    the noise's length) takes the number itself, never an index; `seed` takes its integer.
  Option names match exactly, or in ASCII ("x1" for "×1", "sine->saw", "-1" for "−1").

THE ONE SOURCE. Rows come from docs/design/gui3/params.json, which tools/gen_gui3_controls.py
generates from SPEC-ENGINE section 5 (BD-5). Nothing here lists a key, a range or a default.

TWO READINGS OF THE SPEC THAT ARE CODE HERE, EACH PINNED TO THE TEXT IT READS:
  * PA-8's live-when table (SPEC-ENGINE 6.6) as predicates, one per table row, keyed by the row's
    parameter cell and a hash of its condition cell. tools/agent_manual_check.py is red when the
    spec's table and this list differ in any row, so an edit to PA-8 cannot leave a stale predicate.
  * Appendix B.1's translation of a bank preset (engine2/data/bank/presets.json), used when `base`
    names a preset. Its base values are parsed from B.1's own sentence; the key map and the
    enumerations are written out below. B1_SHA pins the section's text: an edit to B.1 turns the
    check red until this translator is re-read against it.
  Every other fact (keys, ranges, laws, options) is read at run time.

WHAT IT CANNOT SEE. PA-8 says when a row CAN move the output, not whether it moves it audibly; a
live row may still be too small to hear. The bank's blade-envelope routes (B.1) belong to the
modulation system and have no place in horde-patch/1. OU-2's loudness move of a bank preset's level
needs renders, so a bank base takes B.1's level formula only.

Usage: python3 tools/patch_check.py PATCH.json            validate; exit 0 valid, 1 invalid
       python3 tools/patch_check.py PATCH.json --set      also print the page's `set` message, in v
       python3 tools/patch_check.py PATCH.json --resolve  also print every setting that differs from init
       python3 tools/patch_check.py --selftest            this tool's own controls (./verify fast runs it)
"""
import hashlib
import json
import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from gen_gui3_controls import inverse  # the laws' one inversion, shared with GUI 3's manifest; after the path insert

PARAMS = ROOT / "docs/design/gui3/params.json"
SPEC = ROOT / "specs/SPEC-ENGINE.md"
BANK = ROOT / "engine2/data/bank/presets.json"
FORMAT = "horde-patch/1"
SOURCES = ("osc1", "osc2", "sub", "noise")
CARRIER = ("Sync", "FM reset", "FM free", "Ring")       # BL-2's carrier modes; PA-8's "Sync, FM or Ring"
FM = ("FM reset", "FM free")
SPREADS = ("positionSpread", "cutSpread", "widthSpread", "depthSpread", "shapeSpread", "fmIndexSpread",
           "rotateSpread")
FOLLOW = ("as blade 1", "blade 1's", "with blade 1")    # blade 2's options that read blade 1 (5.1, BL-9)
OFF_ZONE = 0.004   # 5.1's `log, off zone` law: a blade width below this is off (and PA-8's `edges` row reads it)


# ---------------------------------------------------------------- the table

def load_rows(path=PARAMS):
    rows = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))["params"]
    return {r["key"]: r for r in rows}


def ascii_form(s):
    """An option name as an agent may type it: lower case, ASCII arrows, signs and times."""
    s = s.replace("−", "-").replace("×", "x").replace("→", "->").replace(" ", " ")
    return re.sub(r"\s+", " ", s).strip().lower()


def option_number(s):
    """An option's numeric value, or None. '2 047' -> 2047, '+1' -> 1, '−3' -> -3."""
    t = s.replace("−", "-").replace(" ", "").replace("+", "")
    try:
        return float(t)
    except ValueError:
        return None


def numeric_options(row):
    opts = row.get("options")
    return bool(opts) and all(option_number(o) is not None for o in opts)


def init_value(row):
    """A row's default in canonical form: the option name for a step row with options, the integer for
    `seed`, the number for a continuous row."""
    if row["law"] == "step":
        return row["options"][row["default_index"]] if row["options"] else int(row["default_index"])
    return row["default_x"]


def init_values(rows):
    return {k: init_value(r) for k, r in rows.items()}


def parse_value(row, val):
    """(canonical value, error or None, warning or None) for a patch value against its row."""
    key = row["key"]
    if isinstance(val, bool):
        return None, f"{key}: true/false is not a value; give the option's name (e.g. \"on\")", None
    if row["law"] == "step" and row["options"]:
        opts = row["options"]
        if numeric_options(row):
            want = option_number(str(val)) if isinstance(val, str) else (float(val) if isinstance(val, (int, float)) else None)
            for o in opts:
                if want is not None and option_number(o) == want:
                    return o, None, None
            return None, f"{key}: {val!r} is not one of its options {opts} (give the number itself)", None
        if isinstance(val, int):
            if 0 <= val < len(opts):
                return opts[val], None, None
            return None, f"{key}: index {val} is outside 0..{len(opts) - 1} ({opts})", None
        if isinstance(val, str):
            for o in opts:
                if val == o or ascii_form(val) == ascii_form(o):
                    return o, None, None
        return None, f"{key}: {val!r} is not one of its options {opts}", None
    if row["law"] == "step":                       # `seed`: an integer range, no option list
        if isinstance(val, (int, float)) and float(val).is_integer() and row["lo"] <= val <= row["hi"]:
            return int(val), None, None
        return None, f"{key}: {val!r} is not an integer in {row['range_text']}", None
    if isinstance(val, str) and row.get("neg_inf") and ascii_form(val) in ("-inf", "-infinity"):
        return -math.inf, None, None
    if not isinstance(val, (int, float)) or not math.isfinite(val):
        return None, f"{key}: {val!r} is not a finite number", None
    if not (row["lo"] <= val <= row["hi"]):
        return None, f"{key}: {val} is outside its range {row['range_text']} ({row['unit'] or 'no unit'})", None
    warn = None
    if row["law"] == "log, off zone" and 0 < val < OFF_ZONE:
        warn = f"{key}: {val} lies in the off zone (below {OFF_ZONE} is off, 5.1); it is sent as off"
    if row["law"] == "dB" and val == row["lo"]:
        warn = f"{key}: {val} dB is the law's v = 0, which is silence (5.1), not {val} dB"
    return float(val), None, warn


def to_v(row, x):
    """The normalised form v (5.1, IF-11) of a canonical value."""
    if row["law"] == "step":
        n = row["n"]
        i = row["options"].index(x) if row["options"] else int(x) - int(row["lo"])
        return 0.0 if n <= 1 else i / (n - 1)
    if row["law"] == "log, off zone" and x < OFF_ZONE:
        return 0.0
    if row["law"] == "dB" and (x == -math.inf or x <= row["lo"]):
        return 0.0
    return inverse(row["law"], row["lo"], row["hi"], x)


# ---------------------------------------------------------------- PA-8, read from the spec

PA8_HEAD = "**Live when** (PA-8;"


def read_pa8(spec_text):
    """PA-8's live-when table as [(parameter cell, condition cell)], and its preamble line."""
    i = spec_text.index(PA8_HEAD)
    pre = spec_text[i:spec_text.index("\n", i)]
    rows = []
    for line in spec_text[i:].splitlines()[1:]:
        if not line.startswith("|"):
            if rows:
                break
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cells[0] in ("parameter(s)", "---"):
            continue
        rows.append((cells[0], " | ".join(cells[1:])))
    return pre, rows


def cond_hash(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:10]


def split_key(key):
    """(source, blade, name): 'osc1.blade2.cut' -> ('osc1', 'blade2', 'cut'); 'mix.sub' -> ('mix', None, 'sub')."""
    parts = key.split(".")
    if len(parts) == 3:
        return parts[0], parts[1], parts[2]
    if len(parts) == 2:
        return parts[0], None, parts[1]
    return None, None, parts[0]


def source_of(key):
    """The source a row belongs to (PA-8's "its source"), or None for the voice, envelopes and output."""
    src, _, name = split_key(key)
    if src in SOURCES:
        return src
    if src == "mix" and name in SOURCES:
        return name
    return None


def _blade(key):
    src, blade, _ = split_key(key)
    return (src, blade) if src in ("osc1", "osc2") and blade in ("blade1", "blade2") else None


def _eff(V, key):
    """A blade 2 option that follows blade 1 reads blade 1's value (PA-8's first row, BL-9)."""
    v = V[key]
    if v in FOLLOW and ".blade2." in key:
        return V[key.replace(".blade2.", ".blade1.")]
    return v


def _bset(key, name):
    src, blade = _blade(key)
    return f"{src}.{blade}.{name}"


def _osc(key):
    return split_key(key)[0]


def _U(V, key):
    return int(option_number(V[f"{_osc(key)}.unison"]))


def _on(V, k):
    return V[k] == "on"


def _carrier_on(V, osc):
    return any(_on(V, f"{osc}.{b}.on") and V[f"{osc}.{b}.mode"] in CARRIER for b in ("blade1", "blade2"))


def _own(V, key, switch):
    return ".blade1." in key or V[_bset(key, switch)] == "its own"


def _followers(V, env):
    return any(_on(V, f"{s}.on") and V[f"{s}.env"] == env for s in SOURCES)


def _is(*names):
    return lambda k: split_key(k)[2] in names


def _osc_row(*names):
    return lambda k: split_key(k)[0] in ("osc1", "osc2") and split_key(k)[1] is None and split_key(k)[2] in names


def _blade_row(*names):
    return lambda k: _blade(k) is not None and split_key(k)[2] in names


def _src_row(src, *names):
    return lambda k: split_key(k)[0] == src and split_key(k)[2] in names


NOISE_ALWAYS_SOURCE = ("spread", "pan", "mute", "solo", "env")

# One entry per PA-8 row: (parameter cell, hash of its condition cell, scope, matches(key), live(V, key)).
# scope "section" rows are the blanket ones the reference states once per section; live is None where
# the row names no manifest parameter (`ratios` is oscillator state, 5.3). A predicate reads PA-8's own
# words and nothing more: where PA-8 is silent, the row is live.
PA8 = [
    ("every blade row", "d2941c6bc7", "section", lambda k: _blade(k) is not None,
     lambda V, k: _on(V, _bset(k, "on"))),
    ("a blade's `wave`", "fed477e8fe", "row", _blade_row("wave"),
     lambda V, k: V[_bset(k, "mode")] in CARRIER),
    ("`cut`", "a71fb900bc", "row", _blade_row("cut"),
     lambda V, k: _eff(V, _bset(k, "cutIn")) in ("× pitch per cycle", "× pitch per blade")),
    ("`cutHz`", "cf7e5b8d23", "row", _blade_row("cutHz"),
     lambda V, k: _eff(V, _bset(k, "cutIn")) == "Hz"),
    ("`fmType`, `modShape`, `fmDepth`, `modIn`, `fmIndexSpread`", "5aed95ca5b", "row",
     _blade_row("fmType", "modShape", "fmDepth", "modIn", "fmIndexSpread"),
     lambda V, k: V[_bset(k, "mode")] in FM and _own(V, k, "fmSource")),
    ("`modRate` / `modRateHz`", "f00db249ae", "row", _blade_row("modRate", "modRateHz"),
     lambda V, k: V[_bset(k, "mode")] in FM and _own(V, k, "fmSource")
     and V[_bset(k, "modIn")] == ("× pitch" if split_key(k)[2] == "modRate" else "Hz")),
    ("`shape`, `shapeSpread`", "231ac0c5eb", "row", _blade_row("shape", "shapeSpread"),
     lambda V, k: V[_bset(k, "mode")] in CARRIER and V[_bset(k, "wave")] == "sine→saw"),
    ("`edges`", "265ee6a912", "row", _blade_row("edges"),
     lambda V, k: V[_bset(k, "width")] > OFF_ZONE),
    ("`ruleDepth`", "f1d59bd1c3", "row", _blade_row("ruleDepth"),
     lambda V, k: V[_bset(k, "cutRule")] != "even"),
    ("`cutSpread`", "8000da7317", "row", _blade_row("cutSpread"),
     lambda V, k: V[_bset(k, "cutRule")] == "even"),
    ("`ratios`", "e2a0d3a885", "row", lambda k: False, None),
    ("`spreadDrift`", "e2c3036cfd", "row", _osc_row("spreadDrift"),
     lambda V, k: V[f"{_osc(k)}.spreadLaw"] == "drift"),
    ("every spread", "7302856562", "row", _blade_row(*SPREADS),
     lambda V, k: _U(V, k) > 1 and _own(V, k, "spreadSource")),
    ("`upperHears`, `stacking`, `collisionPitch`, `collisionBite`", "fc6693cc2d", "row",
     _osc_row("upperHears", "stacking", "collisionPitch", "collisionBite"),
     lambda V, k: _on(V, f"{_osc(k)}.blade1.on") and _on(V, f"{_osc(k)}.blade2.on")),
    ("blade 2's `rotate` and `rotateSpread`", "447a49d018", "row",
     lambda k: _blade(k) is not None and ".blade2." in k and split_key(k)[2] in ("rotate", "rotateSpread"),
     lambda V, k: V[_bset(k, "rotation")] == "its own"),
    ("`crossMod`", "c9af2b6529", "row", _osc_row("crossMod"), lambda V, k: _carrier_on(V, _osc(k))),
    ("`feedback`", "43d9406870", "row", _osc_row("feedback"), lambda V, k: _carrier_on(V, _osc(k))),
    ("`loopDrive`", "43d9406870", "row", _osc_row("loopDrive"), lambda V, k: _carrier_on(V, _osc(k))),
    ("`detune`, `detuneRange`, `coupling`, `onsetLock`, `pivot`, `panImage`, `panInvert`, `motionGrain`, "
     "`spreadLaw`, `quantize`", "48be8c0130", "row",
     _osc_row("detune", "detuneRange", "coupling", "onsetLock", "pivot", "panImage", "panInvert",
              "motionGrain", "spreadLaw", "quantize"),
     lambda V, k: _U(V, k) > 1),
    ("`width`", "b82cf35c37", "row", _osc_row("width"),
     lambda V, k: _U(V, k) > 1 or V[f"{_osc(k)}.panMotion"] > 0),
    ("`panMotion`", "0319fbaedf", "row", _osc_row("panMotion"), lambda V, k: V[f"{_osc(k)}.width"] > 0),
    ("`harmonicReach`", "6318acf4cf", "row", _osc_row("harmonicReach"),
     lambda V, k: _U(V, k) > 1 and V[f"{_osc(k)}.detuneLaw"] == "harmonic"),
    ("`anchor`, `distribution`", "0cd06d0373", "row", _osc_row("anchor", "distribution"),
     lambda V, k: _U(V, k) > 1 and V[f"{_osc(k)}.detuneLaw"] != "harmonic"),
    ("`phaseLag`", "884f13d190", "row", _osc_row("phaseLag"),
     lambda V, k: _U(V, k) > 1 and (V[f"{_osc(k)}.coupling"] > 0 or V[f"{_osc(k)}.onsetLock"] > 0)),
    ("`dissolve`", "d3be5dfa1d", "row", _osc_row("dissolve"),
     lambda V, k: _U(V, k) > 1 and V[f"{_osc(k)}.onsetLock"] != 0),
    ("`timingCorrection`", "504f9617a8", "row", _osc_row("timingCorrection"),
     lambda V, k: _U(V, k) > 1 and V[f"{_osc(k)}.onsetScatter"] > 0),
    ("`driftRate`", "34ef61505d", "row", _osc_row("driftRate"), lambda V, k: V[f"{_osc(k)}.driftDepth"] > 0),
    ("`inertia`", "a22e697c7a", "row", _osc_row("inertia"),
     lambda V, k: _U(V, k) > 1 or V[f"{_osc(k)}.driftDepth"] > 0),
    ("`pulseWidth`", "8f1b1c97c7", "row", _src_row("sub", "pulseWidth"), lambda V, k: V["sub.wave"] == "pulse"),
    ("`bumpHarmonic`, `bumpAmount`, `bumpPhase`, `bump2Harmonic`, `bump2Amount`, `bump2Phase`", "8fe914c253",
     "row", _src_row("sub", "bumpHarmonic", "bumpAmount", "bumpPhase", "bump2Harmonic", "bump2Amount",
                     "bump2Phase"),
     lambda V, k: V["sub.wave"] == "bump"),
    ("`priority`", "c68cea9fab", "row", _src_row("sub", "priority"), lambda V, k: V["sub.voiceMode"] == "mono"),
    ("`softness`", "9c9eaef30f", "row", _src_row("sub", "softness"),
     lambda V, k: V["sub.wave"] == "smooth square"),
    ("`lfsrKeytrack`", "f7ff044406", "row", _src_row("noise", "lfsrKeytrack"), lambda V, k: V["noise.type"] == "LFSR"),
    ("every noise row but `spread`, `pan`, `mute`, `solo` and `env`", "140358e3ab", "section",
     lambda k: split_key(k)[0] == "noise" and split_key(k)[2] not in NOISE_ALWAYS_SOURCE + ("on",),
     lambda V, k: _on(V, "noise.on") or (_on(V, "sub.on") and V["sub.erode"] > 0)),
    ("`character`", "687c4aa22c", "row", _src_row("sub", "character"), lambda V, k: V["sub.drive"] > 0),
    ("every `env1` row", "b17ed82be7", "section", lambda k: split_key(k)[0] == "env1",
     lambda V, k: _followers(V, "ENV 1")),
    ("every `env2`, `env3` and `env4` row", "885a0d46c8", "section",
     lambda k: split_key(k)[0] in ("env2", "env3", "env4"),
     lambda V, k: _followers(V, "ENV " + split_key(k)[0][-1])),
    ("a source's `env`", "e912f703d6", "row", lambda k: split_key(k)[0] in SOURCES and split_key(k)[2] == "env",
     lambda V, k: _on(V, f"{split_key(k)[0]}.on")),
    ("`density` / `rate` / `length` (noise)", "5fdb622303", "row", _src_row("noise", "density", "rate", "length"),
     lambda V, k: V["noise.type"] == {"density": "Crackle", "rate": "S&H", "length": "LFSR"}[split_key(k)[2]]),
    ("`feedback` / `vowel` / `gust` (noise)", "6d49f75be0", "row", _src_row("noise", "feedback", "vowel", "gust"),
     lambda V, k: _on(V, {"feedback": "noise.resonator", "vowel": "noise.whisper",
                          "gust": "noise.wind"}[split_key(k)[2]])),
    ("`cutoff`, `resonance`, `keytrack` (noise)", "fbe4df72a2", "row",
     _src_row("noise", "cutoff", "resonance", "keytrack"), lambda V, k: V["noise.filter"] != "Off"),
    ("`pitch` (noise)", "edc22a36dc", "row", _src_row("noise", "pitch"),
     lambda V, k: V["noise.type"] == "LFSR" or _on(V, "noise.resonator")
     or (V["noise.filter"] != "Off" and V["noise.keytrack"] > 0)),
    ("every reserved row (5.1)", "9512acb9c6", "section", None, lambda V, k: False),
]


def pa8_entries(rows):
    """PA8 with the reserved row's matcher bound to the table (it reads `reserved`, not a name)."""
    out = []
    for cell, h, scope, match, live in PA8:
        if match is None:
            match = (lambda k, _r=rows: _r[k]["reserved"])
        out.append((cell, h, scope, match, live))
    return out


def pa8_drift(spec_text):
    """Differences between the spec's PA-8 table and the PA8 list: [] when they agree row for row."""
    _, spec_rows = read_pa8(spec_text)
    want = [(c, cond_hash(t)) for c, t in spec_rows]
    have = [(c, h) for c, h, *_ in PA8]
    out = []
    for c, h in want:
        if (c, h) not in have:
            mine = [x for x in have if x[0] == c]
            out.append(f"PA-8 row {c!r}: " + (f"its condition changed (spec hash {h}, pinned {mine[0][1]})"
                                              if mine else f"has no predicate in tools/patch_check.py (hash {h})"))
    for c, h in have:
        if c not in [x[0] for x in want]:
            out.append(f"tools/patch_check.py has a predicate for {c!r}, which PA-8 no longer has")
    return out


def matching(entries, key):
    """The PA-8 entries that govern key, in table order."""
    return [e for e in entries if e[3](key)]


def is_live(rows, entries, V, key):
    """(live, [condition cells that failed]) for key under the resolved values V, by PA-8.

    A source row also needs its source on: PA-8's preamble ("live whenever its source is on") for the rows
    it does not list, and for the listed ones because a source that is off contributes nothing (5.2's on
    switches, PF-6). The noise's blanket row replaces that test with its own, which admits Erode."""
    if rows[key]["reserved"]:
        return False, ["every reserved row (5.1)"]
    failed = []
    hits = matching(entries, key)
    noise_blanket = any(c.startswith("every noise row") for c, *_ in hits)
    src = source_of(key)
    if src and not noise_blanket and split_key(key)[2] != "on" and not _on(V, f"{src}.on"):
        failed.append(f"its source is on ({src}.on)")
    for cell, _, _, _, live in hits:
        if live is not None and not live(V, key):
            failed.append(cell)
    return not failed, failed


# ---------------------------------------------------------------- Appendix B.1: a bank preset as a base

B1_SHA = "f297fdde30c8"          # sha256[:12] of B.1's text; see the module docstring
DECAY_SCALE = math.log(1e4) / 4  # B.1 [X22]: the bank's e^(-4t/T) as C.12's curve, ln(10^4)/4


def b1_text(spec_text):
    i = spec_text.index("### B.1 The bank format")
    return spec_text[i:spec_text.index("### B.2", i)]


def b1_base_values(spec_text):
    """B.1's base values, parsed from its own sentence (never copied)."""
    t = b1_text(spec_text)
    body = t[t.index("takes its base value:") + len("takes its base value:"):t.index(". A key with no listed")]
    out = {}
    for m in re.finditer(r'(\w+) ("[^"]*"|−?[\d.]+)', body):
        v = m.group(2)
        out[m.group(1)] = v.strip('"') if v.startswith('"') else float(v.replace("−", "-"))
    return out


ENUM = {   # B.1's enumeration table, by meaning; every name is checked against params.json's options
    "wave": {0: "sine", 1: "triangle", 2: "saw", 3: "square", 4: "reverse saw", 6: "sine→saw"},
    "modShape": {0: "sine", 1: "triangle", 2: "saw", 3: "square", 4: "reverse saw", 5: "smooth noise",
                 7: "S&H noise"},
    "spreadLaw": {0: "gradient", 1: "random", 2: "alternate", 3: "swarm", 4: "drift"},
    "rotation1": {1: "restart per note", 0: "free-running"},
    "rotation2": {1: "with blade 1", 0: "its own"},
    "mode": {0: "Sync", 1: "FM reset", 2: "FM free", 3: "Noise", 4: "Fold", 5: "Ring", 6: "Crush"},
    "mirror": {0: "off", 1: "reflect", 2: "twin −", 3: "twin +", -1: "as blade 1"},
    "cutIn": {0: "× pitch per cycle", 1: "× pitch per blade", 2: "Hz", -1: "as blade 1"},
    "frame": {0: "member", 1: "swarm", -1: "as blade 1"},
    "cutRule": {0: "even", 1: "harmonic", 2: "undertone", 3: "octaves", 4: "major", 5: "minor", 6: "fifths",
                7: "golden", 8: "primes", 9: "custom"},
    "voice.mode": {0: "poly", 1: "mono", 2: "legato"},
    "panImage": {0: "balanced", 1: "spread fan"},
    "quantize": {0: "free", 1: "whole"},
    "fmType": {0: "phase", 1: "pitch"},
    "modIn": {0: "× pitch", 1: "Hz"},
    "source": {0: "blade 1's", 1: "its own"},
    "stacking": {0: "blade 2 over 1", 1: "blade 1 over 2"},
    "onoff": {0: "off", 1: "on"},
}

# B.1's blade keys: bank key (blade 1's; blade 2's adds "2") -> (row name, enumeration or None)
BLADE_KEYS = [("mode", "mode", "mode"), ("hot", "wave", "wave"), ("morph", "shape", None),
              ("lock", "cutIn", "cutIn"), ("k", "cut", None), ("kHz", "cutHz", None),
              ("c", "position", None), ("w", "width", None), ("hard", "edges", None),
              ("depth", "depth", None), ("mirror", "mirror", "mirror"), ("frame", "frame", "frame"),
              ("rotRate", "rotate", None), ("fmType", "fmType", "fmType"), ("mshape", "modShape", "modShape"),
              ("I", "fmDepth", None), ("mUnit", "modIn", "modIn"), ("m", "modRate", None),
              ("mHz", "modRateHz", None), ("bspread", "positionSpread", None), ("kspread", "cutSpread", None),
              ("kRule", "cutRule", "cutRule"), ("kRuleAmt", "ruleDepth", None),
              ("wspread", "widthSpread", None), ("dspread", "depthSpread", None),
              ("mspread", "shapeSpread", None), ("ispread", "fmIndexSpread", None),
              ("rotSpread", "rotateSpread", None)]


def bank_presets(path=BANK):
    return {p["name"]: p for p in json.loads(pathlib.Path(path).read_text(encoding="utf-8"))["presets"]}


def translate(params, base_values, rows):
    """B.1: one bank preset's params -> {key: canonical value} over init (osc 2, sub and noise stay off).
    Returns (values, notes): notes name what B.1 keeps outside the parameters."""
    P = dict(base_values)
    P.update(params)
    V = init_values(rows)
    notes = []

    def put(key, val):
        V[key] = val

    def enum(name, n):
        return ENUM[name][int(n)]

    o, b1, b2 = "osc1", "osc1.blade1", "osc1.blade2"
    # B.1 (round 63, ADR-381): N "as is, clamped to" unison's range, the clamp reported. The top is read from
    # the row, never written here; no bank preset exceeds it today, so this only guards a future bank.
    n_top = int(max(option_number(x) for x in rows[f"{o}.unison"]["options"]))
    if int(P["N"]) > n_top:
        notes.append(f"N {int(P['N'])} clamped to {n_top}, unison's maximum (B.1)")
    put(f"{o}.unison", str(min(int(P["N"]), n_top)))
    put(f"{o}.detune", P["detune"] / 100.0)
    put(f"{o}.detuneLaw", "cents")
    put(f"{o}.detuneRange", "×1")
    put(f"{o}.distribution", "even")
    put(f"{o}.coupling", float(P["K"]))
    if int(P["phaseMode"]) == 0:
        put(f"{o}.phase", 0.0), put(f"{o}.phaseRandom", 100.0)
    else:
        put(f"{o}.phase", 0.0), put(f"{o}.phaseRandom", 0.0)
    put(f"{o}.retrigger", "on")
    put(f"{o}.width", float(P["width"]))
    put(f"{o}.panImage", enum("panImage", P["panOrder"]))
    put(f"{o}.bandLimit", float(P["aa"]))
    if P["xm"] > 0 or P["fb"] > 0:
        put(f"{o}.loopDrive", 1.0)
    put(f"{o}.baseWave", enum("wave", P["base"]))
    put(f"{o}.crossMod", float(P["xm"]))
    put(f"{o}.feedback", float(P["fb"]))
    put(f"{o}.spreadLaw", enum("spreadLaw", P["law"]))
    put(f"{o}.spreadDrift", float(P["driftRate"]))
    put(f"{o}.quantize", enum("quantize", P["kq"]))
    put(f"{o}.stacking", enum("stacking", P["b2order"]))
    put(f"{o}.upperHears", float(P["b2mix"]))
    put(f"{o}.collisionPitch", float(P["colK"]))
    put(f"{o}.collisionBite", float(P["colB"]))
    if P.get("kCustom") and enum("cutRule", P["kRule"]) == "custom":
        notes.append(f"ratios \"{P['kCustom']}\" (oscillator state, 5.3; not a parameter, so not in horde-patch/1)")
    for blade, sfx in ((b1, ""), (b2, "2")):
        for bank_key, name, en in BLADE_KEYS:
            n = P[bank_key + sfx]
            if en:
                put(f"{blade}.{name}", enum(en, n))
            else:
                put(f"{blade}.{name}", float(n))
    put(f"{b1}.on", "on")
    put(f"{b1}.rotation", enum("rotation1", P["rotSync"]))
    put(f"{b2}.on", enum("onoff", P["b2on"]))
    put(f"{b2}.rotation", enum("rotation2", P["rot2Follow"]))
    put(f"{b2}.fmSource", enum("source", P["b2fm"]))
    put(f"{b2}.spreadSource", enum("source", P["b2sp"]))
    put("env1.attack", P["A"] / 1000.0)
    put("env1.attackCurve", 0.0)
    put("env1.hold", 0.0)
    put("env1.decay", P["D"] / 1000.0 * DECAY_SCALE)
    put("env1.release", P["R"] / 1000.0 * DECAY_SCALE)
    put("env1.decayCurve", DECAY_SCALE)
    put("env1.releaseCurve", DECAY_SCALE)
    put("env1.sustain", float(P["S"]))
    put("voice.mode", enum("voice.mode", P["polyMode"]))
    envs = [("env2", "")] + ([("env3", "2")] if int(P["b2env"]) == 1 else [])
    for env, sfx in envs:
        put(f"{env}.attack", P["benvA" + sfx] / 1000.0)
        put(f"{env}.attackCurve", 0.0)
        put(f"{env}.hold", 0.0)
        put(f"{env}.decay", P["benvD" + sfx] / 1000.0 * DECAY_SCALE)
        put(f"{env}.sustain", 0.0)
        put(f"{env}.decayCurve", DECAY_SCALE)
        put(f"{env}.velocity", float(P["benvVel" + sfx]))
        put(f"{env}.oneShot", "on")
    put("out.level", 5 * math.log10(P["N"]))
    if any(P.get(k, 0) for k in ("benvK", "benvW", "benvK2", "benvW2")):
        notes.append("blade-envelope routes (B.1: ENV 2/3 to the blades' cut and width) belong to the "
                     "modulation system and are not in horde-patch/1")
    notes.append("glide is the pitch system's (RC-3) and is not in horde-patch/1")
    return V, notes


def resolve_base(base, rows, spec_text):
    """{key: canonical value} for a patch's base, and B.1's notes; raises KeyError for an unknown base."""
    if base == "init":
        return init_values(rows), []
    presets = bank_presets()
    if base not in presets:
        raise KeyError(base)
    return translate(presets[base]["params"], b1_base_values(spec_text), rows)


# ---------------------------------------------------------------- the patch

def check_patch(patch, rows, spec_text):
    """(errors, warnings, resolved values or None, notes)."""
    errors, warnings = [], []
    if not isinstance(patch, dict):
        return ["a patch is a JSON object"], [], None, []
    extra = sorted(set(patch) - {"format", "base", "values"})
    if extra:
        errors.append(f"unknown member(s) {extra}: a patch holds format, base and values only")
    if patch.get("format") != FORMAT:
        errors.append(f"format is {patch.get('format')!r}; it must be {FORMAT!r}")
    base = patch.get("base", "init")
    try:
        V, notes = resolve_base(base, rows, spec_text)
    except KeyError:
        errors.append(f"base {base!r} is neither 'init' nor a preset in {BANK.relative_to(ROOT)}")
        V, notes = init_values(rows), []
    values = patch.get("values", {})
    if not isinstance(values, dict):
        return errors + ["values is a JSON object of key: value"], warnings, None, notes
    for key, val in values.items():
        if key not in rows:
            errors.append(f"{key}: no such key in SPEC-ENGINE 5 (see docs/manual/agent/parameters.md)")
            continue
        if rows[key]["reserved"]:
            errors.append(f"{key}: reserved (5.1); the engine refuses it in 1.0 (PA-7)")
            continue
        x, err, warn = parse_value(rows[key], val)
        if err:
            errors.append(err)
            continue
        if warn:
            warnings.append(warn)
        V[key] = x
    if errors:
        return errors, warnings, None, notes
    entries = pa8_entries(rows)
    conds = dict(read_pa8(spec_text)[1])
    for key in values:
        live, failed = is_live(rows, entries, V, key)
        if not live:
            needs = "; and ".join(f"{conds[c]} (PA-8: {c})" if c in conds else c for c in failed)
            warnings.append(f"{key}: inert under PA-8 in this patch; it needs: {needs}")
    return errors, warnings, V, notes


def set_message(V, rows, seq=1):
    """The page's `set` message (docs/design/gui3/engine-stub.js, TRANSPORT): every non-reserved row, in v."""
    return {"type": "set", "seq": seq,
            "values": {k: to_v(rows[k], V[k]) for k in rows if not rows[k]["reserved"]}}


def differs_from_init(V, rows):
    init = init_values(rows)
    return {k: V[k] for k in rows if V[k] != init[k]}


def show(x):
    if isinstance(x, float):
        return "-inf" if x == -math.inf else (repr(round(x, 6)) if not x.is_integer() else repr(x))
    return json.dumps(x, ensure_ascii=False)


def b1_drift(spec_text):
    got = hashlib.sha256(b1_text(spec_text).encode("utf-8")).hexdigest()[:12]
    return [] if got == B1_SHA else [f"Appendix B.1 changed (sha {got}, pinned {B1_SHA}): re-read translate() "
                                     f"against it, then pin the new hash in tools/patch_check.py"]


def selftest(rows, spec_text):
    """Failures of this tool's own readings and refusals ([] when all hold). Run by ./verify fast."""
    bad = pa8_drift(spec_text) + b1_drift(spec_text)

    def run(values, base="init", fmt=FORMAT):
        return check_patch({"format": fmt, "base": base, "values": values}, rows, spec_text)

    must_refuse = [   # (what, patch values, base, format, the words the refusal must carry)
        ("unknown key", {"osc1.nonesuch": 1}, "init", FORMAT, "no such key"),
        ("reserved key", {"noise.mode": "noise"}, "init", FORMAT, "reserved"),
        ("out of range", {"osc1.detune": 1.5}, "init", FORMAT, "outside its range"),
        ("unknown option", {"osc1.blade1.mode": "Wobble"}, "init", FORMAT, "not one of its options"),
        ("numeric option off the list", {"osc1.octave": 7}, "init", FORMAT, "not one of its options"),
        ("index past the options", {"osc1.baseWave": 9}, "init", FORMAT, "outside 0..4"),
        ("a boolean", {"osc1.blade1.on": True}, "init", FORMAT, "true/false"),
        ("not finite", {"osc1.detune": float("nan")}, "init", FORMAT, "not a finite number"),
        ("unknown base", {}, "No such preset", FORMAT, "neither 'init' nor a preset"),
        ("wrong format", {}, "init", "horde-patch/0", "format is"),
    ]
    for what, values, base, fmt, words in must_refuse:
        errors = run(values, base, fmt)[0]
        if not any(words in e for e in errors):
            bad.append(f"control '{what}': not refused with \"{words}\" (got {errors})")
    errs, warns, _, _ = run({"osc2.detune": 0.3})
    if errs or not any("osc2.detune: inert" in w for w in warns):
        bad.append(f"control 'inert row': osc2.detune at init (osc 2 off) was not warned ({errs or warns})")
    errs, warns, V, _ = run({"osc1.detune": 0.3, "osc1.octave": -1, "osc1.detuneRange": "x12",
                             "mix.noise": "-inf", "noise.length": 2047})
    if errs or any("osc1." in w for w in warns):
        bad.append(f"positive control: a valid live patch was refused or warned ({errs or warns})")
    else:
        msg = set_message(init_values(rows), rows)
        live_keys = [k for k in rows if not rows[k]["reserved"]]
        if msg["type"] != "set" or list(msg["values"]) != live_keys:
            bad.append("set message: not every non-reserved key, in table order")
        for k, v in msg["values"].items():
            if not (0 <= v <= 1) or round(v, 12) != round(rows[k]["default_v"], 12):
                bad.append(f"set message: {k}'s init v {v} is not params.json's default_v {rows[k]['default_v']}")
                break
    base_values = b1_base_values(spec_text)
    for name, p in bank_presets().items():
        V, _ = translate(p["params"], base_values, rows)
        for k, x in V.items():
            if rows[k]["reserved"]:
                continue
            err = parse_value(rows[k], "-inf" if x == -math.inf else x)[1]
            if err:
                bad.append(f"B.1 translation of {name!r}: {err}")
    return bad


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    flags = {a for a in argv if a.startswith("--")}
    if flags == {"--selftest"} and not args:
        bad = selftest(load_rows(), SPEC.read_text(encoding="utf-8"))
        for b in bad:
            print(f"patch_check: {b}", file=sys.stderr)
        if bad:
            print(f"patch_check: RED — {len(bad)} selftest failure(s)", file=sys.stderr)
            return 1
        print(f"patch_check: GREEN — PA-8's {len(PA8)} rows and B.1 match their readings; 10 must-refuse "
              f"controls refused, the inert control warned, the positive control and init's v held; "
              f"{len(bank_presets())} bank presets translate in range")
        return 0
    if len(args) != 1 or flags - {"--set", "--resolve"}:
        print(__doc__.split("Usage:")[1], file=sys.stderr)
        return 2
    try:
        patch = json.loads(pathlib.Path(args[0]).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        print(f"patch_check: cannot read {args[0]}: {e}", file=sys.stderr)
        return 1
    rows = load_rows()
    spec_text = SPEC.read_text(encoding="utf-8")
    errors, warnings, V, notes = check_patch(patch, rows, spec_text)
    for e in errors:
        print(f"patch_check: ERROR {e}", file=sys.stderr)
    for w in warnings:
        print(f"patch_check: WARNING {w}", file=sys.stderr)
    for n in notes:
        print(f"patch_check: NOTE {n}", file=sys.stderr)
    if errors:
        print(f"patch_check: INVALID — {len(errors)} error(s)", file=sys.stderr)
        return 1
    if "--resolve" in flags:
        entries = pa8_entries(rows)
        for k, x in differs_from_init(V, rows).items():
            print(f"{k} = {show(x)}" + ("" if is_live(rows, entries, V, k)[0] else "   (inert under PA-8)"))
    if "--set" in flags:
        print(json.dumps(set_message(V, rows), ensure_ascii=False))
    print(f"patch_check: VALID — {len(patch.get('values', {}))} value(s), {len(warnings)} warning(s)",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
