/*
 * tseed_check — B149: the ADR-077/078 ensemble-timing stream is SEEDED, and its
 * state survives the chunk.
 *
 * WHY. Until 2026-09-18 the stream was `uint32_t tRng = 12345` in
 * src/swarm_core.h — a literal `p.seed` never reached and no saved state ever
 * carried. The audit (docs/audits/2026-09-18-saw-engine-audit.md §1.3) measured
 * both halves of the failure on the patch `voiceEnv 1, attackScatter 1,
 * relScatter 1, onsetScatter 8`:
 *
 *     A. seed 1234 vs seed 999999                        RMS diff = 0.000e+00
 *     B. same seed + same note, 5 earlier notes vs none   RMS diff = 1.365e-01
 *     C. CONTROL: phase stream, retrig off, seed differs  RMS diff = 1.185e-01
 *
 * A says the seed knob was inert for every patch with onset scatter or
 * per-voice envelopes on. B says a restored instance — a reopened session, a
 * re-bounce — restarts the ensemble's timing history and renders a DIFFERENT
 * phrase, at the same magnitude as C, i.e. a completely different render. Both
 * contradict specs/ACCEPTANCE.md L0-13.
 *
 * WHAT IS ASSERTED, section by section:
 *   A  the seed reaches the stream: two seeds must render DIFFERENTLY on the
 *      audit's patch (this is the must-DIFFER that read 0.000e+00 before the
 *      fix), and the same seed twice must be bit-identical.
 *   B  ATTRIBUTION. The same two seeds on the same patch with the stream shut
 *      (onsetScatter = voiceEnv = 0) must move NOTHING — exactly 0.0. Without
 *      this, A says only "the render answers the seed somewhere". With it, A's
 *      difference has one possible source.
 *   C  CALIBRATION (L0032 / the detector-shares-the-assumption trap). B asserts
 *      a ZERO, and a zero is what a broken render, a broken compare or an
 *      unapplied parameter all produce. So the same rig, with retrig turned off
 *      to hand the seed back to the PHASE stream, must read non-zero — the
 *      audit's C.
 *   D  the stream is off-path when the features are off: the same patch renders
 *      to the FNV-1a hash origin/main renders it to. A hash, not a reading of
 *      the source, because "the branch is not taken" is a claim about the built
 *      binary. Baseline captured by compiling this file's own renderCore()
 *      against origin/main's swarm_core.h (transcript in the trace).
 *   E  the chunk: an instance saved five notes into a phrase and restored into a
 *      FRESH plugin holds the ensemble state EXACTLY — asserted on the `ens=`
 *      text, in both load orders (idle/direct and processing/queued). The
 *      MUST-FAIL control is the same restore with the line stripped out, which
 *      is the pre-fix behaviour exactly; it must not hold the state and the
 *      sixth note must audibly differ (it reads the audit's B magnitude).
 *   F  what the restored state buys, with the boundary stated: the next note's
 *      per-voice onset delays are bit-identical to the live instance's, and a
 *      core denied the state draws different ones. NOT asserted, because it is
 *      not true and this change does not make it true: whole-render bit
 *      identity of a mid-phrase restore. Three other quantities cross a note
 *      boundary (voice-slot ages, the phase stream's `age * 7919` term,
 *      lastPhase/lastNoteF) and none is in the chunk — a separate item.
 *   G  append-only and byte-neutral: a patch that never draws from the stream
 *      emits NO `ens=` key at all, so every chunk that existed before this
 *      change still serialises to the bytes it did. And a blob WITHOUT the key
 *      loads — to the seeded initial state, not to a refusal.
 *
 * Sections A-D and F drive SwarmCore directly (the audit's level); E and G go
 * through the shipped CLAP state extension via tools/statefix_common.h — the
 * chunk is the shell's, so a core-level stand-in would certify the wrong
 * thing.
 *
 * Standalone and NOT wired into ./verify — wiring a gate is the human's
 * decision (charter), proposed in the PR that adds it. Exit 1 on failure.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "statefix_common.h"
#include "../src/swarm_core.h"

using hypersaw::SwarmCore;

namespace
{

int g_failures = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_failures++;
}

/* ---- core level (sections A-C) ------------------------------------------ */

