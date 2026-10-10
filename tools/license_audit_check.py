#!/usr/bin/env python3
"""license_audit_check -- every third-party component is inventoried, its license
text still reads as the id on record, and nothing ships under an id the human has
not allowed (B448 Wave C C1; ADR-197, docs/strategy/blind-spot-armor.md risk row 12).

WIRED: ./verify fast

  python3 tools/license_audit_check.py [--today YYYY-MM-DD]   controls, audit, doc freshness
  python3 tools/license_audit_check.py --check                doc freshness only
  python3 tools/license_audit_check.py --write-doc            regenerate docs/LICENSES-THIRD-PARTY.md
  python3 tools/license_audit_check.py --online               re-read the fetched SDKs' license texts

THREE FILES. tools/license_inventory.json is the hand-kept record: one entry per
component, with its pin, where it sits, whether it SHIPS, the SPDX id READ from
its own license text, and a fingerprint of that text. tools/license_allowlist.json
is the allow list, PROPOSED until the human ratifies it. docs/LICENSES-THIRD-PARTY.md
is generated from the two and never edited by hand.

RED WHEN:
  (a) UNLISTED. The tree holds a component the inventory does not, or the inventory
      names one the tree no longer holds. The tree is RE-DERIVED, never listed:
      git submodules (gitlinks in the index); FetchContent_Declare in the root
      CMakeLists.txt, and any other fetch mechanism in a tracked CMake file; every
      fetch a submodule's own CMake declares (clap-wrapper's CPM packages), each
      recorded as covered by an entry or as not built; every directory vendored
      inside a submodule (libs/, external/, third_party/ and the like); every
      licence that a choc header reachable from our includes registers with
      CHOC_REGISTER_OPEN_SOURCE_LICENCE (how choc declares code it embeds); every
      tracked file carrying a license marker; and any external script, stylesheet,
      font or URL in a shipped GUI page (src/gui/*.html).
  (b) LICENSE. An entry's license file is missing, the text no longer reads as the
      recorded id (key phrases, not a full SPDX matcher), or its fingerprint
      (SHA-256 of the whitespace- and comment-normalised text) has changed. A
      changed copyright line changes the attribution a release must carry.
  (c) PIN. A pinned commit, a pinned file's SHA-256 or a probed version differs
      from the entry; the build's plugin formats or its download switch differ
      from what the inventory's "not built" claims rest on.
  (d) ALLOW. A component that ships has an id that is neither on the shipping
      list nor an exception with a non-empty `approved` and a future `expires`.
      Anything else needs the wider list (shipping plus not-shipped) or an
      exception. An expired exception is red whoever approved it.
  (e) SBOM. tools/gen_sbom.py's output and the inventory disagree on a component
      or its version. Entries nested inside another pin say so in `sbom_absent`,
      and those are PRINTED: the SBOM lists pins, not what rides inside them.
  (f) SHAPE. A file is not valid JSON, is not in canonical form (sorted keys,
      two-space indent), or an entry lacks a field.
  (g) DOC. docs/LICENSES-THIRD-PARTY.md is not what this tool generates.

PENDING THE HUMAN. An exception with `status: needs-human` and an empty `approved`
is red, UNLESS it is named in the allow list's `pending_human` array with the date
it was raised. Those print as `PENDING THE HUMAN` and the run exits 0. That is a
visible hole, not a pass: the summary line counts them, and a pending exception
goes red on its `expires` date like any other.

WITHOUT SUBMODULES (CI's verify-fast job checks none out) the rows that read files
inside a submodule print a WARNING and do not run. The pins, the allow list, the
SBOM comparison, the marker scan and the doc still do.

FETCHED SDKs. The VST3 SDK, the AudioUnitSDK and pluginval are not in the tree.
Offline, their license text is held by the PIN: a commit is content-addressed, so
the same commit carries the same LICENSE. After a bump, `--online` re-reads each
text from its upstream at the recorded commit; it needs the network and is not
part of verify.

THE DATE. `--today` defaults to the day the tool runs, as in
tools/armor_coverage_check.py: an expiry needs a date, and this is a tool, not
the DSP core.

MUST-FAIL CONTROLS, every run, on a planted tree in a temp git repo: the plant
reads GREEN first, then each of these reads red on its own rule -- an unlisted
submodule, FetchContent, nested fetch, vendored directory, registered licence,
marked file and GUI asset; a changed license text and a changed license; a
missing license file; a moved submodule pin and a moved fetch pin; a new plugin
format; a shipping copyleft id with no approval; an unapproved exception that is
not pending; an expired exception; three SBOM disagreements; a stale doc. Three
must-read-green controls: an approved exception, a pending one, and the plant
with its submodule not checked out. If any control misreads, this check is red.

WHAT IT DOES NOT CHECK. Whether an id is acceptable (the allow list is the
human's), whether the obligations text is complete, platform toolchains and
system frameworks, or third-party code pasted into a file with no marker. The
marker scan reads the INDEX, because the audit is about what the repository
redistributes: a new file is seen once it is added, and local untracked files
(the human's drafts beside the tree) never are.
"""
import argparse
import copy
import datetime
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_sbom  # the SBOM's own readers, so the two cannot drift apart

INVENTORY_REL = "tools/license_inventory.json"
ALLOW_REL = "tools/license_allowlist.json"
DOC_REL = "docs/LICENSES-THIRD-PARTY.md"
# Their own pattern text and quoted copyright lines would count as markers.
SELF = {"tools/license_audit_check.py", INVENTORY_REL, ALLOW_REL, DOC_REL}
SCOPES = ("ships", "build", "test", "docs")
VENDOR_DIRS = {"libs", "lib", "third_party", "thirdparty", "3rdparty", "external",
               "extern", "vendor", "deps"}
CMAKE_NAMES = ("CMakeLists.txt",)

# Case-sensitive on purpose: a case-blind `GPL` matches "logplace".
MARKERS = re.compile(
    r"SPDX-License-Identifier|Permission is hereby granted"
    r"|Redistribution and use in source and binary forms"
    r"|(?i:copyright\s+(?:\(c\)|©|[12][0-9]{3}))|(?i:placed in the public domain)"
    r"|Licensed under the|GNU (?:Lesser |Affero )?General Public License"
    r"|@license\b|/\*!|(?i:generated from .{0,80}do not edit)")
