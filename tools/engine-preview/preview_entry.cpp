// The exported clap_entry, apart from the impl library, per the clap-first idiom: the wrapper formats link the
// impl without a second exported entry (same split as src/hypersaw_clap_entry.cpp).
#include <clap/clap.h>

extern "C" {
bool preview_entry_init(const char* plugin_path);
void preview_entry_deinit(void);
const void* preview_entry_get_factory(const char* factory_id);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
const CLAP_EXPORT struct clap_plugin_entry clap_entry = {CLAP_VERSION, preview_entry_init, preview_entry_deinit, preview_entry_get_factory};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
