#!/usr/bin/env python3
"""choc_patch_check -- the vendored choc carries its patches, the build compiles
the patched copy, and the Windows GUI uses the hooks they add (B446, ADR-194
D-S5, ADR-196; the external-drop patch: B448 C3, ADR-205 item 3).

WIRED: ./verify fast

  python3 tools/choc_patch_check.py

WHY. On Windows the GUI admits only its embedded page, refuses page script
every permission (clipboard reads included), and turns off drops from outside
the view, through three options that libs/patches/choc-webview2-navigation.patch,
libs/patches/choc-webview2-permissions.patch and
libs/patches/choc-webview2-external-drop.patch add to choc. A patch carried beside
a submodule can be lost four quiet ways: an upstream bump stops it applying, the
build stops listing it, a relative include compiles the unpatched header
instead, or the backend stops passing the hook. Each is a row here.

THE LIST. The patches are the root CMakeLists.txt's HS_CHOC_PATCHES, in apply
order, read through tools/gen_sbom.py's carried_patches (the reader the SBOM
uses, so the two cannot disagree).

ROWS.
  LIST     HS_CHOC_PATCHES names exactly the libs/patches/choc-*.patch files,
           each once, and the README names each one.
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
               grant only when it is set AND the owner is not deleted (the
               deletion check first, so a deleted owner is never read; critic
               MEDIUM-1), refuses with STATE_DENY, maps an
               unknown kind to `other`, and keeps upstream's grant intact for
               the unset case;
             - passes the DROP rules below, and declares the controller chain
               from IUnknown to ICoreWebView2Controller4 with the SDK's bases,
               interface ids and method order (the two interfaces upstream
               already has included: their methods fill the vtable slots ahead
               of the patch's), with the new block inside
               webviewControllerCreationComplete, after the controller is
               held and before the page is told it is ready.
  DROP     read from the external-drop patch's ADDED lines, so it holds with no
           choc checkout: Options::allowExternalDrop is a bool that defaults
           to TRUE (choc's behaviour for every other user of the patch is
           unchanged); ICoreWebView2Controller3 and ICoreWebView2Controller4
           carry the interface ids, bases and method order of Microsoft's
           WebView2 SDK header (SDK_CHAIN below records them and their
           source; a wrong order would call the wrong vtable slot); getIID()
           returns the same id as the MIDL_INTERFACE line; and
           put_AllowExternalDrop is called once, with FALSE, only when the
           option is false and only after QueryInterface for Controller4
           returned S_OK with a non-null pointer, so an older runtime is left
           as it was and the default path calls nothing.
  ALONE    each patch also applies by itself to the pristine header, so any one
           can be dropped when upstream takes its part (each patch's header
           promises this).
  ORDER    the list applied in EVERY other order (reverse included) gives the
           same bytes as in list order, so no patch depends on another having
           run first.
  CRLF     a CRLF copy of the header (Git for Windows' core.autocrlf checkout)
           patches, through the same list, to the same bytes as the LF one.
           Before 2026-10-08 it did not: file(READ) drops the CRs, so the
           script's CRLF test never fired (traces/2026-10-08-b446-choc-win-clipboard.md).
  UPSTREAM the unpatched header has none of `allowNavigation`,
           `allowPermission`, `allowExternalDrop` and
           `ICoreWebView2Controller4`. Red the day upstream adds a name: that
           is a removal condition arriving.
  WIRING   CMakeLists.txt runs apply_patch.cmake with the quoted list and puts
           the copy on the impl's PUBLIC include path; no file in src/ or tools/
           includes choc_WebView.h through a path into libs/choc; the shared
           makeWebView assigns opts.allowNavigation, opts.allowPermission and
           opts.allowExternalDrop from its parameters, and takes the last as
           a bool defaulting to true, just ahead of the origin; the Windows
           backend passes `false` there; the Windows backend passes
           detail::embeddedPagePolicy and detail::webPermissionPolicy to
           makeWebView and binds hzPasteState; the Windows backend passes
           detail::kEmbeddedOrigin as customSchemeURI and makeWebView assigns
           it; kEmbeddedOrigin is not choc's shared default origin (critic
           HIGH-1); kEmbeddedPage is where choc serves the page from that
           origin (choc's getURIHome custom branch plus its setHTMLURI path,
           read from the patched header); and hypersaw_gui.mm uses neither
           constant nor customSchemeURI, so macOS does not depend on them. What the
           rules DO is tools/embedded_page_policy_check's business, by
           behaviour, not here by token.
  Where libs/choc is not checked out (CI's verify-fast job checks out no
  submodules) APPLY, ORDER, ALONE, CRLF, UPSTREAM and controls C1, C2, C10, C11,
  C17 and C35 to C38 print a WARNING instead, and the origin rules are checked against
  FALLBACK_SERVING (choc's default home and page path at the pin, recorded here)
  rather than read from the header. Every CI build job configures, and so
  applies the patches with a hard stop.

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
fail WIRING or LIST; a gate that reads the owner before checking deletion
fails the permission rules (C17); kEmbeddedOrigin set to choc's shared default
(C18), the backend not passing the origin (C19), makeWebView without the origin
assignment (C20), and the macOS backend using the origin (C21) fail WIRING; a
reverse-order result that differs fails ORDER (C22); a list naming a patch twice
fails LIST (C23). For the external-drop patch: the backend without its
argument (C24) or passing true (C25) and the shared header without the
assignment (C26) fail WIRING; a list missing the patch fails LIST (C27); the
patch with a removed line fails SHAPE (C28); the option defaulting to false
(C29), a call made whatever the option says (C30), two Controller3 methods
swapped (C31), an interface id one digit off (C32), a getIID() that disagrees
with its MIDL_INTERFACE line (C33) and a call made when the query did not
succeed (C34) fail DROP; an upstream controller interface with a method missing fails
the chain rule (C35); a choc whose controller set-up drifted stops the patch
applied alone (C36); and a planted patch that leans on the navigation patch's
lines fails ALONE (C37) and ORDER (C38). If any control reads green the check
is red.

WHAT THIS DOES NOT SHOW. That WebView2 at run time cancels what the navigation
rule refuses or denies what the permission rule refuses, that native PASTE
works, or that a real drop from another application is refused once
AllowExternalDrop is off. The interface ids and method order are compared with a
RECORD of the SDK header (SDK_CHAIN), not with the header itself, which this
repository does not carry. No Windows runtime exists on this Mac; CI's build-windows job compiles
the patched backend, and the runtime behaviour is a recorded residual (B447).
"""
import itertools
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
DROP_PATCH = "libs/patches/choc-webview2-external-drop.patch"
README = PATCH_DIR / "README.md"
SCRIPT = PATCH_DIR / "apply_patch.cmake"
CHOC = ROOT / "libs/choc"
TARGET = "choc/gui/choc_WebView.h"
WIN = ROOT / "src/gui/hypersaw_gui_win.cpp"
COMMON = ROOT / "src/gui/hypersaw_gui_common.h"
POLICY = ROOT / "src/gui/embedded_page_policy.h"
MM = ROOT / "src/gui/hypersaw_gui.mm"
CMAKELISTS = ROOT / "CMakeLists.txt"
KINDS = ("page", "frame", "newWindow", "message")
PERM_KINDS = ("clipboardRead", "microphone", "camera", "geolocation", "notifications", "otherSensors", "other")
# The WebView2 controller chain as Microsoft's SDK header declares it: (interface,
# id, base, methods in vtable order). Source: build/native/include/WebView2.h in
# the NuGet package Microsoft.Web.WebView2, version 1.0.2903.40, read 2026-10-10.
# The first two are upstream choc's own declarations; the external-drop patch adds
# the last two. A COM call goes by slot number, not by name, so put_AllowExternalDrop
# reaches the right function only if every method ahead of it is declared, in this
# order. That is why the whole chain is held here and not only the two new methods.
SDK_CHAIN = (
    ("ICoreWebView2Controller", "4d00c0d1-9434-4eb6-8078-8697a560334f", "IUnknown",
     ("get_IsVisible", "put_IsVisible", "get_Bounds", "put_Bounds", "get_ZoomFactor", "put_ZoomFactor",
      "add_ZoomFactorChanged", "remove_ZoomFactorChanged", "SetBoundsAndZoomFactor", "MoveFocus",
      "add_MoveFocusRequested", "remove_MoveFocusRequested", "add_GotFocus", "remove_GotFocus",
      "add_LostFocus", "remove_LostFocus", "add_AcceleratorKeyPressed", "remove_AcceleratorKeyPressed",
      "get_ParentWindow", "put_ParentWindow", "NotifyParentWindowPositionChanged", "Close", "get_CoreWebView2")),
    ("ICoreWebView2Controller2", "c979903e-d4ca-4228-92eb-47ee3fa96eab", "ICoreWebView2Controller",
     ("get_DefaultBackgroundColor", "put_DefaultBackgroundColor")),
    ("ICoreWebView2Controller3", "f9614724-5d2b-41dc-aef7-73d62b51543b", "ICoreWebView2Controller2",
     ("get_RasterizationScale", "put_RasterizationScale", "get_ShouldDetectMonitorScaleChanges",
      "put_ShouldDetectMonitorScaleChanges", "add_RasterizationScaleChanged", "remove_RasterizationScaleChanged",
      "get_BoundsMode", "put_BoundsMode")),
    ("ICoreWebView2Controller4", "97d418d5-a426-4e49-a151-e1a10f327d9e", "ICoreWebView2Controller3",
     ("get_AllowExternalDrop", "put_AllowExternalDrop")),
)
UPSTREAM_CHAIN, PATCH_CHAIN = SDK_CHAIN[:2], SDK_CHAIN[2:]


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
    for rel in sorted({r for r in listed if listed.count(r) > 1}):
        errs.append(f"HS_CHOC_PATCHES names {rel} {listed.count(rel)} times; a second apply would stop the configure")
    for rel in sorted(set(on_disk) - set(listed)):
        errs.append(f"{rel} is not in HS_CHOC_PATCHES, so the build does not apply it")
    for rel in listed:
        if rel not in on_disk:
            errs.append(f"HS_CHOC_PATCHES lists {rel}, which does not exist")
        if pathlib.PurePosixPath(rel).name not in readme_text:
            errs.append(f"libs/patches/README.md does not name {pathlib.PurePosixPath(rel).name}")
    for need in (NAV_PATCH, PERM_PATCH, DROP_PATCH):
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


