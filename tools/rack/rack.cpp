// The rack (ROADMAP B714). See rack.h for the rules; the plants below are the must-fail builds of tools/rack/tests and
// the test plug-in's smoke, each one fault alone.
#include "rack/rack.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "module.h"

namespace rack {

namespace {
constexpr std::uint64_t kRungOut = std::numeric_limits<std::uint64_t>::max() / 2;   // nothing in it: cleared
// 0 dBFS. A clip is a sample past full scale: a definition of the meter, not a tolerance.
constexpr float kFullScale = 1.0f;
#ifdef HORDE_RACK_PLANT_LEVEL_CUT
// The pre-B714 shell's stop for its first module (a tail() under 1e-7: a level, not exact zero), as the
// must-fail control that the idle check tells "quiet" from "exactly silent".
constexpr double kOldLevelStop = 1e-7;
#endif
double gain_of_db(double db) { return std::pow(10.0, db / 20.0); }
}  // namespace

struct Rack::Slot {
  std::unique_ptr<admission::Module> mod;
  bool usable = false;
  int n_keys = 0;
  int handle[kMaxKeys] = {};
  bool is_param[kMaxKeys] = {};          // false: the slot's own I/O gain, never sent to the module
  KeyLaw law[kMaxKeys] = {};
  int n_const = 0;
  int const_handle[kMaxKeys] = {};
  double const_value[kMaxKeys] = {};
  std::atomic<double> value[kMaxKeys];   // units
  // The host value that set it, so the host reads back exactly what it wrote (host -> units -> host needn't round-trip
  // bit for bit); a value set in units (a load) stores its own host form here.
  std::atomic<double> host[kMaxKeys];
  std::atomic<bool> on{false};
  double sent[kMaxKeys] = {};
  bool was_on = false;
  std::atomic<bool> fresh{true};
  std::uint64_t quiet = kRungOut;        // samples of zero input since the input last sounded (whole blocks)
  int io[2] = {-1, -1};                  // supplied gains: key indices, or -1 where the module owns them
  int io_owned[2] = {-1, -1};            // the module's own io keys (the off-keeps-gains plant reads them)
  double g_cur[2] = {1, 1}, g_target[2] = {1, 1}, g_step[2] = {0, 0};
  std::uint32_t g_left[2] = {0, 0};
  std::size_t budget = 0;
  std::atomic<float> in_peak{0}, out_peak{0};
  std::atomic<bool> in_clip{false}, out_clip{false};
  std::atomic<std::uint64_t> nonfinite{0};
  float acc_in = 0, acc_out = 0;
  double last_out = 0;                   // the last block's output peak (the level-cut plant reads it)
  bool acc_in_clip = false, acc_out_clip = false;
};

Rack::Rack() : slot_(std::make_unique<Slot[]>(kMaxSlots)) {
  for (int s = 0; s < kMaxSlots; ++s)
    for (int k = 0; k < kMaxKeys; ++k) {
      slot_[std::size_t(s)].value[k].store(0, std::memory_order_relaxed);
      slot_[std::size_t(s)].host[k].store(0, std::memory_order_relaxed);
    }
}
Rack::~Rack() = default;

void Rack::attach(const SlotTable& table) {
  table_ = &table;
  n_ids_ = 0;
  for (int s = 0; s < slots(); ++s) {
    const SlotRow& row = table.slots[std::size_t(s)];
    Slot& sl = slot_[std::size_t(s)];
    sl.n_keys = int(row.keys.size());
    for (int k = 0; k < sl.n_keys; ++k) {
      sl.law[k] = row.keys[std::size_t(k)].law;
      sl.is_param[k] = !row.keys[std::size_t(k)].slot_io;
      sl.value[k].store(row.keys[std::size_t(k)].def, std::memory_order_relaxed);
      sl.host[k].store(host_of_units(sl.law[k], row.keys[std::size_t(k)].def), std::memory_order_relaxed);
    }
    sl.on.store(false, std::memory_order_relaxed);
    ids_[n_ids_++] = {row.on_id, std::int16_t(s), -1};
    for (int k = 0; k < sl.n_keys; ++k) ids_[n_ids_++] = {row.keys[std::size_t(k)].host_id, std::int16_t(s), std::int16_t(k)};
  }
  std::sort(ids_, ids_ + n_ids_, [](const IdEntry& a, const IdEntry& b) { return a.id < b.id; });
}

bool Rack::prepare(double sample_rate) {
  if (!table_) return false;
  const SlotTable& table = *table_;
  sample_rate_ = sample_rate;
  errors_.clear();
  for (auto& b : buf_)
    if (!b) b = std::make_unique<double[]>(kMaxBlock);
  for (int s = 0; s < slots(); ++s) {
    const SlotRow& row = table.slots[std::size_t(s)];
    Slot& sl = slot_[std::size_t(s)];
    const auto fail = [&](const std::string& why) { errors_.push_back(row.key + ": " + why + "; the slot stays out of the chain"); sl.usable = false; };
    if (!sl.mod && row.make) sl.mod = row.make();
    if (!sl.mod) { fail("its factory made no module"); continue; }
    if (!sl.mod->prepare(sample_rate, kMaxBlock)) { fail("prepare refused the rate"); continue; }
    const foundations::OperatorProperties props = sl.mod->properties();
    if (!props.validate()) { fail("its declared properties do not validate: an undeclared latency, tail or min_block (RS-2.3)"); continue; }
    bool resolved = true;
    sl.n_const = 0;
    for (int k = 0; k < sl.n_keys && resolved; ++k) {
      const KeyRow& key = row.keys[std::size_t(k)];
      if (key.slot_io) continue;
      sl.handle[k] = sl.mod->resolve(key.address);
      if (sl.handle[k] < 0) { fail("its module does not resolve " + key.address); resolved = false; }
    }
    for (const KeyRow& c : row.constants) {
      if (!resolved) break;
      sl.const_handle[sl.n_const] = sl.mod->resolve(c.address);
      sl.const_value[sl.n_const] = c.def;
      if (sl.const_handle[sl.n_const++] < 0) { fail("its module does not resolve " + c.address); resolved = false; }
    }
    if (!resolved) continue;
    sl.io[0] = sl.io[1] = sl.io_owned[0] = sl.io_owned[1] = -1;
    for (int side = 0; side < 2; ++side) {
      const int k = side == 0 ? row.io_in : row.io_out;
      if (k < 0) continue;
      (row.keys[std::size_t(k)].slot_io ? sl.io : sl.io_owned)[side] = k;
    }
    sl.budget = std::min<std::size_t>(row.budget_row, sl.mod->stateBudget());
    sl.usable = true;
    sl.fresh.store(true, std::memory_order_relaxed);
  }
  return errors_.empty();
}

bool Rack::usable(int slot) const { return slot >= 0 && slot < slots() && slot_[std::size_t(slot)].usable; }
std::size_t Rack::effective_budget(int slot) const { return usable(slot) ? slot_[std::size_t(slot)].budget : 0; }

void Rack::set_units(int slot, int key, double u, double host) {
  if (slot < 0 || slot >= slots() || key < 0 || key >= slot_[std::size_t(slot)].n_keys || !std::isfinite(u)) return;
  Slot& s = slot_[std::size_t(slot)];
  const KeyLaw& law = s.law[key];
  const double units = clamp_units(law, u);
  s.value[key].store(units, std::memory_order_relaxed);
  const double h = !std::isfinite(host) ? host_of_units(law, units)
                   : stepped(law)        ? std::round(std::clamp(host, 0.0, host_max(law)))
                                         : std::clamp(host, 0.0, 1.0);
  s.host[key].store(h, std::memory_order_relaxed);
}
double Rack::host(int slot, int key) const {
  if (slot < 0 || slot >= slots() || key < 0 || key >= slot_[std::size_t(slot)].n_keys) return 0;
  return slot_[std::size_t(slot)].host[key].load(std::memory_order_relaxed);
}
double Rack::units(int slot, int key) const {
  if (slot < 0 || slot >= slots() || key < 0 || key >= slot_[std::size_t(slot)].n_keys) return 0;
  return slot_[std::size_t(slot)].value[key].load(std::memory_order_relaxed);
}
void Rack::set_on(int slot, bool on) {
  if (slot >= 0 && slot < slots()) slot_[std::size_t(slot)].on.store(on, std::memory_order_relaxed);
}
bool Rack::on(int slot) const { return slot >= 0 && slot < slots() && slot_[std::size_t(slot)].on.load(std::memory_order_relaxed); }

void Rack::set_dry() {
  for (int s = 0; s < slots(); ++s) {
    set_on(s, false);
    for (int k = 0; k < slot_[std::size_t(s)].n_keys; ++k) set_units(s, k, table_->slots[std::size_t(s)].keys[std::size_t(k)].def);
  }
}

bool Rack::owns(std::uint32_t id) const {
  const IdEntry* e = std::lower_bound(ids_, ids_ + n_ids_, id, [](const IdEntry& a, std::uint32_t i) { return a.id < i; });
  return e != ids_ + n_ids_ && e->id == id;
}
bool Rack::set_host(std::uint32_t id, double host) {
  const IdEntry* e = std::lower_bound(ids_, ids_ + n_ids_, id, [](const IdEntry& a, std::uint32_t i) { return a.id < i; });
  if (e == ids_ + n_ids_ || e->id != id || !std::isfinite(host)) return false;
  if (e->key < 0) {
    set_on(e->slot, std::lround(std::clamp(host, 0.0, 1.0)) != 0);
    return true;
  }
  Slot& s = slot_[std::size_t(e->slot)];
  const KeyLaw& law = s.law[e->key];
  const double h = stepped(law) ? std::round(std::clamp(host, 0.0, host_max(law))) : std::clamp(host, 0.0, 1.0);
  s.value[e->key].store(units_of_host(law, h), std::memory_order_relaxed);
  s.host[e->key].store(h, std::memory_order_relaxed);
  return true;
}
bool Rack::get_host(std::uint32_t id, double& host) const {
  const IdEntry* e = std::lower_bound(ids_, ids_ + n_ids_, id, [](const IdEntry& a, std::uint32_t i) { return a.id < i; });
  if (e == ids_ + n_ids_ || e->id != id) return false;
  host = e->key < 0 ? (on(e->slot) ? 1.0 : 0.0) : slot_[std::size_t(e->slot)].host[e->key].load(std::memory_order_relaxed);
  return true;
}

void Rack::reset() {
  for (int s = 0; s < slots(); ++s) slot_[std::size_t(s)].fresh.store(true, std::memory_order_relaxed);
}

void Rack::send_values(Slot& s, int slot, bool all) {
  (void)slot;
  for (int k = 0; k < s.n_keys; ++k) {
    const double u = s.value[k].load(std::memory_order_relaxed);
    if (!all && u == s.sent[k]) continue;
    s.sent[k] = u;
#ifndef HORDE_RACK_PLANT_NO_VALUES   // must-fail: the host's values never reach the module (it plays its own defaults)
    if (s.is_param[k]) s.mod->setParam(s.handle[k], u);
#endif
  }
  if (all)
    for (int c = 0; c < s.n_const; ++c) s.mod->setParam(s.const_handle[c], s.const_value[c]);
}

bool Rack::process(float* left, float* right, std::uint32_t frames, bool idle) {
  if (!table_) return false;
#ifdef HORDE_RACK_PLANT_ALLOC   // must-fail of the real-time gate (tools/rack_check.py): one allocation on the audio path
  {
    std::vector<double> scratch(frames + 1);
    sample_rate_ += scratch[0];   // used, so the allocation can't be optimised away; adds 0
  }
#endif
#ifdef HORDE_RACK_PLANT_IDLE_CUT   // must-fail: the shell's idle path cuts every slot, tails and all
  if (idle) return false;
#else
  (void)idle;
#endif
  if (meter_reset_.exchange(false, std::memory_order_relaxed))
    for (int s = 0; s < slots(); ++s) {
      Slot& sl = slot_[std::size_t(s)];
      sl.acc_in = sl.acc_out = 0;
      sl.acc_in_clip = sl.acc_out_clip = false;
    }
  const bool never_cut = never_cut_.load(std::memory_order_relaxed);
  bool ran = false;
  for (std::uint32_t done = 0; done < frames;) {
    const std::uint32_t n = std::min(frames - done, kMaxBlock);
    float* l = left + done;
    float* r = right + done;
    double* x[2] = {buf_[0].get(), buf_[1].get()};
    double* y[2] = {buf_[2].get(), buf_[3].get()};
    bool converted = false;   // whether the signal is in x (double), or still in l and r (float)
    for (int s = 0; s < slots(); ++s) {
      Slot& sl = slot_[std::size_t(s)];
      if (!sl.usable) continue;
      if (!sl.on.load(std::memory_order_relaxed)) {
        sl.was_on = false;
#ifdef HORDE_RACK_PLANT_OFF_KEEPS_GAINS   // must-fail (RS-4.8 as it stood): an off slot keeps its I/O gains in the path
        double db = 0;
        for (int side = 0; side < 2; ++side) {
          const int k = sl.io[side] >= 0 ? sl.io[side] : sl.io_owned[side];
          if (k >= 0) db += sl.value[k].load(std::memory_order_relaxed);
        }
        if (db != 0) {
          if (!converted)
            for (std::uint32_t i = 0; i < n; ++i) x[0][i] = l[i], x[1][i] = r[i];
          converted = true;
          const double g = gain_of_db(db);
          for (std::uint32_t i = 0; i < n; ++i) x[0][i] *= g, x[1][i] *= g;
        }
#endif
        continue;   // true bypass: nothing of the slot is in the path, not even a read of the buffers
      }
#ifdef HORDE_RACK_PLANT_DEAF   // must-fail: the slot passes its input and never runs its module
      continue;
#endif
      const bool fresh = sl.fresh.exchange(false, std::memory_order_relaxed);
      if (!sl.was_on || fresh) {
        // Every value, then the reset, which snaps each to its target: the module starts cleared and lands at once.
        send_values(sl, s, true);
        sl.mod->reset();
        sl.quiet = kRungOut;
        for (int side = 0; side < 2; ++side) {   // RS-4.6: a supplied gain opens at its saved value, unglided
          if (sl.io[side] < 0) continue;
          sl.g_cur[side] = sl.g_target[side] = gain_of_db(sl.value[sl.io[side]].load(std::memory_order_relaxed));
          sl.g_left[side] = 0;
        }
        sl.was_on = true;
      } else {
        send_values(sl, s, false);
      }
      bool zeros = true;
      if (converted)
        for (std::uint32_t i = 0; i < n && zeros; ++i) zeros = x[0][i] == 0 && x[1][i] == 0;
      else
        for (std::uint32_t i = 0; i < n && zeros; ++i) zeros = l[i] == 0 && r[i] == 0;
      if (zeros && !never_cut) {
#ifdef HORDE_RACK_PLANT_LEVEL_CUT   // must-fail: the slot stops once its last block was quiet, not when its tail is over
        if (sl.last_out < kOldLevelStop) continue;
#else
        const foundations::OperatorProperties props = sl.mod->properties();   // re-queried: a tail may change
        const std::uint64_t tail = props.tail ? std::uint64_t(props.tail->samples) : 0;
        if (sl.quiet >= tail) continue;   // its tail has passed: its output would be the zeros already there
#endif
      }
      if (!converted) {
        for (std::uint32_t i = 0; i < n; ++i) x[0][i] = l[i], x[1][i] = r[i];
        converted = true;
      }
      float pk = sl.acc_in;
      for (std::uint32_t i = 0; i < n; ++i) pk = std::max({pk, float(std::fabs(x[0][i])), float(std::fabs(x[1][i]))});
      sl.acc_in_clip = sl.acc_in_clip || pk > kFullScale;
      sl.acc_in = pk;
      // The slot's own I/O gains, where the module declares none (RS-4.7): in before the module, out after it.
      for (int side = 0; side < 2; ++side) {
        if (sl.io[side] < 0) continue;
        const double target = gain_of_db(sl.value[sl.io[side]].load(std::memory_order_relaxed));
        if (target != sl.g_target[side]) {
          const double glide = std::max(1.0, std::round(kGainGlideSeconds * sample_rate_));
          sl.g_target[side] = target;
          sl.g_step[side] = (target - sl.g_cur[side]) / glide;
          sl.g_left[side] = std::uint32_t(glide);
        }
      }
      const auto apply_gain = [&](int side, double* const* io) {
        if (sl.io[side] < 0 || (sl.g_left[side] == 0 && sl.g_cur[side] == 1)) return;
        for (std::uint32_t i = 0; i < n; ++i) {
          if (sl.g_left[side]) {
            sl.g_cur[side] = --sl.g_left[side] ? sl.g_cur[side] + sl.g_step[side] : sl.g_target[side];
          }
          io[0][i] *= sl.g_cur[side];
          io[1][i] *= sl.g_cur[side];
        }
      };
      apply_gain(0, x);
      sl.mod->process(x[0], x[1], y[0], y[1], n);
      apply_gain(1, y);
      std::uint64_t bad = 0;
      float po = sl.acc_out;
      double block_peak = 0;
      for (std::uint32_t i = 0; i < n; ++i) {
        for (int c = 0; c < 2; ++c)
          if (!std::isfinite(y[c][i])) { y[c][i] = 0; ++bad; }
        block_peak = std::max({block_peak, std::fabs(y[0][i]), std::fabs(y[1][i])});
      }
      po = std::max(po, float(block_peak));
      sl.last_out = block_peak;
      sl.acc_out_clip = sl.acc_out_clip || po > kFullScale;
      sl.acc_out = po;
      if (bad) sl.nonfinite.fetch_add(bad, std::memory_order_relaxed);
      std::swap(x[0], y[0]);
      std::swap(x[1], y[1]);
      // Whole blocks only: a block with any sound restarts the count, which can only make a tail run longer.
      sl.quiet = zeros ? std::min(sl.quiet, kRungOut - n) + n : 0;
      ran = true;
    }
    if (converted)
      for (std::uint32_t i = 0; i < n; ++i) l[i] = float(x[0][i]), r[i] = float(x[1][i]);
    done += n;
  }
  for (int s = 0; s < slots(); ++s) {
    Slot& sl = slot_[std::size_t(s)];
    sl.in_peak.store(sl.acc_in, std::memory_order_relaxed);
    sl.out_peak.store(sl.acc_out, std::memory_order_relaxed);
    sl.in_clip.store(sl.acc_in_clip, std::memory_order_relaxed);
    sl.out_clip.store(sl.acc_out_clip, std::memory_order_relaxed);
  }
  return ran;
}

SlotMeters Rack::meters(int slot) const {
  SlotMeters m;
  if (slot < 0 || slot >= slots()) return m;
  const Slot& sl = slot_[std::size_t(slot)];
  m.in_peak = sl.in_peak.load(std::memory_order_relaxed);
  m.out_peak = sl.out_peak.load(std::memory_order_relaxed);
  m.in_clip = sl.in_clip.load(std::memory_order_relaxed);
  m.out_clip = sl.out_clip.load(std::memory_order_relaxed);
  m.nonfinite = sl.nonfinite.load(std::memory_order_relaxed);
  return m;
}
void Rack::clear_meters() { meter_reset_.store(true, std::memory_order_relaxed); }

}  // namespace rack
