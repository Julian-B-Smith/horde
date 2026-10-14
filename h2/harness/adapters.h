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
// [control includes: B487 the cut latch] (the fault build only: empty without H2_ENGINE_FAULTS)
#include "control_latch.h"
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
// One kind of value left stepped on one blade, everything else ramped (seven kinds, two
// blades), and the walk probe (two engines, one with the ramp: how far apart they walk).
inline std::unique_ptr<Adapter> makeStepControl(int kind, int blade) { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kStep, kind, blade); }
inline std::unique_ptr<Adapter> makeStepPositionB1() { return makeStepControl(SpreadPlantControl::kPosition, 1); }
inline std::unique_ptr<Adapter> makeStepPositionB2() { return makeStepControl(SpreadPlantControl::kPosition, 2); }
inline std::unique_ptr<Adapter> makeStepCutB1() { return makeStepControl(SpreadPlantControl::kCut, 1); }
inline std::unique_ptr<Adapter> makeStepCutB2() { return makeStepControl(SpreadPlantControl::kCut, 2); }
inline std::unique_ptr<Adapter> makeStepRotationB1() { return makeStepControl(SpreadPlantControl::kRotation, 1); }
inline std::unique_ptr<Adapter> makeStepRotationB2() { return makeStepControl(SpreadPlantControl::kRotation, 2); }
inline std::unique_ptr<Adapter> makeStepWidthB1() { return makeStepControl(SpreadPlantControl::kWidth, 1); }
inline std::unique_ptr<Adapter> makeStepWidthB2() { return makeStepControl(SpreadPlantControl::kWidth, 2); }
inline std::unique_ptr<Adapter> makeStepDepthB1() { return makeStepControl(SpreadPlantControl::kDepth, 1); }
inline std::unique_ptr<Adapter> makeStepDepthB2() { return makeStepControl(SpreadPlantControl::kDepth, 2); }
inline std::unique_ptr<Adapter> makeStepIndexB1() { return makeStepControl(SpreadPlantControl::kIndex, 1); }
inline std::unique_ptr<Adapter> makeStepIndexB2() { return makeStepControl(SpreadPlantControl::kIndex, 2); }
inline std::unique_ptr<Adapter> makeStepBandB1() { return makeStepControl(SpreadPlantControl::kBand, 1); }
inline std::unique_ptr<Adapter> makeStepBandB2() { return makeStepControl(SpreadPlantControl::kBand, 2); }
inline std::unique_ptr<Adapter> makeWalkProbeControl() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kWalk); }
inline std::unique_ptr<Adapter> makeRampProbeControl() { return std::make_unique<SpreadPlantControl>(SpreadPlantControl::kProbe); }
//
// [control makers: B487 the cut latch] (the fault build only, h2_harness_render_faults)
// The engine at each of D9's fault numbers (0: nothing planted), and the probe of what
// each blade is fed, on the engine at each (control_latch.h).
#ifdef H2_ENGINE_FAULTS
template <int F> std::unique_ptr<Adapter> makeLatchPlant() { return std::make_unique<LatchPlantControl>(F); }
template <int F> std::unique_ptr<Adapter> makeLatchProbe() { return std::make_unique<LatchProbeControl>(F); }
#endif
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
  {"control/step-position-b1", true, &makeStepPositionB1},
  {"control/step-position-b2", true, &makeStepPositionB2},
  {"control/step-cut-b1", true, &makeStepCutB1},
  {"control/step-cut-b2", true, &makeStepCutB2},
  {"control/step-rotation-b1", true, &makeStepRotationB1},
  {"control/step-rotation-b2", true, &makeStepRotationB2},
  {"control/step-width-b1", true, &makeStepWidthB1},
  {"control/step-width-b2", true, &makeStepWidthB2},
  {"control/step-depth-b1", true, &makeStepDepthB1},
  {"control/step-depth-b2", true, &makeStepDepthB2},
  {"control/step-index-b1", true, &makeStepIndexB1},
  {"control/step-index-b2", true, &makeStepIndexB2},
  {"control/step-band-b1", true, &makeStepBandB1},
  {"control/step-band-b2", true, &makeStepBandB2},
  {"control/walk-probe", true, &makeWalkProbeControl},
  {"control/ramp-probe", true, &makeRampProbeControl},
  //
  // [control rows: B487 the cut latch] (the fault build only)
#ifdef H2_ENGINE_FAULTS
  {"control/latch-f0", true, &makeLatchPlant<0>},
  {"control/latch-f15", true, &makeLatchPlant<15>},
  {"control/latch-f16", true, &makeLatchPlant<16>},
  {"control/latch-f17", true, &makeLatchPlant<17>},
  {"control/latch-f18", true, &makeLatchPlant<18>},
  {"control/latch-f19", true, &makeLatchPlant<19>},
  {"control/latch-f20", true, &makeLatchPlant<20>},
  {"control/latch-f21", true, &makeLatchPlant<21>},
  {"control/latch-probe", true, &makeLatchProbe<0>},
  {"control/latch-probe-f15", true, &makeLatchProbe<15>},
  {"control/latch-probe-f16", true, &makeLatchProbe<16>},
  {"control/latch-probe-f17", true, &makeLatchProbe<17>},
  {"control/latch-probe-f18", true, &makeLatchProbe<18>},
  {"control/latch-probe-f19", true, &makeLatchProbe<19>},
  {"control/latch-probe-f20", true, &makeLatchProbe<20>},
  {"control/latch-probe-f21", true, &makeLatchProbe<21>},
#endif
  //
};

// Null for an id the table does not have.
inline std::unique_ptr<Adapter> makeAdapter(const char* id) {
  for (const AdapterRow& r : kAdapters) if (std::strcmp(r.id, id) == 0) return r.make();
  return nullptr;
}

}  // namespace horde2::harness