constexpr double kSr = 44100.0;
constexpr int kBlock = 512;
constexpr int kHeldBlocks = 40;    // ~0.46 s of the strike
constexpr int kGapBlocks = 10;     // release tail between notes
constexpr int kMidi[6] = {45, 52, 57, 45, 60, 45};
constexpr double kHz[6] = {110.0, 164.81377845643496, 220.0,
                           110.0, 261.6255653005986, 110.0};

/* The audit's patch (§1.3), verbatim: `voiceEnv 1, attackScatter 1, relScatter
   1, onsetScatter 8` and NOTHING else moved from the defaults. `ens` off is the
   same patch with the two gates that reach the stream closed.

   `retrigOff` is not cosmetic and must stay off for section A. retrig defaults
   to 1, which zeroes every start phase, and that is exactly WHY the audit could
   read 0.000e+00: with retrig on, `seed` reaches nothing else in this patch, so
   any difference at all is the ensemble stream's. Turning retrig off (section
   C) hands the seed back to the phase stream and is a different measurement —
   one this file makes deliberately, as the calibration. An earlier draft set it
   off in BOTH and the planted-literal control passed, because the number it was
   reading was the phase stream all along. */
void configure(SwarmCore &c, double seed, bool ens, bool retrigOff)
{
  c.setParam("seed", seed);
  c.setParam("n", 7);
  c.setParam("detune", 0.28);
  if (retrigOff) c.setParam("retrig", 0);
  c.setParam("attackScatter", ens ? 1.0 : 0.0);
  c.setParam("relScatter", ens ? 1.0 : 0.0);
  c.setParam("onsetScatter", ens ? 8.0 : 0.0);
  c.setParam("voiceEnv", ens ? 1.0 : 0.0);
}

/* Plays `notes` notes from the table above and returns the audio of the LAST
   one only — the audit's scenario is "this note, after that history". */
std::vector<float> renderCore(double seed, bool ens, int notes, bool retrigOff = false)
{
  SwarmCore c(kSr);
  configure(c, seed, ens, retrigOff);
  std::vector<float> L(kBlock), R(kBlock), out;
  for (int k = 0; k < notes; k++)
  {
    const bool last = (k == notes - 1);
    if (last) out.clear();
    c.noteOn(kMidi[k], kHz[k]);
    for (int b = 0; b < kHeldBlocks; b++)
    {
      c.render(L.data(), R.data(), kBlock);
      if (last)
        for (int i = 0; i < kBlock; i++) { out.push_back(L[i]); out.push_back(R[i]); }
    }
    c.noteOff(kMidi[k]);
    for (int b = 0; b < kGapBlocks; b++)
    {
      c.render(L.data(), R.data(), kBlock);
      if (last)
        for (int i = 0; i < kBlock; i++) { out.push_back(L[i]); out.push_back(R[i]); }
    }
  }
  return out;
}

double rmsDiff(const std::vector<float> &a, const std::vector<float> &b)
{
  if (a.size() != b.size() || a.empty()) return 1e9;
  double acc = 0;
  for (size_t i = 0; i < a.size(); i++)
  {
    const double d = (double)a[i] - (double)b[i];
    acc += d * d;
  }
  return std::sqrt(acc / (double)a.size());
}

bool bitEqual(const std::vector<float> &a, const std::vector<float> &b)
{
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (std::memcmp(&a[i], &b[i], sizeof(float)) != 0) return false;
  return true;
}

uint64_t fnv1a(const std::vector<float> &v)
{
  uint64_t h = 1469598103934665603ull;
  for (float f : v)
  {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    for (int b = 0; b < 4; b++)
    {
      h ^= (uint64_t)((bits >> (8 * b)) & 0xFFu);
      h *= 1099511628211ull;
    }
  }
  return h;
}

