/*
 * h2_swarm_lift_check — lift edit E1 re-proven: SwarmCore's per-voice dynamics
 * run on a Voice the CALLER owns, bit for bit as the core runs its own.
 * ROADMAP B379 step 1; h2/cores/swarm/lift-ledger.json entry E1.
 *
 * WIRED: ./verify full (it needs the C++ build; the byte-identity half of the
 * lift, tools/h2_lift_check.py, is in ./verify fast).
 *
 * WHY. The composed engine's voices follow SCALPEL's voice law (B310), not
 * SwarmCore's allocator, so horde 2 needs the swarm's dynamics WITHOUT the
 * core's own voices[]. controlTick is private and only renderSeg calls it; E1
 * adds one public forwarder, tickVoice(). It changes no code by construction (an
 * unused non-virtual inline member is never emitted). This check proves the
 * other half: that the forwarder, driven from outside on the
 * core's own schedule, IS the core's dynamics, so a composed layer built on it
 * starts from the proven law and not from a look-alike.
 *
 * HOW. Core A plays a note the ordinary way (noteOn + render) and its voice is
 * read after every 128-sample block (voiceAt). Core B has the same parameters
 * and never allocates: it copies a fresh slot as its own Voice, starts it with
 * the public initVoice(), and replays renderSeg's schedule by hand — the
 * gravity-grid segmentation of render(), a control tick wherever the SHARED
 * 16-sample counter is 0 with lastOfSeg on the segment's final tick (B148/O2),
 * and renderSeg's per-sample phase advance (the osSub == 1, no-glide, no-onset
 * expression, src/swarm_core.h:1053-1057). Every observable the dynamics own is
 * compared with memcmp: eff, vf, phase, driftS, KsmS, KsmP, KsmD, Kenv, R, RN,
 * psi, sigma, rngState. Coupling (K 0.45), drift (walk, 14 ct), inertia (0.3,
 * the second-order path) and onset lock (0.4) are all on, n 7, at 44.1 and 48 kHz.
 *
 * SCOPE: the DEFAULT render path only (osSub 1, no fRun glide, no onsD skip, no
 * note travel, no release or cull); docs/port/phase-1b.md says what is not covered.
 *
 * MUST-FAIL CONTROL (L0032): the same replay with the tick one sample late
 * (counter == 1) must NOT match — the comparison can see a schedule slip.
 *
 * This target compiles an h2 core, so it is built with -ffp-contract=off
 * (tools/h2_rules_check.py) and links nothing from the legacy shell.
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "../h2/cores/swarm/swarm_core.h"

using horde2::swarm::hypersaw::SwarmCore;
using Voice = SwarmCore::Voice;

namespace {

constexpr int kN = 7, kBlock = 128, kBlocks = 40, kTick = 16;

void configure(SwarmCore& c) {
  c.setParam("n", kN);
  c.setParam("K", 0.45);
  c.setParam("driftDepth", 14);
  c.setParam("inertia", 0.3);
  c.setParam("onset", 0.4);
}

// Everything the dynamics own, compared bit for bit. Returns the first field that differs, or nullptr.
const char* differs(const Voice& a, const Voice& b) {
  const size_t n = sizeof(double) * kN;
  if (std::memcmp(a.eff, b.eff, n)) return "eff";
  if (std::memcmp(a.vf, b.vf, n)) return "vf";
  if (std::memcmp(a.phase, b.phase, n)) return "phase";
  if (std::memcmp(a.driftS, b.driftS, n)) return "driftS";
  const double sa[] = {a.KsmS, a.KsmP, a.KsmD, a.Kenv, a.R, a.RN, a.psi, a.sigma};
  const double sb[] = {b.KsmS, b.KsmP, b.KsmD, b.Kenv, b.R, b.RN, b.psi, b.sigma};
  const char* names[] = {"KsmS", "KsmP", "KsmD", "Kenv", "R", "RN", "psi", "sigma"};
  for (int i = 0; i < 8; i++) if (std::memcmp(&sa[i], &sb[i], sizeof(double))) return names[i];
  if (a.rngState != b.rngState) return "rngState";
  return nullptr;
}

// Returns the block at which the owned voice first departs from the core's own (-1: never).
int run(double sr, int tickPhase, const char** field) {
  auto A = std::make_unique<SwarmCore>(sr), B = std::make_unique<SwarmCore>(sr);
  configure(*A); configure(*B);
  const int slot = A->noteOn(57, 220.0);
  auto v = std::make_unique<Voice>(B->voiceAt(0));   // a fresh slot's state, now owned by the caller
  B->initVoice(*v, 57, 220.0);
  const int grid = B->gravGridSamples();
  float L[kBlock], R[kBlock];
  long g = 0;       // samples rendered (the shared tick counter is g & 15)
  int acc = 0;      // samples owed to the gravity grid (render()'s gravAccum)
  for (int blk = 0; blk < kBlocks; blk++) {
    A->render(L, R, kBlock);
    for (int done = 0; done < kBlock;) {
      const int room = grid - acc, seg = (kBlock - done) < room ? (kBlock - done) : room;
      const int first = static_cast<int>((kTick - (g & (kTick - 1))) & (kTick - 1));
      const int last = first >= seg ? -1 : first + ((seg - 1 - first) / kTick) * kTick;
      for (int smp = 0; smp < seg; smp++) {
        if (((g + smp) & (kTick - 1)) == tickPhase) B->tickVoice(*v, smp == last);
        for (int i = 0; i < kN; i++) {
          const double f = v->eff[i];
          const double dph = std::max(0.0, f) / (sr * 1);
          double ph = v->phase[i] + dph;
          ph -= std::floor(ph);
          v->phase[i] = ph;
        }
      }
      g += seg; acc += seg; done += seg;
      if (acc >= grid) acc = 0;
    }
    if (const char* f = differs(A->voiceAt(slot), *v)) { *field = f; return blk; }
  }
  *field = "";
  return -1;
}

}  // namespace

int main() {
  int red = 0;
  for (double sr : {44100.0, 48000.0}) {
    const char* f = "";
    const int at = run(sr, 0, &f);
    if (at >= 0) red++;
    std::printf("%s  E1  %.0f Hz: a caller-owned voice ticked through tickVoice() %s the core's own voice over %d blocks%s%s\n",
                at < 0 ? "PASS" : "FAIL", sr, at < 0 ? "matches, bit for bit," : "DEPARTS from", kBlocks,
                at < 0 ? "" : " (first at block ", at < 0 ? "" : (std::to_string(at) + ", field " + f + ")").c_str());
    const char* cf = "";
    const int cat = run(sr, 1, &cf);
    if (cat < 0) red++;
    std::printf("%s  control  %.0f Hz: the same replay with every tick one sample late %s%s%s\n", cat >= 0 ? "PASS" : "FAIL", sr,
                cat >= 0 ? "is caught (first departure at block " : "is NOT caught: the comparison is blind",
                cat >= 0 ? (std::to_string(cat) + ", field " + cf).c_str() : "", cat >= 0 ? ")" : "");
  }
  std::printf("h2_swarm_lift_check: %s\n", red ? "RED" : "GREEN");
  return red ? 1 : 0;
}
