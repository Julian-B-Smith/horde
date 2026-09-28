/* onset_ref_check — B335: the ensemble timing of src/swarm_core.h, rendered by the C++ itself, as
 * the reference for its JS transcription in the composed engine.
 * WIRED: ./verify full.
 *
 * HYPERSAW, 2026-09-28, ROADMAP B335 (records PR #831, branch lead-records-133). The human:
 * "Have we ported gravity over from the original engine? Or the ensemble voice lag correction
 * behavior?" Onset scatter, timing correction, attack scatter, per-partial envelopes and release
 * scatter (ADR-077, ADR-078, B149) exist ONLY in the legacy C++; there is no JS reference. The
 * composed engine (docs/design/scalpel-horde-engine.js) transcribes them, so the C++ is the oracle
 * and the parity is C++ <-> JS.
 *
 * WHAT THIS DOES. It drives SwarmCore directly (swarm_core.h only: no shell, no impl library, the
 * ncap_check pattern) through fixed, seeded scenarios and writes what it measured as JSON:
 *   serial   40 note-ons per run at 48 kHz, no render between them (the draws happen at note-on):
 *            per note, every member's persistent offset tOff (ensembleTiming()), its onset delay
 *            onsD0 in samples, and (one run) its attack and release coefficients onsC / relC.
 *            Timing correction 0 / 0.25 / 1 / 1.5 at seed 1234, and 0.25 at seed 99.
 *   render   44.1 kHz (the rate where swarm_core.h's coupling smoother is the reference's own
 *            0.08, so the swarm itself is SwarmSynth's), one sample per call:
 *              ens      onset + attack scatter, two notes: every member's ENTRY sample (its first
 *                       phase step), and at checkpoints its phase and entry ramp onsE.
 *              venv     per-partial envelopes with attack and release scatter: entries, and each
 *                       member's measured attack and release half-times (samples).
 *              coupled  K 0.35 with onset scatter: phases at checkpoints (waiting members still
 *                       sit in the mean field).
 *              os2      `ens` at the C++'s 2x oversampling: entries only. A FINDING, not a law:
 *                       the C++ counts the wait per SUB-sample, so its delays halve at 2x.
 * tools/labharness/composed_engine_check.mjs (its ONS rows) reads the committed JSON and compares
 * the transcription against it, with the tolerances and reasons stated there.
 *
 * WHY A COMMITTED FILE and not a live call: composed_engine_check runs in `verify full` before the
 * C++ build and with node alone; this tool, built by that same `verify full`, re-derives the file
 * and fails if the committed copy is STALE (byte comparison: same compiler, same libm, same
 * numbers). So the JS is compared with the C++ of the day on every full run, in two legs.
 *
 * Usage:  onset_ref_check --emit                 print the JSON (regenerate: > the fixture)
 *         onset_ref_check <fixture.json>         exit 1 unless the fixture is exactly this output
 * CALIBRATION (L0032): a fixture that is not this output must fail. The check mode plants a
 * one-byte change into a copy of its own output and requires the comparison to catch it.
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/swarm_core.h"

using hypersaw::SwarmCore;

namespace
{
std::string out;

void put(const char *s) { out += s; }
void num(double x)
{
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", x);
  out += b;
}
void arr(const double *x, int n)
{
  put("[");
  for (int i = 0; i < n; i++) { if (i) put(","); num(x[i]); }
  put("]");
}

constexpr int kN = 7;

/* the common swarm: 7 members, even distribution, law 0 at 20 cents, aligned start (retrig), so a
   member that has not entered reads phase exactly 0. The composed engine plays the same swarm at
   N 7, detune 20, dist 0, h.law 0, phaseMode 1. */
void common(SwarmCore &c, double seed)
{
  c.setParam("n", kN);
  c.setParam("dist", 0);
  c.setParam("law", 0);
  c.setParam("detune", 0.2);
  c.setParam("retrig", 1);
  c.setParam("seed", seed);
}