/* Section C's baseline, measured on origin/main (commit 6e33fd7) by compiling
   this file's renderCore(seed=1234, ens=false, notes=6) against that header.
   It is a value from BEFORE the change, so it can only be recaptured from
   history — never re-derived from the working tree, which would make it agree
   with itself. */
constexpr uint64_t kEnsOffBaseline = 0x134c681b4bcb9346ull;   // 51200 floats

/* ---- shell level (sections D-E) ----------------------------------------- */

/* Turn the shell's own saved chunk into the audit's patch by rewriting the four
   parameter lines in place. Going through the chunk rather than the param queue
   keeps this tool on ONE transport: the same text a DAW session stores. */
std::string withEnsemblePatch(std::string blob)
{
  auto setKey = [&blob](const char *key, const char *val) {
    const std::string k = std::string("\n") + key + "=";
    const size_t at = blob.find(k);
    if (at == std::string::npos) return;
    const size_t eol = blob.find('\n', at + 1);
    blob = blob.substr(0, at + k.size()) + val +
           (eol == std::string::npos ? std::string() : blob.substr(eol));
  };
  setKey("onsetScatter", "8");
  setKey("voiceEnv", "1");
  setKey("attackScatter", "1");
  setKey("relScatter", "1");
  return blob;
}

/* The `ens=` line of a blob, or "" if there is none. The whole assertion of
   section D is that this TEXT survives a round trip, so it is compared as
   text — a parsed comparison would have to re-implement the parser and could
   agree with a broken one. */
std::string ensLine(const std::string &blob)
{
  const size_t at = blob.find("\nens=");
  if (at == std::string::npos) return {};
  const size_t eol = blob.find('\n', at + 1);
  return blob.substr(at + 1, eol == std::string::npos ? std::string::npos : eol - at - 1);
}

std::string stripEns(const std::string &blob)
{
  std::string out;
  size_t pos = 0;
  while (pos < blob.size())
  {
    const size_t eol = blob.find('\n', pos);
    const std::string line =
        blob.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = eol == std::string::npos ? blob.size() : eol + 1;
    if (line.rfind("ens=", 0) == 0 || line.find(".ens=") != std::string::npos) continue;
    out += line;
    out += "\n";
  }
  return out;
}

/* One note through the shipped process path, capturing the whole strike.
   Activated once per plugin and left processing, because the point is a PHRASE:
   deactivating between notes is a different scenario than the one that fails. */
/* The plugin must OUTLIVE this: ~Phrase calls stop_processing/deactivate, so a
   `p->destroy(p)` before the Phrase leaves scope is a use-after-free (it was,
   and the crash landed after the last printf — the failure mode that reads as
   "the tool passed"). Every caller scopes the Phrase and destroys after. */
struct Phrase
{
  const clap_plugin_t *p;
  // PARENTHESES, not braces: `vector<float> L{256}` is one element of value
  // 256, and process() then writes 256 floats into a 1-float buffer. It ran,
  // printed every OK, and aborted after the last line.
  std::vector<float> L = std::vector<float>(statefix::kBlock, 0.0f);
  std::vector<float> R = std::vector<float>(statefix::kBlock, 0.0f);
  float *chans[2];
  clap_audio_buffer_t ob{};
  clap_process_t proc{};
  statefix::EvList empty;

  explicit Phrase(const clap_plugin_t *plug) : p(plug)
  {
    chans[0] = L.data();
    chans[1] = R.data();
    ob.data32 = chans;
    ob.channel_count = 2;
    proc.frames_count = statefix::kBlock;
    proc.audio_outputs = &ob;
    proc.audio_outputs_count = 1;
    proc.out_events = &statefix::kOut;
    p->activate(p, statefix::kSampleRate, 32, 1024);
    p->start_processing(p);
  }
  ~Phrase()
  {
    p->stop_processing(p);
    p->deactivate(p);
  }