CUSTOM_HOME = (r'if \(! options\.customSchemeURI\.empty\(\)\)\s*\{\s*'
               r'if \(choc::text::endsWith \(options\.customSchemeURI, "/"\)\)\s*return options\.customSchemeURI;\s*'
               r'return options\.customSchemeURI \+ "/";')
FALLBACK_SERVING = {"shared": "https://choc.localhost/", "path": "getHTMLInternal"}


def windows_serving(choc_text):
    """How choc serves setHTML on Windows: {"shared": its default home, every choc
    plugin's origin; "path": the page's path under the home}, or None. Requires the
    getURIHome branch that returns customSchemeURI (a "/" appended), which is
    what makes kEmbeddedOrigin the home."""
    home = re.search(r'#if CHOC_WINDOWS\s*\n\s*return "(https://[^"]+/)";', choc_text)
    page = re.search(r'setHTMLURI = defaultURI \+ "([^"]+)";', choc_text)
    if not (home and page and re.search(CUSTOM_HOME, choc_text)):
        return None
    return {"shared": home.group(1), "path": page.group(1)}


def rule_order(rc_rev, rev_bytes, ok_bytes):
    if rc_rev != 0:
        return [f"the list applied in reverse order stops (exit {rc_rev})"]
    if rev_bytes != ok_bytes:
        return ["the list applied in reverse order gives different bytes from in order"]
    return []


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
    if windows_serving(text) is None:
        errs.append("cannot find how choc serves the page on Windows (getURIHome's customSchemeURI "
                    "branch, its default home, setHTMLURI)")
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
    # Deletion first: `! deleted && ...` short-circuits before ownerPimpl is read.
    m = re.search(r"args->get_PermissionKind \(std::addressof \(permissionKind\)\);\s*(?://[^\n]*\n\s*)?"
                  r"if \(! deletionCheckerRef->deleted && ownerPimpl\.options\.allowPermission\)\s*\{(.*?)\}\s*"
                  + UPSTREAM_GRANT, text, re.S)
    if not m:
        errs.append("PermissionRequested does not consult allowPermission under "
                    "`if (! deletionCheckerRef->deleted && ownerPimpl.options.allowPermission)` "
                    "ahead of upstream's clipboard grant")
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


