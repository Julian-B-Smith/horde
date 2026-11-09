#!/usr/bin/env python3
"""S0 item 4 (B665): BB-41's worst-case `blade-bake` recipe size on the real row keys (BLADE-BAKE BB-40, BB-41,
BB-F8; CONTRACT §3.1, §3.2).

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6 item 4).

WHAT IT PROVES. It reads the clean-room engine's manifest (read-only: `engine2/core/manifest/manifest.h` for the
block-relative ids, `manifest.cpp` for each row's kind, options and defaults; the repo root from the environment
variable HORDE_CLEANROOM_ENGINE, default ~/Documents/Claude/cleanroom-engine), takes the 68 retired rows of one
oscillator (both blades' rows and the six blade-only oscillator rows, CONTRACT §2.4), builds the worst-case recipe
(every row away from its default at its longest canonical value, the longest `ratios` text, the frame axis, fRef,
the largest seed) in §3.1's canonical text (keys sorted by UTF-8 bytes, no whitespace, ECMAScript's shortest
number form), and reports its bytes against BB-41's 3 KiB, and a patch of 8 such recipes in the `tables`
dictionary against §3.2's 32 KiB.

DETECTOR PLANT (D) (BB-F8): the same recipe with named keys (`blade1.cutSpread`, not `11a`): it must exceed 3 KiB.
CONTROL: a recipe with no row away from default (the base wave only) must sit far under the bound.

CHOICES THE FORMAT LEAVES OPEN (stated; each is a few bytes): the hex id is lowercase without "0x" (`23`, `11f`);
the base wave is written as its row `23` like any row (BB-40 lists it first; written always); the ratios text
under key "ratios"; the frame axis as "axis":{"row":"<id>","start":v,"swing":v} (BB-20 (b), the largest form);
fRef as "fRef":261.63 (BB-13). Values: a continuous row's worst is any double in [0, 1] (host automation reaches
any v through the row's law), whose ECMAScript shortest form is at most 24 characters (17 digits in [1e-6, 1e-5):
"0.00000" + 17 digits); a stepped row's worst is its longest index fraction i / (steps - 1).

Run: python3 -I tools/wavetable_s0/bb41_recipe_size.py   (instant)
"""
import json
import os
import re
import sys

ROOT = os.path.expanduser(os.environ.get("HORDE_CLEANROOM_ENGINE", os.path.join("~", "Documents", "Claude", "cleanroom-engine")))
MAN = os.path.join(ROOT, "engine2", "core", "manifest")
RETIRED_OSC = ("baseWave", "quantize", "stacking", "upperHears", "collisionPitch", "collisionBite")


def es_number(x):
    """ECMAScript Number::toString (the shortest round-trip digits; CONTRACT §3.1's canonical number form)."""
    if x == 0:
        return "0"
    if isinstance(x, int):
        return str(x)
    sign = "-" if x < 0 else ""
    r = repr(abs(x))
    mant, _, exp = r.partition("e")
    e = int(exp) if exp else 0
    ip, _, fp = mant.partition(".")
    digits = (ip + fp).lstrip("0")
    point = len(ip) + e if ip != "0" else e - (len(fp) - len(fp.lstrip("0")))
    digits = digits.rstrip("0") or "0"
    k, n = len(digits), point                      # x = 0.d1d2..dk x 10^n
    if k <= n <= 21:
        s = digits + "0" * (n - k)
    elif 0 < n <= 21:
        s = digits[:n] + "." + digits[n:]
    elif -6 < n <= 0:
        s = "0." + "0" * (-n) + digits
    else:
        e2 = n - 1
        s = digits[0] + ("." + digits[1:] if k > 1 else "") + "e" + ("+" if e2 >= 0 else "-") + str(abs(e2))
    return sign + s


