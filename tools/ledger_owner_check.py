#!/usr/bin/env python3
"""ledger_owner_check -- every approval ledger has a code owner, and acceptances.json is well formed.

WIRED: ./verify fast

WHY. A ratchet check (weakening, tolerance, parameter id, build flag, licence, self-digest) is only
as strong as who may approve into its ledger. ADR-209 item 3: only the human approves. The
mechanism is GitHub code-owner review: .github/CODEOWNERS names the human for each ledger, and the
main ruleset (the human's switch, not ours) requires that review. This check is the repo-side
guard: it cannot make GitHub enforce anything, but it is red when the ownership record and the
ledgers drift apart, so a new ledger cannot appear unowned.

WHAT IT CHECKS (all read the tracked tree; nothing is written).
  1. Every path in LEDGERS exists (a glob must match at least one tracked file) and its last
     matching CODEOWNERS rule names an owner (@handle).
  2. Every CODEOWNERS rule matches at least one tracked file, and matches ONLY ledgers: a rule
     naming a file that does not exist, or a catch-all `*`, is red ("only ledgers get an owner").
  3. No ratchet tool pins a file that is neither in LEDGERS nor in KNOWN_UNLISTED. The scan reads
     every string literal in tools/*.py that names a *.json whose basename looks like a pin
     (pin, lock, baseline, allowlist, tolerances, ledger, divergences, acceptances) and that
     basename is a tracked file. KNOWN_UNLISTED holds pinned files the human has not ruled on; each
     carries its reason and prints every run, so the question stays visible.
  4. docs/armor/acceptances.json is well formed: every entry has exactly id, row, kind, text,
     approved, expires; ids are unique; row is a catalogue.json row id; kind is accepted_limit or
     hole_expiry; approved and text are not empty; expires is an ISO date.
  EXPIRED entries print under their own heading and DO NOT fail the check: a date must never turn
  ./verify red by itself (the lead's dashboard handles expiry). `--today YYYY-MM-DD` sets the date
  compared against (default: the system date) so tests are deterministic.

MUST-FAIL CONTROLS (LIBRARY L0032), in memory on copies of the real text, every run, never
touching the repo's files: CODEOWNERS with one ledger's rule removed is red; CODEOWNERS with a
catch-all `*` added is red; a CODEOWNERS rule naming a missing file is red; a tool source pinning an
unlisted ledger-looking file is red; an acceptances entry missing `approved` is red; an entry with
a past `expires` is reported and still green; a fully valid entry is green; the untouched copies
are green. A control that does not behave as stated fails the run (the judge cannot be trusted).
"""
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CODEOWNERS_REL = ".github/CODEOWNERS"
ACCEPTANCES_REL = "docs/armor/acceptances.json"
CATALOGUE_REL = "docs/armor/catalogue.json"

# The one list of approval ledgers. A `*` entry is a glob over tracked files (the self-digest
# references are one file per toolchain key, so the set grows with new keys).
LEDGERS = [
    "docs/armor/weakening-baseline.json",
    "docs/armor/tolerances.json",
    "docs/armor/acceptances.json",
    "tools/param_id_lock.json",
    "tools/build_flags_pin.json",
    "tools/license_allowlist.json",
    "h2/engine/selfdigest.*",
]

# The files that keep the ledgers honest (ADR-211 item 4): the owner record itself, this check,
# and each ratchet tool's source. A pull request that could edit one of these could unhook a
# ledger without touching it, so each is owned exactly as a ledger is.
GUARDS = [
    ".github/CODEOWNERS",
    "tools/ledger_owner_check.py",
    "tools/weakening_check.py",
    "tools/tolerance_registry_check.py",
    "tools/param_id_lock_check.py",
    "tools/build_flags_check.py",
    "tools/license_audit_check.py",
]