GUI_EXTERNAL = re.compile(
    r"<script[^>]+\bsrc\s*=|<link[^>]+\bhref\s*=|@import\b|@font-face"
    r"|data:(?:font|application/(?:x-)?font)|(?:https?:)?//(?!www\.w3\.org/)[a-z0-9.-]+\.[a-z]{2,}[/\"')]", re.I)
FETCH_OTHER = re.compile(r"\b(ExternalProject_Add|CPMAddPackage|file\s*\(\s*DOWNLOAD)\b", re.I)

_MIT = ("permission is hereby granted, free of charge, to any person obtaining",
        "the above copyright notice and this permission notice shall be included in")
_BSD = ("redistributions of source code must retain", "redistributions in binary form must reproduce")
# First match wins, so the more specific text comes first.
DETECT = [
    ("GPL-3.0", ("gnu general public license", "version 3, 29 june 2007")),
    ("Apache-2.0", ("apache license", "version 2.0, january 2004")),
    ("MIT WITH fmt-exception", _MIT + ("as an exception, if, as a result of your compiling your source code",)),
    ("MIT", _MIT),
    ("ISC", ("permission to use, copy, modify, and/or distribute this software for any purpose "
             "with or without fee is hereby granted",)),
    ("BSD-3-Clause", _BSD + ("endorse or promote products derived from",)),
    ("BSD-2-Clause", _BSD),
    ("LicenseRef-PublicDomain", ("placed in the public domain",)),
]


def normalise(text):
    """License text with comment leaders, markdown bold and layout removed, so a
    re-wrapped or re-commented copy of the same words fingerprints the same."""
    lines = [re.sub(r"^\s*(?://+|/\*+|\*+/?|#+|\||>)\s?", "", ln).replace("**", "")
             for ln in text.splitlines()]
    return re.sub(r"\s+", " ", " ".join(lines)).strip().lower()


def fingerprint(text):
    return "sha256:" + hashlib.sha256(normalise(text).encode("utf-8")).hexdigest()


def detect_spdx(text):
    """-> the id TEXT reads as, or None. A bare SPDX tag counts when no license
    text is present (CPM.cmake carries only the tag)."""
    norm = normalise(text)
    for spdx, phrases in DETECT:
        if all(p in norm for p in phrases):
            return spdx
    tag = re.search(r"SPDX-License-Identifier:\s*([\w.+-]+)", text)
    return tag.group(1) if tag else None


def id_matches(recorded, detected):
    """GPL-3.0-only and GPL-3.0-or-later share one text, and a public-domain
    statement carries its author in the recorded LicenseRef: prefix match."""
    return detected is not None and (recorded == detected or recorded.startswith(detected + "-"))


def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], check=True,
                          capture_output=True, text=True).stdout


def read_index(root):
    """-> ({gitlink path: commit}, [tracked file paths]) from the index."""
    links, files = {}, []
    for line in git(root, "ls-files", "--stage").splitlines():
        meta, path = line.split("\t", 1)
        mode, sha, _stage = meta.split()
        if mode == "160000":
            links[path] = sha
        else:
            files.append(path)
    return links, files


def read_text(path):
    data = path.read_bytes()
    return None if b"\0" in data else data.decode("utf-8", errors="replace")


def checked_out(root, link):
    d = root / link
    return d.is_dir() and any(d.iterdir())


def fetch_pins(cmake_text):
    """-> {FetchContent_Declare name: commit}, through gen_sbom's reader."""
    out = {}
    for comp in gen_sbom.fetched_sdks(cmake_text):
        out[comp["properties"][0]["value"].rsplit(" ", 1)[-1]] = comp["version"]
    return out


def nested_fetches(sub_root):
    """-> names of every package a submodule's own CMake would fetch."""
    names = set()
    for f in sorted(sub_root.rglob("*")):
        if not f.is_file() or not (f.name in CMAKE_NAMES or f.suffix == ".cmake"):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for block in re.findall(r"(?:CPMAddPackage|FetchContent_Declare|ExternalProject_Add)\s*\(([^)]*)\)", text):
            named = re.search(r"\bNAME\s+\"?([\w./-]+)\"?", block)
            short = re.match(r"\s*\"(?:gh|gl|bb):([^#@\"]+)", block)
            first = re.match(r"\s*([\w./-]+)", block)
            hit = named.group(1) if named else short.group(1) if short else first.group(1) if first else None
            if hit:
                names.add(hit)
    return names


def vendored_dirs(root, link):
    """-> repo-relative paths of everything sitting in a vendor directory inside
    submodule LINK, to depth 2 (libs/x and cmake/external/x)."""
    out, base = [], root / link
    for d in [base] + [p for p in sorted(base.iterdir()) if p.is_dir() and p.name != ".git"]:
        for v in sorted(d.iterdir()):
            if v.is_dir() and v.name in VENDOR_DIRS:
                out += [c.relative_to(root).as_posix() for c in sorted(v.iterdir())
                        if not c.name.startswith(".")]
    return out


def choc_registrations(root, tracked):
    """-> {registered name: header} for every licence a choc header REACHABLE from
    our own includes registers. Reachable, not all of choc: it also carries FLAC,
    QuickJS and others we never include."""
    todo, seen, found = [], set(), {}
    for rel in tracked:
        if not rel.endswith((".h", ".hpp", ".cpp", ".mm", ".inc")) or rel.startswith("libs/"):
            continue
        text = read_text(root / rel) if (root / rel).is_file() else None
        for inc in re.findall(r"#\s*include\s*[<\"](?:[./]*libs/choc/)?(choc/[^>\"]+)[>\"]", text or ""):
            todo.append(root / "libs/choc" / inc)
    while todo:
        f = todo.pop().resolve()
        if f in seen or not f.is_file():
            continue
        seen.add(f)
        text = f.read_text(encoding="utf-8", errors="replace")
        for name in re.findall(r"^\s*CHOC_REGISTER_OPEN_SOURCE_LICENCE\s*\(\s*(\w+)", text, re.M):
            found[name] = f.relative_to(root.resolve()).as_posix()
        todo += [f.parent / inc for inc in re.findall(r"#\s*include\s*\"([^\"]+)\"", text)]
    return found


