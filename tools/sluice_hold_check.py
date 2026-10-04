#!/usr/bin/env python3
"""sluice_hold_check — no Sluice spec or manifest text in horde's tracked files (B359).

WIRED: ./verify fast (ADVISORY since 2026-10-04, B418: a failure prints a WARNING and never fails
verify; local-only: SKIPPED wherever the gitignored `local/sluice` link is absent, CI included)

WHY. This repo is public; Sluice is private. The human's hold is "copy NO Sluice
code, data or spec text into horde's tree". It was breached twice by agents who
were allowed to READ Sluice (B333: a code port; B358: spec phrases in a lab, two
traces and ROADMAP rows). The lab's own no-data check only scanned the one file
it lives in, and at first only the manifest's strings, so the phrases went
through. This gate scans EVERY tracked text file (LIBRARY L0067: a brief that
grants reading a private sibling needs a committed-file scan behind it).

THE NEEDLES ARE NEVER IN THIS FILE. They are read at runtime from every text
file under `local/sluice/` (a gitignored link to Sluice's `docs/horde`, the spec
and the manifest, read in place). Nothing Sluice-shaped is written anywhere: a
hit is reported as file, line, run count and a short sha256 of the normalised
phrase, never the phrase. To see what a hit IS, open the file at that line.

NORMALISATION (both sides, the idea of the lab's C17, reimplemented here):
lower case; a string split by concatenation (quote, `+`, quote) rejoined; HTML
entities, HTML comment marks and comment/markdown marks (* _ ` | # > / \\) turned
to spaces; whitespace runs collapsed. A NEEDLE is every 28-character window of
normalised Sluice text with at least 20 letters a-z (so numbers, rules and
table pipes are not needles). For the manifest, the raw text AND every decoded
JSON string are windowed (a JSON escape would otherwise hide a phrase).
Catches: a verbatim phrase, one wrapped across (comment) lines, one in another
case, one split over a string join, any 28-character piece of a longer copy.
Does NOT catch: a paraphrase, a copy shorter than 28 characters, a copy with a
word changed every 27 characters, one broken by an HTML tag or escape inside it,
one assembled at runtime from pieces, or Python's implicit (no `+`) string
concatenation. For speed a file is first PROBED at word starts only and scanned
at every offset only if the probe hits (hit_windows), so a copy of 28 to about
40 characters that holds no word-start window can slip; counts are every offset.

EXCLUSIONS, by principle only (no list of phrases, ever):
  1. `integrations/sluice/**` — Sluice's own filings in our mailbox are their
     act, not ours (INTEGRATIONS mailbox exception).
  2. `libs/**` — vendored third-party code.
  3. PATHS AND IDS, structurally: before normalising, every whitespace-delimited
     token holding a path (a `./` `../` `~/` prefix, two or more `/`, or a
     `/name.ext`), a file name with a known extension, or a project-prefixed
     hyphenated id (`hypersaw-…`, `sluice-…`, `horde-…`) is replaced WHOLE by a
     break character on BOTH sides, so no window spans one.

CREDIT: HORDE-ORIGIN TEXT, PER WINDOW (B417, Proposal C of the B415 triage,
ruled by the human 2026-10-02). Credit is not an exclusion: every window is
scanned, and a hit window W is then CREDITED (not counted) only if all hold —
  a. W is not in the never-credit ledger (below);
  b. W occurs in some Sluice COMMIT: its first Sluice-authored appearance T(W)
     is the earliest commit, over Sluice's whole history, ALL refs, ANY path
     except `integrations/hypersaw/**` (horde's own filings in Sluice's
     mailbox), whose tree holds W after this gate's normalisation. A commit's
     time is min(author time, committer time), in epoch seconds;
  c. horde's tree at the last first-parent commit of HEAD whose committer time
     is strictly before T(W) holds W (that tree minus exclusions 1 and 2:
     Sluice's filings in our mailbox are theirs, not ours).
The decision is the pure function credit(); first_appearances() is the pure
half of (b). One horde snapshot is read per distinct cut commit, reusing this
run's scan of every blob unchanged since. T(W) is CACHED in
`sluice-hold-cache.json` in the main checkout's `local/` (see below;
gitignored; window hashes and times only, never text), keyed by Sluice's HEAD, every ref, and the needle set: a Sluice commit,
ref move or working-tree edit of the linked files invalidates it. This gate's
own docstring (the link sentence above) hits Sluice's later wording; it is
credited by this rule (horde wrote it first), not excluded.

THE NEVER-CREDIT LEDGER, `sluice-hold-ledger.json` (gitignored; sha256 of
normalised windows only, never text, each with a date, a file and a reason).
ONE ledger for every checkout: it and the cache live in the MAIN checkout's
`local/`, the parent of git's common dir, resolved at runtime (state_dir(),
the way tools/labs_preview.sh finds the main checkout), so a failure seen in
any worktree lands in it. Only if the main checkout has no `local/` does a run
fall back to its own `local/`, and its summary line says so.
Every window that is counted on a run (failing, or a PENDING file's) is
appended with reason "failed <date>", and a ledgered window is never credited
again, whatever (b) and (c) say. It was seeded from the B415 triage's
Sluice-origin windows. NO CODE PATH REMOVES AN ENTRY: one leaves only by hand,
and only on a human ruling recorded in a ROADMAP row that names its hash and
the reason. A missing ledger is not silent: the run says so on its summary line
and creates it.

RISKS of the per-window credit, stated so a reader can weigh them:
  1. Uncommitted Sluice text. `local/sluice` is Sluice's working tree, so a
     needle can exist before any Sluice commit holds it; an agent who copied it
     and committed it to horde first would look horde-first once Sluice commits.
     Answered twice: a needle in no Sluice commit is never credited (b), so the
     copy is red when made; and red windows are ledgered, so it stays red after
     Sluice commits. The residue: a copy made AND removed between two gate runs.
  2. Sluice history rewrites. A rebase or squash re-dates commits later, which
     would make horde look first. Taking min(author, committer) blunts it:
     author dates survive a rebase. A rewrite that drops the first commit from
     every ref is not seen.
  3. Paraphrase is credited, and that is the hold's intended line: horde may say
     what Sluice means in its own words; it may not copy Sluice's text.
  4. The needle set is unpinned: it is Sluice's working tree, so a Sluice commit
     or edit can turn horde red with no horde change (B415). Credit by first
     appearance makes that rarer, never impossible.
  5. A snapshot, not every historical blob: text horde wrote AND deleted before
     T(W) is not credited, nor is text horde filed only into Sluice's mailbox,
     nor a window that only a decoded JSON string of a Sluice commit holds.
     All are the stricter choice: a false credit would hide a copy, a missing
     one only asks a human to look.

PENDING REMEDIATION (PENDING below) names FILES only, each with a date and the
record that will clear it. A listed file's hits are counted, not failed; a
listed file that reads 0 hits FAILS, so an entry can never outlive its reason.

CONTROLS (13), every run, on in-memory copies of README.md (never on disk). A
45-character spec piece, cut at runtime, planted (1) bare, (2) wrapped across
comment lines, (3) split over a string join must each add hits; the same words
(4) as a path and (5) as an id must add none. The credit controls plant one
window W that horde held at the control cut (the last first-parent commit
before the linked files' first Sluice commit) and not at its root commit, and
inject the first Sluice appearance of every window the plant adds: (6) at the
control cut adds none, and (7) the same window with credit switched off adds
hits; (8) derived by first_appearances() from a Sluice blob at the cut adds
none; (9) just after horde's root commit (Sluice first) adds hits; (10)
ledgered adds hits though horde had it first; (11) in no Sluice commit adds
hits; (12) earlier only under `integrations/hypersaw/` adds none, and must add
hits when that same appearance is moved to another path (else (12) proves
nothing). (13) Run from a worktree (an injected layout, no real directory),
state_dir() must resolve to the main checkout's `local/` and fall back to the
worktree's own only when that is absent, and the plant's counted windows must
land in the ledger at the resolved path, once (a second append adds none).
A control that behaves wrongly fails the gate: a blind scan reads like a clean
tree.

Usage: python3 tools/sluice_hold_check.py [--detail]
  --detail  also list every pending file's phrases (line, runs, hash).
"""
import bisect
import hashlib
import json
import os
import re
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINK = os.path.join(REPO, "local", "sluice")
# The ledger (never committed) and the cache (disposable) live in the MAIN
# checkout's local/, resolved at runtime by state_dir(), so every worktree
# shares one ledger. Only their names are written here, never a directory.
LEDGER_NAME = "sluice-hold-ledger.json"
CACHE_NAME = "sluice-hold-cache.json"
CACHE_VERSION = 1  # bump when what the cache holds changes meaning
MAILBOX = "integrations/hypersaw/"  # horde's filings in Sluice's tree: never Sluice-authored
RUN = 28           # window length, the lab's C17 constant
MIN_LETTERS = 20   # a window needs this many a-z letters to be a needle
BREAK = "\x00"     # stands in for a masked path/id; never inside a needle

