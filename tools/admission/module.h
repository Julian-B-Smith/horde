// module.h — the hosted-module ABI as horde's rack slot will drive it, for the admission test
// (ROADMAP B561, ADR-295; the test is specified in docs/proposals/rack-slot-contract.md §2.2).
//
// WHY A NEUTRAL INTERFACE. The admission rows are written ONCE, against this class, and each
// module gets a thin adapter over its own shim (contract §1.1: Bulwark's abi.h, Shriek's
// mawcore abi.h, Scape's fx_module.h). Every divergence between a shim and FOUNDATIONS'
// FX-operator draft lives in that module's adapter, never in the rows, so when a shim is swapped
// for the FOUNDATIONS header the change is a rename inside one adapter (ADR-295). Only Scape's
// adapter exists today (scape_adapter.cpp); a Bulwark or Shriek adapter is one more file of the
// same shape plus a line in the driver.
//
// WHY VIRTUAL. The must-fail controls (plants.h) are wrappers that forward to a real module and
// inject one fault each. A virtual base makes every plant module-neutral: the same planted
// "reset that leaves a smoother unsnapped" wraps Scape today and Bulwark tomorrow.
//
// The types follow FOUNDATIONS where FOUNDATIONS has a type (OperatorProperties, its validate(),
// FxSample = double), and the contract's own vocabulary where it does not yet (the parameter
// classes of §4.1, the switch methods of §12.1 / §12.2).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "foundations/fx_operator.h"

namespace admission {

using Sample = foundations::FxSample;

// The three classes of contract §4.1. None = the module declared no class (row A1 refuses it).
enum class Cls { None, Morph, Flip, Device };

// How a flip-class key switches (contract §12.1 as amended by §12.2). The first three are the
// methods the structural pass rule admits. Silent: the key changes no signal-path or detector
// state when it flips (a mode that shapes only a LATER move); the row admits it only after a
// render proves the flip bit-identical to no flip. Dip and Jump are named so a row can refuse
// them by name: Dip is a module-internal duck / swap / un-duck (not an equal-gain crossfade, and
// not the slot's `gate`), Jump is a bare switch of a signal-path state.
enum class Switch { Undeclared, EqualGainCrossfade, Gate, SeededJump, Silent, Dip, Jump };

struct ParamSpec {
  std::string address;
  std::string unit;        // "" is a declared unit (a ratio or plain number); see unitDeclared
  bool unitDeclared = false;
  Cls cls = Cls::None;
  bool rateDeclared = false;   // RS-2.1's rate class (continuous / latched)
  double min = 0, max = 0, def = 0;
  double transitionS = 0;      // a morph's smoothing or a flip's switch length, seconds
  int steps = 0;               // 0 = continuous; else the number of enumerated values
  Switch sw = Switch::Undeclared;   // flip keys only
  std::string switchSource;    // where the adapter read `sw` from (a manifest field, or prose)
};

struct Bind {
  std::string address;
  double lo = 0, hi = 1;
  std::string curve;   // lin / exp / log
  friend bool operator==(const Bind&, const Bind&) = default;
};
struct Macro {
  std::string label;
  double home = 0.5;
  std::vector<Bind> bind;
  friend bool operator==(const Macro&, const Macro&) = default;
};
// One module patch (a preset, or one morph corner): its values by address and its macros.
struct Patch {
  std::string name;
  std::vector<std::pair<std::string, double>> values;
  std::vector<Macro> macros;
  friend bool operator==(const Patch&, const Patch&) = default;
};

struct Meters { double inPeak = 0, outPeak = 0; bool inClip = false, outClip = false; };
// RS-2.4 and RS-4.10: what the module declares about itself beyond its parameters.
struct Declared { bool changesImage = false, changesLevel = false, morphPosition = false; };
using Settings = std::vector<std::pair<std::string, double>>;

class Module {
 public:
  virtual ~Module() = default;

  // ---- declarations (row A1)
  virtual std::vector<ParamSpec> manifest() const = 0;
  virtual std::string manifestJson() const = 0;   // the module's own manifest text, as it ships
  virtual Declared declared() const = 0;

  // ---- lifecycle (FOUNDATIONS FxOperatorLifecycle)
  virtual bool prepare(double sampleRate, std::uint32_t maxBlock) = 0;
  virtual void reset() = 0;
  virtual foundations::OperatorProperties properties() const = 0;

  // ---- parameters (FOUNDATIONS FxOperatorParams; a handle is an int here, -1 = none)
  virtual int resolve(std::string_view address) const = 0;
  virtual bool setParam(int handle, double value) = 0;
  virtual double getParam(int handle) const = 0;   // the target

  // ---- audio: stereo, planar, double; any n from min_block to max_block
  virtual void process(const Sample* inL, const Sample* inR, Sample* outL, Sample* outR, std::uint32_t n) = 0;

  // ---- state (RS-6.10..6.13): saved text, a two-step staged load (RS-6.6)
  virtual std::size_t stateBudget() const = 0;
  virtual std::size_t saveState(char* out, std::size_t cap) const = 0;
  virtual bool stageState(std::string_view text) = 0;
  virtual bool adoptStaged() = 0;

  // ---- patches and macros (§6.2, RS-5.3)
  virtual bool parsePatch(std::string_view text, Patch& out, std::string* error) const = 0;
  virtual std::string writePatch(const Patch& p) const = 0;
  virtual bool stagePatch(const Patch& p) = 0;   // then adoptStaged()
  virtual double macroValue(const Bind& b, double v) const = 0;

  // ---- meters and the non-finite guard (RS-4.7, RS-7.9)
  virtual Meters meters() const = 0;
  virtual void clearMeters() = 0;
  virtual std::uint64_t nonFiniteCount() const = 0;
  virtual bool nonFiniteLatched() const = 0;
  virtual void clearNonFiniteLatch() = 0;

  // ---- adapter knowledge (the per-module part of a row's inputs)
  // Plant one non-finite value inside the module's recursive state (row A9). False when the
  // module offers no seam for it: the row then reports that half NOT RUN.
  virtual bool plantNonFiniteState() = 0;
  // A setting at which the module passes its input dry (out = in x the I/O gains), used by rows
  // A4 and A6 to read gain and delay sample-exactly. Empty when the module has none.
  virtual Settings dryProbe() const = 0;
  // A setting that engages the module's whole signal path (row A11's sweep, row A2's stress case).
  virtual Settings stressProbe() const = 0;
};

// What the driver knows about one module: how to make an instance and where its factory
// presets are.
struct ModuleInfo {
  std::string name;
  std::function<std::unique_ptr<Module>()> make;
  std::vector<std::pair<std::string, std::string>> presets;   // (file name, text), sorted
};

}  // namespace admission
