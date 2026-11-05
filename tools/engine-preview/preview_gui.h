#pragma once
// What the preview's window (preview_gui.mm) needs from the plug-in, and nothing more. All of it runs on the main
// thread. A `plugin` here is clap_plugin_t::plugin_data.
#include <clap/clap.h>

#include <cstddef>

namespace preview {

const clap_plugin_gui_t* gui_extension();

constexpr clap_id kPresetId = 0xB00001, kVoicesId = 0xB00002;
constexpr int kMaxVoicesParam = 16;

int bank_size();
const char* preset_name(int index);
int row_count();
clap_id row_id(int row);
const char* row_key(int row);
int row_steps(int row);   // the option count for a stepped row, 0 for a continuous one

int preset(void* plugin);
int voices(void* plugin);
double row_host_value(void* plugin, int row);
void row_text(int row, double host_value, char* out, std::size_t size);
void stats(void* plugin, int preset_index, double* mean_pct, double* worst_pct, unsigned long long* overruns);

// Queues a change for the audio thread, which also tells the host, so a move in the window behaves like automation.
void edit(void* plugin, clap_id id, double host_value);

void*& gui_slot(void* plugin);

}  // namespace preview
