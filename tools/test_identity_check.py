#!/usr/bin/env python3
"""test_identity_check -- the real plugin and its side-by-side TEST identity share no id (B456).

WIRED: ./verify fast (static rows); ./verify full adds --built (the built bundles, macOS).

WHY. A host finds a plugin by identity, not by name: the CLAP id, the VST3 class id derived from
it, the AU type/subtype/manufacturer triple. The same sources can be built under a second
identity (CMake option HORDE_TEST_IDENTITY, default OFF) so an unmerged change can be tried in a
real host BESIDE the plugin real projects load. That is only safe while two things hold, and no
audio oracle sees either:
  - the two identities share NOTHING a host or the disk keys on. A test build that kept the real
    CLAP id or AU subtype would be loaded IN PLACE of the real plugin; one that kept the store
    folder would read and write the user's presets;
  - the real identity never moves. A changed id orphans every saved session.

WHAT IS READ. Statically, no build: src/plugin_identity.h (CLAP id, display name, store folder,
and the one #ifdef that selects between the two), CMakeLists.txt's identity block (output name,
bundle identifier, AU subtype, and the option), the two consumers (the descriptor in
src/hypersaw_clap.cpp, the path helper in src/gui/preset_store.h), and every file under src/,
h2/ and tools/ for hard-coded copies.

ROWS (each failure carries a tag; the controls below assert the TAG, so a control cannot pass
by going red for some other reason).
  frozen     the real identity equals FROZEN_REAL, the table in this file. Changing the real
             plugin's identity therefore takes an edit HERE too, in the same PR, where a
             reviewer sees it. The store path's three per-platform forms are pinned with it.
  share      CLAP id, display name, store folder, bundle identifier, output name and AU subtype
             all differ between the two. Folder, bundle identifier and output name are compared
             case-folded: the default macOS volume is case-insensitive, so HYPERSAW and hypersaw
             are one folder. The store folder must be one path component (a sibling, never a
             child of the real one).
  option     HORDE_TEST_IDENTITY is declared once, defaults OFF, is never set by the build
             itself, and its compile definition exists only inside the option's ON branch.
  wiring     the header selects kTest under the definition and kReal otherwise; the descriptor
             and the store path read kIdentity; the plugin target takes its output name, bundle
             identifier and AU subtype from the identity block; each identity's bundle
             identifier equals its CLAP id.
  hardcode   outside the identity header, no code in src/ or h2/ spells either CLAP id or either
             store folder as a string, and the selecting macro appears nowhere else. A file that
             names a CLAP id only in a COMMENT must be listed in COMMENT_SITES with a reason, and
             a listed file that no longer names one is a stale allowance. In CMakeLists.txt the
             identity block's two bundle identifiers are the only strings that may hold one.
             tools/ may name the REAL id: those oracles create the plugin by its frozen id on
             purpose, as witnesses that do not read the header. Only the files in
             TEST_WITNESSES may name the TEST identity.
  harness    nothing the harness runs (verify, install, the workflows, tools/*.sh) passes a -D
             for the option: every gate and every release builds the real plugin.

--built <build-dir> real|test (macOS). Reads the three bundles in <build-dir>/HYPERSAW_assets:
their Info.plists, the CLAP descriptor (the bundle is loaded and asked), the VST3 class id (the
same), and `codesign --verify --deep --strict`. `real` must match FROZEN_REAL and the frozen VST3
class id; `test` must match the sources' test identity, must differ from the real one in every
id, and must be sealed (its build signs it; the real bundles are signed by ./install, so their
seal is reported, not required). The tree's CMakeCache must agree with the word given.

MUST-FAIL CONTROLS, on in-memory copies of the real sources, every run: the test identity reusing
the real CLAP id; the same AU subtype; the same store folder; a store folder differing only in
case; the same bundle identifier; the same output name; the option defaulting ON; the real CLAP
id altered; the real AU subtype altered; the real store folder altered; the #ifdef inverted; the
definition set unconditionally; a source hard-coding the store folder; a source hard-coding the
test CLAP id; a second #ifdef on the macro; verify configuring the test identity. Each must read
red WITH ITS OWN TAG, and the untouched copy must read green.
"""
import ctypes
import functools
import pathlib
import plistlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SELF = "tools/test_identity_check.py"
HEADER = "src/plugin_identity.h"
CMAKE = "CMakeLists.txt"
SHELL = "src/hypersaw_clap.cpp"
STORE = "src/gui/preset_store.h"
MACRO = "HORDE_TEST_IDENTITY"