def load_json(path):
    """-> (data, error). Canonical form is part of the contract: a sorted,
    two-space file diffs one entry at a time."""
    try:
        raw = path.read_text(encoding="utf-8")
        data = json.loads(raw)
    except OSError:
        return None, f"{path.name}: cannot be read"
    except ValueError as e:
        return None, f"{path.name}: {e}"
    if raw != json.dumps(data, indent=2, sort_keys=True) + "\n":
        return None, f"{path.name}: not canonical (sorted keys, two-space indent, ASCII, one trailing newline)"
    return data, None


def _date(s):
    try:
        return datetime.date.fromisoformat(s) if isinstance(s, str) else None
    except ValueError:
        return None


def license_text(root, lic):
    """-> the text an entry's `license` block points at, or None if absent."""
    f = root / lic["path"]
    if not f.is_file():
        return None
    text = f.read_text(encoding="utf-8", errors="replace")
    if "lines" in lic:
        a, b = lic["lines"]
        text = "\n".join(text.splitlines()[a - 1:b])
    return text


def audit(root, inv, allow, today, sbom_rows):
    """-> (fails [(rule, msg)], warnings, pending, notes). SBOM_ROWS is
    {component name: version} as tools/gen_sbom.py emits them."""
    root = pathlib.Path(root)
    fails, warns, pending, notes = [], [], [], []
    comps = inv.get("components", {})
    links, tracked = read_index(root)
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    absent = sorted(p for p in links if not checked_out(root, p))
    for p in absent:
        warns.append(f"{p} is not checked out; the rows that read its files DID NOT RUN")

    def under_absent(rel):
        return any(rel == p or rel.startswith(p + "/") for p in absent)

    # (f) shape, before anything reads a field.
    for cid, c in sorted(comps.items()):
        lacking = [k for k in ("name", "version", "pin", "where", "scope", "spdx", "license", "obligations")
                   if k not in c]
        if lacking:
            fails.append(("f", f"{cid}: lacks {', '.join(lacking)}"))
        elif c["scope"] not in SCOPES:
            fails.append(("f", f"{cid}: scope {c['scope']!r} is not one of {', '.join(SCOPES)}"))
        elif not (c.get("sbom") or c.get("sbom_absent")):
            fails.append(("f", f"{cid}: needs `sbom` (its SBOM component names) or `sbom_absent` (why not)"))
    if fails:
        return fails, warns, pending, notes
    by_kind = {}
    for cid, c in comps.items():
        by_kind.setdefault(c["pin"]["kind"], {})[cid] = c

    # (a)+(c) submodules.
    subs = {c["pin"]["path"]: (cid, c) for cid, c in by_kind.get("submodule", {}).items()}
    for path, sha in sorted(links.items()):
        if path not in subs:
            fails.append(("a", f"submodule {path} is not in the inventory"))
        elif subs[path][1]["pin"]["commit"] != sha:
            fails.append(("c", f"{subs[path][0]}: {path} is pinned at {sha}, the entry says "
                               f"{subs[path][1]['pin']['commit']}; re-read its license and update the entry"))
    for path in sorted(set(subs) - set(links)):
        fails.append(("a", f"{subs[path][0]}: the inventory lists submodule {path}, the tree has none"))

    # (a)+(c) fetches the root build declares.
    try:
        pins = fetch_pins(cmake)
    except gen_sbom.SbomError as e:
        pins = {}
        fails.append(("a", f"CMakeLists.txt FetchContent could not be read: {e}"))
    fetched = {c["pin"]["declare"]: (cid, c) for cid, c in by_kind.get("fetchcontent", {}).items()}
    for name, commit in sorted(pins.items()):
        if name not in fetched:
            fails.append(("a", f"FetchContent_Declare({name}) is not in the inventory"))
        elif fetched[name][1]["pin"]["commit"] != commit:
            fails.append(("c", f"{fetched[name][0]}: CMakeLists.txt pins {name} at {commit}, the entry says "
                               f"{fetched[name][1]['pin']['commit']}; run --online and update the entry"))
    for name in sorted(set(fetched) - set(pins)):
        fails.append(("a", f"{fetched[name][0]}: the inventory lists FetchContent {name}, CMakeLists.txt has none"))
    for rel in tracked:
        if (rel.rsplit("/", 1)[-1] in CMAKE_NAMES or rel.endswith(".cmake")) and (root / rel).is_file():
            m = FETCH_OTHER.search(re.sub(r"(?m)#.*$", "", (root / rel).read_text(encoding="utf-8")))
            if m:
                fails.append(("a", f"{rel} uses {m.group(1)}, a fetch this check cannot read; "
                                   "inventory what it fetches and teach the check"))

    # (c) what the "not built" claims rest on.
    build = inv.get("build", {})
    formats = re.search(r"^\s*set\(HYPERSAW_PLUGIN_FORMATS\s+([^)]*)\)", cmake, re.M)
    got = sorted(formats.group(1).split()) if formats else None
    if got != sorted(build.get("plugin_formats", [])):
        fails.append(("c", f"CMakeLists.txt builds plugin formats {got}, the inventory says "
                           f"{build.get('plugin_formats')}; a new format can pull in a new SDK"))
    target = re.search(r"^\s*set\(CMAKE_OSX_DEPLOYMENT_TARGET\s+\"([0-9.]+)\"", cmake, re.M)
    if not target or tuple(int(n) for n in target.group(1).split(".")) < (10, 15):
        fails.append(("c", f"CMakeLists.txt's macOS deployment target is {target.group(1) if target else 'unset'}: "
                           "below 10.15 clap-wrapper fetches gulrak/filesystem, which is recorded as not built"))
    if not re.search(r"^\s*set\(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES\s+OFF\b", cmake, re.M):
        fails.append(("c", "CMakeLists.txt no longer sets CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF: "
                           "clap-wrapper may now fetch SDKs by tag, unpinned and uninventoried"))

    # (a) what sits inside each submodule.
    wheres = {c["where"] for c in comps.values()}
    registered = {c["pin"].get("registered_as") for c in comps.values()} - {None}
    for path in sorted(links):
        if path in absent:
            continue
        recorded = inv.get("nested_fetches", {}).get(path, {})
        for name in sorted(nested_fetches(root / path) - set(recorded)):
            fails.append(("a", f"{path}'s CMake can fetch {name!r}; record it in nested_fetches as "
                               "covered by an entry or as not built"))
        for name, how in sorted(recorded.items()):
            if how.startswith("covered:") and how[len("covered:"):] not in comps:
                fails.append(("a", f"nested_fetches {path}/{name}: {how!r} names no entry"))
        for rel in vendored_dirs(root, path):
            if rel not in wheres:
                fails.append(("a", f"{rel} is vendored inside {path} and is not in the inventory"))
    if "libs/choc" in links and "libs/choc" not in absent:
        for name, header in sorted(choc_registrations(root, tracked).items()):
            if name not in registered:
                fails.append(("a", f"{header} registers the licence of {name}, embedded code we compile; "
                                   "it is not in the inventory"))

    # (a) our own tracked files: license markers and GUI assets.
    claimed = set(inv.get("own_files", {})) | SELF
    for c in by_kind.get("tracked-file", {}).values():
        claimed |= set(c["pin"]["files"])
    scanned = gui_pages = 0
    for rel in tracked:
        f = root / rel
        if not f.is_file():
            continue            # tracked, deleted in the working tree
        text = read_text(f)
        if text is None:
            continue            # binary
        scanned += 1
        m = MARKERS.search(text)
        if m and rel not in claimed:
            fails.append(("a", f"{rel} carries a license marker ({m.group(0)[:40]!r}) and no entry claims it"))
        if rel.startswith("src/gui/") and rel.endswith(".html"):
            gui_pages += 1
            m = GUI_EXTERNAL.search(text)
            if m:
                fails.append(("a", f"{rel} is a shipped GUI page and references an external asset "
                                   f"({m.group(0)[:60]!r}); inventory it"))
    notes.append(f"{scanned} tracked text files scanned for license markers, "
                 f"{gui_pages} shipped GUI pages for external assets")
    if not gui_pages:
        fails.append(("a", "no src/gui/*.html found: the GUI asset scan read nothing"))

    # (b) license text, (c) file and version pins.
    for cid, c in sorted(comps.items()):
        lic, pin = c["license"], c["pin"]
        if lic["source"] == "tree" and not under_absent(lic["path"]):
            text = license_text(root, lic)
            if text is None:
                fails.append(("b", f"{cid}: license file {lic['path']} is missing"))
            else:
                seen = detect_spdx(text)
                if not id_matches(c["spdx"], seen):
                    fails.append(("b", f"{cid}: {lic['path']} reads as {seen}, the entry says {c['spdx']}"))
                elif fingerprint(text) != lic.get("fingerprint"):
                    fails.append(("b", f"{cid}: the license text in {lic['path']} changed "
                                       f"(now {fingerprint(text)}); re-read it and update the entry"))
        elif lic["source"] == "none" and c["spdx"] != "NOASSERTION":
            fails.append(("b", f"{cid}: no license text on record, so the id must be NOASSERTION"))
        elif lic["source"] not in ("tree", "upstream", "none"):
            fails.append(("f", f"{cid}: license source {lic['source']!r} is not tree, upstream or none"))
        for rel, digest in sorted(pin.get("files", {}).items()):
            f = root / rel
            now = hashlib.sha256(f.read_bytes()).hexdigest() if f.is_file() else "absent"
            if now != digest:
                fails.append(("c", f"{cid}: {rel} is {now}, the entry pins {digest}"))
        probe = pin.get("version_probe")
        if probe and not under_absent(probe["path"]):
            f = root / probe["path"]
            m = re.search(probe["regex"], f.read_text(encoding="utf-8", errors="replace")) if f.is_file() else None
            if not m or m.group(1) != probe["value"]:
                fails.append(("c", f"{cid}: {probe['path']} reads version {m.group(1) if m else None}, "
                                   f"the entry says {probe['value']}"))
        if pin["kind"] == "rides-with" and pin.get("parent") not in comps:
            fails.append(("f", f"{cid}: rides with {pin.get('parent')!r}, which is not an entry"))
        # An upstream text is held by its commit, so the URL must name the pin:
        # a bumped pin with the old URL is a license nobody re-read.
        at = pin.get("commit") or pin.get("tag_commit")
        if lic["source"] == "upstream" and (not at or f"/{at}/" not in lic.get("url", "")):
            fails.append(("c", f"{cid}: its license url does not name the pinned commit {at}"))

    # (d) the allow list.
    ships_ok = set(allow.get("ships", {}))
    wider_ok = ships_ok | set(allow.get("not_shipped", {}))
    exceptions = allow.get("exceptions", {})
    raised = {p.get("component"): p.get("raised") for p in allow.get("pending_human", [])}
    usable = {}
    for cid, x in sorted(exceptions.items()):
        lacking = [k for k in ("spdx", "status", "note", "approved", "expires") if k not in x]
        if lacking:
            fails.append(("f", f"exception {cid}: lacks {', '.join(lacking)}"))
        elif _date(x["expires"]) is None:
            fails.append(("f", f"exception {cid}: expires {x['expires']!r} is not an ISO date"))
        elif (x["status"], bool(x["approved"])) not in (("needs-human", False), ("approved", True)):
            fails.append(("f", f"exception {cid}: status {x['status']!r} with approved {x['approved']!r}; "
                               "needs-human goes with an empty approved, approved with the human's ref"))
        elif cid not in comps:
            fails.append(("d", f"exception {cid}: no such entry"))
        else:
            usable[cid] = x
    for cid, when in sorted(raised.items()):
        if _date(when) is None:
            fails.append(("f", f"pending_human {cid}: raised {when!r} is not an ISO date"))
        if cid not in usable or usable[cid]["approved"]:
            fails.append(("d", f"pending_human names {cid}, which has no unapproved exception; remove it"))
    for cid, c in sorted(comps.items()):
        ships = c["scope"] == "ships"
        x = usable.get(cid)
        if c["spdx"] in (ships_ok if ships else wider_ok):
            if cid in exceptions:
                fails.append(("d", f"exception {cid}: {c['spdx']} is on the allow list now; remove the exception"))
            continue
        what = f"{cid} {'ships' if ships else 'is ' + c['scope'] + '-only'} under {c['spdx']}"
        if cid in exceptions and x is None:
            continue            # its shape failure is already reported
        if x is None:
            fails.append(("d", f"{what}, which is not on the {'shipping' if ships else 'wider'} allow list "
                               "and has no exception"))
        elif x["spdx"] != c["spdx"]:
            fails.append(("d", f"{what}; its exception is for {x['spdx']}"))
        elif today > _date(x["expires"]):
            fails.append(("d", f"{what}; its exception expired {x['expires']}"))
        elif x["approved"]:
            notes.append(f"exception: {cid} ({c['spdx']}) approved by {x['approved']} until {x['expires']}")
        elif cid in raised:
            pending.append(f"{cid} ({c['spdx']}, {'ships' if ships else c['scope'] + '-only'}), raised "
                           f"{raised[cid]}, expires {x['expires']}: {x['note']}")
        else:
            fails.append(("d", f"{what}; its exception is not approved and is not in pending_human"))

    # (e) the SBOM. A ci-download has no commit; its SBOM version is its release.
    want = {}
    for cid, c in comps.items():
        for name in c.get("sbom", []):
            want[name] = (cid, c["pin"].get("commit") or c["pin"].get("version"))
    for name, ver in sorted(sbom_rows.items()):
        if name not in want:
            fails.append(("e", f"the SBOM lists {name} {ver}; no inventory entry names it"))
        elif want[name][1] != ver:
            fails.append(("e", f"{want[name][0]}: the SBOM has {name} at {ver}, the entry at {want[name][1]}"))
    for name in sorted(set(want) - set(sbom_rows)):
        fails.append(("e", f"{want[name][0]}: the entry names SBOM component {name}; the SBOM has none"))
    outside = sorted(cid for cid, c in comps.items() if not c.get("sbom"))
    if outside:
        shipping = [cid for cid in outside if comps[cid]["scope"] == "ships"]
        notes.append(f"NOT IN THE SBOM (nested inside another pin, or a tracked file): {', '.join(outside)}; "
                     f"{len(shipping)} of them ship ({', '.join(shipping) or 'none'})")
    return fails, warns, pending, notes


