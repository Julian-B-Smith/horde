#!/usr/bin/env python3
"""design_chain_check — the design chain of custody holds, hop by hop (ADR-336, B595).

WIRED: ./verify fast (wired in the PR that creates it, B595, under ADR-180 §1)

THE CHAIN. ruling (an ADR) → design master (docs/design-system/master/) → vendored bundle
(docs/design/arp-face/) → GUI 3 shell (docs/design/gui3*) → engine. docs/design/CLAUDE.md
states it; docs/design/design-ledger.json records it. Before ADR-336 nothing recorded which
rulings had reached the master or the shell, the master's source lived only in the artifact,
and no lab said whether it was live or retired, so "where is the truth?" had no answer an
agent could check. This file is that answer's gate.

WHAT IT CHECKS (each with its own tag; every tag has a must-fail plant below):
  master     docs/design-system/master/VERSION names the artifact, a version, a date and a
             sha256 for every file, and the tree is exactly those files with those hashes.
             A file edited without re-recording, an extra file or a missing one is red.
  vendored   docs/design/arp-face/{bundle.js,bundle.css,tokens.css} == vendor(master), the
             transform in tools/design_vendor.py (one stamp line; tokens.css generated from
             tokens.json). A difference is red UNLESS the ledger's `vendoring.known_drift`
             records exactly that difference, pinned to the vendored file's sha256 and to the
             master version, with its ADR. So a recorded drift is a visible, named finding
             printed every run, never a silent pass; a NEW drift, a changed file under an old
             entry, or an entry whose drift is gone (re-vendored) is red. Whether to re-vendor
             is the human's call; this check only refuses to let the question disappear.
  ledger     design-ledger.json is well-formed: the format, the five states, unique row ids,
             and every ADR, lab, master component, master version, bundle export and shell
             place a row names exists. A row's state carries what that state claims:
             in-master names a component and a master version; vendored names an export the
             vendored bundle actually defines; in-shell and wired name a shell place
             `path: needle` whose file contains the needle. `unknown` is allowed only with
             `settle` (what evidence would decide it): grounded refusal, never a guess.
  labs       every lab the design index lists (tracked docs/design/*.html except index.html,
             the set tools/gen_lab_index.py reads) has a status in the ledger, or a
             `not_labs` reason; every status is exploring, ruled (with its ADR), harvested
             (with its master version) or retired (with its reason); no entry names a lab
             that is not there.
  numbers    no ADR number has two primary headings in DECISIONS.md and no B number two rows
             in ROADMAP.md, so two ROADMAP writers (ADR-336 ruling 2) collide visibly. A
             primary heading is `## ADR-n — ` or `## ADR-n · ` (the two separators the log
             uses); `## ADR-261 A1 — `, `## ADR-086 Amendment 1 — `, `## ADR-105 A2 + ADR-100 A3`
             and `## ADR-091 / ADR-122 …` are amendments, not duplicates, and a plant proves
             they read clean. Duplicates that predate this check are KNOWN_DUPLICATES below,
             pinned to their count: fixing one or adding another is red until this list moves.
  coverage   every ADR in DECISIONS.md has a ledger row or a `not_design` entry with a reason.
             REPORT-ONLY until the B596 audit fills the ledger (AUDITED below); then blocking,
             and `unaudited` stops being an accepted lab status.

MUST-FAIL PLANTS, every run, on in-memory copies (the real tree is never touched): one per
tag, each must come back red with ITS tag and only its tag; plus must-read-clean controls
(an amendment heading, the real tree's own data) so a detector that is always red proves
nothing either. A plant that does not fire turns the whole check red ("the comparison is
blind"), the repo's L0032 rule.

Python only, no dependencies, no clock, no network; well under a second.
"""
import hashlib
import importlib.util
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MASTER = ROOT / "docs/design-system/master"
VENDORED = ROOT / "docs/design/arp-face"
LEDGER = ROOT / "docs/design/design-ledger.json"
DESIGN = ROOT / "docs/design"

