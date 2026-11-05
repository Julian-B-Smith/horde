// The preview's window (B624): the human couldn't reach Live's generic panel, so Preset, Voices and the rows get plain
// AppKit controls, plus a readout of what the preset costs. Not GUI 3, and it owes GUI 3 nothing: no web view, because
// that would add its own CPU to the very thing this plug-in measures.
//
// The target class is built at runtime under a name holding this image's address. Objective-C classes share one global
// namespace per process, and the VST3 and AU copies of this code can both be loaded: with one fixed name, one copy's
// controls would call into the other's, and crash when that bundle unloads.
#import <AppKit/AppKit.h>
#include <objc/runtime.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "preview_gui.h"

namespace {

constexpr int kWidth = 620, kHeight = 660, kHeaderHeight = 132, kRowHeight = 26, kGutter = 16;
constexpr NSInteger kPrevTag = 1, kNextTag = 2, kPresetTag = 3, kVoicesTag = 4, kRowTagBase = 1000;
constexpr double kTouchHoldSeconds = 0.4;   // how long a moved control ignores the readback, so it can't jump under the hand

struct RowControls {
  NSSlider* slider = nil;
  NSPopUpButton* popup = nil;
  NSTextField* value = nil;
  double touched_at = 0;
};

struct GuiState {
  void* plugin = nullptr;
  NSView* root = nil;
  id target = nil;
  NSTimer* timer = nil;
  NSPopUpButton* preset = nil;
  NSPopUpButton* voices = nil;
  NSTextField* readout = nil;
  std::vector<RowControls> rows;
  double preset_touched_at = 0, voices_touched_at = 0;
};

// "osc1.blade2.fmDepth" to "Osc 1  Blade 2  FM depth": the keys are stable addresses, not names to read.
std::string label_for(const char* key) {
  std::string out;
  std::string part;
  auto flush = [&] {
    if (part.empty()) return;
    std::string word;
    for (std::size_t i = 0; i < part.size(); ++i) {
      const char c = part[i];
      if (std::isdigit((unsigned char)c) && i > 0 && !std::isdigit((unsigned char)part[i - 1])) word += ' ';
      if (std::isupper((unsigned char)c) && i > 0) {
        word += ' ';
        word += char(std::tolower((unsigned char)c));
      } else {
        word += c;
      }
    }
    word[0] = char(std::toupper((unsigned char)word[0]));
    if (word.rfind("Fm", 0) == 0) word.replace(0, 2, "FM");
    if (!out.empty()) out += "  ";
    out += word;
    part.clear();
  };
  for (const char* p = key; *p; ++p) {
    if (*p == '.') flush();
    else part += *p;
  }
  flush();
  return out;
}

NSTextField* make_label(const std::string& text, NSRect frame, CGFloat size, NSColor* colour) {
  NSTextField* label = [NSTextField labelWithString:[NSString stringWithUTF8String:text.c_str()]];
  label.frame = frame;
  label.font = [NSFont systemFontOfSize:size];
  label.textColor = colour;
  return label;
}

GuiState* state_of(id target) {
  void* out = nullptr;
  object_getInstanceVariable(target, "state", &out);
  return static_cast<GuiState*>(out);
}

double now() { return [NSDate timeIntervalSinceReferenceDate]; }

void choose_preset(GuiState& g, int index) {
  index = std::clamp(index, 0, preview::bank_size() - 1);
  g.preset_touched_at = now();
  [g.preset selectItemAtIndex:index];
  preview::edit(g.plugin, preview::kPresetId, index);
}

void act_imp(id self, SEL, id sender) {
  GuiState& g = *state_of(self);
  const NSInteger tag = [sender tag];
  if (tag == kPrevTag) { choose_preset(g, int(g.preset.indexOfSelectedItem) - 1); return; }
  if (tag == kNextTag) { choose_preset(g, int(g.preset.indexOfSelectedItem) + 1); return; }
  if (tag == kPresetTag) { choose_preset(g, int(g.preset.indexOfSelectedItem)); return; }
  if (tag == kVoicesTag) {
    g.voices_touched_at = now();
    preview::edit(g.plugin, preview::kVoicesId, double(g.voices.indexOfSelectedItem + 1));
    return;
  }
  const int row = int(tag - kRowTagBase);
  if (row < 0 || row >= int(g.rows.size())) return;
  RowControls& c = g.rows[std::size_t(row)];
  c.touched_at = now();
  const double value = c.popup ? double(c.popup.indexOfSelectedItem) : c.slider.doubleValue;
  preview::edit(g.plugin, preview::row_id(row), value);
  char text[64];
  preview::row_text(row, value, text, sizeof text);
  c.value.stringValue = [NSString stringWithUTF8String:text];
}

// Five times a second: enough to follow a preset change or automation, and cheap beside what it measures.
void tick_imp(id self, SEL, id) {
  GuiState& g = *state_of(self);
  const double t = now();
  const int preset = preview::preset(g.plugin);
  if (t - g.preset_touched_at > kTouchHoldSeconds && g.preset.indexOfSelectedItem != preset) [g.preset selectItemAtIndex:preset];
  const int voices = preview::voices(g.plugin);
  if (t - g.voices_touched_at > kTouchHoldSeconds && g.voices.indexOfSelectedItem != voices - 1) [g.voices selectItemAtIndex:voices - 1];

  for (int row = 0; row < int(g.rows.size()); ++row) {
    RowControls& c = g.rows[std::size_t(row)];
    if (t - c.touched_at <= kTouchHoldSeconds) continue;
    const double value = preview::row_host_value(g.plugin, row);
    if (c.popup) {
      if (c.popup.indexOfSelectedItem != NSInteger(value)) [c.popup selectItemAtIndex:NSInteger(value)];
    } else if (c.slider.doubleValue != value) {
      c.slider.doubleValue = value;
    }
    char text[64];
    preview::row_text(row, value, text, sizeof text);
    NSString* s = [NSString stringWithUTF8String:text];
    if (![c.value.stringValue isEqualToString:s]) c.value.stringValue = s;
  }

  double mean = 0, worst = 0;
  unsigned long long overruns = 0;
  preview::stats(g.plugin, preset, &mean, &worst, &overruns);
  char line[160];
  if (worst == 0) std::snprintf(line, sizeof line, "Play a note to see what this preset costs.");
  else std::snprintf(line, sizeof line, "This preset: averaging %.0f %% of the time it has, worst %.0f %%. %llu block%s too late.", mean, worst, overruns, overruns == 1 ? "" : "s");
  NSString* s = [NSString stringWithUTF8String:line];
  if (![g.readout.stringValue isEqualToString:s]) g.readout.stringValue = s;
  g.readout.textColor = overruns ? NSColor.systemRedColor : NSColor.secondaryLabelColor;
}

Class target_class() {
  static Class cls = [] {
    char name[64];
    std::snprintf(name, sizeof name, "HordeEnginePreviewTarget_%p", (void*)&kWidth);
    Class c = objc_allocateClassPair([NSObject class], name, 0);
    class_addIvar(c, "state", sizeof(void*), 3, "^v");
    class_addMethod(c, @selector(act:), (IMP)act_imp, "v@:@");
    class_addMethod(c, @selector(tick:), (IMP)tick_imp, "v@:@");
    objc_registerClassPair(c);
    return c;
  }();
  return cls;
}

void build(GuiState& g) {
  g.target = [[target_class() alloc] init];
  object_setInstanceVariable(g.target, "state", &g);
  g.root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
  // Its own appearance and its own ground: a host's window can be light or dark, and text colours that follow the system
  // over a ground that follows the host came out white on white in the first picture of this window.
  g.root.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
  g.root.wantsLayer = YES;
  g.root.layer.backgroundColor = [NSColor colorWithSRGBRed:0.13 green:0.13 blue:0.15 alpha:1].CGColor;

  CGFloat y = kHeight - 34;
  [g.root addSubview:make_label("horde ENGINE PREVIEW", NSMakeRect(kGutter, y, 300, 22), 15, NSColor.labelColor)];
  [g.root addSubview:make_label("Test build: oscillators only", NSMakeRect(kWidth - 216, y + 2, 200, 16), 11, NSColor.tertiaryLabelColor)];
  ((NSTextField*)g.root.subviews.lastObject).alignment = NSTextAlignmentRight;

  y -= 36;
  [g.root addSubview:make_label("Preset", NSMakeRect(kGutter, y + 4, 50, 18), 12, NSColor.secondaryLabelColor)];
  NSButton* prev = [NSButton buttonWithTitle:@"<" target:g.target action:@selector(act:)];
  prev.tag = kPrevTag;
  prev.frame = NSMakeRect(kGutter + 54, y, 36, 26);
  [g.root addSubview:prev];
  g.preset = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(kGutter + 94, y, 300, 26) pullsDown:NO] autorelease];
  for (int i = 0; i < preview::bank_size(); ++i) {
    // addItemWithTitle would merge duplicates and treat some titles specially; a menu item keeps the bank's order.
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:preview::preset_name(i)] action:nil keyEquivalent:@""] autorelease];
    [g.preset.menu addItem:item];
  }
  g.preset.tag = kPresetTag;
  g.preset.target = g.target;
  g.preset.action = @selector(act:);
  [g.root addSubview:g.preset];
  NSButton* next = [NSButton buttonWithTitle:@">" target:g.target action:@selector(act:)];
  next.tag = kNextTag;
  next.frame = NSMakeRect(kGutter + 398, y, 36, 26);
  [g.root addSubview:next];

  [g.root addSubview:make_label("Voices", NSMakeRect(kWidth - kGutter - 112, y + 4, 44, 18), 12, NSColor.secondaryLabelColor)];
  g.voices = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(kWidth - kGutter - 64, y, 64, 26) pullsDown:NO] autorelease];
  for (int i = 1; i <= preview::kMaxVoicesParam; ++i) [g.voices addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
  g.voices.tag = kVoicesTag;
  g.voices.target = g.target;
  g.voices.action = @selector(act:);
  [g.root addSubview:g.voices];

  y -= 30;
  g.readout = make_label("Play a note to see what this preset costs.", NSMakeRect(kGutter, y, kWidth - 2 * kGutter, 18), 12, NSColor.secondaryLabelColor);
  [g.root addSubview:g.readout];

  const int n = preview::row_count();
  g.rows.resize(std::size_t(n));
  const CGFloat doc_height = n * kRowHeight + 8;
  const CGFloat scroll_width = kWidth, scroll_height = kHeight - kHeaderHeight;
  NSScrollView* scroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, scroll_width, scroll_height)] autorelease];
  scroll.hasVerticalScroller = YES;
  scroll.borderType = NSNoBorder;
  scroll.drawsBackground = NO;
  NSView* doc = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, scroll_width - 16, doc_height)] autorelease];
  for (int row = 0; row < n; ++row) {
    const CGFloat ry = doc_height - (row + 1) * kRowHeight;
    RowControls& c = g.rows[std::size_t(row)];
    [doc addSubview:make_label(label_for(preview::row_key(row)), NSMakeRect(kGutter, ry + 4, 216, 18), 12, NSColor.labelColor)];
    const int steps = preview::row_steps(row);
    if (steps > 0) {
      c.popup = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(kGutter + 222, ry, 96, 24) pullsDown:NO] autorelease];
      for (int k = 0; k < steps; ++k) [c.popup addItemWithTitle:[NSString stringWithFormat:@"%d", k]];
      c.popup.tag = kRowTagBase + row;
      c.popup.target = g.target;
      c.popup.action = @selector(act:);
      c.popup.controlSize = NSControlSizeSmall;
      [doc addSubview:c.popup];
    } else {
      c.slider = [NSSlider sliderWithValue:0 minValue:0 maxValue:1 target:g.target action:@selector(act:)];
      c.slider.frame = NSMakeRect(kGutter + 222, ry + 2, 250, 20);
      c.slider.tag = kRowTagBase + row;
      c.slider.continuous = YES;
      [doc addSubview:c.slider];
    }
    c.value = make_label("", NSMakeRect(kWidth - 16 - kGutter - 70, ry + 4, 70, 18), 11, NSColor.secondaryLabelColor);
    c.value.alignment = NSTextAlignmentRight;
    c.value.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
    [doc addSubview:c.value];
  }
  scroll.documentView = doc;
  [doc scrollPoint:NSMakePoint(0, doc_height)];
  [g.root addSubview:scroll];

  tick_imp(g.target, @selector(tick:), nil);
  g.timer = [NSTimer timerWithTimeInterval:0.2 target:g.target selector:@selector(tick:) userInfo:nil repeats:YES];
  // Common modes, or the readout would freeze while the host's menus or a dragged slider hold the run loop.
  [[NSRunLoop currentRunLoop] addTimer:g.timer forMode:NSRunLoopCommonModes];
}

bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool is_floating) { return !is_floating && !std::strcmp(api, CLAP_WINDOW_API_COCOA); }
bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* is_floating) {
  *api = CLAP_WINDOW_API_COCOA;
  *is_floating = false;
  return true;
}
bool gui_create(const clap_plugin_t* p, const char* api, bool is_floating) {
  if (!gui_is_api_supported(p, api, is_floating)) return false;
  void*& slot = preview::gui_slot(p->plugin_data);
  if (slot) return false;
  auto* g = new GuiState;
  g->plugin = p->plugin_data;
  build(*g);
  slot = g;
  return true;
}
void gui_destroy(const clap_plugin_t* p) {
  void*& slot = preview::gui_slot(p->plugin_data);
  auto* g = static_cast<GuiState*>(slot);
  if (!g) return;
  [g->timer invalidate];   // first: the timer retains the target and would tick into freed state
  [g->root removeFromSuperview];
  [g->root release];
  [g->target release];
  delete g;
  slot = nullptr;
}
bool gui_set_scale(const clap_plugin_t*, double) { return false; }   // AppKit scales by itself
bool gui_get_size(const clap_plugin_t*, std::uint32_t* w, std::uint32_t* h) {
  *w = kWidth;
  *h = kHeight;
  return true;
}
bool gui_can_resize(const clap_plugin_t*) { return false; }
bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t*) { return false; }
bool gui_adjust_size(const clap_plugin_t*, std::uint32_t* w, std::uint32_t* h) {
  *w = kWidth;
  *h = kHeight;
  return true;
}
bool gui_set_size(const clap_plugin_t*, std::uint32_t w, std::uint32_t h) { return w == kWidth && h == kHeight; }
bool gui_set_parent(const clap_plugin_t* p, const clap_window_t* window) {
  auto* g = static_cast<GuiState*>(preview::gui_slot(p->plugin_data));
  if (!g || !window->cocoa) return false;
  [static_cast<NSView*>(window->cocoa) addSubview:g->root];
  return true;
}
bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }
void gui_suggest_title(const clap_plugin_t*, const char*) {}
bool gui_show(const clap_plugin_t*) { return true; }
bool gui_hide(const clap_plugin_t*) { return true; }

const clap_plugin_gui_t kGui = {gui_is_api_supported, gui_get_preferred_api, gui_create, gui_destroy, gui_set_scale, gui_get_size, gui_can_resize,
                                gui_get_resize_hints, gui_adjust_size, gui_set_size, gui_set_parent, gui_set_transient, gui_suggest_title, gui_show, gui_hide};

}  // namespace

const clap_plugin_gui_t* preview::gui_extension() { return &kGui; }
