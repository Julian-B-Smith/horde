#!/usr/bin/env python3
"""choc_patch_check -- the vendored choc carries its one patch, the build compiles
the patched copy, and the Windows GUI uses the hook it adds (B446, ADR-194 D-S5).

WIRED: ./verify fast

  python3 tools/choc_patch_check.py

WHY. On Windows the GUI can admit only its embedded page through an option that
libs/patches/choc-webview2-navigation.patch adds to choc. A patch carried beside
a submodule can be lost three quiet ways: an upstream bump stops it applying, a
relative include compiles the unpatched header instead, or the backend stops
passing the hook. Each is a row here.

ROWS.
  PIN      the patch's `Base:` line names the commit libs/choc is pinned to (the
           gitlink in the index, so it holds without a submodule checkout). A
           bump is red until a person re-checks the patch and its removal
           condition and updates that line.
  SHAPE    the patch only ADDS lines, writes only choc/gui/choc_WebView.h, and
           its header states PURPOSE, UPSTREAM STATUS and REMOVAL CONDITION; the
           README names it.
  APPLY    libs/patches/apply_patch.cmake (the build's own mechanism, run here,
           never re-implemented) applies the patch to a copy of libs/choc/choc,
           and the result declares Options::allowNavigation, registers the three
           navigation events only when the option is set, filters page messages
           only when it is set, and still serves the page at the URI the Windows
           backend admits.
  UPSTREAM the unpatched header has no `allowNavigation`. Red the day upstream
           adds the name: that is the removal condition arriving.
  WIRING   CMakeLists.txt runs apply_patch.cmake with this patch and puts the
           copy on the impl's PUBLIC include path; no file in src/ or tools/
           includes choc_WebView.h through a path into libs/choc; the shared
           makeWebView assigns opts.allowNavigation from its parameter; the
           Windows backend passes its policy to makeWebView, refuses every frame
           and new-window request, and admits the page and its messages only
           against kEmbeddedPage.
  Where libs/choc is not checked out (CI's verify-fast job checks out no
  submodules) APPLY, UPSTREAM and controls C1-C2 print a WARNING instead, and
  kEmbeddedPage is checked for presence but not against choc's URI. Every CI
  build job configures, and so applies the patch with a hard stop.

MUST-FAIL CONTROLS, every run: a choc header with one context line changed makes
apply_patch.cmake exit nonzero; the unpatched header fails APPLY's content rules;
a gitlink other than Base fails PIN; a patch with a removed line fails SHAPE; a
planted relative include fails WIRING; the Windows backend without the policy
argument, with a frame request allowed, with the page check removed, or with a
different page URI fails WIRING; the shared header without the assignment fails
WIRING. If any control reads green the check is red.

WHAT THIS DOES NOT SHOW. That WebView2 at run time cancels what the policy
refuses. No Windows runtime exists on this Mac; CI's build-windows job compiles
the patched backend, and the runtime behaviour is a recorded residual.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PATCH = ROOT / "libs/patches/choc-webview2-navigation.patch"
README = ROOT / "libs/patches/README.md"
SCRIPT = ROOT / "libs/patches/apply_patch.cmake"
CHOC = ROOT / "libs/choc"
TARGET = "choc/gui/choc_WebView.h"
WIN = ROOT / "src/gui/hypersaw_gui_win.cpp"
COMMON = ROOT / "src/gui/hypersaw_gui_common.h"
CMAKELISTS = ROOT / "CMakeLists.txt"
KINDS = ("page", "frame", "newWindow", "message")


# ---------------------------------------------------------------- pure rules
def rule_pin(patch_text, gitlink):
    m = re.search(r"^Base:\s+choc\s+([0-9a-f]{40})\b", patch_text, re.M)
    if not m:
        return ["the patch header has no `Base:     choc <40-hex commit>` line"]
    if m.group(1) != gitlink:
        return [f"libs/choc is pinned to {gitlink} but the patch was made against {m.group(1)}. "
                "Re-check that it still applies and whether upstream now provides the hook "
                "(its REMOVAL CONDITION), then update the Base line"]
    return []


def rule_shape(patch_text, readme_text):
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
    if PATCH.name not in readme_text:
        errs.append(f"libs/patches/README.md does not name {PATCH.name}")
    return errs


def windows_page_uri(choc_text):
    """The URI choc's setHTML serves the page at on Windows with no custom scheme."""
    home = re.search(r'#if CHOC_WINDOWS\s*\n\s*return "(https://[^"]+/)";', choc_text)
    page = re.search(r'setHTMLURI = defaultURI \+ "([^"]+)";', choc_text)
    return home.group(1) + page.group(1) if home and page else None


