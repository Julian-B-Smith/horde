/*
 * adapters.h — the table of harness adapters: one row per ENROLLED engine or
 * module, then the control adapters. ROADMAP B448 phase 2, P2 (ADR-209).
 *
 * ENROLLING AN ENGINE (the full steps are in h2/harness/README.md): write
 * adapter_<id>.h, include it here, add its row to kAdapters, and add the unit to
 * docs/armor/engines.json. tools/engine_enrolment_check.py reads this file's rows
 * as TEXT and compares them with that registry, so keep one row per line in the
 * exact form  {"<id>", false, &make<Name>},  . h2/harness/selftest.mjs then asks
 * the built binary for its table (--list) and compares again, so the text cannot
 * drift from what was compiled.
 */
#pragma once

#include <cstring>
#include <memory>

#include "adapter_composed.h"
#include "control_fuzz.h"
#include "harness.h"

namespace horde2::harness {

struct AdapterRow {
  const char* id;
  bool control;   // true: a control adapter, not an engine (never in the registry)
  std::unique_ptr<Adapter> (*make)();
};

inline std::unique_ptr<Adapter> makeComposedAdapter() { return std::make_unique<ComposedAdapter>(); }
inline std::unique_ptr<Adapter> makeSilentControl() { return std::make_unique<SilentControl>(); }
inline std::unique_ptr<Adapter> makeToneControl() { return std::make_unique<ToneControl>(); }
inline std::unique_ptr<Adapter> makeBlockControl() { return std::make_unique<BlockControl>(); }
// The parameter fuzz's ten planted faults (control_fuzz.h, package P3).
inline std::unique_ptr<Adapter> makeFuzzBlowupControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kBlowup); }
inline std::unique_ptr<Adapter> makeFuzzSlowControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kSlow); }
inline std::unique_ptr<Adapter> makeFuzzPlateauControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kPlateau); }
inline std::unique_ptr<Adapter> makeFuzzBurstControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kBurst); }
inline std::unique_ptr<Adapter> makeFuzzLoudControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kLoud); }
inline std::unique_ptr<Adapter> makeFuzzKeyedControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kKeyed); }
inline std::unique_ptr<Adapter> makeFuzzNanControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kNan); }
inline std::unique_ptr<Adapter> makeFuzzNanTailControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kNanTail); }
inline std::unique_ptr<Adapter> makeFuzzHangControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kHang); }
inline std::unique_ptr<Adapter> makeFuzzLateControl() { return std::make_unique<FuzzPlantControl>(FuzzPlantControl::kLate); }

inline constexpr AdapterRow kAdapters[] = {
  {"composed", false, &makeComposedAdapter},
  {"control/silent", true, &makeSilentControl},
  {"control/tone", true, &makeToneControl},
  {"control/block", true, &makeBlockControl},
  {"control/fuzz-blowup", true, &makeFuzzBlowupControl},
  {"control/fuzz-slow", true, &makeFuzzSlowControl},
  {"control/fuzz-plateau", true, &makeFuzzPlateauControl},
  {"control/fuzz-burst", true, &makeFuzzBurstControl},
  {"control/fuzz-loud", true, &makeFuzzLoudControl},
  {"control/fuzz-keyed", true, &makeFuzzKeyedControl},
  {"control/fuzz-nan", true, &makeFuzzNanControl},
  {"control/fuzz-nan-tail", true, &makeFuzzNanTailControl},
  {"control/fuzz-hang", true, &makeFuzzHangControl},
  {"control/fuzz-late", true, &makeFuzzLateControl},
};

// Null for an id the table does not have.
inline std::unique_ptr<Adapter> makeAdapter(const char* id) {
  for (const AdapterRow& r : kAdapters) if (std::strcmp(r.id, id) == 0) return r.make();
  return nullptr;
}

}  // namespace horde2::harness
