#!/usr/bin/env python3
"""armor_coverage_check -- every armor row is a running gate or a dated, owned hole
(B448 A1; ADR-197, docs/strategy/blind-spot-armor.md).

WIRED: ./verify fast

  python3 tools/armor_coverage_check.py [--today YYYY-MM-DD]

THE CATALOGUE. docs/armor/catalogue.json holds one row per armor category: the
twelve risk-register rows R1-R12, the seven agent-signature rules S1-S7, and the
security method's four categories SEC-input / SEC-webview / SEC-supply /
SEC-hygiene (B446). Each row is `guarded`, `partial` or `hole`, names the gates
that cover it, and says in prose what is missing. docs/armor/README.md is the
how-to; this docstring is the contract.

RED WHEN:
  (a) a row has no gates and is not a `hole` -- coverage claimed with nothing behind it;
  (b) a `hole` lacks `tracked_by` or `expires`, or has expired (today > expires);
  (c) a listed gate does not run. "Runs" is PARSED, never listed by hand: a
      `python3 tools/X.py`, `"$build_dir/X"`, `node tools/.../X.mjs` or a call to
      a function defined in ./verify or .kit/kit-gates.sh, on a NON-COMMENT line
      of ./verify; or a job id or `- name:` step in .github/workflows/*.yml. A
      mention in prose is not an invocation (./verify explains at length why
      cpu_check is NOT wired). A gate marked {"name": ..., "pending": true} on a
      row that is not `guarded` is REPORTED, not failed: it names a check being
      built in a parallel PR, and the row must not claim it yet;
  (d) a `guarded` row has non-empty `gaps` or any pending gate;
  (e) the file is not valid JSON, or does not hold exactly the 23 ids above, or a
      row's shape is wrong (unknown status, `partial`/`hole` without `tracked_by`,
      a malformed date);
  (f) any row's text contains a name from .leakcheck-names. The file is found the
      way tools/private_name_check.py finds it (its own functions, imported, so
      the two cannot drift), matched case-sensitively like that gate, and a hit
      is never echoed. Absent everywhere -> a WARNING, as that gate prints.

THE DATE. `--today` defaults to the date the tool runs. This is a tool, not the
DSP core: SPEC 5.7's no-wall-clock rule binds the engine. An expired hole turning
verify red on its expiry day is the point -- a hole nobody re-dates is a hole
nobody owns.

MUST-FAIL CONTROLS, every run, on planted catalogues in a temp dir: a missing id,
a gate that is not run, an expired hole, a hole without tracked_by, a guarded row
with gaps, a planted leak name (a synthetic word in a temp names file, never a
real one), plus invalid JSON, a gateless partial row, a pending gate on a guarded
row, and a commented-out invocation the parser must not count. The un-mutated
plant must read GREEN first (the must-read-zero control: an always-red detector
proves nothing). If any control fails to go red, this check is red.

OUTPUT on green: `armor: G/12 green, P partial, H hole` (risk rows only), then the
same counts for S and SEC, then any pending gates.
"""
import datetime
import json
import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import private_name_check as pnc  # noqa: E402  (shared names-file lookup, ADR-014)

CATALOGUE = ROOT / "docs/armor/catalogue.json"
VERIFY = ROOT / "verify"
KIT_GATES = ROOT / ".kit/kit-gates.sh"
WORKFLOWS = ROOT / ".github/workflows"

RISK_IDS = [f"R{i}" for i in range(1, 13)]
SIG_IDS = [f"S{i}" for i in range(1, 8)]
SEC_IDS = ["SEC-input", "SEC-webview", "SEC-supply", "SEC-hygiene"]
ALL_IDS = RISK_IDS + SIG_IDS + SEC_IDS
STATUSES = ("guarded", "partial", "hole")
FIELDS = {"id", "title", "status", "gates", "gaps", "tracked_by", "expires", "tripwire"}
TRACKED_RE = re.compile(r"^B\d+$")