# The real plugin's identity. FROZEN: a host stores clapId (and the VST3 class id derived from
# it) and the AU triple in every saved session; the user's presets live in storeFolder. The
# display name and output name are not what a host keys on, but they are what the human sees and
# what ./install copies, so a change is deliberate and lands here in the same PR.
FROZEN_REAL = {
    "clapId": "com.lifted-truck.hypersaw",
    "displayName": "horde",
    "storeFolder": "HYPERSAW",
    "bundleIdentifier": "com.lifted-truck.hypersaw",
    "outputName": "horde",
    "auSubtype": "Hsaw",
}
# Both identities carry these two: the manufacturer is the vendor, the type says "instrument".
FROZEN_AU_MANUFACTURER = "LfTk"
FROZEN_AU_TYPE = "aumu"
# clap-wrapper derives the VST3 class id from the CLAP id (libs/clap-wrapper/src/
# wrapasvst3_entry.cpp: create_sha1_guid_from_name over the descriptor id when the plugin gives
# no explicit componentId, which this one does not). Read from the built real bundle on
# 2026-10-10, before and after the identity header landed. A wrapper bump that changed the
# derivation would orphan every saved VST3 session; --built makes that red.
FROZEN_REAL_VST3_CID = "F730E1CE68C657DB87D2452E272BD28F"
# The three per-platform forms of the store path (B129), with the identity's folder as the leaf.
FROZEN_STORE_PATHS = (
    'fs::path(home) / "Library" / "Application Support" / "LiftedTruck" / folder',
    'fs::path(appdata) / "LiftedTruck" / folder',
    'fs::path(home) / ".local" / "share" / "LiftedTruck" / folder',
)
SHARE_FIELDS = ("clapId", "displayName", "storeFolder", "bundleIdentifier", "outputName", "auSubtype")
CASEFOLDED = {"storeFolder", "bundleIdentifier", "outputName"}

# Files that name a CLAP id in a COMMENT only, and why that is fine.
COMMENT_SITES = {
    SHELL: "the descriptor's comment explains why the id is frozen; the value is read from the header",
}
# The only files outside the identity header and the identity block that may name the TEST identity.
TEST_WITNESSES = {
    SELF: "this check plants the test identity in its controls",
    "tools/identity_store_check.mm": "the run-time store check holds its own copy of both identities, "
                                     "so it does not learn the expected folder from the code under test",
}
CODE_EXT = (".h", ".hpp", ".c", ".cc", ".cpp", ".mm", ".m", ".inc")
HARNESS_FILES = ("verify", "install")
HARNESS_DIRS = (".github/workflows/",)


# ---- reading sources ---------------------------------------------------------------------

_CPP_TOKEN = re.compile(r"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'""", re.S)


@functools.lru_cache(maxsize=None)
def cpp_split(text):
    """(code, strings): `code` is `text` with comments blanked (strings and newlines kept);
    `strings` is the content of every ordinary string literal that sits in code. One regex
    pass, so a quote inside a comment and a `//` inside a string are both read correctly.
    Cached on the text: the controls re-judge the whole tree with one file changed."""
    strings = []

    def repl(m):
        s = m.group(0)
        if s[0] == "/":
            return re.sub(r"[^\n]", " ", s)
        if s[0] == '"':
            strings.append(s[1:-1])
        return s

    return _CPP_TOKEN.sub(repl, text), tuple(strings)


def cmake_code(text):
    """CMake text with `#` comments blanked; a `#` inside "..." survives."""
    out = []
    for line in text.splitlines():
        q, cut = False, len(line)
        for i, ch in enumerate(line):
            if ch == '"' and (i == 0 or line[i - 1] != "\\"):
                q = not q
            elif ch == "#" and not q:
                cut = i
                break
        out.append(line[:cut])
    return "\n".join(out)


def tree_files():
    """Tracked AND untracked files (--others is load-bearing: a brand-new source is exactly the
    case, and plain ls-files does not list it), minus ignored ones."""
    r = subprocess.run(["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard",
                        "--", "src", "h2", "tools", CMAKE, *HARNESS_FILES, *HARNESS_DIRS],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"test_identity_check: FAILED -- git ls-files: {r.stderr.strip()}")
    files = {}
    for rel in sorted(set(r.stdout.split("\n"))):
        p = ROOT / rel
        if not rel or not p.is_file():
            continue
        try:
            files[rel] = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue   # a binary under tools/ carries no source literal this check reads
    return files


# ---- parsing the two owners ----------------------------------------------------------------

