#pragma once
// The rack (ROADMAP B714; SPEC-ENGINE RC-5; the rack-slot contract's RS-3 to RS-4): every slot of a SlotTable as a
// live module on one serial chain, in -> fx/1 -> fx/2 -> ... -> out, in the table's order. One instance per shell
// instance. S3 replaces the fixed chain with routing_core.h's crosspoint; nothing here assumes a module.
//
// What it does, and the rule behind each:
// - TRUE BYPASS (RC-5.2, ADR-411 r2). A slot whose switch is off is not touched at all: not its module, not its I/O
//   gains, not even a read of the buffers. Settled off is bit-identical to no module.
// - I/O GAINS (RS-4.6, RS-4.7). A module that declares io.inGain and io.outGain owns them (they are its parameters);
//   for any other the slot applies an identical pair, +-24 dB, gliding linearly over kGainGlideBoundS and opening at
//   the saved gain. Never both.
// - SWITCH ON. A slot turned on sends every value, then resets its module, so it starts cleared with every value
//   landed (nothing stale rings out, nothing ramps in). S5 replaces this with MO-29's fade.
// - IDLE CUT (critic finding 4, one rule for every module). An on slot runs while the time since its input last held a
//   non-zero sample is under its DECLARED tail, re-queried every block (a tail may change, TailDecl::may_change).
//   After it, the slot is skipped and passes its input, which is zeros, so its output is exact zero; a module whose
//   tail is honest loses nothing. The rule is the same on the shell's idle path and its sounding path.
// - VALUES. Every key is stored in its module's units (law.h). The audio thread sends a key to setParam only when it
//   changed, once a block; the module's own smoothing is the only smoothing (RS-4.2).
// - GUARD (RS-7.9, Blind-Spot Armor: latch and report, never clamp silently). A non-finite sample out of a module is
//   counted, latched on its slot and replaced by 0 before the next slot hears it; the counts are readable.
// - FLUSH-TO-ZERO (B660's subnormal rule, now the caller's). process() must run under horde2::underFlushToZero, as the
//   engine's render does: a module's long silent tail otherwise decays through subnormals. The rack can't see a
//   subnormal its modules hold, so it checks the cause instead: each block a slot runs its module with the FP control
//   word's flush bits off is counted and latched on the slot (SlotMeters::unflushed), and the test plug-in's smoke holds
//   the count at 0 against a build that calls the rack outside the scope.
// Real time: process() allocates nothing, takes no lock and makes no call into the system; everything it touches is
// sized at prepare(), which runs on the main thread (a module may allocate there: tens of MiB, for a reverb).
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "rack/slot_table.h"

namespace admission { class Module; }

namespace rack {

constexpr std::uint32_t kMaxBlock = 4096;          // the engine's (DT-2): process() runs longer blocks in pieces
constexpr double kGainGlideBoundS = 0.020;         // RS-4.6's slot gain ramp, in seconds (ADR-009)

struct SlotMeters {
  float in_peak = 0, out_peak = 0;
  bool in_clip = false, out_clip = false;
  std::uint64_t nonfinite = 0;   // samples zeroed since the slot was prepared: the latch is "not zero"
  bool tail_lost = false;        // properties() stopped declaring a tail after prepare (latched): the slot was not cut
  std::uint64_t starts = 0;      // times the slot started cleared
  std::uint64_t unflushed = 0;   // blocks its module ran without flush-to-zero in force (latched): the caller's fault
};

class Rack {
 public:
  Rack();
  ~Rack();
  Rack(const Rack&) = delete;
  Rack& operator=(const Rack&) = delete;

  // Main thread, once, before anything else (a shell's init): the table, every key at its default, every switch off.
  // From here the values and the host ids work; no module exists yet. Allocates nothing on the audio side.
  void attach(const SlotTable& table);
  // Main thread, at activation: makes each slot's module (once per Rack), prepares it at the rate, resolves every key
  // and checks its declared properties (RS-2.3: an undeclared latency, tail or min_block is refused, never read as
  // 0). A slot that fails is unusable and stays out of the chain; errors() says why. Allocates. Values survive.
  bool prepare(double sample_rate);
  const std::vector<std::string>& errors() const { return errors_; }
  const SlotTable* table() const { return table_; }
  int slots() const { return table_ ? int(table_->slots.size()) : 0; }
  bool usable(int slot) const;
  // RS-6.12's figure per corner: the table's row, or the module's own declared budget where that is smaller.
  std::size_t effective_budget(int slot) const;

  // Values, any thread (atomics). Keys by the slot row's index; units are the module's, clamped to its range. `host`,
  // where given and finite, is the host value that names those units (a reloaded key keeps the host value it was set
  // by); otherwise it is derived from the units.
  void set_units(int slot, int key, double units, double host = std::numeric_limits<double>::quiet_NaN());
  double units(int slot, int key) const;
  double host(int slot, int key) const;   // the key's host value: v, or a stepped key's option index
  void set_on(int slot, bool on);
  bool on(int slot) const;
  // Every switch off and every key at its default: what a patch with no `fx` loads (MO-31's dry state).
  void set_dry();

  // By host id (the ledger's), audio-thread safe: a binary search over a fixed array. False: not a rack id, or (set)
  // a value that is not finite.
  bool owns(std::uint32_t id) const;
  bool set_host(std::uint32_t id, double host);
  bool get_host(std::uint32_t id, double& host) const;

  // Audio thread. reset(): the host's reset; each slot that is on starts cleared at its next block.
  void reset();
  // In place, on the engine's float output. True when any slot ran its module, so the caller guards and meters the
  // block even on its idle path. `idle`: the shell's idle path called it (zeros in); the rule is the same there.
  bool process(float* left, float* right, std::uint32_t frames, bool idle);

  // Meters, read on the main thread; clear_meters() asks the audio thread to start them again.
  SlotMeters meters(int slot) const;
  void clear_meters();

  // Tests only: never skip a slot for its tail, so a render can be held against one that does.
  void set_never_cut(bool on) { never_cut_.store(on, std::memory_order_relaxed); }
  // Tests only, between process calls: the slot's count of silent samples since its input last sounded.
  void test_set_quiet(int slot, std::uint64_t samples);

 private:
  struct Slot;
  void send_values(Slot& s, int slot, bool all);
  const SlotTable* table_ = nullptr;
  std::vector<std::string> errors_;
  std::unique_ptr<Slot[]> slot_;
  struct IdEntry { std::uint32_t id; std::int16_t slot, key; };
  IdEntry ids_[kMaxSlots * (kMaxKeys + 1)] = {};
  int n_ids_ = 0;
  std::unique_ptr<double[]> buf_[4];
  double sample_rate_ = 44100;
  std::atomic<bool> never_cut_{false};
  std::atomic<bool> meter_reset_{false};
};

}  // namespace rack
