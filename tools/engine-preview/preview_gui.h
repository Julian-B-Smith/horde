#pragma once
// What the preview's windows need from the plug-in, and nothing more: the web view hosting GUI 3's page
// (preview_webview.mm, the default) or the AppKit fallback (preview_gui.mm, HORDE_PREVIEW_WEBVIEW=OFF). Whichever is
// built defines gui_extension(). All of it runs on the main thread. A `plugin` here is clap_plugin_t::plugin_data.
#include <clap/clap.h>

#include <cstddef>

namespace preview {

const clap_plugin_gui_t* gui_extension();

constexpr clap_id kPresetId = 0xB00001;   // 0xB00002 was Voices, retired 2026-11-05: never reuse it for another meaning

int bank_size();
const char* preset_name(int index);
int row_count();
clap_id row_id(int row);
const char* row_key(int row);
int row_steps(int row);   // the option count for a stepped row, 0 for a continuous one

int preset(void* plugin);
// The engine's voice.polyphony, the one voice control: not a host parameter, so the window sets it through the edit
// queue like the page does. Its options are consecutive voice counts, polyphony_lo() to polyphony_hi().
int polyphony_lo();
int polyphony_hi();
int polyphony(void* plugin);
void set_polyphony(void* plugin, int voices);
double row_host_value(void* plugin, int row);
void row_text(int row, double host_value, char* out, std::size_t size);
void stats(void* plugin, int preset_index, double* mean_pct, double* worst_pct, unsigned long long* overruns);

// Queues a change for the audio thread, which also tells the host, so a move in the window behaves like automation.
void edit(void* plugin, clap_id id, double host_value);

void*& gui_slot(void* plugin);
const char* build_label();   // "engine <commit>, build <n>"

// GUI 3's bridge (B628, bridge_core.h): a page message in, and the next snapshot out, valid until the next call.
void bridge_receive(void* plugin, const char* json);
const char* bridge_frame(void* plugin);
void bridge_resend(void* plugin);   // the page knows nothing yet: the next frame carries everything again (no-op after a hello)
void bridge_open(void* plugin);     // a new window, a new page: the session starts over

}  // namespace preview