def parse_header(text, fails):
    code, _ = cpp_split(text)
    ids = {}
    m = re.search(r"struct\s+Identity\s*\{(.*?)\}\s*;", code, re.S)
    order = re.findall(r"const\s+char\s*\*\s*(\w+)\s*;", m.group(1)) if m else []
    if order != ["clapId", "displayName", "storeFolder"]:
        fails.append(("wiring", f"{HEADER}: struct Identity's fields are {order}, expected clapId, "
                                "displayName, storeFolder in that order (the initialisers are positional)"))
        return ids, code
    for name in ("kReal", "kTest"):
        found = re.findall(r"inline\s+constexpr\s+Identity\s+" + name +
                           r'\s*=\s*\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}\s*;', code)
        if len(found) != 1:
            fails.append(("wiring", f"{HEADER}: expected exactly one `inline constexpr Identity {name} = "
                                    f"{{\"..\", \"..\", \"..\"}};`, found {len(found)}"))
            continue
        ids[name] = dict(zip(order, found[0]))
    if not re.search(r"kIdentity\s*=\s*#\s*ifdef\s+" + MACRO + r"\s+kTest\s*;\s*#\s*else\s+kReal\s*;\s*#\s*endif",
                     code):
        fails.append(("wiring", f"{HEADER}: kIdentity must be kTest under `#ifdef {MACRO}` and kReal otherwise"))
    return ids, code


def parse_cmake(text, fails):
    code = cmake_code(text)
    out = {"real": {}, "test": {}}
    opts = re.findall(r"\boption\s*\(\s*" + MACRO + r'\s+"[^"]*"\s*(\w*)\s*\)', code)
    if len(opts) != 1:
        fails.append(("option", f"{CMAKE}: expected exactly one option({MACRO} ...), found {len(opts)}"))
    elif opts[0] != "OFF":
        fails.append(("option", f"{CMAKE}: option({MACRO}) defaults to {opts[0] or 'nothing stated'!r}; it must "
                                "default OFF, so every plain configure builds the real plugin"))
    if re.search(r"\bset\s*\(\s*" + MACRO + r"\b", code):
        fails.append(("option", f"{CMAKE}: set({MACRO} ...) overrides the option; only the command line may turn it on"))
    blocks = re.findall(r"\bif\s*\(\s*" + MACRO + r"\s*\)(.*?)\belse\s*\(\s*\)(.*?)\bendif\s*\(\s*\)", code, re.S)
    if len(blocks) != 1:
        fails.append(("wiring", f"{CMAKE}: expected exactly one if({MACRO}) / else() / endif() identity block, "
                                f"found {len(blocks)}"))
        return out, code
    setter = re.compile(r'\bset\s*\(\s*HORDE_(\w+)\s+"([^"]*)"\s*\)')
    names = {"OUTPUT_NAME": "outputName", "BUNDLE_IDENTIFIER": "bundleIdentifier",
             "AUV2_SUBTYPE_CODE": "auSubtype", "AUV2_DISPLAY_NAME": "auDisplayName"}
    for which, body in zip(("test", "real"), blocks[0]):
        for var, val in setter.findall(body):
            if var in names:
                out[which][names[var]] = val
        for need in ("outputName", "bundleIdentifier", "auSubtype"):
            if need not in out[which]:
                fails.append(("wiring", f"{CMAKE}: the {which} branch of the identity block sets no {need}"))
    for var in ("OUTPUT_NAME", "BUNDLE_IDENTIFIER", "AUV2_SUBTYPE_CODE"):
        n = len(re.findall(r"\bset\s*\(\s*HORDE_" + var + r"\b", code))
        if n != 2:
            fails.append(("wiring", f"{CMAKE}: HORDE_{var} is set {n} times; the identity block's two "
                                    "branches are the only places it may be set"))
    defs = [m.start() for m in re.finditer(r"\b(?:target_compile_definitions|add_compile_definitions|"
                                           r"add_definitions)\s*\([^)]*" + MACRO, code)]
    test_branch = re.search(r"\bif\s*\(\s*" + MACRO + r"\s*\)(.*?)\belse\s*\(\s*\)", code, re.S)
    inside = [p for p in defs if test_branch.start(1) <= p < test_branch.end(1)]
    if len(defs) != 1 or len(inside) != 1:
        fails.append(("option", f"{CMAKE}: the {MACRO} compile definition must be set exactly once, inside "
                                f"if({MACRO}); found {len(defs)} in the file, {len(inside)} inside that branch"))
    return out, code


# ---- the static rows -----------------------------------------------------------------------