# Turned True by B596 once every ADR is judged and every lab audited (ADR-336 ruling 3).
AUDITED = False

STATES = ("ruled", "in-master", "vendored", "in-shell", "wired")
LAB_STATUSES = ("exploring", "ruled", "harvested", "retired")
FORMAT = "horde.design-ledger/1"
VERSION_ID = re.compile(r"\d+-[0-9a-f]+")

# Duplicate B rows present in ROADMAP.md when this check landed (2026-10-30): two stale
# copies of a row each, with different status cells. ROADMAP.md is the lead's to fix, not
# this check's; the pin is the count, so fixing either (or adding a third copy) is red until
# this list is updated in the same PR.
KNOWN_DUPLICATES = {"B147": 2, "B155": 2}


def _load_vendor():
    spec = importlib.util.spec_from_file_location("design_vendor", ROOT / "tools/design_vendor.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


DV = _load_vendor()


# ---------------------------------------------------------------- master ----
def parse_version(text):
    head = {}
    hashes = {}
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        m = re.fullmatch(r"([0-9a-f]{64})  (\S.*)", line)
        if m:
            hashes[m.group(2)] = m.group(1)
            continue
        m = re.fullmatch(r"([a-z]+): (.+)", line)
        if m:
            head[m.group(1)] = m.group(2)
    return head, hashes


def check_master(files, version_text):
    """files: {relative path: bytes} of the master tree (VERSION excluded)."""
    errs = []
    head, hashes = parse_version(version_text)
    for k in ("artifact", "version", "date", "files"):
        if k not in head:
            errs.append(("master", f"VERSION has no `{k}:` line"))
    if "version" in head and not VERSION_ID.fullmatch(head["version"]):
        errs.append(("master", f"VERSION's version {head['version']!r} is not an artifact id"))
    if head.get("files", "").isdigit() and int(head["files"]) != len(hashes):
        errs.append(("master", f"VERSION says files: {head['files']} but lists {len(hashes)} hashes"))
    for p in sorted(set(hashes) - set(files)):
        errs.append(("master", f"{p}: listed in VERSION, missing from the tree"))
    for p in sorted(set(files) - set(hashes)):
        errs.append(("master", f"{p}: in the tree, not in VERSION (record-master after publishing)"))
    for p in sorted(set(files) & set(hashes)):
        if hashlib.sha256(files[p]).hexdigest() != hashes[p]:
            errs.append(("master", f"{p}: sha256 differs from VERSION (edited, not re-recorded)"))
    return errs, head


def read_master():
    return {p: (MASTER / p).read_bytes() for p in DV.master_files(MASTER)}


# -------------------------------------------------------------- vendored ----
def observed_drift(master_bodies, mversion, vendored):
    """The differences between the vendored files and vendor(master), one dict per file."""
    out = []
    for name in DV.FILES:
        data = vendored.get(name)
        if data is None:
            out.append({"file": name, "kind": "missing"})
            continue
        st = DV.parse_stamp(data)
        if st is None:
            out.append({"file": name, "kind": "no-stamp", "vendored_sha256": hashlib.sha256(data).hexdigest()})
            continue
        sv, sd, body = st
        kinds = []
        if sv != mversion:
            kinds.append("stamp")
        if body != master_bodies[name]:
            kinds.append("content")
        if kinds:
            out.append({"file": name, "kind": "+".join(kinds), "stamped": sv, "master": mversion,
                        "vendored_sha256": hashlib.sha256(data).hexdigest()})
    return out


def check_vendored(master_bodies, mversion, vendored, known):
    """Red unless the observed drift equals the recorded drift exactly. Returns (errs, notes)."""
    errs, notes = [], []
    keyed = {}
    for e in known:
        for k in ("file", "kind", "adr", "note"):
            if not e.get(k):
                errs.append(("vendored", f"known_drift entry {e.get('file')!r} has no `{k}`"))
        keyed[e.get("file")] = e
    obs = {d["file"]: d for d in observed_drift(master_bodies, mversion, vendored)}
    for f in sorted(set(obs) | set(keyed)):
        o, k = obs.get(f), keyed.get(f)
        if o and not k:
            errs.append(("vendored", f"{f}: {o['kind']} drift from the master at {mversion} "
                         f"(stamped {o.get('stamped')}); re-vendor with tools/design_vendor.py, or record it "
                         "in the ledger's vendoring.known_drift with its ADR for the human to rule"))
        elif k and not o:
            errs.append(("vendored", f"{f}: known_drift entry is stale — the file equals vendor(master) "
                         "now; remove the entry"))
        elif o and k:
            diff = [x for x in ("kind", "stamped", "master", "vendored_sha256") if o.get(x) != k.get(x)]
            if diff:
                errs.append(("vendored", f"{f}: drift differs from its known_drift entry in "
                             f"{', '.join(diff)} (observed {', '.join(f'{x}={o.get(x)}' for x in diff)})"))
            else:
                notes.append((f"{f}: KNOWN DRIFT ({k['adr']}), {o['kind']}: stamped {o.get('stamped')}, "
                              f"master {mversion}", k["note"]))
    return errs, notes


def read_vendored():
    return {n: (VENDORED / n).read_bytes() for n in DV.FILES if (VENDORED / n).exists()}


# ---------------------------------------------------------------- ledger ----
def bundle_exports(js):
    """Names the bundle defines: its @ds-bundle component list, `H.X =` / `Horde.X =`
    assignments, `Viz.x =` (as "Viz.x") and the `Name: Name` pairs of its export object."""
    text = js.decode("utf-8") if isinstance(js, bytes) else js
    names = set()
    m = re.search(r"/\* @ds-bundle: (\{.*?\}) \*/", text)
    if m:
        names |= {c["name"] for c in json.loads(m.group(1)).get("components", [])}
    names |= set(re.findall(r"\b(?:H|Horde|window\.Horde)\.([A-Z][A-Za-z0-9]*) =", text))
    names |= {"Viz." + n for n in re.findall(r"\bViz\.([A-Za-z0-9]+) =", text)}
    names |= {a for a, b in re.findall(r"\b([A-Z][A-Za-z0-9]*): ([A-Z][A-Za-z0-9]*)\b", text) if a == b}
    return names


def check_ledger(led, ctx):
    errs = []
    e = lambda msg: errs.append(("ledger", msg))
    if led.get("format") != FORMAT:
        e(f"format is {led.get('format')!r}, want {FORMAT!r}")
    if not isinstance(led.get("note"), str) or not led["note"].strip():
        e("no top-level note")
    if list(led.get("states", {})) != list(STATES):
        e(f"states must be exactly {', '.join(STATES)} in that order, each with its meaning")
    versions = {}
    for v in led.get("master_versions", []):
        if not VERSION_ID.fullmatch(v.get("version", "")) or not v.get("evidence"):
            e(f"master_versions entry {v!r} needs an artifact id and its evidence")
        versions[v.get("version")] = v
    if ctx["master_version"] not in versions:
        e(f"master_versions does not list the master's own version {ctx['master_version']}")
    seen = set()
    rank = {s: i for i, s in enumerate(STATES)}
    for r in led.get("rows", []):
        rid = r.get("id") or "?"
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", rid):
            e(f"row id {rid!r} must be kebab-case")
        if rid in seen:
            e(f"row id {rid} appears twice")
        seen.add(rid)
        if norm(r.get("adr")) not in ctx["adrs"]:
            e(f"{rid}: ADR {r.get('adr')!r} has no heading in DECISIONS.md")
        if not r.get("clause"):
            e(f"{rid}: no clause (which ruling or item of the ADR)")
        if not r.get("note"):
            e(f"{rid}: no note")
        sup = r.get("superseded_by")
        if sup is not None and norm(sup) not in ctx["adrs"]:
            e(f"{rid}: superseded_by {sup!r} has no heading in DECISIONS.md")
        lab = r.get("lab")
        if lab is not None and ctx["read"](lab) is None:
            e(f"{rid}: lab {lab!r} does not exist")
        st = r.get("state")
        if st == "unknown":
            if not r.get("settle"):
                e(f"{rid}: state unknown needs `settle` (what evidence would decide it)")
            continue
        if st not in rank:
            e(f"{rid}: state {st!r} is not one of {', '.join(STATES)} (or unknown with settle)")
            continue
        comp, mv = r.get("component"), r.get("master_version")
        if comp is not None and comp not in ctx["components"]:
            e(f"{rid}: component {comp!r} is not a master component (docs/design-system/master/components/)")
        if mv is not None and mv not in versions:
            e(f"{rid}: master_version {mv!r} is not in master_versions")
        if rank[st] >= rank["in-master"] and (comp is None or mv is None):
            e(f"{rid}: state {st} needs a component and a master_version")
        if rank[st] >= rank["vendored"]:
            exp = r.get("bundle") or comp
            if exp not in ctx["exports"]:
                e(f"{rid}: state {st} but the vendored bundle defines no {exp!r}")
        place = r.get("shell")
        if place is not None or rank[st] >= rank["in-shell"]:
            m = re.fullmatch(r"(\S+): (.+)", place or "")
            if not m:
                e(f"{rid}: state {st} needs a shell place `path: needle`")
            else:
                text = ctx["read"](m.group(1))
                if text is None:
                    e(f"{rid}: shell place {m.group(1)!r} does not exist")
                elif m.group(2) not in text:
                    e(f"{rid}: shell place {m.group(1)!r} does not contain {m.group(2)!r}")
    for n in led.get("not_design", []):
        if norm(n.get("adr")) not in ctx["adrs"] or not n.get("reason"):
            e(f"not_design entry {n!r} needs an existing ADR and a one-line reason")
    return errs


# ------------------------------------------------------------------ labs ----
def check_labs(led, labs, audited):
    errs = []
    e = lambda msg: errs.append(("labs", msg))
    status, not_labs = led.get("labs", {}), led.get("not_labs", {})
    for lab in labs:
        if lab not in status and lab not in not_labs:
            e(f"{lab}: no status in the ledger (exploring, ruled, harvested or retired)")
    for lab in sorted(set(status) | set(not_labs)):
        if lab not in labs:
            e(f"{lab}: has a ledger entry but is not a lab in the design index")
        if lab in status and lab in not_labs:
            e(f"{lab}: both a lab status and a not_labs reason")
    for lab, s in sorted(not_labs.items()):
        if not isinstance(s, str) or not s.strip():
            e(f"{lab}: not_labs needs a reason")
    allowed = LAB_STATUSES if audited else LAB_STATUSES + ("unaudited",)
    for lab, s in sorted(status.items()):
        k = s.get("status")
        if k not in allowed:
            e(f"{lab}: status {k!r} is not one of {', '.join(allowed)}")
        if k == "ruled" and not re.fullmatch(r"ADR-\d+", s.get("adr", "")):
            e(f"{lab}: ruled needs its ADR")
        if k == "harvested" and not VERSION_ID.fullmatch(s.get("master_version", "")):
            e(f"{lab}: harvested needs the master version that holds it")
        if k == "retired" and not s.get("note"):
            e(f"{lab}: retired needs its reason")
    return errs


def index_labs():
    # The index's own set (tools/gen_lab_index.py), plus untracked-but-not-ignored files,
    # so a lab added in a working tree is held to a status before it is even committed.
    out = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "--",
                          "docs/design/*.html"], cwd=ROOT, capture_output=True, text=True,
                         check=True).stdout.split()
    return sorted({pathlib.PurePosixPath(p).name for p in out
                   if pathlib.PurePosixPath(p).parent.as_posix() == "docs/design"
                   and not p.endswith("/index.html") and (ROOT / p).is_file()})


