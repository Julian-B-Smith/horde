#!/usr/bin/env python3
"""tolerance_registry_check -- every comparison tolerance is pinned; any change is red.

WIRED: ./verify fast

WHY (B448 / ADR-197, armor edit G, "Tolerance creep"). Comparison tolerances widen
a little per PR until the test means nothing, and no oracle can see it: the widened
test still passes. The tolerances stay where they live; docs/armor/tolerances.json
pins their VALUES, and this check re-extracts each one and fails on any difference.
A changed tolerance is a gate-weakening event (CLAUDE.md "Never weaken a gate").

WHAT IS INVENTORIED (four sources, see `--list`):
  (a) specs/ACCEPTANCE.md  prose thresholds: a number introduced by +-, >=, <=, <, >,
      "eps =", an "in (lo, hi)" interval or an "a-b s" range. Keyed by section + the
      words before the number, so a reword is a red locator-miss, not a silent pass.
  (b) tools/**/*.py        constants whose NAME is a tolerance word (eps, epsilon,
      tol, tolerance, thresh, threshold, margin, slack, atol, rtol, max_err, limit,
      bound), kwargs such as rel_tol=1e-9, and name-vs-literal comparisons.
  (c) C++ checks           the same, plus EXPECT_NEAR/ASSERT_NEAR third arguments.
      The checks live in tools/**/*.cpp,*.h (src/ and h2/ hold only cores; a src/ or
      h2/ file whose basename says test/probe/check would be scanned too).
  (d) verify               NAME=number assignments and --tol-ish=number flags.
  PLUS, in (b) and (c), one `inline-float-compare` entry per file: the NON-ZERO FLOAT
  literals in comparison position (`x >= 0.95`, `fabs(a-b) < 1e-6`). The checks here
  mostly write their thresholds inline, not as named constants, so a name-only
  inventory would miss the very thresholds that creep. The group is over-inclusive on
  purpose (float loop bounds and clamps are in it); a human reads the value list.

NAME MATCH is by identifier PART (kEps, k_eps, EPS, kRmsTol are tolerances; `reps` and
`stolen` are not). That is a deliberate tightening of the substring regex in the
brief: substring matching read `reps` as `eps`.

KNOWN BLIND SPOTS (stated, not hidden): integer thresholds written inline
(`count >= 3`), thresholds computed from other names (`2 * kEps`), literals the lexer
cannot see inside macros, and tolerances in tools/**/*.mjs and docs/**/*.js (the JS
labs; out of this check's brief). A tolerance written as an unnamed integer is not
seen. This check raises the cost of creep; it is not a proof of absence.

RULE. Per entry, re-extract the value with the entry's `locator` (a regex, group 1
is the value; all matches in the file are joined with " | "). A different value or a
locator that no longer matches is red. A candidate found by the scan that is neither
registered nor in `excluded` is red. Both print: "tolerance change is a
gate-weakening event -- needs the human's approval recorded as `approved: <ref>`".

APPROVING. `--approve <id> <ref>` re-reads the entry's current value and records
`approved: <ref>`; <ref> is required and non-empty. It also registers a NEW candidate
by its id (printed by the red message). It cannot repair a locator that no longer
matches: edit the JSON locator by hand. The ref must be a HUMAN decision (a
ROADMAP/ADR id); an agent that approves its own widening has defeated the check.
Registry edits (governs/ref wording) never affect the verdict; only value, locator
and file do.

MUST-FAIL CONTROLS (LIBRARY L0032), every run, in a scratch copy: a changed
ACCEPTANCE value, a widened tools constant, a changed EXPECT_NEAR, a new unregistered
`TOL = 0.1`, a deleted locator target, a new `--tol=` flag in verify, and an
`--approve` with an empty ref (refused). A positive control (the untouched scratch
reads green) keeps the judge from being always-red. Any control that does not go the
right way makes this check red.
"""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = "docs/armor/tolerances.json"
PROSE_FILES = ["specs/ACCEPTANCE.md"]
SHELL_FILES = ["verify"]
SELF = "tools/tolerance_registry_check.py"  # its own regexes would read as candidates
BASELINE = "baseline 2026-10-08"
GATE_MSG = ("tolerance change is a gate-weakening event — needs the human's approval "
            "recorded as `approved: <ref>`")