# Pinned files a check reads that the brief did not list as ledgers. Not owned, not hidden: they
# print on every run until the human rules each in or out. Adding one here is a visible choice.
KNOWN_UNLISTED = {
    "docs/port/divergences.json": "ratified-divergence record; whether it is an approval ledger is the human's ruling",
    "h2/cores/swarm/lift-ledger.json": "lift record; whether it is an approval ledger is the human's ruling",
}

PINISH = re.compile(r"(^|[-_.])(pin|lock|baseline|allowlist|tolerances|ledger|divergences|acceptances)([-_.]|$)")
JSON_LITERAL = re.compile(r"""["']([^"'\s]*\.json)["']""")
FIELDS = ("id", "row", "kind", "text", "approved", "expires")
KINDS = ("accepted_limit", "hole_expiry")
ISO_DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


def tracked_files(root=ROOT):
    out = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True, check=True).stdout
    return [p for p in out.splitlines() if p]


# ---- CODEOWNERS: the subset of gitignore matching the file uses (anchored paths, * and ?) ----
def parse_codeowners(text):
    rules = []
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            parts = line.split()
            rules.append((parts[0], parts[1:]))
    return rules


def pattern_regex(pat):
    anchored = pat.startswith("/") or "/" in pat.rstrip("/")
    body, out, i = pat.lstrip("/"), "", 0
    while i < len(body):
        if body.startswith("**", i):
            out, i = out + ".*", i + 2
        elif body[i] == "*":
            out, i = out + "[^/]*", i + 1
        elif body[i] == "?":
            out, i = out + "[^/]", i + 1
        else:
            out, i = out + re.escape(body[i]), i + 1
    if pat.endswith("/"):
        out += ".*"
    return re.compile(("^" if anchored else "^(.*/)?") + out + "$")


def owners_of(rules, path):
    """GitHub semantics: the LAST matching rule decides. -> owner list ([] = unowned)."""
    found = []
    for pat, owners in rules:
        if pattern_regex(pat).match(path):
            found = owners
    return [o for o in found if o.startswith("@")]


def expand_ledgers(tracked):
    """-> (concrete ledger paths, problems). A ledger entry matching no tracked file is a problem."""
    paths, problems = [], []
    for led in LEDGERS + GUARDS:
        hits = [t for t in tracked if pattern_regex("/" + led).match(t)]
        if not hits:
            problems.append(f"listed ledger or guard {led} matches no tracked file")
        paths += hits
    return sorted(set(paths)), problems


def check_owners(codeowners_text, tracked):
    ledgers, problems = expand_ledgers(tracked)
    rules = parse_codeowners(codeowners_text)
    for path in ledgers:
        if not owners_of(rules, path):
            problems.append(f"ledger {path} has no code owner in {CODEOWNERS_REL}")
    for pat, _ in rules:
        hit = [t for t in tracked if pattern_regex(pat).match(t)]
        if not hit:
            problems.append(f"{CODEOWNERS_REL} rule {pat!r} matches no tracked file")
        stray = [t for t in hit if t not in ledgers]
        if stray:
            problems.append(f"{CODEOWNERS_REL} rule {pat!r} owns non-ledger file(s) {stray[:3]}"
                            " (only ledgers get an owner; add the ledger to LEDGERS if it is one)")
    return problems, len(ledgers)


def check_unlisted(sources, tracked):
    """sources: {tool path: text}. -> (problems, notes). Pinned files in no list are problems."""
    ledgers, _ = expand_ledgers(tracked)
    by_base = {}
    for t in tracked:
        by_base.setdefault(t.rsplit("/", 1)[-1], []).append(t)
    problems, seen = [], {}
    for tool, text in sorted(sources.items()):
        for lit in JSON_LITERAL.findall(text):
            base = lit.rsplit("/", 1)[-1]
            if not PINISH.search(base):
                continue
            for hit in by_base.get(base, []):
                if hit not in ledgers and hit not in KNOWN_UNLISTED:
                    problems.append(f"{tool} pins {hit}, which is in neither LEDGERS nor KNOWN_UNLISTED")
                elif hit in KNOWN_UNLISTED:
                    seen.setdefault(hit, set()).add(tool)
    notes = [f"{p} (named in {', '.join(sorted(t))}): {KNOWN_UNLISTED[p]}" for p, t in sorted(seen.items())]
    return problems, notes