# --------------------------------------------------------------- numbers ----
PRIMARY = re.compile(r"^## ADR-(\d+) (?:—|·) ", re.M)
BROW = re.compile(r"^\| B(\d+) \|", re.M)


def adr_numbers(decisions):
    """Every ADR id with a heading at any level (primaries and amendments alike), for
    `exists` checks; ADR-113's heading is `###`, and amendments are real targets too."""
    ids = set()
    for m in re.finditer(r"^#{2,3} (ADR-.*)$", decisions, re.M):
        ids |= {f"ADR-{int(n):03d}" for n in re.findall(r"ADR-(\d+)", m.group(1).split(" — ")[0])}
    return ids


def norm(adr):
    m = re.fullmatch(r"ADR-(\d+)", adr or "")
    return f"ADR-{int(m.group(1)):03d}" if m else adr


def check_numbers(decisions, roadmap, known):
    errs = []
    from collections import Counter
    for n, c in sorted(Counter(PRIMARY.findall(decisions)).items(), key=lambda x: int(x[0])):
        if c > 1:
            errs.append(("numbers", f"ADR-{n} has {c} primary headings in DECISIONS.md (take the next free number)"))
    bc = Counter("B" + n for n in BROW.findall(roadmap))
    for b, c in sorted(bc.items(), key=lambda x: int(x[0][1:])):
        if c > 1 and known.get(b) != c:
            errs.append(("numbers", f"{b} has {c} rows in ROADMAP.md (take the next free number)"))
    for b, c in sorted(known.items()):
        if bc.get(b, 0) != c:
            errs.append(("numbers", f"{b}: KNOWN_DUPLICATES pins {c} rows, ROADMAP.md has {bc.get(b, 0)}; "
                         "update the pin in this file"))
    return errs