def identities(files, fails):
    """{'real': {...six fields...}, 'test': {...}} from the two owners, or None if unreadable."""
    hdr, _ = parse_header(files.get(HEADER, ""), fails)
    cm, _ = parse_cmake(files.get(CMAKE, ""), fails)
    if set(hdr) != {"kReal", "kTest"}:
        return None
    out = {}
    for which, key in (("real", "kReal"), ("test", "kTest")):
        out[which] = {**hdr[key], **cm[which]}
        if any(f not in out[which] for f in SHARE_FIELDS):
            return None
    return out


def judge(files):
    """Every static row over `files` ({repo-relative path: text}). Returns [(tag, message)]."""
    fails = []
    for need in (HEADER, CMAKE, SHELL, STORE):
        if need not in files:
            fails.append(("wiring", f"{need}: missing"))
    if fails:
        return fails
    ids = identities(files, fails)
    if ids is None:
        return fails or [("wiring", "the two identities could not be read from the header and the identity block")]
    real, test = ids["real"], ids["test"]

    # frozen
    for f, want in FROZEN_REAL.items():
        if real[f] != want:
            fails.append((f"frozen:{f}", f"the REAL identity's {f} is {real[f]!r}; it is frozen at {want!r} (a host or "
                                         f"the user's disk keys on it). A ruled change edits FROZEN_REAL in {SELF} "
                                         "in the same PR"))
    # share
    for f in SHARE_FIELDS:
        a, b = (real[f].casefold(), test[f].casefold()) if f in CASEFOLDED else (real[f], test[f])
        if a == b:
            fails.append((f"share:{f}", f"the two identities have the same {f}: {real[f]!r} and {test[f]!r}"
                                        + (" (compared case-folded)" if real[f] != test[f] else "")
                                        + ". Installed side by side, one would take the other's place"))
    for which, i in ids.items():
        folder = i["storeFolder"]
        if not folder or folder in (".", "..") or re.search(r"[/\\]", folder):
            fails.append(("share:storeFolder", f"the {which} store folder {folder!r} is not a single path "
                                               "component; the two stores must be siblings"))
        if i["bundleIdentifier"] != i["clapId"]:
            fails.append(("wiring", f"the {which} bundle identifier {i['bundleIdentifier']!r} ({CMAKE}) is not its "
                                    f"CLAP id {i['clapId']!r} ({HEADER}); the two files have drifted"))
        if not re.fullmatch(r"[\x21-\x7e]{4}", i["auSubtype"]) or i["auSubtype"].islower():
            fails.append(("wiring", f"the {which} AU subtype {i['auSubtype']!r} must be four printable ASCII "
                                    "characters, not all lower case (Apple reserves those)"))
    if test.get("auDisplayName") != test["displayName"]:
        fails.append(("wiring", f"{CMAKE}: the test AU display name {test.get('auDisplayName')!r} is not the "
                                f"header's {test['displayName']!r}; the three formats would show two names"))

    # wiring: the build's consumer, then the two source consumers
    cm_code = cmake_code(files[CMAKE])
    call = re.search(r"make_clapfirst_plugins\s*\(\s*TARGET_NAME\s+\$\{PROJECT_NAME\}(.*?)\n\)", cm_code, re.S)
    if not call:
        fails.append(("wiring", f"{CMAKE}: make_clapfirst_plugins(TARGET_NAME ${{PROJECT_NAME}} ...) not found"))
    else:
        for key, val in (("OUTPUT_NAME", "${HORDE_OUTPUT_NAME}"), ("BUNDLE_IDENTIFIER", "${HORDE_BUNDLE_IDENTIFIER}"),
                         ("AUV2_SUBTYPE_CODE", "${HORDE_AUV2_SUBTYPE_CODE}"),
                         ("AUV2_MANUFACTURER_CODE", FROZEN_AU_MANUFACTURER), ("AUV2_INSTRUMENT_TYPE", FROZEN_AU_TYPE)):
            got = re.findall(r"(?<![\w$])" + key + r'\s+"([^"]*)"', call.group(1))
            if got != [val]:
                tag = "frozen:auTriple" if val in (FROZEN_AU_MANUFACTURER, FROZEN_AU_TYPE) else "wiring"
                fails.append((tag, f"{CMAKE}: the plugin target's {key} is {got}, expected [{val!r}]"))
    shell_code, _ = cpp_split(files[SHELL])
    if not re.search(r"s_desc\s*=\s*\{\s*CLAP_VERSION_INIT\s*,\s*hypersaw::identity::kIdentity\.clapId\s*,"
                     r"\s*hypersaw::identity::kIdentity\.displayName\s*,", shell_code):
        fails.append(("wiring", f"{SHELL}: the descriptor's id and name must be kIdentity.clapId and "
                                "kIdentity.displayName"))
    store_code, _ = cpp_split(files[STORE])
    if not re.search(r"folder\s*=\s*identity::kIdentity\.storeFolder\s*;", store_code):
        fails.append(("wiring", f"{STORE}: presetRootFor must take its leaf folder from kIdentity.storeFolder"))
    flat = re.sub(r"\s+", " ", store_code)
    for want in FROZEN_STORE_PATHS:
        if flat.count("return " + want + ";") != 1:
            fails.append(("frozen:storePath", f"{STORE}: expected exactly one `return {want};` (the per-user "
                                              "store path is frozen, B129)"))

    # hardcode, harness
    clap_ids = (real["clapId"], test["clapId"])
    folders = (real["storeFolder"], test["storeFolder"])
    # The real names are prefixes of the test ones, so a test name is matched whole.
    test_words = [n for n in (test["clapId"], test["storeFolder"], test["outputName"]) if n]
    test_names = re.compile("|".join(r"(?<!\w)" + re.escape(n) + r"(?!\w)" for n in test_words))
    for rel, text in files.items():
        if rel in (HEADER, SELF):
            continue
        in_src = rel.startswith(("src/", "h2/"))
        in_tools = rel.startswith("tools/")
        if in_src and rel.endswith(CODE_EXT):
            code, strings = cpp_split(text)
            for s in strings:
                if any(c in s for c in clap_ids):
                    fails.append(("hardcode:clapId", f"{rel}: a string literal spells a CLAP id ({s[:60]!r}); "
                                                     f"read hypersaw::identity::kIdentity ({HEADER})"))
                if s in folders:
                    fails.append(("hardcode:storeFolder", f"{rel}: the string literal {s!r} is a store folder name; "
                                                          "the store path is preset_store.h's presetRoot() alone "
                                                          "(B129)"))
            if re.search(r"\b" + MACRO + r"\b", code):
                fails.append(("hardcode:macro", f"{rel}: uses {MACRO}; {HEADER} holds the only #ifdef on identity"))
            named = any(c in text for c in clap_ids)
            if named and rel not in COMMENT_SITES:
                fails.append(("hardcode:clapId", f"{rel}: names a CLAP id; if it is a comment, list the file in "
                                                 f"COMMENT_SITES in {SELF} with a reason"))
            if rel in COMMENT_SITES and not named:
                fails.append(("hardcode:clapId", f"{rel}: listed in COMMENT_SITES but names no CLAP id; a stale "
                                                 "allowance, remove it"))
        if (in_src or in_tools) and rel not in TEST_WITNESSES:
            # Substring test first: the pattern opens with a lookbehind, which the regex engine
            # cannot skip ahead on, and that alone took 6 s over the tree.
            m = test_names.search(text) if any(w in text for w in test_words) else None
            if m:
                fails.append(("hardcode:testIdentity", f"{rel}: names the TEST identity ({m.group(0)!r}); only "
                                                       f"{HEADER}, {CMAKE} and {sorted(TEST_WITNESSES)} may"))
        if rel in HARNESS_FILES or rel.startswith(HARNESS_DIRS) or (in_tools and rel.endswith(".sh")):
            if re.search(r"-D\s*" + MACRO, text):
                fails.append(("harness", f"{rel}: passes -D{MACRO}; verify, install, the workflows and the build "
                                         "scripts build the real plugin only"))
    cm_ids = sorted(s for s in re.findall(r'"([^"]*)"', cm_code) if any(c in s for c in clap_ids))
    if cm_ids != sorted([real["bundleIdentifier"], test["bundleIdentifier"]]):
        fails.append(("hardcode:clapId", f"{CMAKE}: strings holding a CLAP id are {cm_ids}; the identity block's two "
                                         "bundle identifiers are the only ones allowed"))
    return fails


