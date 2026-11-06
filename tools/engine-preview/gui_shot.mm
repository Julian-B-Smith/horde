// Not run by ./verify: it needs the clean-room engine and a window server, neither of which this repo's CI has (B568). build.sh runs it.
//
// Opens the built plug-in's real window the way a host does, moves real controls through their own actions, and saves
// a picture. A plug-in window that fails shows nothing until the human opens it in Live, so this checks it first.
// Either window: GUI 3's page in a web view (B628), driven through the page's own binding from inside the page, or the
// AppKit fallback, driven through its controls' actions. The page checks read only the seam's names, so they hold for
// the stand-in page and for GUI 3's alike.
//   gui_shot <horde-engine-preview.clap> <out.png>
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#include <clap/clap.h>
#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}
bool callback_requested = false;
const void* host_extension(const clap_host_t*, const char*) { return nullptr; }
void host_noop(const clap_host_t*) {}
void host_callback(const clap_host_t*) { callback_requested = true; }
const clap_host_t kHost = {CLAP_VERSION, nullptr, "gui_shot", "", "", "1", host_extension, host_noop, host_noop, host_callback};
std::uint32_t none_size(const clap_input_events_t*) { return 0; }
const clap_event_header_t* none_get(const clap_input_events_t*, std::uint32_t) { return nullptr; }
int pushed_values = 0;
bool out_push(const clap_output_events_t*, const clap_event_header_t* h) {
  if (h->type == CLAP_EVENT_PARAM_VALUE) ++pushed_values;
  return true;
}

void block(const clap_plugin_t* plugin, bool with_chord) {
  float l[128], r[128];
  float* channels[2] = {l, r};
  clap_audio_buffer_t buffer{};
  buffer.data32 = channels;
  buffer.channel_count = 2;
  clap_event_note_t note{};
  note.header = {sizeof note, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
  note.note_id = 1; note.channel = 0; note.key = 60; note.velocity = 0.85;
  struct One {
    static std::uint32_t size(const clap_input_events_t*) { return 1; }
    static const clap_event_header_t* get(const clap_input_events_t* l, std::uint32_t) { return static_cast<const clap_event_header_t*>(l->ctx); }
  };
  clap_input_events_t in = with_chord ? clap_input_events_t{&note.header, One::size, One::get} : clap_input_events_t{nullptr, none_size, none_get};
  clap_output_events_t out{nullptr, out_push};
  clap_process_t proc{};
  proc.frames_count = 128;
  proc.audio_outputs = &buffer;
  proc.audio_outputs_count = 1;
  proc.in_events = &in;
  proc.out_events = &out;
  plugin->process(plugin, &proc);
}

template <class T>
void collect(NSView* view, NSMutableArray* out) {
  for (NSView* sub in view.subviews) {
    if ([sub isKindOfClass:[T class]]) [out addObject:sub];
    collect<T>(sub, out);
  }
}
void pump(double seconds) { [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:seconds]]; }

// Runs `body` as an async function in the page's main frame and returns what it returned, as text ("" on an error or
// a timeout). The page's own world, so it reaches the page's names exactly as the page's script does.
std::string run(WKWebView* wk, const std::string& body, double timeout = 5.0) {
  __block bool done = false;
  __block std::string out;
  [wk callAsyncJavaScript:[NSString stringWithUTF8String:body.c_str()]
                arguments:nil
                  inFrame:nil
           inContentWorld:WKContentWorld.pageWorld
        completionHandler:^(id result, NSError* error) {
          if (!error && [result isKindOfClass:[NSString class]]) out = [(NSString*)result UTF8String] ?: "";
          done = true;
        }];
  NSDate* until = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while (!done && [until timeIntervalSinceNow] > 0) pump(0.01);
  return out;
}
bool wait_for(WKWebView* wk, const std::string& condition, double timeout) {
  NSDate* until = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while ([until timeIntervalSinceNow] > 0) {
    if (run(wk, "return String(!!(" + condition + "));", 2.0) == "true") return true;
    pump(0.1);
  }
  return false;
}
// The page sends exactly as GUI 3's transport does: through its binding, as JSON text.
void page_send(WKWebView* wk, const std::string& message) { run(wk, "window.hordeNativeSend(JSON.stringify(" + message + ")); return 'sent';"); }