EXCLUDED_PREFIXES = ("integrations/sluice/", "libs/")

# File -> (date entered, what clears it). FILE NAMES ONLY, never phrase text.
PENDING = {
}

# ---- masking: paths and ids (exclusion 3) ---------------------------------
# Token-level on purpose: a whitespace-delimited token that LOOKS like a path or
# a project-prefixed id is masked whole. A substring regex over the whole text
# was ~3x slower (it retries every start position inside long words).
_EXT = r"(?:md|json|html?|mjs|js|py|cpp|hpp|h|txt|tsv|sh|cmake|csv|yml|yaml|toml|clap|vst3)"
_PATHLIKE = re.compile(
    r"(?:^|[^\w.-])(?:\.{1,2}/|~/)"                 # ./x ../x ~/x
    r"|[^/\s]/[^/\s]+/"                             # a/b/... (two or more slashes)
    r"|/[\w@*{}-]+\." + _EXT + r"\b"                 # a/b.ext
    r"|\w\." + _EXT + r"\b"                          # file.ext
    r"|\b(?:hypersaw|sluice|horde)-[a-z0-9]",        # hypersaw-brief-003, sluice-notice-x
    re.IGNORECASE)
_TOKEN = re.compile(r"\S+")


def is_pathlike(tok):
    return ("/" in tok or "." in tok or "-" in tok) and _PATHLIKE.search(tok) is not None


