/*
 * subosc_check — the SUB OSC port's oracle (B155 port phase 1).
 *
 * WHAT IT GATES
 *  1. L0-1 parity: `hypersaw::SubOscCore` reproduces reference/subosc.html's
 *     `SubOscCore` to <= 1e-6 RMS on every scenario, at BOTH 48 000 and
 *     44 100 Hz. Goldens from tools/golden/gen_subosc_goldens.mjs, which slices
 *     the lab live. There is no monitoring stage to invert here (see that
 *     file's header), so parity is measured at full engine amplitude.
 *  2. SPEC-SUBOSC §10's MEASURED rows, re-measured against the C++ core with
 *     their must-fail controls: determinism (§10.2), sample-rate independence
 *     (§10.3), block-size independence (§10.4), exact silence and a
 *     subnormal-free tail (§10.5), the bump's two-lobe silhouette and its
 *     peak-exact normaliser (§10.6), plus §5.1's rate-warp finding (audit A7)
 *     and §7's clamped-table contract (audit A2).
 *  3. A CPU number. REPORTED, never gated: the absolute ratio is
 *     machine-dependent and SPEC-SUBOSC §10.7 claims no budget ("CPU per voice
 *     — still to measure before a port"). This is that measurement.
 *
 * WIRED in `./verify full` (ADR-180 §1: a check is wired in the PR that creates
 * it), beside station's chain: generator --selfcheck, generator, then this
 * binary with the golden dir as argv[1].
 *
 * EVERY ROW CARRIES A CONTROL that must read the other answer (L0016/L0032).
 * Where the lab harness plants a defective build to get its must-fail control
 * (tools/labharness/subosc_check.mjs, also wired in `verify full`), C++ cannot;
 * the equivalent here is a row that asserts BOTH directions — the quantity
 * under test AND a deliberately-wrong hypothesis the same detector must reject,
 * evaluated through the core's own public surface (`flushFloor` is the one test
 * hook, the StationCore::pmConst idiom).
 *
 * B172 PHASE 2 ADDED A SECOND LAYER: THE SHELL (section 11 below). Sections
 * 1-10 drive `SubOscCore` DIRECTLY, which is the right instrument for parity
 * and for §10's laws — and structurally blind to everything BETWEEN the core
 * and the output (L0031 (B): a reference oracle covers only the surface it
 * spans). The shell rows drive the real plugin through the CLAP factory, the
 * `slotcontract_check` / `measure_cpu` idiom, and gate the claims the port's
 * phase 2 actually makes: the block's gate is bit-inert while off, the row
 * reproduces the core through the matrix at the same eps, the chunk carries
 * every new id, the headroom bound holds as a measurement, and hard sync's
 * REFUSAL is pinned so it cannot become an accidental presence.
 *
 * THE ONE §10 ROW THAT IS NOT HERE, AND WHY. §10.1's aliasing floors are a
 * 65 536-point Kaiser-windowed FFT sweep for the worst inharmonic bin across
 * six shapes and four notes. No FFT harness exists on the C++ side, and the
 * property is a fact about the SHAPE LAWS, which parity pins bit-for-bit — an
 * alias regression in this port cannot happen without a parity failure first,
 * and the floors themselves are gated at the lab by subosc_check.mjs, which
 * `./verify full` now runs. What IS here instead is a narrow, Goertzel-based
 * confirmation that the polyBLEP correction is present in THIS build, with a
 * naive saw as its control: comparable to §10.1's numbers, not identical with
 * them (the same boundary station_check records for its DRW alias row).
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"
#include "../src/subosc_core.h"

using hypersaw::SubOscCore;

namespace
{
int g_failures = 0;

/* ---- THE SHELL RIG (section 11) -----------------------------------------
   The real plugin through the real factory, driven with real parameter and
   note events — the slotcontract_check idiom. Nothing here pokes state: a
   value a host could not reach must not reach the DSP here either. */
const void *shHostExt(const clap_host_t *, const char *) { return nullptr; }
void shHostNoop(const clap_host_t *) {}
const clap_host_t kShHost = {CLAP_VERSION, nullptr, "subosc_check", "-", "-", "1.0",
                             shHostExt, shHostNoop, shHostNoop, shHostNoop};
bool shPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kShOut = {nullptr, shPush};

// ADR-088 SUB OSC block, restated here as a BOUND rather than included: this
// tool links the plugin entry, not the shell's internals. Membership still
// comes from the host's own id list (every row below asks findParam via the
// params extension), so this is a coordinate, not a second decoder.
constexpr clap_id kSubIdBase = 4000;
constexpr clap_id kSubOnId = 4015;
/* The block's ids: 4000..4014 are SubOscCore::Param positionally, 4015 is the
   gate, and 4016..4019 are B181's SHELL rows (mono, bias, glide, pitchMod) —
   voice assignment and glide are the shell's job for the swarm too, so the
   core stays a single-voice renderer. Spelled out rather than imported
   because this file drives the plugin through the CLAP factory and must not
   read the shell's internals to state what the shell should expose. */
constexpr clap_id kSubMonoId = 4016, kSubBiasId = 4017, kSubGlideId = 4018,
                  kSubPitchModId = 4019;
constexpr int kSubIdCount = 20;
constexpr clap_id kOscEnable0 = 150, kOscEnable1 = 1150;

struct ShellRig
{
  const clap_plugin_t *p = nullptr;
  const clap_plugin_params_t *params = nullptr;
  std::vector<float> L, R;
  float *ch[2];
  clap_audio_buffer_t out{};

  void boot(double sr, uint32_t block)
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kShHost, "com.lifted-truck.hypersaw");
    p->init(p);
    params = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    p->activate(p, sr, block, block);
    p->start_processing(p);
    L.assign(block, 0);
    R.assign(block, 0);
    ch[0] = L.data();
    ch[1] = R.data();
    out.data32 = ch;
    out.channel_count = 2;
  }
  void kill()
  {
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
  double read(clap_id id) const
  {
    double v = 0;
    return params->get_value(p, id, &v) ? v : NAN;
  }
  bool known(clap_id id) const
  {
    double v = 0;
    return params->get_value(p, id, &v);
  }

  /* One block. `sets` are applied at frame 0 (so they are live for the whole
     block), a note-on at frame 0 when key >= 0. Returns the left channel. */
  std::vector<float> block(int key, const std::vector<std::pair<clap_id, double>> &sets,
                           int blocks)
  {
    std::vector<clap_event_param_value_t> pv;
    for (const auto &kv : sets)
    {
      clap_event_param_value_t e{};
      e.header.size = sizeof(e);
      e.header.type = CLAP_EVENT_PARAM_VALUE;
      e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      e.header.time = 0;
      e.param_id = kv.first;
      e.value = kv.second;
      e.note_id = -1;
      e.port_index = -1;
      e.channel = -1;
      e.key = -1;
      pv.push_back(e);
    }
    clap_event_note_t on{};
    on.header.size = sizeof(on);
    on.header.type = CLAP_EVENT_NOTE_ON;
    on.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    on.header.time = 0;
    on.note_id = 1;
    on.port_index = 0;
    on.channel = 0;
    on.key = (int16_t)key;
    on.velocity = 1.0;

    std::vector<const clap_event_header_t *> ord;
    struct Ctx { std::vector<const clap_event_header_t *> *o; } ctx{&ord};
    clap_input_events_t in{};
    in.ctx = &ctx;
    in.size = [](const clap_input_events_t *l) -> uint32_t {
      return (uint32_t)((Ctx *)l->ctx)->o->size();
    };
    in.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return (*((Ctx *)l->ctx)->o)[i];
    };

    std::vector<float> acc;
    for (int b = 0; b < blocks; b++)
    {
      ord.clear();
      if (b == 0)
      {
        for (auto &e : pv) ord.push_back(&e.header);
        if (key >= 0) ord.push_back(&on.header);
      }
      clap_process_t proc{};
      proc.frames_count = (uint32_t)L.size();
      proc.audio_outputs = &out;
      proc.audio_outputs_count = 1;
      proc.in_events = &in;
      proc.out_events = &kShOut;
      p->process(p, &proc);
      acc.insert(acc.end(), L.begin(), L.end());
    }
    return acc;
  }

  /* B181 note 2 needs a SEQUENCE of note events, which `block` above cannot
     express (one note-on, at frame 0, once). This is the same transport with
     an event list the caller supplies per block: `keys[b]` is what happens at
     the top of block b — a positive key is a note-on, a negative one is a
     note-off of |key|, 0 is nothing. Returns the left channel. */
  std::vector<float> sequence(const std::vector<std::pair<clap_id, double>> &sets,
                              const std::vector<int> &keys)
  {
    std::vector<clap_event_param_value_t> pv;
    for (const auto &kv : sets)
    {
      clap_event_param_value_t e{};
      e.header.size = sizeof(e);
      e.header.type = CLAP_EVENT_PARAM_VALUE;
      e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      e.header.time = 0;
      e.param_id = kv.first;
      e.value = kv.second;
      e.note_id = -1;
      e.port_index = -1;
      e.channel = -1;
      e.key = -1;
      pv.push_back(e);
    }
    std::vector<clap_event_note_t> ne(keys.size());
    std::vector<const clap_event_header_t *> ord;
    struct Ctx { std::vector<const clap_event_header_t *> *o; } ctx{&ord};
    clap_input_events_t in{};
    in.ctx = &ctx;
    in.size = [](const clap_input_events_t *l) -> uint32_t {
      return (uint32_t)((Ctx *)l->ctx)->o->size();
    };
    in.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return (*((Ctx *)l->ctx)->o)[i];
    };
    std::vector<float> acc;
    for (size_t b = 0; b < keys.size(); b++)
    {
      ord.clear();
      if (b == 0)
        for (auto &e : pv) ord.push_back(&e.header);
      if (keys[b] != 0)
      {
        clap_event_note_t &e = ne[b];
        e.header.size = sizeof(e);
        e.header.type = keys[b] > 0 ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
        e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        e.header.time = 0;
        e.note_id = keys[b] > 0 ? keys[b] : -keys[b];
        e.port_index = 0;
        e.channel = 0;
        e.key = (int16_t)(keys[b] > 0 ? keys[b] : -keys[b]);
        e.velocity = keys[b] > 0 ? 1.0 : 0.0;
        ord.push_back(&e.header);
      }
      clap_process_t proc{};
      proc.frames_count = (uint32_t)L.size();
      proc.audio_outputs = &out;
      proc.audio_outputs_count = 1;
      proc.in_events = &in;
      proc.out_events = &kShOut;
      p->process(p, &proc);
      acc.insert(acc.end(), L.begin(), L.end());
    }
    return acc;
  }
};

/* A fresh instance, one patch, one note, N blocks — every shell row's unit, so
   no row can be contaminated by the state another left behind. */
std::vector<float> shellRender(double sr, uint32_t blockSize, int blocks, int key,
                               const std::vector<std::pair<clap_id, double>> &sets)
{
  ShellRig r;
  r.boot(sr, blockSize);
  std::vector<float> out = r.block(key, sets, blocks);
  r.kill();
  return out;
}

// Both swarm oscillators OFF: what is left in the output is the sub's row and
// nothing else, which is what makes row 11c a parity measurement rather than a
// difference of two mixtures.
const std::vector<std::pair<clap_id, double>> kSwarmSilent = {{kOscEnable0, 0}, {kOscEnable1, 0}};

