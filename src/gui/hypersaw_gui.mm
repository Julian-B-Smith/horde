/*
 * hypersaw_gui.mm — macOS (WKWebView via choc) backend of the GUI seam.
 * Objective-C++ so the NSView attach is plain Cocoa; the JS bridge and viz
 * serializer are shared with the Windows backend (hypersaw_gui_common.h).
 * All bindings run on the main thread and touch only GuiHost callbacks.
 */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <objc/runtime.h>

#include <memory>
#include <string>
#include "hypersaw_gui_common.h"

namespace hypersaw
{

/* B446 — THE WEB VIEW SHOWS ONE PAGE, AND ONLY THAT PAGE TALKS TO THE PLUGIN.
   The bridge's bindings are document-start user scripts, so they belong to
   whatever document the view shows; the rule is therefore that it shows only
   the embedded page. Applied here, in our wrapper, with libs/choc untouched
   (ADR-194 D-S5):
   - NAVIGATION. A navigation delegate that admits exactly one load, the
     embedded page (loadHTMLString:baseURL:nil, which WebKit reports as
     about:blank), and cancels every navigation after it in every frame. It
     replaces choc's navigation delegate, whose only job was a failure page;
     that page is retired with it. New windows stay refused as before: choc's
     UI delegate does not implement createWebViewWithConfiguration:.
   - SENDER. The "external" message handler is re-registered behind a check
     that the message comes from the main frame of the admitted page, then
     forwarded to choc's handler unchanged.
   - CLIPBOARD. choc enables javaScriptCanAccessClipboard and DOMPasteAllowed
     for every view; both go off, so page script has no clipboard read. PASTE
     reads natively instead (hzPasteState, below) and the text never reaches
     the page.
   Installed from the ready callback, after choc has built the view and its
   bindings and BEFORE setHTML, so the first navigation the policy sees is the
   page's own. The guard is a runtime class with a unique name (choc's
   createDelegateClass), because two builds of this plugin can share one host
   process and a fixed Objective-C class name would collide. Its state lives in
   associated objects, so it needs no C++ owner: the content controller holds
   it, and choc's destructor releases it by removing the "external" handler.
   tools/gui_webview_check drives all of this on a real WKWebView. */
namespace
{
char kInnerHandlerKey;   // associated-object keys: only the ADDRESS matters (non-const,
char kPageAdmittedKey;   // so the compiler can never fold the two into one)

Class navGuardClass()
{
  static Class cls = [] {
    Class c = choc::objc::createDelegateClass("NSObject", "HordeNavGuard_");
    class_addMethod(c, @selector(webView:decidePolicyForNavigationAction:decisionHandler:),
                    imp_implementationWithBlock(^(id self, WKWebView *, WKNavigationAction *action,
                                                  void (^decide)(WKNavigationActionPolicy)) {
                      const bool admit = !objc_getAssociatedObject(self, &kPageAdmittedKey)
                                         && action.targetFrame && action.targetFrame.isMainFrame
                                         && [action.request.URL.absoluteString isEqualToString:@"about:blank"];
                      if (admit) objc_setAssociatedObject(self, &kPageAdmittedKey, @YES, OBJC_ASSOCIATION_RETAIN);
                      decide(admit ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
                    }),
                    "v@:@@@?");
    class_addMethod(c, @selector(userContentController:didReceiveScriptMessage:),
                    imp_implementationWithBlock(^(id self, WKUserContentController *ucc, WKScriptMessage *msg) {
                      if (!msg.frameInfo.isMainFrame || !objc_getAssociatedObject(self, &kPageAdmittedKey))
                        return;   // dropped: the caller's promise simply never settles
                      id inner = objc_getAssociatedObject(self, &kInnerHandlerKey);
                      [inner userContentController:ucc didReceiveScriptMessage:msg];
                    }),
                    "v@:@@");
    objc_registerClassPair(c);
    return c;
  }();
  return cls;
}

void lockToEmbeddedPage(choc::ui::WebView &w)
{
  WKWebView *wk = (__bridge WKWebView *)w.getViewHandle();
  if (!wk) return;
  // `configuration` is a copy, but it shares the live controller and
  // preferences objects with the view (B79's preference writes rely on the same).
  WKWebViewConfiguration *config = wk.configuration;
  id chocHandler = (id)wk.navigationDelegate;   // choc's one delegate object
  id guard = [[navGuardClass() alloc] init];
  objc_setAssociatedObject(guard, &kInnerHandlerKey, chocHandler, OBJC_ASSOCIATION_RETAIN);
  [config.userContentController removeScriptMessageHandlerForName:@"external"];
  [config.userContentController addScriptMessageHandler:guard name:@"external"];   // retains the guard
  wk.navigationDelegate = guard;                                                 // weak
  [guard release];
  @try
  {
    [config.preferences setValue:@NO forKey:@"javaScriptCanAccessClipboard"];
    [config.preferences setValue:@NO forKey:@"DOMPasteAllowed"];
  } @catch (NSException *) {}
}
}  // namespace

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
  /* B118 (human 2026-09-14: "the history hotkeys still aren't working"). The
     page's keydown listener only fires while the webview is first responder,
     and hosts keep key status on their own window — Live answers Cmd+Z with
     ITS undo before our view ever sees a key. This process-local monitor sees
     the event first. It claims Cmd/Ctrl+Z and Shift+Cmd/Ctrl+Z ONLY while the
     pointer is over our view (the "keyboard follows the mouse" rule the GUI
     already applies), steps the shell's history directly, and swallows the
     event so the host's undo does not fire as well. When our view IS first
     responder the event is left alone: the page's own listener handles it and
     keeps text-field undo native. */
  id keyMonitor = nil;
  void installKeyMonitor()
  {
    if (keyMonitor || !web) return;
    keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                                       handler:^NSEvent *(NSEvent *e) {
      const NSEventModifierFlags mods = e.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
      const bool cmd = (mods & NSEventModifierFlagCommand) || (mods & NSEventModifierFlagControl);
      if (!cmd || (mods & NSEventModifierFlagOption)) return e;
      NSString *k = [e.charactersIgnoringModifiers lowercaseString];
      if (![k isEqualToString:@"z"]) return e;
      NSView *v = web ? (__bridge NSView *)web->getViewHandle() : nil;
      if (!v || !v.window) return e;
      /* Cmd+Z is a MENU key equivalent in every host: with our view as first
         responder the window offers it to the views, WKWebView declines, and
         the host's Edit > Undo takes it before the page ever sees a keydown —
         so Ctrl+Z reached the page and Cmd+Z did not (human 2026-09-14: "ctrl+z
         works instead of cmd+z"). Claim Cmd+Z here whenever the pointer is over
         us; defer to the page only for Ctrl+Z, which no host menu owns. The
         one cost: Cmd+Z inside one of our own text fields steps history rather
         than the field's text — Ctrl+Z still does the native thing there. */
      const bool isCmd = (mods & NSEventModifierFlagCommand) != 0;
      if (!isCmd && [v.window firstResponder] == v) return e;   // the page's listener owns Ctrl+Z
      const NSPoint p = [v convertPoint:[v.window mouseLocationOutsideOfEventStream] fromView:nil];
      if (!NSPointInRect(p, v.bounds)) return e;      // pointer over the host: the host's undo
      if (host.undoStep) host.undoStep((mods & NSEventModifierFlagShift) ? +1 : -1);
      return nil;
    }];
  }
  ~Impl()
  {
    if (keyMonitor) { [NSEvent removeMonitor:keyMonitor]; keyMonitor = nil; }
  }

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
      lockToEmbeddedPage(w);   // B446: before setHTML, which runs right after this callback
      /* B446: PASTE without giving the page the clipboard. The text goes from
         the pasteboard straight to the shell's state parser — exactly what
         hzApplyState(text) did when the page read it — and only a status comes
         back: 1 applied, 0 nothing to paste, 2 not a patch (pasteStatus,
         input_guards.h). The size cap is checked on the string's lengths
         before its UTF-8 bytes are asked for (critic MEDIUM-3, PR #973): a
         UTF-16 length over the cap is over it in UTF-8 too, and
         lengthOfBytesUsingEncoding: counts without building the bytes. */
      w.bind("hzPasteState", [this](const choc::value::ValueView &) -> choc::value::Value {
        NSString *s = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (!s.length) return choc::value::createInt32(0);
        if (!pastedStateFits(s.length) || !pastedStateFits([s lengthOfBytesUsingEncoding:NSUTF8StringEncoding]))
          return choc::value::createInt32(2);
        const char *utf8 = s.UTF8String;
        if (!utf8) return choc::value::createInt32(0);
        return choc::value::createInt32(pasteStatus(std::string_view(utf8), [this](std::string_view t) {
          return host.applyStateJson && host.applyStateJson(std::string(t), std::string());
        }));
      });
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
  impl->installKeyMonitor();                // B118: undo/redo keys, host-independent

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