def added_lines(patch_text):
    """The lines a patch adds, as one text. It is what the patch itself says, so
    rules on it need no choc checkout (CI's fast job has none)."""
    body_at = patch_text.find("\n--- a/")
    return "\n".join(ln[1:] for ln in patch_text[body_at + 1:].splitlines()
                     if ln.startswith("+") and not ln.startswith("+++ "))


def interface_decl(text, name):
    """(id, base, methods in declared order, body) of interface `name`, or None."""
    m = re.search(r'MIDL_INTERFACE\("([0-9a-fA-F-]+)"\)\s*' + name + r" : public (\w+)\s*\{(.*?)\n\};", text, re.S)
    if not m:
        return None
    return (m.group(1).lower(), m.group(2),
            tuple(re.findall(r"virtual HRESULT STDMETHODCALLTYPE (\w+)\s*\(", m.group(3))), m.group(3))


def getter_iid(body):
    """The id a `static IID getIID()` returns, in MIDL_INTERFACE's text form, or None."""
    hx = r"(0x[0-9a-fA-F]+)"
    m = re.search(r"static IID getIID\(\)\s*\{ return \{ " + hx + ", " + hx + ", " + hx
                  + r", \{ ((?:0x[0-9a-fA-F]+(?:, )?){8}) \} \}; \}", body)
    if not m:
        return None
    d = [int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", m.group(4))]
    return "%08x-%04x-%04x-%02x%02x-%s" % (int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16),
                                           d[0], d[1], "".join("%02x" % x for x in d[2:]))


