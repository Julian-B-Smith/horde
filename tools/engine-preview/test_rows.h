#pragma once
// The test rows the bridge publishes (B711): one descriptor per host parameter of the test-only filter (B660) and reverb
// (B710), built from the same tables params_get_info reads, so the page and the host can't be told two stories. Shared
// by the plug-in and bridge_test; smoke holds the plug-in's published copy to its CLAP parameter list, independently.
// A build compiled without the filter or the reverb (the bit-identity plants) has none of its rows here either.
#include <string>
#include <vector>

#include "bridge_core.h"
#include "filter_core.h"
#ifndef HORDE_PREVIEW_PLANT_NO_REVERB
#include "post_reverb.h"
#endif

namespace preview::bridge {

inline std::vector<TestRow> test_rows() {
  std::vector<TestRow> out;
#ifndef HORDE_PREVIEW_PLANT_NO_FILTER
  for (int f = 0; f < filter::kParams; ++f) {
    TestRow r;
    r.key = std::string(kTestPrefix) + "filter." + filter::key_name(f);
    r.id = filter::id_of(f);
    r.name = filter::param_name(f);
    r.min = 0;
    r.max = filter::max_value(f);
    r.def = filter::default_value(f);
    r.stepped = filter::stepped(f);
    // The labels params_value_to_text gives: Filter on reads off or on, Filter mode Weir's mode names. The cutoff has no
    // unit of its own: its value is a position on a log law (filter::cutoff_hz), which the README states.
    if (f == filter::kOn) r.labels = {"off", "on"};
    if (f == filter::kMode)
      for (int i = 0; i < filter::kModeCount; ++i) r.labels.emplace_back(filter::mode_name(i));
    out.push_back(std::move(r));
  }
#endif
#ifndef HORDE_PREVIEW_PLANT_NO_REVERB
  for (int k = -1; k < reverb::kScapeParams; ++k) {   // -1 is Scape on
    TestRow r;
    r.key = std::string(kTestPrefix) + "scape." + (k < 0 ? "on" : reverb::info(k).name);
    r.id = k < 0 ? reverb::kOnId : reverb::id_of(k);
    char text[64];
    reverb::param_name(k, text, sizeof text);
    r.name = text;
    r.min = reverb::min_value(k);
    r.max = reverb::max_value(k);
    r.def = reverb::default_value(k);
    r.stepped = reverb::stepped(k);
    for (int i = 0; r.stepped && i <= int(r.max - r.min); ++i) {
      reverb::value_text(k, r.min + i, text, sizeof text);   // Scape's own names for an enum, off and on for Scape on
      r.labels.emplace_back(text);
    }
    if (k >= 0 && !reverb::is_enum(k)) r.unit = reverb::decl(k).unit;   // an enum's unit is "enum": its labels say it
    out.push_back(std::move(r));
  }
#endif
  return out;
}

}  // namespace preview::bridge
