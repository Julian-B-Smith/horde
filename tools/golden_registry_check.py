#!/usr/bin/env python3
"""golden_registry_check -- every golden and pin has a named reason to be trusted (S2, R10).

WIRED: ./verify fast

WHY. A golden is a stored expected result: a reference render, a digest file, a
pinned number. It records what the code DID on the day it was pinned, not what
is CORRECT, so with no independent justification it only proves the code has not
changed (blind-spot-armor.md, "snapshot of the bug"; catalogue row S2; R10's
tripwire "Goldens with independent check: N of M"). docs/armor/goldens.json lists
every golden with its kind and evidence; this check keeps that list complete and
honest, and prints the count.

WHAT IT CHECKS (red on any of these):
  UNREGISTERED   a golden found BY RULE (RULES below) that no registry entry covers.
                 Nobody has to remember to register one: the rules find it.
  NO_FILE        an entry whose path matches no file.
  UNRUN_CHECK    an entry whose check (or also_checks) ./verify does not invoke, or
                 whose source carries a not-wired header (that counts as not run).
  MISSING_FIELD  an entry lacking a field, or holding an empty one.
  BAD_KIND       a kind that is not one of KINDS.
  BAD_INDEPENDENT  independent=true with kind prototype_parity_at_pin, or with empty
                 evidence. `independent` is true only for closed_form,
                 prototype_plus_signoff and measurement, and only on evidence found.
  BAD_CITATION   evidence that cites nothing checkable (no ADR, B-row or path:line), or
                 cites a path:line that does not exist, an ADR not in DECISIONS.md, a
                 B-row not in ROADMAP.md.
  BLIND          a discovery rule that matches no file (a stale rule must not read as
                 "no goldens"), or fewer entries than DISCOVERY_FLOOR.
  STALE_EXCLUSION an exclusion that matches no file.
The last line is always `Goldens with independent check: N of M` (M = registry
entries, N = those with independent=true), after a breakdown by kind.

MUST-FAIL CONTROLS (LIBRARY L0032), run on EVERY run against temporary copies of
the inputs, never the real tree: a new golden file with no entry; an entry citing a
check ./verify does not run; an entry citing a check whose header says UNWIRED;
independent=true with no evidence; independent=true with kind prototype_parity_at_pin;
an entry whose path matches nothing; an entry with a field removed. Each plant must
add its own tag to the failures and the untouched copy must be green, so a judge that
is always red, or red for a different reason, is caught (detector-shares-assumption).

DISCOVERY IS BY RULE, FROM THE FILESYSTEM (not `git ls-files`) so a golden that is
new and untracked is found too. Constants embedded in a check's source are found only
where a convention marks them (the FLOOR rule); an unmarked literal is a known blind
spot, listed in docs/armor/goldens.json "known_limits".

Standard library only. Usage: python3 tools/golden_registry_check.py [--quiet]
"""
import collections
import functools
import importlib.util
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = "docs/armor/goldens.json"

KINDS = ("closed_form", "prototype_plus_signoff", "measurement", "prototype_parity_at_pin")
INDEPENDENT_KINDS = ("closed_form", "prototype_plus_signoff", "measurement")
REQUIRED = ("path", "pins", "check", "kind", "evidence", "independent")
READ_EXT = (".py", ".cpp", ".h", ".mjs")   # sources the judge reads for headers and FLOOR tokens
DISCOVERY_FLOOR = 25  # today's discovered count is above this; a collapse is BLIND, not green

# Directories never walked: build output, vendored code, other worktrees, local scratch.
PRUNE = {".git", ".claude", "libs", "node_modules", "local", ".harness", "__pycache__"}
PRUNE_PREFIX = ("build",)

# ---------------------------------------------------------------------------
# DISCOVERY RULES. Each says what it catches; a rule matching no file is BLIND.
# ---------------------------------------------------------------------------
# Data files only: code and prose that merely mention "golden" are not goldens (the
# data they read are). A new data extension used for expected values goes here.
DATA_EXT = (".json", ".txt", ".tsv", ".f32", ".bin", ".csv")
NAME_RE = re.compile(r"golden|digest|fixture|baseline|_pin|_lock|\.ref\.|expected|_ref[_.]")