def rule_chain(text, chain):
    """Each interface in `chain` is declared in `text` with the SDK's id, base and
    method order, and a getIID() (where it has one) that returns the same id."""
    errs = []
    for name, iid, base, methods in chain:
        decl = interface_decl(text, name)
        if decl is None:
            errs.append(f"no MIDL_INTERFACE declaration of {name}")
            continue
        got_iid, got_base, got_methods, body = decl
        if got_iid != iid:
            errs.append(f"{name} is declared with id {got_iid}; the SDK's is {iid}")
        if got_base != base:
            errs.append(f"{name} derives from {got_base}; in the SDK it derives from {base}")
        if got_methods != methods:
            errs.append(f"{name} declares {got_methods}; the SDK's order is {methods}. A method missing or "
                        "out of order moves every later vtable slot, and a call lands in the wrong function")
        if body.count("virtual") != len(got_methods):
            errs.append(f"{name} has a virtual member this rule cannot read, so its slot count is unknown")
        if "getIID" in body and getter_iid(body) != got_iid:
            errs.append(f"{name}::getIID() returns {getter_iid(body)}, not the {got_iid} of its MIDL_INTERFACE line")
    return errs


# The whole of the added block: gated on the option being OFF (so the default path
# calls nothing), the query's result checked before use (so a runtime without
# Controller4 is left as it was), and FALSE passed.
DROP_CALL = (r"if \(! options\.allowExternalDrop\)\s*\{\s*"
             r"COMPtr<ICoreWebView2Controller4> controller4;\s*(?://[^\n]*\n\s*)*"
             r"if \(controller->QueryInterface \(ICoreWebView2Controller4::getIID\(\), "
             r"\(void\*\*\) controller4\.getAddress\(\)\) == S_OK\s*&& controller4 != nullptr\)\s*\{\s*"
             r"controller4->put_AllowExternalDrop \(FALSE\);\s*\}\s*\}")