# ---- name test --------------------------------------------------------------
TOL_PARTS = {"eps", "epsilon", "tol", "tols", "tolerance", "tolerances", "thresh",
             "threshold", "thresholds", "margin", "slack", "atol", "rtol", "maxerr",
             "limit", "bound", "bounds"}


def name_parts(ident):
    return [p.lower() for p in re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])|\d+", ident)]


def is_tol_name(ident):
    parts = name_parts(ident)
    if any(p in TOL_PARTS for p in parts):
        return True
    return any(a == "max" and b == "err" for a, b in zip(parts, parts[1:]))


# ---- regex building blocks --------------------------------------------------
NUM = r"[-+]?(?:\d[\d_.]*(?:[eE][-+]?\d+)?[fFuUlL]*|0[xX][0-9a-fA-F]+)"
EXPR = NUM + r"(?:\s*[-+*/]\s*\(?\s*" + NUM + r"\)?)*"
# Non-zero float: a literal that is exactly zero (0, 0.0, .0, 0e0) is a sign test, not
# a threshold. `0.05` survives because the zero-run must reach a non-word end.
FLOAT_NZ = (r"-?(?!0*(?:\.0*)?(?:[eE][-+]?\d+)?[fF]?(?![\w.]))"
            r"(?:\d+\.\d*|\.\d+|\d+(?=[eE]))(?:[eE][-+]?\d+)?[fF]?")
NUM_NZ = r"[-+]?(?!0*(?:\.0*)?(?:[eE][-+]?\d+)?[fFuUlL]*(?![\w.]))" + NUM[len("[-+]?"):]
OP = r"(?:<=|>=|<|>)(?![<>=])"
TERM = r"(?=\s*(?:[;,)\]}]|$))"
ANNOT = r"(?:\s*:\s*[\w\[\], .]+?)?"
ASSIGN_TAIL = ANNOT + r"\s*=(?!=)\s*(" + EXPR + r")" + TERM
ARG = r"(?:[^(),;]|\((?:[^()]|\([^()]*\))*\))+"
CALL_MACROS = ("EXPECT_NEAR", "ASSERT_NEAR")

ASSIGN_G = re.compile(r"(?<![\w])([A-Za-z_]\w*)" + ASSIGN_TAIL, re.M)
CMP_G = re.compile(r"(?<![\w])([A-Za-z_]\w*)\s*" + OP + r"\s*(" + NUM_NZ + r")(?![\w.])", re.M)
FLAG_G = re.compile(r"(?<![\w-])--([A-Za-z][\w-]*)[= ]\s*(" + NUM + r")(?![\w.])", re.M)
INLINE_LOC = (r"((?<![<>=!\-])" + OP + r"\s*" + FLOAT_NZ + r"(?![\w.])"
              r"|(?<![\w.])" + FLOAT_NZ + r"\s*(?:<=|>=|<|>)(?![<>=]))")

PROSE_TOKEN = (r"(?:±|≥|≤|>=|<=|ε\s*=|(?<![-<!\w])[<>](?![=>\-])"
               r"|\b(?:below|above|within)\s+~?)"
               r"\s*[−-]?\d+(?:\.\d+)?(?:[eE][−-]?\d+)?"
               r"(?:\s?(?:%|×|dB|¢|ms|Hz|s\b))?"
               r"|(?<=\bin )\(\s*\d+(?:\.\d+)?\s*,\s*\d+(?:\.\d+)?\s*\)"
               r"|\d+–\d+ s\b")
PROSE_RE = re.compile(PROSE_TOKEN)
REF_RE = re.compile(r"ADR-\d+[a-z]?|L0-\d+|\bB\d{2,3}\b")


def locator_assign(sym):
    return r"(?<![\w])" + re.escape(sym) + ASSIGN_TAIL


def locator_cmp(sym):
    return (r"(?<![\w])" + re.escape(sym) + r"\s*(" + OP + r"\s*" + NUM_NZ + r")(?![\w.])")


def locator_flag(name):
    return r"(?<![\w-])--" + re.escape(name) + r"[= ]\s*(" + NUM + r")(?![\w.])"


def locator_call(macro):
    return (r"\b" + macro + r"\s*\(\s*" + ARG + r",\s*" + ARG + r",\s*(" + EXPR
            + r")\s*\)")


# ---- lexing: blank comments and string contents, keep offsets ----------------
def lang_of(rel):
    if rel in SHELL_FILES:
        return "sh"
    if rel in PROSE_FILES:
        return "prose"
    return "py" if rel.endswith(".py") else "c"


