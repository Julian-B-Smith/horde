// rack_test — the slot library's module-neutral tests, generated from the slot table (ROADMAP B714 acceptance (5),
// (6) and (9); critic findings 1, 3, 4, 8, 20). Built and run by tools/rack_check.py in ./verify full, once clean and
// once per plant (tools/rack/tests/CMakeLists.txt), each plant one fault alone, and each must make this binary fail.
//
// Every check loops over the slots of the registry's table: none names a module, so a module added by data alone
// (tools/rack/modules.json) is tested by the same code. The one exception is the legacy migration, whose input is the
// pre-B714 chunk lines themselves; it runs only for a slot that hosts the module the line was written for.
//
// The input is a seeded burst (mulberry32, seed fixed), so every run reads the same samples, and every comparison is
// exact: bit for bit, or a stated equality of units, but one. The idle cut's residue is held to kIdleResidueBound,
// ADR-413's -140 dB: a module may declare its tail over below it (Weir does), so what the cut drops is bounded, not 0.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "flush_to_zero.h"
#include "module.h"
#include "rack/chunk.h"
#include "rack/descriptor.h"
#include "rack/rack.h"
#include "rack/slot_table.h"

namespace {

int failures = 0;
void check(bool ok, const std::string& what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok) ++failures;
}

constexpr double kRate = 44100;
constexpr std::uint32_t kBlock = 512;
constexpr double kPi = 3.14159265358979323846;
// How long after its input stops a declared tail must have ended for the idle check below to see the cut (Scape's
// default tail is preDelay + 1 + 11 x decay, about 25 s).
constexpr double kIdleTailBoundS = 40;
// What the idle cut may drop of a module's tail: below -140 dB, the level ADR-413 lets a module (Weir's adapter, as the
// pre-B714 shell's rung_out did) declare its tail over at. HORDE-INTERIM, as the adapter's own threshold is.
constexpr double kIdleResidueBound = 1e-7;
// The delay probe's delay, and so its declared tail: past three blocks, so its output is silent for whole blocks after
// its input stops and then sounds (its echo), the case a level-based cut gets wrong.
constexpr std::uint32_t kDelaySamples = 3 * kBlock + 100;

std::uint32_t mulberry32(std::uint32_t& a) {
  std::uint32_t t = a += 0x6D2B79F5u;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}

// Stereo, interleaved: `sound` samples of a seeded burst (noise under two sines), then `silence` samples of zeros.
std::vector<float> burst(std::uint32_t sound, std::uint32_t silence, std::uint32_t seed = 714) {
  std::vector<float> x(2 * std::size_t(sound + silence), 0.0f);
  for (std::uint32_t i = 0; i < sound; ++i) {
    const double n0 = double(mulberry32(seed)) / 4294967296.0 - 0.5, n1 = double(mulberry32(seed)) / 4294967296.0 - 0.5;
    x[2 * i] = float(0.25 * n0 + 0.3 * std::sin(2 * kPi * 220 * i / kRate));
    x[2 * i + 1] = float(0.25 * n1 + 0.3 * std::sin(2 * kPi * 331 * i / kRate));
  }
  return x;
}

// Runs the input through the rack in blocks; a block of zeros in is called as the shell's idle path calls it, unless
// `idle_path` is false (the reference: as if the shell never took its idle path). `ran_last`: whether the last block
// ran any slot's module.
std::vector<float> render(rack::Rack& r, const std::vector<float>& in, bool* ran_last = nullptr, bool idle_path = true, std::uint32_t block = kBlock) {
  const std::uint32_t frames = std::uint32_t(in.size() / 2);
  std::vector<float> out(in.size());
  std::vector<float> l(block), rr(block);
  bool ran = false;
  for (std::uint32_t pos = 0; pos < frames; pos += block) {
    const std::uint32_t n = std::min(block, frames - pos);
    bool zeros = true;
    for (std::uint32_t i = 0; i < n; ++i) {
      l[i] = in[2 * (pos + i)];
      rr[i] = in[2 * (pos + i) + 1];
      zeros = zeros && l[i] == 0 && rr[i] == 0;
    }
    ran = r.process(l.data(), rr.data(), n, idle_path && zeros);
    for (std::uint32_t i = 0; i < n; ++i) out[2 * (pos + i)] = l[i], out[2 * (pos + i) + 1] = rr[i];
  }
  if (ran_last) *ran_last = ran;
  return out;
}

bool nonzero_after(const std::vector<float>& x, std::size_t from) {
  for (std::size_t i = from; i < x.size(); ++i)
    if (x[i] != 0) return true;
  return false;
}

// A table of the registry's modules, leaving `without` out (-1: none).
rack::SlotTable table_without(int without) {
  std::vector<rack::ModuleSource> m;
  for (int i = 0; i < int(rack::registered_modules().size()); ++i)
    if (i != without) m.push_back(rack::registered_modules()[std::size_t(i)]);
  return rack::build_slot_table(m, rack::registered_ledgers());
}

std::unique_ptr<rack::Rack> make_rack(const rack::SlotTable& t) {
  auto r = std::make_unique<rack::Rack>();
  r->attach(t);
  r->prepare(kRate);
  return r;
}

int key_of(const rack::SlotRow& row, const std::string& address) {
  for (std::size_t k = 0; k < row.keys.size(); ++k)
    if (row.keys[k].address == address) return int(k);
  return -1;
}
// The adapter's own probe settings (admission::Module::stressProbe / dryProbe), set on slot s by address.
void set_probe(rack::Rack& r, int s, const admission::Settings& probe) {
  const rack::SlotRow& row = r.table()->slots[std::size_t(s)];
  for (const auto& [address, units] : probe) {
    const int k = key_of(row, address);
    if (k >= 0) r.set_units(s, k, units);
  }
}