def rule_drop(text):
    """The external-drop rules that the patch's added lines carry by themselves;
    `text` is those lines or the patched header."""
    errs = rule_chain(text, PATCH_CHAIN)
    decl = interface_decl(text, "ICoreWebView2Controller4")
    if decl is not None and "getIID" not in decl[3]:
        errs.append("ICoreWebView2Controller4 has no getIID() for the QueryInterface call")
    defaults = re.findall(r"^\s*bool allowExternalDrop = (\w+);", text, re.M)
    if defaults != ["true"]:
        errs.append(f"Options must declare `bool allowExternalDrop = true;` exactly once (found {defaults}): "
                    "any other default changes choc's behaviour for every user of the patch")
    if not re.search(DROP_CALL, text):
        errs.append("put_AllowExternalDrop (FALSE) is not called under `if (! options.allowExternalDrop)`, "
                    "after a QueryInterface for ICoreWebView2Controller4 that returned S_OK with a non-null pointer")
    if len(re.findall(r"->put_AllowExternalDrop \(", text)) != 1:
        errs.append("put_AllowExternalDrop must be called exactly once")
    if len(re.findall(r"options\.allowExternalDrop\b", text)) != 1:
        errs.append("allowExternalDrop must be read exactly once (the gate)")
    return errs


def rule_drop_in_header(text):
    """What only the patched header can show: the slots AHEAD of the patch's
    interfaces are the SDK's, and the call sits where the controller exists."""
    errs = rule_chain(text, UPSTREAM_CHAIN)
    m = re.search(r"void webviewControllerCreationComplete \(ICoreWebView2Controller\* controller, "
                  r"ICoreWebView2\* view\)\s*\{(.*?)\n    \}", text, re.S)
    fn = m.group(1) if m else ""
    held = fn.find("coreWebViewController = controller;")
    call = fn.find("controller4->put_AllowExternalDrop")
    ready = fn.find("options.webviewIsReady (owner)")
    if not 0 <= held < call < ready:
        errs.append("put_AllowExternalDrop is not called inside webviewControllerCreationComplete, after the "
                    "controller is held and before webviewIsReady")
    return errs


def dependent_patch(nav_only_text):
    """A planted patch that adds one line straight after the navigation patch's
    last Options line. Its context exists only once that patch has run, which is
    the fault ALONE and ORDER exist for (controls C37, C38)."""
    lines = nav_only_text.split("\n")
    at = next(i for i, ln in enumerate(lines) if ln.endswith("> allowNavigation;")) + 1
    hunk = ([f"@@ -{at - 2},6 +{at - 2},7 @@"] + [" " + s for s in lines[at - 3:at]]
            + ["+        bool planted = true;"] + [" " + s for s in lines[at:at + 3]])
    return f"planted\n\n--- a/{TARGET}\n+++ b/{TARGET}\n" + "\n".join(hunk) + "\n"


def rule_patched(text):
    return rule_navigation(text) + rule_permission(text) + rule_drop(text) + rule_drop_in_header(text)


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


