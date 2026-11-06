#pragma once
// The preview's private CLAP extension (B628): GUI 3's bridge, reachable through the plug-in's own interface, so the
// smoke test and the frames dumper drive exactly the path the window drives, from outside the binary. A host never
// asks for it. Every call is main-thread only.
#include <clap/clap.h>

#include <cstdint>

static constexpr const char kPreviewBridgeExt[] = "com.mindlathe.horde-engine-preview.bridge/1";

struct preview_bridge_ext {
  // One page message, as the page's hordeNativeSend passes it: JSON text.
  void (*receive)(const clap_plugin_t* plugin, const char* json);
  // The next snapshot, as the window's 30 fps timer would push it: JSON text, valid until the next call.
  const char* (*frame)(const clap_plugin_t* plugin);
  // Tests only, between process calls: the live engine's stored v for a row (NaN for an unknown id, or no engine).
  double (*engine_v)(const clap_plugin_t* plugin, std::uint32_t id);
};