# ---- normalisation ----------------------------------------------------------
# Two implementations of ONE function: norm_fast for detection (C-speed passes,
# no source map) and norm_mapped for reporting line numbers (only for files
# that hit). They must yield the same windows; main() cross-checks the run
# counts of the two on every file it reports and fails on any disagreement.
_JOIN = r"['\"`]\s*\+\s*['\"`]"   # 'abc ' + 'def' -> abc def
_ENT = r"<!--|-->|&[a-zA-Z]+;|&#\d+;|&#x[0-9a-fA-F]+;"
_MARKCHARS = "*_`|#>/\\"
_JOIN_RE, _ENT_RE = re.compile(_JOIN), re.compile(_ENT)
_MARK_TABLE = str.maketrans({c: " " for c in _MARKCHARS})
_SPECIAL = re.compile("(?P<join>" + _JOIN + ")|(?P<mark>(?:" + _ENT + "|["
                      + re.escape(_MARKCHARS) + r"]|\s)+)|(?P<brk>\x00+)")


def norm_fast(raw):
    ps = _PATHLIKE.search  # is_pathlike, inlined: this is the hottest line in the gate
    s = " ".join([BREAK if ("/" in w or "." in w or "-" in w) and ps(w) else w
                  for w in raw.split()])
    s = _ENT_RE.sub(" ", _JOIN_RE.sub("", s)).translate(_MARK_TABLE)
    return " ".join(s.split()).lower()


def norm_mapped(raw):
    """-> (norm, segs). segs is a sorted list of (norm_pos, src_pos): norm[n]
    came from raw[src_pos + (n - norm_pos)] up to the next seg. A masked token
    becomes BREAK * its length, so offsets into `raw` survive masking."""
    raw = _TOKEN.sub(lambda m: BREAK * len(m.group()) if is_pathlike(m.group()) else m.group(), raw)
    out, segs, n, prev_space, pos = [], [], 0, True, 0
    for m in _SPECIAL.finditer(raw):
        s = m.start()
        if s > pos:
            segs.append((n, pos)); chunk = raw[pos:s].lower(); out.append(chunk)
            n += len(chunk); prev_space = False
        if m.lastgroup == "brk":  # a masked token: whitespace-delimited, as in norm_fast
            segs.append((n, s)); out.append(BREAK); n += 1; prev_space = False
        elif m.lastgroup == "mark" and not prev_space:
            segs.append((n, s)); out.append(" "); n += 1; prev_space = True
        pos = m.end()
    if pos < len(raw):
        segs.append((n, pos)); out.append(raw[pos:].lower())
    return "".join(out).rstrip(" "), segs


_LETTERS = re.compile(r"[a-z]")
_SPACE = re.compile(" ")


def windows(norm):
    """Every needle-shaped window of a normalised text."""
    out = set()
    for i in range(len(norm) - RUN + 1):
        w = norm[i:i + RUN]
        if BREAK not in w and len(_LETTERS.findall(w)) >= MIN_LETTERS:
            out.add(w)
    return out


def hit_positions(norm, needles):
    """Start offsets of every window of `norm` that is a needle, every offset."""
    return [i for i in range(len(norm) - RUN + 1) if norm[i:i + RUN] in needles]