def allow_status(cid, c, allow):
    """One table cell: where the allow list leaves this entry. No date is read,
    so the generated doc does not change from one day to the next."""
    ships = c["scope"] == "ships"
    if c["spdx"] in allow.get("ships", {}):
        return "allowed (shipping list)"
    if not ships and c["spdx"] in allow.get("not_shipped", {}):
        return "allowed (not-shipped list)"
    x = allow.get("exceptions", {}).get(cid)
    if x is None:
        return "NOT ALLOWED"
    if x.get("approved"):
        return f"exception approved ({x['approved']}) until {x['expires']}"
    return f"**PENDING THE HUMAN** until {x['expires']}"


def render_doc(inv, allow):
    comps = inv["components"]
    order = sorted(comps, key=lambda cid: (SCOPES.index(comps[cid]["scope"]), cid))
    cell = lambda s: str(s).replace("|", "\\|")
    out = ["# Third-party licenses",
           "",
           "*GENERATED by `python3 tools/license_audit_check.py --write-doc` from "
           f"`{INVENTORY_REL}` and `{ALLOW_REL}`. Do not edit: `./verify fast` fails when this "
           "file is stale. This is an inventory of facts read from each component's own license "
           "text at its pinned revision. It is not legal advice, and the allow list is "
           f"**{allow.get('status', 'PROPOSED')}**.*",
           "",
           "## Inventory",
           "",
           "| Component | Version | Pin | Where | Ships? | SPDX id | License text | Allow list |",
           "|---|---|---|---|---|---|---|---|"]
    for cid in order:
        c = comps[cid]
        pin, lic = c["pin"], c["license"]
        at = pin.get("commit") or pin.get("tag_commit")
        how = (f"{pin['kind']} `{at[:12]}`" if at else
               f"rides with {pin['parent']}" if pin["kind"] == "rides-with" else pin["kind"])
        where = ("none found" if lic["source"] == "none" else
                 f"`{lic['path']}`" + (f" lines {lic['lines'][0]}-{lic['lines'][1]}" if "lines" in lic else "")
                 if lic["source"] == "tree" else f"[upstream at the pin]({lic['url']})")
        ships = "**ships**" + (f" ({c['ships_in']})" if c.get("ships_in") else "") if c["scope"] == "ships" \
            else f"no: {c['scope']} only"
        out.append("| " + " | ".join(cell(v) for v in (
            c["name"], c["version"], how, f"`{c['where']}`", ships, f"`{c['spdx']}`", where,
            allow_status(cid, c, allow))) + " |")
    out += ["", "## Exceptions and items pending the human", ""]
    exceptions = allow.get("exceptions", {})
    raised = {p["component"]: p["raised"] for p in allow.get("pending_human", [])}
    if not exceptions:
        out.append("None.")
    for cid, x in sorted(exceptions.items()):
        state = f"approved: {x['approved']}" if x["approved"] else \
            f"**PENDING THE HUMAN** (raised {raised.get(cid, 'NOT LISTED')})"
        out += [f"- **{comps[cid]['name']}** (`{x['spdx']}`, {comps[cid]['scope']}): {state}, "
                f"expires {x['expires']}. {x['note']}"]
    out += ["",
            "A pending item is a visible hole, not a pass: the check exits 0 on it only while it is "
            "listed in `pending_human`, and it goes red on its expiry date.",
            "", "## Obligations for a release", "",
            "What each component that ships asks of a release, as its own text states it. The "
            "copyright lines are the ones each text names; whether and where they ship is question 1.",
            ""]
    for cid in order:
        c = comps[cid]
        if c["scope"] != "ships":
            continue
        out += [f"### {c['name']} (`{c['spdx']}`)", "",
                f"- Copyright line: {c.get('copyright', 'none stated')}"]
        out += [f"- {o}" for o in c["obligations"]]
        out.append("")
    out += ["### Questions for the human", "",
            "These are open. Nothing here decides them.", ""]
    out += [f"{i}. **{q['title']}** {q['question']}" for i, q in enumerate(inv.get("release_questions", []), 1)]
    out += ["", "## Not shipped", ""]
    for cid in order:
        c = comps[cid]
        if c["scope"] != "ships":
            out += [f"- **{c['name']}** (`{c['spdx']}`, {c['scope']} only): " + " ".join(c["obligations"])]
    out += ["", "## Fetches a submodule declares", "",
            "What each submodule's own CMake could download. `covered:<entry>` means the root build "
            "supplies that component from the named entry's pin, so the submodule's download does not "
            "run; `not-built` gives the reason the fetch is never reached.", ""]
    for path, names in sorted(inv.get("nested_fetches", {}).items()):
        out += [f"- `{path}` / `{name}`: {how}" for name, how in sorted(names.items())]
    out += ["", "## The allow list", "", f"Status: **{allow.get('status', 'PROPOSED')}**.", "",
            "Ids a component that SHIPS may carry:", ""]
    out += [f"- `{k}`: {v}" for k, v in sorted(allow.get("ships", {}).items())]
    out += ["", "Further ids a build-, test- or docs-only component may carry:", ""]
    out += [f"- `{k}`: {v}" for k, v in sorted(allow.get("not_shipped", {}).items())]
    out += ["", "## What this audit does not cover", ""]
    out += [f"- {line}" for line in inv.get("not_covered", [])]
    return "\n".join(out) + "\n"