# Invocation shapes on a code line of ./verify. Same shapes test_table_check and
# sanitize_oracles.sh read, so "runs" means one thing across the gates.
INVOKE_RES = [
    re.compile(r"python3 (?:-\S+ )*tools/(?:[A-Za-z0-9_]+/)*([A-Za-z0-9_]+)\.py"),
    re.compile(r'"\$build_dir/([A-Za-z0-9_]+)"'),
    re.compile(r"node (?:\S+ )*tools/(?:[A-Za-z0-9_]+/)*([A-Za-z0-9_]+)\.mjs"),
]
FUNC_DEF_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\(\)\s*\{", re.M)
FUNC_CALL_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\|\|")


def code_part(line):
    """The line minus a whole-line or trailing comment. `\\s#` (not bare `#`) so
    `$#` and `${#x}` survive; an echo string holding ' #' loses only its tail,
    which can hide a mention but never invent an invocation."""
    if line.lstrip().startswith("#"):
        return ""
    return re.split(r"\s#", line, maxsplit=1)[0]


def verify_runs(verify_text, funcs_text=""):
    """Gate names ./verify invokes, parsed from its non-comment lines."""
    funcs = set(FUNC_DEF_RE.findall(verify_text)) | set(FUNC_DEF_RE.findall(funcs_text))
    out = set()
    for line in verify_text.splitlines():
        code = code_part(line)
        if not code.strip():
            continue
        for rx in INVOKE_RES:
            out.update(rx.findall(code))
        m = FUNC_CALL_RE.match(code)
        if m and m.group(1) in funcs:
            out.add(m.group(1))
    return out


def workflow_runs(texts):
    """Job ids (two-space keys under `jobs:`) and `- name:` step names. A regex,
    not a YAML parser: PyYAML is not a dependency here, and these two shapes are
    all the catalogue may cite."""
    out = set()
    for text in texts:
        in_jobs = False
        for line in text.splitlines():
            if re.match(r"^\S", line):
                in_jobs = line.rstrip() == "jobs:"
                continue
            m = re.match(r"^  ([A-Za-z0-9_-]+):\s*$", line)
            if in_jobs and m:
                out.add(m.group(1))
            m = re.match(r"^\s+- name:\s*(.+?)\s*$", line)
            if m:
                out.add(m.group(1).strip("'\""))
    return out


def running_gates():
    v = VERIFY.read_text(encoding="utf-8")
    k = KIT_GATES.read_text(encoding="utf-8") if KIT_GATES.is_file() else ""
    wf = [p.read_text(encoding="utf-8") for p in sorted(WORKFLOWS.glob("*.yml"))]
    return verify_runs(v, k) | workflow_runs(wf)


def load(path):
    """-> (data, error string or None)."""
    try:
        return json.loads(pathlib.Path(path).read_text(encoding="utf-8")), None
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as e:
        return None, f"{path}: not readable as JSON ({type(e).__name__}: {e})"


def gate_entries(gates):
    """-> list of (name, pending) or None when malformed. A gate is a string, or
    an object {"name": str, "pending": true}."""
    if not isinstance(gates, list):
        return None
    out = []
    for g in gates:
        if isinstance(g, str) and g:
            out.append((g, False))
        elif (isinstance(g, dict) and set(g) <= {"name", "pending"}
              and isinstance(g.get("name"), str) and g["name"]
              and g.get("pending", False) in (True, False)):
            out.append((g["name"], bool(g.get("pending", False))))
        else:
            return None
    return out


def _date(s):
    try:
        return datetime.date.fromisoformat(s) if isinstance(s, str) else None
    except ValueError:
        return None


def _row_text(row):
    parts = []
    for v in row.values():
        if isinstance(v, str):
            parts.append(v)
        elif isinstance(v, list):
            parts.extend(g if isinstance(g, str) else str(g.get("name", "")) for g in v
                         if isinstance(g, (str, dict)))
    return "\n".join(parts)


def judge(data, runs, today, leak_re=None):
    """The whole policy, pure. -> (fails, pending). fails is a list of
    (rule letter, message); pending is a list of 'ID gate'. The letters are what
    selftest() asserts, so a control is red for ITS reason, not a bystander's."""
    fails, pending = [], []
    if not isinstance(data, dict) or not isinstance(data.get("rows"), list):
        return [("e", "catalogue is not an object with a `rows` list")], pending
    rows = data["rows"]
    ids = [r.get("id") if isinstance(r, dict) else None for r in rows]
    if sorted(map(str, ids)) != sorted(ALL_IDS) or len(ids) != len(set(ids)):
        missing = [i for i in ALL_IDS if i not in ids]
        extra = sorted({str(i) for i in ids if i not in ALL_IDS})
        dup = sorted({str(i) for i in ids if ids.count(i) > 1})
        fails.append(("e", f"ids must be exactly {len(ALL_IDS)} (R1-R12, S1-S7, SEC-*): "
                           f"missing {missing}, unexpected {extra}, duplicated {dup}"))
    for row in rows:
        if not isinstance(row, dict):
            fails.append(("e", "a row is not an object"))
            continue
        rid = row.get("id", "?")
        unknown = set(row) - FIELDS
        if unknown:
            fails.append(("e", f"{rid}: unknown field(s) {sorted(unknown)}"))
        for f in ("title", "tripwire"):
            if not isinstance(row.get(f), str) or not row[f].strip():
                fails.append(("e", f"{rid}: `{f}` must be non-empty text"))
        if not isinstance(row.get("gaps", ""), str):
            fails.append(("e", f"{rid}: `gaps` must be text"))
        status = row.get("status")
        if status not in STATUSES:
            fails.append(("e", f"{rid}: status {status!r} is not one of {STATUSES}"))
            continue
        gates = gate_entries(row.get("gates"))
        if gates is None:
            fails.append(("e", f"{rid}: `gates` must be a list of names or "
                               '{"name": ..., "pending": true} objects'))
            continue
        tracked = row.get("tracked_by")
        if tracked is not None and not (isinstance(tracked, str) and TRACKED_RE.match(tracked)):
            fails.append(("e", f"{rid}: tracked_by {tracked!r} is not a ROADMAP id like B448"))
        if status == "partial" and not tracked:
            fails.append(("e", f"{rid}: a partial row needs tracked_by"))
        # (a)
        if not gates and status != "hole":
            fails.append(("a", f"{rid}: {status} with no gates — a row with nothing behind it is a hole"))
        # (b)
        if status == "hole":
            exp = _date(row.get("expires"))
            if not tracked:
                fails.append(("b", f"{rid}: a hole needs tracked_by (a ROADMAP id that owns it)"))
            if "expires" not in row:
                fails.append(("b", f"{rid}: a hole needs expires (an ISO date)"))
            elif exp is None:
                fails.append(("b", f"{rid}: expires {row.get('expires')!r} is not an ISO date"))
            elif today > exp:
                fails.append(("b", f"{rid}: hole expired {exp.isoformat()} — close it, or the "
                                   "human re-dates it (docs/armor/README.md)"))
        elif "expires" in row and _date(row["expires"]) is None:
            fails.append(("e", f"{rid}: expires {row['expires']!r} is not an ISO date"))
        # (c) and (d)
        for name, pend in gates:
            if pend:
                if status == "guarded":
                    fails.append(("d", f"{rid}: guarded but gate {name} is pending"))
                else:
                    pending.append(f"{rid} {name}")
            elif name not in runs:
                fails.append(("c", f"{rid}: gate {name!r} is not run by ./verify or any workflow"))
        if status == "guarded" and row.get("gaps", "").strip():
            fails.append(("d", f"{rid}: guarded but gaps is not empty — say partial, or close the gap"))
        # (f)
        if leak_re is not None and leak_re.search(_row_text(row)):
            fails.append(("f", f"{rid}: holds a name from {pnc.NAMES} (not echoed; alias it, ADR-014)"))
    return fails, pending


def counts(rows, ids):
    st = [r.get("status") for r in rows if isinstance(r, dict) and r.get("id") in ids]
    return st.count("guarded"), st.count("partial"), st.count("hole")


def summary(rows):
    g, p, h = counts(rows, RISK_IDS)
    sg, sp, sh = counts(rows, SIG_IDS)
    cg, cp, ch = counts(rows, SEC_IDS)
    return (f"armor: {g}/12 green, {p} partial, {h} hole",
            f"armor: signatures {sg}/7 green, {sp} partial, {sh} hole; "
            f"security {cg}/4 green, {cp} partial, {ch} hole")


def leak_regex(names_path):
    """-> (compiled regex or None, error or None). The names file's own join
    (pnc.pattern), compiled with Python's re: the names are plain words, and a
    pattern Python cannot compile is red, never a silent no-match."""
    pat = pnc.pattern(names_path)
    if not pat:
        return None, None
    try:
        return re.compile(f"(?:{pat})"), None
    except re.error as e:
        return None, f"{pnc.NAMES} does not compile as a regex ({e})"


def _plant_base():
    """A catalogue that must read GREEN against runs={'ctl_gate'}: every id, one
    of each status, a pending gate where pending is legal."""
    rows = []
    for rid in ALL_IDS:
        rows.append({"id": rid, "title": "t", "status": "partial", "gates": ["ctl_gate"],
                     "gaps": "g", "tracked_by": "B0", "tripwire": "w"})
    rows[0].update(status="guarded", gaps="")
    del rows[0]["tracked_by"]                                                  # R1
    rows[12].update(status="hole", gates=[], expires="2030-01-01")          # S1
    rows[14]["gates"] = ["ctl_gate", {"name": "ctl_future", "pending": True}]  # S3
    return {"version": 1, "rows": rows}


def selftest():
    """-> list of (control, verdict) and a failure string or None."""
    fake = "Zq" + "ArmorPlantWord"      # synthetic; split so no scan matches this file
    today = datetime.date(2026, 10, 8)
    runs = {"ctl_gate"}
    results = []

    def byid(d, rid):
        return next(r for r in d["rows"] if r["id"] == rid)

    def m_missing(d): d["rows"] = [r for r in d["rows"] if r["id"] != "R12"]
    def m_notrun(d): byid(d, "R2")["gates"].append("ctl_never_run")
    def m_expired(d): byid(d, "S1")["expires"] = "2026-10-07"
    def m_untracked(d): del byid(d, "S1")["tracked_by"]
    def m_gaps(d): byid(d, "R1")["gaps"] = "something is missing"
    def m_leak(d): byid(d, "R3")["title"] = f"t {fake} t"
    def m_gateless(d): byid(d, "R4")["gates"] = []
    def m_pend_guarded(d): byid(d, "R1")["gates"].append({"name": "ctl_gate2", "pending": True})

    controls = [("missing id", m_missing, "e"), ("gate not run", m_notrun, "c"),
                ("expired hole", m_expired, "b"), ("hole without tracked_by", m_untracked, "b"),
                ("guarded with gaps", m_gaps, "d"), ("planted leak name", m_leak, "f"),
                ("partial with no gates", m_gateless, "a"),
                ("pending gate on guarded", m_pend_guarded, "d"),
                ("invalid JSON", None, "e")]
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        names = td / "names"
        names.write_text(f"# synthetic control word\n\n{fake}\n", encoding="utf-8")
        leak_re, err = leak_regex(names)
        if err or leak_re is None:
            return results, f"control names file did not compile ({err})"

        def run(doc, raw=None):
            p = td / "catalogue.json"
            p.write_text(raw if raw is not None else json.dumps(doc), encoding="utf-8")
            data, lerr = load(p)
            if lerr:
                return [("e", lerr)]
            return judge(data, runs, today, leak_re)[0]

        base = run(_plant_base())
        results.append(("unplanted catalogue reads green", "green" if not base else "RED"))
        if base:
            return results, f"the unplanted control catalogue read red: {base[0][1]}"
        for label, mut, letter in controls:
            if mut is None:
                fails = run(None, raw='{"rows": [ ')
            else:
                doc = _plant_base()
                mut(doc)
                fails = run(doc)
            red = any(code == letter for code, _ in fails)
            results.append((label, "red" if red else "GREEN"))
            if not red:
                return results, f"control '{label}' did not read red on rule ({letter})"
            if any(fake in msg for _, msg in fails):
                return results, "a leak hit echoed the name it found"

    # The parser must count an invocation and refuse a mention.
    v = ("ctl_fn() {\n  :\n}\nfast() {\n  python3 tools/ctl_real.py || ok=1\n"
         "  # python3 tools/ctl_commented.py || ok=1\n  ctl_fn || ok=1\n"
         '  "$build_dir/ctl_bin" || return 1   # python3 tools/ctl_trailing.py\n}\n')
    got = verify_runs(v)
    want = {"ctl_real", "ctl_fn", "ctl_bin"}
    parse_ok = got == want
    results.append(("parser: invocation counted, mention refused", "ok" if parse_ok else "WRONG"))
    if not parse_ok:
        return results, f"verify parser read {sorted(got)}, expected {sorted(want)}"
    wf = workflow_runs(["on:\n  push:\njobs:\n  ctl-job:\n    steps:\n"
                        "      - name: ctl step\n        with:\n          name: not-a-step\n"])
    if wf != {"ctl-job", "ctl step"}:
        return results, f"workflow parser read {sorted(wf)}"
    return results, None


def main(argv):
    today = datetime.date.today()
    if len(argv) == 3 and argv[1] == "--today":
        today = _date(argv[2])
        if today is None:
            print(f"armor_coverage_check: --today {argv[2]!r} is not an ISO date", file=sys.stderr)
            return 2
    elif len(argv) != 1:
        print("usage: armor_coverage_check.py [--today YYYY-MM-DD]", file=sys.stderr)
        return 2

    results, bad = selftest()
    if bad:
        print(f"armor_coverage_check: FAILED — control: {bad}", file=sys.stderr)
        for label, verdict in results:
            print(f"    {label}: {verdict}", file=sys.stderr)
        return 1

    leak_re = None
    nf = pnc.names_file(ROOT)
    if nf is None:
        print(f"verify: WARNING — {pnc.NAMES} not found here or in the main checkout: the "
              "armor catalogue's private-name rule (f) DID NOT RUN.", file=sys.stderr)
    else:
        leak_re, err = leak_regex(nf)
        if err:
            print(f"armor_coverage_check: FAILED — {err}", file=sys.stderr)
            return 1

    data, err = load(CATALOGUE)
    if err:
        print(f"armor_coverage_check: FAILED — (e) {err}", file=sys.stderr)
        return 1
    fails, pending = judge(data, running_gates(), today, leak_re)
    if fails:
        print(f"armor_coverage_check: FAILED ({len(fails)}) — docs/armor/README.md says how to fix:",
              file=sys.stderr)
        for code, msg in fails:
            print(f"    ({code}) {msg}", file=sys.stderr)
        return 1
    for line in summary(data["rows"]):
        print(line)
    if pending:
        print(f"armor: pending gates, reported not failed: {', '.join(pending)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
