#!/usr/bin/env python3
"""module_neutral_check -- no module is named in the shell's hosting code (ROADMAP B714 acceptance (1), its
secondary gate; critic finding 3).

WIRED: ./verify fast

  python3 tools/module_neutral_check.py

WHY. Acceptance (1) asks that a module be hosted from its pin, manifest and state-budget row "with no per-module branch
in preview_plugin.cpp, the bridge or the page bindings". The PRIMARY proof is behavioural and lives in the preview's
smoke: a third module added by data alone passes the generated checks with no change to those files, and the same build
with one `if (module == ...)` planted fails them. This check is the cheap secondary one, run on every fast verify: a
name the data files give a module must not appear in the hosting sources at all, code or comment, because a name in a
comment today is the branch someone writes beside it tomorrow.

THE NAMES, all read from data, never listed here: every tools/rack/modules.json entry's key, module, factory and name;
every address in its law overlay, and each address's first dotted segment (`verb` of verb.mix: a prefix test such as
`address.rfind("verb.", 0)` is a branch on the module); the run-ons of each of a module's words with each of its key
tails and `on` (`scapeon`, `verbmix`), lowercase with no separator; the ledger's legacy line prefixes and pre-B714 alias
keys (tools/engine-preview/host_ids.lock.json); and every host id the ledgers hold (that one and
tools/rack/test/host_ids*.json), written as a number, hex or decimal (`0xA00016`: a branch on a slot's id is a branch on
its module). Two addresses are exempt by contract, not by module: io.inGain and io.outGain are the names RS-4.7 gives
the I/O pair of EVERY module, and the slot supplies them under those names, so `io` is never a word.

THE SOURCES: tools/engine-preview/preview_plugin.cpp, bridge_core.h and bridge_core.cpp, and the slot library itself
(tools/rack/*.h, *.cpp and CMakeLists.txt), never its adapters, recipes, tests or data.

MATCHING IS BY TOKEN AND BY THE WORDS INSIDE ONE: a token is a run of letters, digits, '_' and '.', its trailing dots
dropped. An address matches the whole token exactly; a name (or run-on) matches the whole token or any of its words
(split on '.', '_' and camelCase), case-insensitively; a host id matches a token that reads as that number. "scape"
never matches "escape"; "fxfilter.mix" matches only itself.

MUST-FAIL CONTROLS, every run, on in-memory copies of preview_plugin.cpp: a module's name in a comment, inside camelCase,
k-constant and snake_case identifiers, a branch on a display name, an `if (module == "<name>")` branch, an address, a
prefix test on an address's first segment, a run-on (`<word>on`) and a ledger host id as a hex literal; each must read
red. A planted "escape" must read green.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODULES = "tools/rack/modules.json"
LEDGER = "tools/engine-preview/host_ids.lock.json"
ID_LEDGERS = [LEDGER] + sorted(str(p.relative_to(ROOT)) for p in (ROOT / "tools/rack/test").glob("host_ids*.json"))
SOURCES = ["tools/engine-preview/preview_plugin.cpp", "tools/engine-preview/bridge_core.h", "tools/engine-preview/bridge_core.cpp"]
CONTRACT_NAMES = {"io.inGain", "io.outGain"}   # RS-4.7: every module's I/O pair, owned or supplied
TOKEN = re.compile(r"[A-Za-z0-9_.]+")


def sources():
    out = list(SOURCES)
    for p in sorted((ROOT / "tools/rack").glob("*")):
        if p.is_file() and (p.suffix in (".h", ".cpp") or p.name == "CMakeLists.txt"):
            out.append(str(p.relative_to(ROOT)))
    return out


def names():
    """(case-insensitive names, exact addresses, host ids), each with where it came from."""
    words, addresses, ids = {}, {}, {}
    for m in json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"]:
        own = []
        for field in ("key", "module", "factory", "name"):
            if m.get(field):
                words[m[field].lower()] = f"modules.json {m['key']}.{field}"
                own.append(m[field].lower())
        over = ROOT / m["overlay"]
        tails = {"on"}
        if over.is_file():
            for a in json.loads(over.read_text(encoding="utf-8")).get("keys", {}):
                if a in CONTRACT_NAMES:
                    continue
                addresses[a] = f"{m['overlay']}"
                head, _, tail = a.partition(".")
                if head and head != "io":
                    words[head.lower()] = f"{m['overlay']} address segment"
                    own.append(head.lower())
                if tail:
                    tails.add(re.sub(r"[^a-z0-9]", "", tail.lower()))
        for w in own:
            if w.isalnum():
                for tail in tails:
                    words.setdefault(w + tail, f"modules.json {m['key']}: run-on of {w!r} and a key tail")
    for rel in ID_LEDGERS:
        for e in json.loads((ROOT / rel).read_text(encoding="utf-8")).get("ids", []):
            try:
                ids[int(e["id"], 0)] = f"{rel} host id {e['id']} ({e.get('module')})"
            except (KeyError, TypeError, ValueError):
                pass
    ledger = json.loads((ROOT / LEDGER).read_text(encoding="utf-8"))
    for l in ledger.get("legacy", []):
        words[l["prefix"].lower()] = f"{LEDGER} legacy prefix"
    for a in ledger.get("aliases", []):
        addresses[a["preview"]] = f"{LEDGER} alias"
        words[a["preview"].split(".")[1].lower()] = f"{LEDGER} alias module word"
    return words, addresses, ids


NUMBER = re.compile(r"(0[xX][0-9a-fA-F]+|\d+)[uUlL]*")


def as_number(tok):
    m = NUMBER.fullmatch(tok)
    return int(m.group(1), 0) if m and not (len(m.group(1)) > 1 and m.group(1)[0] == "0" and m.group(1)[1] not in "xX") else None


CAMEL = re.compile(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+")


def parts(tok):
    """A token's words: split on '.', '_' and camelCase boundaries, so `scape_on`, `kScapeOnId` and `scapeGain` each
    hold the word `scape`, while `escape` stays one word that is no module's."""
    out = []
    for piece in re.split(r"[._]", tok):
        out += [w.lower() for w in CAMEL.findall(piece)]
    return out