def doc_fails(root, inv, allow):
    f = pathlib.Path(root) / DOC_REL
    if not f.is_file() or f.read_text(encoding="utf-8") != render_doc(inv, allow):
        return [("g", f"{DOC_REL} is stale or missing; run tools/license_audit_check.py --write-doc")]
    return []


# ---- must-fail controls: a planted tree, synthetic names only -------------------
_T_MIT = ("MIT License\n\nCopyright (c) 2020 Plant Author\n\nPermission is hereby granted, free of charge, "
          "to any person obtaining a copy of this software.\nThe above copyright notice and this permission "
          "notice shall be included in all copies.\n")
_T_ISC = ("// Copyright (c) 2021 Plant Inner\n// Permission to use, copy, modify, and/or distribute this software "
          "for any purpose with or without fee is hereby granted.\n")
_T_BSD = ("Copyright (C) Plant Loader.\nRedistributions of source code must retain the above.\nRedistributions in "
          "binary form must reproduce the above.\nNames may not be used to endorse or promote products derived "
          "from this software.\n")
_T_GPL = "GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n"
_C_DEP, _C_CHOC, _C_SDK = "1" * 40, "3" * 40, "2" * 40
_CMAKE = ("include(FetchContent)\nset(PLANT_SDK_COMMIT %s)\nFetchContent_Declare(plant_sdk\n"
          "  GIT_REPOSITORY https://github.com/plant/sdk.git\n  GIT_TAG ${PLANT_SDK_COMMIT})\n"
          "set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL \"\" FORCE)\n"
          "set(CMAKE_OSX_DEPLOYMENT_TARGET \"11.0\" CACHE STRING \"\" FORCE)\n"
          "set(HYPERSAW_PLUGIN_FORMATS CLAP VST3)\n")
