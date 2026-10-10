/*
 * hypersaw_gui_win.cpp — Windows (WebView2 via choc) backend of the GUI seam.
 * Same shape as the macOS .mm: everything platform-specific lives here.
 *
 * Status: first native load 2026-09-09 (Live 12.3 Beta, Windows 11): embeds,
 * plays. The GUI was a black rectangle until the bridge moved into choc's
 * ready callback (hypersaw_gui_common.h, makeWebView). Focus and resize under
 * a host are still only lightly exercised.
 */
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cwchar>
#include <memory>
#include <string>
#include <utility>

#include "hypersaw_gui_common.h"

namespace hypersaw
{

/* B446 — THE WEB VIEW SHOWS ONE PAGE, AND ONLY THAT PAGE TALKS TO THE PLUGIN.
   The Windows half of the rule hypersaw_gui.mm applies on macOS (its
   lockToEmbeddedPage carries the reasoning). choc keeps its WebView2 object
   private, so the rule goes through the allowNavigation option that
   libs/patches/choc-webview2-navigation.patch adds (ADR-194 D-S5). The rule
   itself is detail::embeddedPagePolicy (embedded_page_policy.h): one load of
   the embedded page, no frames, no new windows, messages only from that page.
   It is a pure function so tools/embedded_page_policy_check can test its
   behaviour; this file only hands it to the view. Refused navigations are
   cancelled, and refused new windows are marked handled, so none opens.
   Runs on the message thread only, so pageState needs no lock.
   Runtime-unverified on Windows as of 2026-10-08; see
   traces/2026-10-08-b446-choc-win-nav.md.

   B446 — NO CLIPBOARD READS FOR PAGE SCRIPT (ADR-196). Upstream choc grants
   every clipboard read a page asks for. The allowPermission option that
   libs/patches/choc-webview2-permissions.patch adds is handed
   detail::webPermissionPolicy, which refuses every permission kind, so page
   script cannot read the clipboard. PASTE reads it natively instead
   (hzPasteState, below), the macOS rule (hypersaw_gui.mm): the text goes to
   the state parser and never reaches the page. Runtime-unverified on Windows
   as of 2026-10-08; see traces/2026-10-08-b446-choc-win-clipboard.md.

   B446 — ITS OWN ORIGIN (critic HIGH-1, PR #973). The page loads from
   detail::kEmbeddedOrigin, passed to choc as customSchemeURI, not from choc's
   shared default: embedded_page_policy.h ORIGIN says why.

   B448 C3 — NO DROPS FROM OUTSIDE THE VIEW (ADR-205 item 3). The GUI has no
   drag-and-drop feature. The allowExternalDrop option that
   libs/patches/choc-webview2-external-drop.patch adds is passed false, which
   sets WebView2's AllowExternalDrop off: the Windows counterpart of
   unregisterDraggedTypes in hypersaw_gui.mm. It is one layer of three, and
   each stands without the others: the page refuses dragenter, dragover and
   drop in script (tools/gui_sink_check.py), and the navigation rule above
   refuses any navigation a drop could start. Where the installed WebView2
   runtime lacks ICoreWebView2Controller4 the switch is not set, and those two
   still apply. Runtime-unverified on Windows as of 2026-10-10 (B447); see
   traces/2026-10-10-choc-patch-external-drop.md. */
namespace
{
// Closes the clipboard and unlocks its block on every path out, exceptions
// included (the std::string below can throw).
struct ClipboardOpen
{
  const bool open;
  explicit ClipboardOpen(HWND owner) : open(OpenClipboard(owner) != FALSE) {}
  ~ClipboardOpen() { if (open) CloseClipboard(); }
  ClipboardOpen(const ClipboardOpen &) = delete;
  ClipboardOpen &operator=(const ClipboardOpen &) = delete;
};
struct GlobalLocked
{
  HGLOBAL h;
  const wchar_t *text;
  explicit GlobalLocked(HGLOBAL g) : h(g), text(g ? static_cast<const wchar_t *>(GlobalLock(g)) : nullptr) {}
  ~GlobalLocked() { if (text) GlobalUnlock(h); }
  GlobalLocked(const GlobalLocked &) = delete;
  GlobalLocked &operator=(const GlobalLocked &) = delete;
};

enum class ClipText { none, tooLarge, text };

/* The clipboard's text as UTF-8 in `out`. The size cap (kMaxPastedStateBytes,
   input_guards.h) is applied BEFORE anything is allocated: the scan stops one
   UTF-16 unit past the cap (each unit is at least one UTF-8 byte, so more units
   than the cap is over it), and the UTF-8 size is measured by a sizing call
   before `out` is grown. The scan is also bounded by the block's size, so text
   with no terminator cannot be over-read. */
ClipText clipboardTextUtf8(HWND owner, std::string &out)
{
  ClipboardOpen clip(owner);
  if (!clip.open) return ClipText::none;
  GlobalLocked block(GetClipboardData(CF_UNICODETEXT));
  if (!block.text) return ClipText::none;
  const size_t units = GlobalSize(block.h) / sizeof(wchar_t);
  const size_t n = wcsnlen(block.text, units < kMaxPastedStateBytes + 1 ? units : kMaxPastedStateBytes + 1);
  if (n == 0) return ClipText::none;
  if (!pastedStateFits(n)) return ClipText::tooLarge;
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, block.text, (int)n, nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) return ClipText::none;
  if (!pastedStateFits((size_t)bytes)) return ClipText::tooLarge;
  out.resize((size_t)bytes);
  WideCharToMultiByte(CP_UTF8, 0, block.text, (int)n, out.data(), bytes, nullptr, nullptr);
  return ClipText::text;
}
}  // namespace

