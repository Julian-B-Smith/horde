#pragma once
// A rack key's value laws (ROADMAP B714; the rack-slot contract's RS-2.9; SPEC-ENGINE MO-28). Pure, header-only, any
// thread, no allocation: the audio thread converts host values with these.
//
// Three forms of one value, and which one each side holds:
// - units: the module's own (Hz, s, dB, an option's number), what setParam takes. The rack stores this, so a value
//   loaded from a chunk reaches the module exactly as written (%.17g), with no round trip through v.
// - v in [0, 1]: the resolver's and the page's form (MO-28), by the key's law.
// - host: the CLAP parameter's value (ADR-412 r2, "as morph rows"): v for a continuous key, the option index
//   0..n-1 for a stepped one, as the engine's stepped rows are (preview_plugin.cpp host_to_v).
// A law outside FOUNDATIONS' schema until it answers horde's brief seq 6: these are horde's interim names (lin, log,
// step, io), read from tools/rack/law/<module>.json.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rack {

enum class Law : std::uint8_t { Lin, Log, Step, Io };

struct KeyLaw {
  Law law = Law::Lin;
  double lo = 0, hi = 1;
  int n = 0;   // >= 2: a stepped key with n options (v in steps of 1/(n - 1)); 0: continuous
};

inline bool stepped(const KeyLaw& k) { return k.n >= 2; }

// io is a gain in dB, lin in its unit: RS-4.6's 48 x (v - 1/2) is lin over [-24, 24].
inline double units_of_v(const KeyLaw& k, double v) {
  v = std::clamp(v, 0.0, 1.0);
  if (stepped(k)) {
    const double index = std::round(v * double(k.n - 1));
    // The step in a statement of its own, so the compiler can't fuse the product into an fma one build and not the
    // next: lo + index x step must give one value everywhere it is computed.
    const double step = (k.hi - k.lo) / double(k.n - 1);
    const double offset = index * step;
    return std::clamp(k.lo + offset, std::min(k.lo, k.hi), std::max(k.lo, k.hi));
  }
  if (k.law == Law::Log) return k.lo * std::pow(k.hi / k.lo, v);   // 20 x pow(1000, x): filter_core.h's cutoff law
  const double offset = v * (k.hi - k.lo);
  return std::clamp(k.lo + offset, std::min(k.lo, k.hi), std::max(k.lo, k.hi));
}

inline double v_of_units(const KeyLaw& k, double u) {
  if (!(k.hi != k.lo)) return 0;
  u = std::clamp(u, std::min(k.lo, k.hi), std::max(k.lo, k.hi));
  if (k.law == Law::Log && !stepped(k)) return std::clamp(std::log(u / k.lo) / std::log(k.hi / k.lo), 0.0, 1.0);
  return std::clamp((u - k.lo) / (k.hi - k.lo), 0.0, 1.0);
}

// The host form's range: [0, n - 1] for a stepped key, [0, 1] otherwise.
inline double host_max(const KeyLaw& k) { return stepped(k) ? double(k.n - 1) : 1.0; }

inline double units_of_host(const KeyLaw& k, double host) {
  if (stepped(k)) return units_of_v(k, std::round(std::clamp(host, 0.0, host_max(k))) / double(k.n - 1));
  return units_of_v(k, host);
}

inline double host_of_units(const KeyLaw& k, double u) {
  const double v = v_of_units(k, u);
  return stepped(k) ? std::round(v * double(k.n - 1)) : v;
}

// A value in units as the module would hold it: inside its range, and on an option of a stepped key.
inline double clamp_units(const KeyLaw& k, double u) {
  u = std::clamp(u, std::min(k.lo, k.hi), std::max(k.lo, k.hi));
  return stepped(k) ? units_of_v(k, v_of_units(k, u)) : u;
}

}  // namespace rack
