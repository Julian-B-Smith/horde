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

#include "hypersaw_gui_common.h"

namespace hypersaw
{

struct HypersawGui::Impl
{
  GuiHost host;
  std::unique_ptr<choc::ui::WebView> web;
  HWND parent = nullptr;

  explicit Impl(GuiHost h) : host(std::move(h))
  {
    // The bind lives inside the ready callback (see makeWebView): here it runs
    // on the message loop once the WebView2 controller exists, after `web` is
    // assigned, so the body may read it.
    web = detail::makeWebView(host, [this](choc::ui::WebView &w) {
      w.bind("hzGrabKeys", [this](const choc::value::ValueView &) -> choc::value::Value {
        if (HWND h = (HWND)web->getViewHandle()) SetFocus(h);
        return {};
      });
    });
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