// A value unlike the default for every key: v 0.3 by its law (0.7 where the default sits there), option 1 or 0.
double other_units(const rack::KeyRow& k) {
  if (rack::stepped(k.law)) return rack::units_of_host(k.law, rack::host_of_units(k.law, k.def) == 1 ? 0 : 1);
  const double a = rack::units_of_v(k.law, 0.3);
  return a == k.def ? rack::units_of_v(k.law, 0.7) : a;
}

// ---- the table's own rules, on synthetic manifests (no module code at all) ----------------------------------------
rack::ModuleSource synthetic(const std::string& params, const std::string& overlay_keys) {
  rack::ModuleSource m;
  m.key = "synthetic";
  m.name = "Synth";
  m.manifest = "{\"module\":\"synthetic\",\"params\":[" + params + "]}";
  m.overlay = "{\"module\":\"synthetic\",\"keys\":{" + overlay_keys + "}}";
  m.budgets = "{\"modules\":{\"synthetic\":{\"bytes\":4096,\"status\":\"provisional\"}}}";
  m.budget_key = "synthetic";
  return m;
}
std::string synthetic_ledger(int keys) {
  std::string ids = "{\"id\":\"0xF00000\",\"module\":\"synthetic\",\"switch\":true}";
  for (int k = 0; k < keys; ++k) {
    char b[120];
    std::snprintf(b, sizeof b, ",{\"id\":\"0x%06X\",\"module\":\"synthetic\",\"key\":\"s.k%d\"}", 0xF00001 + k, k);
    ids += b;
  }
  for (const char* io : {"io.inGain", "io.outGain"}) ids += std::string(",{\"id\":\"0x") + (io[3] == 'i' ? "F0FFF0" : "F0FFF1") + "\",\"module\":\"synthetic\",\"key\":\"" + io + "\",\"slot_io\":true}";
  return "{\"ids\":[" + ids + "]}";
}
void table_rules() {
  const auto key = [](int k, const char* cls, const char* extra) {
    char b[300];
    std::snprintf(b, sizeof b, "{\"address\":\"s.k%d\",\"class\":\"%s\",\"min\":0,\"max\":1,\"default\":0,\"unit\":\"\"%s}", k, cls, extra);
    return std::string(b);
  };
  const auto law = [](int k, const char* l) { return "\"s.k" + std::to_string(k) + "\":{\"law\":\"" + l + "\",\"mod\":true}"; };
  {
    std::string params, laws;
    for (int k = 0; k <= rack::kMaxKeys; ++k) params += (k ? "," : "") + key(k, "morph", ""), laws += (k ? "," : "") + law(k, "lin");
    const rack::SlotTable t = rack::build_slot_table({synthetic(params, laws)}, {synthetic_ledger(rack::kMaxKeys + 1)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("kMaxKeys") != std::string::npos,
          "table: a module over kMaxKeys (" + std::to_string(rack::kMaxKeys + 1) + " keys plus its slot's io pair) is refused loudly: " + (t.refused.empty() ? "" : t.refused[0]));
  }
  {
    const rack::SlotTable t = rack::build_slot_table({synthetic(key(0, "loose", ""), law(0, "lin"))}, {synthetic_ledger(1)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("no class") != std::string::npos, "table: a key with no class is refused loudly (A1)");
  }
  {
    const rack::SlotTable t = rack::build_slot_table({synthetic(key(0, "flip", ",\"values\":[\"a\",\"b\"]"), law(0, "step"))}, {synthetic_ledger(1)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("switch") != std::string::npos, "table: a flip key with no switch method is refused loudly (A11)");
  }
  {
    const rack::SlotTable t =
        rack::build_slot_table({synthetic(key(0, "flip", ",\"values\":[\"only\"],\"switch\":{\"method\":\"crossfade\"}") + "," + key(1, "morph", ""), law(0, "step") + "," + law(1, "lin"))},
                               {synthetic_ledger(2)});
    check(t.slots.size() == 1 && t.slots[0].constants.size() == 1 && t.slots[0].keys.size() == 3 && t.slots[0].keys[0].address == "s.k1",
          "table: a flip key with one value is a constant, held out of the keys and the host ids (the morph key and the supplied io pair remain)");
  }
  {
    const rack::SlotTable t = rack::build_slot_table({synthetic(key(0, "morph", "") + "," + key(1, "morph", ""), law(0, "lin"))}, {synthetic_ledger(2)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("no law") != std::string::npos, "table: an address with no overlay law is refused loudly");
  }
  {
    const rack::SlotTable t = rack::build_slot_table({synthetic(key(0, "morph", "") + "," + key(1, "morph", ""), law(0, "lin") + "," + law(1, "lin"))}, {synthetic_ledger(1)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("no host id") != std::string::npos, "table: an address with no ledger id is refused loudly");
  }
  {  // kMaxKeys holds the constants too: more one-value flip keys than a slot's fixed array is refused at the table
    std::string params = key(0, "morph", ""), laws = law(0, "lin");
    for (int k = 1; k <= rack::kMaxKeys + 1; ++k)
      params += "," + key(k, "flip", ",\"values\":[\"only\"],\"switch\":{\"method\":\"crossfade\"}"), laws += "," + law(k, "step");
    const rack::SlotTable t = rack::build_slot_table({synthetic(params, laws)}, {synthetic_ledger(1)});
    check(t.slots.empty() && !t.refused.empty() && t.refused[0].find("constant keys") != std::string::npos,
          "table: a module with " + std::to_string(rack::kMaxKeys + 1) + " constants, past kMaxKeys, is refused loudly: " + (t.refused.empty() ? "" : t.refused[0]));
  }
  {  // an alias can rename a stepped key's options, never change how many there are
    std::string ledger = synthetic_ledger(1);
    ledger.insert(ledger.size() - 1, ",\"aliases\":[{\"module\":\"synthetic\",\"key\":\"s.k0\",\"preview\":\"preview.synthetic.k0\",\"form\":\"host\","
                                     "\"name\":\"Synth k0\",\"labels\":[\"a\",\"b\",\"c\"]}]");
    const rack::SlotTable t =
        rack::build_slot_table({synthetic(key(0, "flip", ",\"values\":[\"one\",\"two\"],\"switch\":{\"method\":\"crossfade\"}"), law(0, "step"))}, {ledger});
    check(t.aliases.empty() && !t.refused.empty() && t.refused[0].find("3 labels") != std::string::npos,
          "table: an alias with 3 labels for a key's 2 options is refused loudly: " + (t.refused.empty() ? "" : t.refused[0]));
  }
}

// ---- test modules of this file's own, for the guard, the tail sentinel and a tail lost at run time -----------------
// Each passes its input, one key (t.gain, unused), and does one thing on top: emits a NaN once; or declares
// kInfiniteTail; or stops declaring a tail once `lose` is set; or records whether its input ever held a non-finite; or
// delays its input by kDelaySamples and declares exactly that as its tail.
struct Probe final : admission::Module {
  enum Kind { kNanOnce, kInfinite, kLosesTail, kRecorder, kDelay } kind;
  explicit Probe(Kind k) : kind(k) {}
  bool lose = false, saw_nonfinite = false, emitted = false;
  std::uint64_t calls = 0;
  std::vector<admission::ParamSpec> manifest() const override { return {}; }
  std::string manifestJson() const override { return ""; }
  admission::Declared declared() const override { return {}; }
  bool prepare(double, std::uint32_t) override {
    ring.assign(2 * std::size_t(kDelaySamples), 0.0);
    prepared = true;
    return true;
  }
  void reset() override { std::fill(ring.begin(), ring.end(), 0.0), at = 0; }
  foundations::OperatorProperties properties() const override {
    foundations::OperatorProperties p;
    if (!prepared) return p;
    p.latency = foundations::LatencyDecl{0, 0.0, foundations::ResidualSign::kNone};
    if (!(kind == kLosesTail && lose))
      p.tail = foundations::TailDecl{kind == kInfinite || kind == kLosesTail ? foundations::kInfiniteTail : kind == kDelay ? kDelaySamples : 0u, true};
    p.min_block = 1;
    return p;
  }
  int resolve(std::string_view a) const override { return a == "t.gain" ? 0 : -1; }
  bool setParam(int, double) override { return true; }
  double getParam(int) const override { return 0; }
  void process(const admission::Sample* il, const admission::Sample* ir, admission::Sample* ol, admission::Sample* orr, std::uint32_t n) override {
    ++calls;
    for (std::uint32_t i = 0; i < n; ++i) {
      saw_nonfinite = saw_nonfinite || !std::isfinite(il[i]) || !std::isfinite(ir[i]);
      ol[i] = il[i], orr[i] = ir[i];
      if (kind == kDelay) {
        std::swap(ol[i], ring[2 * at]);
        std::swap(orr[i], ring[2 * at + 1]);
        at = (at + 1) % kDelaySamples;
      }
    }
    if (kind == kNanOnce && !emitted && n > 10) ol[10] = std::numeric_limits<double>::quiet_NaN(), emitted = true;
  }
  std::size_t stateBudget() const override { return 64; }
  std::size_t saveState(char*, std::size_t) const override { return 0; }
  bool stageState(std::string_view) override { return true; }
  bool adoptStaged() override { return true; }
  bool parsePatch(std::string_view, admission::Patch&, std::string*) const override { return true; }
  std::string writePatch(const admission::Patch&) const override { return ""; }
  bool stagePatch(const admission::Patch&) override { return true; }
  double macroValue(const admission::Bind&, double v) const override { return v; }
  admission::Meters meters() const override { return {}; }
  void clearMeters() override {}
  std::uint64_t nonFiniteCount() const override { return 0; }
  bool nonFiniteLatched() const override { return false; }
  void clearNonFiniteLatch() override {}
  bool plantNonFiniteState() override { return false; }
  admission::Settings dryProbe() const override { return {}; }
  bool dryExact() const override { return true; }
  admission::Settings stressProbe() const override { return {}; }
  bool prepared = false;
  std::vector<double> ring;   // the delay's, stereo interleaved
  std::uint32_t at = 0;
};
// A table of Probe modules, one per kind given, slot i named "probe<i>", their instances kept in `made`.
rack::SlotTable probe_table(const std::vector<Probe::Kind>& kinds, std::vector<Probe*>& made) {
  std::vector<rack::ModuleSource> m;
  std::string ids;
  int next = 0xF10000;
  made.assign(kinds.size(), nullptr);
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    const std::string name = "probe" + std::to_string(i);
    rack::ModuleSource s;
    s.key = s.name = name;
    s.manifest = "{\"module\":\"" + name + "\",\"params\":[{\"address\":\"t.gain\",\"class\":\"morph\",\"min\":0,\"max\":1,\"default\":0,\"unit\":\"\"}]}";
    s.overlay = "{\"module\":\"" + name + "\",\"keys\":{\"t.gain\":{\"law\":\"lin\",\"mod\":false}}}";
    s.budgets = "{\"modules\":{\"" + name + "\":{\"bytes\":64,\"status\":\"test\"}}}";
    s.budget_key = name;
    const Probe::Kind k = kinds[i];
    Probe** slot = &made[i];
    s.make = [k, slot] { auto p = std::make_unique<Probe>(k); *slot = p.get(); return std::unique_ptr<admission::Module>(std::move(p)); };
    m.push_back(s);
    char b[400];
    std::snprintf(b, sizeof b, "%s{\"id\":\"0x%06X\",\"module\":\"%s\",\"switch\":true},{\"id\":\"0x%06X\",\"module\":\"%s\",\"key\":\"t.gain\"},"
                  "{\"id\":\"0x%06X\",\"module\":\"%s\",\"key\":\"io.inGain\",\"slot_io\":true},{\"id\":\"0x%06X\",\"module\":\"%s\",\"key\":\"io.outGain\",\"slot_io\":true}",
                  i ? "," : "", next, name.c_str(), next + 1, name.c_str(), next + 2, name.c_str(), next + 3, name.c_str());
    ids += b;
    next += 4;
  }
  return rack::build_slot_table(m, {"{\"ids\":[" + ids + "]}"});
}

void guard_and_tail_rules() {
  {  // RS-7.9: a NaN out of a module is zeroed before the next slot hears it, counted, and the count stays (latched)
    std::vector<Probe*> made;
    const rack::SlotTable t = probe_table({Probe::kNanOnce, Probe::kRecorder}, made);
    auto r = make_rack(t);
    r->set_on(0, true);
    r->set_on(1, true);
    const std::vector<float> y = render(*r, burst(4 * kBlock, 0, 5));
    bool finite = true;
    for (float x : y) finite = finite && std::isfinite(x);
    const std::uint64_t first = r->meters(0).nonfinite;
    render(*r, burst(4 * kBlock, 0, 6));
    const std::uint64_t later = r->meters(0).nonfinite;
    check(t.refused.empty() && made[0] && made[0]->emitted && finite && made[1] && !made[1]->saw_nonfinite,
          "guard: a NaN out of a module never reaches the next slot or the output");
    check(first == 1 && later == 1, "guard: it is counted (" + std::to_string(first) + ") and the count is latched, not cleared by later blocks (" + std::to_string(later) + ")");
    check(rack::slot_meters_json(*r).find("\"nonfinite\":1") != std::string::npos, "guard: the frame's slot meters report it");
  }
  {  // kInfiniteTail is never read as 2^32 - 1 samples
    std::vector<Probe*> made;
    const rack::SlotTable t = probe_table({Probe::kInfinite}, made);
    auto r = make_rack(t);
    r->set_on(0, true);
    render(*r, burst(kBlock, 0, 7));
    r->test_set_quiet(0, 0xFFFFFFFFull - 600);
    const std::uint64_t before = made[0]->calls;
    bool ran = false;
    render(*r, burst(0, 4 * kBlock), &ran);
    check(made[0]->calls == before + 4 && ran, "tail: a slot declaring kInfiniteTail still runs after 2^32 - 600 silent samples (" +
                                                   std::to_string(made[0]->calls - before) + " of 4 blocks)");
  }
  {  // a tail the module stops declaring after prepare is neither read as 0 nor hidden
    std::vector<Probe*> made;
    const rack::SlotTable t = probe_table({Probe::kLosesTail}, made);
    auto r = make_rack(t);
    r->set_on(0, true);
    render(*r, burst(kBlock, 0, 8));
    made[0]->lose = true;
    const std::uint64_t before = made[0]->calls;
    render(*r, burst(0, 2 * kBlock));
    check(made[0]->calls == before + 2 && r->meters(0).tail_lost && rack::slot_meters_json(*r).find("\"tailLost\":true") != std::string::npos,
          "tail: a tail lost at run time does not cut the slot, and is latched and reported");
  }
  {  // the declared tail is what cuts (rack.h, IDLE CUT): a module silent for whole blocks after its input stops and then
     // sounding (a delay's echo) runs exactly the blocks it declares, its echo whole, and not one block more. A rack
     // that cuts early (on the idle path, or on its output's level) loses the echo; one that never cuts runs on.
    std::vector<Probe*> made;
    const rack::SlotTable t = probe_table({Probe::kDelay}, made);
    auto cut = make_rack(t);
    Probe* const pc = made[0];
    auto never = make_rack(t);
    cut->set_on(0, true);
    never->set_on(0, true);
    never->set_never_cut(true);
    const std::vector<float> in = burst(kBlock, 20 * kBlock, 12);
    bool cut_ran = true;
    const std::vector<float> yc = render(*cut, in, &cut_ran), yn = render(*never, in, nullptr, false);
    const std::uint64_t want = 1 + (kDelaySamples + kBlock - 1) / kBlock;
    check(yc == yn && nonzero_after(yc, 2 * std::size_t(kBlock) * 2), "tail: a delay declaring its delay as its tail is cut with its echo whole (bit-identical to the rack that never cuts)");
    check(pc && pc->calls == want && !cut_ran, "tail: it runs exactly the " + std::to_string(want) + " blocks its declared tail covers (" + std::to_string(pc ? pc->calls : 0) +
                                                    "), and is skipped after");
  }
  {  // flush-to-zero: a block a slot runs outside it is counted on the slot and latched; one inside it is not
    std::vector<Probe*> made;
    const rack::SlotTable t = probe_table({Probe::kRecorder}, made);
    auto r = make_rack(t);
    r->set_on(0, true);
    horde2::underFlushToZero([&] { render(*r, burst(4 * kBlock, 0, 13)); });
    const std::uint64_t inside = r->meters(0).unflushed;
    render(*r, burst(2 * kBlock, 0, 14));
    horde2::underFlushToZero([&] { render(*r, burst(kBlock, 0, 15)); });
    const std::uint64_t after = r->meters(0).unflushed;
    check(inside == 0 && after == 2 && rack::slot_meters_json(*r).find("\"unflushed\":2") != std::string::npos,
          "flush-to-zero: blocks run under it count nothing (" + std::to_string(inside) + "); the 2 run outside it are counted, latched and reported (" + std::to_string(after) + ")");
  }
}

}  // namespace

int main() {
  table_rules();
  guard_and_tail_rules();
  const rack::SlotTable& table = rack::registered_table();
  for (const std::string& r : table.refused) std::printf("     table refused: %s\n", r.c_str());
  check(table.refused.empty() && !table.slots.empty(), "the registry's table builds every module it names (" + std::to_string(table.slots.size()) + " slots)");
  auto full = make_rack(table);
  for (const std::string& e : full->errors()) std::printf("     prepare: %s\n", e.c_str());
  check(full->errors().empty(), "every slot prepares, resolves every key and declares valid properties");
  for (int s = 0; s < full->slots(); ++s) {
    const rack::SlotRow& row = table.slots[std::size_t(s)];
    std::printf("     fx/%d %s (%s): %zu keys, %zu constants, owns io %s, budget %zu bytes (row %u, %s)\n", row.n, row.key.c_str(), row.module.c_str(), row.keys.size(),
                row.constants.size(), row.owns_io ? "yes" : "no", full->effective_budget(s), row.budget_row, row.budget_status.c_str());
  }
  const std::string hello = rack::slots_json(*full);
  bool every_key = true;
  for (const auto& row : table.slots) {
    every_key = every_key && hello.find(rack::json_quoted(rack::switch_name(row.n))) != std::string::npos;
    for (const auto& k : row.keys) every_key = every_key && hello.find(rack::json_quoted(rack::key_name(row.n, k.address))) != std::string::npos;
  }
  check(every_key, "the hello descriptor names every slot's switch and every key by MO-28's names");

  const std::vector<float> in = burst(std::uint32_t(kRate / 2), std::uint32_t(kRate / 2));

  // ---- (9) and (5) off: settled off is no module, bit for bit ----
  {
    auto r = make_rack(table);
    check(render(*r, in) == in, "off: every slot off passes the input bit for bit");
  }
  for (int s = 0; s < full->slots(); ++s) {
    const rack::SlotRow& row = table.slots[std::size_t(s)];
    const rack::SlotTable less = table_without(s);
    auto a = make_rack(table), b = make_rack(less);
    for (int o = 0; o < a->slots(); ++o) a->set_on(o, o != s);
    for (int o = 0; o < b->slots(); ++o) b->set_on(o, true);
    // On, for a block, then off again: what is settled off must hold nothing of the time it ran.
    a->set_on(s, true);
    render(*a, burst(kBlock, 0, 9));
    a->reset();
    a->set_on(s, false);
    render(*b, burst(kBlock, 0, 9));
    b->reset();
    const std::vector<float> ya = render(*a, in), yb = render(*b, in);
    std::size_t first = 0;
    while (first < ya.size() && ya[first] == yb[first]) ++first;
    if (first < ya.size()) std::printf("     first difference at sample %zu: %.9g against %.9g\n", first / 2, double(ya[first]), double(yb[first]));
    check(ya == yb, "fx/" + std::to_string(row.n) + " " + row.key + " off, the others on: bit-identical to the same chain built without it");
  }
  // ---- (9) true bypass at +-6 dB of I/O gain, owned or supplied ----
  for (int s = 0; s < full->slots(); ++s) {
    const rack::SlotRow& row = table.slots[std::size_t(s)];
    if (row.io_in < 0 || row.io_out < 0) {
      check(false, row.key + ": no I/O pair at all (RS-4.7: owned or supplied, never neither)");
      continue;
    }
    for (double db : {6.0, -6.0}) {
      auto r = make_rack(table);
      r->set_units(s, row.io_in, db);
      r->set_units(s, row.io_out, db);
      check(render(*r, in) == in, "fx/" + std::to_string(row.n) + " " + row.key + " off with its " + (row.owns_io ? "own" : "supplied") + " I/O gains at " +
                                      (db > 0 ? "+6" : "-6") + " dB: the input bit for bit (an off slot's gains go too)");
    }
  }
  // ---- (5) deaf, and the values reach the module ----
  for (int s = 0; s < full->slots(); ++s) {
    const rack::SlotRow& row = table.slots[std::size_t(s)];
    const auto probe = row.make()->stressProbe();
    auto stress = make_rack(table), defaults = make_rack(table);
    stress->set_on(s, true);
    defaults->set_on(s, true);
    set_probe(*stress, s, probe);
    const std::vector<float> ys = render(*stress, in), yd = render(*defaults, in);
    check(ys != in, "fx/" + std::to_string(row.n) + " " + row.key + " on at its stress settings changes the signal (the slot runs its module)");
    bool finite = true;
    for (float x : ys) finite = finite && std::isfinite(x);
    check(finite, "fx/" + std::to_string(row.n) + " " + row.key + " at its stress settings: every sample finite");
    // The module's output doesn't depend on how the host cuts its blocks (B710's check on the old wrapper, now on the
    // slot): 1, 64, 100 and 4096 frames against 512, at the stress settings, the burst and the half second after it.
    // With the idle cut held off, in both: the cut falls on a block's edge by design (rack.h: whole blocks only), so
    // where a module declares its tail over at a level (ADR-413) the block size moves it; the idle check below bounds
    // what that moves.
    bool split = true;
    std::vector<float> whole;
    for (std::uint32_t b : {512u, 1u, 64u, 100u, 4096u}) {
      auto other = make_rack(table);
      other->set_on(s, true);
      other->set_never_cut(true);
      set_probe(*other, s, probe);
      const std::vector<float> y = render(*other, in, nullptr, true, b);
      if (b == 512u) whole = y;
      else split = split && y == whole;
    }
    check(split, "fx/" + std::to_string(row.n) + " " + row.key + " at its stress settings: bit-identical whether the host's blocks are 1, 64, 100, 512 or 4096 frames");
    check(ys != yd, "fx/" + std::to_string(row.n) + " " + row.key + " at its stress settings differs from its defaults (the values reach the module)");
    const auto dry_module = row.make();
    if (dry_module->dryExact() && !dry_module->dryProbe().empty()) {
      auto dry = make_rack(table);
      dry->set_on(s, true);
      set_probe(*dry, s, dry_module->dryProbe());
      check(render(*dry, in) == in, "fx/" + std::to_string(row.n) + " " + row.key + " on at its exact dry setting passes the input bit for bit");
    }
  }
  // ---- (5) idle cut: one rule, against the same rack that never cuts ----
  // Up to the block where the slot's declared tail ends, the cut render IS the never-cut one, bit for bit; from it on,
  // exact zero; and what the never-cut rack still plays there is under kIdleResidueBound (a module may declare its tail
  // over at -140 dB, ADR-413; one whose tail ends in exact zero drops nothing). At its defaults and at its stress
  // settings; at its defaults, 40 s on, the slot is skipped.
  {
    const std::vector<float> tail_in = burst(std::uint32_t(kRate / 4), std::uint32_t(kIdleTailBoundS * kRate));
    for (int s = 0; s < full->slots(); ++s)
      for (bool stress : {false, true}) {   // at its defaults, and at its stress settings (a resonant filter rings longer)
      const rack::SlotRow& row = table.slots[std::size_t(s)];
      const std::string name = "fx/" + std::to_string(row.n) + " " + row.key + (stress ? " at its stress settings" : " at its defaults");
      auto cut = make_rack(table), never = make_rack(table);
      cut->set_on(s, true);
      never->set_on(s, true);
      never->set_never_cut(true);
      if (stress) {
        set_probe(*cut, s, row.make()->stressProbe());
        set_probe(*never, s, row.make()->stressProbe());
      }
      bool cut_ran = true, never_ran = false;
      const std::vector<float> yc = render(*cut, tail_in, &cut_ran), yn = render(*never, tail_in, &never_ran, false);
      // From the first block that is all zeros: the first the shell's idle path would take.
      const std::size_t after = (std::size_t(kRate / 4) + kBlock - 1) / kBlock * kBlock * 2;
      // Rings: the never-cut rack still plays at or above the bound once the input has stopped.
      double late = 0;
      for (std::size_t i = after; i < yn.size(); ++i) late = std::max(late, std::fabs(double(yn[i])));
      const bool rings = !(late < kIdleResidueBound);
      std::size_t first = 0;
      while (first < yc.size() && yc[first] == yn[first]) ++first;
      double residue = 0;
      for (std::size_t i = first; i < yn.size(); ++i) residue = std::max(residue, std::fabs(double(yn[i])));
      char at[160];
      if (first == yc.size()) std::snprintf(at, sizeof at, "the cut drops nothing (its tail ends in exact zero)");
      else std::snprintf(at, sizeof at, "the cut renders differ from %.3f s, where the never-cut rack plays at most %.1f dB", double(first / 2) / kRate, 20 * std::log10(residue));
      std::printf("     %s: rings above -140 dB after its input stops: %s; %s; still running 40 s later: %s (never-cut: %s)\n", name.c_str(), rings ? "yes" : "no", at,
                  cut_ran ? "yes" : "no", never_ran ? "yes" : "no");
      check(first >= after && !nonzero_after(yc, first), name + " idle: bit-identical to the rack that never cuts it until its input has stopped and its declared tail has ended, exact zero after");
      check(residue < kIdleResidueBound, name + " idle: what the cut drops is under -140 dB (ADR-413)");
      check(!rings || nonzero_after(yc, after), name + " idle: it runs on the idle path while its tail rings above -140 dB");
      // At its defaults only: a stress setting may declare a tail past the render (Scape's longest decay rings minutes).
      if (!stress) check(!cut_ran, name + " idle: 40 s after its input stopped it is skipped (its declared tail has passed)");
      }
  }
  // ---- (5) state round trip, on a non-default fixture (critic finding 20) ----
  {
    auto a = make_rack(table);
    for (int s = 0; s < a->slots(); ++s) {
      a->set_on(s, true);
      for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k) {
        const rack::KeyRow& key = table.slots[std::size_t(s)].keys[k];
        // the I/O pair a step off unity only, so the fixture stays a sound
        a->set_units(s, int(k), key.law.law == rack::Law::Io ? -1.5 : other_units(key));
      }
    }
    std::string text;
    rack::write_fx(rack::capture(*a), text);
    bool every_slot = !text.empty();
    for (const auto& row : table.slots) every_slot = every_slot && text.find("fx.slot " + std::to_string(row.n) + " " + row.module + " ") != std::string::npos;
    check(every_slot, "round trip: the saved section is not empty and holds every slot's record");
    auto b = make_rack(table);
    rack::FxState read;
    std::size_t at = 0;
    while (at < text.size()) {
      const std::size_t end = text.find('\n', at);
      rack::read_fx_line(std::string_view(text).substr(at, end - at), read);
      at = end + 1;
    }
    std::vector<std::string> report;
    rack::apply(*b, read, report);
    for (const auto& line : report) std::printf("     load: %s\n", line.c_str());
    bool same = true;
    for (int s = 0; s < a->slots(); ++s) {
      same = same && a->on(s) == b->on(s);
      for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k) same = same && a->units(s, int(k)) == b->units(s, int(k));
    }
    check(same, "round trip: every switch and every key loads back exactly");
    std::string again;
    rack::write_fx(rack::capture(*b), again);
    check(again == text, "round trip: save, load, save is byte-identical (%.17g)");
    check(render(*a, in) == render(*b, in), "round trip: the loaded rack plays what the saved one played, bit for bit");
  }
  // ---- the corner sweep (B660's, generated from the table): every value of every stepped key, crossed with each
  // continuous key alone at its minimum, middle and maximum (the others at their defaults), and with all of them at
  // their minima and then their maxima. Each module's dry-probe keys are held away from dry (at the end of their range
  // away from the dry value, the top where dry is inside it) unless the swept key is one, so a run hears the module.
  // Under flush-to-zero, as the shell runs the rack, through a long silent tail with subnormal samples in its input.
  // Every run: every sample finite, and the output not the input (bar a dry-probe key swept onto its dry value).
  // No subnormal check: under flush-to-zero the rack's float output cannot hold one by construction (the double to
  // float conversion flushes it), so a count of 0 could never fail. What can fail is the shell calling the rack outside
  // flush-to-zero: the rack's `unflushed` meter, held to 0 through the plug-in by smoke.cpp against its no-ftz plant. ----
  {
    std::vector<float> corner_in(2 * 60 * 2048, 0.0f);
    {
      const std::vector<float> b = burst(4 * 2048, 0, 11);
      std::copy(b.begin(), b.end(), corner_in.begin());
      for (std::size_t i = 2 * 4 * 2048; i < corner_in.size(); i += 97) corner_in[i] = 1e-40f;   // subnormal input, here and there
    }
    std::size_t runs = 0;
    for (int s = 0; s < full->slots(); ++s) {
      const rack::SlotRow& row = table.slots[std::size_t(s)];
      const admission::Settings dry = row.make()->dryProbe();
      std::vector<int> stepped_keys, continuous;
      for (std::size_t k = 0; k < row.keys.size(); ++k) (rack::stepped(row.keys[k].law) ? stepped_keys : continuous).push_back(int(k));
      std::size_t combos = 1;
      for (int k : stepped_keys) combos *= std::size_t(row.keys[std::size_t(k)].law.n);
      // A sweep point: one continuous key (index into `continuous`) at one place, or (-1) all of them at lo or hi.
      struct Point { int key; int place; };   // place 0 lo, 1 mid, 2 hi
      std::vector<Point> points = {{-1, 0}, {-1, 2}};
      for (int c = 0; c < int(continuous.size()); ++c)
        for (int place = 0; place < 3; ++place) points.push_back({c, place});
      std::vector<std::string> bad;
      for (std::size_t c = 0; c < combos; ++c)
        for (const Point& pt : points) {
          auto r = make_rack(table);
          r->set_on(s, true);
          std::size_t rest = c;
          std::string label;
          for (int k : stepped_keys) {
            const rack::KeyLaw& law = row.keys[std::size_t(k)].law;
            r->set_units(s, k, rack::units_of_host(law, double(rest % std::size_t(law.n))));
            label += row.keys[std::size_t(k)].address + "=" + std::to_string(rest % std::size_t(law.n)) + " ";
            rest /= std::size_t(law.n);
          }
          const auto place_of = [](const rack::KeyLaw& law, int place) { return place == 0 ? law.lo : place == 2 ? law.hi : rack::units_of_v(law, 0.5); };
          if (pt.key < 0)
            for (int k : continuous) r->set_units(s, k, place_of(row.keys[std::size_t(k)].law, pt.place));
          bool exempt = false;
          for (const auto& [address, units] : dry) {
            const int k = key_of(row, address);
            if (k < 0 || (pt.key >= 0 && continuous[std::size_t(pt.key)] == k)) continue;
            const rack::KeyLaw& law = row.keys[std::size_t(k)].law;
            r->set_units(s, k, units == law.hi ? law.lo : law.hi);
          }
          if (pt.key >= 0) {
            const int k = continuous[std::size_t(pt.key)];
            const double u = place_of(row.keys[std::size_t(k)].law, pt.place);
            r->set_units(s, k, u);
            for (const auto& [address, units] : dry) exempt = exempt || (key_of(row, address) == k && r->units(s, k) == rack::clamp_units(row.keys[std::size_t(k)].law, units));
            label += row.keys[std::size_t(k)].address + (pt.place == 0 ? " lo" : pt.place == 1 ? " mid" : " hi");
          } else {
            label += pt.place == 0 ? "all lo" : "all hi";
          }
          std::vector<float> y;
          horde2::underFlushToZero([&] { y = render(*r, corner_in, nullptr, true, 2048); });
          bool finite = true;
          for (float v : y) finite = finite && std::isfinite(v);
          if (!finite || (!exempt && y == corner_in)) bad.push_back(label + (finite ? " (the input, untouched)" : " (not finite)"));
          ++runs;
        }
      check(bad.empty(), "fx/" + std::to_string(row.n) + " " + row.key + " corner sweep, " + std::to_string(combos * points.size()) +
                             " runs: every one finite, and none the input untouched" + (bad.empty() ? "" : " (" + std::to_string(bad.size()) + " not; first: " + bad[0] + ")"));
    }
    std::printf("     corner sweep: %zu runs\n", runs);
  }
  // ---- a load while the audio thread runs never passes through a dry state: a slot it keeps on is never restarted,
  // and no key reads its default in between (every key is off its default before the load and in what it loads) ----
  {
    // A second value off the default, unlike other_units's where it can be: a stepped key's other option is its only one.
    const auto alt_units = [](const rack::KeyRow& k) {
      if (rack::stepped(k.law)) return other_units(k);
      const double a = rack::units_of_v(k.law, 0.6);
      return a == k.def ? rack::units_of_v(k.law, 0.4) : a;
    };
    auto src = make_rack(table), r = make_rack(table);
    for (int s = 0; s < r->slots(); ++s) {
      src->set_on(s, true);
      r->set_on(s, true);
      for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k) {
        src->set_units(s, int(k), other_units(table.slots[std::size_t(s)].keys[k]));
        r->set_units(s, int(k), alt_units(table.slots[std::size_t(s)].keys[k]));
      }
    }
    render(*r, in);
    const rack::FxState keep = rack::capture(*src);   // every slot on, every key off its default
    std::vector<std::uint64_t> before;
    for (int s = 0; s < r->slots(); ++s) before.push_back(r->meters(s).starts);
    std::vector<float> l(kBlock), rr(kBlock);
    std::uint32_t seed = 21;
    int blocks = 0;
    std::string at_default;
    std::vector<std::string> report;
    rack::apply(*r, keep, report, [&] {
      for (int s = 0; s < r->slots() && at_default.empty(); ++s)
        for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k)
          if (r->units(s, int(k)) == table.slots[std::size_t(s)].keys[k].def) {
            at_default = "fx/" + std::to_string(table.slots[std::size_t(s)].n) + "/" + table.slots[std::size_t(s)].keys[k].address;
            break;
          }
      for (std::uint32_t i = 0; i < kBlock; ++i) l[i] = float(double(mulberry32(seed)) / 4294967296.0 - 0.5), rr[i] = l[i];
      r->process(l.data(), rr.data(), kBlock, false);
      ++blocks;
    });
    bool kept = true, landed = true;
    for (int s = 0; s < r->slots(); ++s) {
      kept = kept && r->meters(s).starts == before[std::size_t(s)] && r->on(s);
      for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k) landed = landed && r->units(s, int(k)) == src->units(s, int(k));
    }
    check(kept && landed && blocks > 0, "a load that keeps every slot on, with an audio block after every store (" + std::to_string(blocks) +
                                            "), never switches one off or restarts it, and lands every value");
    check(at_default.empty(), "a load passes no key through its default on the way (" + (at_default.empty() ? std::string("none did") : at_default + " did") + ")");
  }
  // ---- (6) the frozen legacy lines migrate (a), and a reader that drops them is caught (d) ----
  {
    struct Line { int version; const char* text; const char* module; const char* address; double units; bool on; };
    // The pre-B714 lines and what each meant, written out here from filter_core.h and reverb_core.h as they were:
    // B660's filter host values (on, mode index, the cutoff's position on 20 x 1000^x, then 0..1), and B710's Scape
    // values in Scape's own units by its manifest id.
    const double cutoff = 20.0 * std::pow(1000.0, 0.45);
    const Line lines[] = {
        {5, "filter 1 3 0.45 0.6 0.25 0.75", "fxfilter", "fxfilter.mode", 3, true},
        {5, "filter 1 3 0.45 0.6 0.25 0.75", "fxfilter", "fxfilter.cutoff", cutoff, true},
        {5, "filter 1 3 0.45 0.6 0.25 0.75", "fxfilter", "fxfilter.resonance", 0.6, true},
        {5, "filter 1 3 0.45 0.6 0.25 0.75", "fxfilter", "fxfilter.morph", 0.25, true},
        {5, "filter 1 3 0.45 0.6 0.25 0.75", "fxfilter", "fxfilter.mix", 0.75, true},
        {6, "scape on 1\nscape 10 6\nscape 0 0.5\nscape 27 -3\nscape 24 1", "scape", "verb.decay", 6, true},
        {6, "scape on 1\nscape 10 6\nscape 0 0.5\nscape 27 -3\nscape 24 1", "scape", "verb.mix", 0.5, true},
        {6, "scape on 1\nscape 10 6\nscape 0 0.5\nscape 27 -3\nscape 24 1", "scape", "io.inGain", -3, true},
        {6, "scape on 1\nscape 10 6\nscape 0 0.5\nscape 27 -3\nscape 24 1", "scape", "verb.era", 1, true},
    };
    int ran = 0;
    for (const Line& l : lines) {
      int slot = -1;
      for (int s = 0; s < full->slots(); ++s)
        if (table.slots[std::size_t(s)].module == l.module) slot = s;
      if (slot < 0) continue;
      ++ran;
      rack::FxState fx;
      const std::string text = l.text;
      std::size_t at = 0;
      bool consumed = true;
      while (at < text.size()) {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        consumed = rack::read_legacy_line(table, l.version, std::string_view(text).substr(at, end - at), fx) && consumed;
        at = end + 1;
      }
      auto r = make_rack(table);
      std::vector<std::string> report;
      rack::apply(*r, fx, report);
      const int k = key_of(table.slots[std::size_t(slot)], l.address);
      const double got = k >= 0 ? r->units(slot, k) : NAN;
      check(consumed && fx.migrated && r->on(slot) == l.on && got == l.units,
            std::string("migration: version ") + std::to_string(l.version) + " " + l.module + " " + l.address + " reads back " + std::to_string(got) + ", exactly the old line's value");
    }
    check(ran > 0, "migration: at least one legacy line had a slot to land in");
  }
  std::printf("rack_test: %s (%d failed)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