def blank(text, lang):
    """Same length as `text`, with comments and string/char literal contents turned
    into spaces (newlines kept), so a threshold in a label or a comment is never read
    as code. `prose` is returned untouched."""
    if lang == "prose":
        return text
    out, n, i = list(text), len(text), 0

    def wipe(a, b):
        for k in range(a, b):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        ch, two = text[i], text[i:i + 2]
        if lang == "c" and two == "//" or lang == "py" and ch == "#":
            j = text.find("\n", i)
            j = n if j < 0 else j
            wipe(i, j)
            i = j
        elif lang == "c" and two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            wipe(i, j)
            i = j
        elif lang == "sh":
            if ch == "#" and (i == 0 or text[i - 1] in " \t\n;|&("):  # not $# or ${#
                j = text.find("\n", i)
                j = n if j < 0 else j
                wipe(i, j)
                i = j
            elif ch in "'\"":  # skip quoted text so a '#' inside it is not a comment
                j = i + 1
                while j < n and text[j] != "\n" and (text[j] != ch or text[j - 1] == "\\"):
                    j += 1
                i = j + 1
            else:
                i += 1
        elif ch == '"' and lang == "c" and i and text[i - 1] == "R":
            m = re.match(r'"([^()\\ ]{0,16})\(', text[i:])
            end = text.find(")" + (m.group(1) if m else "") + '"', i) if m else -1
            j = n if end < 0 else end + len(m.group(1)) + 2
            wipe(i, j)
            i = j
        elif ch in "'\"":
            if ch == "'" and lang == "c" and i and text[i - 1].isalnum():
                i += 1  # C++14 digit separator (1'000), not a char literal
                continue
            q = ch * 3 if lang == "py" and text[i:i + 3] == ch * 3 else ch
            j = i + len(q)
            while j < n:
                if text[j] == "\\":
                    j += 2
                elif text.startswith(q, j):
                    j += len(q)
                    break
                elif text[j] == "\n" and len(q) == 1:
                    break
                else:
                    j += 1
            wipe(i, min(j, n))
            i = j
        else:
            i += 1
    return "".join(out)


# ---- which files, and how to read a value ------------------------------------
C_SUFFIX = (".cpp", ".cc", ".h", ".hpp", ".mm")


def scan_files(root):
    """Files whose tolerances are inventoried, repo-relative. Untracked-but-not-ignored
    files count, so a new check is seen before it is committed. Without a git tree
    (the scratch copy) every file under root counts."""
    if (root / ".git").exists():
        r = subprocess.run(["git", "ls-files", "-z", "--cached", "--others",
                            "--exclude-standard"], cwd=root, capture_output=True, text=True)
        names = [n for n in r.stdout.split("\0") if n]
    else:
        names = [str(p.relative_to(root)) for p in root.rglob("*") if p.is_file()]
    keep = []
    for rel in sorted(set(names)):
        if not (root / rel).is_file() or rel == SELF:
            continue
        base = rel.rsplit("/", 1)[-1]
        if rel in PROSE_FILES or rel in SHELL_FILES:
            keep.append(rel)
        elif rel.startswith("tools/") and (rel.endswith(".py") or rel.endswith(C_SUFFIX)):
            keep.append(rel)
        elif rel.startswith(("src/", "h2/")) and rel.endswith(C_SUFFIX) \
                and re.search(r"(?i)test|probe|check", base):
            keep.append(rel)
    return keep


def norm(v, rel):
    """Whitespace-insensitive for comparisons (`>= 0.95` == `>=0.95`), single-spaced
    for everything else. Otherwise the value is exactly as written."""
    if lang_of(rel) != "prose" and v[:1] in "<>":
        return re.sub(r"\s+", "", v)
    return " ".join(v.split())


def extract(text, rel, locator):
    """-> (values, 1-based line numbers) for every match of `locator` (group 1)."""
    body = blank(text, lang_of(rel))
    vals, lines = [], []
    for m in re.finditer(locator, body, re.M):
        vals.append(norm(m.group(1), rel))
        lines.append(body.count("\n", 0, m.start(1)) + 1)
    return vals, lines


def make_locator(kind, sym):
    return {"assign": lambda: locator_assign(sym), "cmp": lambda: locator_cmp(sym),
            "flag": lambda: locator_flag(sym), "call": lambda: locator_call(sym),
            "inline": lambda: INLINE_LOC}[kind]()