  void note(int16_t key, uint16_t type)
  {
    struct One
    {
      clap_input_events_t list;
      clap_event_note_t ev{};
      bool sent = false;
    } one;
    one.ev.header.size = sizeof(one.ev);
    one.ev.header.type = type;
    one.ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    one.ev.note_id = -1;
    one.ev.port_index = 0;
    one.ev.channel = 0;
    one.ev.key = key;
    one.ev.velocity = 1.0;
    one.list.ctx = &one;
    one.list.size = [](const clap_input_events_t *l) -> uint32_t {
      return ((One *)l->ctx)->sent ? 0u : 1u;
    };
    one.list.get = [](const clap_input_events_t *l, uint32_t) -> const clap_event_header_t * {
      return &((One *)l->ctx)->ev.header;
    };
    proc.in_events = &one.list;
    p->process(p, &proc);
    one.sent = true;
  }

  // `out` null = play the note and throw the audio away (history, not evidence).
  void strike(int16_t key, int blocks, std::vector<float> *out)
  {
    note(key, CLAP_EVENT_NOTE_ON);
    if (out)
      for (uint32_t i = 0; i < statefix::kBlock; i++) { out->push_back(L[i]); out->push_back(R[i]); }
    proc.in_events = &empty.list;
    for (int b = 1; b < blocks; b++)
    {
      p->process(p, &proc);
      if (out)
        for (uint32_t i = 0; i < statefix::kBlock; i++) { out->push_back(L[i]); out->push_back(R[i]); }
    }
    note(key, CLAP_EVENT_NOTE_OFF);
    proc.in_events = &empty.list;
    for (int b = 0; b < 8; b++)
    {
      p->process(p, &proc);
      if (out)
        for (uint32_t i = 0; i < statefix::kBlock; i++) { out->push_back(L[i]); out->push_back(R[i]); }
    }
  }
};

}   // namespace