def rule_wiring(win_text, common_text, policy_text, cmake_text, includers, serving, mm_text):
    errs = []
    if "apply_patch.cmake" not in cmake_text or not re.search(r'"-DPATCH=\$\{HS_CHOC_PATCHES\}"', cmake_text):
        errs.append("CMakeLists.txt does not run libs/patches/apply_patch.cmake with \"-DPATCH=${HS_CHOC_PATCHES}\" "
                    "(quoted: unquoted, the list splits into separate arguments)")
    if not re.search(r"target_include_directories\(\$\{PROJECT_NAME\}-impl PUBLIC \$\{HS_CHOC_DIR\}\)", cmake_text):
        errs.append("CMakeLists.txt does not put HS_CHOC_DIR on the impl's PUBLIC include path")
    for rel, line in includers:
        errs.append(f"{rel}: `{line.strip()}` compiles the UNPATCHED choc; use #include <choc/gui/choc_WebView.h>")
    cc = strip_comments(common_text)
    for opt in ("allowNavigation", "allowPermission", "customSchemeURI"):
        if not re.search(r"opts\." + opt + r"\s*=\s*std::move\(" + opt + r"\);", cc):
            errs.append(f"hypersaw_gui_common.h: makeWebView does not assign opts.{opt} from its parameter")
    if not re.search(r"opts\.allowExternalDrop\s*=\s*allowExternalDrop;", cc):
        errs.append("hypersaw_gui_common.h: makeWebView does not assign opts.allowExternalDrop from its parameter")
    # Position and default: the backend's `false` is positional, and macOS passes
    # nothing, so it must keep choc's default.
    if not re.search(r"allowPermission\s*=\s*\{\},\s*bool allowExternalDrop\s*=\s*true,\s*"
                     r"std::string customSchemeURI\s*=\s*\{\}\)", cc):
        errs.append("hypersaw_gui_common.h: makeWebView does not take `bool allowExternalDrop = true` between "
                    "allowPermission and customSchemeURI")
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
        if not re.search(r",\s*std::string\(detail::kEmbeddedOrigin\)\s*\)$", call):
            errs.append("hypersaw_gui_win.cpp: makeWebView is not passed std::string(detail::kEmbeddedOrigin) "
                        "as its customSchemeURI, so the page shares choc's default origin")
        if not re.search(r"\}\s*,\s*false\s*,\s*std::string\(detail::kEmbeddedOrigin\)\s*\)$", call):
            errs.append("hypersaw_gui_win.cpp: makeWebView is not passed `false` as allowExternalDrop, between "
                        "the permission policy and the origin, so WebView2 keeps accepting external drops")
    pc = strip_comments(policy_text)
    page = re.search(r'kEmbeddedPage\s*=\s*"([^"]*)"', pc)
    origin = re.search(r'kEmbeddedOrigin\s*=\s*"([^"]*)"', pc)
    if not page or not origin:
        errs.append("embedded_page_policy.h: no kEmbeddedPage or no kEmbeddedOrigin")
    else:
        home = origin.group(1) if origin.group(1).endswith("/") else origin.group(1) + "/"
        if not re.match(r"^https://[a-z0-9.-]+/$", home):
            errs.append(f"embedded_page_policy.h: kEmbeddedOrigin {origin.group(1)!r} is not an https origin")
        if serving is not None:
            if home == serving["shared"]:
                errs.append(f"embedded_page_policy.h: kEmbeddedOrigin is choc's shared default {home!r}: every "
                            "choc plugin's page lives there (critic HIGH-1)")
            if page.group(1) != home + serving["path"]:
                errs.append(f"embedded_page_policy.h: kEmbeddedPage is {page.group(1)!r} but choc serves the "
                            f"page at {home + serving['path']!r}")
    mc = strip_comments(mm_text)
    for name in ("kEmbeddedOrigin", "kEmbeddedPage", "customSchemeURI"):
        if name in mc:
            errs.append(f"hypersaw_gui.mm uses {name}: macOS must not depend on the Windows origin "
                        "(its page is about:blank; WKWebView refuses an https scheme handler)")
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
    mm_text = MM.read_text()
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
    # From the patch's own added lines, so it runs where there is no choc checkout.
    drop_text = texts.get(DROP_PATCH, "")
    drop_added = added_lines(drop_text)
    row("DROP", rule_drop(drop_added) if drop_text else [f"{DROP_PATCH} is missing"])

    pristine_path = CHOC / TARGET
    have_choc = pristine_path.exists()
    serving = None
    patched = ""
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
                serving = windows_serving(patched) if rc == 0 else None
                # Every ordering but the list's own; the reverse is among them.
                ok_bytes = (tmp / "ok" / TARGET).read_bytes() if rc == 0 else b"?"
                order_errs = []
                for i, perm in enumerate(itertools.permutations(order)):
                    if list(perm) == order:
                        continue
                    rcr, _ = run_apply(CHOC / "choc", tmp / f"perm{i}", list(perm))
                    rev = (tmp / f"perm{i}" / TARGET).read_bytes() if rcr == 0 else b""
                    names = ", ".join(pathlib.PurePosixPath(r).name for r in perm)
                    order_errs += [f"{e} [{names}]" for e in rule_order(rcr, rev, ok_bytes)]
                row("ORDER", order_errs)
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
                                 for n in ("allowNavigation", "allowPermission", "allowExternalDrop",
                                           "ICoreWebView2Controller4") if n in pristine])
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
                ungated = patched.replace(
                    "if (! deletionCheckerRef->deleted && ownerPimpl.options.allowPermission)\n", "if (true)\n", 1)
                control("C11 an ungated permission filter fails the permission rules",
                        ungated != patched and bool(rule_permission(ungated)))
                # C17: the owner read before the deletion check.
                late = patched.replace("if (! deletionCheckerRef->deleted && ownerPimpl.options.allowPermission)",
                                       "if (ownerPimpl.options.allowPermission && ! deletionCheckerRef->deleted)", 1)
                control("C17 a gate that reads the owner before checking deletion fails the permission rules",
                        late != patched and bool(rule_permission(late)))
                # C35: upstream's own controller interface a method short. Every later
                # slot moves, the patch's included, and only the chain rule can see it.
                short_chain = patched.replace(
                    "    virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;\n", "", 1)
                control("C35 an upstream controller interface with a method missing fails the chain rule",
                        short_chain != patched and not rule_drop(short_chain)
                        and bool(rule_drop_in_header(short_chain)))
                # C36: the lines the external-drop patch's third hunk sits on, changed.
                drift3 = tmp / "drift3" / "choc" / "gui"
                drift3.mkdir(parents=True)
                (drift3 / "choc_WebView.h").write_text(
                    pristine.replace("controller2->put_DefaultBackgroundColor ({ 0, 0, 0, 0 });",
                                     "controller2->put_DefaultBackgroundColor ({});", 1))
                rc5, _ = run_apply(tmp / "drift3" / "choc", tmp / "drift3out", [DROP_PATCH])
                control("C36 a drifted controller set-up stops the external-drop patch applied alone", rc5 != 0)
                # C37, C38: a patch that leans on another's lines. In list order it
                # applies (so the plant is a real patch); alone it does not, and
                # ahead of the patch it leans on it does not.
                dep = tmp / "dependent.patch"
                dep.write_text(dependent_patch(nav_only) if rc3 == 0 else "")
                rc6, _ = run_apply(CHOC / "choc", tmp / "dep_ok", [NAV_PATCH, dep])
                rc7, _ = run_apply(CHOC / "choc", tmp / "dep_alone", [dep])
                rc8, _ = run_apply(CHOC / "choc", tmp / "dep_rev", [dep, NAV_PATCH])
                dep_ok = (tmp / "dep_ok" / TARGET).read_bytes() if rc6 == 0 else b"?"
                dep_rev = (tmp / "dep_rev" / TARGET).read_bytes() if rc8 == 0 else b""
                control("C37 a patch that leans on another's lines does not apply alone", rc6 == 0 and rc7 != 0)
                control("C38 a patch that leans on another's lines fails ORDER",
                        rc6 == 0 and bool(rule_order(rc8, dep_rev, dep_ok)))
        else:
            notes.append("WARNING: libs/choc is not checked out; APPLY, ORDER, ALONE, CRLF, UPSTREAM and controls "
                         "C1, C2, C10, C11, C17 and C35 to C38 not run here, and the origin rules use FALLBACK_SERVING "
                         "(every build job applies the patches at configure, with a hard stop)")

    row("WIRING", rule_wiring(win_text, common_text, policy_text, cmake_text, relative_includers(files),
                              serving or FALLBACK_SERVING,
                              mm_text))

    # Controls on in-memory text.
    nav_text = texts.get(NAV_PATCH, "")
    other = "0" * 40 if link != "0" * 40 else "1" * 40
    control("C3 another gitlink fails PIN", bool(rule_pin(nav_text, other)))
    # C4: the first context line of the first hunk, turned into a removal.
    planted_removal = re.sub(r"(\n@@[^\n]*\n) ", r"\1-", nav_text, count=1)
    control("C4 a removed upstream line fails SHAPE",
            planted_removal != nav_text and bool(rule_shape(planted_removal)))

    def wiring(win=win_text, common=common_text, policy=policy_text, cmake=cmake_text, inc=(), mm=mm_text):
        return bool(rule_wiring(win, common, policy, cmake, relative_includers(files) + list(inc),
                                serving or FALLBACK_SERVING, mm))

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
    no_perm = re.sub(r",\s*\[\]\(choc::ui::WebView::Options::PermissionKind.*?\},", ",", win_text,
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
    shared = (serving or FALLBACK_SERVING)["shared"]
    shared_policy = re.sub(r'(kEmbeddedOrigin\s*=\s*")[^"]*"', r'\g<1>' + shared + '"', policy_text, count=1)
    shared_policy = re.sub(r'(kEmbeddedPage\s*=\s*")[^"]*"',
                           r'\g<1>' + shared + (serving or FALLBACK_SERVING)["path"] + '"', shared_policy, count=1)
    control("C18 kEmbeddedOrigin set to choc's shared default fails WIRING",
            shared_policy != policy_text and wiring(policy=shared_policy))
    no_origin = re.sub(r",\s*std::string\(detail::kEmbeddedOrigin\)\s*\)", ")", win_text, count=1)
    control("C19 the backend not passing the origin fails WIRING", no_origin != win_text and wiring(win=no_origin))
    no_origin_assign = common_text.replace("opts.customSchemeURI = std::move(customSchemeURI);", "", 1)
    control("C20 makeWebView without the origin assignment fails WIRING",
            no_origin_assign != common_text and wiring(common=no_origin_assign))
    control("C21 the macOS backend using the origin fails WIRING",
            wiring(mm=mm_text + "\nstatic auto planted = hypersaw::detail::kEmbeddedOrigin;\n"))
    control("C22 a reverse-order result that differs fails ORDER", bool(rule_order(0, b"a", b"b")))
    control("C23 a list naming a patch twice fails LIST",
            bool(listed) and bool(rule_list(listed + listed[:1], on_disk, readme_text)))

    # The external-drop patch (ADR-205 item 3). C24 to C28: the wiring, the list
    # and the shape. C29 to C34: the patch's own added lines, each with one fault.
    no_drop = re.sub(r"\n[ \t]*false,\n", "\n", win_text, count=1)
    control("C24 the backend without its external-drop argument fails WIRING",
            no_drop != win_text and wiring(win=no_drop))
    drop_on = re.sub(r"(\n[ \t]*)false,\n", r"\1true,\n", win_text, count=1)
    control("C25 the backend passing true for external drops fails WIRING",
            drop_on != win_text and wiring(win=drop_on))
    no_drop_assign = common_text.replace("opts.allowExternalDrop = allowExternalDrop;", "", 1)
    control("C26 makeWebView without the external-drop assignment fails WIRING",
            no_drop_assign != common_text and wiring(common=no_drop_assign))
    no_drop_listed = [r for r in listed if r != DROP_PATCH]
    control("C27 a list missing the external-drop patch fails LIST",
            no_drop_listed != listed and bool(rule_list(no_drop_listed, on_disk, readme_text)))
    drop_removal = re.sub(r"(\n@@[^\n]*\n) ", r"\1-", drop_text, count=1)
    control("C28 the external-drop patch with a removed upstream line fails SHAPE",
            drop_removal != drop_text and bool(rule_shape(drop_removal)))

    def drop_fault(old, new):
        """True if the patch's added lines, with `old` turned into `new`, fail DROP."""
        planted = drop_added.replace(old, new, 1)
        return planted != drop_added and bool(rule_drop(planted))

    control("C29 the option defaulting to false fails DROP",
            drop_fault("bool allowExternalDrop = true;", "bool allowExternalDrop = false;"))
    control("C30 a call made whatever the option says fails DROP",
            drop_fault("if (! options.allowExternalDrop)", "if (true)"))
    swap = ("    virtual HRESULT STDMETHODCALLTYPE get_RasterizationScale(double*) = 0;\n",
            "    virtual HRESULT STDMETHODCALLTYPE put_RasterizationScale(double) = 0;\n")
    control("C31 two Controller3 methods swapped fails DROP", drop_fault(swap[0] + swap[1], swap[1] + swap[0]))
    control("C32 an interface id one digit off fails DROP",
            drop_fault('MIDL_INTERFACE("97d418d5-', 'MIDL_INTERFACE("97d418d6-'))
    control("C33 a getIID() that disagrees with its MIDL_INTERFACE line fails DROP",
            drop_fault("0x97d418d5,", "0x97d418d6,"))
    control("C34 a call made when the query did not succeed fails DROP", drop_fault(" == S_OK", " != S_OK"))

    for n in notes:
        print(f"  {n}")
    if fails:
        print("choc_patch_check: FAIL", file=sys.stderr)
        for f in fails:
            print(f"    {f}", file=sys.stderr)
        return 1
    print(f"choc_patch_check: OK ({len(order)} patches; list, pin, shape, drop, wiring"
          + (", apply, order, alone, crlf, upstream" if have_choc else "")
          + f"; {29 + (9 if have_choc else 0)} controls red as designed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
