#!/usr/bin/env python3
"""choc_patch_check -- the vendored choc carries its patches, the build compiles
the patched copy, and the Windows GUI uses the hooks they add (B446, ADR-194
D-S5, ADR-196).

WIRED: ./verify fast

  python3 tools/choc_patch_check.py

WHY. On Windows the GUI admits only its embedded page, and refuses page script
every permission (clipboard reads included), through two options that
libs/patches/choc-webview2-navigation.patch and
libs/patches/choc-webview2-permissions.patch add to choc. A patch carried beside
a submodule can be lost four quiet ways: an upstream bump stops it applying, the
build stops listing it, a relative include compiles the unpatched header
instead, or the backend stops passing the hook. Each is a row here.

THE LIST. The patches are the root CMakeLists.txt's HS_CHOC_PATCHES, in apply
order, read through tools/gen_sbom.py's carried_patches (the reader the SBOM
uses, so the two cannot disagree).

ROWS.
  LIST     HS_CHOC_PATCHES names exactly the libs/patches/choc-*.patch files,
           and the README names each one.
  PIN      each patch's `Base:` line names the commit libs/choc is pinned to (the
           gitlink in the index, so it holds without a submodule checkout). A
           bump is red until a person re-checks the patch and its removal
           condition and updates that line.
  SHAPE    each patch only ADDS lines, writes only choc/gui/choc_WebView.h, and
           its header states PURPOSE, UPSTREAM STATUS and REMOVAL CONDITION.
  APPLY    libs/patches/apply_patch.cmake (the build's own mechanism, run here,
           never re-implemented) applies the list in order to a copy of
           libs/choc/choc, and the result
             - declares Options::allowNavigation, registers the three navigation
               events only when it is set, filters page messages only when it
               is set, and still serves the page at the URI the Windows backend
               admits;
             - declares Options::allowPermission with choc's seven kinds,
               consults it in PermissionRequested ahead of upstream's clipboard
               grant only when it is set, refuses with STATE_DENY, maps an
               unknown kind to `other`, and keeps upstream's grant intact for
               the unset case.
  ALONE    each patch also applies by itself to the pristine header, so either
           can be dropped when upstream takes its half (each patch's header
           promises this).
  CRLF     a CRLF copy of the header (Git for Windows' core.autocrlf checkout)
           patches, through the same list, to the same bytes as the LF one.
           Before 2026-10-08 it did not: file(READ) drops the CRs, so the
           script's CRLF test never fired (traces/2026-10-08-b446-choc-win-clipboard.md).
  UPSTREAM the unpatched header has neither `allowNavigation` nor
           `allowPermission`. Red the day upstream adds a name: that is a
           removal condition arriving.
  WIRING   CMakeLists.txt runs apply_patch.cmake with the quoted list and puts
           the copy on the impl's PUBLIC include path; no file in src/ or tools/
           includes choc_WebView.h through a path into libs/choc; the shared
           makeWebView assigns opts.allowNavigation and opts.allowPermission
           from its parameters; the Windows backend passes
           detail::embeddedPagePolicy and detail::webPermissionPolicy to
           makeWebView and binds hzPasteState; and kEmbeddedPage
           (embedded_page_policy.h) is where choc serves the page. What the
           rules DO is tools/embedded_page_policy_check's business, by
           behaviour, not here by token.
  Where libs/choc is not checked out (CI's verify-fast job checks out no
  submodules) APPLY, ALONE, CRLF, UPSTREAM and controls C1, C2, C10 and C11 print a
  WARNING instead, and kEmbeddedPage is checked for presence but not against
  choc's URI. Every CI build job configures, and so applies the patches with a
  hard stop.

MUST-FAIL CONTROLS, every run: a choc header with one context line changed makes
apply_patch.cmake exit nonzero (C1); the unpatched header fails APPLY's content
rules (C2); a gitlink other than Base fails PIN (C3); a patch with a removed line
fails SHAPE (C4); a planted relative include fails WIRING (C5); the Windows
backend without its navigation argument (C6) or passing another rule (C7), a
different page URI (C8), and the shared header without the navigation
assignment (C9) fail WIRING; the navigation patch applied alone fails the
permission rules (C10); a permission filter consulted whether or not the option
is set fails them too (C11); the backend without its permission argument (C12)
or without hzPasteState (C13), the shared header without the permission
assignment (C14), an unquoted list (C15) and a list missing a patch file (C16)
fail WIRING or LIST. If any control reads green the check is red.

WHAT THIS DOES NOT SHOW. That WebView2 at run time cancels what the navigation
rule refuses or denies what the permission rule refuses, or that native PASTE
works. No Windows runtime exists on this Mac; CI's build-windows job compiles
the patched backend, and the runtime behaviour is a recorded residual (B447).
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_sbom  # noqa: E402  (carried_patches: the one reader of HS_CHOC_PATCHES)

ROOT = pathlib.Path(__file__).resolve().parent.parent
PATCH_DIR = ROOT / "libs/patches"
NAV_PATCH = "libs/patches/choc-webview2-navigation.patch"
PERM_PATCH = "libs/patches/choc-webview2-permissions.patch"
README = PATCH_DIR / "README.md"
SCRIPT = PATCH_DIR / "apply_patch.cmake"
CHOC = ROOT / "libs/choc"
TARGET = "choc/gui/choc_WebView.h"
WIN = ROOT / "src/gui/hypersaw_gui_win.cpp"
COMMON = ROOT / "src/gui/hypersaw_gui_common.h"
POLICY = ROOT / "src/gui/embedded_page_policy.h"
CMAKELISTS = ROOT / "CMakeLists.txt"
KINDS = ("page", "frame", "newWindow", "message")
PERM_KINDS = ("clipboardRead", "microphone", "camera", "geolocation", "notifications", "otherSensors", "other")


# ---------------------------------------------------------------- pure rules
def listed_patches(cmake_text):
    """HS_CHOC_PATCHES in order, or [] if CMakeLists.txt has no such list."""
    try:
        return gen_sbom.carried_patches(cmake_text).get("choc", [])
    except gen_sbom.SbomError:
        return []


def rule_list(listed, on_disk, readme_text):
    errs = []
    if not listed:
        errs.append("CMakeLists.txt has no set(HS_CHOC_PATCHES ...) list")
    for rel in sorted(set(on_disk) - set(listed)):
        errs.append(f"{rel} is not in HS_CHOC_PATCHES, so the build does not apply it")
    for rel in listed:
        if rel not in on_disk:
            errs.append(f"HS_CHOC_PATCHES lists {rel}, which does not exist")
        if pathlib.PurePosixPath(rel).name not in readme_text:
            errs.append(f"libs/patches/README.md does not name {pathlib.PurePosixPath(rel).name}")
    for need in (NAV_PATCH, PERM_PATCH):
        if need not in listed:
            errs.append(f"HS_CHOC_PATCHES does not list {need}")
    return errs


def rule_pin(patch_text, gitlink):
    m = re.search(r"^Base:\s+choc\s+([0-9a-f]{40})\b", patch_text, re.M)
    if not m:
        return ["the patch header has no `Base:     choc <40-hex commit>` line"]
    if m.group(1) != gitlink:
        return [f"libs/choc is pinned to {gitlink} but the patch was made against {m.group(1)}. "
                "Re-check that it still applies and whether upstream now provides the hook "
                "(its REMOVAL CONDITION), then update the Base line"]
    return []


def rule_shape(patch_text):
    errs = []
    body_at = patch_text.find("\n--- a/")
    if body_at < 0:
        return ["the patch has no `--- a/` diff body"]
    head, body = patch_text[:body_at], patch_text[body_at + 1:]
    for key in ("PURPOSE.", "UPSTREAM STATUS.", "REMOVAL CONDITION."):
        if key not in head:
            errs.append(f"the patch header lacks its {key[:-1]} paragraph")
    targets = re.findall(r"^\+\+\+ b/(\S+)", body, re.M)
    if targets != [TARGET]:
        errs.append(f"the patch must write exactly {TARGET}; it writes {targets}")
    removed = [ln for ln in body.splitlines() if ln.startswith("-") and not ln.startswith("--- ")]
    if removed:
        errs.append(f"the patch removes {len(removed)} upstream line(s) (first: {removed[0][:70]!r}); "
                    "it must only add, so the default stays upstream's")
    return errs


def windows_page_uri(choc_text):
    """The URI choc's setHTML serves the page at on Windows with no custom scheme."""
    home = re.search(r'#if CHOC_WINDOWS\s*\n\s*return "(https://[^"]+/)";', choc_text)
    page = re.search(r'setHTMLURI = defaultURI \+ "([^"]+)";', choc_text)
    return home.group(1) + page.group(1) if home and page else None