def hits(text, words, addresses, ids):
    out = []
    for n, line in enumerate(text.splitlines(), 1):
        for tok in TOKEN.findall(line):
            tok = tok.strip(".")
            if not tok:
                continue
            if as_number(tok) in ids:
                out.append((n, tok, ids[as_number(tok)]))
            elif tok in addresses:
                out.append((n, tok, addresses[tok]))
            elif tok.lower() in words:
                out.append((n, tok, words[tok.lower()]))
            else:
                for part in parts(tok):
                    if part in words:
                        out.append((n, tok, words[part]))
                        break
    return out


def main():
    words, addresses, ids = names()
    fails = []
    files = sources()
    for rel in files:
        text = (ROOT / rel).read_text(encoding="utf-8")
        for n, tok, why in hits(text, words, addresses, ids):
            fails.append(f"{rel}:{n}: \"{tok}\" ({why})")
    plugin = (ROOT / SOURCES[0]).read_text(encoding="utf-8")
    some_name = sorted(words)[0]
    some_address = sorted(addresses)[0] if addresses else None
    word = next((w for w in sorted(words) if w.isalpha() and len(w) > 3), some_name)
    camel = word[0].upper() + word[1:]
    display = next((json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"][i]["name"]
                    for i in range(len(json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"]))), "x")
    plants = [(f"a module's name in a comment (\"{some_name}\")", plugin + f"\n// the {some_name} slot\n", True),
              (f"a name inside a camelCase identifier (\"{word}Gain\")", plugin + f"\nconst double {word}Gain = 1;\n", True),
              (f"a name inside a k-constant (\"k{camel}OnId\")", plugin + f"\nconstexpr int k{camel}OnId = 1;\n", True),
              (f"a name inside a snake_case identifier (\"{word}_on\")", plugin + f"\nconst bool {word}_on = true;\n", True),
              (f"a branch on a display name (\"{display}\")", plugin + f"\nif (row.name == \"{display}\") return;\n", True),
              ("an if (module == ...) branch", plugin + f"\nif (row.module == \"{some_name}\") return;\n", True),
              ] + [(f"every module word inside a camelCase identifier (\"{w}Gain\")", plugin + f"\nconst double {w}Gain = 1;\n", True)
                   for w in sorted(words) if w.isalnum()] + [
              ("\"escape\", a word holding a name inside it", plugin + "\n// escape the loop\n", False)]
    if some_address:
        plants.append((f"an address (\"{some_address}\")", plugin + f"\nresolve(\"{some_address}\");\n", True))
    # The three the review named, each from the data: a prefix test on an address's first segment, a run-on of a module
    # word and its switch, and a slot's host id as a literal. Each is planted for every module the data names.
    for m in json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"]:
        over = ROOT / m["overlay"]
        heads = sorted({a.partition(".")[0] for a in (json.loads(over.read_text(encoding="utf-8")).get("keys", {}) if over.is_file() else {})
                        if a not in CONTRACT_NAMES and a.partition(".")[0] != "io"})
        for head in heads:
            plants.append((f"a prefix test on an address's first segment (address.rfind(\"{head}.\", 0))",
                           plugin + f"\nif (address.rfind(\"{head}.\", 0) == 0) return;\n", True))
        plants.append((f"a run-on of a module word and its switch (\"{m['key'].lower()}on\")", plugin + f"\nbool {m['key'].lower()}on = true;\n", True))
    switches = [int(e["id"], 0) for e in json.loads((ROOT / LEDGER).read_text(encoding="utf-8")).get("ids", []) if e.get("switch")]
    for i in switches:   # every slot's switch id (0xA00016 among them), hex and decimal
        plants.append((f"a slot's switch id as a hex literal (0x{i:06X})", plugin + f"\nif (id == 0x{i:06X}u) return;\n", True))
        plants.append((f"a slot's switch id as a decimal literal ({i})", plugin + f"\nif (id == {i}) return;\n", True))
    for label, text, should in plants:
        red = len(hits(text, words, addresses, ids)) > len(hits(plugin, words, addresses, ids))
        ok = red == should
        print(f"{'PASS' if ok else 'FAIL'}  control: {label}: {'red' if red else 'green'}")
        if not ok:
            fails.append(f"control {label!r} read {'red' if red else 'green'}")
    for f in fails:
        print("FAIL  " + f)
    print(f"module_neutral_check: {'RED' if fails else 'GREEN'} ({len(words)} names, {len(addresses)} addresses and {len(ids)} host ids from the data, "
          f"across {len(files)} hosting sources)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