def rule_patched(text):
    errs = []
    if not re.search(r"std::function<bool\s*\(NavigationType, const std::string& uri\)> allowNavigation;", text):
        errs.append("no `std::function<bool(NavigationType, const std::string& uri)> allowNavigation;` in Options")
    enum = re.search(r"enum class NavigationType\s*\{(.*?)\};", text, re.S)
    names = re.findall(r"^\s*(\w+),?\s*(?:///.*)?$", enum.group(1), re.M) if enum else []
    if tuple(names) != KINDS:
        errs.append(f"NavigationType must be {KINDS}, found {tuple(names)}")
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


def case_segment(code, name):
    m = re.search(r"case\s+(?:\w+::)*" + name + r"\s*:", code)
    if not m:
        return None
    nxt = re.search(r"\bcase\s+(?:\w+::)*\w+\s*:(?!:)|\bdefault\s*:|\n\s*\}", code[m.end():])
    # a run of grouped labels (`case a: case b: return false;`) shares the next body
    seg = code[m.end():]
    while nxt and not seg[:nxt.start()].strip():
        seg = seg[nxt.end():]
        nxt = re.search(r"\bcase\s+(?:\w+::)*\w+\s*:(?!:)|\bdefault\s*:|\n\s*\}", seg)
    return seg[:nxt.start()] if nxt else seg


def strip_comments(t):
    """Blank // and /* */ comments, keeping string literals intact (a URI's `//`
    is not a comment) and newlines in place."""
    tok = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|//[^\n]*|/\*.*?\*/', re.S)
    return tok.sub(lambda m: m.group(0) if m.group(0)[0] in "\"'" else re.sub(r"[^\n]", " ", m.group(0)), t)


