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
// [control includes: P4 invariance matrix]
//
// [control includes: P6 soak]
#include "control_soak.h"
//
// [control includes: B459 the steal fade]
#include "control_steal.h"
//
// [control includes: B460 the spread ramp]
#include "control_spread.h"
//
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
// Each later package adds its makers under its own marker; the blank comment lines keep
// parallel pull requests from colliding.
//
// [control makers: P4 invariance matrix]
//
//
// [control makers: P6 soak]
// The long-run soak's seven plants (control_soak.h, package P6).
inline std::unique_ptr<Adapter> makeSoakSteadyControl() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kSteady); }
inline std::unique_ptr<Adapter> makeSoakLevelControl() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kLevel); }
inline std::unique_ptr<Adapter> makeSoakDcControl() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kDc); }
inline std::unique_ptr<Adapter> makeSoakWrap31Control() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kWrap31); }
inline std::unique_ptr<Adapter> makeSoakWrap32Control() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kWrap32); }
inline std::unique_ptr<Adapter> makeSoakNanControl() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kNan); }
inline std::unique_ptr<Adapter> makeSoakHangControl() { return std::make_unique<SoakPlantControl>(SoakPlantControl::kHang); }
//
// [control makers: B459 the steal fade]
// The steal fade's five plants (control_steal.h): no ramp; a cut inside the ramp; a ramp
// that stops short; a start that steps; a start that is not late enough.
inline std::unique_ptr<Adapter> makeStealCutControl() { return std::make_unique<StealPlantControl>(StealPlantControl::kCut); }
inline std::unique_ptr<Adapter> makeStealCut5Control() { return std::make_unique<StealPlantControl>(StealPlantControl::kCut5); }
inline std::unique_ptr<Adapter> makeStealShortControl() { return std::make_unique<StealPlantControl>(StealPlantControl::kShort); }
inline std::unique_ptr<Adapter> makeStealStepControl() { return std::make_unique<StealPlantControl>(StealPlantControl::kStep); }
inline std::unique_ptr<Adapter> makeStealEarlyControl() { return std::make_unique<StealPlantControl>(StealPlantControl::kEarly); }
//
// [control makers: B460 the spread ramp]
// The spread ramp's plants (control_spread.h): the ramp on blade 1 only; on blade 2 only;
// dropped at one tick in four; and the two-blade composition with nothing planted.
inline std::unique_ptr<Adapter> makeRampBlade1Control() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kBlade1); }
inline std::unique_ptr<Adapter> makeRampBlade2Control() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kBlade2); }
inline std::unique_ptr<Adapter> makeRampBothControl() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kBoth); }
inline std::unique_ptr<Adapter> makeRampSkipControl() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kSkip); }
//

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
  // [control rows: P4 invariance matrix]
  //
  //
  // [control rows: P6 soak]
  {"control/soak-steady", true, &makeSoakSteadyControl},
  {"control/soak-level", true, &makeSoakLevelControl},
  {"control/soak-dc", true, &makeSoakDcControl},
  {"control/soak-wrap31", true, &makeSoakWrap31Control},
  {"control/soak-wrap32", true, &makeSoakWrap32Control},
  {"control/soak-nan", true, &makeSoakNanControl},
  {"control/soak-hang", true, &makeSoakHangControl},
  //
  // [control rows: B459 the steal fade]
  {"control/steal-cut", true, &makeStealCutControl},
  {"control/steal-cut5", true, &makeStealCut5Control},
  {"control/steal-short", true, &makeStealShortControl},
  {"control/steal-step", true, &makeStealStepControl},
  {"control/steal-early", true, &makeStealEarlyControl},
  //
  // [control rows: B460 the spread ramp]
  {"control/ramp-blade1", true, &makeRampBlade1Control},
  {"control/ramp-blade2", true, &makeRampBlade2Control},
  {"control/ramp-both", true, &makeRampBothControl},
  {"control/ramp-skip", true, &makeRampSkipControl},
  //
};

// Null for an id the table does not have.
inline std::unique_ptr<Adapter> makeAdapter(const char* id) {
  for (const AdapterRow& r : kAdapters) if (std::strcmp(r.id, id) == 0) return r.make();
  return nullptr;
}

}  // namespace horde2::harness