void serial()
{
  const double sr = 48000;
  struct Run { double alpha, seed; bool coefs; };
  const Run runs[] = {{0, 1234, false}, {0.25, 1234, true}, {1, 1234, false}, {1.5, 1234, false}, {0.25, 99, false}};
  const int notes = 40;
  put("\"serial\":{\"sr\":48000,\"n\":7,\"f\":220,\"scatter\":20,\"attackScatter\":0.5,\"relScatter\":0.5,\"A\":10,\"R\":100,\"notes\":40,\"runs\":[");
  bool first = true;
  for (const Run &r : runs)
  {
    SwarmCore c(sr);
    common(c, r.seed);
    c.setParam("onsetScatter", 20);
    c.setParam("onsetAlpha", r.alpha);
    c.setParam("attackScatter", 0.5);
    c.setParam("relScatter", 0.5);
    c.setParam("attack", 10 * 0.001);
    c.setParam("release", 100 * 0.001);
    std::vector<double> tOff, onsD0, onsC, relC;
    for (int k = 0; k < notes; k++)
    {
      const int midi = 57 + (k % 5);
      const int slot = c.noteOn(midi, 220.0);
      const auto e = c.ensembleTiming();
      const auto &v = c.voiceAt(slot);
      for (int i = 0; i < kN; i++)
      {
        tOff.push_back(e.off[i]);
        onsD0.push_back(v.onsD0[i]);
        onsC.push_back(v.onsC[i]);
        relC.push_back(v.relC[i]);
      }
      c.noteOff(midi);
    }
    if (!first) put(",");
    first = false;
    put("{\"alpha\":"); num(r.alpha); put(",\"seed\":"); num(r.seed);
    put(",\"tOff\":"); arr(tOff.data(), (int)tOff.size());
    put(",\"onsD0\":"); arr(onsD0.data(), (int)onsD0.size());
    if (r.coefs)
    {
      put(",\"onsC\":"); arr(onsC.data(), (int)onsC.size());
      put(",\"relC\":"); arr(relC.data(), (int)relC.size());
    }
    put("}");
  }
  put("]}");
}

struct Scen
{
  const char *name;
  double K, scatter, alpha, atkSc, relSc, venv, A, R, os;
  int on2;            // second note-on (samples), 0 = none
  int off1, off2;     // note-offs (samples)
  int total, cpEvery, cpUntil;
  bool trace, halves;
};

void render(const Scen &q, bool comma)
{
  const double sr = 44100;
  SwarmCore c(sr);
  common(c, 1234);
  c.setParam("K", q.K);
  c.setParam("onsetScatter", q.scatter);
  c.setParam("onsetAlpha", q.alpha);
  c.setParam("attackScatter", q.atkSc);
  c.setParam("relScatter", q.relSc);
  c.setParam("voiceEnv", q.venv);
  c.setParam("attack", q.A * 0.001);
  c.setParam("release", q.R * 0.001);
  c.setParam("sustain", 1);
  c.setParam("oversample", q.os);
  float l = 0, r = 0;
  std::vector<double> entries, cps, ph, oe;
  double t50a[kN], t50r[kN], eOff[kN];
  for (int i = 0; i < kN; i++) { t50a[i] = -1; t50r[i] = -1; eOff[i] = 0; }
  int slot1 = -1, slot2 = -1;
  int ent1[kN], ent2[kN];
  for (int i = 0; i < kN; i++) { ent1[i] = -1; ent2[i] = -1; }
  for (int t = 0; t < q.total; t++)
  {
    if (t == 0) slot1 = c.noteOn(57, 220.0);
    if (q.on2 && t == q.on2) slot2 = c.noteOn(57, 220.0);
    if (t == q.off1) { for (int i = 0; i < kN; i++) eOff[i] = c.voiceAt(slot1).onsE[i]; c.noteOff(57); }
    if (q.off2 && t == q.off2) c.noteOff(57);
    c.render(&l, &r, 1);
    const auto &v = c.voiceAt(slot1);
    for (int i = 0; i < kN; i++)
    {
      if (ent1[i] < 0 && v.phase[i] != 0.0) ent1[i] = t;
      if (q.halves)
      {
        if (t50a[i] < 0 && ent1[i] >= 0 && v.onsE[i] >= 0.5) t50a[i] = t - ent1[i];
        if (t50r[i] < 0 && t >= q.off1 && v.onsE[i] <= 0.5 * eOff[i]) t50r[i] = t - q.off1;
      }
    }
    if (slot2 >= 0 && t >= q.on2)
    {
      const auto &w = c.voiceAt(slot2);
      for (int i = 0; i < kN; i++) if (ent2[i] < 0 && w.phase[i] != 0.0) ent2[i] = t - q.on2;
    }
    if (q.trace && t < q.cpUntil && (t % q.cpEvery) == q.cpEvery - 1)
    {
      cps.push_back(t);
      for (int i = 0; i < kN; i++) { ph.push_back(v.phase[i]); oe.push_back(v.onsE[i]); }
    }
  }
  for (int i = 0; i < kN; i++) entries.push_back(ent1[i]);
  if (q.on2) for (int i = 0; i < kN; i++) entries.push_back(ent2[i]);
  if (comma) put(",");
  put("{\"name\":\""); put(q.name); put("\"");
  put(",\"K\":"); num(q.K); put(",\"scatter\":"); num(q.scatter); put(",\"alpha\":"); num(q.alpha);
  put(",\"attackScatter\":"); num(q.atkSc); put(",\"relScatter\":"); num(q.relSc); put(",\"voiceEnv\":"); num(q.venv);
  put(",\"A\":"); num(q.A); put(",\"R\":"); num(q.R); put(",\"oversample\":"); num(q.os);
  put(",\"on2\":"); num(q.on2); put(",\"off1\":"); num(q.off1); put(",\"off2\":"); num(q.off2); put(",\"total\":"); num(q.total);
  put(",\"entries\":"); arr(entries.data(), (int)entries.size());
  if (q.trace)
  {
    put(",\"cp\":"); arr(cps.data(), (int)cps.size());
    put(",\"phase\":"); arr(ph.data(), (int)ph.size());
    put(",\"onsE\":"); arr(oe.data(), (int)oe.size());
  }
  if (q.halves) { put(",\"t50a\":"); arr(t50a, kN); put(",\"t50r\":"); arr(t50r, kN); }
  put("}");
}