# ---- acceptances.json ----
def check_acceptances(text, catalogue_rows, today):
    """-> (problems, expired entries, entry count)."""
    try:
        data = json.loads(text)
    except ValueError as e:
        return [f"{ACCEPTANCES_REL}: not valid JSON ({e})"], [], 0
    entries = data.get("entries") if isinstance(data, dict) else None
    if not isinstance(entries, list):
        return [f"{ACCEPTANCES_REL}: needs an `entries` list"], [], 0
    problems, expired, ids = [], [], set()
    for n, e in enumerate(entries):
        tag = f"entry {n}" + (f" ({e.get('id')})" if isinstance(e, dict) and e.get("id") else "")
        if not isinstance(e, dict):
            problems.append(f"{tag}: not an object")
            continue
        for k in FIELDS:
            if k not in e:
                problems.append(f"{tag}: missing `{k}`")
        for k in e:
            if k not in FIELDS:
                problems.append(f"{tag}: unknown key `{k}`")
        for k in ("id", "text", "approved"):
            if k in e and not (isinstance(e[k], str) and e[k].strip()):
                problems.append(f"{tag}: `{k}` is empty")
        if isinstance(e.get("id"), str):
            if e["id"] in ids:
                problems.append(f"{tag}: duplicate id")
            ids.add(e["id"])
        if "row" in e and e["row"] not in catalogue_rows:
            problems.append(f"{tag}: row {e['row']!r} is not a row of {CATALOGUE_REL}")
        if "kind" in e and e["kind"] not in KINDS:
            problems.append(f"{tag}: kind {e['kind']!r} is not one of {KINDS}")
        if "expires" in e:
            try:
                if not (isinstance(e["expires"], str) and ISO_DATE.match(e["expires"])):
                    raise ValueError
                when = datetime.date.fromisoformat(e["expires"])
            except ValueError:
                problems.append(f"{tag}: `expires` {e['expires']!r} is not an ISO date YYYY-MM-DD")
            else:
                if when < today:
                    expired.append(e)
    return problems, expired, len(entries)


def valid_entry(**over):
    e = {"id": "A001", "row": "R1", "kind": "accepted_limit", "text": "t", "approved": "ADR-000",
         "expires": "2099-01-01"}
    e.update(over)
    return e


def acceptances_with(*entries):
    return json.dumps({"entries": list(entries)})