def hit_windows(norm, needles):
    """The needle windows of `norm` (with multiplicity), found in two stages
    for speed: windows starting at a WORD START are probed first (about a
    sixth of the offsets), and only a text with a probe hit is scanned at
    every offset. The cost: a copy that holds no word-start window — under
    28 characters plus the length of one word — is not seen. Every copy the
    controls plant (45 characters) is, and a reported count is every offset."""
    L = len(norm) - RUN + 1
    if L <= 0:
        return []
    if norm[:RUN] not in needles and not any(
            norm[i:i + RUN] in needles for i in (m.end() for m in _SPACE.finditer(norm))):
        return []
    return [norm[i:i + RUN] for i in hit_positions(norm, needles)]




def phrases(norm, segs, raw, hits):
    """Merge overlapping hit windows into phrases -> [(line, runs, hash)]."""
    out, cur = [], None
    for i in hits:
        if cur and i <= cur[1]:
            cur[1] = i + RUN; cur[2] += 1
        else:
            if cur: out.append(cur)
            cur = [i, i + RUN, 1]
    if cur: out.append(cur)
    nl = [m.start() for m in re.finditer("\n", raw)]
    starts = [s[0] for s in segs]
    res = []
    for s, e, runs in out:
        k = bisect.bisect_right(starts, s) - 1
        src = segs[k][1] + (s - segs[k][0]) if k >= 0 else 0
        line = bisect.bisect_right(nl, src) + 1
        res.append((line, runs, hashlib.sha256(norm[s:e].encode()).hexdigest()[:12]))
    return res


# ---- inputs ----------------------------------------------------------------
def decode(b):
    """Text, or None for a binary blob (a NUL in its first 8 KiB, git's rule)."""
    return None if b"\x00" in b[:8192] else b.decode("utf-8", errors="replace")


def json_strings(o):
    if isinstance(o, dict):
        for k, v in o.items():
            yield k; yield from json_strings(v)
    elif isinstance(o, list):
        for v in o:
            yield from json_strings(v)
    elif isinstance(o, str):
        yield o


def load_sluice():
    """-> (needles, spec_texts, files): every text file under the link."""
    needles, specs, files = set(), [], []
    for root, _dirs, names in os.walk(LINK, followlinks=True):
        for nm in sorted(names):
            p = os.path.join(root, nm)
            with open(p, "rb") as f:
                t = decode(f.read())
            if t is None:
                continue
            files.append(os.path.relpath(p, LINK))
            needles |= windows(norm_fast(t))
            if nm.endswith(".json"):
                try:
                    for s in json_strings(json.loads(t)):
                        needles |= windows(norm_fast(s))
                except ValueError:
                    pass
            if nm.endswith(".md"):
                specs.append(t)
    return needles, specs, files


def git(*args, cwd=REPO, inp=None):
    return subprocess.run(["git", "-C", cwd, *args], capture_output=True,
                          input=inp, check=True).stdout


def sluice_top():
    """Top of the repo the link points into (read-only, never written)."""
    target = os.path.realpath(LINK)
    return git("rev-parse", "--show-toplevel", cwd=target).decode().strip(), target


def cutoff(top, target):
    """Earliest first-commit time of the linked files, from THEIR repo's log,
    min(author, committer), epoch. Since B417 this is only the CONTROL cut:
    the credit controls plant a window horde held before it."""
    rel = os.path.relpath(target, top)
    times = [min(int(a), int(c)) for a, c in (ln.split() for ln in git(
        "log", "--diff-filter=A", "--format=%at %ct", "--", rel, cwd=top).decode().splitlines())]
    if not times:
        raise RuntimeError("the linked files have no add-commit in their own repo")
    return min(times)


def blob_id(b):
    """git's blob id for bytes: lets a horde snapshot reuse this run's scan of
    every blob unchanged since its cut, so only blobs that changed since are
    read and normalised again."""
    return hashlib.sha1(b"blob %d\x00" % len(b) + b).hexdigest()


def cat_blobs(shas, cwd=REPO):
    """-> {sha: text or None} for blob ids, one `cat-file --batch` call."""
    res = {}
    if not shas:
        return res
    out = git("cat-file", "--batch", cwd=cwd, inp=("\n".join(shas) + "\n").encode())
    k = 0
    while k < len(out):
        eol = out.index(b"\n", k)
        sha, _kind, size = out[k:eol].split()
        size = int(size)
        res[sha.decode()] = decode(out[eol + 1:eol + 1 + size])
        k = eol + 1 + size + 1
    return res