std::vector<std::pair<clap_id, double>> with(std::vector<std::pair<clap_id, double>> a,
                                             std::vector<std::pair<clap_id, double>> b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
double peakOf(const std::vector<float> &a)
{
  double m = 0;
  for (float v : a) m = std::max(m, std::fabs((double)v));
  return m;
}

void row(bool ok, const char *label, const char *fmt = nullptr, double v = 0)
{
  std::printf("%s %s", ok ? "PASS" : "FAIL", label);
  if (fmt) { std::printf("  "); std::printf(fmt, v); }
  std::printf("\n");
  if (!ok) g_failures++;
}

void head(const char *s) { std::printf("\n== %s ==\n", s); }

constexpr double kTwoPi = 6.283185307179586;
constexpr double kPi = 3.141592653589793;
// The smallest NORMAL float32. Anything below it and non-zero is a subnormal —
// the contract the host sees (§10.5, limit L5: this is not evidence about CPU
// stalls, which this machine cannot show either way).
constexpr double kF32MinNormal = 1.1754943508222875e-38;

double mtof(double m) { return 440 * std::pow(2, (m - 69) / 12); }

double rms(const std::vector<float> &a)
{
  double s = 0;
  for (float v : a) s += (double)v * v;
  return std::sqrt(s / std::max<size_t>(1, a.size()));
}

// ---------------------------------------------------------------- the core --
struct Set
{
  SubOscCore::Param p;
  double v;
};

SubOscCore make(double sr, std::initializer_list<Set> sets)
{
  SubOscCore c(sr);
  for (const Set &s : sets) c.setParam(s.p, s.v);
  return c;
}

// Mono left channel; `master` is the whole buffer (the core reads [0, n)).
std::vector<float> pull(SubOscCore &c, int n, const float *master = nullptr)
{
  std::vector<float> out((size_t)n), r((size_t)n);
  c.render(out.data(), r.data(), n, master);
  return out;
}

// First index at which two buffers differ, or -1. Bit comparison, not epsilon:
// every determinism and block-size row here claims EQUALITY, and an epsilon
// would quietly convert those claims into weaker ones.
long firstDiff(const std::vector<float> &a, const std::vector<float> &b)
{
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i]) return (long)i;
  return a.size() == b.size() ? -1 : (long)n;
}

// Spread of a set of measurements as a percentage of their mean — the lab
// harness's `spread`, so the two suites' drift numbers are comparable.
double spread(const std::vector<double> &xs)
{
  double lo = xs[0], hi = xs[0], sum = 0;
  for (double x : xs) { lo = std::min(lo, x); hi = std::max(hi, x); sum += x; }
  return (hi - lo) / (sum / (double)xs.size()) * 100;
}

// Goertzel: the magnitude of ONE frequency. A Hann window keeps the skirt of
// the (enormous) fundamental off the (tiny) alias bins being read.
double goertzel(const std::vector<float> &x, double sr, double f)
{
  const double w = kTwoPi * f / sr, c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  const size_t N = x.size();
  for (size_t i = 0; i < N; i++)
  {
    const double win = 0.5 - 0.5 * std::cos(kTwoPi * (double)i / (double)(N - 1));
    const double s0 = (double)x[i] * win + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) * 2 / (double)N;
}

/* THE SOUNDING FREQUENCY OF A SINE, from rising zero crossings with linear
   interpolation between the bracketing samples (B181 note 2). A Goertzel reads
   ENERGY at a frequency you already guessed; a glide is measured by asking
   what the frequency IS, which is a different question and needs a different
   instrument. First-to-last crossing over cycle count, so the window's ragged
   ends cost nothing; DC is not an issue (the sub's tone stage passes the
   fundamental and the shape here is a sine). Returns 0 if fewer than two
   crossings fall in the window, which is a reading no assertion accepts. */
double sineHz(const std::vector<float> &x, double sr, size_t from, size_t to)
{
  to = std::min(to, x.size());
  double first = -1, last = -1;
  long cycles = -1;
  for (size_t i = from + 1; i < to; i++)
    if (x[i - 1] <= 0 && x[i] > 0)
    {
      const double d = (double)x[i] - (double)x[i - 1];
      const double at = (double)(i - 1) + (d != 0 ? -(double)x[i - 1] / d : 0.0);
      if (first < 0) first = at;
      last = at;
      cycles++;
    }
  if (cycles < 1 || last <= first) return 0;
  return (double)cycles * sr / (last - first);
}

// ------------------------------------------------------------------ parity --
struct Scenario
{
  std::string name;
  double sr = 48000, secs = 0.5, vel = 1, master = -1;
  int block = 128, offAt = -1, note = 36;
  std::vector<std::pair<std::string, double>> keys;
};

// The master phase a sync scenario states: a saw at an absolute frequency,
// accumulated in double and stored f32 — the generator's `masterPhase`, the
// same two operations in the same order, which is what makes a sync reset land
// on the same sample in both languages.
std::vector<float> masterPhase(double hz, double sr, int n)
{
  std::vector<float> a((size_t)n);
  const double d = hz / sr;
  double ph = 0;
  for (int i = 0; i < n; i++)
  {
    ph += d;
    ph -= std::floor(ph);
    a[(size_t)i] = (float)ph;
  }
  return a;
}

// Interleaved stereo, rendered exactly as the generator does: BLOCK chunks with
// the note-off split onto its own boundary.
std::vector<float> renderScenario(const Scenario &sc, bool *keyOk = nullptr)
{
  SubOscCore c(sc.sr);
  bool ok = true;
  for (const auto &kv : sc.keys)
  {
    const int i = SubOscCore::indexOf(kv.first.c_str());
    // An UNKNOWN KEY IS A FAILURE, not a shrug: it means the lab grew a
    // parameter this port does not carry, which is exactly the silent parity
    // hole the whole-table dump exists to prevent.
    if (i < 0)
    {
      ok = false;
      std::printf("     unknown manifest key '%s'\n", kv.first.c_str());
      continue;
    }
    c.setParam((SubOscCore::Param)i, kv.second);
  }
  if (keyOk) *keyOk = ok;
  c.noteOn(sc.note, sc.vel);

  const int total = (int)std::lround(sc.sr * sc.secs);
  std::vector<float> master;
  if (sc.master > 0) master = masterPhase(sc.master, sc.sr, total);
  std::vector<float> out((size_t)total * 2);
  std::vector<float> L((size_t)sc.block), R((size_t)sc.block);
  for (int off = 0; off < total;)
  {
    int k = std::min(sc.block, total - off);
    if (sc.offAt > off && sc.offAt < off + k) k = sc.offAt - off;
    if (off == sc.offAt) c.noteOff();
    c.render(L.data(), R.data(), k, master.empty() ? nullptr : master.data() + off);
    for (int i = 0; i < k; i++)
    {
      out[(size_t)(off + i) * 2] = L[(size_t)i];
      out[(size_t)(off + i) * 2 + 1] = R[(size_t)i];
    }
    off += k;
  }
  return out;
}

// ------------------------------------------------------------- §10.6 lobes --
// The lobes are read from a RISING ZERO CROSSING, not from wherever the render
// window happens to start: without that anchor the "first" maximum is whichever
// one the buffer opened on, and the ordering claim silently inverts at some
// notes (it did, at MIDI 48, in the lab's first draft of this measurement).
struct Lobes
{
  double f0 = 0, per = 0, amax = 0;
  std::vector<double> pk, tr;
};

Lobes bumpLobes(double sr, int midi, double a, double phi)
{
  SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kBump},
                           {SubOscCore::kOctave, 0},
                           {SubOscCore::kKeytrack, 1},
                           {SubOscCore::kLevel, 1},
                           {SubOscCore::kTone, 20000},
                           {SubOscCore::kAttack, 0.0005},
                           {SubOscCore::kRelease, 0.01},
                           {SubOscCore::kPhase, 0},
                           {SubOscCore::kBumpAmt, a},
                           {SubOscCore::kBumpPhase, phi}});
  c.noteOn(midi, 1);
  pull(c, 4096);  // past the attack ramp
  Lobes r;
  r.f0 = c.freqHz();
  r.per = sr / r.f0;
  const std::vector<float> y = pull(c, (int)std::ceil(3 * r.per) + 4);
  long z = -1;
  for (size_t i = 1; i < y.size(); i++)
    if (y[i - 1] <= 0 && y[i] > 0) { z = (long)i; break; }
  const long end = std::min((long)y.size() - 1, z + (long)std::lround(r.per));
  for (long i = z + 1; i < end; i++)
  {
    if (y[i] > 0 && y[i] > y[i - 1] && y[i] >= y[i + 1]) r.pk.push_back(y[i]);
    if (y[i] > 0 && y[i] < y[i - 1] && y[i] <= y[i + 1]) r.tr.push_back(y[i]);
  }
  for (float v : y) r.amax = std::max(r.amax, (double)std::fabs(v));
  return r;
}

// §10.6's (a, phi) grid, the lab's exactly — extremes a = 0 / a = 0.6 and
// phi = +-pi are in it deliberately.
const double kAs[] = {0, 0.1, 0.2, 0.3, 0.35, 0.4, 0.5, 0.6};
const double kPhis[] = {-kPi, -2, -1, -0.25, 0, 0.25, 1, 2, kPi};