# ---- controls ----
def controls(codeowners_text, tracked, today):
    """-> list of (label, ok). Each runs the real judges on a mutated in-memory copy."""
    rows = {"R1"}
    res = []

    def red(label, problems):
        res.append((label + " reads red", bool(problems)))

    def green(label, problems):
        res.append((label + " reads green", not problems))

    green("untouched CODEOWNERS", check_owners(codeowners_text, tracked)[0])
    victim = expand_ledgers(tracked)[0][0]
    kept = "\n".join(ln for ln in codeowners_text.splitlines() if "/" + victim not in ln.split("#")[0])
    red(f"CODEOWNERS without the rule for {victim}", check_owners(kept, tracked)[0])
    guard = "tools/weakening_check.py"
    red(f"CODEOWNERS without the rule for the guard {guard}", check_owners(
        "\n".join(ln for ln in codeowners_text.splitlines() if "/" + guard not in ln.split("#")[0]), tracked)[0])
    red("CODEOWNERS without the rule for itself", check_owners(
        "\n".join(ln for ln in codeowners_text.splitlines() if "/.github/CODEOWNERS" not in ln.split("#")[0]), tracked)[0])
    red("CODEOWNERS plus a catch-all `*`", check_owners(codeowners_text + "\n* @someone\n", tracked)[0])
    red("CODEOWNERS plus a rule naming a missing file",
        check_owners(codeowners_text + "\n/docs/armor/no-such-ledger.json @someone\n", tracked)[0])
    red("a rule with no owner handle", check_owners(
        "\n".join(ln.split("@")[0] if "/" + victim in ln else ln for ln in codeowners_text.splitlines()),
        tracked)[0])
    planted = {"tools/planted_check.py": 'PIN = "tools/planted_pin.json"'}
    red("a tool pinning an unlisted pin-like tracked file", check_unlisted(
        planted, tracked + ["tools/planted_pin.json"])[0])
    green("a tool pinning a listed ledger", check_unlisted(
        {"tools/planted_check.py": f'PIN = "{victim}"'}, tracked)[0])
    green("an empty acceptances file", check_acceptances(acceptances_with(), rows, today)[0])
    green("a valid acceptances entry", check_acceptances(acceptances_with(valid_entry()), rows, today)[0])
    no_approved = valid_entry()
    del no_approved["approved"]
    red("an entry missing `approved`", check_acceptances(acceptances_with(no_approved), rows, today)[0])
    red("an entry with blank `approved`", check_acceptances(acceptances_with(valid_entry(approved=" ")), rows, today)[0])
    red("an entry with a bad date", check_acceptances(acceptances_with(valid_entry(expires="soon")), rows, today)[0])
    red("an entry on an unknown row", check_acceptances(acceptances_with(valid_entry(row="R99")), rows, today)[0])
    past = check_acceptances(acceptances_with(valid_entry(expires="2000-01-01")), rows, today)
    res.append(("an entry with a past `expires` is reported", len(past[1]) == 1))
    res.append(("an entry with a past `expires` still reads green", not past[0]))
    return res


def main(argv):
    args = list(argv)
    today = datetime.date.today()
    if "--today" in args:
        i = args.index("--today")
        try:
            today = datetime.date.fromisoformat(args[i + 1])
        except (IndexError, ValueError):
            print("ledger_owner_check: --today needs YYYY-MM-DD")
            return 2
    tracked = tracked_files()
    try:
        co_text = (ROOT / CODEOWNERS_REL).read_text(encoding="utf-8")
        acc_text = (ROOT / ACCEPTANCES_REL).read_text(encoding="utf-8")
        rows = {r["id"] for r in json.loads((ROOT / CATALOGUE_REL).read_text(encoding="utf-8"))["rows"]}
    except (OSError, ValueError, KeyError) as e:
        print(f"ledger_owner_check: RED -- cannot read an input file: {e}")
        return 1

    ctl = controls(co_text, tracked, today)
    if "-v" in args:
        for label, ok in ctl:
            print(f"  control: {label}: {'as expected' if ok else 'WRONG'}")
    failed = [label for label, ok in ctl if not ok]
    if failed:
        print("ledger_owner_check: SELFTEST FAILED -- the checker cannot be trusted:\n  " + "\n  ".join(failed))
        return 1

    problems, n_ledgers = check_owners(co_text, tracked)
    sources = {t: (ROOT / t).read_text(encoding="utf-8", errors="replace")
               for t in tracked if t.startswith("tools/") and t.endswith(".py") and t != "tools/ledger_owner_check.py"}
    unl_problems, notes = check_unlisted(sources, tracked)
    acc_problems, expired, n_acc = check_acceptances(acc_text, rows, today)
    problems += unl_problems + acc_problems

    for note in notes:
        print(f"  PINNED, NOT A LEDGER YET (the human rules): {note}")
    if expired:
        print(f"  EXPIRED as of {today} (reported, never red; the row is partial again):")
        for e in expired:
            print(f"    {e['id']} on {e['row']} ({e['kind']}), expired {e['expires']}: {e['text']}")
    if problems:
        print("ledger_owner_check: RED")
        for p in problems:
            print("  " + p)
        return 1
    print(f"ledger_owner_check: GREEN ({n_ledgers} ledger files owned, no unlisted pin; "
          f"{n_acc} acceptance entries, {len(expired)} expired; {len(ctl)} controls fired as expected)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