# ---- must-fail controls ----------------------------------------------------------------------

def planted(files, rel, old, new):
    """A copy of `files` with `old` -> `new` in `rel`. `old` must occur exactly once: a control
    whose plant silently missed would read green for the wrong reason and be reported as blind."""
    text = files.get(rel, "")
    if text.count(old) != 1:
        raise LookupError(f"{rel}: the text to plant on ({old!r}) occurs {text.count(old)} times, expected 1")
    return {**files, rel: text.replace(old, new)}


def controls(files):
    """(number of controls that fired, None), or (0, what went wrong). Runs on in-memory
    copies of a tree that judge() has already read green; touches no file."""
    ids = identities(files, [])
    real, test = ids["real"], ids["test"]
    q = '"{}"'.format
    cases = [
        ("the test identity reuses the real CLAP id", "share:clapId",
         lambda f: planted(f, HEADER, "kTest = {" + q(test["clapId"]), "kTest = {" + q(real["clapId"]))),
        ("the same AU subtype", "share:auSubtype",
         lambda f: planted(f, CMAKE, "set(HORDE_AUV2_SUBTYPE_CODE " + q(test["auSubtype"]) + ")",
                           "set(HORDE_AUV2_SUBTYPE_CODE " + q(real["auSubtype"]) + ")")),
        ("the same store folder", "share:storeFolder",
         lambda f: planted(f, HEADER, q(test["storeFolder"]) + "};", q(real["storeFolder"]) + "};")),
        ("a store folder differing only in case", "share:storeFolder",
         lambda f: planted(f, HEADER, q(test["storeFolder"]) + "};", q(real["storeFolder"].lower()) + "};")),
        ("the same bundle identifier", "share:bundleIdentifier",
         lambda f: planted(f, CMAKE, "set(HORDE_BUNDLE_IDENTIFIER " + q(test["bundleIdentifier"]) + ")",
                           "set(HORDE_BUNDLE_IDENTIFIER " + q(real["bundleIdentifier"]) + ")")),
        ("the same output name", "share:outputName",
         lambda f: planted(f, CMAKE, "set(HORDE_OUTPUT_NAME " + q(test["outputName"]) + ")",
                           "set(HORDE_OUTPUT_NAME " + q(real["outputName"]) + ")")),
        ("the option defaulting ON", "option",
         lambda f: {**f, CMAKE: re.sub(r"(\boption\s*\(\s*" + MACRO + r'\s+"[^"]*"\s*)OFF', r"\1ON", f[CMAKE])}),
        ("the real CLAP id altered", "frozen:clapId",
         lambda f: planted(f, HEADER, "kReal = {" + q(real["clapId"]), "kReal = {" + q("com.lifted-truck.horde"))),
        ("the real AU subtype altered", "frozen:auSubtype",
         lambda f: planted(f, CMAKE, "set(HORDE_AUV2_SUBTYPE_CODE " + q(real["auSubtype"]) + ")",
                           'set(HORDE_AUV2_SUBTYPE_CODE "Hord")')),
        ("the real store folder altered", "frozen:storeFolder",
         lambda f: planted(f, HEADER, q(real["storeFolder"]) + "};", q("horde") + "};")),
        ("the #ifdef inverted", "wiring",
         lambda f: planted(f, HEADER, "#ifdef " + MACRO, "#ifndef " + MACRO)),
        ("the definition set unconditionally", "option",
         lambda f: {**f, CMAKE: f[CMAKE] + "\ntarget_compile_definitions(X-impl PUBLIC " + MACRO + "=1)\n"}),
        ("the descriptor hard-coding the id again", "hardcode:clapId",
         lambda f: planted(f, SHELL, "hypersaw::identity::kIdentity.clapId,", q(real["clapId"]) + ",")),
        ("a source hard-coding the store folder", "hardcode:storeFolder",
         lambda f: {**f, "src/gui/planted_store.h": 'auto p = fs::path(home) / "LiftedTruck" / '
                                                    + q(real["storeFolder"]) + ";\n"}),
        ("a source hard-coding the test CLAP id", "hardcode:clapId",
         lambda f: {**f, "src/planted_id.cpp": "auto *p = create(" + q(test["clapId"]) + ");\n"}),
        ("a second #ifdef on the macro", "hardcode:macro",
         lambda f: {**f, "src/planted_ifdef.h": "#ifdef " + MACRO + "\nint x;\n#endif\n"}),
        ("a tool naming the test store folder", "hardcode:testIdentity",
         lambda f: {**f, "tools/planted_probe.cpp": "const char *k = " + q(test["storeFolder"]) + ";\n"}),
        ("verify configuring the test identity", "harness",
         lambda f: {**f, "verify": f.get("verify", "") + "\ncmake -S . -B b -D" + MACRO + "=ON\n"}),
    ]
    for label, tag, plant in cases:
        try:
            got = {t for t, _ in judge(plant(files))}
        except LookupError as e:
            return 0, f"control '{label}' could not be planted: {e}"
        if tag not in got:
            return 0, (f"control '{label}' did not read red as {tag} (it read {sorted(got) or 'GREEN'}): "
                       "the check is blind to it")
    # Must stay green: the folder's name in a comment, and inside a longer literal (the state
    # format's tag is "HYPERSAW" too, and is not a path).
    benign = {**files, "src/planted_benign.h": "// the store folder is " + real["storeFolder"] + "\n"
                                               'const char *t = "{\\"plugin\\":\\"' + real["storeFolder"] + '\\"}";\n'}
    extra = [m for t, m in judge(benign) if "planted_benign" in m]
    if extra:
        return 0, f"positive control: a comment and a state tag naming the folder read red: {extra[0]}"
    return len(cases), None