RULES = [
    ("GEN", "glob", ["tools/golden/gen_*goldens*.mjs"],
     "the generators that render the prototype's output into build-golden/ at verify time; "
     "the golden IS the generator (committed binaries are never the source of truth, ADR-187 item 7)"),
    ("NAME", "name", None,
     "any data file whose path names a golden, digest, fixture, baseline, pin, lock, "
     ".ref. or expected (NAME_RE; DATA_EXT only), anywhere in the tree: catches the next one by convention"),
    ("PINS", "readme_pins", "h2/README.md",
     "every `<path>@<blob>` pinned in h2/README.md: the prototype files horde 2's parity is judged against "
     "(same parser as golden_pin_check, so the two cannot disagree about what a pin is)"),
    ("DATA", "glob", ["tests/*.txt", "tests/*.tsv", "tests/*.json",
                      "tools/*.json", "tools/patchspace/*.json", "tools/labharness/*.json",
                      "docs/armor/*.json", "docs/port/*.json", "docs/design/*.json",
                      "h2/**/*.json"],
     "committed machine data in the directories gates read: a new file here is a golden, a ledger or an "
     "input, and must be classified (registered or excluded with a reason) before it merges"),
    ("SPEC", "glob", ["specs/ACCEPTANCE.md"],
     "the oracle contract: its L0 numbers are pinned measurements that trajectory_check and others assert"),
    ("FLOOR", "token", ("tools", ("scenarioFloorHolds(", "kMinScenarios", "kFloorPins",
                                  "PARITY_FLOOR", "SWARM48_FLOOR")),
     "a check source that pins a scenario-count floor (tools/scenario_floor.h convention): a pinned number "
     "embedded in code, found by its marker token"),
]

# Files a rule matches that are NOT goldens. Never a silent skip: path glob + the reason.
EXCLUDED = [
    ("docs/armor/tolerances.json", "approval ledger: the pinned comparison tolerances (S3), approved by the human; not an expected result"),
    ("docs/armor/acceptances.json", "approval ledger: accepted limits and hole expiries (ADR-209 item 2), approved by the human; not an expected result"),
    ("docs/armor/engines.json", "the engine enrolment registry (B448 P2): which units under h2/ are enrolled and why; a ledger, not an expected result"),
    ("h2/harness/patches.json", "inputs: the harness's reference patches, generated from the ledger presets and the declared ranges; its loud/silent label is checked live by the harness selftest, it stores no result"),
    ("docs/armor/weakening-baseline.json", "approval ledger: the accepted count of gate-weakening patterns (S4); not an expected result"),
    ("docs/armor/catalogue.json", "the risk catalogue: data about gates, not an expected result"),
    ("docs/armor/goldens.json", "the registry itself"),
    ("tools/param_id_lock.json", "approval ledger: the frozen parameter ids (R7); a lock on names, not an expected render"),
    ("tools/build_flags_pin.json", "approval ledger: the pinned build flags (R11); not an expected result"),
    ("tools/license_allowlist.json", "approval ledger: the ratified license allow list (R12); not an expected result"),
    ("tools/license_inventory.json", "an inventory of dependency licences read by license_audit_check; not an expected result"),
    ("docs/port/divergences.json", "approval ledger: divergences from the prototype the human ratified (ADR-187 item 5); it explains golden changes, it is not one"),
    ("h2/cores/*/lift-ledger.json", "provenance ledger of a verbatim source lift (which blob, which edits); pins where code came from, not an output"),
    ("tests/feature_tests.tsv", "the feature-to-test table read by test_table_check; not an expected result"),
    ("tests/state_fixtures/*.txt", "INPUT state blobs; the .f32 render beside each is the golden (tests/state_fixtures/README.md)"),
    ("tools/labharness/fixtures/*.json", "INPUT records (frozen porter output) fed to a check; the expected value is asserted in the check, not stored here"),
    ("tools/scenario_floor.h", "defines the scenario-floor helper; holds no floor itself"),
    ("tools/golden_registry_check.py", "this tool names the FLOOR marker tokens it searches for"),
]


# --- small helpers ---------------------------------------------------------------------