void build()
{
  out.clear();
  put("{\"tool\":\"tools/onset_ref_check.cpp\",\"reference\":\"src/swarm_core.h\",");
  put("\"regenerate\":\"build-release/onset_ref_check --emit > tools/labharness/onset_ref_cpp.json\",");
  serial();
  put(",\"render\":{\"sr\":44100,\"n\":7,\"f\":220,\"detune\":20,\"scen\":[");
  //        name       K     scat alpha atkSc relSc venv  A   R    os on2    off1   off2   total  cpE cpU  trace halves
  render({"ens",      0,    15,  0.25, 0.6,  0,    0,   20, 100, 0, 13230, 8820,  22050, 26460, 64, 4096, true,  false}, false);
  render({"venv",     0,    8,   0.25, 0.8,  0.8,  1,   40, 200, 0, 0,     22050, 0,     44100, 64, 0,    false, true}, true);
  render({"coupled",  0.35, 20,  0.25, 0,    0,    0,   20, 100, 0, 0,     8192,  0,     8192,  64, 8192, true,  false}, true);
  render({"os2",      0,    15,  0.25, 0.6,  0,    0,   20, 100, 1, 0,     8820,  0,     8820,  64, 0,    false, false}, true);
  put("]}}\n");
}
}  // namespace

int main(int argc, char **argv)
{
  build();
  if (argc > 1 && std::strcmp(argv[1], "--emit") == 0)
  {
    std::fputs(out.c_str(), stdout);
    return 0;
  }
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: onset_ref_check --emit | onset_ref_check <fixture.json>\n");
    return 2;
  }
  std::ifstream f(argv[1], std::ios::binary);
  if (!f)
  {
    std::fprintf(stderr, "onset_ref_check: cannot read %s\n", argv[1]);
    return 1;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string fixture = ss.str();
  /* calibration: the comparison must see a one-byte change (the last digit of the first number) */
  std::string planted = out;
  const size_t at = planted.find_first_of("0123456789", planted.find("\"tOff\":["));
  planted[at] = planted[at] == '9' ? '8' : (char)(planted[at] + 1);
  if (planted == out)
  {
    std::fprintf(stderr, "onset_ref_check: CALIBRATION FAILED — a planted one-byte change compared equal\n");
    return 1;
  }
  if (fixture != out)
  {
    size_t k = 0;
    while (k < fixture.size() && k < out.size() && fixture[k] == out[k]) k++;
    std::fprintf(stderr, "onset_ref_check: FAIL — %s is STALE against src/swarm_core.h (first difference at byte %zu).\n"
                         "  regenerate: build-release/onset_ref_check --emit > %s\n", argv[1], k, argv[1]);
    return 1;
  }
  std::printf("onset_ref_check: OK — %s is swarm_core.h's own output (%zu bytes; a planted one-byte change is caught)\n",
              argv[1], out.size());
  return 0;
}