def make_id(rel, kind, sym):
    return {"assign": f"{rel}::{sym}", "cmp": f"{rel}::{sym}[cmp]", "flag": f"{rel}::--{sym}",
            "call": f"{rel}::{sym}", "inline": f"{rel}::inline-float-compare"}[kind]


def discover_code(rel, text):
    """-> [(kind, sym)] groups of candidate tolerances in one code file."""
    lang, found = lang_of(rel), []
    body = blank(text, lang)

    def add(kind, sym):
        if (kind, sym) not in found:
            found.append((kind, sym))

    for m in ASSIGN_G.finditer(body):
        if is_tol_name(m.group(1)):
            add("assign", m.group(1))
    if lang == "sh":
        for m in FLAG_G.finditer(body):
            if is_tol_name(m.group(1)):
                add("flag", m.group(1))
        return found
    for m in CMP_G.finditer(body):
        if is_tol_name(m.group(1)):
            add("cmp", m.group(1))
    if lang == "c":
        for macro in CALL_MACROS:
            if re.search(locator_call(macro), body):
                add("call", macro)
    if re.search(INLINE_LOC, body):
        add("inline", None)
    return found


def left_regex(left):
    return "".join(r"\s+" if t.isspace() else re.escape(t) for t in re.split(r"(\s+)", left) if t)


def discover_prose(rel, text):
    """-> [dict(id, locator, sec, title, left, token, line)] one per distinct locator.
    The locator is the words before the token (extended until unique) + the token, so
    it keys on what the number MEANS, not on its position."""
    heads = [(m.start(), m.group(1), re.sub(r"^[^·]*·\s*", "", m.group(0)[2:].strip()))
             for m in re.finditer(r"^##\s+(\S+)[^\n]*", text, re.M)]
    out, by_loc, used = [], {}, {}
    for m in PROSE_RE.finditer(text):
        s = m.start()
        ls = text.rfind("\n", 0, s) + 1
        for width in (28, 60, 120, 10 ** 6):
            a = max(ls, s - width)
            left = text[a:s]
            if a > ls and " " in left:
                left = left[left.find(" ") + 1:]
            rx = ("^" if a == ls else "") + left_regex(left) + "(" + PROSE_TOKEN + ")"
            if len(list(re.finditer(rx, text, re.M))) <= 1:
                break
        if rx in by_loc:
            continue
        sec, title = "preamble", ""
        for pos, hs, ht in heads:
            if pos < s:
                sec, title = hs, ht
        slug = re.sub(r"[^\w.%¢=+≥≤±-]+", "_", left.strip()[-24:]).strip("_")
        base = f"{rel}::{sec}/{slug or 'line-start'}"
        used[base] = used.get(base, 0) + 1
        cid = base if used[base] == 1 else f"{base}#{used[base]}"
        c = dict(id=cid, locator=rx, sec=sec, title=title, left=left.strip()[-40:],
                 token=m.group(0), line=text.count("\n", 0, s) + 1)
        by_loc[rx] = c
        out.append(c)
    return out


# ---- registry entries ---------------------------------------------------------
def attribution(text, lines):
    """ADR / ACCEPTANCE-row / ROADMAP ids named on the matched lines or in the comment
    block directly above them. Evidence, not inference: nothing found -> unattributed."""
    raw, refs = text.splitlines(), []
    for ln in lines:
        k = ln - 1
        block = [raw[k]] if 0 <= k < len(raw) else []
        j = k - 1
        while j >= 0 and k - j <= 3 and raw[j].strip().startswith(("//", "/*", "*", "#")):
            block.append(raw[j])
            j -= 1
        refs += REF_RE.findall(" ".join(block))
    return ", ".join(sorted(set(refs))) or "unattributed"


def governs_hint(text, lines, default):
    raw = text.splitlines()
    k = lines[0] - 1 if lines else -1
    if 0 <= k < len(raw):
        m = re.search(r"(?://|#)\s*(\S.*)$", raw[k])
        if m:
            return m.group(1).strip()[:100]
        if k and raw[k - 1].strip().startswith(("//", "#")):
            return raw[k - 1].strip().lstrip("/# ").strip()[:100] or default
    return default