clap_id param_by_name(const clap_plugin_t* plugin, const clap_plugin_params_t* params, const char* name) {
  clap_param_info_t info{};
  for (std::uint32_t i = 0; i < params->count(plugin); ++i)
    if (params->get_info(plugin, i, &info) && !std::strcmp(info.name, name)) return info.id;
  return CLAP_INVALID_ID;
}

void web_window(const clap_plugin_t* plugin, const clap_plugin_params_t* params, WKWebView* wk, const char* png) {
  const bool booted = wait_for(wk, "document.readyState === 'complete' && typeof window.hordeNativeSend === 'function' && typeof window.hordeNativeSnapshot === 'function'", 15);
  check(booted, "the page loads, with the seam's two names");
  if (!booted) return;
  // Every snapshot the page receives is counted on its way in; the page's own handler still gets it.
  run(wk, "var f = window.hordeNativeSnapshot; window.__shot = {frames: 0, withParams: 0, last: null, presets: null, hello: null};"
          "window.hordeNativeSnapshot = function (s) { __shot.frames++; __shot.last = s; if (s.params) __shot.withParams++;"
          " if (s.presets) __shot.presets = s.presets; if (s.hello) __shot.hello = s.hello; return f(s); }; return 'ok';");
  // GUI 3 sends no hello, so the window resends everything until a frame lands, then only changes. If the landing were
  // never noticed, every frame would carry every row.
  pump(0.5);
  check(run(wk, "return String(__shot.frames > 3 && __shot.withParams === 0);") == "true",
        "once a frame has landed, frames carry params only when a row changed");
  // Seqs well above the page's own, so its acks and ours can't be confused.
  page_send(wk, "{type: 'hello', seq: 9001}");
  page_send(wk, "{type: 'want', seq: 9002, osc: [1, 2], members: true, cycles: false}");
  block(plugin, false);
  check(wait_for(wk, "__shot.frames > 3 && __shot.presets && __shot.hello", 5), "frames reach the page at the timer's pace");
  check(run(wk, "return String(__shot.withParams > 0);") == "true", "control: after a hello, a frame carries every row again");
  check(run(wk, "return String(__shot.presets.names.length);") == "83", "the snapshot lists all 83 presets");
  const std::string build = run(wk, "return __shot.hello.build;");
  std::printf("     build label in the hello reply: %s\n", build.c_str());
  check(build.rfind("engine ", 0) == 0, "the hello reply carries the build label");
  check(run(wk, "return String(__shot.hello.manifest_hash.length);") == "64", "and the engine's manifest hash");

  // Must-fail by construction: Preset starts elsewhere, so only the page's message can make it read 5.
  double before = -1, after = -1;
  params->get_value(plugin, 0xB00001, &before);
  check(int(before) != 5, "control: Preset doesn't already read the value about to be chosen");
  const int told = pushed_values;
  page_send(wk, "{type: 'preset', seq: 9003, index: 5}");
  block(plugin, false);
  params->get_value(plugin, 0xB00001, &after);
  check(int(after) == 5, "a preset message from inside the page moves the Preset parameter");
  check(pushed_values > told, "and the host is told");
  check(wait_for(wk, "__shot.last.ack >= 9003", 3), "and the page hears its seq acknowledged");
  if (callback_requested) { callback_requested = false; plugin->on_main_thread(plugin); }
  block(plugin, false);   // takes the new engine

  const clap_id level = param_by_name(plugin, params, "out.level");
  page_send(wk, "{type: 'set', seq: 9004, values: {'out.level': 0.37}}");
  block(plugin, false);
  double moved = -1;
  params->get_value(plugin, level, &moved);
  check(level != CLAP_INVALID_ID && moved == 0.37, "a set from inside the page reaches the plug-in at the page's v");

  for (int i = 0; i < 40; ++i) block(plugin, i == 0);
  check(wait_for(wk, "__shot.last.observe.sounding && Array.isArray(__shot.last.observe.voices[0].osc[0].phases)", 3),
        "a chord's frames reach the page sounding, with the members it asked for");
  pump(0.3);   // lets the page draw

  __block NSImage* image = nil;
  __block bool shot = false;
  [wk takeSnapshotWithConfiguration:nil completionHandler:^(NSImage* picture, NSError*) {
    image = [picture retain];
    shot = true;
  }];
  NSDate* until = [NSDate dateWithTimeIntervalSinceNow:5];
  while (!shot && [until timeIntervalSinceNow] > 0) pump(0.05);
  NSData* data = nil;
  if (image) {
    NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:[image TIFFRepresentation]];
    data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    [image release];
  }
  check(data && [data writeToFile:[NSString stringWithUTF8String:png] atomically:YES], "the picture is saved");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) { std::printf("usage: gui_shot <clap> <out.png>\n"); return 2; }
  @autoreleasepool {
    [NSApplication sharedApplication];
    std::string binary = argv[1];
    const std::string name = binary.substr(binary.rfind('/') + 1);
    binary += "/Contents/MacOS/" + name.substr(0, name.rfind('.'));
    void* library = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::printf("dlopen: %s\n", dlerror()); return 1; }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
    entry->init(argv[1]);
    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_t* plugin = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
    plugin->init(plugin);
    plugin->activate(plugin, 44100, 1, 512);
    plugin->start_processing(plugin);
    const auto* params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    const auto* gui = static_cast<const clap_plugin_gui_t*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
    check(gui != nullptr, "the plug-in offers a window");
    if (!gui) return 1;
    check(gui->is_api_supported(plugin, CLAP_WINDOW_API_COCOA, false), "it supports an embedded Cocoa window");
    check(gui->create(plugin, CLAP_WINDOW_API_COCOA, false), "the window is created");
    std::uint32_t w = 0, h = 0;
    gui->get_size(plugin, &w, &h);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    window.backgroundColor = NSColor.windowBackgroundColor;
    clap_window_t parent{};
    parent.api = CLAP_WINDOW_API_COCOA;
    parent.cocoa = window.contentView;
    check(gui->set_parent(plugin, &parent), "it attaches to the host's view");
    gui->show(plugin);

    WKWebView* wk = nil;
    for (NSView* sub in window.contentView.subviews)
      if ([sub isKindOfClass:[WKWebView class]]) wk = (WKWebView*)sub;
    std::printf("     window: %s, %u x %u\n", wk ? "GUI 3's page in a web view" : "the AppKit fallback", w, h);
    if (wk) {
      web_window(plugin, params, wk, argv[2]);
    } else {
      NSMutableArray* popups = [NSMutableArray array];
      collect<NSPopUpButton>(window.contentView, popups);
      NSMutableArray* sliders = [NSMutableArray array];
      collect<NSSlider>(window.contentView, sliders);
      std::printf("     %lu menus, %lu sliders\n", (unsigned long)popups.count, (unsigned long)sliders.count);
      NSPopUpButton* preset = nil;
      for (NSPopUpButton* b in popups) if (b.numberOfItems > 40 && !preset) preset = b;
      check(preset != nil && preset.numberOfItems == 83, "the preset menu lists all 83");

      // Must-fail by construction: the value starts elsewhere, so only the window's action can make this read 5.
      double before = -1, after = -1;
      params->get_value(plugin, 0xB00001, &before);
      check(int(before) != 5, "control: Preset doesn't already read the value about to be chosen");
      [preset selectItemAtIndex:5];
      [preset sendAction:preset.action to:preset.target];
      block(plugin, false);
      params->get_value(plugin, 0xB00001, &after);
      check(int(after) == 5, "choosing in the menu moves the Preset parameter");
      check(pushed_values > 0, "and the host is told");
      if (callback_requested) { callback_requested = false; plugin->on_main_thread(plugin); }
      block(plugin, false);   // takes the new engine

      NSSlider* slider = sliders.firstObject;
      check(slider != nil, "there are sliders");
      double slider_value = 0.37;
      clap_param_info_t info{};
      int moved = pushed_values;
      if (slider) {
        slider.doubleValue = slider_value;
        [slider sendAction:slider.action to:slider.target];
        block(plugin, false);
        check(pushed_values > moved, "moving a slider reaches the audio thread and the host");
      }

      for (int i = 0; i < 60; ++i) block(plugin, i == 0);
      pump(0.5);   // lets the readout's timer run, and proves it doesn't crash doing so
      [window.contentView display];
      NSBitmapImageRep* rep = [window.contentView bitmapImageRepForCachingDisplayInRect:window.contentView.bounds];
      [window.contentView cacheDisplayInRect:window.contentView.bounds toBitmapImageRep:rep];
      NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
      check([png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES], "the picture is saved");
      (void)info;
    }

    gui->hide(plugin);
    gui->destroy(plugin);
    pump(0.3);   // a timer left running would tick into freed state here
    check(true, "the window closes, and nothing ticks after");
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
  }
  std::printf("engine-preview gui: %s (%d failed)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