def enum_ids(text, name):
    body = re.search(r"enum " + name + r" : std::uint32_t \{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"//[^\n]*", "", body)
    ids, nxt = {}, 0
    for tok in [t.strip() for t in body.split(",") if t.strip()]:
        if "=" in tok:
            k, v = [p.strip() for p in tok.split("=")]
            nxt = int(v, 0)
        else:
            k = tok
        ids[k] = nxt
        nxt += 1
    return ids


def rows(cpp, func):
    """(enum, key, kind, steps) for each row call in `func`'s list, by balanced parentheses (calls span lines)."""
    body = re.search(r"std::vector<Row> " + func + r"\(\) \{(.*?)\n\}", cpp, re.S).group(1)
    consts = dict(re.findall(r'constexpr const char\* (\w+) = "([^"]*)";', cpp))
    consts.update(re.findall(r'constexpr const char\* (\w+) = "([^"]*)";', body))   # a local list wins
    out = []
    for m in re.finditer(r"\b(cont|choice|count)\(", body):
        depth, i = 1, m.end()
        while depth:
            depth += {"(": 1, ")": -1}.get(body[i], 0)
            i += 1
        args = [a.strip() for a in re.findall(r'"[^"]*"|[^,]+', body[m.end():i - 1])]
        kind, enum, key = m.group(1), args[0], args[1].strip('"')
        if kind == "choice":
            lists = [consts.get(a, a.strip('"')) for a in args[4:] if a.startswith('"') or a in consts]
            out.append((enum, key, "step", max(len(lst.split("|")) for lst in lists)))
        elif kind == "count":
            out.append((enum, key, "step", int(float(args[6]) - float(args[5]) + 1)))
        else:
            out.append((enum, key, "cont", None))
    return out


def worst_value(kind, steps):
    if kind == "cont":
        return 1.2345678901234567e-6                # 24 characters in ECMAScript's form; any v in [0, 1] is reachable
    best = max((i / (steps - 1) for i in range(1, steps)), key=lambda v: len(es_number(v)))
    return best


def canonical(obj):
    if isinstance(obj, dict):
        items = sorted(obj.items(), key=lambda kv: kv[0].encode("utf-8"))
        return "{" + ",".join(json.dumps(k, ensure_ascii=False) + ":" + canonical(v) for k, v in items) + "}"
    if isinstance(obj, str):
        return json.dumps(obj, ensure_ascii=False)
    return es_number(obj)


def main():
    h = open(os.path.join(MAN, "manifest.h")).read()
    cpp = open(os.path.join(MAN, "manifest.cpp")).read()
    osc_ids, blade_ids = enum_ids(h, "Osc"), enum_ids(h, "Blade")
    orows = {key: (enum, kind, steps) for enum, key, kind, steps in rows(cpp, "osc_rows")}
    brows = rows(cpp, "blade_rows")
    retired = []                                    # (hex id, named key, kind, steps)
    for key in RETIRED_OSC:
        enum, kind, steps = orows[key]
        retired.append((format(osc_ids[enum], "x"), key, kind, steps))
    for b in (0, 1):
        for enum, key, kind, steps in brows:
            if b == 0 and enum in ("kFmSource", "kSpreadSource"):
                continue                            # blade 2 only (manifest.cpp fill())
            rid = 0x100 * (b + 1) + blade_ids[enum]
            # a stepped row's count is the longer of its two option lists (blade 2's adds "as blade 1"): the worst case
            retired.append((format(rid, "x"), f"blade{b + 1}.{key}", kind, steps))
    print(f"retired rows found on one oscillator: {len(retired)} (CONTRACT §2.4: 68); "
          f"{sum(1 for r in retired if r[2] == 'cont')} continuous, {sum(1 for r in retired if r[2] == 'step')} stepped")
    print(f"ids span {min(int(r[0], 16) for r in retired):#x} .. {max(int(r[0], 16) for r in retired):#x}")

    ratios_256 = ("1/2," * 64)[:256]
    ratios_512 = ("1/2," * 128)[:512]
    axis = {"row": "1ff", "start": 1.2345678901234567e-6, "swing": 1.2345678901234567e-6}

    def recipe(named, ratios, with_rows=True, seed=4294967295):
        s = {(name if named else rid): worst_value(kind, steps) for rid, name, kind, steps in retired} if with_rows \
            else {("baseWave" if named else "23"): 2 / 5}
        s.update({"ratios": ratios, "axis": axis, "fRef": 261.63}) if with_rows else s.update({"fRef": 261.63})
        return {"v": 1, "kind": "gen", "gen": "blade-bake", "genV": 1, "set": s, "seed": seed}

    worst = canonical(recipe(False, ratios_256))
    worst512 = canonical(recipe(False, ratios_512))
    named = canonical(recipe(True, ratios_256))
    named_ws = json.dumps(recipe(True, ratios_256), indent=1)
    minimal = canonical(recipe(False, "", with_rows=False))
    row_bytes = [len(json.dumps(rid)) + 1 + len(es_number(worst_value(k, s))) for rid, _, k, s in retired]
    print(f"\nper row (key, colon, value; comma not counted): max {max(row_bytes)} bytes, all 68 {sum(row_bytes)} bytes")
    print(f"worst-case recipe, hex-id keys, ratios 256 bytes (the spec's bound, IF-12/MO-13): {len(worst.encode())} bytes "
          f"({len(worst.encode()) / 1024:.2f} KiB) against 3 KiB = 3072: {'under' if len(worst.encode()) <= 3072 else 'OVER'}")
    print(f"   the same with ratios 512 bytes (the engine's kRatiosMaxBytes = 512, tests/harness/horde_engine.h): "
          f"{len(worst512.encode())} bytes ({'under' if len(worst512.encode()) <= 3072 else 'OVER'} 3 KiB)")
    seed6 = canonical(recipe(False, ratios_256, seed=999999))
    print(f"   with the seed row's own range (0..999999) instead of uint32: {len(seed6.encode())} bytes")
    print(f"detector plant (D), named keys: {len(named.encode())} bytes canonical ({'fires' if len(named.encode()) > 3072 else 'DOES NOT FIRE'});"
          f" {len(named_ws.encode())} bytes pretty-printed")
    print(f"control, no row away from default (base wave only): {len(minimal.encode())} bytes")

    def dict_bytes(recipes):
        return len(("{" + ",".join('"' + "0123456789abcdef"[:16] + '":' + r for r in recipes) + "}").encode())

    for label, r in (("ratios 256", worst), ("ratios 512", worst512)):
        d8 = dict_bytes([r] * 8)
        print(f"patch with 8 worst-case bake recipes ({label}) in the tables dictionary: {d8} bytes ({d8 / 1024:.2f} KiB)"
              f" against 32 KiB: {'under' if d8 <= 32768 else 'OVER'}; room left for {(32768 - d8) // 512} 512-byte recipes")
    print("\nworst-case recipe text (first 300 bytes):", worst[:300])


if __name__ == "__main__":
    main()