_WEB_H = "// web view\nCHOC_REGISTER_OPEN_SOURCE_LICENCE (Loader, R\"(\n" + _T_BSD + ")\")\n"
_NESTED = "CPMAddPackage(\n  NAME \"sdk\"\n  GIT_TAG \"v1\")\nCPMAddPackage(\"gh:plant/unbuilt#v1\")\n"


class Plant:
    """A miniature horde in a fresh git repo: two submodules (gitlinks with their
    files beside them), one fetched SDK, one CI tool, one vendored bundle."""

    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.root.mkdir(parents=True)
        git(self.root, "init", "-q")
        self.write("CMakeLists.txt", _CMAKE % _C_SDK)
        self.write("src/gui.h", "#include <choc/gui/web.h>\n")
        self.write("src/gui/page.html", '<svg xmlns="http://www.w3.org/2000/svg"></svg>\n<script>// a.b/\n</script>\n')
        self.write("docs/bundle.js", "/*! bundle */\n" + _T_MIT)
        self.gitlink("libs/dep", _C_DEP)
        self.gitlink("libs/choc", _C_CHOC)
        self.write("libs/dep/LICENSE", _T_MIT, track=False)
        self.write("libs/dep/libs/inner/inner.h", _T_ISC, track=False)
        self.write("libs/dep/cmake/deps.cmake", _NESTED, track=False)
        self.write("libs/choc/LICENSE.md", _T_ISC, track=False)
        self.write("libs/choc/choc/gui/web.h", _WEB_H, track=False)

        def entry(pin, where, scope, spdx, lic, **more):
            return dict(name=where, version="1", pin=pin, where=where, scope=scope, spdx=spdx,
                        license=lic, obligations=["keep the notice"], **more)

        def tree(path, text, **more):
            return dict(source="tree", path=path, fingerprint=fingerprint(text), **more)

        up = dict(source="upstream", url=f"https://raw.githubusercontent.com/plant/sdk/{_C_SDK}/LICENSE", fingerprint="x")
        self.inv = {"build": {"plugin_formats": ["CLAP", "VST3"]}, "own_files": {},
                    "nested_fetches": {"libs/dep": {"sdk": "covered:sdk", "plant/unbuilt": "not-built: plant"}},
                    "components": {
            "dep": entry({"kind": "submodule", "path": "libs/dep", "commit": _C_DEP}, "libs/dep", "ships", "MIT",
                         tree("libs/dep/LICENSE", _T_MIT), sbom=["dep"]),
            "choc": entry({"kind": "submodule", "path": "libs/choc", "commit": _C_CHOC}, "libs/choc", "ships", "ISC",
                          tree("libs/choc/LICENSE.md", _T_ISC), sbom=["choc"]),
            "inner": entry({"kind": "rides-with", "parent": "dep"}, "libs/dep/libs/inner", "ships", "ISC",
                           tree("libs/dep/libs/inner/inner.h", _T_ISC), sbom_absent="nested"),
            "loader": entry({"kind": "rides-with", "parent": "choc", "registered_as": "Loader"},
                            "libs/choc/choc/gui/web.h", "ships", "BSD-3-Clause",
                            tree("libs/choc/choc/gui/web.h", _T_BSD, lines=[3, 6]), sbom_absent="nested"),
            "sdk": entry({"kind": "fetchcontent", "declare": "plant_sdk", "commit": _C_SDK}, "build tree", "ships",
                         "Apache-2.0", up, sbom=["sdk"]),
            "tool": entry({"kind": "ci-download", "version": "v1", "tag_commit": _C_SDK}, "CI", "test",
                          "GPL-3.0-only", up, sbom=["tool.zip"]),
            "bundle": entry({"kind": "tracked-file",
                             "files": {"docs/bundle.js": hashlib.sha256(("/*! bundle */\n" + _T_MIT).encode()).hexdigest()}},
                            "docs/bundle.js", "docs", "MIT", tree("docs/bundle.js", _T_MIT, lines=[2, 7]),
                            sbom_absent="tracked file")}}
        self.allow = {"status": "PLANT", "ships": {"MIT": "", "ISC": "", "BSD-3-Clause": "", "Apache-2.0": ""},
                      "not_shipped": {"GPL-3.0-only": ""}, "exceptions": {}, "pending_human": []}
        self.sbom = {"dep": _C_DEP, "choc": _C_CHOC, "sdk": _C_SDK, "tool.zip": "v1"}
        self.write(DOC_REL, render_doc(self.inv, self.allow))

    def write(self, rel, text, track=True):
        f = self.root / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")
        if track:
            git(self.root, "add", "--", rel)

    def gitlink(self, path, sha):
        git(self.root, "update-index", "--add", "--cacheinfo", f"160000,{sha},{path}")

    def copyleft(self, **exception):
        """libs/dep really IS GPL here, text and entry both, so only the allow
        list can object. An exception, when given, is filed for it."""
        self.write("libs/dep/LICENSE", _T_GPL, track=False)
        self.inv["components"]["dep"].update(spdx="GPL-3.0-only", license={
            "source": "tree", "path": "libs/dep/LICENSE", "fingerprint": fingerprint(_T_GPL)})
        if exception:
            self.allow["exceptions"]["dep"] = dict(
                {"spdx": "GPL-3.0-only", "status": "needs-human", "note": "n", "approved": "",
                 "expires": "2030-01-01"}, **exception)

    def run(self, today):
        return audit(self.root, self.inv, self.allow, today, self.sbom)[:3]