def code_entry(root, rel, kind, sym):
    text = (root / rel).read_text(encoding="utf-8", errors="replace")
    loc = make_locator(kind, sym)
    vals, lines = extract(text, rel, loc)
    default = {"assign": f"named tolerance constant `{sym}`",
               "cmp": f"`{sym}` compared with a literal threshold",
               "flag": f"numeric `--{sym}` flag passed by verify",
               "call": f"third argument (the tolerance) of {sym}",
               "inline": "non-zero float literals in comparisons (over-inclusive: also "
                         "float loop bounds and clamps)"}[kind]
    return dict(id=make_id(rel, kind, sym), file=rel, locator=loc, value=" | ".join(vals),
                governs=governs_hint(text, lines, default) if kind != "inline" else default,
                ref=attribution(text, lines), approved=BASELINE)


def prose_entry(root, rel, c):
    text = (root / rel).read_text(encoding="utf-8")
    vals, lines = extract(text, rel, c["locator"])
    raw = text.splitlines()[c["line"] - 1]
    adrs = sorted(set(re.findall(r"ADR-\d+[a-z]?", raw)))
    return dict(id=c["id"], file=rel, locator=c["locator"], value=" | ".join(vals),
                governs=f"{c['sec']} {c['title']}: ...{c['left']} {c['token']}"[:140],
                ref=", ".join([f"{rel} {c['sec']}"] + adrs), approved=BASELINE)


def all_candidates(root):
    """-> {id: (file, locator, builder)} for every candidate the scan finds."""
    out = {}
    for rel in scan_files(root):
        text = (root / rel).read_text(encoding="utf-8", errors="replace")
        if lang_of(rel) == "prose":
            for c in discover_prose(rel, text):
                out[c["id"]] = (rel, c["locator"], lambda c=c, rel=rel: prose_entry(root, rel, c))
        else:
            for kind, sym in discover_code(rel, text):
                out[make_id(rel, kind, sym)] = (
                    rel, make_locator(kind, sym),
                    lambda rel=rel, kind=kind, sym=sym: code_entry(root, rel, kind, sym))
    return out


def build_registry(root, excluded=()):
    """Baseline registry from the scan. `excluded` is [{id, reason}]: candidates a human
    judged not to be tolerances, carried into the registry's `excluded` list."""
    cands, skip = all_candidates(root), {x["id"]: x for x in excluded}
    entries = [b() for i, (f, l, b) in sorted(cands.items()) if i not in skip]
    ex = [dict(id=i, file=cands[i][0], locator=cands[i][1], reason=skip[i]["reason"])
          for i in sorted(skip) if i in cands]
    return dict(schema=1, entries=entries, excluded=ex)


def load_registry(root):
    p = root / REGISTRY
    return json.loads(p.read_text(encoding="utf-8")) if p.is_file() else None


def save_registry(root, reg):
    reg["entries"] = sorted(reg["entries"], key=lambda e: e["id"])
    reg["excluded"] = sorted(reg["excluded"], key=lambda e: e["id"])
    p = root / REGISTRY
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(reg, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
                 encoding="utf-8")


# ---- the judge ------------------------------------------------------------------
REQUIRED = ("id", "file", "locator", "value", "governs", "ref", "approved")


def judge(root, reg):
    """-> (failures, stats). Pure over (tree, registry): every registered value
    re-extracted, every scan candidate covered by an entry or an exclusion."""
    fails, entries, excluded = [], reg.get("entries", []), reg.get("excluded", [])
    seen = set()
    for e in entries:
        missing = [k for k in REQUIRED if not str(e.get(k, "")).strip() and k != "value"]
        if missing or "value" not in e:
            fails.append(f"{e.get('id', '?')}: registry entry lacks {missing or ['value']}")
            continue
        if e["id"] in seen:
            fails.append(f"{e['id']}: registered twice")
        seen.add(e["id"])
        path = root / e["file"]
        if not path.is_file():
            fails.append(f"{e['id']}: file {e['file']} is gone (registered value {e['value']!r})")
            continue
        vals, _ = extract(path.read_text(encoding="utf-8", errors="replace"), e["file"], e["locator"])
        now = " | ".join(vals)
        if not vals:
            fails.append(f"{e['id']}: locator no longer matches in {e['file']} "
                         f"(registered value {e['value']!r}; deleted or reworded)")
        elif now != e["value"]:
            fails.append(f"{e['id']}: changed {e['value']!r} -> {now!r}")
    for x in excluded:
        if "value" in x:
            p = root / x["file"]
            vals, _ = extract(p.read_text(encoding="utf-8", errors="replace"), x["file"],
                              x["locator"]) if p.is_file() else ([], [])
            if " | ".join(vals) != x["value"]:
                fails.append(f"{x['id']}: excluded value pinned as {x['value']!r}, now {' | '.join(vals)!r}")
    covered = {(e["file"], e["locator"]) for e in entries} | {(x["file"], x["locator"]) for x in excluded}
    for cid, (rel, loc, build) in sorted(all_candidates(root).items()):
        if (rel, loc) not in covered:
            fails.append(f"{cid}: new tolerance candidate {build()['value']!r} is neither registered "
                         f"nor excluded (register it: --approve {cid} <ref>)")
    return fails, dict(n=len(entries), unattributed=sum(e.get("ref") == "unattributed" for e in entries),
                       excluded=len(excluded))