def enum_names(text, enum):
    m = re.search(r"enum class " + enum + r"\s*\{(.*?)\};", text, re.S)
    return tuple(re.findall(r"^\s*(\w+),?\s*(?:///.*)?$", m.group(1), re.M)) if m else ()


def rule_navigation(text):
    errs = []
    if not re.search(r"std::function<bool\s*\(NavigationType, const std::string& uri\)> allowNavigation;", text):
        errs.append("no `std::function<bool(NavigationType, const std::string& uri)> allowNavigation;` in Options")
    if enum_names(text, "NavigationType") != KINDS:
        errs.append(f"NavigationType must be {KINDS}, found {enum_names(text, 'NavigationType')}")
    if not re.search(r"if \(ownerPimpl\.options\.allowNavigation\)\s*\n\s*ownerPimpl\.addNavigationFilters \(view\);", text):
        errs.append("the navigation filters are not registered under `if (ownerPimpl.options.allowNavigation)`")
    for ev in ("add_NavigationStarting", "add_FrameNavigationStarting", "add_NewWindowRequested"):
        if len(re.findall(r"view->" + ev + r" \(", text)) != 1:
            errs.append(f"expected exactly one {ev} registration (inside addNavigationFilters)")
    if not re.search(r"if \(ownerPimpl\.options\.allowNavigation\)\s*\{\s*LPWSTR source = \{\};\s*"
                     r"args->get_Source", text):
        errs.append("the page-message filter does not sit under `if (ownerPimpl.options.allowNavigation)`")
    if windows_page_uri(text) is None:
        errs.append("cannot find where choc serves the page on Windows (getURIHome + setHTMLURI)")
    return errs