# ---- --built: the bundles a build tree actually produced (macOS) -----------------------------

class _Ver(ctypes.Structure):
    _fields_ = [("major", ctypes.c_uint32), ("minor", ctypes.c_uint32), ("revision", ctypes.c_uint32)]


class _Desc(ctypes.Structure):   # the head of clap_plugin_descriptor_t; later fields are not read
    _fields_ = [("clap_version", _Ver), ("id", ctypes.c_char_p), ("name", ctypes.c_char_p)]


class _Factory(ctypes.Structure):
    pass


_Factory._fields_ = [
    ("get_plugin_count", ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.POINTER(_Factory))),
    ("get_plugin_descriptor", ctypes.CFUNCTYPE(ctypes.POINTER(_Desc), ctypes.POINTER(_Factory), ctypes.c_uint32))]


class _Entry(ctypes.Structure):
    _fields_ = [("clap_version", _Ver), ("init", ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_char_p)),
                ("deinit", ctypes.CFUNCTYPE(None)),
                ("get_factory", ctypes.CFUNCTYPE(ctypes.POINTER(_Factory), ctypes.c_char_p))]


class _PClassInfo(ctypes.Structure):   # Steinberg::PClassInfo
    _fields_ = [("cid", ctypes.c_ubyte * 16), ("cardinality", ctypes.c_int32),
                ("category", ctypes.c_char * 32), ("name", ctypes.c_char * 64)]


