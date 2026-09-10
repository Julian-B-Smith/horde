/*
 * hypersaw_gui.mm — macOS (WKWebView via choc) backend of the GUI seam.
 * Objective-C++ so the NSView attach is plain Cocoa; the JS bridge and viz
 * serializer are shared with the Windows backend (hypersaw_gui_common.h).
 * All bindings run on the main thread and touch only GuiHost callbacks.
 */

#import <Cocoa/Cocoa.h>

#include "hypersaw_gui_common.h"

namespace hypersaw
{

/* B79 — THE PLUGIN WEBVIEW WAS A DEGRADED SURFACE, MEASURED: the in-GUI
   health line inside Ableton read `frame 69ms · dpr 1` (≈14 fps rAF while
   fully visible, non-retina canvases on a retina display) against 6 ms and
   dpr 2 for the identical page in a browser. Two WebKit behaviours cause it:
   WKWebView's occlusion heuristic decides a host's child view is "background"
   and throttles rAF, and the WebContent process gets visibility-based
   suppression on top.

   The knobs that turn those off are WebKit SPI. They are reached through KVC
   (`setValue:forKey:@"windowOcclusionDetectionEnabled"` resolves to
   `_setWindowOcclusionDetectionEnabled:` via KVC's `_set<Key>:` search rule)
   rather than private headers, and every call sits in @try — a WebKit rename
   degrades to a silent no-op, never a crash, and the health line then shows
   `raf 69ms` again so the regression is VISIBLE rather than mysterious. The
   JS-side timer watchdog stays as the fallback for exactly that case. This is
   a locally-installed instrument, not App Store material; the tradeoff is
   recorded here and in B79. Exit criterion, readable in the GUI corner:
   `raf 16ms · dpr 2`. */
static void configureSurfaceForPluginWindow(NSView *child)
{
  if (!child || ![child isKindOfClass:NSClassFromString(@"WKWebView")]) return;
  id wk = child;
  @try { [wk setValue:@NO forKey:@"windowOcclusionDetectionEnabled"]; } @catch (NSException *) {}
  @try
  {
    id prefs = [[wk valueForKey:@"configuration"] valueForKey:@"preferences"];
    [prefs setValue:@NO forKey:@"pageVisibilityBasedProcessSuppressionEnabled"];
  } @catch (NSException *) {}
  @try
  {
    // dpr 1 in-window means WebKit never picked up the backing scale; override
    // with the real one. Window when attached, main screen before that.
    const CGFloat sc = child.window ? child.window.backingScaleFactor
                                    : NSScreen.mainScreen.backingScaleFactor;
    if (sc > 1.0) [wk setValue:@(sc) forKey:@"overrideDeviceScaleFactor"];
  } @catch (NSException *) {}
}

struct HypersawGui::Impl
{
  GuiHost host;
  std::unique_ptr<choc::ui::WebView> web;
  void *parentView = nullptr;

  explicit Impl(GuiHost h) : host(std::move(h))
  {
    // Hosts often do not hand the plugin view keyboard focus on click; the
    // GUI's text-entry path requests it explicitly (2026-07-18 report: edit
    // boxes lost focus instantly in Live — the INVERSE of the classic
    // webview-steals-keys problem). Bound from the ready callback, which on
    // macOS fires synchronously inside makeWebView — before `web` is assigned,
    // which is why the bind uses `w` and only the BODY (invoked from JS later)
    // reads `web`.
    web = detail::makeWebView(host, [this](choc::ui::WebView &w) {
      w.bind("hzGrabKeys", [this](const choc::value::ValueView &) -> choc::value::Value {
        NSView *v = (__bridge NSView *)web->getViewHandle();
        if (v && v.window)
        {
          // makeFirstResponder on a non-key window never receives keys — Live
          // keeps key status on its main window, so claim it first. Live also
          // re-takes it moments later (2026-07-18 report: "focus for a split
          // second"), which is why the JS side re-grabs for the edit's lifetime
          // rather than trusting one call.
          if (![v.window isKeyWindow]) [v.window makeKeyWindow];
          [v.window makeFirstResponder:v];
        }
        return {};
      });
    });
    // B79: throttle/suppression off as early as possible; the retina override
    // is re-applied at attach, when the real window (and its scale) exists.
    configureSurfaceForPluginWindow((__bridge NSView *)web->getViewHandle());
  }
};

HypersawGui::HypersawGui(GuiHost host) : impl(new Impl(std::move(host)))
{
  /* Installed here, not in Impl's constructor: it needs `impl` to exist so it
     can read parentView, which attachToParent fills in later. Resigning to the
     PARENT rather than to nil matters — nil leaves the window with no first
     responder, and Live does not necessarily route keys anywhere useful then. */
  impl->host.releaseKeyFocus = [this]() {
    if (!impl->parentView) return;
    NSView *parent = (__bridge NSView *)impl->parentView;
    if (NSWindow *w = [parent window])
      if ([w firstResponder] != parent) [w makeFirstResponder:parent];
  };
}

HypersawGui::~HypersawGui() { delete impl; }

bool HypersawGui::attachToParent(void *parentView)
{
  if (!impl->web) return false;
  NSView *parent = (__bridge NSView *)parentView;
  NSView *child = (__bridge NSView *)impl->web->getViewHandle();
  if (!parent || !child) return false;
  [child setFrame:[parent bounds]];
  [child setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [parent addSubview:child];
  configureSurfaceForPluginWindow(child);   // B79: now the window's scale is real

  /* THE FIX for the 2026-08-12 lingering-note report. A WKWebView becomes first
     responder on click and then keeps it, so every subsequent keystroke goes to
     us instead of to Live — and Live, which generates the computer-keyboard
     notes, stops seeing key-ups. We give focus back to the host's view after any
     interaction that does not need text entry (the JS side decides which). We
     cannot make the host send the note-off it never generated; we can stop being
     the reason it never generates one. */
  impl->parentView = parentView;
  return true;
}

void HypersawGui::getSize(uint32_t &width, uint32_t &height) const
{
  width = detail::kGuiWidth;
  height = detail::kGuiHeight;
}

}  // namespace hypersaw