# -------------------------------------------------------------- coverage ----
def coverage_gaps(led, decisions):
    judged = {norm(r.get("adr")) for r in led.get("rows", [])} | {norm(n.get("adr")) for n in led.get("not_design", [])}
    allp = sorted({int(n) for n in PRIMARY.findall(decisions)} |
                  {int(n) for n in re.findall(r"^### ADR-(\d+) (?:—|·) ", decisions, re.M)})
    return [f"ADR-{n:03d}" for n in allp if f"ADR-{n:03d}" not in judged], len(allp)


# ---------------------------------------------------------------- plants ----
def run_all(master_files, version_text, master_bodies, vendored, led, ctx, labs, decisions, roadmap):
    errs, head = check_master(master_files, version_text)
    mv = head.get("version", "?")
    verrs, notes = check_vendored(master_bodies, mv, vendored, led.get("vendoring", {}).get("known_drift", []))
    errs += verrs
    errs += check_ledger(led, dict(ctx, master_version=mv))
    errs += check_labs(led, labs, AUDITED)
    errs += check_numbers(decisions, roadmap, KNOWN_DUPLICATES)
    return errs, notes


def selftest(base):
    """Each plant must turn red with its own tag, and add nothing else."""
    import copy
    real = run_all(*base)[0]
    real_set = set(real)
    failures = []
    ran = {"plants": 0, "clean": 0}

    def plant(desc, tag, **over):
        ran["plants"] += 1
        args = list(base)
        names = ["master_files", "version_text", "master_bodies", "vendored", "led", "ctx", "labs",
                 "decisions", "roadmap"]
        for k, fn in over.items():
            i = names.index(k)
            args[i] = fn(copy.deepcopy(args[i]))
        new = [x for x in run_all(*args)[0] if x not in real_set]
        if not new or any(t != tag for t, _ in new):
            failures.append(f"{desc}: expected new [{tag}] findings, got {new or 'none'}")

    def clean(desc, **over):
        ran["clean"] += 1
        args = list(base)
        names = ["master_files", "version_text", "master_bodies", "vendored", "led", "ctx", "labs",
                 "decisions", "roadmap"]
        for k, fn in over.items():
            i = names.index(k)
            args[i] = fn(copy.deepcopy(args[i]))
        new = [x for x in run_all(*args)[0] if x not in real_set]
        if new:
            failures.append(f"{desc}: must read clean, got {new}")

    first = sorted(base[0])[0]

    def flip(files):
        files[first] = files[first] + b" "
        return files
    plant("a master file edited without re-recording", "master", master_files=flip)
    plant("an extra file in the master", "master", master_files=lambda f: dict(f, **{"planted.md": b"x"}))
    plant("a master file missing", "master", master_files=lambda f: {k: v for k, v in f.items() if k != first})

    def body(v):
        v["bundle.css"] = v["bundle.css"] + b"/* planted */\n"
        return v
    plant("a vendored body edited by hand", "vendored", vendored=body)

    def drop_known(led):
        led.setdefault("vendoring", {})["known_drift"] = []
        return led
    if base[4].get("vendoring", {}).get("known_drift"):
        plant("a recorded drift deleted from the ledger", "vendored", led=drop_known)

    def stale(led):
        led.setdefault("vendoring", {}).setdefault("known_drift", []).append(
            {"file": "tokens.css", "kind": "stamp", "stamped": "1-0", "master": "1-0",
             "vendored_sha256": "0" * 64, "adr": "ADR-336", "note": "planted"})
        return led
    if not any(d.get("file") == "tokens.css" for d in base[4].get("vendoring", {}).get("known_drift", [])):
        plant("a known_drift entry for a file in step", "vendored", led=stale)
    else:
        # The recorded drift resolved (a correct re-vendor of tokens.css) while its entry stays.
        mver = parse_version(base[1])[0].get("version")

        def revendor(v):
            v["tokens.css"] = DV.vendor_one("tokens.css", base[2]["tokens.css"], mver, "2026-10-30")
            return v
        plant("a recorded drift resolved but its entry left behind", "vendored", vendored=revendor)

    def row(**kw):
        def f(led):
            r = {"id": "planted-row", "adr": "ADR-336", "clause": "planted", "lab": None, "component": None,
                 "master_version": None, "shell": None, "state": "ruled", "note": "planted"}
            r.update(kw)
            led.setdefault("rows", []).append(r)
            return led
        return f
    mv = parse_version(base[1])[0].get("version")
    clean("a well-formed ruled row", led=row())
    plant("a row naming a component the master lacks", "ledger",
          led=row(state="in-master", component="NoSuchWidget", master_version=mv))
    plant("a row naming an ADR with no heading", "ledger", led=row(adr="ADR-9999"))
    plant("a row in an unknown state", "ledger", led=row(state="done"))
    plant("an unknown state without settle", "ledger", led=row(state="unknown"))
    plant("a row naming a lab that is not there", "ledger", led=row(lab="docs/design/planted-lab.html"))
    plant("a row naming a master version nobody recorded", "ledger",
          led=row(state="in-master", component="Knob", master_version="1-dead"))
    plant("an in-shell row whose place lacks its needle", "ledger",
          led=row(state="in-shell", component="Knob", master_version=mv,
                  shell="docs/design/gui3.html: planted-needle-absent"))
    plant("a vendored row whose export the bundle lacks", "ledger",
          led=row(state="vendored", component="Specimen", master_version=mv, bundle="Viz.planted"))

    def nolab(labs):
        return labs + ["planted-lab.html"]
    plant("a lab with no status", "labs", labs=nolab)

    def badstatus(led):
        k = sorted(led.get("labs", {}))[0]
        led["labs"][k] = {"status": "ruled"}
        return led
    if base[4].get("labs"):
        plant("a ruled lab with no ADR", "labs", led=badstatus)
    plant("a status for a lab that does not exist", "labs",
          led=lambda l: (l.setdefault("labs", {}).update({"gone-lab.html": {"status": "exploring"}}), l)[1])

    plant("an ADR number taken twice", "numbers", decisions=lambda d: d + "\n## ADR-001 — planted\n")
    clean("an amendment heading is not a duplicate", decisions=lambda d: d + "\n## ADR-261 A9 — planted amendment\n")
    plant("a B number taken twice", "numbers", roadmap=lambda r: r + "\n| B1 | planted |\n")
    plant("a known duplicate fixed without moving its pin", "numbers",
          roadmap=lambda r: r.replace("| B147 |", "| B147x |", 1))
    return failures, ran