UPSTREAM_GRANT = (r"if \(permissionKind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ\)\s*\n"
                  r"\s*args->put_State \(COREWEBVIEW2_PERMISSION_STATE_ALLOW\);")


def rule_permission(text):
    errs = []
    if not re.search(r"std::function<bool\s*\(PermissionKind\)> allowPermission;", text):
        errs.append("no `std::function<bool(PermissionKind)> allowPermission;` in Options")
    if enum_names(text, "PermissionKind") != PERM_KINDS:
        errs.append(f"PermissionKind must be {PERM_KINDS}, found {enum_names(text, 'PermissionKind')}")
    # The filter: inside the PermissionRequested handler, after the kind is read,
    # gated on the option, refusing with DENY and returning before upstream's grant.
    m = re.search(r"args->get_PermissionKind \(std::addressof \(permissionKind\)\);\s*"
                  r"if \(ownerPimpl\.options\.allowPermission\)\s*\{(.*?)\}\s*" + UPSTREAM_GRANT, text, re.S)
    if not m:
        errs.append("PermissionRequested does not consult allowPermission under "
                    "`if (ownerPimpl.options.allowPermission)` ahead of upstream's clipboard grant")
    else:
        block = m.group(1)
        if "COREWEBVIEW2_PERMISSION_STATE_DENY" not in block or "return S_OK;" not in block:
            errs.append("the allowPermission branch does not refuse with STATE_DENY and return")
        if "ownerPimpl.options.allowPermission (getPermissionKind (permissionKind))" not in block:
            errs.append("the allowPermission branch does not pass the mapped kind")
    if len(re.findall(r"options\.allowPermission\b", text)) != 2:
        errs.append("allowPermission must be read exactly twice (the gate and the call)")
    if not re.search(r"case COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ:\s*return Options::PermissionKind::clipboardRead;", text):
        errs.append("getPermissionKind does not map CLIPBOARD_READ to clipboardRead")
    if not re.search(r"default:\s*return Options::PermissionKind::other;", text):
        errs.append("getPermissionKind does not map an unknown kind to `other`")
    if not re.search(UPSTREAM_GRANT, text):
        errs.append("upstream's clipboard grant (the unset-option behaviour) is gone")
    return errs


def rule_patched(text):
    return rule_navigation(text) + rule_permission(text)