def _controls():
    """-> [(label, rule expected or None for green, mutation, extra predicate)]."""
    approved = dict(status="approved", approved="ADR-000")
    listed = lambda p: p.allow["pending_human"].append({"component": "dep", "raised": "2026-10-09"})

    def pend(p, **x):
        p.copyleft(**x)
        listed(p)

    def gone(p):
        for f in sorted((p.root / "libs/dep").rglob("*"), reverse=True):
            f.unlink() if f.is_file() else f.rmdir()

    one = lambda fails, warns, pending: len(pending) == 1
    none = lambda fails, warns, pending: not pending
    warned = lambda fails, warns, pending: len(warns) == 1
    return [
        ("unlisted submodule", "a", lambda p: p.gitlink("libs/fresh", "4" * 40), None),
        ("unlisted FetchContent", "a", lambda p: p.write("CMakeLists.txt", _CMAKE % _C_SDK + (
            "FetchContent_Declare(plant_two\n  GIT_REPOSITORY https://github.com/plant/two.git\n"
            "  GIT_TAG %s)\n" % ("6" * 40))), None),
        ("unreadable fetch mechanism", "a", lambda p: p.write("cmake/get.cmake", "ExternalProject_Add(zed)\n"), None),
        ("unlisted nested fetch", "a", lambda p: p.write(
            "libs/dep/cmake/deps.cmake", _NESTED + "CPMAddPackage(\n  NAME \"fresh\")\n", track=False), None),
        ("unlisted vendored directory", "a", lambda p: p.write("libs/dep/libs/other/o.h", "int o;\n", track=False), None),
        ("unlisted registered licence", "a", lambda p: p.write(
            "libs/choc/choc/gui/web.h", _WEB_H + "CHOC_REGISTER_OPEN_SOURCE_LICENCE (Fresh, R\"(x)\")\n", track=False), None),
        ("unlisted file with a license marker", "a", lambda p: p.write("src/pasted.h", "// Copyright (c) 2019 Someone\n"), None),
        ("GUI page loads an external script", "a", lambda p: p.write(
            "src/gui/page.html", '<script src="https://cdn.example.com/x.js"></script>\n'), None),
        ("changed license text", "b", lambda p: p.write(
            "libs/dep/LICENSE", _T_MIT.replace("Plant Author", "Someone Else"), track=False), None),
        ("changed license", "b", lambda p: p.write("libs/dep/LICENSE", _T_GPL, track=False), None),
        ("missing license file", "b", lambda p: (p.root / "libs/dep/LICENSE").unlink(), None),
        ("moved submodule pin", "c", lambda p: p.gitlink("libs/dep", "5" * 40), None),
        ("moved fetch pin", "c", lambda p: p.write("CMakeLists.txt", _CMAKE % ("7" * 40)), None),
        ("changed vendored file", "c", lambda p: p.write("docs/bundle.js", "/*! bundle 2 */\n" + _T_MIT), None),
        ("new plugin format", "c", lambda p: p.write(
            "CMakeLists.txt", (_CMAKE % _C_SDK).replace("CLAP VST3", "CLAP VST3 STANDALONE")), None),
        ("macOS target that turns a nested fetch on", "c", lambda p: p.write(
            "CMakeLists.txt", (_CMAKE % _C_SDK).replace('"11.0"', '"10.13"')), None),
        ("download switch removed", "c", lambda p: p.write(
            "CMakeLists.txt", (_CMAKE % _C_SDK).replace("DEPENDENCIES OFF", "DEPENDENCIES ON")), None),
        ("shipping copyleft id, no approval", "d", lambda p: p.copyleft(), None),
        ("unapproved exception, not pending", "d", lambda p: p.copyleft(status="needs-human"), None),
        ("pending exception reads PENDING, exit 0", None, lambda p: pend(p, status="needs-human"), one),
        ("approved exception reads green", None, lambda p: p.copyleft(**approved), none),
        ("expired exception", "d", lambda p: p.copyleft(expires="2026-10-08", **approved), None),
        ("expired pending exception", "d", lambda p: pend(p, expires="2026-10-08"), None),
        ("pending_human with no exception", "d", listed, None),
        ("SBOM version differs", "e", lambda p: p.sbom.update(dep="9" * 40), None),
        ("SBOM lacks a component", "e", lambda p: p.sbom.pop("sdk"), None),
        ("SBOM has an extra component", "e", lambda p: p.sbom.update(extra="1"), None),
        ("entry lacks a field", "f", lambda p: p.inv["components"]["dep"].pop("spdx"), None),
        ("submodule not checked out: warning, no failure", None, gone, warned),
    ]