# ------------------------------------------------------------------ main ----
def main():
    decisions = (ROOT / "DECISIONS.md").read_text(encoding="utf-8")
    roadmap = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    try:
        led = json.loads(LEDGER.read_text(encoding="utf-8"))
    except (OSError, ValueError) as x:
        print(f"design_chain_check: {LEDGER.relative_to(ROOT)} unreadable: {x}", file=sys.stderr)
        return 1
    master_files = read_master()
    version_text = (MASTER / "VERSION").read_text(encoding="utf-8")
    master_bodies = DV.bodies(MASTER)
    vendored = read_vendored()

    def read(rel):
        p = ROOT / rel
        return p.read_text(encoding="utf-8", errors="replace") if p.is_file() else None
    comps = {p.name for p in (MASTER / "components").iterdir() if p.is_dir()}
    ctx = {"adrs": adr_numbers(decisions),
           "components": comps, "exports": bundle_exports(vendored.get("bundle.js", b"")), "read": read}
    labs = index_labs()
    base = (master_files, version_text, master_bodies, vendored, led, ctx, labs, decisions, roadmap)

    blind, ran = selftest(base)
    if blind:
        print("design_chain_check: CALIBRATION FAILED — a plant did not fire as it must; the check is blind:",
              file=sys.stderr)
        for b in blind:
            print("    " + b, file=sys.stderr)
        return 1

    errs, notes = run_all(*base)
    gaps, total = coverage_gaps(led, decisions)
    if AUDITED and gaps:
        errs += [("coverage", f"{a}: no ledger row and no not_design entry") for a in gaps]
    # A recorded drift is printed every run (a named finding, never a silent pass); each
    # distinct explanation once, after the files it covers.
    said = []
    for line, why in notes:
        print("design_chain_check: " + line)
        if why not in said:
            said.append(why)
    for why in said:
        print("design_chain_check:   why recorded: " + why)
    if errs:
        print(f"design_chain_check: {len(errs)} finding(s) on the design chain:", file=sys.stderr)
        for tag, msg in errs:
            print(f"    [{tag}] {msg}", file=sys.stderr)
        return 1
    rows = led.get("rows", [])
    by = {s: sum(1 for r in rows if r.get("state") == s) for s in STATES + ("unknown",)}
    st = {}
    for s in led.get("labs", {}).values():
        st[s.get("status")] = st.get(s.get("status"), 0) + 1
    cov = (f"coverage {total - len(gaps)} of {total} ADRs judged"
           + ("" if AUDITED else f" (REPORT-ONLY until B596: {len(gaps)} unjudged)"))
    print(f"design_chain_check: master {len(master_files)} files at {parse_version(version_text)[0]['version']} "
          f"match VERSION; vendored == vendor(master) or its recorded drift; ledger {len(rows)} rows "
          f"({', '.join(f'{k} {v}' for k, v in by.items() if v)}), {len(led.get('not_design', []))} not_design; "
          f"{len(labs)} labs ({', '.join(f'{k} {v}' for k, v in sorted(st.items()))}, "
          f"{len(led.get('not_labs', {}))} not a lab); no duplicate ADR or B number beyond "
          f"{len(KNOWN_DUPLICATES)} pinned; {cov}; all {ran['plants']} plants fired, "
          f"{ran['clean']} clean controls read clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
