// Not run by ./verify: it needs the clean-room engine and a window server, neither of which this repo's CI has (B568). build.sh runs it.
//
// Opens the built plug-in's real window the way a host does, moves real controls through their own actions, and saves
// a picture. A plug-in window that fails shows nothing until the human opens it in Live, so this checks it first.
//   gui_shot <horde-engine-preview.clap> <out.png>
#import <AppKit/AppKit.h>
#include <clap/clap.h>
#include <dlfcn.h>

#include <cmath>
#include <cstdio>
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

    gui->hide(plugin);
    gui->destroy(plugin);
    pump(0.3);   // a timer left running would tick into freed state here
    check(true, "the window closes, and nothing ticks after");
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    (void)info;
  }
  std::printf("engine-preview gui: %s (%d failed)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