def selftest():
    """-> ([(label, verdict)], failure or None). Dates are fixed: no wall clock."""
    today = datetime.date(2026, 10, 9)
    results = []
    with tempfile.TemporaryDirectory() as td:
        plant = Plant(pathlib.Path(td) / "base")
        base = plant.run(today)
        fresh = doc_fails(plant.root, plant.inv, plant.allow)
        results.append(("unplanted tree reads green", "green" if not (base[0] or fresh) else "RED"))
        if base[0] or base[1] or base[2] or fresh:
            return results, f"the unplanted control tree did not read clean: {base[0][:1] or base[1] or base[2] or fresh}"
        plant.write(DOC_REL, render_doc(plant.inv, plant.allow) + "edited by hand\n")
        stale = {r for r, _ in doc_fails(plant.root, plant.inv, plant.allow)} == {"g"}
        results.append(("stale doc", "red" if stale else "MISREAD"))
        if not stale:
            return results, "control 'stale doc' did not read red on rule (g)"
        for i, (label, rule, mutate, also) in enumerate(_controls()):
            p = Plant(pathlib.Path(td) / f"c{i}")
            mutate(p)
            fails, warns, pending = p.run(today)
            rules = {r for r, _ in fails}
            ok = (rules == {rule}) if rule else not fails
            ok = ok and (also is None or also(fails, warns, pending))
            results.append((label, ("red" if rule else "green") if ok else "MISREAD"))
            if not ok:
                return results, (f"control '{label}' expected {'rule (' + rule + ') only' if rule else 'green'}, "
                                 f"read {sorted(rules) or 'green'}: {[m for _, m in fails][:2]}, "
                                 f"{len(warns)} warnings, {len(pending)} pending")
    for raw, label in (('{"b": 1, "a": 2}\n', "non-canonical JSON"), ('{"a": [ ', "invalid JSON")):
        with tempfile.TemporaryDirectory() as td:
            f = pathlib.Path(td) / "x.json"
            f.write_text(raw, encoding="utf-8")
            bad = load_json(f)[1] is not None
        results.append((label, "red" if bad else "MISREAD"))
        if not bad:
            return results, f"control '{label}' loaded without an error"
    return results, None


def online(inv):
    """Re-read every upstream license text at its recorded commit. Needs the
    network, so it is a step after a pin bump, never part of verify."""
    bad = 0
    for cid, c in sorted(inv["components"].items()):
        lic = c["license"]
        if lic["source"] != "upstream":
            continue
        if not lic["url"].startswith("https://raw.githubusercontent.com/"):
            print(f"  {cid}: RED -- {lic['url']} is not a raw.githubusercontent.com URL")
            bad += 1
            continue
        try:
            with urllib.request.urlopen(lic["url"], timeout=30) as r:
                text = r.read().decode("utf-8", errors="replace")
        except OSError as e:
            print(f"  {cid}: RED -- could not read {lic['url']} ({e})")
            bad += 1
            continue
        seen, now = detect_spdx(text), fingerprint(text)
        ok = id_matches(c["spdx"], seen) and now == lic["fingerprint"]
        bad += not ok
        print(f"  {cid}: {'ok' if ok else 'RED'} -- reads as {seen}, {now}" +
              ("" if ok else f"; the entry says {c['spdx']}, {lic['fingerprint']}"))
    print(f"license_audit_check --online: {'FAILED' if bad else 'GREEN'}")
    return 1 if bad else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--today")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write-doc", action="store_true")
    mode.add_argument("--online", action="store_true")
    a = ap.parse_args(argv[1:])
    today = _date(a.today) if a.today else datetime.date.today()
    if today is None:
        print(f"license_audit_check: --today {a.today!r} is not an ISO date", file=sys.stderr)
        return 2

    inv, e1 = load_json(ROOT / INVENTORY_REL)
    allow, e2 = load_json(ROOT / ALLOW_REL)
    if e1 or e2:
        print(f"license_audit_check: FAILED -- (f) {e1 or e2}", file=sys.stderr)
        return 1
    if a.online:
        return online(inv)
    if a.write_doc:
        (ROOT / DOC_REL).write_text(render_doc(inv, allow), encoding="utf-8")
        print(f"license_audit_check: wrote {DOC_REL}")
        return 0
    if a.check:
        stale = doc_fails(ROOT, inv, allow)
        print(f"license_audit_check: {stale[0][1]}" if stale else f"license_audit_check: {DOC_REL} is current",
              file=sys.stderr if stale else sys.stdout)
        return 1 if stale else 0

    results, bad = selftest()
    if bad:
        print(f"license_audit_check: FAILED -- control: {bad}", file=sys.stderr)
        for label, verdict in results:
            print(f"    {label}: {verdict}", file=sys.stderr)
        return 1
    try:
        bom = gen_sbom.build_sbom(ROOT, "0.0.0")
        sbom_rows = {c["name"]: c["version"] for c in bom["components"]}
        fails, warns, pending, notes = audit(ROOT, inv, allow, today, sbom_rows)
    except (gen_sbom.SbomError, subprocess.CalledProcessError, OSError, KeyError, TypeError) as e:
        print(f"license_audit_check: FAILED -- could not read the tree ({type(e).__name__}: {e})", file=sys.stderr)
        return 1
    fails += doc_fails(ROOT, inv, allow)
    for w in warns:
        print(f"verify: WARNING -- license audit: {w}", file=sys.stderr)
    if fails:
        print(f"license_audit_check: FAILED ({len(fails)})", file=sys.stderr)
        for rule, msg in fails:
            print(f"  ({rule}) {msg}", file=sys.stderr)
        return 1
    for n in notes:
        print(f"  note  {n}")
    for p in pending:
        print(f"license_audit_check: PENDING THE HUMAN -- {p}")
    comps = inv["components"].values()
    counts = ", ".join(f"{sum(c['scope'] == s for c in comps)} {s}" for s in SCOPES)
    red = sum(v == "red" for _, v in results)
    summary = (f"{len(inv['components'])} components: {counts}; {red} controls red and "
               f"{len(results) - red} green as planted")
    if pending:
        print(f"license_audit_check: 0 failures, {len(pending)} PENDING THE HUMAN -- a visible hole, "
              f"not a pass ({summary})")
    else:
        print(f"license_audit_check: GREEN ({summary})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
