// rack_test — the slot library's module-neutral tests, generated from the slot table (ROADMAP B714 acceptance (5),
// (6) and (9); critic findings 1, 3, 4, 8, 20). Built and run by tools/rack_check.py in ./verify full, once clean and
// once per plant (tools/rack/tests/CMakeLists.txt), each plant one fault alone, and each must make this binary fail.
//
// Every check loops over the slots of the registry's table: none names a module, so a module added by data alone
// (tools/rack/modules.json) is tested by the same code. The one exception is the legacy migration, whose input is the
// pre-B714 chunk lines themselves; it runs only for a slot that hosts the module the line was written for.
//
// The input is a seeded burst (mulberry32, seed fixed), so every run reads the same samples, and every comparison is
// exact: bit for bit, or a stated equality of units. There is no tolerance in this file.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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
}

}  // namespace

int main() {
  table_rules();
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
    // The output doesn't depend on how the host cuts its blocks (B710's check on the old wrapper, now on the slot): 1,
    // 64, 100 and 4096 frames against 512, at the stress settings, the burst and the half second after it.
    bool split = true;
    for (std::uint32_t b : {1u, 64u, 100u, 4096u}) {
      auto other = make_rack(table);
      other->set_on(s, true);
      set_probe(*other, s, probe);
      split = split && render(*other, in, nullptr, true, b) == ys;
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
  {
    const std::vector<float> tail_in = burst(std::uint32_t(kRate / 4), std::uint32_t(40 * kRate));
    for (int s = 0; s < full->slots(); ++s) {
      const rack::SlotRow& row = table.slots[std::size_t(s)];
      auto cut = make_rack(table), never = make_rack(table);
      cut->set_on(s, true);
      never->set_on(s, true);
      never->set_never_cut(true);
      bool cut_ran = true, never_ran = false;
      const std::vector<float> yc = render(*cut, tail_in, &cut_ran), yn = render(*never, tail_in, &never_ran, false);
      // From the first block that is all zeros: the first the shell's idle path would take.
      const std::size_t after = (std::size_t(kRate / 4) + kBlock - 1) / kBlock * kBlock * 2;
      const bool rings = nonzero_after(yn, after);
      std::printf("     fx/%d %s: rings after its input stops: %s; still running 40 s later: %s (never-cut: %s)\n", row.n, row.key.c_str(), rings ? "yes" : "no",
                  cut_ran ? "yes" : "no", never_ran ? "yes" : "no");
      check(yc == yn, "fx/" + std::to_string(row.n) + " " + row.key + " idle: bit-identical to the rack that never cuts it and never takes the idle path (its tail ends in exact zero; nothing audible is cut)");
      check(!rings || nonzero_after(yc, after), "fx/" + std::to_string(row.n) + " " + row.key + " idle: it runs on the idle path while its tail rings");
      check(!cut_ran, "fx/" + std::to_string(row.n) + " " + row.key + " idle: 40 s after its input stopped it is skipped (its declared tail has passed)");
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