def tree_blobs(treeish, cwd=REPO):
    """-> [(path, blob id)] for every regular blob of a tree (no links, no
    submodules)."""
    out = []
    for ent in git("ls-tree", "-r", "-z", treeish, cwd=cwd).split(b"\x00"):
        if not ent:
            continue
        meta, path = ent.split(b"\t", 1)
        mode, kind, sha = meta.split()
        if kind == b"blob" and mode != b"120000":
            out.append((path.decode("utf-8", errors="replace"), sha.decode()))
    return out


def horde_origin(commit, needles, seen):
    """Needles present in horde's tree at `commit`. `seen` maps blob id -> the
    needle windows found in that blob; blobs read here are added to it, so the
    next snapshot of the same run reuses them."""
    found, todo = set(), []
    for path, sha in tree_blobs(commit):
        if path.startswith(EXCLUDED_PREFIXES):
            continue
        if sha in seen:
            found |= seen[sha]
        else:
            todo.append(sha)
    for sha, t in cat_blobs(todo).items():
        seen[sha] = set(hit_windows(norm_fast(t), needles)) if t is not None else set()
        found |= seen[sha]
    return found


# ---- per-window credit (B417) ----------------------------------------------
def wsha(w):
    """The ledger's and the cache's name for a window: never the text."""
    return hashlib.sha256(w.encode()).hexdigest()


def first_appearances(occurrences, windows_of):
    """Pure half of credit rule (b). occurrences: (epoch, path, blob) for every
    blob of every Sluice commit; windows_of(blobs) -> {blob: needle windows}.
    -> {window: earliest epoch}. A blob seen only under MAILBOX (horde's own
    filings) is never a Sluice-authored appearance."""
    blob_t = {}
    for t, path, blob in occurrences:
        if not path.startswith(MAILBOX) and (blob not in blob_t or t < blob_t[blob]):
            blob_t[blob] = t
    first = {}
    for blob, ws in windows_of(list(blob_t)).items():
        t = blob_t[blob]
        for w in ws:
            if w not in first or t < first[w]:
                first[w] = t
    return first


def credit(hits, first_of, ledgered, held_before):
    """THE credit decision, pure: the subset of `hits` that is horde-origin.
    first_of(w) -> T(w) or None (in no Sluice commit); ledgered: window hashes
    never to credit; held_before(t) -> the windows horde's tree held at the
    last first-parent commit strictly before t."""
    out = set()
    for w in hits:
        if wsha(w) in ledgered:
            continue
        t = first_of(w)
        if t is not None and w in held_before(t):
            out.add(w)
    return out


def sluice_first(top, needles, cache):
    """-> ({window hash: T}, 'warm' | 'cold'). T(W) for every needle that some
    Sluice commit holds, read-only from Sluice's git; cached at `cache`."""
    refs = git("for-each-ref", "--format=%(objectname) %(refname)", cwd=top)
    head = git("rev-parse", "HEAD", cwd=top)
    nd = hashlib.sha256("\n".join(sorted(needles)).encode()).digest()
    key = hashlib.sha256(b"%d\n" % CACHE_VERSION + head + refs + nd).hexdigest()
    try:
        with open(cache) as f:
            c = json.load(f)
        if c.get("key") == key:
            return c["first"], "warm"
    except (OSError, ValueError):
        pass
    # One time per distinct root tree: the earliest commit holding it.
    tree_t = {}
    for ln in git("log", "--all", "--format=%T %at %ct", cwd=top).decode().splitlines():
        tree, a, c = ln.split()
        t = min(int(a), int(c))
        if tree not in tree_t or t < tree_t[tree]:
            tree_t[tree] = t
    occ = [(t, path, sha) for tree, t in tree_t.items() for path, sha in tree_blobs(tree, cwd=top)]

    def windows_of(blobs):
        return {sha: set(hit_windows(norm_fast(t), needles))
                for sha, t in cat_blobs(blobs, cwd=top).items() if t is not None}

    first = {wsha(w): t for w, t in first_appearances(occ, windows_of).items()}
    tmp = cache + ".tmp"
    with open(tmp, "w") as f:
        json.dump({"about": "sluice_hold cache (B417): window sha256 -> first Sluice "
                   "commit time. Hashes only. Safe to delete.", "key": key, "first": first}, f)
    os.replace(tmp, cache)
    return first, "cold"


def make_held_before(needles, seen):
    """held_before(t) over HEAD's first-parent history, one snapshot per cut."""
    fp = [(sha, int(ct)) for sha, ct in (ln.split() for ln in git(
        "log", "--first-parent", "--format=%H %ct", "HEAD").decode().splitlines())]
    snaps = {}

    def held_before(t):
        cut = next((sha for sha, ct in fp if ct < t), None)  # newest first
        if cut is None:
            return frozenset()
        if cut not in snaps:
            snaps[cut] = frozenset(horde_origin(cut, needles, seen))
        return snaps[cut]

    held_before.snaps = snaps
    held_before.root_t = fp[-1][1]
    return held_before