def approve(root, reg, cid, ref):
    """Record a human decision: re-read `cid`'s current value and set approved=ref.
    Registers a new candidate by id. -> error string or None (reg is edited in place)."""
    if not ref or not ref.strip():
        return "refused: --approve needs a non-empty <ref> (ADR / ROADMAP id of the human's decision)"
    for e in reg["entries"]:
        if e["id"] == cid:
            vals, _ = extract((root / e["file"]).read_text(encoding="utf-8"), e["file"], e["locator"])
            if not vals:
                return f"refused: {cid}'s locator matches nothing; edit the locator in {REGISTRY} by hand"
            e["value"], e["approved"] = " | ".join(vals), ref.strip()
            return None
    cands = all_candidates(root)
    if cid not in cands:
        return f"refused: no registered entry or scan candidate has id {cid}"
    entry = cands[cid][2]()
    entry["approved"] = ref.strip()
    reg["entries"].append(entry)
    return None


SOURCES = (("a", "specs/ACCEPTANCE.md prose"), ("b", "tools/**/*.py"),
           ("c", "C++ checks (tools/**/*.cpp,*.h)"), ("d", "verify"))


def source_of(rel):
    return "a" if rel in PROSE_FILES else "d" if rel in SHELL_FILES else "b" if rel.endswith(".py") else "c"


def list_inventory(reg):
    counts = {k: 0 for k, _ in SOURCES}
    for e in reg["entries"]:
        counts[source_of(e["file"])] += 1
        print(f"{e['id']}\t{e['value']}\t{e['ref']}")
    print("-- excluded:")
    for x in reg["excluded"]:
        print(f"{x['id']}\t{x['reason']}")
    by = ", ".join(f"({k}) {counts[k]} {name}" for k, name in SOURCES)
    print(f"-- {len(reg['entries'])} registered: {by}; {len(reg['excluded'])} excluded; "
          f"{sum(e['ref'] == 'unattributed' for e in reg['entries'])} unattributed")


# ---- must-fail controls (scratch copy, never the repo's own files) ----------------
SCRATCH_PY = "TOL = 1e-6\n\n\ndef close(a, b):\n    return abs(a - b) < 1e-9\n"
SCRATCH_CPP = ("constexpr double kEps = 1e-6;\n"
               "void t() { EXPECT_NEAR(a, b, 1e-3); if (r >= 0.95) {} }\n")
SCRATCH_SH = "ok=0\nrun_check --seconds=60\nSLACK_MAX=3\n"


def _scratch(tmp):
    root = Path(tmp)
    for rel, text in {"specs/ACCEPTANCE.md": (ROOT / "specs/ACCEPTANCE.md").read_text(encoding="utf-8"),
                      "tools/x_check.py": SCRATCH_PY, "tools/x_check.cpp": SCRATCH_CPP,
                      "verify": SCRATCH_SH}.items():
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text(text, encoding="utf-8")
    return root


def _edit(root, rel, old, new):
    p = root / rel
    t = p.read_text(encoding="utf-8")
    if old not in t:
        raise AssertionError(f"scratch edit target {old!r} not in {rel}")
    p.write_text(t.replace(old, new, 1), encoding="utf-8")