def call_span(text, start):
    """Text of the call whose `(` is at text[start], to its matching `)`."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return text[start:]


def strip_comments(t):
    """Blank // and /* */ comments, keeping string literals intact (a URI's `//`
    is not a comment) and newlines in place."""
    tok = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|//[^\n]*|/\*.*?\*/', re.S)
    return tok.sub(lambda m: m.group(0) if m.group(0)[0] in "\"'" else re.sub(r"[^\n]", " ", m.group(0)), t)


def rule_wiring(win_text, common_text, policy_text, cmake_text, includers, page_uri):
    errs = []
    if "apply_patch.cmake" not in cmake_text or not re.search(r'"-DPATCH=\$\{HS_CHOC_PATCHES\}"', cmake_text):
        errs.append("CMakeLists.txt does not run libs/patches/apply_patch.cmake with \"-DPATCH=${HS_CHOC_PATCHES}\" "
                    "(quoted: unquoted, the list splits into separate arguments)")
    if not re.search(r"target_include_directories\(\$\{PROJECT_NAME\}-impl PUBLIC \$\{HS_CHOC_DIR\}\)", cmake_text):
        errs.append("CMakeLists.txt does not put HS_CHOC_DIR on the impl's PUBLIC include path")
    for rel, line in includers:
        errs.append(f"{rel}: `{line.strip()}` compiles the UNPATCHED choc; use #include <choc/gui/choc_WebView.h>")
    cc = strip_comments(common_text)
    for opt in ("allowNavigation", "allowPermission"):
        if not re.search(r"opts\." + opt + r"\s*=\s*std::move\(" + opt + r"\);", cc):
            errs.append(f"hypersaw_gui_common.h: makeWebView does not assign opts.{opt} from its parameter")
    wc = strip_comments(win_text)
    at = wc.find("detail::makeWebView(")
    if at < 0:
        errs.append("hypersaw_gui_win.cpp: no detail::makeWebView( call")
    else:
        call = call_span(wc, at + len("detail::makeWebView"))
        # The rules' BEHAVIOUR is tools/embedded_page_policy_check's business; this
        # only pins that the backend hands the view those shared rules.
        if not re.search(r"NavigationType\s+\w+\s*,\s*const std::string &?\s*\w+\)\s*\{\s*"
                         r"return\s+detail::embeddedPagePolicy\(", call):
            errs.append("hypersaw_gui_win.cpp: makeWebView is not passed detail::embeddedPagePolicy")
        if not re.search(r"PermissionKind\s+\w+\)\s*\{\s*return\s+detail::webPermissionPolicy\(", call):
            errs.append("hypersaw_gui_win.cpp: makeWebView is not passed detail::webPermissionPolicy")
        if not re.search(r'\.bind\("hzPasteState",', call):
            errs.append("hypersaw_gui_win.cpp: no hzPasteState binding, so PASTE has no native path")
    m = re.search(r'kEmbeddedPage\s*=\s*"([^"]*)"', strip_comments(policy_text))
    if not m:
        errs.append("embedded_page_policy.h: no kEmbeddedPage")
    elif page_uri is not None and m.group(1) != page_uri:
        errs.append(f"embedded_page_policy.h: kEmbeddedPage is {m.group(1)!r} but choc serves the page at {page_uri!r}")
    return errs


def relative_includers(files):
    """(rel, line) for each include of choc_WebView.h through a path into libs/choc."""
    out = []
    for rel, text in files:
        for line in text.splitlines():
            if re.match(r'\s*#\s*(include|import)\s*"[^"]*libs/choc/choc/gui/choc_WebView\.h"', line):
                out.append((rel, line))
    return out


# ---------------------------------------------------------------- effects
def run_apply(src, out, patches):
    """apply_patch.cmake on SRC -> OUT with PATCHES in order; returns (rc, combined output)."""
    p = subprocess.run(["cmake", f"-DSRC={src}", f"-DOUT={out}",
                        "-DPATCH=" + ";".join(str(ROOT / r) for r in patches), "-P", str(SCRIPT)],
                       capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def gitlink():
    p = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-s", "libs/choc"], capture_output=True, text=True)
    m = re.match(r"160000 ([0-9a-f]{40}) ", p.stdout)
    return m.group(1) if m else None


def tree_files():
    out = []
    for d in ("src", "tools"):
        for p in sorted((ROOT / d).rglob("*")):
            if p.is_file() and p.suffix in {".h", ".hpp", ".cpp", ".cc", ".mm", ".m", ".inc"}:
                out.append((str(p.relative_to(ROOT)), p.read_text(errors="ignore")))
    return out


def main():
    fails, notes = [], []

    def row(tag, errs):
        for e in errs:
            fails.append(f"{tag}: {e}")
        print(f"  {'PASS' if not errs else 'FAIL'}  {tag}")

    def control(tag, red):
        if not red:
            fails.append(f"CONTROL {tag}: read green; the rule cannot see the fault it exists for")
        print(f"  {'PASS' if red else 'FAIL'}  control {tag}")

    readme_text = README.read_text() if README.exists() else ""
    win_text, common_text, cmake_text = WIN.read_text(), COMMON.read_text(), CMAKELISTS.read_text()
    policy_text = POLICY.read_text()
    files = tree_files()
    link = gitlink()
    listed = listed_patches(cmake_text)
    on_disk = sorted(p.relative_to(ROOT).as_posix() for p in PATCH_DIR.glob("choc-*.patch"))
    texts = {rel: (ROOT / rel).read_text() for rel in on_disk}

    print("choc_patch_check:")
    row("LIST", rule_list(listed, on_disk, readme_text))
    for rel in on_disk:
        name = pathlib.PurePosixPath(rel).name
        row(f"PIN {name}", ["libs/choc has no gitlink in the index"] if link is None else rule_pin(texts[rel], link))
        row(f"SHAPE {name}", rule_shape(texts[rel]))

    pristine_path = CHOC / TARGET
    have_choc = pristine_path.exists()
    page_uri = None
    order = [r for r in listed if r in texts]
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        if have_choc:
            if shutil.which("cmake") is None:
                row("APPLY", ["cmake is not on PATH, so the build's patch step cannot be exercised"])
            else:
                rc, log = run_apply(CHOC / "choc", tmp / "ok", order)
                patched = (tmp / "ok" / TARGET).read_text() if rc == 0 else ""
                row("APPLY", [f"apply_patch.cmake exited {rc}:\n{log}"] if rc else rule_patched(patched))
                page_uri = windows_page_uri(patched) if rc == 0 else None
                for i, rel in enumerate(order):
                    rc1, log1 = run_apply(CHOC / "choc", tmp / f"alone{i}", [rel])
                    row(f"ALONE {pathlib.PurePosixPath(rel).name}",
                        [f"does not apply by itself to the pristine header:\n{log1}"] if rc1 else [])
                # CRLF: Git for Windows' autocrlf checkout must patch to the same bytes.
                crlf = tmp / "crlf" / "choc" / "gui"
                crlf.mkdir(parents=True)
                (crlf / "choc_WebView.h").write_bytes(pristine_path.read_bytes().replace(b"\n", b"\r\n"))
                rc4, log4 = run_apply(tmp / "crlf" / "choc", tmp / "crlfout", order)
                same = rc4 == 0 and rc == 0 and \
                    (tmp / "crlfout" / TARGET).read_bytes() == (tmp / "ok" / TARGET).read_bytes()
                row("CRLF", [] if same else [f"a CRLF copy of the header does not patch to the LF result "
                                             f"(apply_patch.cmake exited {rc4}):\n{log4[-600:]}"])
                pristine = pristine_path.read_text()
                row("UPSTREAM", [f"the unpatched choc already has `{n}`: upstream may now provide the "
                                 "hook; see the matching patch's REMOVAL CONDITION"
                                 for n in ("allowNavigation", "allowPermission") if n in pristine])
                # C1: a drifted choc stops the build's patch step.
                drift = tmp / "drift" / "choc" / "gui"
                drift.mkdir(parents=True)
                (drift / "choc_WebView.h").write_text(
                    pristine.replace("view->add_PermissionRequested (this, std::addressof (token));",
                                     "view->add_PermissionRequested (this, &token);", 1))
                rc2, _ = run_apply(tmp / "drift" / "choc", tmp / "driftout", order)
                control("C1 drifted choc stops apply_patch.cmake", rc2 != 0)
                # C2: the unpatched header fails the content rules.
                control("C2 unpatched choc fails APPLY's rules", bool(rule_patched(pristine)))
                # C10: the navigation patch alone leaves the permission rules red.
                rc3, _ = run_apply(CHOC / "choc", tmp / "navonly", [NAV_PATCH])
                nav_only = (tmp / "navonly" / TARGET).read_text() if rc3 == 0 else ""
                control("C10 navigation patch alone fails the permission rules",
                        rc3 == 0 and not rule_navigation(nav_only) and bool(rule_permission(nav_only)))
                # C11: a filter consulted whether or not the option is set.
                ungated = patched.replace("if (ownerPimpl.options.allowPermission)\n", "if (true)\n", 1)
                control("C11 an ungated permission filter fails the permission rules",
                        ungated != patched and bool(rule_permission(ungated)))
        else:
            notes.append("WARNING: libs/choc is not checked out; APPLY, ALONE, CRLF, UPSTREAM and controls C1, C2, "
                         "C10 and C11 not run here (every build job applies the patches at configure, "
                         "with a hard stop)")

    row("WIRING", rule_wiring(win_text, common_text, policy_text, cmake_text, relative_includers(files), page_uri))

    # Controls on in-memory text.
    nav_text = texts.get(NAV_PATCH, "")
    other = "0" * 40 if link != "0" * 40 else "1" * 40
    control("C3 another gitlink fails PIN", bool(rule_pin(nav_text, other)))
    # C4: the first context line of the first hunk, turned into a removal.
    planted_removal = re.sub(r"(\n@@[^\n]*\n) ", r"\1-", nav_text, count=1)
    control("C4 a removed upstream line fails SHAPE",
            planted_removal != nav_text and bool(rule_shape(planted_removal)))

    def wiring(win=win_text, common=common_text, policy=policy_text, cmake=cmake_text, inc=()):
        return bool(rule_wiring(win, common, policy, cmake, relative_includers(files) + list(inc),
                                page_uri or "https://choc.localhost/getHTMLInternal"))

    plant = relative_includers([("src/gui/planted.h", '#include "../../libs/choc/choc/gui/choc_WebView.h"\n')])
    control("C5 a relative include of the unpatched header fails WIRING", wiring(inc=plant))
    no_arg = re.sub(r",\s*\[this\]\(choc::ui::WebView::Options::NavigationType.*?\},", ",", win_text,
                    count=1, flags=re.S)
    control("C6 the backend without its navigation argument fails WIRING", no_arg != win_text and wiring(win=no_arg))
    other_rule = win_text.replace("detail::embeddedPagePolicy(", "detail::anyOtherPolicy(", 1)
    control("C7 the backend passing another rule fails WIRING", other_rule != win_text and wiring(win=other_rule))
    other_uri = re.sub(r'(kEmbeddedPage\s*=\s*")[^"]*"', r'\1https://example.invalid/"', policy_text, count=1)
    control("C8 a different page URI fails WIRING", other_uri != policy_text and wiring(policy=other_uri))
    no_assign = common_text.replace("opts.allowNavigation = std::move(allowNavigation);", "", 1)
    control("C9 makeWebView without the navigation assignment fails WIRING",
            no_assign != common_text and wiring(common=no_assign))
    no_perm = re.sub(r",\s*\[\]\(choc::ui::WebView::Options::PermissionKind.*?\}\);", ");", win_text,
                     count=1, flags=re.S)
    control("C12 the backend without its permission argument fails WIRING", no_perm != win_text and wiring(win=no_perm))
    no_paste = win_text.replace('w.bind("hzPasteState",', 'w.bind("hzSomethingElse",', 1)
    control("C13 the backend without hzPasteState fails WIRING", no_paste != win_text and wiring(win=no_paste))
    no_perm_assign = common_text.replace("opts.allowPermission = std::move(allowPermission);", "", 1)
    control("C14 makeWebView without the permission assignment fails WIRING",
            no_perm_assign != common_text and wiring(common=no_perm_assign))
    unquoted = cmake_text.replace('"-DPATCH=${HS_CHOC_PATCHES}"', "-DPATCH=${HS_CHOC_PATCHES}", 1)
    control("C15 an unquoted patch list fails WIRING", unquoted != cmake_text and wiring(cmake=unquoted))
    short = [r for r in listed if r != PERM_PATCH]
    control("C16 a list missing a patch file fails LIST",
            short != listed and bool(rule_list(short, on_disk, readme_text)))

    for n in notes:
        print(f"  {n}")
    if fails:
        print("choc_patch_check: FAIL", file=sys.stderr)
        for f in fails:
            print(f"    {f}", file=sys.stderr)
        return 1
    print(f"choc_patch_check: OK ({len(order)} patches; list, pin, shape, wiring"
          + (", apply, alone, crlf, upstream" if have_choc else "")
          + f"; {12 + (4 if have_choc else 0)} controls red as designed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