struct HypersawGui::Impl
{
  GuiHost host;
  std::unique_ptr<choc::ui::WebView> web;
  HWND parent = nullptr;
  detail::EmbeddedPageState pageState;

  explicit Impl(GuiHost h) : host(std::move(h))
  {
    // The bind lives inside the ready callback (see makeWebView): here it runs
    // on the message loop once the WebView2 controller exists, after `web` is
    // assigned, so the body may read it.
    web = detail::makeWebView(
        host,
        [this](choc::ui::WebView &w) {
          /* B446: PASTE without giving the page the clipboard, as hzPasteState
             does on macOS: the text goes straight to the shell's state parser
             and only a status comes back: 1 applied, 0 nothing to paste, 2 not
             a patch. */
          w.bind("hzPasteState", [this](const choc::value::ValueView &) -> choc::value::Value {
            std::string text;
            switch (clipboardTextUtf8((HWND)web->getViewHandle(), text))
            {
              case ClipText::none: return choc::value::createInt32(0);
              case ClipText::tooLarge: return choc::value::createInt32(2);
              case ClipText::text: break;
            }
            return choc::value::createInt32(pasteStatus(text, [this](std::string_view t) {
              return host.applyStateJson && host.applyStateJson(std::string(t), std::string());
            }));
          });
          w.bind("hzGrabKeys", [this](const choc::value::ValueView &) -> choc::value::Value {
            if (HWND h = (HWND)web->getViewHandle()) SetFocus(h);
            return {};
          });
        },
        [this](choc::ui::WebView::Options::NavigationType type, const std::string &uri) {
          return detail::embeddedPagePolicy(pageState, type, uri);
        },
        [](choc::ui::WebView::Options::PermissionKind kind) {
          return detail::webPermissionPolicy(kind);
        },
        // allowExternalDrop: off (NO DROPS FROM OUTSIDE THE VIEW, above). The
        // comment sits before the argument: choc_patch_check's planted faults
        // edit this call as text, and one after the comma would hide the origin
        // argument from control C19.
        false,
        std::string(detail::kEmbeddedOrigin));
  }
};

HypersawGui::HypersawGui(GuiHost host) : impl(new Impl(std::move(host)))
{
  /* The Windows half of the 2026-08-12 lingering-note fix, which the .mm has
     had since then and this file never did — so the JS side's release
     request was a no-op here and the first Windows GUI session (2026-09-09,
     Live 12.3 Beta) reproduced the exact Mac symptom: click a control, the
     computer-keyboard MIDI dies, because WebView2's inner Chromium HWND takes
     focus on click and keeps it. Hand focus back to the host's parent HWND,
     which is what makes Live see the keys again; same policy as the Mac
     (resign to the PARENT, never to nothing). Installed here rather than in
     Impl's constructor because it reads `parent`, which attachToParent fills
     in later. */
  impl->host.releaseKeyFocus = [this]() {
    if (impl->parent && IsWindow(impl->parent)) SetFocus(impl->parent);
  };
}

HypersawGui::~HypersawGui() { delete impl; }

bool HypersawGui::attachToParent(void *parentView)
{
  if (!impl->web) return false;
  HWND parent = (HWND)parentView;
  HWND child = (HWND)impl->web->getViewHandle();
  if (!parent || !child) return false;
  SetWindowLongPtr(child, GWL_STYLE,
                   (GetWindowLongPtr(child, GWL_STYLE) | WS_CHILD) & ~(LONG_PTR)WS_POPUP);
  SetParent(child, parent);
  RECT r;
  GetClientRect(parent, &r);
  SetWindowPos(child, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
               SWP_NOZORDER | SWP_SHOWWINDOW);
  impl->parent = parent;
  return true;
}

void HypersawGui::getSize(uint32_t &width, uint32_t &height) const
{
  width = detail::kGuiWidth;
  height = detail::kGuiHeight;
}

}  // namespace hypersaw

#endif  // _WIN32