def controls():
    """-> (error or None, [(name, fired)]). Each mutation must read red with the named
    id in the failure; the untouched scratch must read green."""
    results = []

    def run(name, mutate, expect):
        with tempfile.TemporaryDirectory() as tmp:
            root = _scratch(tmp)
            reg = build_registry(root)
            kinds = {source_of(e["file"]) for e in reg["entries"]}
            if kinds != {"a", "b", "c", "d"}:
                return f"scratch registry covers sources {sorted(kinds)}, not a-d: the controls are blind"
            mutate(root)
            fails, _ = judge(root, reg)
            if expect is None:
                ok = not fails
            else:
                ok = any(expect in f for f in fails)
            results.append((name, ok))
            if not ok:
                return f"control '{name}' did not go the right way (failures: {fails[:3]})"
        return None

    def new_file(root):
        (root / "tools/new_check.py").write_text("TOL = 0.1\n", encoding="utf-8")

    for name, mutate, expect in (
            ("untouched scratch reads green (positive)", lambda r: None, None),
            ("ACCEPTANCE value edited", lambda r: _edit(r, "specs/ACCEPTANCE.md", "±0.08", "±0.18"),
             "specs/ACCEPTANCE.md::L0-2"),
            ("tools constant widened 1e-6 -> 1e-5", lambda r: _edit(r, "tools/x_check.py", "TOL = 1e-6", "TOL = 1e-5"),
             "tools/x_check.py::TOL: changed"),
            ("EXPECT_NEAR tolerance changed", lambda r: _edit(r, "tools/x_check.cpp", "b, 1e-3)", "b, 1e-2)"),
             "tools/x_check.cpp::EXPECT_NEAR: changed"),
            ("inline comparison threshold loosened", lambda r: _edit(r, "tools/x_check.cpp", ">= 0.95", ">= 0.9"),
             "inline-float-compare: changed"),
            ("new unregistered TOL = 0.1", new_file, "tools/new_check.py::TOL: new tolerance candidate"),
            ("locator target deleted", lambda r: _edit(r, "tools/x_check.cpp", "constexpr double kEps = 1e-6;\n", ""),
             "tools/x_check.cpp::kEps: locator no longer matches"),
            ("new --tol flag in verify", lambda r: _edit(r, "verify", "ok=0\n", "ok=0\nrun_x --tol=1e-6\n"),
             "verify::--tol: new tolerance candidate")):
        err = run(name, mutate, expect)
        if err:
            return err, results
    # --approve: an empty ref is refused; a real ref re-baselines exactly that entry.
    with tempfile.TemporaryDirectory() as tmp:
        root = _scratch(tmp)
        reg = build_registry(root)
        _edit(root, "tools/x_check.py", "TOL = 1e-6", "TOL = 1e-5")
        for bad in ("", "   ", None):
            fired = approve(root, reg, "tools/x_check.py::TOL", bad) is not None
            results.append((f"--approve with ref {bad!r} refused", fired))
            if not fired:
                return f"--approve accepted an empty ref {bad!r}", results
        if not judge(root, reg)[0]:
            return "refused approvals still changed the verdict", results
        err = approve(root, reg, "tools/x_check.py::TOL", "ADR-999")
        green = err is None and not judge(root, reg)[0]
        results.append(("--approve with a ref re-baselines (positive)", green))
        if not green:
            return f"--approve with a ref did not re-baseline: {err or judge(root, reg)[0][:2]}", results
    return None, results


def main(argv):
    if "--approve" in argv:
        i = argv.index("--approve")
        reg = load_registry(ROOT)
        err = approve(ROOT, reg, *(argv[i + 1:i + 3] + [""] * 2)[:2]) if reg else "no registry"
        if err:
            print("tolerance_registry_check: " + err, file=sys.stderr)
            return 1
        save_registry(ROOT, reg)
        print(f"tolerance_registry_check: approved {argv[i + 1]} as {argv[i + 2]}")
        return 0
    reg = load_registry(ROOT)
    if reg is None:
        print(f"tolerance_registry_check: FAILED -- {REGISTRY} is missing", file=sys.stderr)
        return 1
    if "--list" in argv:
        list_inventory(reg)
        return 0
    err, results = controls()
    if err:
        print("tolerance_registry_check: FAILED (the check itself is broken) -- " + err, file=sys.stderr)
        return 1
    if "-v" in argv:
        for name, ok in results:
            print(f"  control: {'FIRED ' if ok else 'MISSED'} {name}")
    fails, st = judge(ROOT, reg)
    if fails:
        print(f"tolerance_registry_check: FAILED -- {len(fails)} problem(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        print("    " + GATE_MSG, file=sys.stderr)
        return 1
    print(f"tolerances: {st['n']} registered ({st['unattributed']} unattributed), 0 changed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