# ---- the never-credit ledger -----------------------------------------------
def state_dir(common_dir, repo, isdir=os.path.isdir):
    """-> (directory for the ledger and the cache, True if it is the MAIN
    checkout's). The main checkout is the parent of git's common dir, found the
    way tools/labs_preview.sh finds it, so a run from any worktree lands its
    failures in the ONE ledger. Pure given `isdir` (the controls inject it).
    With no main-checkout local/, this checkout's local/ is the fallback, and
    the summary line says so."""
    main_local = os.path.join(os.path.dirname(common_dir.rstrip(os.sep)), "local")
    if isdir(main_local):
        return main_local, True
    return os.path.join(repo, "local"), False


def load_ledger(path):
    """-> (data, created?). A missing ledger is reported and created, never
    silently skipped; a malformed one is an error, not an empty ledger."""
    if not os.path.exists(path):
        return {"about": LEDGER_ABOUT, "entries": []}, True
    with open(path) as f:
        data = json.load(f)
    if not isinstance(data.get("entries"), list) or not all(
            isinstance(e, dict) and re.fullmatch(r"[0-9a-f]{64}", str(e.get("sha256")))
            for e in data["entries"]):
        raise ValueError(LEDGER_NAME + " is malformed; fix it by hand")
    return data, False


def write_ledger(data, path):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    os.replace(tmp, path)


def ledger_append(data, counted, today):
    """Append every counted window (window -> the file it hit) not yet in the
    ledger, in place -> the new entries. Append-only: nothing here, or anywhere
    in this gate, removes an entry."""
    have = {e["sha256"] for e in data["entries"]}
    new = [{"sha256": h, "date": today, "file": rel, "reason": "failed " + today}
           for h, rel in sorted((wsha(w), rel) for w, rel in counted.items()) if h not in have]
    data["entries"].extend(new)
    return new


LEDGER_ABOUT = ("sluice_hold never-credit ledger (B417): sha256 of normalised windows "
                "the gate counted, never text. A ledgered window is never credited. "
                "No code removes an entry: remove one by hand only on a human ruling "
                "in a ROADMAP row that names the hash and the reason.")


def count_hits(raw, needles, origin):
    return sum(1 for w in hit_windows(norm_fast(raw), needles) if w not in origin)


def report(raw, needles, origin):
    """-> (runs, [(line, runs, hash)]) from the MAPPED normaliser."""
    norm, segs = norm_mapped(raw)
    hits = [i for i in hit_positions(norm, needles) if norm[i:i + RUN] not in origin]
    return len(hits), phrases(norm, segs, raw, hits)


# ---- controls --------------------------------------------------------------
def plant_piece(specs, needles, origin):
    """A 45-character piece of spec prose, every window of it a counted needle:
    from the first qualifying prose line, cut from its middle at runtime."""
    for t in specs:
        for line in t.split("\n"):
            line = line.strip()
            if len(line) < 70 or not line[0].isalpha() or "|" in line or "`" in line:
                continue
            i0 = len(line) // 2 - 22
            piece = line[i0:i0 + 45]
            sp = piece.find(" ", 18)
            if len(piece) < 45 or sp < 0:
                continue
            norm = norm_fast(piece)
            ws = [norm[i:i + RUN] for i in range(len(norm) - RUN + 1)]
            if len(norm) == 45 and all(w in needles and w not in origin for w in ws):
                return piece, sp
    return None, None