int main()
{
  std::printf("tseed_check — B149 ensemble-timing stream (ADR-077/078)\n\n");

  /* ---- A. the seed reaches the stream --------------------------------- */
  const auto a1234 = renderCore(1234, true, 1);
  const auto a999k = renderCore(999999, true, 1);
  const auto a1234b = renderCore(1234, true, 1);
  const double dSeed = rmsDiff(a1234, a999k);
  std::printf("A  seed 1234 vs 999999, ensemble ON : RMS diff = %.3e "
              "(audit A, before the fix: 0.000e+00)\n", dSeed);
  check(dSeed > 1e-4, "A1 the seed reaches the ensemble-timing stream (must DIFFER)");
  check(bitEqual(a1234, a1234b), "A2 the same seed renders bit-identically (must MATCH)");

  /* ---- B. ATTRIBUTION: A1's difference is the stream and nothing else -- */
  const auto b1234 = renderCore(1234, false, 1);
  const auto b999k = renderCore(999999, false, 1);
  const double dOff = rmsDiff(b1234, b999k);
  std::printf("B  the same two seeds, ensemble OFF : RMS diff = %.3e (must be 0)\n", dOff);
  check(dOff == 0.0,
        "B1 with the stream closed the same seed change moves NOTHING (so A1 is the stream)");

  /* ---- C. CALIBRATION: the instrument can see a seed change at all ----- */
  const auto c1234 = renderCore(1234, false, 1, /*retrigOff=*/true);
  const auto c999k = renderCore(999999, false, 1, /*retrigOff=*/true);
  const double dPhase = rmsDiff(c1234, c999k);
  std::printf("C  seeds differ, retrig OFF, ensemble OFF: RMS diff = %.3e (audit C: 1.185e-01)\n",
              dPhase);
  check(dPhase > 1e-4,
        "C1 CONTROL the render and the comparison can see a seed change (must DIFFER)");

  /* ---- D. off-path when the features are off -------------------------- */
  const auto dOffRender = renderCore(1234, false, 6);
  const uint64_t h = fnv1a(dOffRender);
  std::printf("D  onsetScatter = voiceEnv = 0, 6 notes: FNV-1a = %016llx (origin/main: %016llx)\n",
              (unsigned long long)h, (unsigned long long)kEnsOffBaseline);
  check(h == kEnsOffBaseline, "D1 a patch that never draws the stream is bit-identical to origin/main");

  /* ---- E. the chunk carries the ensemble state -------------------------
     ONE activation per phrase, on purpose: plug_activate REPLACES the core
     (preserving its params), so a scenario that re-activates between the fifth
     note and the sixth is not "a phrase" — it is two phrases, and it renders
     identically whether the chunk carries the timing or not. That false green
     is what D3/D5 exist to catch; it is also why plug_activate now carries the
     state across the replacement.

     WHAT IS ASSERTED HERE is the state, byte for byte, through the shipped text
     transport — not the audio. See the KNOWN BOUNDARY at E. */
  std::vector<float> contA, contB, contStripped;
  std::string blob, mid, reB, reStripped, reQueued;
  {
    const clap_plugin_t *p = statefix::makePlugin();
    blob = withEnsemblePatch(statefix::saveChunk(p));
    check(statefix::loadChunk(p, blob), "E0 the ensemble patch loads");
    {
      Phrase ph(p);
      for (int k = 0; k < 5; k++) ph.strike((int16_t)kMidi[k], 24, nullptr);
      mid = statefix::saveChunk(p);          // saved MID-PHRASE, as a host does
      ph.strike((int16_t)kMidi[5], 24, &contA);
    }
    p->destroy(p);
  }
  const std::string ensMid = ensLine(mid);
  check(!ensMid.empty(), "E1 a phrase with onset scatter writes the ens= key");
  {   // the host's own order: create, setState (idle), activate, process
    const clap_plugin_t *p = statefix::makePlugin();
    statefix::loadChunk(p, mid);
    reB = ensLine(statefix::saveChunk(p));
    {
      Phrase ph(p);
      ph.strike((int16_t)kMidi[5], 24, &contB);
    }
    p->destroy(p);
  }
  {   // the same restore with the key removed — exactly the pre-fix behaviour
    const clap_plugin_t *p = statefix::makePlugin();
    statefix::loadChunk(p, stripEns(mid));
    reStripped = ensLine(statefix::saveChunk(p));
    {
      Phrase ph(p);
      ph.strike((int16_t)kMidi[5], 24, &contStripped);
    }
    p->destroy(p);
  }
  {   // ... and the OTHER load order: while processing, parameters are queued
      // to the audio thread and therefore arrive AFTER the ens= key. The key
      // carries the seed it was derived under so this restores the same state.
    const clap_plugin_t *p = statefix::makePlugin();
    statefix::loadChunk(p, blob);
    {
      Phrase ph(p);
      statefix::loadChunk(p, mid);
      reQueued = ensLine(statefix::saveChunk(p));
    }
    p->destroy(p);
  }
  check(reB == ensMid,
        "E2 the ensemble state round-trips through the chunk EXACTLY (idle load)");
  check(reQueued == ensMid,
        "E3 ... and identically when the load is queued (processing) instead of direct");
  check(reStripped != ensMid,
        "E4 MUST-FAIL CONTROL a restore with the key stripped does NOT hold the state");
  const double dStrip = rmsDiff(contB, contStripped);
  std::printf("E  sixth note, restored WITH the key vs WITHOUT it: RMS diff = %.3e "
              "(audit B: 1.365e-01)\n", dStrip);
  check(dStrip > 1e-4,
        "E5 ... and the key is AUDIBLE: the two restores render differently");

  /* ---- F. the continuation the state buys ------------------------------
     KNOWN BOUNDARY, stated rather than hidden: the sixth note of a RESTORED
     plugin is not bit-identical to the sixth note of the instance that saved
     it, and this change does not make it so. Three other things cross a note
     boundary and none of them is in the chunk — the voice-slot allocator's
     ages (a released slot is reused in a different order after a restore), the
     per-voice phase stream's `age * 7919` term, and lastPhase/lastNoteF. They
     are a separate item; B149 is the ensemble-timing stream.

     What IS bit-identical is the quantity the stream produces: the next note's
     per-voice onset delays. Asserted core-direct, where the three confounders
     above can be held equal by construction. */
  {
    // Heap, not stack: sizeof(SwarmCore) is 667 KB (audit A9), and three of
    // them in one frame is 2 MB of automatic storage for no reason.
    auto liveP = std::make_unique<SwarmCore>(kSr);
    auto restoredP = std::make_unique<SwarmCore>(kSr);
    auto naiveP = std::make_unique<SwarmCore>(kSr);
    SwarmCore &live = *liveP, &restored = *restoredP, &naive = *naiveP;
    configure(live, 1234, true, false);
    configure(restored, 1234, true, false);
    configure(naive, 1234, true, false);
    std::vector<float> L(kBlock), R(kBlock);
    for (int k = 0; k < 5; k++)
    {
      live.noteOn(kMidi[k], kHz[k]);
      for (int b = 0; b < kHeldBlocks; b++) live.render(L.data(), R.data(), kBlock);
      live.noteOff(kMidi[k]);
      for (int b = 0; b < kGapBlocks; b++) live.render(L.data(), R.data(), kBlock);
    }
    restored.setEnsembleTiming(live.ensembleTiming());
    const int sl = live.noteOn(kMidi[5], kHz[5]);
    const int sr2 = restored.noteOn(kMidi[5], kHz[5]);
    const int sn = naive.noteOn(kMidi[5], kHz[5]);
    const int n = 7;
    bool same = true, differs = false;
    for (int i = 0; i < n; i++)
    {
      if (live.voiceAt(sl).onsD[i] != restored.voiceAt(sr2).onsD[i]) same = false;
      if (live.voiceAt(sl).onsD[i] != naive.voiceAt(sn).onsD[i]) differs = true;
    }
    // The WIDEST delay, not voice 0: initVoice shifts the earliest voice to
    // exactly 0, so voice 0 prints 0.000 in every arm and would read as a dead
    // meter (the numbers below are samples at 44.1 kHz).
    auto widest = [](const SwarmCore &c, int slot, int nv) {
      double w = 0;
      for (int i = 0; i < nv; i++) w = std::max(w, c.voiceAt(slot).onsD[i]);
      return w;
    };
    std::printf("F  widest onset delay (samples): live %.9f  restored %.9f  "
                "no-restore %.9f\n",
                widest(live, sl, n), widest(restored, sr2, n), widest(naive, sn, n));
    check(same, "F1 a restored core draws the SAME next-note onset delays, bit for bit");
    check(differs, "F2 MUST-FAIL CONTROL a core without the state draws different ones");
  }

  /* ---- G. append-only and byte-neutral -------------------------------- */
  {
    const clap_plugin_t *p = statefix::makePlugin();
    const std::string plain = statefix::saveChunk(p);
    check(plain.find("ens=") == std::string::npos,
          "G1 a patch that never draws the stream emits NO ens= key");
    // A five-note phrase on the DEFAULT patch still must not write the key:
    // absence is about the stream being untouched, not about note count.
    {
      Phrase ph(p);
      for (int k = 0; k < 5; k++) ph.strike((int16_t)kMidi[k], 8, nullptr);
    }
    const std::string played = statefix::saveChunk(p);
    check(played.find("ens=") == std::string::npos,
          "G2 ... and still none after five notes (onsetScatter = voiceEnv = 0)");
    check(statefix::loadChunk(p, stripEns(mid)),
          "G3 a blob with no ens= key still loads (absent => the seeded initial state)");
    p->destroy(p);
  }

  std::printf("\n%s tseed_check: %d failure(s)\n", g_failures ? "FAIL" : "OK  ", g_failures);
  return g_failures ? 1 : 0;
}
