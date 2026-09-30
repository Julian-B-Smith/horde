/*
 * swarm_sr_parity.h — the body of swarm48_check (src/swarm_core.h) and h2_swarm48_check (the lifted
 * copy, h2/cores/swarm/): the swarm core against the JS reference at 48 kHz, every scenario reported.
 * HYPERSAW, 2026-09-30, ROADMAP B382 (the human's ruling on B379: "a 48 kHz golden set").
 *
 * ONE BODY, TWO TUs, ON PURPOSE. h2_rules_check rule 4 forbids a TU that includes both the legacy
 * core and its h2 copy, so each check is a two-line .cpp that includes ITS core and defines SWARM_NS
 * (the namespace that holds SwarmCore, kTick and kKsmTauSeconds) and SWARM_TOOL (its name in the
 * output), then includes this file, which includes no core.
 *
 * WHAT IT DOES. Reads build-golden/sr48000/ (tools/golden/gen_goldens_sr.mjs; the header there says
 * what each file is) and, per scenario, renders the C++ core under parity_check's protocol at 48 kHz
 * (A3, 4 s, 1024-sample blocks, note-off at the first block at or after 3 s, params in the manifest's
 * order) and compares it with BOTH goldens:
 *   MIRRORED  SwarmSynth with every divergence the composed engine registers ON (the lab's law);
 *   PLAIN     SwarmSynth as the reference has it (DynSynth for the dyn scenarios).
 * The verdict is on the MIRRORED golden and the manifest's expectation:
 *   match        rms < 1e-6, or the row is an UNEXPECTED DIFFERENCE (red): something no ledgered
 *                divergence explains;
 *   differ:<why> rms >= 1e-6, or the expectation is STALE (red): a known, named difference that
 *                vanished must be re-read, not silently kept.
 * Every scenario is printed with both numbers, and a match whose PLAIN rms is beyond 1e-6 is marked
 * `(mirrored)`: that difference from SwarmSynth IS a ledgered divergence (the lab's law explains it).
 * THE COEFFICIENT (M1): for each rate in ksm.tsv, the C++ constructor's expression for the coupling
 * smoother's per-tick coefficient (swarm_core.h's SwarmCore(), B150: 0.08 at 44.1 kHz, else
 * 1 - exp(-(kTick/sr)/kKsmTauSeconds), here with this TU's libm) must equal the composed engine's, bit
 * for bit. The expression is the constructor's, verbatim, over the core's own constants.
 * Exit 1 on any red row. Deterministic; no clock.
 */
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace swarm_sr
{
constexpr double kSR = 48000.0;
constexpr int kSeconds = 4;
constexpr int kNoteOffAt = 3;
constexpr int kBlock = 1024;
constexpr double kEps = 1e-6;
inline double mtof(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

struct Err { double rms = 0, maxAbs = 0; bool ok = true; std::string why; };

/* one scenario on the C++ core, compared with a golden file (stereo-interleaved float32) */
inline Err compare(const std::string &dir, const std::string &file, const std::string &paramList,
                   const std::vector<int> &notes)
{
  Err r;
  SWARM_NS::SwarmCore core(kSR);
  std::stringstream ss(paramList);
  std::string kv;
  while (std::getline(ss, kv, ','))
  {
    const auto eq = kv.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = kv.substr(0, eq);
    if (!core.setParam(key, std::atof(kv.c_str() + eq + 1))) { r.ok = false; r.why = "unknown param " + key; return r; }
  }
  for (int m : notes) core.noteOn(m, mtof(m));
  std::ifstream gf(dir + "/" + file, std::ios::binary);
  if (!gf) { r.ok = false; r.why = "missing golden " + file; return r; }
  const long total = (long)(kSeconds * kSR);
  std::vector<float> L(kBlock), R(kBlock), g(kBlock * 2);
  double sum = 0;
  long n = 0;
  bool off = false;
  for (long o = 0; o < total; o += kBlock)
  {
    if (!off && o >= (long)(kNoteOffAt * kSR)) { for (int m : notes) core.noteOff(m); off = true; }
    core.render(L.data(), R.data(), kBlock);
    const long nf = std::min((long)kBlock, total - o);
    gf.read(reinterpret_cast<char *>(g.data()), nf * 2 * sizeof(float));
    if (gf.gcount() != (std::streamsize)(nf * 2 * sizeof(float))) { r.ok = false; r.why = "golden truncated " + file; return r; }
    for (long i = 0; i < nf; i++)
    {
      const double dl = (double)L[i] - (double)g[i * 2], dr = (double)R[i] - (double)g[i * 2 + 1];
      sum += dl * dl + dr * dr;
      n += 2;
      r.maxAbs = std::max(r.maxAbs, std::max(std::fabs(dl), std::fabs(dr)));
    }
  }
  r.rms = std::sqrt(sum / (double)n);
  return r;
}

/* the constructor's coefficient expression (swarm_core.h SwarmCore(), B150), verbatim */
inline double ksmC(double sampleRate)
{
  using namespace SWARM_NS;
  return sampleRate == 44100.0 ? 0.08 : 1 - std::exp(-((double)kTick / sampleRate) / kKsmTauSeconds);
}

inline int run(int argc, char **argv)
{
  if (argc != 2) { std::fprintf(stderr, "usage: %s <build-golden/sr48000>\n", SWARM_TOOL); return 64; }
  const std::string dir = argv[1];
  std::ifstream tsv(dir + "/manifest.tsv");
  if (!tsv) { std::fprintf(stderr, "%s: cannot open %s/manifest.tsv (run tools/golden/gen_goldens_sr.mjs first)\n", SWARM_TOOL, dir.c_str()); return 1; }
  int red = 0, count = 0, match = 0, mirrored = 0, known = 0;
  double worst = 0;
  std::string worstName, line;
  while (std::getline(tsv, line))
  {
    if (line.empty()) continue;
    std::vector<std::string> c;
    { std::stringstream ss(line); std::string x; while (std::getline(ss, x, '\t')) c.push_back(x); }
    if (c.size() != 8) { std::fprintf(stderr, "%s: malformed manifest line: %s\n", SWARM_TOOL, line.c_str()); return 1; }
    std::vector<int> notes;
    { std::stringstream ss(c[5]); std::string x; while (std::getline(ss, x, '+')) notes.push_back(std::atoi(x.c_str())); }
    const std::string &name = c[0], &expect = c[7];
    const Err m = compare(dir, c[2], c[4], notes), p = c[6] == c[2] ? m : compare(dir, c[6], c[4], notes);
    if (!m.ok || !p.ok) { std::fprintf(stderr, "%s: %s: %s\n", SWARM_TOOL, name.c_str(), (!m.ok ? m.why : p.why).c_str()); return 1; }
    count++;
    const bool within = m.rms < kEps, want = expect == "match";
    const char *tag;
    if (want && within) { tag = "OK  "; match++; if (p.rms >= kEps) mirrored++; }
    else if (!want && !within) { tag = "KNOWN"; known++; }
    else { tag = "FAIL"; red++; }
    if (want && m.rms > worst) { worst = m.rms; worstName = name; }
    std::printf("%s %-28s mirrored rms=%.3e max=%.3e | plain rms=%.3e%s%s%s\n", tag, name.c_str(), m.rms, m.maxAbs, p.rms,
                want && within && p.rms >= kEps ? "  (mirrored)" : "",
                want && !within ? "  UNEXPECTED DIFFERENCE (no ledgered divergence explains it)" : "",
                !want ? (within ? ("  STALE: expected " + expect + ", now within eps").c_str() : ("  " + expect).c_str()) : "");
  }
  /* M1's coefficient, rate by rate, bit for bit */
  std::ifstream kt(dir + "/ksm.tsv");
  int krows = 0;
  if (!kt) { std::printf("FAIL ksm.tsv missing: the engine registers no M1 coefficient to compare\n"); red++; }
  while (kt && std::getline(kt, line))
  {
    if (line.empty()) continue;
    const auto tab = line.find('\t');
    const double sr = std::atof(line.substr(0, tab).c_str()), js = std::strtod(line.c_str() + tab + 1, nullptr), cpp = ksmC(sr);
    const bool ok = std::memcmp(&js, &cpp, sizeof js) == 0;
    if (!ok) red++;
    krows++;
    std::printf("%s ksmC @ %6.0f Hz  C++ %.17g  JS %.17g\n", ok ? "OK  " : "FAIL", sr, cpp, js);
  }
  std::printf("%s: %d scenarios at 48 kHz: %d within eps=%.0e of the mirrored reference (%d of them only because a "
              "ledgered divergence is mirrored; worst %.3e @ %s), %d known differences, %d red; %d coefficient rows\n",
              SWARM_TOOL, count, match, kEps, mirrored, worst, worstName.c_str(), known, red, krows);
  return red ? 1 : 0;
}
}  // namespace swarm_sr

int main(int argc, char **argv) { return swarm_sr::run(argc, argv); }