def clap_descriptors(bundle, stem):
    """[(id, name)] as the built CLAP itself reports them through clap_entry."""
    lib = ctypes.CDLL(str(bundle / "Contents" / "MacOS" / stem))
    entry = _Entry.in_dll(lib, "clap_entry")
    entry.init(str(bundle).encode())
    fac = entry.get_factory(b"clap.plugin-factory")
    return [(d.id.decode(), d.name.decode()) for d in
            (fac.contents.get_plugin_descriptor(fac, i).contents for i in range(fac.contents.get_plugin_count(fac)))]


def vst3_classes(bundle, stem):
    """[(class id as 32 hex digits, name)] from the built VST3's IPluginFactory. Slots 4 and 5
    of its vtable are countClasses and getClassInfo (after FUnknown's three and getFactoryInfo)."""
    lib = ctypes.CDLL(str(bundle / "Contents" / "MacOS" / stem))
    lib.GetPluginFactory.restype = ctypes.c_void_p
    fac = lib.GetPluginFactory()
    vtbl = ctypes.cast(ctypes.cast(fac, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    count = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p)(vtbl[4])
    info = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(_PClassInfo))(vtbl[5])
    out = []
    for i in range(count(fac)):
        ci = _PClassInfo()
        info(fac, i, ctypes.byref(ci))
        out.append((bytes(ci.cid).hex().upper(), ci.name.decode()))
    return out