@functools.lru_cache(maxsize=None)
def glob_re(pat):
    """Glob -> regex (memoised: the controls re-judge nine trees with the same few dozen globs). `**` crosses directories, `*` and `?` do not. fnmatch's `*` crosses
    `/`, which would let `tests/*.txt` swallow `tests/state_fixtures/x.txt`."""
    out, i = [], 0
    while i < len(pat):
        c = pat[i]
        if pat.startswith("**/", i):
            out.append("(?:.*/)?")
            i += 3
            continue
        if pat.startswith("**", i):
            out.append(".*")
            i += 2
            continue
        out.append("[^/]*" if c == "*" else "[^/]" if c == "?" else re.escape(c))
        i += 1
    return re.compile("^" + "".join(out) + "$")


def matches(pat, rel):
    return bool(glob_re(pat).match(rel))


def walk(root):
    """Every file under root as a sorted list of posix relative paths."""
    files = []
    for d, ds, fs in os.walk(root):
        ds[:] = [x for x in ds if x not in PRUNE and not x.startswith(PRUNE_PREFIX)]
        for f in fs:
            files.append(Path(d, f).relative_to(root).as_posix())
    return sorted(files)


def read(root, rel):
    try:
        return (Path(root) / rel).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


def parse_pins(root):
    """The h2 README's pinned paths, via golden_pin_check's own parser."""
    spec = importlib.util.spec_from_file_location("golden_pin_check", ROOT / "tools/golden_pin_check.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return [p for p, _ in mod.parse_pins(read(root, "h2/README.md") or "")]


# --- discovery -------------------------------------------------------------------------

def discover(root, files):
    """-> (hits: {rule_id: [paths]}, found: sorted paths a rule matched, before exclusions)."""
    hits = {}
    fileset = set(files)
    for rid, kind, spec, _why in RULES:
        got = []
        if kind == "glob":
            got = [f for f in files if any(matches(g, f) for g in spec)]
        elif kind == "name":
            got = [f for f in files if f.endswith(DATA_EXT) and NAME_RE.search(f.lower())]
        elif kind == "readme_pins":
            got = [p for p in parse_pins(root) if p in fileset]
        elif kind == "token":
            base, tokens = spec
            for f in files:
                if f.startswith(base + "/") and f.endswith((".cpp", ".h", ".py", ".mjs")):
                    text = read(root, f) or ""
                    if any(t in text for t in tokens):
                        got.append(f)
        hits[rid] = sorted(set(got))
    found = sorted({f for v in hits.values() for f in v})
    return hits, found


# --- what ./verify runs ----------------------------------------------------------------

# A declaration is line-anchored, exactly test_table_check's grammar (prose that merely
# quotes the marker is not a declaration).
DECL_RE = re.compile(r"^[ \t]*(?:/\*+|\*+|//+|#+)?[ \t]*(UN)?WIRED:[ \t]*(\S.*)$")
INVOKE_RES = (re.compile(r'"\$build_dir/([A-Za-z0-9_]+)"'),
              re.compile(r"python3 tools/(?:[a-z_]+/)*([A-Za-z0-9_]+)\.py"),
              re.compile(r"node tools/(?:[a-z_]+/)*([A-Za-z0-9_]+)\.mjs"))


def invoked(verify_text):
    """-> {check name: set of lanes}. Detected by INVOCATION, not mention (the
    test_table_check rule): comment lines and trailing comments are dropped, so the
    prose about checks that are NOT wired cannot read as wiring."""
    lane, out = "pre", {}
    for line in verify_text.split("\n"):
        if line.startswith("fast()"):
            lane = "fast"
        elif line.startswith("full()"):
            lane = "full"
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        s = re.sub(r"\s#\s.*$", "", s)
        for rx in INVOKE_RES:
            for name in rx.findall(s):
                out.setdefault(name, set()).add(lane)
    return out


def unwired_headers(root, files):
    """Check names whose source declares UNWIRED in its first 40 lines."""
    bad = set()
    for f in files:
        if f.startswith("tools/") and f.endswith((".py", ".cpp", ".mjs")):
            head = (read(root, f) or "").split("\n")[:40]
            for line in head:
                m = DECL_RE.match(line)
                if m and m.group(1):
                    bad.add(Path(f).stem)
    return bad


# --- the judge -------------------------------------------------------------------------

CITE_FILE = re.compile(r"([A-Za-z0-9_./-]+\.(?:md|json|py|cpp|h|mjs|js|txt|html|tsv|sh)):(\d+)(?:-(\d+))?")
CITE_ADR = re.compile(r"\bADR-(\d+)\b")
CITE_ROW = re.compile(r"\bB(\d{2,3})\b")
_cache = {}


def cached(root, rel):
    key = (str(root), rel)
    if key not in _cache:
        _cache[key] = read(root, rel)
    return _cache[key]


def judge_citations(label, evidence, ev_root):
    """Evidence must cite something that can be looked up, and what it cites must exist.
    Read-only against ev_root (the real tree, also for the controls: the plants change the
    registry's claims, not the records they point at)."""
    bad = []
    files = CITE_FILE.findall(evidence)
    adrs = CITE_ADR.findall(evidence)
    rows = CITE_ROW.findall(evidence)
    if not (files or adrs or rows):
        return [f"{label}: evidence cites no ADR, B-row or path:line: {evidence[:60]!r}"]
    for rel, a, b in files:
        text = cached(ev_root, rel)
        top = int(b or a)
        if text is None:
            bad.append(f"{label}: evidence cites {rel}:{a}, which does not exist")
        elif top > line_count(ev_root, rel):
            bad.append(f"{label}: evidence cites {rel}:{top}, but the file has {line_count(ev_root, rel)} lines")
    for n in adrs:
        if not mentions(ev_root, "DECISIONS.md", rf"ADR-0*{int(n)}\b"):
            bad.append(f"{label}: evidence cites ADR-{n}, which is not in DECISIONS.md")
    for n in rows:
        if not mentions(ev_root, "ROADMAP.md", rf"\bB{n}\b"):
            bad.append(f"{label}: evidence cites B{n}, which is not in ROADMAP.md")
    return bad


@functools.lru_cache(maxsize=None)
def line_count(root, rel):
    return (cached(root, rel) or "").count("\n") + 1


@functools.lru_cache(maxsize=None)
def mentions(root, rel, pattern):
    """Does the record file contain the pattern (ROADMAP.md is 1.7 MB: ask once per pattern)."""
    return bool(re.search(pattern, cached(root, rel) or ""))


def judge(root, ev_root):
    """-> (fails [(tag, message)], entries, goldens, invoked). Pure over the tree at `root`."""
    fails = []

    def fail(tag, msg):
        fails.append((tag, msg))

    files = walk(root)
    hits, found = discover(root, files)
    for rid, _k, _s, _why in RULES:
        if not hits[rid]:
            fail("BLIND", f"discovery rule {rid} matched no file: the rule is stale or the tree moved")
    excluded = set()
    for glob, _why in EXCLUDED:
        got = [f for f in found if matches(glob, f)]
        if not got:
            fail("STALE_EXCLUSION", f"exclusion {glob} matches no discovered file; delete it")
        excluded.update(got)
    goldens = [f for f in found if f not in excluded]

    text = read(root, REGISTRY)
    try:
        data = json.loads(text) if text is not None else None
    except ValueError as exc:
        data = None
        fail("MISSING_FIELD", f"{REGISTRY} is not valid JSON: {exc}")
    if data is None or not isinstance(data.get("entries"), list):
        fail("BLIND", f"{REGISTRY} missing or holds no `entries` list")
        return fails, [], goldens, {}
    entries = data["entries"]
    if len(entries) < DISCOVERY_FLOOR:
        fail("BLIND", f"{len(entries)} registry entries, below the floor of {DISCOVERY_FLOOR}")

    inv = invoked(read(root, "verify") or "")
    unwired = unwired_headers(root, files)
    covered = set()
    for i, e in enumerate(entries):
        label = f"entry {e.get('path', '#' + str(i))}" if isinstance(e, dict) else f"entry #{i}"
        if not isinstance(e, dict):
            fail("MISSING_FIELD", f"{label}: not an object")
            continue
        for k in REQUIRED:
            v = e.get(k)
            if k not in e or v is None or (isinstance(v, str) and not v.strip()):
                fail("MISSING_FIELD", f"{label}: field `{k}` missing or empty")
        if not isinstance(e.get("independent"), bool):
            fail("MISSING_FIELD", f"{label}: `independent` must be true or false")
        path = e.get("path")
        if isinstance(path, str) and path:
            got = [f for f in files if matches(path, f)]
            if not got:
                fail("NO_FILE", f"{label}: path matches no file")
            covered.update(got)
        kind = e.get("kind")
        if kind not in KINDS:
            fail("BAD_KIND", f"{label}: kind {kind!r} is not one of {', '.join(KINDS)}")
        evidence = e.get("evidence") if isinstance(e.get("evidence"), str) else ""
        if e.get("independent") is True:
            if kind not in INDEPENDENT_KINDS:
                fail("BAD_INDEPENDENT", f"{label}: independent=true with kind {kind!r}; "
                     f"only {', '.join(INDEPENDENT_KINDS)} may be independent")
            if not evidence.strip():
                fail("BAD_INDEPENDENT", f"{label}: independent=true with empty evidence")
        for c in [e.get("check")] + list(e.get("also_checks") or []):
            if not isinstance(c, str) or not c:
                continue
            if c not in inv:
                fail("UNRUN_CHECK", f"{label}: check `{c}` is not invoked by ./verify")
            elif c in unwired:
                fail("UNRUN_CHECK", f"{label}: check `{c}` carries an UNWIRED header; it does not run")
        if evidence.strip():
            for msg in judge_citations(label, evidence, ev_root):
                fail("BAD_CITATION", msg)
    for g in goldens:
        if g not in covered:
            fail("UNREGISTERED", f"{g}: found by rule, no registry entry covers it "
                 f"(add one to {REGISTRY} with its kind and evidence, never a guess)")
    return fails, entries, goldens, inv


# --- must-fail controls ----------------------------------------------------------------

def build_world(dest, found, entries):
    """A skeletal copy of the inputs: everything a rule found, everything an entry points at,
    ./verify, the registry, h2/README.md and the tools' sources (headers and tokens)."""
    want = set(found) | {"verify", REGISTRY, "h2/README.md"}
    cited = {c for e in entries if isinstance(e, dict) for c in [e.get("check")] + list(e.get("also_checks") or [])}
    for f in walk(ROOT):
        if f.startswith("tools/") and f.endswith(READ_EXT):
            if Path(f).stem in cited:   # the cited checks' own headers (UNWIRED lookup)
                want.add(f)
        elif any(isinstance(e, dict) and isinstance(e.get("path"), str) and matches(e["path"], f)
                 for e in entries):
            want.add(f)
    for f in want:
        src = ROOT / f
        if src.is_file():
            (dest / f).parent.mkdir(parents=True, exist_ok=True)
            # Only text the judge READS is copied; a golden's bytes are never read here, so a
            # placeholder is enough (the f32 renders alone are a megabyte).
            if f in ("verify", REGISTRY, "h2/README.md") or f.endswith(READ_EXT):
                shutil.copy2(src, dest / f)
            else:
                (dest / f).write_bytes(b"")


def rewrite(path, text):
    """The worlds are hardlink copies of one base; unlink first so a plant never edits the base."""
    path.unlink(missing_ok=True)
    path.write_text(text)


def with_registry(world, edit):
    p = world / REGISTRY
    data = json.loads(p.read_text())
    edit(data["entries"])
    rewrite(p, json.dumps(data))


def run_controls(found, entries, real_fails):
    """-> list of (name, ok, detail). Each plant is made in a fresh copy and must add the tag
    it is about; the untouched copy must be no worse than the real tree."""
    def plain(entries_):
        return next(e for e in entries_ if e["independent"] is False)

    def c_new_golden(w):
        (w / "tests/state_fixtures").mkdir(parents=True, exist_ok=True)
        (w / "tests/state_fixtures/zz-planted-golden.f32").write_bytes(b"\0" * 8)

    def c_unrun(w):
        with_registry(w, lambda es: es[0].__setitem__("check", "no_such_check_zz"))

    def c_unwired_header(w):
        # The marker is assembled here, not written out: weakening_check counts the literal
        # text as a not-wired check, and this file is not one. It only plants one in a copy.
        (w / "tools/zz_planted_check.py").write_text("# " + "UNWIRED" + ": planted, header says it does not run\n")
        v = w / "verify"
        rewrite(v, v.read_text() + "\npython3 tools/zz_planted_check.py || ok=1\n")
        # The plant must be INVOKED, so only its header can make it red.
        assert "zz_planted_check" in invoked(v.read_text()), "plant is not invoked: wrong-reason control"
        with_registry(w, lambda es: es[0].__setitem__("check", "zz_planted_check"))

    def c_indep_no_evidence(w):
        def edit(es):
            e = plain(es)
            e["independent"], e["kind"], e["evidence"] = True, "measurement", ""
        with_registry(w, edit)

    def c_indep_parity_kind(w):
        def edit(es):
            e = plain(es)
            e["independent"], e["kind"] = True, "prototype_parity_at_pin"
        with_registry(w, edit)

    def c_no_file(w):
        with_registry(w, lambda es: es[0].__setitem__("path", "tests/zz/matches-nothing.f32"))

    def c_missing_field(w):
        with_registry(w, lambda es: es[0].pop("kind"))

    def c_bad_citation(w):
        with_registry(w, lambda es: es[0].__setitem__("evidence", "tools/golden_pin_check.py:999999"))

    plants = [
        ("a new golden file with no entry", c_new_golden, "UNREGISTERED"),
        ("an entry citing a check ./verify does not run", c_unrun, "UNRUN_CHECK"),
        ("an entry citing a check whose header says UNWIRED", c_unwired_header, "UNRUN_CHECK"),
        ("independent=true with empty evidence", c_indep_no_evidence, "BAD_INDEPENDENT"),
        ("independent=true with kind prototype_parity_at_pin", c_indep_parity_kind, "BAD_INDEPENDENT"),
        ("an entry whose path matches no file", c_no_file, "NO_FILE"),
        ("an entry with a field removed", c_missing_field, "MISSING_FIELD"),
        ("evidence citing a line that does not exist", c_bad_citation, "BAD_CITATION"),
    ]
    results = []
    # Counted, not a set: on a tree that is ALREADY red by some tag, a plant of the same tag
    # must still add one more of it, or the control would read "not red" for the wrong reason.
    def tags(fails):
        return collections.Counter(t for t, _ in fails)
    with tempfile.TemporaryDirectory(prefix="golden-registry-") as tmp:
        base = Path(tmp) / "base"
        base.mkdir()
        build_world(base, found, entries)
        bf = judge(base, ROOT)[0]
        worse = tags(bf) - tags(real_fails)
        detail = "green" if not bf else (f"NEW failures {dict(worse)}: {bf[0][1]}" if worse
                                         else f"same failures as the tree ({dict(tags(bf))})")
        results.append(("the untouched copy is no worse than the tree", not worse, detail))
        for i, (name, plant, tag) in enumerate(plants):
            w = Path(tmp) / f"w{i}"
            shutil.copytree(base, w, copy_function=os.link)
            plant(w)
            added = tags(judge(w, ROOT)[0]) - tags(bf)
            results.append((name, added[tag] > 0, f"red by {tag}" if added[tag] > 0
                            else f"NOT red by {tag} (added: {dict(added) or 'none'})"))
    return results


# --- report ----------------------------------------------------------------------------

def main(argv):
    fails, entries, goldens, inv = judge(ROOT, ROOT)
    found_all = discover(ROOT, walk(ROOT))[1]
    n_ind = sum(1 for e in entries if isinstance(e, dict) and e.get("independent") is True)
    by_kind = {k: [0, 0] for k in KINDS}
    for e in entries:
        if isinstance(e, dict) and e.get("kind") in by_kind:
            by_kind[e["kind"]][0] += 1
            by_kind[e["kind"]][1] += 1 if e.get("independent") is True else 0

    print(f"golden_registry_check: {len(found_all)} files found by {len(RULES)} rules; "
          f"{len(goldens)} after {len(EXCLUDED)} listed exclusions; {len(entries)} registry entries")
    for e in entries:
        if isinstance(e, dict):
            lanes = "+".join(sorted(inv.get(e.get("check"), {"?"})))
            print(f"  {'IND' if e.get('independent') is True else 'pin'}  {str(e.get('kind')):<24} "
                  f"{str(e.get('check')):<28} [{lanes}]  {e.get('path')}")
    print("by kind (entries, of which independent):")
    for k, (n, i) in by_kind.items():
        print(f"  {k:<24} {n:>3}  {i:>3}")

    rc = 0
    for tag, msg in fails:
        print(f"FAIL {tag}: {msg}")
        rc = 1
    for name, ok, detail in run_controls(found_all, entries, fails):
        print(f"{'PASS' if ok else 'FAIL'}  control: {name}: {detail}")
        rc = rc or (0 if ok else 1)
    print(f"Goldens with independent check: {n_ind} of {len(entries)}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
