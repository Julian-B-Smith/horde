/*
 * control_spread.h — the spread ramp's planted faults. ROADMAP B460 (ADR-216 D8).
 *
 * WHY THEY EXIST. tools/h2_spread_ramp_check.mjs judges that the member spread values
 * reach the blades as ramps (engine.h, rampSpread). A row that has never been seen to
 * fail proves nothing (LIBRARY L0032; control_steal.h is the precedent), so each fault
 * the check is there to catch has a plant here that the check must read as a fault, on
 * every run.
 *
 * NOT engines, and never enrolled. The check plays a job through one of these adapters,
 * reads it with the SAME measure as the real engine, and is red unless the plant reads
 * as the fault it carries.
 *
 *   id                    the fault
 *   control/ramp-blade1   THE RAMP REACHES BLADE 1 ONLY: blade 2 takes each tick's
 *                         values in one sample, as the present law does
 *   control/ramp-blade2   THE RAMP REACHES BLADE 2 ONLY: the same, the other way
 *   control/ramp-skip     A STEP LEFT AT THE TICK BOUNDARY: the ramp is dropped at one
 *                         tick in every kSkipEvery, so the values step there, and at
 *                         the tick after it too (the engine then starts its ramps
 *                         afresh, and a fresh start is a set)
 *   control/ramp-both     NOTHING PLANTED: the two-blade composition below with the
 *                         ramp on both blades. It must render what the engine renders,
 *                         or the composed plants here mean nothing
 *   control/step-<kind>-b1, control/step-<kind>-b2   (ADR-216 A4 item 6, PROPOSED)
 *                         ONE KIND OF VALUE LEFT STEPPED, EVERYTHING ELSE RAMPED, for
 *                         <kind> in position, cut, rotation, width, depth, index, band
 *                         (the band-limit pair), on blade 1 or on blade 2
 *   control/ramp-probe    NOT A SOUND: what each of the eighteen values does sample by
 *                         sample, read from an engine with the ramp and from one
 *                         without (the second is the plant: any kind left stepped)
 *   control/walk-probe    NOT A FAULT: two engines on one job, one with the ramp and
 *                         one without, and what comes out is how far apart their
 *                         spread walks are. It shows the check that a job's walk does
 *                         not depend on the flag (or, on the pad as shipped, that it does)
 *
 * HOW, WITHOUT TOUCHING THE ENGINE. No engine can be asked from outside to ramp one
 * blade and not the other, and a second engine build would need a CMake target of
 * its own. Everything here goes through the shipped engine's public interface.
 *
 *   THE TWO-BLADE COMPOSITION (blade1, blade2, both). Three shipped engines get the
 *   same seed and the same calls, so their swarms and their spread laws are the same
 *   sample for sample:
 *     A  blade 2 made inert (its depth forced to 0: a blade of depth 0 returns its
 *        input, engine.h voicePlainK), so A's bus is the base wave plus blade 1's part;
 *     B  blade 1 made inert the same way: the base wave plus blade 2's part;
 *     C  both inert: the base wave alone.
 *   With the blades in parallel the member's output is blade 1 plus blade 2 less the
 *   base (engine.h outK), and everything after it up to the limiter is linear, so on
 *   the pre-limiter bus A + B - C is the whole engine. The plant gives A and B
 *   DIFFERENT `spreadRamp` values: blade1 leaves A's as the job set it and forces B's
 *   to 0; blade2 does the reverse; both forces nothing.
 *   WHAT THAT ASSUMES OF THE JOB, and what happens otherwise: the blades in parallel
 *   (b2mix 0), no Depth spread unless blade 2 owns its spreads (an inert blade's own
 *   Depth spread is forced to 0 with its depth, or it would lift the depth off 0; with
 *   b2sp 0 blade 2 follows blade 1's, which cannot be forced apart), no
 *   cross-modulation or feedback, a base that is not the sine->saw
 *   shape, and NO NOISE BLADE (mode, mode2 not 3). A noise blade draws from the
 *   engine's one random stream at every noise step, and how many it takes depends on
 *   how its position moves; the drift law draws from the same stream, so a stepped
 *   engine and a ramped one would then walk different spreads. The check pins such a
 *   job and plays it through control/ramp-both as well: a job that breaks an
 *   assumption makes that render differ from the engine's, and the check is red.
 *
 *   ONE KIND STEPPED (step-<kind>-b1, -b2). No engine can be asked to step one kind
 *   and ramp the rest either. But blade 2 can own its spreads (b2sp 1), and the
 *   composition above can ramp one blade and not the other. So the plant is that
 *   composition, with the stepped blade's engine never hearing the flag, on a job in
 *   which THE STEPPED BLADE MOVES THAT KIND AND NOTHING ELSE while the other blade
 *   moves what it likes: then the one kind steps and everything else ramps. The
 *   adapter holds the job to that. It sets b2sp 1 itself, and it REFUSES a job that,
 *   as it stands when a render begins, has b2sp off, gives the stepped blade any
 *   other kind's spread, gives it the sine->saw shape when the kind is not the
 *   band-limit (that shape's band-limit follows each member's frequency, and so
 *   moves on every patch), or quantizes cut when the kind is cut (a quantized cut is
 *   set, not ramped, so there would be nothing planted). A refused job renders NaN:
 *   no reading at all, which the check reads as red. (Judged at each render and not
 *   at each set, because a job writes a patch first and its own settings over it.)
 *   Blade 1 and blade 2 each have their
 *   own plant, because a ramp that reached only one blade's values is a fault of its
 *   own (ramp-blade1, ramp-blade2).
 *
 *   THE WALK PROBE (walk-probe). Two shipped engines, the same seed and calls; the
 *   second never hears `spreadRamp` (it stays 0). After every render call the adapter
 *   reads both through the engine's public voice view and writes, to every sample of
 *   that call, on the left the largest |difference| between the two engines over
 *   every sounding member's fourteen spread slots (rv) and its phase, and on the right
 *   how many members it compared (0 would mean it compared nothing). Exactly 0 on the
 *   left throughout means the two walked the same spread.
 *
 *   THE RAMP PROBE (ramp-probe; ADR-216 A4 item 6, PROPOSED). Not a sound: a reading
 *   of the VALUES THEMSELVES, for the kinds no measure of the output can tell stepped
 *   from ramped (a rotation rate, a depth, the band-limit pair). Two shipped engines
 *   on one job, the first with the job's `spreadRamp`, the second never hearing it;
 *   both rendered one sample at a time, and after every sample each of the eighteen
 *   values of every member of the sounding voice is read through the engine's public
 *   voice view. Over each tick a value that moved is scored by how far its moves per
 *   sample are from equal shares (probe(), below: 0 for a ramp, 15 for a step at the
 *   tick). The readings ride the output: at sample n, with s = n mod 64, the left
 *   channel carries the first engine's worst score for value s (s 0 to 17), the
 *   second engine's for value s - 18 (18 to 35), the count of ticks over which value
 *   s - 36 moved in the first engine (36 to 53), else 0; the right channel carries s.
 *   THE PLANT IS THE SECOND ENGINE: its values for one kind, put in the place of the
 *   first engine's, are "that kind left stepped, everything else ramped", read by the
 *   same score. That needs the two engines on one walk, so a job with no noise blade.
 *
 *   THE SKIPPED TICK (skip). One shipped engine. The engine's couple tick falls on
 *   every kTick-th sample counted from its first (engine.h, kCoupleSamples and
 *   coupleTick), so the adapter counts samples, and at one tick in kSkipEvery it sets
 *   `spreadRamp` 0 for that one sample and puts the job's value back after it.
 *   Assumes every sample the engine renders goes through this adapter, which is so:
 *   prepare() builds the engine.
 *
 * The pre-limiter bus is the composed adapter's (atanh of the output), except the
 * two probes', whose samples are readings and are returned as they are.
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class SpreadPlantControl final : public Adapter {
 public:
  enum Plant { kBlade1, kBlade2, kBoth, kSkip, kStep, kWalk, kProbe };
  // probe: the values read (engine.h kSpreadKeys' order: nine of blade 1, nine of blade
  // 2), and how its readings are laid along the output (header)
  static constexpr int kKeys = horde2::engine::kSpreadKeyCount, kSlots = 64;
  // step: the kinds of value the spread law writes, in engine.h kSpreadKeys' order
  // (the band-limit's r and normaliser are one kind, the pair)
  enum Kind { kPosition, kCut, kRotation, kWidth, kDepth, kIndex, kBand, kKinds };
  // the engine's couple tick, in samples (engine.h kCoupleSamples, which is private)
  static constexpr int kTick = 32;
  // skip: the ramp is dropped at one tick in this many
  static constexpr int kSkipEvery = 4;

  // `blade`: 1 or 2, the blade whose values the plant leaves stepped. ramp-blade1 (the
  // ramp reaches blade 1 only) steps blade 2, and ramp-blade2 steps blade 1.
  explicit SpreadPlantControl(Plant p, int kind = -1, int blade = 0)
      : plant(p), kind(kind), stepped(p == kBlade1 ? 2 : p == kBlade2 ? 1 : p == kStep ? blade : 0) {}
  void prepare(double sampleRate, uint32_t seed) override {
    const int engines = plant == kSkip ? 1 : plant == kWalk || plant == kProbe ? 2 : kEngines;
    for (auto& w : worst) for (double& x : w) x = 0;
    for (double& x : moved) x = 0;
    for (auto& g : grp) for (auto& m : g) for (Group& k : m) k = Group{};
    for (int i = 0; i < kEngines; i++) {
      e[i].reset();
      if (i >= engines) continue;
      e[i] = std::make_unique<horde2::engine::Engine>(sampleRate);
      e[i]->seedRandom(seed);
    }
    pos = 0; ramp = 0;
    const double fresh[kWatched] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0};   // b2sp as set below; hot 2, base 0, kq 0 are the engine's
    std::memcpy(seen, fresh, sizeof seen);
    if (plant == kSkip || plant == kWalk || plant == kProbe) return;
    // the inert blades (their depth, and their own Depth spread)
    for (const char* k : {"depth2", "dspread2"}) { e[kA]->set(k, 0); e[kC]->set(k, 0); }
    for (const char* k : {"depth", "dspread"}) { e[kB]->set(k, 0); e[kC]->set(k, 0); }
    if (plant == kStep) for (auto& x : e) x->set("b2sp", 1);
  }
  bool set(const char* key, double value) override {
    const auto is = [key](const char* k) { return std::strcmp(key, k) == 0; };
    if (plant == kSkip) { if (is("spreadRamp")) ramp = value; return e[kA]->set(key, value); }
    if (plant == kWalk || plant == kProbe) { const bool known = e[kA]->set(key, value); e[kB]->set(key, is("spreadRamp") ? 0 : value); return known; }
    bool known = true;
    for (int i = 0; i < kEngines; i++) {
      double v = value;
      // the inert blades stay inert whatever the job sets
      if ((is("depth") || is("dspread")) && i != kA) v = 0;
      if ((is("depth2") || is("dspread2")) && i != kB) v = 0;
      // the plant itself: the engine carrying the stepped blade never hears the flag
      if (is("spreadRamp") && ((stepped == 2 && i == kB) || (stepped == 1 && i == kA))) v = 0;
      known = e[i]->set(key, v) && known;
    }
    if (plant == kStep) watch(key, value);
    return known;
  }
  bool setString(const char* key, const char* value) override { bool k = true; for (auto& x : e) if (x) k = x->setString(key, value) && k; return k; }
  void snap() override { for (auto& x : e) if (x) x->snap(); }
  void noteOn(int note, double freq, double vel) override { for (auto& x : e) if (x) x->noteOn(note, freq, vel); }
  void noteOff(int note) override { for (auto& x : e) if (x) x->noteOff(note); }
  void panic() override { for (auto& x : e) if (x) x->panic(); }
  void render(double* L, double* R, int n) override {
    if (plant == kSkip) {
      int done = 0;
      while (done < n) {
        const int at = static_cast<int>(pos % kTick);
        if (at == 0 && (pos / kTick) % kSkipEvery == 0) {   // this sample's tick is the dropped one
          e[kA]->set("spreadRamp", 0);
          e[kA]->render(L + done, R + done, 1);
          e[kA]->set("spreadRamp", ramp);
          done++; pos++;
          continue;
        }
        const int m = n - done < kTick - at ? n - done : kTick - at;   // up to the next tick
        e[kA]->render(L + done, R + done, m);
        done += m; pos += static_cast<uint64_t>(m);
      }
      return;
    }
    if (plant == kStep && outsideStepJob()) {   // not this plant's job: no reading at all
      for (int i = 0; i < n; i++) L[i] = R[i] = std::nan("");
      return;
    }
    if (plant == kProbe) {   // one sample at a time: what each value did on that sample
      double x, y;
      for (int i = 0; i < n; i++, pos++) {
        for (int en = 0; en < 2; en++) { e[en]->render(&x, &y, 1); probe(en); }
        const int s = static_cast<int>(pos % kSlots);
        L[i] = s < kKeys ? worst[0][s] : s < 2 * kKeys ? worst[1][s - kKeys] : s < 3 * kKeys ? moved[s - 2 * kKeys] : 0;
        R[i] = s;
      }
      return;
    }
    if (static_cast<int>(tl.size()) < n) { tl.resize(n); tr.resize(n); ul.resize(n); ur.resize(n); }
    e[kA]->render(L, R, n);
    e[kB]->render(tl.data(), tr.data(), n);
    if (plant == kWalk) {   // how far apart the two engines' walks stand after this call
      double far = 0, members = 0;
      for (int v = 0; v < horde2::engine::Engine::kVoices; v++) {
        const horde2::engine::Voice& a = e[kA]->voice(v);
        const horde2::engine::Voice& b = e[kB]->voice(v);
        if (a.active != b.active) far = INFINITY;   // not even the same voices
        if (!a.active || !b.active) continue;
        for (int q = 0; q < horde2::engine::kMembers; q++) {
          members++;
          for (int j = 0; j < 14; j++) { const double d = std::fabs(a.m[q].rv[j] - b.m[q].rv[j]); if (!(d <= far)) far = d; }
          const double d = std::fabs(a.m[q].phi - b.m[q].phi);
          if (!(d <= far)) far = d;
        }
      }
      for (int i = 0; i < n; i++) { L[i] = far; R[i] = members; }
      return;
    }
    e[kC]->render(ul.data(), ur.data(), n);
    for (int i = 0; i < n; i++) {
      L[i] = std::tanh(std::atanh(L[i]) + std::atanh(tl[i]) - std::atanh(ul[i]));
      R[i] = std::tanh(std::atanh(R[i]) + std::atanh(tr[i]) - std::atanh(ur[i]));
    }
  }
  double preLimiter(double out) const override { return plant == kWalk || plant == kProbe ? out : std::atanh(out); }

 private:
  enum { kA, kB, kC, kEngines };
  // probe: one value of one member over the tick in progress: where it stood after the
  // last sample, and its move at each of the tick's samples
  struct Group { double prev = 0, d[kTick] = {}; int n = 0; bool live = false; };
  // The sample at `pos` has just been rendered by engine `en`; a value is read AFTER
  // the sample, which is all the public view allows. A ramp of 32 equal shares s then
  // shows as 2s at the tick's position 0 (the tick's own share and the share taken at
  // that sample's end), s at positions 1 to 30, and 0 at position 31 (no share is
  // taken at the end of a tick's last sample: the next tick takes its own; engine.h,
  // the end of renderCallK's sample loop). So with u = half the move at position 0,
  // the moves at 1 to 30, and the move at 31 plus one share, every u is one share,
  // (all the moves) / 32, and a value that moved over the tick (by more than
  // kProbeMoved in all) scores its largest |u - share| / |share|: 0 for a ramp, 15
  // for a value that steps at the tick. When position 0 comes round the tick before
  // it is scored. A position is read modulo one cycle. A note's first three ticks are
  // left out (its values are set at the note-on, by rule).
  static constexpr double kProbeMoved = 1e-9;
  void probe(int en) {
    static const double horde2::engine::Member::* const kField[kKeys] = {
        &horde2::engine::Member::cOff,  &horde2::engine::Member::kAdd,  &horde2::engine::Member::kMul,  &horde2::engine::Member::rotOff,
        &horde2::engine::Member::wMul,  &horde2::engine::Member::dAdd,  &horde2::engine::Member::iMul,  &horde2::engine::Member::mr,
        &horde2::engine::Member::mn,    &horde2::engine::Member::cOff2, &horde2::engine::Member::kAdd2, &horde2::engine::Member::kMul2,
        &horde2::engine::Member::rotOff2, &horde2::engine::Member::wMul2, &horde2::engine::Member::dAdd2, &horde2::engine::Member::iMul2,
        &horde2::engine::Member::mr2,   &horde2::engine::Member::mn2};
    const horde2::engine::Voice* v = nullptr;
    for (int i = 0; i < horde2::engine::Engine::kVoices && !v; i++) if (e[en]->voice(i).active) v = &e[en]->voice(i);
    const bool first = pos % kTick == 0;
    for (int q = 0; q < horde2::engine::kMembers; q++) for (int k = 0; k < kKeys; k++) {
      Group& g = grp[en][q][k];
      if (!v) { g = Group{}; continue; }
      const double now = v->m[q].*kField[k];
      if (first) {   // the tick that has just ended
        double sum = 0;
        for (int j = 0; j < kTick; j++) sum += g.d[j];
        if (g.live && g.n == kTick && pos >= 4 * kTick && std::fabs(sum) > kProbeMoved) {
          const double share = sum / kTick;
          double score = 0;
          for (int j = 0; j < kTick; j++) {
            const double u = j == 0 ? g.d[0] / 2 : j == kTick - 1 ? g.d[j] + share : g.d[j];
            const double dev = std::fabs(u - share) / std::fabs(share);
            if (!(dev <= score)) score = dev;
          }
          if (!(score <= worst[en][k])) worst[en][k] = score;
          if (en == kA) moved[k]++;
        }
        g.n = 0;
      }
      if (g.live && g.n < kTick) {
        double d = now - g.prev;
        if (k % 9 == 0) d -= std::nearbyint(d);
        g.d[g.n++] = d;
      }
      g.prev = now; g.live = true;
    }
  }
  Group grp[2][horde2::engine::kMembers][kKeys];
  double worst[2][kKeys] = {}, moved[kKeys] = {};
  // step: what the job has set, so far, of the keys the plant's job is held to (header):
  // the stepped blade's seven spread amounts, then b2sp, its carrier shape, the base
  // shape and Quantize. The engine's own defaults until the job sets them.
  enum { kB2sp = kKinds, kHot, kBase, kKq, kWatched };
  void watch(const char* key, double value) {
    static const char* const kAmount[2][kKinds] = {
        {"bspread", "kspread", "rotSpread", "wspread", "dspread", "ispread", "mspread"},
        {"bspread2", "kspread2", "rotSpread2", "wspread2", "dspread2", "ispread2", "mspread2"}};
    const auto is = [key](const char* k) { return std::strcmp(key, k) == 0; };
    for (int k = 0; k < kKinds; k++) if (is(kAmount[stepped - 1][k])) seen[k] = value;
    if (is("b2sp")) seen[kB2sp] = value;
    if (is(stepped == 1 ? "hot" : "hot2")) seen[kHot] = value;
    if (is("base")) seen[kBase] = value;
    if (is("kq")) seen[kKq] = value;
  }
  // step: is the job, as it stands, outside the one the plant is a plant on? The
  // stepped blade may move its own kind and nothing else.
  bool outsideStepJob() const {
    if (seen[kB2sp] != 1) return true;
    for (int k = 0; k < kKinds; k++) if (k != kind && seen[k] != 0) return true;
    if (kind != kBand && (seen[kHot] == 6 || seen[kBase] == 6)) return true;
    return kind == kCut && seen[kKq] != 0;
  }
  double seen[kWatched] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0};
  const Plant plant;
  const int kind;      // step: which kind is left stepped
  const int stepped;   // the blade whose values step (1 or 2); 0 for both, skip and walk
  std::unique_ptr<horde2::engine::Engine> e[kEngines];   // skip: e[kA] alone; walk: e[kA] and e[kB]
  std::vector<double> tl, tr, ul, ur;   // a control adapter: not a real-time path
  uint64_t pos = 0;    // skip: samples rendered since prepare()
  double ramp = 0;     // skip: the job's own spreadRamp
};

}  // namespace horde2::harness