def controls(base, specs, needles, origin, ledgered, held_before, cut_t):
    """-> (names that behaved, failure strings). Plants go into in-memory copies
    of `base` (a tracked file's text), never onto disk."""
    piece, sp = plant_piece(specs, needles, origin)
    if piece is None:
        return [], ["control unbuildable: no spec prose line to plant — the scan is untested"]
    a, b = piece[:sp], piece[sp + 1:]
    words = re.findall(r"[A-Za-z]+", piece)
    cases = [  # (name, plant, origin set used, must add hits?)
        ("verbatim phrase", piece, origin, True),
        ("phrase wrapped across comment lines",
         "/* " + a + "\n * " + b + " */\n# " + a + "\n# " + b, origin, True),
        ("phrase split over a string join", "x = '" + a + " ' +\n    '" + b + "';", origin, True),
        ("the phrase as a path", "see docs/" + "/".join(words) + ".md here", origin, False),
        ("the phrase as an id", "filed as sluice-" + "-".join(w.lower() for w in words), origin, False),
    ]
    fails = []
    # W: a window horde held at the control cut and not at its root, not
    # ledgered. It must start and end inside a word: planted on its own line it
    # is then a word-start window, which is what hit_windows() probes. P is
    # every window the plant adds (W, and any needle W makes with the line
    # break before it); the injected first appearance covers all of P, and the
    # pick requires horde to have held all of P, so the plant is horde text.
    held0 = held_before(cut_t)
    early_t = held_before.root_t + 1  # cut = horde's root commit
    held_root = held_before(early_t)
    in_base = set(hit_windows(norm_fast(base), needles))
    pick = None
    for w in sorted(x for x in held0 if x[0] != " " and x[-1] != " "):  # deterministic
        P = set(hit_windows(norm_fast(base + "\n" + w + "\n"), needles)) - in_base
        if w in P and P <= held0 and not P & held_root and \
                not {wsha(x) for x in P} & ledgered:
            pick = w, P
            break
    if pick:
        w, P = pick
        rest = origin - P

        def org(first_of, led=ledgered):
            return rest | credit(P, first_of, led, held_before)

        def at(t):  # inject T = t for every window of the plant
            return {x: t for x in P}.get

        def from_occ(occ):  # T through the real rule (b) code
            return first_appearances(occ, lambda blobs: {bl: P for bl in blobs}).get

        mailbox_early = [(early_t, MAILBOX + "notice.md", "m"), (cut_t, "docs/spec.md", "s")]
        moved_early = [(early_t, "docs/notice.md", "m"), (cut_t, "docs/spec.md", "s")]
        cases += [
            ("a horde-held window, first in Sluice at the cut", w, org(at(cut_t)), False),
            ("the same window with credit off", w, rest, True),
            ("horde held it before its first Sluice blob", w,
             org(from_occ([(cut_t, "docs/spec.md", "s")])), False),
            ("Sluice had it first", w, org(at(early_t)), True),
            ("ledgered though horde had it first", w,
             org(at(cut_t), ledgered | {wsha(x) for x in P}), True),
            ("a needle in no Sluice commit", w, org({}.get), True),
            ("earlier only in our mailbox filing", w, org(from_occ(mailbox_early)), False),
        ]
        # (12) proves nothing unless the same appearance off the mailbox path is red.
        moved = org(from_occ(moved_early))
        if count_hits(base + "\n" + w + "\n", needles, moved) <= count_hits(base, needles, moved):
            fails.append("control vacuous: earlier only in our mailbox filing "
                         "(moving it off the mailbox path did not turn it red)")
    else:
        fails.append("control unbuildable: no window horde held at the control cut "
                     "(and not at its root) — the credit rule is untested")
    # (13) A failure planted in a WORKTREE run lands in the MAIN checkout's
    # ledger: resolve from an injected worktree layout ("M" is the main
    # checkout; names only, no directory is touched), then append the plant's
    # counted windows to an in-memory ledger at the resolved path, twice.
    name13 = "a worktree failure lands in the main checkout's ledger"
    common, wt = os.path.join("M", ".git"), os.path.join("M", ".claude", "worktrees", "w")
    main_local = os.path.join("M", "local")
    got, got_main = state_dir(common, wt, isdir=lambda d: d == main_local)
    fb, fb_main = state_dir(common, wt, isdir=lambda d: False)
    counted = {x: "README.md" for x in
               set(hit_windows(norm_fast(base + "\n" + piece + "\n"), needles)) - origin}
    books = {os.path.join(got, LEDGER_NAME): {"entries": []}}
    book = books.get(os.path.join(main_local, LEDGER_NAME))
    first_new = ledger_append(book, counted, "d") if book is not None else []
    if (got_main and not fb_main and fb == os.path.join(wt, "local") and counted
            and {e["sha256"] for e in first_new} == {wsha(x) for x in counted}
            and not ledger_append(book, counted, "d")):
        ok = [name13]
    else:
        ok = []
        fails.append("control NOT as built: " + name13 + " (main-checkout resolution, "
                     "its fallback, or append-once broke)")
    for name, plant, org_set, must_hit in cases:
        before = count_hits(base, needles, org_set)
        after = count_hits(base + "\n" + plant + "\n", needles, org_set)
        if (after > before) if must_hit else (after == before):
            ok.append(name)
        else:
            fails.append("control %s: %s" % ("NOT caught (must fail)" if must_hit
                                             else "wrongly caught (must pass)", name))
    return ok, fails