def rule_wiring(win_text, common_text, cmake_text, includers, page_uri):
    errs = []
    if "apply_patch.cmake" not in cmake_text or "libs/patches/choc-webview2-navigation.patch" not in cmake_text:
        errs.append("CMakeLists.txt does not run libs/patches/apply_patch.cmake with the choc patch")
    if not re.search(r"target_include_directories\(\$\{PROJECT_NAME\}-impl PUBLIC \$\{HS_CHOC_DIR\}\)", cmake_text):
        errs.append("CMakeLists.txt does not put HS_CHOC_DIR on the impl's PUBLIC include path")
    for rel, line in includers:
        errs.append(f"{rel}: `{line.strip()}` compiles the UNPATCHED choc; use #include <choc/gui/choc_WebView.h>")
    cc = strip_comments(common_text)
    if not re.search(r"opts\.allowNavigation\s*=\s*std::move\(allowNavigation\);", cc):
        errs.append("hypersaw_gui_common.h: makeWebView does not assign opts.allowNavigation from its parameter")
    wc = strip_comments(win_text)
    at = wc.find("detail::makeWebView(")
    if at < 0:
        errs.append("hypersaw_gui_win.cpp: no detail::makeWebView( call")
    else:
        call = call_span(wc, at + len("detail::makeWebView"))
        if not re.search(r"NavigationType\s+\w+\s*,\s*const std::string &?\s*\w+\)\s*\{\s*return\s+allowNavigation\(", call):
            errs.append("hypersaw_gui_win.cpp: makeWebView is not passed the allowNavigation policy")
    m = re.search(r'kEmbeddedPage\s*=\s*"([^"]*)"', wc)
    if not m:
        errs.append("hypersaw_gui_win.cpp: no kEmbeddedPage")
    elif page_uri is not None and m.group(1) != page_uri:
        errs.append(f"hypersaw_gui_win.cpp: kEmbeddedPage is {m.group(1)!r} but choc serves the page at {page_uri!r}")
    for k in ("frame", "newWindow"):
        seg = case_segment(wc, k)
        if seg is None:
            errs.append(f"hypersaw_gui_win.cpp: the policy has no `case ...{k}:`")
        elif not re.match(r"\s*return\s+false\s*;", seg):
            errs.append(f"hypersaw_gui_win.cpp: the {k} case does not simply `return false;`")
    for k in ("page", "message"):
        seg = case_segment(wc, k)
        if seg is None:
            errs.append(f"hypersaw_gui_win.cpp: the policy has no `case ...{k}:`")
        elif "kEmbeddedPage" not in seg or "pageAdmitted" not in seg:
            errs.append(f"hypersaw_gui_win.cpp: the {k} case does not check pageAdmitted and kEmbeddedPage")
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
def run_apply(src, out):
    """apply_patch.cmake on SRC -> OUT; returns (rc, combined output)."""
    p = subprocess.run(["cmake", f"-DSRC={src}", f"-DOUT={out}", f"-DPATCH={PATCH}", "-P", str(SCRIPT)],
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

    patch_text = PATCH.read_text() if PATCH.exists() else ""
    readme_text = README.read_text() if README.exists() else ""
    win_text, common_text, cmake_text = WIN.read_text(), COMMON.read_text(), CMAKELISTS.read_text()
    files = tree_files()
    link = gitlink()

    print("choc_patch_check:")
    row("PIN", ["libs/choc has no gitlink in the index"] if link is None else rule_pin(patch_text, link))
    row("SHAPE", rule_shape(patch_text, readme_text))

    pristine_path = CHOC / TARGET
    have_choc = pristine_path.exists()
    page_uri = None
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        if have_choc:
            if shutil.which("cmake") is None:
                row("APPLY", ["cmake is not on PATH, so the build's patch step cannot be exercised"])
            else:
                rc, log = run_apply(CHOC / "choc", tmp / "ok")
                patched = (tmp / "ok" / TARGET).read_text() if rc == 0 else ""
                row("APPLY", [f"apply_patch.cmake exited {rc}:\n{log}"] if rc else rule_patched(patched))
                page_uri = windows_page_uri(patched) if rc == 0 else None
                pristine = pristine_path.read_text()
                row("UPSTREAM", ["the unpatched choc already has `allowNavigation`: upstream may now "
                                 "provide the hook; see the patch's REMOVAL CONDITION"]
                    if "allowNavigation" in pristine else [])
                # C1: a drifted choc stops the build's patch step.
                drift = tmp / "drift" / "choc" / "gui"
                drift.mkdir(parents=True)
                (drift / "choc_WebView.h").write_text(
                    pristine.replace("view->add_PermissionRequested (this, std::addressof (token));",
                                     "view->add_PermissionRequested (this, &token);", 1))
                rc2, _ = run_apply(tmp / "drift" / "choc", tmp / "driftout")
                control("C1 drifted choc stops apply_patch.cmake", rc2 != 0)
                # C2: the unpatched header fails the content rules.
                control("C2 unpatched choc fails APPLY's rules", bool(rule_patched(pristine)))
        else:
            notes.append("WARNING: libs/choc is not checked out; APPLY, UPSTREAM and controls C1-C2 "
                         "not run here (every build job applies the patch at configure, with a hard stop)")

    row("WIRING", rule_wiring(win_text, common_text, cmake_text, relative_includers(files), page_uri))

    # Controls on in-memory text.
    other = "0" * 40 if link != "0" * 40 else "1" * 40
    control("C3 another gitlink fails PIN", bool(rule_pin(patch_text, other)))
    # C4: the first context line of the first hunk, turned into a removal.
    planted_removal = re.sub(r"(\n@@[^\n]*\n) ", r"\1-", patch_text, count=1)
    control("C4 a removed upstream line fails SHAPE",
            planted_removal != patch_text and bool(rule_shape(planted_removal, readme_text)))

    def wiring(win=win_text, common=common_text, inc=()):
        return bool(rule_wiring(win, common, cmake_text, relative_includers(files) + list(inc),
                                page_uri or "https://choc.localhost/getHTMLInternal"))

    plant = relative_includers([("src/gui/planted.h", '#include "../../libs/choc/choc/gui/choc_WebView.h"\n')])
    control("C5 a relative include of the unpatched header fails WIRING", wiring(inc=plant))
    no_arg = re.sub(r",\s*\[this\]\(choc::ui::WebView::Options::NavigationType.*?\}\);", ");", win_text,
                    count=1, flags=re.S)
    control("C6 the backend without its policy argument fails WIRING", no_arg != win_text and wiring(win=no_arg))
    frame_open = re.sub(r"(case T::frame:\s*case T::newWindow:\s*)return false;", r"\1return true;", win_text, count=1)
    control("C7 a frame or new window allowed fails WIRING", frame_open != win_text and wiring(win=frame_open))
    no_check = win_text.replace("if (pageAdmitted || uri != kEmbeddedPage) return false;", "", 1)
    control("C8 the page admitted without its check fails WIRING", no_check != win_text and wiring(win=no_check))
    other_uri = re.sub(r'(kEmbeddedPage\s*=\s*")[^"]*"', r'\1https://example.invalid/"', win_text, count=1)
    control("C9 a different page URI fails WIRING", other_uri != win_text and wiring(win=other_uri))
    no_assign = common_text.replace("opts.allowNavigation = std::move(allowNavigation);", "", 1)
    control("C10 makeWebView without the assignment fails WIRING", no_assign != common_text and wiring(common=no_assign))

    for n in notes:
        print(f"  {n}")
    if fails:
        print("choc_patch_check: FAIL", file=sys.stderr)
        for f in fails:
            print(f"    {f}", file=sys.stderr)
        return 1
    print("choc_patch_check: OK (pin, shape, wiring" + (", apply, upstream" if have_choc else "")
          + f"; {8 + (2 if have_choc else 0)} controls red as designed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