def built(build_dir, which, ids):
    """Rows over the three bundles of a configured-and-built tree. Returns (fails, facts)."""
    fails, facts = [], []
    if sys.platform != "darwin":
        return [("built", "--built reads macOS bundles (Info.plist, Mach-O); run it on macOS")], facts
    want = dict(FROZEN_REAL) if which == "real" else dict(ids["test"])
    cache = build_dir / "CMakeCache.txt"
    m = re.search(r"^" + MACRO + r":BOOL=(\w+)", cache.read_text() if cache.is_file() else "", re.M)
    configured = "test" if m and m.group(1).upper() in ("ON", "1", "TRUE", "YES") else "real"
    if not cache.is_file() or configured != which:
        return [("built", f"{build_dir}: expected a tree configured as the {which} identity; its CMakeCache "
                          f"{'is missing' if not cache.is_file() else 'says ' + configured}")], facts
    assets, stem = build_dir / "HYPERSAW_assets", want["outputName"]

    def row(ok, what):
        (facts if ok else fails).append(what if ok else ("built", what))

    for ext, suffix in (("clap", "clap"), ("vst3", "vst3"), ("component", "auv2")):
        b = assets / f"{stem}.{ext}"
        if not (b / "Contents" / "Info.plist").is_file():
            fails.append(("built", f"{b}: not built"))
            continue
        pl = plistlib.loads((b / "Contents" / "Info.plist").read_bytes())
        got, exp = pl.get("CFBundleIdentifier"), f"{want['bundleIdentifier']}.{suffix}"
        row(got == exp, f"{stem}.{ext}: bundle identifier {got}" + ("" if got == exp else f", expected {exp}"))
        if ext == "component":
            ac = (pl.get("AudioComponents") or [{}])[0]
            triple = "/".join(str(ac.get(k)) for k in ("type", "subtype", "manufacturer"))
            exp = f"{FROZEN_AU_TYPE}/{want['auSubtype']}/{FROZEN_AU_MANUFACTURER}"
            row(triple == exp, f"{stem}.{ext}: AU triple {triple}" + ("" if triple == exp else f", expected {exp}"))
            exp = f"Mindlathe: {want['displayName']}"
            row(ac.get("name") == exp, f"{stem}.{ext}: AU name {ac.get('name')!r}"
                + ("" if ac.get("name") == exp else f", expected {exp!r}"))
        r = subprocess.run(["codesign", "--verify", "--deep", "--strict", str(b)], capture_output=True, text=True)
        sealed = r.returncode == 0
        if which == "test":
            row(sealed, f"{stem}.{ext}: codesign --verify --deep --strict "
                + ("passes" if sealed else f"FAILS ({r.stderr.strip()}); a host's scanner skips it without a log line"))
        else:
            facts.append(f"{stem}.{ext}: seal {'verifies' if sealed else 'does not verify as built (./install signs it)'}")
    try:
        got = clap_descriptors(assets / f"{stem}.clap", stem)
        exp = [(want["clapId"], want["displayName"])]
        row(got == exp, f"{stem}.clap: descriptor {got}" + ("" if got == exp else f", expected {exp}"))
        got = vst3_classes(assets / f"{stem}.vst3", stem)
        cids, names = [c for c, _ in got], [n for _, n in got]
        row(names == [want["displayName"]], f"{stem}.vst3: class name {names}")
        if which == "real":
            row(cids == [FROZEN_REAL_VST3_CID], f"{stem}.vst3: class id {cids}"
                + ("" if cids == [FROZEN_REAL_VST3_CID] else f", frozen at {FROZEN_REAL_VST3_CID}"))
        else:
            row(len(cids) == 1 and FROZEN_REAL_VST3_CID not in cids,
                f"{stem}.vst3: class id {cids}, " + ("not" if FROZEN_REAL_VST3_CID not in cids else "EQUAL TO")
                + f" the real plugin's {FROZEN_REAL_VST3_CID}")
    except (OSError, ValueError, AttributeError) as e:
        fails.append(("built", f"{stem}: could not load a built bundle to read its ids: {e}"))
    return fails, facts


def main(argv):
    files = tree_files()
    fails = judge(files)
    if fails:
        print(f"test_identity_check: FAILED -- {len(fails)} problem(s):", file=sys.stderr)
        for tag, msg in fails:
            print(f"    [{tag}] {msg}", file=sys.stderr)
        return 1
    fired, err = controls(files)
    if err:
        print("test_identity_check: FAILED (the check itself is broken) -- " + err, file=sys.stderr)
        return 1
    ids = identities(files, [])
    tools_real = sum(1 for rel, text in files.items() if rel.startswith("tools/") and rel != SELF
                     and ids["real"]["clapId"] in text)
    if argv[:1] == ["--built"]:
        if len(argv) != 3 or argv[2] not in ("real", "test"):
            print("usage: test_identity_check.py [--built <build-dir> real|test]", file=sys.stderr)
            return 2
        bfails, facts = built(pathlib.Path(argv[1]).resolve(), argv[2], ids)
        for f in facts:
            print("    " + f)
        if bfails:
            print(f"test_identity_check --built: FAILED -- {len(bfails)} problem(s) in the {argv[2]} build:",
                  file=sys.stderr)
            for _, msg in bfails:
                print("    " + msg, file=sys.stderr)
            return 1
        print(f"test_identity_check --built: GREEN (the {argv[2]} build's three bundles carry the {argv[2]} "
              f"identity: {len(facts)} facts above)")
        return 0
    if argv:
        print("usage: test_identity_check.py [--built <build-dir> real|test]", file=sys.stderr)
        return 2
    t = ids["test"]
    print(f"test_identity_check: GREEN (the real identity equals its frozen table; the test identity "
          f"[{t['clapId']}, AU {t['auSubtype']}, {t['outputName']}.*, store {t['storeFolder']}] shares none of "
          f"{len(SHARE_FIELDS)} ids with it; the option defaults OFF; {len(files)} files hold no hard-coded copy "
          f"({tools_real} oracles name the real id on purpose); {fired} must-fail controls fired)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