def main():
    detail = "--detail" in sys.argv[1:]
    t0 = time.monotonic()
    if not os.path.isdir(LINK):
        print("sluice_hold: local/sluice absent — Sluice-hold scan SKIPPED "
              "(expected off this Mac and in CI; see the tool header)")
        return 0
    needles, specs, files = load_sluice()
    if not needles or not specs:
        print("sluice_hold: FAIL — local/sluice holds no readable spec text; "
              "a scan with no needles proves nothing", file=sys.stderr)
        return 1
    top, target = sluice_top()
    cut_t = cutoff(top, target)
    sdir, sdir_main = state_dir(git("rev-parse", "--path-format=absolute",
                                    "--git-common-dir").decode().strip(), REPO)
    os.makedirs(sdir, exist_ok=True)
    ledger_path = os.path.join(sdir, LEDGER_NAME)
    first, cache_state = sluice_first(top, needles, os.path.join(sdir, CACHE_NAME))
    ledger, ledger_created = load_ledger(ledger_path)
    ledgered = {e["sha256"] for e in ledger["entries"]}

    # Pass 1: every tracked text file (working-tree copy), no credit yet.
    seen, texts, base = {}, {}, ""
    for rel in git("ls-files", "-z").decode("utf-8", errors="replace").split("\x00"):
        if not rel or rel.startswith(EXCLUDED_PREFIXES):
            continue
        p = os.path.join(REPO, rel)
        if os.path.islink(p) or not os.path.isfile(p):
            continue
        with open(p, "rb") as f:
            b = f.read()
        t = decode(b)
        if t is None:
            continue
        if rel == "README.md":
            base = t
        wins = hit_windows(norm_fast(t), needles)
        seen[blob_id(b)] = set(wins)
        if wins:
            texts[rel] = (t, wins)
    hitset = set().union(*(set(wins) for _t, wins in texts.values()))
    held_before = make_held_before(needles, seen)
    origin = credit(hitset, lambda w: first.get(wsha(w)), ledgered, held_before)
    n_cuts = len(held_before.snaps)

    # Pass 2: only files that hit, credit applied, line numbers mapped.
    failing, pending, drift, counted = [], {}, [], {}
    for rel, (t, wins) in sorted(texts.items()):
        left = [w for w in wins if w not in origin]
        if not left:
            continue
        for w in left:
            counted.setdefault(w, rel)
        n, found = report(t, needles, origin)
        if n != len(left):
            drift.append("%s: fast %d vs mapped %d" % (rel, len(left), n))
        if rel in PENDING:
            pending[rel] = (n, found)
        else:
            failing.append((rel, n, found))
    stale = [rel for rel in PENDING if rel not in pending]

    ctl_ok, ctl_fail = controls(base, specs, needles, origin, ledgered, held_before, cut_t)

    # Ledger every counted window (rule a): it is never credited afterwards.
    new = ledger_append(ledger, counted, time.strftime("%Y-%m-%d"))
    if new or ledger_created:
        write_ledger(ledger, ledger_path)
    dt = time.monotonic() - t0

    ok = not failing and not stale and not ctl_fail and not drift
    for rel, n, found in failing:
        print("sluice_hold: HIT %s — %d run(s) in %d phrase(s)" % (rel, n, len(found)), file=sys.stderr)
        for line, runs, h in found:
            print("    %s:%d  runs=%d  phrase#%s" % (rel, line, runs, h), file=sys.stderr)
    for rel, (n, found) in sorted(pending.items()):
        print("sluice_hold: PENDING %s — %d run(s) in %d phrase(s) [%s; %s]"
              % (rel, n, len(found), PENDING[rel][0], PENDING[rel][1]))
        if detail:
            for line, runs, h in found:
                print("    %s:%d  runs=%d  phrase#%s" % (rel, line, runs, h))
    for rel in stale:
        print("sluice_hold: STALE pending entry — %s reads 0 hits; delete its PENDING row"
              % rel, file=sys.stderr)
    for d in drift:
        print("sluice_hold: normaliser drift (fast vs mapped disagree) — " + d, file=sys.stderr)
    for f in ctl_fail:
        print("sluice_hold: " + f, file=sys.stderr)
    print("sluice_hold: %s — %d needles from %d linked file(s); %d hit window(s), %d credited "
          "by first Sluice appearance over %d horde cut(s) (%s cache); ledger %d%s; "
          "controls %d/%d; %.1f s"
          % ("ok" if ok else "FAIL", len(needles), len(files), len(hitset), len(origin), n_cuts,
             cache_state, len(ledger["entries"]),
             (" in the main checkout" if sdir_main else
              " in THIS checkout (main checkout has no local/ — fallback)")
             + (" (+%d)" % len(new) if new else "")
             + (" (was MISSING — created)" if ledger_created else ""),
             len(ctl_ok), len(ctl_ok) + len(ctl_fail), dt))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
