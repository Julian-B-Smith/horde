// Not run by ./verify: it needs the clean-room engine, which isn't in this public repo or in CI yet (B568). B642.
//
// The minimal reproduction of the morph's one silent drop-out class (B642): under Quantum each source's on switch picks
// its corner by MO-7 on its own (MO-26 (3)), so where two corners each sound a different source, the pad has regions
// where every switch picks a corner in which its source is off, and a held note goes silent. No audio is rendered: the
// clean room's own resolver is driven along a straight line of the pad, one control interval per step, and every
// position where the switches it has sent leave no source on is reported.
//   preview-morph-hole            sweeps the A-B edge with A = osc 1 only, B = osc 2 only (C = A, D = B), at
//                                 cohesions 0, 0.3 (D.2's default), 0.7 and 1, seeds 1024 (D.2's default) to 1033
// The control that must fail: under Blend the same sweep finds no hole (a source's level fades, MO-26 (1), and the
// two levels sum to 1 on the edge); and the sweep must see each switch flip at least once (a resolver that sends
// nothing would read as "no hole").
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "horde_engine.h"

namespace {

struct Sweep { int holes = 0; double width = 0; double first = -1; int flips = 0; };

// Walks the pad from (0, 0) to (1, 0) and back over `steps` positions each way, one position per control interval.
Sweep sweep(horde::MorphLaw law, double cohesion, int seed, int steps) {
  auto e = horde::make_engine();
  auto res = horde::make_morph_resolver();
  if (!e || !res || !e->prepare(44100, 512) || !res->prepare(44100)) return {};
  const horde::ParamInfo* sw[4] = {horde::find_param("osc1.on"), horde::find_param("osc2.on"), horde::find_param("sub.on"), horde::find_param("noise.on")};
  std::vector<double> a(horde::manifest_id_span(), 0.0);
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) a[horde::manifest_param(i).id] = e->param_v(horde::manifest_param(i).id);
  std::vector<double> b = a;
  a[sw[0]->id] = 1, a[sw[1]->id] = 0, a[sw[2]->id] = 0, a[sw[3]->id] = 0;   // A and C: osc 1 only
  b[sw[0]->id] = 0, b[sw[1]->id] = 1, b[sw[2]->id] = 0, b[sw[3]->id] = 0;   // B and D: osc 2 only
  const std::string t0 = e->ratios(0), t1 = e->ratios(1);
  res->set_corner(0, a.data(), t0.c_str(), t1.c_str(), 0);
  res->set_corner(1, b.data(), t0.c_str(), t1.c_str(), 0);
  res->set_corner(2, a.data(), t0.c_str(), t1.c_str(), 0);
  res->set_corner(3, b.data(), t0.c_str(), t1.c_str(), 0);
  horde::MorphSettings s;
  s.on = true;
  s.law = law;
  s.cohesion = cohesion;
  s.seed = seed;
  s.glide = 0;
  res->set_settings(s, 0);
  res->set_position(0, 0, 0);
  const std::uint32_t ci = 29;   // PA-3 at 44.1 kHz
  std::vector<horde::MorphEvent> out(4096);
  bool on[4] = {true, false, false, false};   // what the engine holds before the morph speaks: corner A's
  double level[4] = {1, 1, 1, 1};
  Sweep r;
  bool in_hole = false;
  double hole_from = 0;
  for (int k = 0; k <= 2 * steps; ++k) {
    const double x = k <= steps ? double(k) / steps : double(2 * steps - k) / steps;
    res->set_position(x, 0, res->clock());
    // Two intervals per position: the position applies at the next boundary, and a switch going off under Blend waits
    // for its mirrored level (MO-26 (1)); what counts is what the engine would hold after them.
    for (int rep = 0; rep < 2; ++rep) {
      const std::size_t n = res->process(ci, out.data(), out.size());
      for (std::size_t i = 0; i < n; ++i) {
        const horde::MorphEvent& m = out[i];
        for (int j = 0; j < 4; ++j) {
          if (m.kind == horde::MorphEvent::Kind::Param && m.param_id == sw[j]->id) {
            const bool now = m.value > 0.5;   // a switch's v is 0 or 1
            r.flips += now != on[j];
            on[j] = now;
          }
          if (m.kind == horde::MorphEvent::Kind::Level && m.source == j) level[j] = m.value;
        }
      }
    }
    // Silent: no source on, or (under Blend) every on source's level below MO-26's floor.
    bool sounding = false;
    for (int j = 0; j < 4; ++j) sounding = sounding || (on[j] && level[j] > 1e-3);   // MO-26's floor, about -60 dB
    if (!sounding && !in_hole) { in_hole = true; hole_from = x; ++r.holes; if (r.first < 0) r.first = x; }
    if (sounding && in_hole) { in_hole = false; r.width += std::fabs(x - hole_from); }
  }
  return r;
}

}  // namespace

int main() {
  int failures = 0;
  const int steps = 2000;   // 0.0005 of the pad per position
  std::printf("Quantum, A = osc 1 only, B = osc 2 only (C = A, D = B), the pad swept along A-B and back:\n");
  int holes_at_default = 0, seeds_with_hole = 0;
  for (double h : {0.0, 0.3, 0.7, 1.0}) {
    for (int seed = 1024; seed < 1034; ++seed) {
      const Sweep q = sweep(horde::MorphLaw::Quantum, h, seed, steps);
      if (h == 0.3 && seed == 1024) holes_at_default = q.holes;
      if (h == 0.3) seeds_with_hole += q.holes > 0;
      std::printf("  cohesion %.1f seed %d: %d silent stretch(es), %.3f of the pad in all, first at x = %.4f (%d switch flips)\n", h, seed, q.holes, q.width,
                  q.first, q.flips);
      if (q.flips == 0) { std::printf("FAIL the sweep saw no switch flip: the resolver said nothing\n"); ++failures; }
    }
  }
  std::printf("D.2's defaults (cohesion 0.3, seed 1024): %d silent stretch(es); seeds 1024-1033 with a hole at cohesion 0.3: %d of 10\n", holes_at_default,
              seeds_with_hole);
  const Sweep blend = sweep(horde::MorphLaw::Blend, 0.3, 1024, steps);
  std::printf("control, Blend at D.2's defaults: %d silent stretch(es) (%d switch flips)\n", blend.holes, blend.flips);
  if (blend.holes != 0) { std::printf("FAIL Blend went silent: the fade (MO-26 (1)) should keep the edge sounding\n"); ++failures; }
  return failures ? 1 : 0;
}
