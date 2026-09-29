#!/usr/bin/env python3
"""sluice_hold_check — no Sluice spec or manifest text in horde's tracked files (B359).

WIRED: ./verify fast (local-only: SKIPPED wherever the gitignored `local/sluice` link is absent, CI included)

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
  4. HORDE-ORIGIN TEXT: a window that already occurs in horde's own tree as of
     the last first-parent commit of HEAD dated before the Sluice files' first
     commit is horde's (wording Sluice's docs reuse from horde's own specs).
     The cut-off date is read from the link target's own git log, read-only;
     the horde tree at that commit is read, minus exclusions 1 and 2 (Sluice's
     pre-date filings are theirs, not ours), reusing this run's scan for every
     blob unchanged since. A snapshot, not every historical blob: text horde
     wrote AND deleted before the cut-off is not credited, nor is text horde
     filed only into Sluice's own mailbox. Both are the stricter choice: a
     false credit would hide a copy, a missing one only asks a human to look.

PENDING REMEDIATION (PENDING below) names FILES only, each with a date and the
record that will clear it. A listed file's hits are counted, not failed; a
listed file that reads 0 hits FAILS, so an entry can never outlive its reason.

CONTROLS (7), every run, on in-memory copies of README.md (never on disk). A
45-character spec piece, cut at runtime, planted (1) bare, (2) wrapped across
comment lines, (3) split over a string join must each add hits; the same words
(4) as a path and (5) as an id must add none; (6) a horde-origin window must add
none, and (7) the same window with exclusion 4 switched off must add hits. A
control that behaves wrongly fails the gate: a blind scan reads like a clean tree.

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


def cutoff():
    """Earliest first-commit date of the linked files, from THEIR repo's log
    (read-only) -> (iso date, the last first-parent horde commit before it)."""
    target = os.path.realpath(LINK)
    top = git("rev-parse", "--show-toplevel", cwd=target).decode().strip()
    rel = os.path.relpath(target, top)
    dates = git("log", "--diff-filter=A", "--format=%cI", "--", rel, cwd=top).decode().split()
    if not dates:
        raise RuntimeError("the linked files have no add-commit in their own repo")
    date = min(dates)
    commit = git("rev-list", "-1", "--first-parent", "--before=" + date, "HEAD").decode().strip()
    if not commit:
        raise RuntimeError("HEAD has no commit before " + date)
    return date, commit


def blob_id(b):
    """git's blob id for bytes: lets the pre-date tree reuse this run's scan of
    every blob unchanged since the cut-off (most of them), so only blobs that
    changed since are read and normalised a second time."""
    return hashlib.sha1(b"blob %d\x00" % len(b) + b).hexdigest()


def horde_origin(commit, needles, seen):
    """Needles present in horde's tree at `commit` (exclusion 4). `seen` maps
    blob id -> the needle windows this run already found in that blob."""
    found, todo = set(), []
    for ent in git("ls-tree", "-r", "-z", commit).split(b"\x00"):
        if not ent:
            continue
        meta, path = ent.split(b"\t", 1)
        mode, kind, sha = meta.split()
        if kind != b"blob" or mode == b"120000" or \
                path.decode("utf-8", errors="replace").startswith(EXCLUDED_PREFIXES):
            continue
        sha = sha.decode()
        if sha in seen:
            found |= seen[sha]
        else:
            todo.append(sha)
    if todo:
        out = git("cat-file", "--batch", inp=("\n".join(todo) + "\n").encode())
        k = 0
        while k < len(out):
            eol = out.index(b"\n", k)
            size = int(out[k:eol].split()[2])
            t = decode(out[eol + 1:eol + 1 + size])
            k = eol + 1 + size + 1
            if t is not None:
                found.update(hit_windows(norm_fast(t), needles))
    return found


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


def controls(base, specs, needles, origin):
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
    # The pick must start and end inside a word: planted on its own line it is
    # then a word-start window, which is what hit_windows() probes.
    whole = sorted(w for w in origin if w[0] != " " and w[-1] != " ")
    if whole:
        w = whole[0]  # deterministic pick
        cases += [("a horde-origin window", w, origin, False),
                  ("the same window with exclusion 4 off", w, set(), True)]
    else:
        fails.append("control unbuildable: no horde-origin window exists, exclusion 4 is untested")
    ok = []
    for name, plant, org, must_hit in cases:
        before = count_hits(base, needles, org)
        after = count_hits(base + "\n" + plant + "\n", needles, org)
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
    date, commit = cutoff()

    # Pass 1: every tracked text file (working-tree copy), exclusion 4 not yet applied.
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
    origin = horde_origin(commit, needles, seen)

    # Pass 2: only files that hit, exclusion 4 applied, line numbers mapped.
    failing, pending, drift = [], {}, []
    for rel, (t, wins) in sorted(texts.items()):
        n_fast = sum(1 for w in wins if w not in origin)
        if not n_fast:
            continue
        n, found = report(t, needles, origin)
        if n != n_fast:
            drift.append("%s: fast %d vs mapped %d" % (rel, n_fast, n))
        if rel in PENDING:
            pending[rel] = (n, found)
        else:
            failing.append((rel, n, found))
    stale = [rel for rel in PENDING if rel not in pending]

    ctl_ok, ctl_fail = controls(base, specs, needles, origin)
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
    print("sluice_hold: %s — %d needles from %d linked file(s); horde-origin cut-off %s "
          "(horde %s, %d window(s) credited); controls %d/%d; %.1f s"
          % ("ok" if ok else "FAIL", len(needles), len(files), date, commit[:9], len(origin),
             len(ctl_ok), len(ctl_ok) + len(ctl_fail), dt))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