// The peak of the SHAPE (not of a render) by brute-force scan, against the
// normaliser the core's own recalc() computed for those parameters. The scan is
// the INDEPENDENT half: brute force over the raw formula where the core uses a
// 32-bracket bisection search, so agreement is two different methods meeting,
// not one method agreeing with itself. Scan bias is bounded by
// |f''|/2 * (dth/2)^2 <= 6.4/2 * (4.8e-5)^2 = 7e-9, two orders under the 1e-6
// tolerance. `norm` = 0 means "ask the core", anything else plants a normaliser.
constexpr int kPeakScan = 65536;
double shapePeak(double a, double phi, double norm = 0)
{
  if (norm == 0)
  {
    SubOscCore c = make(44100, {{SubOscCore::kBumpAmt, a}, {SubOscCore::kBumpPhase, phi}});
    norm = 1 / SubOscCore::bumpPeak(c.param(SubOscCore::kBumpAmt), c.param(SubOscCore::kBumpPhase));
  }
  double m = 0;
  for (int i = 0; i < kPeakScan; i++)
    m = std::max(m, std::fabs(SubOscCore::bumpAt((double)i / kPeakScan, a, phi, norm)));
  return m;
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/subosc";

  // ================================================================ 1. parity
  std::ifstream mf(dir + "/subosc-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL subosc manifest missing (run tools/golden/gen_subosc_goldens.mjs)\n");
    return 1;
  }
  std::vector<Scenario> scenarios;
  std::string line;
  while (std::getline(mf, line))
  {
    if (line.empty() || line[0] == '#') continue;
    const auto tab = line.find('\t');
    Scenario sc;
    sc.name = line.substr(0, tab);
    std::istringstream ss(line.substr(tab + 1));
    std::string tok;
    while (ss >> tok)
    {
      const auto eq = tok.find('=');
      const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
      if (k == "@sr") sc.sr = std::atof(v.c_str());
      else if (k == "@secs") sc.secs = std::atof(v.c_str());
      else if (k == "@block") sc.block = std::atoi(v.c_str());
      else if (k == "@off") sc.offAt = std::atoi(v.c_str());
      else if (k == "@master") sc.master = std::atof(v.c_str());
      else if (k == "@note")
      {
        const auto c1 = v.find(':');
        sc.note = std::atoi(v.c_str());
        sc.vel = std::atof(v.c_str() + c1 + 1);
      }
      else sc.keys.push_back({k, std::atof(v.c_str())});
    }
    scenarios.push_back(sc);
  }
  std::printf("-- parity (L0-1, eps = 1e-6 RMS) -- %zu scenarios at 48 000 and 44 100 Hz --\n",
              scenarios.size());

  double worst = 0;
  for (const auto &sc : scenarios)
  {
    std::ifstream gf(dir + "/" + sc.name + ".f32", std::ios::binary);
    if (!gf)
    {
      row(false, sc.name.c_str(), "golden missing");
      continue;
    }
    std::vector<float> ref;
    gf.seekg(0, std::ios::end);
    ref.resize((size_t)gf.tellg() / 4);
    gf.seekg(0);
    gf.read((char *)ref.data(), (std::streamsize)ref.size() * 4);

    bool keyOk = true;
    const std::vector<float> got = renderScenario(sc, &keyOk);
    double acc = 0, era = 0;
    const size_t n = std::min(ref.size(), got.size());
    for (size_t i = 0; i < n; i++)
    {
      const double d = (double)got[i] - (double)ref[i];
      acc += d * d;
      era += (double)ref[i] * ref[i];
    }
    const double r = std::sqrt(acc / std::max<size_t>(1, n));
    const double refRms = std::sqrt(era / std::max<size_t>(1, n));
    worst = std::max(worst, r);
    char buf[192];
    std::snprintf(buf, sizeof buf, "parity %-18s (ref rms %.4f, %zu samples)  rms =",
                  sc.name.c_str(), refRms, n);
    // refRms > 1e-4 is not decoration: a golden of silence would pass an RMS
    // comparison against a port that also renders silence, which is how a
    // scenario stops testing anything without anyone noticing.
    row(keyOk && ref.size() == got.size() && r <= 1e-6 && refRms > 1e-4, buf, "%.3e", r);
  }
  std::printf("   worst parity rms over all scenarios: %.3e  (gate 1e-6)\n", worst);

  // ========================================================= 2. §10.2 determinism
  head("§10.2 determinism — the seed is the control, and it reaches the stream");
  {
    auto noisy = [](uint32_t seed) {
      return make(44100, {{SubOscCore::kWave, SubOscCore::kNoise},
                          {SubOscCore::kTone, 1500},
                          {SubOscCore::kSeed, (double)seed}});
    };
    SubOscCore a = noisy(1), b = noisy(1);
    a.noteOn(36, 1);
    b.noteOn(36, 1);
    const std::vector<float> ya = pull(a, 16384), yb = pull(b, 16384);
    row(firstDiff(ya, yb) == -1 && rms(ya) > 1e-3,
        "two instances, same seed -> bit-identical (16384 samples)");

    // Five notes of history, then allOff(), must be indistinguishable from a
    // fresh core: D3's claim is that NO stream survives the reset.
    SubOscCore h = noisy(1);
    for (int i = 0; i < 5; i++)
    {
      h.noteOn(40 + i, 0.6);
      pull(h, 3000);
      h.noteOff();
      pull(h, 2000);
    }
    SubOscCore hk = h;  // the same history, WITHOUT the reset — the control
    h.allOff();
    h.noteOn(36, 1);
    const std::vector<float> yh = pull(h, 16384);
    row(firstDiff(ya, yh) == -1, "five notes of history + allOff() -> bit-identical to fresh");

    hk.noteOn(36, 1);
    const std::vector<float> yk = pull(hk, 16384);
    row(firstDiff(ya, yk) != -1,
        "CONTROL history WITHOUT allOff must differ (env/filter carry, by design)",
        "first diff at %.0f", (double)firstDiff(ya, yk));

    SubOscCore d = noisy(9876543);
    d.noteOn(36, 1);
    const std::vector<float> yd = pull(d, 16384);
    row(firstDiff(ya, yd) == 0, "CONTROL different seed must differ from sample 0",
        "first diff at %.0f", (double)firstDiff(ya, yd));

    // The bump draws on no stream, so its determinism claim is the narrower
    // one: the output is a pure function of (parameters, note). Its must-differ
    // control is therefore the phase, not a seed.
    auto bumper = [](double phi) {
      return make(44100, {{SubOscCore::kWave, SubOscCore::kBump}, {SubOscCore::kBumpPhase, phi}});
    };
    SubOscCore p1 = bumper(-0.25), p2 = bumper(-0.25), p3 = bumper(-0.24);
    p1.noteOn(36, 1);
    p2.noteOn(36, 1);
    p3.noteOn(36, 1);
    const std::vector<float> y1 = pull(p1, 16384), y2 = pull(p2, 16384), y3 = pull(p3, 16384);
    row(firstDiff(y1, y2) == -1 && rms(y1) > 1e-3,
        "bump: two instances, same params -> bit-identical");
    row(firstDiff(y1, y3) == 0, "CONTROL bumpPhase -0.25 vs -0.24 must differ from sample 0",
        "first diff at %.0f", (double)firstDiff(y1, y3));
  }

  // =============================================== 3. §10.3 sample-rate independence
  head("§10.3 sample-rate independence, 44.1 / 48 / 96 kHz");
  const std::vector<double> kRates = {44100, 48000, 96000};
  {
    // (a) envelope attack: time to env = 0.9, rendered one sample at a time so
    // the observable is read at sample resolution, the crossing interpolated.
    auto attackSeconds = [](double sr) {
      SubOscCore c = make(sr, {{SubOscCore::kAttack, 0.05},
                               {SubOscCore::kLevel, 1},
                               {SubOscCore::kWave, SubOscCore::kSine}});
      c.noteOn(60, 1);
      float l, r;
      double prev = 0;
      const int n = (int)std::ceil(0.2 * sr);
      for (int i = 0; i < n; i++)
      {
        c.render(&l, &r, 1);
        if (c.env >= 0.9)
        {
          const double frac = (0.9 - prev) / (c.env - prev);
          return (i + frac) / sr;
        }
        prev = c.env;
      }
      return std::nan("");
    };
    std::vector<double> t;
    for (double sr : kRates)
    {
      t.push_back(attackSeconds(sr));
      std::printf("   attack to 0.9 @ %.0f = %.4f ms\n", sr, t.back() * 1000);
    }
    row(spread(t) <= 0.5, "attack time drift <= 0.5 %", "%.4f %%", spread(t));

    // CONTROL — the ADR-009 trap itself, and the reason every time constant in
    // this core is a number of SECONDS: the same detector, run over a ramp
    // whose per-sample increment was fixed once at 44.1 kHz, must drift wildly.
    // The plant is arithmetic here rather than a mutated build (C++ cannot
    // plant a string into its own source the way the lab harness does), which
    // is legitimate because the quantity it corrupts — the increment — is the
    // whole of the mechanism under test.
    std::vector<double> tc;
    const double fixedInc = 1.0 / (0.05 * 44100);
    for (double sr : kRates)
    {
      double e = 0;
      int i = 0;
      for (; e < 0.9 && i < (int)(0.5 * sr); i++) e += fixedInc;
      tc.push_back(i / sr);
    }
    std::printf("   control (increment fixed at 44.1 k): %.1f / %.1f / %.1f ms\n",
                tc[0] * 1000, tc[1] * 1000, tc[2] * 1000);
    row(spread(tc) > 10, "CONTROL per-sample-constant attack must drift > 10 %", "%.2f %%", spread(tc));

    // (b) tone time constant, in SECONDS, measured from the decay RATIO at two
    // points well past the edge. A ratio of two late samples cancels the
    // one-sample BLEP smear at the edge, which is itself rate-dependent and
    // would otherwise be the thing measured.
    auto toneTau = [](double sr) {
      SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSquare},
                               {SubOscCore::kKeytrack, 0},
                               {SubOscCore::kOctave, -2},
                               {SubOscCore::kTone, 200},
                               {SubOscCore::kLevel, 1},
                               {SubOscCore::kAttack, 0.0005},
                               {SubOscCore::kPhase, 0}});
      c.noteOn(60, 1);
      const std::vector<float> y = pull(c, (int)std::ceil(0.06 * sr));
      const double f0 = c.freqHz();
      const long edge = std::lround(0.5 / f0 * sr);             // the square's falling edge
      const double settled = y[(size_t)(edge + std::lround(0.020 * sr))];
      const double tau0 = 1 / (kTwoPi * 200);
      const long i1 = edge + std::lround(2 * tau0 * sr), i2 = edge + std::lround(6 * tau0 * sr);
      const double d1 = std::fabs(y[(size_t)i1] - settled), d2 = std::fabs(y[(size_t)i2] - settled);
      return ((i2 - i1) / sr) / std::log(d1 / d2);
    };
    std::vector<double> tt;
    for (double sr : kRates)
    {
      tt.push_back(toneTau(sr));
      std::printf("   tone tau (fc 200 Hz) @ %.0f = %.2f us   (ideal 1/2pi.fc = %.2f us)\n",
                  sr, tt.back() * 1e6, 1e6 / (kTwoPi * 200));
    }
    row(spread(tt) <= 0.5, "tone time constant drift <= 0.5 %", "%.4f %%", spread(tt));

    // (c) audit A7's actual finding: does the tone's MAGNITUDE response move
    // with the sample rate? A time constant can be right while the response
    // warps. octave 0 EXPLICITLY — the default is -1 and the closed-form anchor
    // below is written in terms of mtof(83).
    auto toneMagRatio = [](double sr) {
      auto at = [sr](double fc) {
        SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSine},
                                 {SubOscCore::kTone, fc},
                                 {SubOscCore::kLevel, 1},
                                 {SubOscCore::kAttack, 0.0005},
                                 {SubOscCore::kKeytrack, 1},
                                 {SubOscCore::kOctave, 0}});
        c.noteOn(83, 1);  // 987.77 Hz, the same note at every rate
        pull(c, (int)std::lround(0.05 * sr));
        return rms(pull(c, (int)std::lround(0.2 * sr)));
      };
      return at(500) / at(20000);
    };
    std::vector<double> mag, closed;
    for (double sr : kRates)
    {
      mag.push_back(toneMagRatio(sr));
      // Closed-form anchor: the TPT one-pole's magnitude is g/sqrt(g^2+tan^2),
      // an oracle that needs no reference implementation at all (L0031).
      const double g = std::tan(kPi * 500 / sr), t = std::tan(kPi * mtof(83) / sr);
      closed.push_back(g / std::sqrt(g * g + t * t));
      std::printf("   |H(987.8 Hz)| fc 500 @ %.0f = %.6f   closed form %.6f\n",
                  sr, mag.back(), closed.back());
    }
    row(spread(mag) <= 0.5, "tone magnitude drift <= 0.5 %", "%.4f %%", spread(mag));
    double err = 0;
    for (size_t i = 0; i < mag.size(); i++)
      err = std::max(err, std::fabs(mag[i] - closed[i]) / closed[i] * 100);
    row(err < 1, "tone magnitude matches closed form within 1 %", "worst %.4f %%", err);

    // (d) D4 made visible — audit A7 / §3.6: with the naive pole
    // `1 - exp(-2pi*fc/sr)`, "tone wide open" IS A DIFFERENT FILTER AT EVERY
    // SAMPLE RATE. Read at Nyquist with the tone at its §7 default of 20 kHz,
    // where the defect is largest and where the TPT has a structural zero — the
    // bilinear map sends s = inf to z = -1, so the ported law reads exactly 0
    // at 44.1, 48 AND 96 kHz while the naive one moves by more than a dB.
    //
    // THIS ROW COMPARES TWO LAWS IN CLOSED FORM, not two builds: the core's
    // adherence to the TPT law is row (c)'s business (measured against the same
    // closed form, worst error 0.108 %), and the naive law has no build here to
    // run — its whole point is that this port does not contain it. It is the
    // CONTROL for row (c): a detector that showed no difference between the two
    // laws anywhere would make row (c)'s 0.16 % drift evidence of nothing. The
    // audit's own figures (-1.34 dB at 44.1 k vs -5.53 dB at 96 k) were taken
    // on the SAW engine's filter at its own setting; these are the same class
    // of defect re-measured at SUB OSC's default, not a reproduction of them.
    std::vector<double> naive;
    for (double sr : kRates)
    {
      const double a = 1 - std::exp(-kTwoPi * 20000 / sr);  // the naive coefficient
      // |H| of y += a*(x - y) at Nyquist (w = pi) is a/(2 - a).
      naive.push_back(a / (2 - a));
      std::printf("   control naive one-pole |H(Nyquist)| fc 20000 @ %.0f = %.6f (%.2f dB); "
                  "the TPT law reads exactly 0 at every rate\n",
                  sr, naive.back(), 20 * std::log10(naive.back()));
    }
    row(spread(naive) > 10,
        "CONTROL the naive one-pole's 'wide open' drifts with the sample rate (audit A7)",
        "%.2f %%", spread(naive));

    // (e) the envelope reaches EXACTLY 1 in exactly `attack` seconds at every
    // rate — §5.3's whole argument for a linear ramp over an exponential one.
    // The tolerance is ONE SAMPLE, which is the quantisation of the claim and
    // not a fudge: `atkInc = 1/(attack*sr)` needs ceil(attack*sr) steps.
    std::vector<double> full;
    for (double sr : kRates)
    {
      SubOscCore c = make(sr, {{SubOscCore::kAttack, 0.05}, {SubOscCore::kLevel, 1}});
      c.noteOn(60, 1);
      float l, r;
      int i = 0;
      for (; i < (int)(0.2 * sr) && c.env < 1.0; i++) c.render(&l, &r, 1);
      full.push_back(i / sr);
      std::printf("   envelope reaches exactly 1.0 @ %.0f after %.5f ms (attack 50 ms, %d samples)\n",
                  sr, full.back() * 1000, i);
    }
    double wErr = 0;
    for (size_t i = 0; i < full.size(); i++)
      wErr = std::max(wErr, std::fabs(full[i] - 0.05) * kRates[i]);
    row(wErr <= 1.5 && spread(full) <= 0.5,
        "envelope hits exactly 1.0 at `attack` seconds, every rate (within 1 sample)",
        "worst error %.2f samples", wErr);
  }

  // ============================================ 4. §10.4 block-size independence
  head("§10.4 block-size independence, chunks 1 / 7 / 64 / 256 / 333 vs one whole render");
  {
    constexpr int kTotal = 20000;
    const std::vector<float> master = masterPhase(220, 44100, kTotal);
    // `reset` emulates the defect class this row exists to catch: a core whose
    // state does not survive a render call (audit A10, the SAW engine's pan
    // motion). It is the control, and it must break the comparator.
    auto chunked = [&](int leg, int chunk, bool reset) {
      SubOscCore c = leg == 0
          ? make(44100, {{SubOscCore::kWave, SubOscCore::kPulse},
                         {SubOscCore::kWidth, 0.27},
                         {SubOscCore::kTone, 900},
                         {SubOscCore::kLevel, 0.9},
                         {SubOscCore::kSync, 1},
                         {SubOscCore::kAttack, 0.004},
                         {SubOscCore::kRelease, 0.05}})
          : make(44100, {{SubOscCore::kWave, SubOscCore::kNoise},
                         {SubOscCore::kTone, 1500},
                         {SubOscCore::kLevel, 0.9},
                         {SubOscCore::kSeed, 7},
                         {SubOscCore::kAttack, 0.002}});
      c.noteOn(45, 1);
      std::vector<float> out((size_t)kTotal), L((size_t)chunk), R((size_t)chunk);
      for (int i = 0; i < kTotal;)
      {
        const int n = std::min(chunk, kTotal - i);
        if (reset && i > 0) { c.allOff(); c.noteOn(45, 1); }
        c.render(L.data(), R.data(), n, leg == 0 ? master.data() + i : nullptr);
        for (int k = 0; k < n; k++) out[(size_t)(i + k)] = L[(size_t)k];
        i += n;
      }
      return out;
    };
    const char *legName[2] = {"pulse + tone + hard sync", "seeded noise + tone"};
    for (int leg = 0; leg < 2; leg++)
    {
      const std::vector<float> ref = chunked(leg, kTotal, false);
      long bad = -1;
      int badChunk = 0;
      for (int ch : {1, 7, 64, 256, 333})
      {
        const long d = firstDiff(ref, chunked(leg, ch, false));
        if (d != -1 && bad == -1) { bad = d; badChunk = ch; }
      }
      char buf[128];
      std::snprintf(buf, sizeof buf, "block-size independent: %s", legName[leg]);
      if (bad == -1) row(true, buf, "all 5 chunkings bit-identical");
      else { std::printf("     chunk %d differs at %ld\n", badChunk, bad); row(false, buf); }
    }
    const std::vector<float> ref = chunked(0, kTotal, true);
    const long d = firstDiff(ref, chunked(0, 64, true));
    row(d != -1, "CONTROL a core reset between calls must break block independence",
        "first diff at %.0f", (double)d);
  }

  // ====================================== 5. §10.5 exact silence and denormals
  head("§10.5 level 0 is silence exactly, and the release tail never goes subnormal");
  {
    // The count comes from the enum, not from a literal: a seventh shape was
    // added (bump, B155) and a hardcoded "six" would have made this row lie
    // about its own coverage while still passing.
    long bad = -1;
    int badWave = -1;
    for (int w = 0; w < SubOscCore::kWaveCount; w++)
    {
      SubOscCore c = make(44100, {{SubOscCore::kWave, (double)w},
                                  {SubOscCore::kLevel, 0},
                                  {SubOscCore::kTone, 900},
                                  {SubOscCore::kAttack, 0.001}});
      c.noteOn(36, 1);
      const std::vector<float> y = pull(c, 8192);
      for (size_t i = 0; i < y.size(); i++)
        if (y[i] != 0) { bad = (long)i; badWave = w; break; }
      if (bad != -1) break;
    }
    char buf[128];
    std::snprintf(buf, sizeof buf, "all %d waveforms, level 0 -> every sample exactly 0",
                  SubOscCore::kWaveCount);
    if (bad == -1) row(true, buf, "7 waveforms x 8192 samples");
    else { std::printf("     wave %d non-zero at %ld\n", badWave, bad); row(false, buf); }

    SubOscCore c = make(44100, {{SubOscCore::kWave, SubOscCore::kSaw},
                                {SubOscCore::kLevel, 0.001},
                                {SubOscCore::kTone, 900},
                                {SubOscCore::kAttack, 0.001}});
    c.noteOn(36, 1);
    const std::vector<float> y = pull(c, 8192);
    int nz = 0;
    for (float v : y) if (v != 0) nz++;
    row(nz > 0, "CONTROL level 0.001 must NOT be silent", "%.0f non-zero samples", (double)nz);
  }
  {
    auto tail = [](double flush) {
      SubOscCore c = make(44100, {{SubOscCore::kWave, SubOscCore::kSaw},
                                  {SubOscCore::kLevel, 1},
                                  {SubOscCore::kTone, 200},
                                  {SubOscCore::kAttack, 0.001},
                                  {SubOscCore::kRelease, 0.01}});
      c.flushFloor = flush;
      c.noteOn(36, 1);
      pull(c, (int)std::lround(0.2 * 44100));
      c.noteOff();
      const std::vector<float> y = pull(c, (int)std::lround(2.0 * 44100));
      long n = 0, last = 0;
      for (size_t i = 0; i < y.size(); i++)
      {
        const double a = std::fabs(y[i]);
        if (a != 0 && a < kF32MinNormal) n++;
        if (a != 0) last = (long)i;
      }
      std::printf("   flush %.0e: %ld subnormal sample(s); last non-zero at %ld of %zu\n",
                  flush, n, last, y.size());
      return n;
    };
    row(tail(SubOscCore::kFlush) == 0, "2.0 s of tail contains 0 float32-subnormal samples");
    // CONTROL: remove the flush-to-zero (the one test hook) and the tail decays
    // straight through the subnormal range. Limit L5: this counts subnormals in
    // the OUTPUT, the contract the host sees — it is not evidence about CPU
    // stalls, which this machine cannot show either way.
    row(tail(0) > 0, "CONTROL flush-to-zero removed must produce subnormals");
  }

  // ================================================ 6. §10.6 the bump's shape
  head("§10.6 bump: the two-lobe silhouette at the defaults, and a peak of exactly 1");
  {
    // DERIVED 2026-09-19 by rendering one period over an (a, phi) grid and
    // measuring the two positive lobes (traces/2026-09-19-b155-subosc-bump.md):
    // the trailing lobe is 0.8823 of the leading one (1.09 dB down — audible as
    // a shape, not as two events), the trough between them at 0.637 of the
    // leading peak. phi's SIGN decides the order.
    constexpr double kRatio = 0.8823, kTroughFrac = 0.6369, kTol = 0.05;
    bool ok = true;
    for (int m : {24, 36, 48, 60})
    {
      const Lobes r = bumpLobes(44100, m, 0.35, -0.25);
      if (r.pk.size() != 2)
      {
        std::printf("   MIDI %d: %zu positive lobes\n", m, r.pk.size());
        ok = false;
        continue;
      }
      const double ratio = r.pk[1] / r.pk[0];
      const double tf = r.tr.empty() ? std::nan("") : r.tr[0] / r.pk[0];
      std::printf("   MIDI %-3d f0 %7.2f Hz  period %7.1f smp  lobes %.5f / %.5f  "
                  "ratio %.5f  trough/peak %.5f  peak %.5f\n",
                  m, r.f0, r.per, r.pk[0], r.pk[1], ratio, tf, r.amax);
      if (std::fabs(ratio - kRatio) / kRatio > kTol) ok = false;
      if (!(std::fabs(tf - kTroughFrac) / kTroughFrac <= kTol)) ok = false;
    }
    row(ok, "bump lobe ratio = 0.8823 +-5 % at every note (big bump FIRST)");

    // CONTROL (must read ~1): at phi = 0 the lobes are exactly equal by
    // symmetry. If this still read 0.88 the measurement would be returning a
    // constant and the row above would be vacuous.
    const Lobes sym = bumpLobes(44100, 36, 0.35, 0);
    const double symRatio = sym.pk.size() == 2 ? sym.pk[1] / sym.pk[0] : 0;
    row(std::fabs(symRatio - 1) < 0.002, "CONTROL phi = 0 must give two EQUAL lobes (+-0.2 %)",
        "%.5f", symRatio);
    // CONTROL (must invert): phi = +0.25 is the mirror, so the SMALL bump
    // leads. This pins the human's ordering to the SIGN of the default rather
    // than to the indexing of the measurement.
    const Lobes mir = bumpLobes(44100, 36, 0.35, 0.25);
    const double mirRatio = mir.pk.size() == 2 ? mir.pk[1] / mir.pk[0] : 0;
    row(std::fabs(mirRatio - 1 / kRatio) / (1 / kRatio) < kTol,
        "CONTROL phi = +0.25 must INVERT the order (small bump leads)", "%.5f", mirRatio);
  }
  {
    // PEAK-EXACT, NOT MERELY BOUNDED (ADR-178 R7). The shape used to be divided
    // by the analytic bound 1 + a, provable in one operation but leaving the
    // bump at 0.750 of full scale at the defaults — up to 3.01 dB under every
    // other waveform (superseded limit L7). Swept, not spot-checked: the claim
    // is max|y| = 1 for EVERY (a, phi).
    double worstDev = 0, lo = 9, hi = 0, unnorm = 0;
    double atA = 0, atPhi = 0;
    for (double a : kAs)
      for (double phi : kPhis)
      {
        const double pk = shapePeak(a, phi);
        lo = std::min(lo, pk);
        hi = std::max(hi, pk);
        if (std::fabs(pk - 1) > worstDev) { worstDev = std::fabs(pk - 1); atA = a; atPhi = phi; }
        unnorm = std::max(unnorm, shapePeak(a, phi, 1.0));
      }
    std::printf("   8 x 9 grid, %d-point scan of one period: peak in [%.9f, %.9f], "
                "worst deviation %.2e (a=%.2f, phi=%.2f)\n",
                kPeakScan, lo, hi, worstDev, atA, atPhi);
    row(worstDev <= 1e-6, "bump peaks at 1.000000 +-1e-6 over the whole (a, phi) grid",
        "worst |peak - 1| = %.3e", worstDev);
    // CONTROL: the same scan with the normaliser removed must overshoot. Without
    // it, "peak <= 1" could be true because the shape is quiet rather than
    // because it is normalised.
    row(unnorm > 1.05, "CONTROL normaliser removed must overshoot 1", "worst %.6f", unnorm);
    // CONTROL: plant the OLD analytic bound back; the defaults must read 0.750,
    // the number §9's limit L7 recorded for it. A peak check that still read
    // 1.000 with the wrong normaliser in place would be measuring the scan and
    // not the core (L0032).
    const double bound = shapePeak(0.35, -0.25, 1 / (1 + 0.35));
    row(std::fabs(bound - 0.750) < 5e-4, "CONTROL old 1/(1+a) bound must read 0.750 at the defaults",
        "%.6f", bound);
  }

  // ================================ 7. full scale: no shape is quieter than the rest
  head("full scale: every shape reaches 1, which is the 3.01 dB ruling R7 bought");
  {
    // Measured through the WHOLE render path at the lowest note the pitch law
    // reaches (MIDI 0, octave -2, semis -12 = 1.03 Hz, 42 824 samples per
    // period at 44.1 kHz), so the render samples each shape densely enough for
    // the peak to be the SHAPE's and not the sampling grid's: the naive
    // triangle's apex is missed by at most 4 * (0.5/42824) = 4.7e-5, and that
    // resolution is the gate's floor rather than something tuned around.
    //
    // THE CEILING IS NOT GATED HERE, AND THE REASON IS MEASURED, NOT ASSERTED.
    // The discontinuous shapes read ~1.056 through the module, which is the TPT
    // one-pole's STEP OVERSHOOT at a near-Nyquist cutoff (its state pole sits at
    // 1-2G = -0.726 with the tone wide open), not the oscillator exceeding
    // unity: the same saw through a 200 Hz tone reads just UNDER 1, and that
    // pair of numbers is printed below. Limit L4 is the pin — "the numbers are
    // the MODULE's, not the oscillator's", because the tone filter is in the
    // signal path — and headroom below the module is the shell's business, not
    // the core's. The oscillator's own ceiling is gated where it can be seen
    // exactly: §10.6's 65 536-point shape scan, above.
    const char *names[] = {"sine", "triangle", "square", "saw", "pulse", "noise", "bump"};
    auto shapeRenderPeak = [](int w, double tone) {
      SubOscCore c = make(44100, {{SubOscCore::kWave, (double)w},
                                  {SubOscCore::kOctave, -2},
                                  {SubOscCore::kSemis, -12},
                                  {SubOscCore::kLevel, 1},
                                  {SubOscCore::kTone, tone},
                                  {SubOscCore::kWidth, 0.27},
                                  {SubOscCore::kAttack, 0.0005}});
      c.noteOn(0, 1);
      pull(c, 2048);  // past the attack ramp
      const std::vector<float> y = pull(c, 52000);
      double pk = 0;
      for (float v : y) pk = std::max(pk, (double)std::fabs(v));
      return pk;
    };
    double worstShort = 0;
    double pkSine = 0, pkBump = 0;
    for (int w = 0; w < SubOscCore::kWaveCount; w++)
    {
      const double pk = shapeRenderPeak(w, 20000);
      std::printf("   %-9s peak %.6f\n", names[w], pk);
      if (w == SubOscCore::kSine) pkSine = pk;
      if (w == SubOscCore::kBump) pkBump = pk;
      // Noise is printed but not gated on a peak: a full-band signal through a
      // near-Nyquist one-pole reads the FILTER's ringing (1.43), and the
      // shape's own amplitude claim — uniform on [-1, 1) — is pinned bit-for-bit
      // by the two seeded parity scenarios instead. A stated boundary, not a
      // silent exclusion (L0033).
      if (w != SubOscCore::kNoise) worstShort = std::max(worstShort, 1 - pk);
    }
    std::printf("   saw at tone 20000 = %.6f vs at tone 200 = %.6f — the excess above 1 is the "
                "TPT step overshoot, not the oscillator\n",
                shapeRenderPeak(SubOscCore::kSaw, 20000), shapeRenderPeak(SubOscCore::kSaw, 200));
    row(worstShort <= 1e-4,
        "the six deterministic shapes all reach full scale (within the render's 1e-4 resolution)",
        "worst shortfall %.2e", worstShort);
    // R7, end to end: the bump is as loud as the plainest shape through the
    // identical path. Its must-fail control is the shape-level row above — the
    // superseded 1/(1+a) bound reads 0.750, i.e. 3.01 dB under this.
    row(std::fabs(pkBump - pkSine) <= 1e-4,
        "bump reaches the SAME peak as the sine (R7; the old 1/(1+a) bound reads 0.750)",
        "|bump - sine| = %.2e", std::fabs(pkBump - pkSine));
  }

  /* ============= 7b. THE BOTTOM OCTAVE (B181 note 1: octave floor -2 -> -3)
     A WIDENED RANGE IS ZERO COVERAGE UNLESS SOMETHING ASKS ABOUT IT (L0031
     (B)): the goldens now carry two -3 scenarios, but parity certifies only
     AGREEMENT with the lab — it cannot see a shared defect, and "1.02 Hz is
     fine" is exactly the kind of claim a shared implementation would confirm
     for both sides. So the floor is asserted here against absolutes instead:
     finiteness, no subnormals, and a peak still under the shell row's headroom
     divisor. THE DEFAULT IS ALSO PINNED — the widening was sanctioned as a
     floor move, and a default that drifted with it would silently re-pitch
     every stored patch. */
  head("B181 note 1: the -3 floor — 1.02 Hz at MIDI 0, and the default did not move");
  {
    row(SubOscCore::kParamTable[SubOscCore::kOctave].min == -3 &&
            SubOscCore::kParamTable[SubOscCore::kOctave].max == 0 &&
            SubOscCore::kParamTable[SubOscCore::kOctave].def == -1,
        "octave is [-3, 0] with the default STILL -1 (bit-inertness of the widening)",
        "min %.0f", SubOscCore::kParamTable[SubOscCore::kOctave].min);
    {
      SubOscCore c = make(44100, {{SubOscCore::kOctave, -3}});
      c.noteOn(0, 1);
      // 440 * 2^((0 - 36 - 69)/12). Closed form, not a number copied off a run.
      const double want = 440 * std::pow(2.0, (0.0 - 36.0 - 69.0) / 12.0);
      row(std::fabs(c.freqHz() - want) < 1e-12 && c.freqHz() > 1.0 && c.freqHz() < 1.05,
          "the lowest fundamental the module can be asked for is 1.02197 Hz", "%.6f Hz",
          c.freqHz());
    }
    // Every shape, two seconds each with the note released half way, at the
    // floor and at full level: the release tail is where a 2.3e-5 phase
    // increment would park the filter state in the subnormal range.
    constexpr float kF32Min = 1.1754943508222875e-38f;
    double worstPeak = 0;
    long subn = 0, nonFinite = 0;
    for (int w = 0; w < SubOscCore::kWaveCount; w++)
    {
      SubOscCore c = make(44100, {{SubOscCore::kWave, (double)w},
                                  {SubOscCore::kOctave, -3},
                                  {SubOscCore::kLevel, 1},
                                  {SubOscCore::kRelease, 1.0}});
      c.noteOn(0, 1);
      for (int b = 0; b < 172; b++)   // 172 * 512 = 88 064 samples = 2.0 s
      {
        if (b == 43) c.noteOff();
        for (float v : pull(c, 512))
        {
          if (!std::isfinite(v)) nonFinite++;
          const float a = std::fabs(v);
          if (a > 0 && a < kF32Min) subn++;
          worstPeak = std::max(worstPeak, (double)a);
        }
      }
    }
    row(nonFinite == 0 && subn == 0,
        "at the floor every shape stays finite and never enters the subnormal range",
        "non-finite + subnormal samples = %.0f", (double)(nonFinite + subn));
    // The shell row divides by 1.425 so a sub at level 1 cannot reach the rail
    // (renderSubSpan's kSubRowHeadroomPeak). The new floor must not breach the
    // constant that bound was measured for.
    row(worstPeak < 1.425,
        "the floor does not breach the shell row's headroom divisor (1.425)",
        "worst peak over all seven shapes %.6f", worstPeak);
    /* CONTROL, and it is the must-read-differently half (L0032): the SAME scan
       one octave up must give a DIFFERENT peak set. Without it these rows would
       pass for a core that silently clamped -3 back to -2 — the exact failure
       the widening could have. */
    {
      SubOscCore lo = make(44100, {{SubOscCore::kOctave, -3}}), hi = make(44100, {{SubOscCore::kOctave, -2}});
      lo.noteOn(36, 1);
      hi.noteOn(36, 1);
      row(std::fabs(lo.freqHz() * 2 - hi.freqHz()) < 1e-12 && lo.freqHz() != hi.freqHz(),
          "CONTROL -3 is an octave BELOW -2, not -2 clamped", "%.6f Hz at -3", lo.freqHz());
    }
  }

  // ====================== 8. the polyBLEP correction is present in THIS build
  head("§3/§10.1 the polyBLEP is in this build (Goertzel, not the lab's FFT — see the header)");
  {
    // The worst FOLDED harmonic of a saw at MIDI 60, read one bin at a time.
    // Harmonics above Nyquist fold to |k*f0 - round(k*f0/sr)*sr|; bins within
    // 25 Hz of a real harmonic are skipped so the fundamental's own skirt and
    // the true partials are never mistaken for an alias.
    const double sr = 44100;
    auto floorDb = [&](const std::vector<float> &y, double f0) {
      const double fund = goertzel(y, sr, f0);
      double worstMag = 0, worstF = 0;
      for (int k = (int)std::ceil(0.5 * sr / f0) + 1; k < 260; k++)
      {
        const double raw = k * f0;
        const double fold = std::fabs(raw - std::round(raw / sr) * sr);
        if (fold < 20 || fold > 0.5 * sr - 20) continue;
        bool nearHarmonic = false;
        for (int h = 1; h * f0 < 0.5 * sr; h++)
          if (std::fabs(fold - h * f0) < 25) { nearHarmonic = true; break; }
        if (nearHarmonic) continue;
        const double m = goertzel(y, sr, fold);
        if (m > worstMag) { worstMag = m; worstF = fold; }
      }
      std::printf("   worst folded bin %.0f Hz at %.1f dB below the fundamental\n",
                  worstF, 20 * std::log10(worstMag / fund));
      return 20 * std::log10(worstMag / fund);
    };
    SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSaw},
                             {SubOscCore::kOctave, 0},
                             {SubOscCore::kLevel, 1},
                             {SubOscCore::kTone, 20000},
                             {SubOscCore::kAttack, 0.0005}});
    c.noteOn(60, 1);
    pull(c, 4096);
    const std::vector<float> y = pull(c, 32768);
    const double f0 = c.freqHz();
    std::printf("   port   (polyBLEP): ");
    const double portDb = floorDb(y, f0);

    // CONTROL: the same note, same length, same detector, on a NAIVE saw — no
    // BLEP at all. §10.1 measured 11 dB of separation between the two at this
    // note with a Kaiser FFT; anything close to zero here would mean the
    // correction is not in the build (or that the detector cannot see it).
    std::vector<float> naive((size_t)y.size());
    {
      double ph = 0;
      const double d = f0 / sr;
      for (size_t i = 0; i < naive.size(); i++)
      {
        ph += d;
        ph -= std::floor(ph);
        naive[i] = (float)(2 * ph - 1);
      }
    }
    std::printf("   control (naive):   ");
    const double naiveDb = floorDb(naive, f0);
    row(portDb < naiveDb - 6, "polyBLEP buys real separation from the naive shape",
        "%.1f dB", naiveDb - portDb);
    row(naiveDb > -46, "CONTROL the naive saw must BREACH §10.1's MIDI-60 limit (-46 dB)",
        "%.1f dB", naiveDb);
  }

  // ========================================== 9. §7 the clamped table contract
  head("§7/D2 one clamped table: setParam clamps every known key and throws on an unknown one");
  {
    SubOscCore c(48000);
    bool threw = false;
    try { c.setParam("notAParameter", 1); }
    catch (const std::exception &) { threw = true; }
    row(threw, "setParam throws on an unknown address (audit A2: no silent no-op)");

    bool threwKnown = false;
    try { c.setParam("wave", 999); }
    catch (const std::exception &) { threwKnown = true; }
    // CONTROL: a KNOWN key must not throw — a setParam that rejected everything
    // would pass the row above while being useless.
    row(!threwKnown, "CONTROL a known address does not throw");
    row(c.param(SubOscCore::kWave) == SubOscCore::kWaveCount - 1,
        "an out-of-range value lands on the bound, it does not walk past it",
        "wave = %.0f", c.param(SubOscCore::kWave));
    c.setParam("octave", 7);
    c.setParam("tone", -1000);
    c.setParam("bumpAmt", 99);
    row(c.param(SubOscCore::kOctave) == 0 && c.param(SubOscCore::kTone) == 30 &&
            c.param(SubOscCore::kBumpAmt) == 0.6,
        "octave / tone / bumpAmt clamp to §7's bounds from both directions");
    // JS Math.round is half-UP, not half-away-from-zero: octave -1.5 is -1 in
    // the lab and would be -2 under std::round. A stepped parameter that lands
    // one step away from the lab's is a parity failure nothing else here sees,
    // because no golden happens to set a stepped parameter to a half-integer.
    c.setParam("octave", -1.5);
    row(c.param(SubOscCore::kOctave) == -1, "stepped parameters round half-UP, as JS does",
        "octave(-1.5) = %.0f", c.param(SubOscCore::kOctave));
  }

  // ================================================ 11. THE SHELL (B172 phase 2)
  // Sections 1-10 drive the core DIRECTLY and are structurally blind to
  // everything between it and the output (L0031 (B)). These rows drive the
  // real plugin through the CLAP factory.
  head("11. the shell: the SUB OSC engine block (B172 phase 2)");
  {
    /* ---- 11.0 DISPATCH. Any id >= 3000 is unreachable without the engine
       intercept in findParam — `oscOfId(4000)` is 4, which fails
       `osc >= kNumOsc` and returns nullptr, silently, with every other gate
       green. And an id the block does NOT claim must stay unreachable, or
       "the block exists" degenerates into "the band exists". */
    {
      ShellRig r;
      r.boot(48000, 256);
      bool all = true;
      for (int i = 0; i < kSubIdCount; i++) all = all && r.known((clap_id)(kSubIdBase + i));
      row(all, "11.0a every one of the block's 20 ids resolves through the shell");
      // 4020 is the first id ABOVE the block (it was 4016 until B181 claimed
      // it); the probe stays hard against the block's top edge on purpose.
      row(!r.known(3000) && !r.known(3999) && !r.known(4020) && !r.known(4999),
          "11.0b REFUSAL: an unclaimed id in the reserved engine band is not a parameter");
      row(r.read(kSubOnId) == 0, "11.0c the block's gate ships OFF", "subOn = %.0f",
          r.read(kSubOnId));
      // POSITIONAL, and the ranges are the CORE TABLE'S — asked through the
      // host's own param info, never through the shell's internals.
      bool pos = true;
      for (int i = 0; i < SubOscCore::kParamCount; i++)
      {
        const SubOscCore::ParamSpec &sp = SubOscCore::kParamTable[i];
        clap_param_info_t info{};
        bool got = false;
        for (uint32_t x = 0, n = r.params->count(r.p); x < n && !got; x++)
          if (r.params->get_info(r.p, x, &info) && info.id == (clap_id)(kSubIdBase + i))
            got = true;
        pos = pos && got && info.min_value == sp.min && info.max_value == sp.max &&
              info.default_value == sp.def &&
              ((info.flags & CLAP_PARAM_IS_STEPPED) != 0) == (sp.step != 0);
      }
      row(pos, "11.0d id - 4000 IS the core's enum index, and every row's range, default "
               "and steppedness are SubOscCore::kParamTable's");
      r.kill();
    }

    /* ---- 11.a THE GATE OFF IS BIT-INERT, and this is the CONTROL row.
       "Identical to a build WITHOUT the block" cannot be measured from inside
       a build that has it. What IS measured — and is the operative half — is
       that with the gate off, nothing the block can be set to reaches the
       output at all. Bit equality, not eps.
       The other half, that the block's mere PRESENCE changed nothing, is
       carried by statefix_check (pre-B172 fixtures against goldens stored
       before this change existed) and by bank_check's re-save bit-identity.
       Both run in the same `./verify full`; neither is restated here. */
    const std::vector<std::pair<clap_id, double>> loud = {
        {kSubIdBase + 0, SubOscCore::kNoise}, {kSubIdBase + 7, 1.0},
        {kSubIdBase + 10, 20000.0},           {kSubIdBase + 4, 0.0},
        {kSubIdBase + 9, 1.0},                {kSubIdBase + 12, 0.0005}};
    const std::vector<float> bare = shellRender(48000, 256, 16, 45, {});
    const std::vector<float> armedOff = shellRender(48000, 256, 16, 45, loud);
    row(firstDiff(bare, armedOff) < 0,
        "11a CONTROL: with the gate OFF, a fully-driven SUB block changes not one sample");
    const std::vector<float> armedOn =
        shellRender(48000, 256, 16, 45, with(loud, {{kSubOnId, 1}}));
    row(firstDiff(bare, armedOn) >= 0,
        "11a CALIBRATION: the same patch with the gate ON does differ — 11a's comparison "
        "can fail", "first differing sample %.0f", (double)firstDiff(bare, armedOn));

    // ---- 11.b the gate ON at level 0 is exact silence FROM THE ROW.
    const std::vector<float> onZero =
        shellRender(48000, 256, 16, 45, with(loud, {{kSubOnId, 1}, {kSubIdBase + 7, 0.0}}));
    row(firstDiff(bare, onZero) < 0,
        "11b the gate ON at level 0 is exact silence — bit-identical to the gate off");

    /* ---- 11.c PARITY THROUGH THE SHELL. Both swarm oscillators OFF, so what
       leaves the plugin IS source row 2 carried by the matrix's default
       topology. The expected signal is the same core sections 1-10 drive,
       times the row's headroom divisor — the only thing the shell adds.
       Stating it as an explicit factor is what makes a mismatch in EITHER
       direction visible. */
    constexpr double kHeadroom = 1.0 / 1.425;
    struct Case { int wave; int key; double tone; };
    const Case cases[] = {{SubOscCore::kSaw, 45, 20000}, {SubOscCore::kPulse, 33, 1200},
                          {SubOscCore::kBump, 28, 6000}, {SubOscCore::kNoise, 60, 900}};
    for (const Case &cs : cases)
    {
      const auto sets = with(kSwarmSilent, {{kSubOnId, 1},
                                            {kSubIdBase + 0, (double)cs.wave},
                                            {kSubIdBase + 7, 1.0},
                                            {kSubIdBase + 10, cs.tone},
                                            {kSubIdBase + 4, 0.0}});
      const std::vector<float> got = shellRender(48000, 256, 24, cs.key, sets);
      SubOscCore c = make(48000, {{SubOscCore::kWave, (double)cs.wave},
                                  {SubOscCore::kLevel, 1.0},
                                  {SubOscCore::kTone, cs.tone},
                                  {SubOscCore::kOctave, 0}});
      c.noteOn(cs.key, 1.0);
      std::vector<float> want;
      for (int b = 0; b < 24; b++)
        for (float v : pull(c, 256)) want.push_back((float)(v * kHeadroom));
      double num = 0, den = 0;
      for (size_t i = 0; i < want.size(); i++)
      {
        const double d = (double)got[i] - (double)want[i];
        num += d * d;
        den += (double)want[i] * want[i];
      }
      const double rr = std::sqrt(num / (double)want.size());
      row(rr <= 1e-6 && den > 0,
          "11c the row reproduces the CORE through the shell at the L0-1 bar", "rms %.3e", rr);
    }
    /* CALIBRATION (L0032): the same comparison against a core WITHOUT the
       headroom factor must FAIL. Without it, 11c would also pass for a shell
       that dropped the divisor — the detector would share the assumption. */
    {
      const auto sets =
          with(kSwarmSilent, {{kSubOnId, 1}, {kSubIdBase + 7, 1.0}, {kSubIdBase + 4, 0.0}});
      const std::vector<float> got = shellRender(48000, 256, 8, 45, sets);
      SubOscCore c = make(48000, {{SubOscCore::kLevel, 1.0}, {SubOscCore::kOctave, 0}});
      c.noteOn(45, 1.0);
      double num = 0;
      size_t n = 0;
      for (int b = 0; b < 8; b++)
        for (float v : pull(c, 256)) { const double d = (double)got[n++] - (double)v; num += d * d; }
      row(std::sqrt(num / (double)n) > 1e-6,
          "11c CALIBRATION: the same comparison against an UNSCALED core fails — 11c is "
          "measuring the headroom law, not an accidental match",
          "rms %.3e", std::sqrt(num / (double)n));
    }

    // ---- 11.d the chunk carries every new id, and the layout marker.
    {
      ShellRig a;
      a.boot(48000, 256);
      // Every row driven OFF its default, inside its declared range, so a key
      // that silently fails to round-trip cannot hide behind equality.
      const double want[kSubIdCount] = {SubOscCore::kBump, 0.31, 0.55,  1.75,   -2,
                                        7,                 -42.5, 0.37, 0.62,   0,
                                        311,               1,     0.041, 1.37,  987654,
                                        1,
                                        // B181's shell rows: mono, bias, glide, pitchMod
                                        1, 2, 0.75, -17.5};
      std::vector<std::pair<clap_id, double>> odd;
      for (int i = 0; i < kSubIdCount; i++) odd.push_back({(clap_id)(kSubIdBase + i), want[i]});
      a.block(-1, odd, 2);
      std::vector<char> buf(1 << 18);
      hypersaw_debug_state(a.p, buf.data(), (uint32_t)buf.size());
      const std::string chunk(buf.data());
      static const char *const addr[kSubIdCount] = {
          "sub.wave", "sub.width",  "sub.bumpAmt", "sub.bumpPhase", "sub.octave", "sub.semis",
          "sub.fine", "sub.level",  "sub.phase",   "sub.keytrack",  "sub.tone",   "sub.sync",
          "sub.attack", "sub.release", "sub.seed", "sub.on",
          "sub.mono", "sub.bias", "sub.glide", "sub.pitchMod"};
      // morphLayout 7 = B181: sub.glide and sub.pitchMod are MORPHABLE, so
      // they append to the corner array and its order changed again.
      bool keys = chunk.find("\"morphLayout\":7") != std::string::npos;
      for (const char *k : addr)
        keys = keys && chunk.find(std::string("\"") + k + "\"") != std::string::npos;
      row(keys, "11d the state chunk carries all 20 prefixed keys and morphLayout 7");

      ShellRig b;
      b.boot(48000, 256);
      hypersaw_debug_apply(b.p, chunk.c_str());
      b.block(-1, {}, 2);   // drain the param queue
      bool same = true;
      for (int i = 0; i < kSubIdCount; i++)
        same = same && b.read((clap_id)(kSubIdBase + i)) == a.read((clap_id)(kSubIdBase + i));
      row(same, "11d a fresh instance restores all 20 ids exactly");
      // CONTROL: the same chunk with the sub keys STRIPPED must NOT restore
      // them — otherwise "restored" could mean "both happened to be default".
      std::string stripped = chunk;
      for (const char *k : addr)
      {
        const std::string needle = std::string("\"") + k + "\"";
        for (size_t q = stripped.find(needle); q != std::string::npos;
             q = stripped.find(needle))
        {
          const size_t e = stripped.find(',', q);
          stripped.erase(q, (e == std::string::npos ? stripped.size() : e + 1) - q);
        }
      }
      ShellRig cc;
      cc.boot(48000, 256);
      hypersaw_debug_apply(cc.p, stripped.c_str());
      cc.block(-1, {}, 2);
      bool differs = false;
      for (int i = 0; i < kSubIdCount; i++)
        differs =
            differs || cc.read((clap_id)(kSubIdBase + i)) != a.read((clap_id)(kSubIdBase + i));
      row(differs, "11d CONTROL: a chunk with the sub keys removed does NOT restore them");
      a.kill();
      b.kill();
      cc.kill();
    }

    /* ---- 11.e HARD SYNC'S REFUSAL, PINNED (L0036).
       SPEC-SUBOSC §6 wants oscillator 1's per-sample fundamental phase; the
       swarm publishes none, so the shell passes nullptr and `sync` is INERT.
       A deliberate absence needs a test or it becomes an accidental presence.
       The day the master phase is wired this row goes red and names itself. */
    {
      const auto syncOff = with(kSwarmSilent, {{kSubOnId, 1}, {kSubIdBase + 7, 1.0}});
      const auto syncOn = with(syncOff, {{kSubIdBase + 11, 1.0}});
      row(firstDiff(shellRender(48000, 256, 8, 40, syncOff),
                    shellRender(48000, 256, 8, 40, syncOn)) < 0,
          "11e REFUSAL pinned: hard sync is not wired — sync ON renders bit-identically to "
          "sync OFF");
    }

    /* ==== 11.g THE SUB'S OWN MONO, BIAS AND GLIDE (B181 note 2) ============
       The human: "Sub should have its own mono toggle, with 'lowest' as the
       MIDI bias and a simple glide knob."

       Measured as PITCH out of the plugin, never as a state read: a per-voice
       getter would test the accessor, which is how a suite ends up agreeing
       with itself through a broken one (mpe_check's note). The sub is driven
       as a sine with both swarm oscillators off, so the output IS its row and
       the sounding frequency is a zero-crossing count. */
    {
      auto subSine = [&](std::vector<std::pair<clap_id, double>> extra) {
        return with(with(kSwarmSilent, {{kSubOnId, 1},
                                        {kSubIdBase + 0, SubOscCore::kSine},
                                        {kSubIdBase + 7, 1.0},
                                        {kSubIdBase + 4, 0.0},      // octave 0: audible
                                        {kSubIdBase + 12, 0.0005}}),  // attack: settle fast
                    extra);
      };
      /* 11g.0 CONTROL — THE DEFAULTS ARE BIT-INERT. With `sub.mono` off, the
         bias and the glide time are not allowed to move one sample; with
         `sub.pitchMod` at its default, naming it must be the same as not
         naming it. Without this row the three new parameters could be
         reaching the audio path when they are supposed to be asleep. */
      {
        const auto plain = subSine({});
        const auto armed = subSine({{kSubBiasId, 2}, {kSubGlideId, 2.0}, {kSubPitchModId, 0.0}});
        row(firstDiff(shellRender(48000, 256, 16, 45, plain),
                      shellRender(48000, 256, 16, 45, armed)) < 0,
            "11g.0 CONTROL: with mono OFF, bias and glide change not one sample, and pitchMod "
            "at 0 is the same as absent");
        // CALIBRATION: the same comparison with mono ON and a second key would
        // differ — otherwise 11g.0 could be passing because nothing works.
        const auto live = subSine({{kSubMonoId, 1}, {kSubPitchModId, 7.0}});
        row(firstDiff(shellRender(48000, 256, 16, 45, plain),
                      shellRender(48000, 256, 16, 45, live)) >= 0,
            "11g.0 CALIBRATION: pitchMod at 7 st DOES differ — the comparison can fail");
      }
      /* 11g.a/b MONO AND ITS BIAS. Three keys down at once (48, 60, 55, in
         that order): poly sounds all three, mono sounds exactly ONE, and WHICH
         one is the bias's whole job. Goertzel at each of the three
         fundamentals is the reading; the two silent ones are the control that
         must read ~0 in the same measurement (L0032). */
      {
        const std::vector<int> press = {48, 0, 60, 0, 55, 0, 0, 0};
        std::vector<int> seq;
        for (int i = 0; i < 8; i++) seq.push_back(press[i]);
        for (int i = 0; i < 40; i++) seq.push_back(0);   // ~0.21 s of steady tone
        auto mags = [&](std::vector<std::pair<clap_id, double>> extra, double m[3]) {
          ShellRig r;
          r.boot(48000, 256);
          const std::vector<float> y = r.sequence(subSine(extra), seq);
          r.kill();
          const std::vector<float> tail(y.end() - 40 * 256, y.end());
          const int keys[3] = {48, 60, 55};
          for (int i = 0; i < 3; i++)
            m[i] = goertzel(tail, 48000, 440 * std::pow(2.0, (keys[i] - 69) / 12.0));
        };
        double poly[3], lo[3], hi[3], last[3];
        mags({}, poly);
        mags({{kSubMonoId, 1}, {kSubBiasId, 0}}, lo);
        mags({{kSubMonoId, 1}, {kSubBiasId, 1}}, hi);
        mags({{kSubMonoId, 1}, {kSubBiasId, 2}}, last);
        row(poly[0] > 0.02 && poly[1] > 0.02 && poly[2] > 0.02,
            "11g.a CONTROL: with mono OFF the sub sounds all THREE held keys", "weakest %.4f",
            std::min(poly[0], std::min(poly[1], poly[2])));
        row(lo[0] > 0.05 && lo[1] < 0.005 && lo[2] < 0.005,
            "11g.a mono + bias LOWEST sounds 48 and only 48", "48 %.4f", lo[0]);
        row(hi[1] > 0.05 && hi[0] < 0.005 && hi[2] < 0.005,
            "11g.b mono + bias HIGHEST sounds 60 and only 60", "60 %.4f", hi[1]);
        row(last[2] > 0.05 && last[0] < 0.005 && last[1] < 0.005,
            "11g.b mono + bias LAST sounds 55 (the most recent press) and only 55", "55 %.4f",
            last[2]);
      }
      /* 11g.c THE GLIDE, in SECONDS. Press 36, then 48 a tenth of a second
         later: the pitch must be BETWEEN the two shortly after the second
         press and AT 48 once the glide time has elapsed. The must-read-
         differently control is the same sequence at glide 0, which has to be
         at 48 immediately — without it this row would pass for a shell that
         simply jumped and took its time about being measured. */
      {
        auto glideSeq = [&](double glide, double *early, double *late) {
          std::vector<int> seq = {36};
          for (int i = 0; i < 18; i++) seq.push_back(0);   // 0.1 s
          seq.push_back(48);
          for (int i = 0; i < 200; i++) seq.push_back(0);  // 1.07 s
          ShellRig r;
          r.boot(48000, 256);
          const std::vector<float> y =
              r.sequence(subSine({{kSubMonoId, 1}, {kSubBiasId, 2}, {kSubGlideId, glide}}), seq);
          r.kill();
          // Blocks 26..38 = 0.04..0.10 s after the retarget (glide 0.5 s, so
          // ~10-20 % of the journey); blocks 180..219 = well past it.
          *early = sineHz(y, 48000, 26 * 256, 38 * 256);
          *late = sineHz(y, 48000, 180 * 256, 219 * 256);
        };
        const double f36 = 440 * std::pow(2.0, (36 - 69) / 12.0);
        const double f48 = 440 * std::pow(2.0, (48 - 69) / 12.0);
        double gEarly = 0, gLate = 0, sEarly = 0, sLate = 0;
        glideSeq(0.5, &gEarly, &gLate);
        glideSeq(0.0, &sEarly, &sLate);
        row(gEarly > f36 * 1.02 && gEarly < f48 * 0.90,
            "11g.c glide 0.5 s: 40-100 ms after the new key the pitch is BETWEEN the two",
            "%.2f Hz (36 is 65.41, 48 is 130.81)", gEarly);
        row(std::fabs(gLate - f48) < f48 * 0.02,
            "11g.c glide 0.5 s: past the glide time it has ARRIVED at the new key", "%.2f Hz",
            gLate);
        row(std::fabs(sEarly - f48) < f48 * 0.02,
            "11g.c CONTROL glide 0 s snaps — the same window is already at the new key",
            "%.2f Hz", sEarly);
        row(std::fabs(sLate - f48) < f48 * 0.02, "11g.c CONTROL glide 0 s stays there",
            "%.2f Hz", sLate);
      }
      /* 11g.d THE GLIDE IS IN SECONDS, NOT SAMPLES (ADR-009 / the rule
         samplerate_check enforces for the instrument). The same gesture at
         44.1 and at 96 kHz must be at the same place at the same TIME. */
      {
        /* THE WINDOW IS A TIME AT BOTH RATES, and that is not pedantry: the
           first version of this row measured a fixed number of BLOCKS, which
           is 226 ms at 44.1 kHz and 104 ms at 96 kHz. The pitch is still
           moving inside it, so the two averages differed by 9.5 % and the row
           went red on its own measurement rather than on the shell. Sample
           indices, derived from the rate, everywhere. */
        auto atTime = [&](double sr, double secs) {
          const int pre = (int)(0.1 * sr / 256);           // ~0.1 s before the retarget
          const int tail = (int)((secs + 0.15) * sr / 256) + 2;
          std::vector<int> seq = {36};
          for (int i = 0; i < pre; i++) seq.push_back(0);
          seq.push_back(48);
          for (int i = 0; i < tail; i++) seq.push_back(0);
          ShellRig r;
          r.boot(sr, 256);
          const std::vector<float> y = r.sequence(
              subSine({{kSubMonoId, 1}, {kSubBiasId, 2}, {kSubGlideId, 0.5}}), seq);
          r.kill();
          const size_t retarget = (size_t)(pre + 1) * 256;
          const size_t at = retarget + (size_t)(secs * sr);
          return sineHz(y, sr, at, at + (size_t)(0.06 * sr));   // 60 ms, both rates
        };
        const double a = atTime(44100, 0.25), b = atTime(96000, 0.25);
        row(std::fabs(a - b) < std::max(a, b) * 0.03,
            "11g.d the glide is a TIME, not a sample count — same pitch at 0.25 s at 44.1 and "
            "96 kHz",
            "44.1 kHz %.2f Hz", a);
        std::printf("   (96 kHz read %.2f Hz at the same instant)\n", b);
      }
    }

    /* ==== 11.h THE SUB AS A PITCH-ENVELOPE DESTINATION (B181 note 4) =======
       The human: "Sub needs one of its pitch parameters to also be mappable
       smoothly to a pitch envelope."

       WHAT ALREADY WORKED, ASKED RATHER THAN ASSUMED. `sub.fine` is
       continuous, it is in the block, and nothing in modAddRoute or in the
       GUI's modDestOptions refuses it — so the capability was there and the
       gap was RANGE: +/-100 cents is ONE SEMITONE, which is not a pitch
       envelope. 11h.a measures that it routes and 11h.b measures how far it
       can go, so the premise of the fix is evidence and not a reading.

       THE FIX is a dedicated `sub.pitchMod` in SEMITONES over the
       instrument's own pitch range (+/-48, ADR-135's clamp), which leaves
       `fine` alone — every stored value of it keeps its pitch — and costs no
       recalc() on the audio-parameter path (`fine` is a core table row, so
       modulating IT reruns the BUMP peak search on all sixteen instances
       every mod tick; `pitchMod` is a single store).

       Routes are installed through the state chunk, which is the transport
       the shell already has — a second debug hook for the same job would be
       one more thing to keep in step. Source 14 is VELOCITY (ADR-149), which
       is the one global source that is live with both swarm oscillators off;
       ENV 1 follows the ENABLED oscillators' amp envelope and would read 0
       in exactly the rig that makes the sub measurable on its own. */
    {
      auto routed = [&](const char *chunk, std::vector<std::pair<clap_id, double>> extra,
                        int key) {
        ShellRig r;
        r.boot(48000, 256);
        std::string j = "{\"params\":{},\"modRoutes\":\"";
        j += chunk;
        j += "\"}";
        hypersaw_debug_apply(r.p, j.c_str());
        r.block(-1, {}, 2);   // drain the load's queue before the patch lands
        const auto sets = with(with(kSwarmSilent, {{kSubOnId, 1},
                                                   {kSubIdBase + 0, SubOscCore::kSine},
                                                   {kSubIdBase + 7, 1.0},
                                                   {kSubIdBase + 4, 0.0},
                                                   {kSubIdBase + 12, 0.0005}}),
                               extra);
        std::vector<int> seq = {key};
        for (int i = 0; i < 60; i++) seq.push_back(0);
        const std::vector<float> y = r.sequence(sets, seq);
        r.kill();
        return sineHz(y, 48000, 40 * 256, 60 * 256);   // steady, past the ramp
      };
      const double f48 = 440 * std::pow(2.0, (48 - 69) / 12.0);
      // 11h.a `fine` ALREADY routes. Depth is a fraction of the destination's
      // own range (200 cents), so 0.25 asks for +50 c = a factor 1.02930.
      const double fineOn = routed("14:4006:0.25;", {}, 48);
      const double fineOff = routed("", {}, 48);
      row(std::fabs(fineOff - f48) < f48 * 0.01 &&
              std::fabs(fineOn - f48 * std::pow(2.0, 50.0 / 1200.0)) < f48 * 0.01,
          "11h.a `sub.fine` was ALREADY a mod destination — velocity at depth 0.25 moves it "
          "+50 cents",
          "%.3f Hz", fineOn);
      // 11h.b AND THAT IS THE GAP. `fine` at its ceiling is one semitone; a
      // pitch envelope is not one semitone. Stated as a measurement so the
      // reason for a new parameter is on the record.
      const double fineMax = routed("14:4006:0.5;", {}, 48);
      row(std::fabs(fineMax - f48 * 2.0) > f48 * 0.05,
          "11h.b THE GAP: `fine` at FULL depth is +100 c — one semitone, not a pitch envelope",
          "%.3f Hz (an octave would be 261.63)", fineMax);
      /* 11h.c THE FIX, MEASURED. `sub.pitchMod` spans 96 st, so depth 0.125
         asks for exactly +12 st — one octave, a number a wrong answer cannot
         land on by accident. The control is the same route at depth 0. */
      const double modOn = routed("14:4019:0.125;", {}, 48);
      const double modOff = routed("14:4019:0;", {}, 48);
      row(std::fabs(modOn - f48 * 2.0) < f48 * 0.02,
          "11h.c a source routed to `sub.pitchMod` at depth 0.125 moves the sub by exactly "
          "+12 st",
          "%.3f Hz (want 261.63)", modOn);
      row(std::fabs(modOff - f48) < f48 * 0.01,
          "11h.c CONTROL the same route at depth 0 leaves the pitch alone", "%.3f Hz", modOff);
      // 11h.d a NEGATIVE depth goes DOWN — the range is signed, not a magnitude.
      const double modDown = routed("14:4019:-0.125;", {}, 48);
      row(std::fabs(modDown - f48 * 0.5) < f48 * 0.02,
          "11h.d depth -0.125 moves it an octave DOWN", "%.3f Hz (want 65.41)", modDown);
      /* 11h.e SMOOTH, and that is the word the human used. The parameter is
         continuous (not CLAP-stepped), and a sweep of it must produce
         strictly increasing, all-distinct frequencies — a quantised
         destination would land on repeats. 16 steps of 0.75 st. */
      {
        ShellRig q;
        q.boot(48000, 256);
        clap_param_info_t info{};
        bool got = false;
        for (uint32_t x = 0, n2 = q.params->count(q.p); x < n2 && !got; x++)
          if (q.params->get_info(q.p, x, &info) && info.id == kSubPitchModId) got = true;
        row(got && (info.flags & CLAP_PARAM_IS_STEPPED) == 0 && info.min_value == -48 &&
                info.max_value == 48 && info.default_value == 0,
            "11h.e `sub.pitchMod` is CONTINUOUS over +/-48 st, defaulting to 0");
        q.kill();
        double prev = 0;
        bool rising = true, distinct = true;
        // 0.7 st, deliberately NOT a whole semitone: a destination that
        // quantised to semitones would still rise on a 1-st grid.
        for (int i = 0; i <= 16; i++)
        {
          const double f = routed("", {{kSubPitchModId, i * 0.7}}, 48);
          if (i && (f <= prev * 1.0001)) rising = false;
          if (i && std::fabs(f - prev) < 1e-3) distinct = false;
          prev = f;
        }
        const double top = f48 * std::pow(2.0, 11.2 / 12.0);
        row(rising && distinct && std::fabs(prev - top) < top * 0.01,
            "11h.e sweeping it in 0.7 st steps gives 17 strictly rising, all-distinct "
            "frequencies and lands where the law says — no quantisation",
            "top %.3f Hz (want 249.69)", prev);
      }
    }

    /* ---- 11.f HEADROOM, measured THROUGH THE SHELL and gated. Phase 1
       finding 1: the core's TPT tone stage overshoots above unity at a
       near-Nyquist cutoff. The row divides by 1.425 so a sub at level 1 into a
       unity path cannot reach the rail. This measures the ROW, not the core. */
    {
      double worst = 0;
      int worstWave = -1, worstKey = -1;
      for (int w = 0; w < SubOscCore::kWaveCount; w++)
        for (int key : {12, 24, 36, 48, 60, 72})
        {
          const auto sets = with(kSwarmSilent, {{kSubOnId, 1},
                                                {kSubIdBase + 0, (double)w},
                                                {kSubIdBase + 7, 1.0},
                                                {kSubIdBase + 10, 20000.0},
                                                {kSubIdBase + 4, 0.0},
                                                {kSubIdBase + 1, 0.05},
                                                {kSubIdBase + 2, 0.6},
                                                {kSubIdBase + 12, 0.0005}});
          const double pk = peakOf(shellRender(48000, 256, 24, key, sets));
          if (pk > worst) { worst = pk; worstWave = w; worstKey = key; }
        }
      row(worst <= 1.0,
          "11f the SUB row at level 1 cannot exceed unity — the headroom law holds as a "
          "MEASUREMENT", "worst peak %.4f", worst);
      std::printf("     worst case: wave %d, MIDI %d, tone 20 kHz, level 1, 48 kHz\n",
                  worstWave, worstKey);
      row(worst * 1.425 > 1.0,
          "11f CALIBRATION: the same render WITHOUT the divisor exceeds unity — the bound "
          "is doing work, not describing a signal that was already quiet",
          "undivided peak %.4f", worst * 1.425);
    }
  }

  // ==================================================================== 10. CPU
  {
    // REPORTED, NEVER GATED — the absolute number is machine-dependent.
    // SPEC-SUBOSC §10.7 lists "CPU per voice" as still to measure before a
    // port; this is that measurement, taken on the standalone core. 16
    // INSTANCES, because §1/§8 make the sub a PER-VOICE source: 16 voices of
    // the device means 16 of these, not one rendering 16 notes.
    const int secs = 5, N = 48000 * secs;
    double best = 1e30;
    for (int t = 0; t < 3; t++)
    {
      std::vector<SubOscCore> v;
      v.reserve(16);
      for (int i = 0; i < 16; i++)   // VOICES, not ids — hypersaw::kPoly's worth
      {
        v.push_back(make(48000, {{SubOscCore::kWave, SubOscCore::kPulse},
                                 {SubOscCore::kWidth, 0.27},
                                 {SubOscCore::kTone, 1200},
                                 {SubOscCore::kSync, 1}}));
        v.back().noteOn(36 + i, 1);
      }
      const std::vector<float> master = masterPhase(110, 48000, 256);
      std::vector<float> L(256), R(256);
      const auto t0 = std::chrono::steady_clock::now();
      for (int off = 0; off < N; off += 256)
      {
        const int k = std::min(256, N - off);
        for (auto &c : v) c.render(L.data(), R.data(), k, master.data());
      }
      best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    const double pct = best / secs * 100;
    std::printf("\n-- CPU (REPORT, not gated) --\n");
    std::printf("   16 instances (16 voices' worth), pulse + tone + hard sync, 48 kHz, 5 s, "
                "min of 3: %.4f s  =  %.3f %% of one core\n", best, pct);
    std::printf("   SPEC-SUBOSC §10.7 claims no budget; this is the first measurement. It is "
                "load-sensitive — treat a single reading as a sample, not as the figure.\n");
  }

  // ================================= 12. CPU THROUGH THE SHELL (B172, REPORTED)
  /* What the ROADMAP needs is not the core's cost but the DEVICE's: sixteen
     voices of the real plugin with the sub on against the same thing with it
     off. Both figures come from the same rig and the same notes, so the
     DIFFERENCE is the row's bill and nothing else. REPORTED, never gated. */
  {
    auto bench = [](bool on) {
      const int blocks = 48000 * 5 / 256;
      double best = 1e30;
      for (int t = 0; t < 3; t++)
      {
        ShellRig r;
        r.boot(48000, 256);
        std::vector<std::pair<clap_id, double>> sets = {
            {kSubIdBase + 0, SubOscCore::kPulse}, {kSubIdBase + 1, 0.27},
            {kSubIdBase + 10, 1200.0},            {kSubIdBase + 7, 0.8},
            {kSubOnId, on ? 1.0 : 0.0}};
        // Sixteen voices: one note per slot, struck before the clock starts.
        r.block(-1, sets, 1);
        for (int v = 0; v < 16; v++) r.block(36 + v, {}, 1);
        const auto t0 = std::chrono::steady_clock::now();
        r.block(-1, {}, blocks);
        best = std::min(
            best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        r.kill();
      }
      return best;
    };
    const double off = bench(false), on = bench(true);
    std::printf("\n-- CPU through the SHELL (REPORT, not gated) --\n");
    std::printf("   16 voices, 5 s at 48 kHz, min of 3 — sub OFF %.4f s (%.2f %% of one core), "
                "sub ON %.4f s (%.2f %%)\n", off, off / 5 * 100, on, on / 5 * 100);
    std::printf("   the sub's bill: %+.2f percentage points (%.2fx)\n",
                (on - off) / 5 * 100, off > 0 ? on / off : 0.0);
    std::printf("   NB the sub's parameter writes run recalc() on all sixteen instances "
                "(282 transcendentals each, phase 1 finding 4) — that is a CONTROL-path "
                "cost this steady-state number does not contain.\n");
  }

  std::printf("\nsubosc_check: %s (%d failure%s; worst parity rms %.3e)\n",
              g_failures ? "RED" : "GREEN", g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
