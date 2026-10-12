/*
 * h2_engine_stream.h — reads the scenario stream tools/h2_engine_render.mjs
 * writes, and replays a scenario through horde 2's composed engine
 * (h2/engine/engine.h). Shared by the parity check (tools/h2_engine_parity_check.cpp)
 * and any CPU bench that replays the same scripts, so they can never replay a
 * script differently. Include AFTER engine.h (the check defines H2_ENGINE_FAULTS
 * first, which this file honours).
 *
 * EVERY REPLAY RUNS WITH FLUSH-TO-ZERO ON AND ROUNDING TO NEAREST (ADR-211 item
 * 1, ruled by the human 2026-10-11: the horde 2 shell runs the engine with
 * flush-to-zero on; and the human's ruling of 2026-10-12, which the lead records
 * as ADR-213: the same switch forces round-to-nearest inside its scope and gives
 * the host its own mode back). replay() below is
 * the whole replay inside h2/flush_to_zero.h's switch, the one copy the shell
 * will use: the engine's construction and seeding, every event call, every
 * render. So the parity check, its FMA control, both self-digest tools, the
 * RealtimeSanitizer probe and the CPU bench's replay mode all run ONE engine, the
 * one that ships; until 2026-10-11 the self-digests pinned the mode-on engine while parity
 * judged the mode-off one, and nothing tied the two together. A tool cannot
 * replay with the mode off by forgetting something: the only replay offered here
 * sets it, and a replay made any other way is missing from the count below.
 * Each replay also records the mode it really ran in (gModeTally), and
 * printModeControls() turns that into three must-fire lines a gate prints and judges:
 *   control FZ     every replay ran with flush-to-zero on; the switch works from a
 *                  host that enters with the mode off AND from one that enters
 *                  with it on; a throw inside it leaves the host's word as it was;
 *   control ROUND  the switch SETS round-to-nearest. Every replay ran rounding to
 *                  nearest, ties to even. And a host that calls in rounding to
 *                  nearest, toward zero, upward and downward each gets: nearest
 *                  inside, its own mode named in the switch's report, and its
 *                  own mode back afterwards, also after a throw. Three sources
 *                  must agree on each of those: arithmetic (roundingSeen below),
 *                  the C library (the host's mode is set and read with <cfenv>,
 *                  never with the header's own constants) and the header's
 *                  read-back. Then the first clean replay this gate made is
 *                  made again from each of the four hosts: through the switch
 *                  it must give the bits the gate judged; and with NO switch
 *                  the three other modes must each give OTHER bits, so "the
 *                  same bits" is not the reading of a script that does not
 *                  care how it is rounded (the must-differ half);
 *   control WORD   the switch writes its two fields and no other. The bits that
 *                  really move are measured (the rounding field is what <cfenv>
 *                  moves between its modes; the flush bits are what differs
 *                  between the switch and its forced-off form) and must be
 *                  exactly what the header declares it owns. Then, from each of
 *                  the four hosts as it came AND with every field the switch
 *                  does not own flipped (trap enables, default-NaN and the rest
 *                  on arm64; the sticky flags on x86-64), the word inside must
 *                  be the host's with the flush bits set and the rounding field
 *                  cleared, bit for bit, and the word after must be the host's.
 * What the controls do not reach: the whole 543-scenario set from a host in
 * another mode is a measurement made by hand (docs/proposals/h2-flush-to-zero.md,
 * section 8), and the numbers of a script are read OUTSIDE the switch (strtod
 * below), in whatever mode the process is in.
 *
 * The stream is line-oriented text with binary payloads:
 *   H2ENGINE 1 <n>                                    header
 *   NODE <version>                                    the golden's Node (its libm)
 *   LIBM <fn> <n> + n*3 float64 (x, y, JS f(x[, y]))  libm probes (optional)
 *   ORACLE <path> <git blob sha1>                     one per golden file
 *   SCN <i> <name> / SR / SEED / commands / EV ... / NI <0|1> / CNT <culled> <refused>
 *       <stolen> / [EXCL / SELF]
 *       / DATA <frames> + frames*2 float64 (the PRISTINE golden's samples)
 *   END <n>
 * Commands: set <key> <number> | sets <key> <string> | snap | on <note> <freq> <vel>
 *   | off <note> | re <note> <freq> | panic | render <block> <count> | cap <n>
 *   | capPolicy <n>. Numbers are JS's shortest round-trip spelling, so strtod
 *   recovers the exact double the golden used.
 */
#pragma once

#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../h2/flush_to_zero.h"

namespace h2engine_stream {

struct Cmd { std::string op, key, str; double a = 0, b = 0, c = 0; };
struct Scenario {
  int index = -1;
  std::string name, excl;
  double sr = 48000;
  uint32_t seed = 0;
  std::vector<Cmd> cmds;
  uint64_t ev[5] = {0, 0, 0, 0, 0};
  uint32_t h1 = 0, h2 = 0;
  int ni = -1;   // 1: the instrumented copy's samples equal the pristine golden's
  bool hasCnt = false;
  double cnt[3] = {0, 0, 0};   // the golden's load readouts after the script: culled, refused, stolen
  bool hasSelf = false;
  double selfRms = 0, selfMax = 0;
  std::vector<double> js;   // the golden's samples, interleaved L/R
};

inline bool readLine(FILE* f, std::string& out) {
  out.clear();
  int ch;
  while ((ch = std::fgetc(f)) != EOF) { if (ch == '\n') return true; out.push_back(static_cast<char>(ch)); }
  return !out.empty();
}
inline std::string word(const std::string& s, size_t& p) {
  while (p < s.size() && s[p] == ' ') p++;
  const size_t b = p;
  while (p < s.size() && s[p] != ' ') p++;
  return s.substr(b, p - b);
}
inline std::string rest(const std::string& s, size_t p) { return p < s.size() ? s.substr(p + 1) : std::string(); }

// Reads from the line after `SCN ...` through the DATA payload.
inline bool readScenario(FILE* f, const std::string& first, Scenario& sc) {
  size_t p = 0;
  word(first, p);
  sc.index = std::atoi(word(first, p).c_str());
  sc.name = rest(first, p);
  std::string line;
  for (;;) {
    if (!readLine(f, line)) return false;
    p = 0;
    const std::string op = word(line, p);
    if (op == "SR") sc.sr = std::strtod(word(line, p).c_str(), nullptr);
    else if (op == "SEED") sc.seed = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
    else if (op == "set") { Cmd c; c.op = op; c.key = word(line, p); c.a = std::strtod(word(line, p).c_str(), nullptr); sc.cmds.push_back(c); }
    else if (op == "sets") { Cmd c; c.op = op; c.key = word(line, p); c.str = rest(line, p); sc.cmds.push_back(c); }
    else if (op == "snap" || op == "off" || op == "on" || op == "render" || op == "re" || op == "panic" || op == "cap" || op == "capPolicy") {
      Cmd c; c.op = op;
      c.a = std::strtod(word(line, p).c_str(), nullptr);
      c.b = std::strtod(word(line, p).c_str(), nullptr);
      c.c = std::strtod(word(line, p).c_str(), nullptr);
      sc.cmds.push_back(c);
    }
    else if (op == "EV") {
      for (int k = 1; k <= 4; k++) sc.ev[k] = std::strtoull(word(line, p).c_str(), nullptr, 10);
      sc.h1 = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
      sc.h2 = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
    }
    else if (op == "NI") sc.ni = std::atoi(word(line, p).c_str());
    else if (op == "CNT") { sc.hasCnt = true; for (double& x : sc.cnt) x = std::strtod(word(line, p).c_str(), nullptr); }
    else if (op == "EXCL") sc.excl = rest(line, p);
    else if (op == "SELF") { sc.hasSelf = true; sc.selfRms = std::strtod(word(line, p).c_str(), nullptr); sc.selfMax = std::strtod(word(line, p).c_str(), nullptr); }
    else if (op == "DATA") {
      const size_t frames = std::strtoull(word(line, p).c_str(), nullptr, 10);
      sc.js.resize(frames * 2);
      return std::fread(sc.js.data(), sizeof(double), frames * 2, f) == frames * 2;
    }
    else { std::fprintf(stderr, "h2_engine stream: unknown line '%s'\n", line.c_str()); return false; }
  }
}

// What a replay saw besides its samples: keys the engine does not have, and its
// load readouts at the end.
struct ReplayInfo { int unknownKeys = 0; std::string firstUnknown; double cnt[3] = {0, 0, 0}; };

// ---- the mode every replay runs in (header: EVERY REPLAY RUNS WITH FLUSH-TO-ZERO ON) ----

// Local to this file and undefined at its end.
#if defined(_MSC_VER) && !defined(__clang__)
#define H2S_NOINLINE __declspec(noinline)
#else
#define H2S_NOINLINE __attribute__((noinline))
#endif

// The direction arithmetic rounds in RIGHT NOW on the calling thread, as one of
// <cfenv>'s four names, or -1 for none of them. Judged by three additions and
// never by reading a field, so it leans on neither h2/flush_to_zero.h's
// constants nor the C library's: control ROUND holds all three to each other.
// Never inlined and its operands volatile, for horde2::flushToZeroState()'s
// reasons: the additions must run where it is called and must not be folded.
// 1 plus a quarter of its ULP moves off 1 only when rounding upward; minus 1
// minus the same moves off minus 1 only when rounding downward; 1 plus three
// quarters reaches the next double when rounding to nearest, and stays 1 when
// rounding toward zero.
H2S_NOINLINE inline int roundingSeen() noexcept {
  volatile double one = 1.0, minusOne = -1.0;
  volatile double quarterUlp = 0x1p-54, threeQuarterUlp = 0x1.8p-53;   // of 1.0, whose ULP is 2^-52
  const double a = one + quarterUlp;
  const double b = minusOne - quarterUlp;
  const double c = one + threeQuarterUlp;
  const bool up = a != 1.0, down = b != -1.0;
  if (up && down) return -1;
  if (up) return FE_UPWARD;
  if (down) return FE_DOWNWARD;
  return c != 1.0 ? FE_TONEAREST : FE_TOWARDZERO;
}

// What every replay in this process really ran in, counted from INSIDE the switch
// (so a switch that silently does nothing is counted as it is, not as it was
// asked). `enteredOther` is the one count taken outside it: the replays whose
// caller arrived in another rounding mode, from what the switch returns. It is
// the count a shell's latch will keep (h2/flush_to_zero.h, "the caller's rounding
// mode is reported"), printed here and never judged.
struct ModeTally { uint64_t replays = 0, flushed = 0, nearest = 0, enteredOther = 0; };
inline ModeTally gModeTally;

// THE mode scope. replay() below and the hooked copy
// (tools/h2_engine_hooked_replay.h) both run their whole body through it, so no
// two replays can set the mode differently.
template <class F>
inline void inMode(F&& f) {
  const horde2::Rounding arrived = horde2::underFlushToZero([&] {
    gModeTally.replays++;
    const horde2::FlushToZeroState in = horde2::flushToZeroState();
    if (in.on()) gModeTally.flushed++;
    if (in.nearest()) gModeTally.nearest++;
    f();
  });
  if (arrived != horde2::Rounding::nearest) gModeTally.enteredOther++;
}

// The replay's body, with no mode of its own. NOT for tools: call replay(), which
// runs this inside the switch. A tool that called it directly would leave those
// replays out of gModeTally, and the FZ control's count would come up short.
// (One caller below does so on purpose: control ROUND's no-switch half, which
// needs a host's rounding mode to reach the engine.)
inline void replayBody(const Scenario& sc, std::vector<double>& out, int fault, horde2::engine::EventLog* log, double eps, ReplayInfo* info,
                       int block) {
  auto* c = new horde2::engine::Engine(sc.sr);   // 114 KB (arm64): the heap, not a small stack
#ifdef H2_ENGINE_FAULTS
  c->fault = fault;
  c->faultEps = eps;
  c->armFaults();
#else
  (void)fault; (void)eps;
#endif
  c->events = log;
  c->seedRandom(sc.seed);
  out.clear();
  out.reserve(sc.js.size());
  std::vector<double> L, R;
  for (const Cmd& m : sc.cmds) {
    if (m.op == "set" || m.op == "sets") {
      const bool known = m.op == "set" ? c->set(m.key.c_str(), m.a) : c->setString(m.key.c_str(), m.str.c_str());
      if (!known && info) { if (!info->unknownKeys++) info->firstUnknown = m.key; }
    }
    else if (m.op == "snap") c->snap();
    else if (m.op == "on") c->noteOn(static_cast<int>(m.a), m.b, m.c);
    else if (m.op == "off") c->noteOff(static_cast<int>(m.a));
    else if (m.op == "re") c->retune(static_cast<int>(m.a), m.b);
    else if (m.op == "panic") c->panic();
    else if (m.op == "cap") c->setVoiceCap(m.a);
    else if (m.op == "capPolicy") c->setCapPolicy(m.a);
    else if (m.op == "render") {
      const int n = static_cast<int>(m.a), k = static_cast<int>(m.b);
      if (block > 0) {   // the same frames, cut at `block`
        long left = static_cast<long>(n) * k;
        L.assign(block, 0); R.assign(block, 0);
        while (left > 0) {
          const int b = left < block ? static_cast<int>(left) : block;
          c->render(L.data(), R.data(), b);
          for (int i = 0; i < b; i++) { out.push_back(L[i]); out.push_back(R[i]); }
          left -= b;
        }
        continue;
      }
      L.assign(n, 0); R.assign(n, 0);
      for (int b = 0; b < k; b++) { c->render(L.data(), R.data(), n); for (int i = 0; i < n; i++) { out.push_back(L[i]); out.push_back(R[i]); } }
    }
  }
  if (info) { info->cnt[0] = c->culled(); info->cnt[1] = c->refused(); info->cnt[2] = c->stolen(); }
  delete c;
}

// The first CLEAN replay this process made (no planted fault): its script, the
// host block it was cut at, and the samples the gate went on to judge. Control
// ROUND replays that script again from hosts in other rounding modes and holds
// the result to these samples, so "the pinned bits out" is about a row this gate
// judged, not a script made up for the control. Kept once per process, without
// the golden's samples (replayBody never reads them).
struct FirstClean { bool have = false; Scenario sc; int block = 0; std::vector<double> out; };
inline FirstClean gFirstClean;
inline void keepFirstClean(const Scenario& sc, int block, const std::vector<double>& out) {
  if (gFirstClean.have) return;
  gFirstClean.have = true;
  gFirstClean.sc.index = sc.index;
  gFirstClean.sc.name = sc.name;
  gFirstClean.sc.sr = sc.sr;
  gFirstClean.sc.seed = sc.seed;
  gFirstClean.sc.cmds = sc.cmds;
  gFirstClean.block = block;
  gFirstClean.out = out;
}

// Replays `sc` through a fresh engine, seeded as the golden was, WITH FLUSH-TO-ZERO
// ON over the whole of it (header). `out` receives the interleaved samples.
// `fault` / `eps` plant a must-fail control where the engine was compiled with
// H2_ENGINE_FAULTS; `log` receives the blade events. `block` > 0 renders every
// `render` command in host blocks of that size instead of the script's own (the
// determinism rows: the same samples whatever the host sends, events staying on
// their samples).
inline void replay(const Scenario& sc, std::vector<double>& out, int fault = 0, horde2::engine::EventLog* log = nullptr,
                   double eps = 0, ReplayInfo* info = nullptr, int block = 0) {
  inMode([&] { replayBody(sc, out, fault, log, eps, info, block); });
  if (fault == 0 && eps == 0) keepFirstClean(sc, block, out);
}

// ---- the three mode controls (header; LIBRARY L0032) ----

// The switch, watched from a host that enters with the mode in a KNOWN state. The
// two words are read immediately around the switch, with no arithmetic in
// between: on x86-64 the register also holds sticky exception flags, which the
// readings before and after would otherwise move.
struct SwitchSeen {
  horde2::FlushToZeroState before, inside, forcedOff, backOn, after;
  uint64_t word0 = 0, wordIn = 0, word1 = 0;
  bool ok(bool hostOn) const {
    const bool host = hostOn ? (before.on() && after.on()) : (before.off() && after.off());
    return host && inside.on() && forcedOff.off() && backOn.on() && word1 == word0;
  }
};
inline SwitchSeen switchFrom(bool hostOn) {
  SwitchSeen h;
  auto host = [&] {
    h.before = horde2::flushToZeroState();
    h.word0 = horde2::fpControlWord();
    horde2::underFlushToZero([&] {
      h.wordIn = horde2::fpControlWord();
      h.inside = horde2::flushToZeroState();
      // forced off inside it: "on" is not a reading that is always true
      horde2::flush_tools_only::withoutFlushToZero([&] { h.forcedOff = horde2::flushToZeroState(); });
      h.backOn = horde2::flushToZeroState();
    });
    h.word1 = horde2::fpControlWord();
    h.after = horde2::flushToZeroState();
  };
  if (hostOn) horde2::underFlushToZero(host);
  else horde2::flush_tools_only::withoutFlushToZero(host);
  return h;
}

inline bool sameSamples(const std::vector<double>& a, const std::vector<double>& b) {
  return a.size() == b.size() && !a.empty() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}
inline bool sameEvents(const horde2::engine::EventLog& a, const horde2::engine::EventLog& b) {
  return std::memcmp(&a.count[1], &b.count[1], 4 * sizeof(uint64_t)) == 0 && a.h1 == b.h1 && a.h2 == b.h2;
}

// The switch, watched from a host that calls in under ONE rounding mode. The
// host's mode is set with <cfenv>'s fesetround, not with anything from
// h2/flush_to_zero.h, so the header's constants are judged against the C
// library's and against arithmetic (roundingSeen), never against themselves.
struct RoundHost {
  int fe = 0;                                            // the mode this host calls in with
  horde2::Rounding want = horde2::Rounding::nearest;     // what the switch must report for it
  bool set = false;                                      // fesetround took it
  int seenBefore = -2, seenInside = -2, seenAfter = -2;  // arithmetic: before, inside the switch, after
  int feAfter = -2;                                      // <cfenv>'s reading after
  horde2::Rounding reported = horde2::Rounding::nearest;
  horde2::FlushToZeroState inside, forcedOff;            // the header's own read-back, in the switch and forced off in it
  uint64_t word0 = 0, wordIn = 0, wordOff = 0, word1 = 0;   // the control word: before, inside, forced off, after
  // The same host with the fields the switch does not own FLIPPED, so each of
  // those bits is seen set from one host and clear from the other. Words only: no
  // arithmetic may run there (flush_tools_only::asHostWithOtherFieldsFlipped).
  // oBase is the word as it came, read immediately before the flip.
  uint64_t oBase = 0, oWord0 = 0, oWordIn = 0, oWordOff = 0, oWord1 = 0;
  bool threw = false;                                    // a throw inside the switch
  uint64_t tWord0 = 0, tWord1 = 0;
  // The first clean replay, made again from this host.
  bool replayed = false;
  bool pinnedBits = false;    // through replay(): the samples the gate judged, the same events and readouts
  bool bareDiffers = false;   // the body alone, with NO switch: other samples than the same body rounding to nearest

  // Arithmetic, the C library and the header agree on the host's mode before and
  // after, and on round-to-nearest inside; the caller's mode is reported.
  bool modeOk() const {
    return set && seenBefore == fe && reported == want && inside.engineMode() && seenInside == FE_TONEAREST && forcedOff.off() &&
           forcedOff.nearest() && seenAfter == fe && feAfter == fe && threw;
  }
  // Inside, the word is the host's with the flush bits set and the rounding field
  // cleared, and NOTHING else differs; after, it is the host's bit for bit. From
  // the host as it came and from the host with its other fields flipped (which
  // must really be another word, and differ in no field the switch owns).
  bool wordOk(uint64_t flushBits, uint64_t roundingField) const {
    auto one = [&](uint64_t w0, uint64_t in, uint64_t off, uint64_t w1) {
      return in == ((w0 | flushBits) & ~roundingField) && off == (w0 & ~flushBits & ~roundingField) && w1 == w0;
    };
    const bool flipped = oWord0 != oBase && ((oWord0 ^ oBase) & (flushBits | roundingField)) == 0;
    return one(word0, wordIn, wordOff, word1) && flipped && one(oWord0, oWordIn, oWordOff, oWord1) && tWord1 == tWord0;
  }
  bool replayOk() const { return replayed && pinnedBits && (fe == FE_TONEAREST || bareDiffers); }
};

// What the nearest-entered host's control replays gave, for the other three to be
// held to.
struct RoundRef { horde2::engine::EventLog log; ReplayInfo info; std::vector<double> bare; bool bareIsPinned = false; };

inline RoundHost roundHost(int fe, horde2::Rounding want, RoundRef& ref) {
  RoundHost h;
  h.fe = fe;
  h.want = want;
  // fesetround also writes the x87 control word on x86, which the switch's restore
  // does not hold: the mode this thread came with is put back by hand, last.
  const int came = std::fegetround();
  h.set = std::fesetround(fe) == 0;
  h.seenBefore = roundingSeen();
  h.word0 = horde2::fpControlWord();
  h.reported = horde2::underFlushToZero([&] {
    h.wordIn = horde2::fpControlWord();
    // the nested call first: its word is read before any arithmetic in this scope can raise an x86 flag
    horde2::flush_tools_only::withoutFlushToZero([&] {
      h.wordOff = horde2::fpControlWord();
      h.forcedOff = horde2::flushToZeroState();
    });
    h.inside = horde2::flushToZeroState();
    h.seenInside = roundingSeen();
  });
  h.word1 = horde2::fpControlWord();
  h.seenAfter = roundingSeen();
  h.feAfter = std::fegetround();
  // WORDS ONLY in here (the trap enables may be set): no arithmetic, no read-back.
  h.oBase = horde2::fpControlWord();
  horde2::flush_tools_only::asHostWithOtherFieldsFlipped([&] {
    h.oWord0 = horde2::fpControlWord();
    horde2::underFlushToZero([&] {
      h.oWordIn = horde2::fpControlWord();
      horde2::flush_tools_only::withoutFlushToZero([&] { h.oWordOff = horde2::fpControlWord(); });
    });
    h.oWord1 = horde2::fpControlWord();
  });
  h.tWord0 = horde2::fpControlWord();
  try {
    horde2::underFlushToZero([] { throw 0; });
  } catch (int) {
    h.threw = true;
  }
  h.tWord1 = horde2::fpControlWord();
  if (gFirstClean.have) {
    const FirstClean& k = gFirstClean;
    std::vector<double> out, bare;
    horde2::engine::EventLog log;
    ReplayInfo info;
    replay(k.sc, out, 0, &log, 0, &info, k.block);        // the gate's own replay, entered in this host's mode
    replayBody(k.sc, bare, 0, nullptr, 0, nullptr, k.block);   // NO switch: this host's mode reaches the engine
    h.replayed = true;
    if (fe == FE_TONEAREST) {
      ref.log = log;
      ref.info = info;
      ref.bare = bare;
      ref.bareIsPinned = sameSamples(bare, k.out);
    }
    h.pinnedBits = sameSamples(out, k.out) && sameEvents(log, ref.log) && std::memcmp(info.cnt, ref.info.cnt, sizeof info.cnt) == 0;
    h.bareDiffers = !sameSamples(bare, ref.bare);
  }
  std::fesetround(came);
  return h;
}

struct ModeControls {
  // The state this thread is in where the controls run. REPORTED, never judged by
  // itself: a process entered with the mode already on, or in another rounding
  // mode, is not a broken switch. What IS judged is what each replay ran in (the
  // tally) and what the switch does from each known state.
  horde2::FlushToZeroState entered;
  int enteredSeen = -2;
  SwitchSeen fromOff, fromOn;
  // A throw inside the switch, from a host that entered off.
  bool threw = false;
  uint64_t throwWord0 = 0, throwWord1 = 0;
  horde2::FlushToZeroState throwInside, throwAfter;
  // The switch from a host in each rounding mode; to nearest first (it is the
  // others' reference).
  RoundHost hosts[4];
  RoundRef roundRef;
  // The bits that really move, measured: the rounding field is what <cfenv>
  // changes between its four modes (words read back to back, no arithmetic in
  // between), the flush bits are what differs between the switch and its
  // forced-off form. Held to what the header declares it owns.
  uint64_t roundingMoved = 0, flushMoved = 0;
  ModeTally tally;
  uint64_t need = 0;

  bool counted() const { return tally.replays >= need && tally.replays > 0; }
  bool fz() const {
    return counted() && tally.flushed == tally.replays && fromOff.ok(false) && fromOn.ok(true) && threw && throwInside.on() &&
           throwWord1 == throwWord0 && throwAfter.off();
  }
  bool round() const {
    bool ok = counted() && tally.nearest == tally.replays;
    for (const RoundHost& h : hosts) ok = ok && h.modeOk() && h.replayOk();
    return ok;
  }
  bool word() const {
    const uint64_t fb = horde2::flush_tools_only::kFlushBits, rf = horde2::flush_tools_only::kRoundingField;
    bool ok = flushMoved == fb && roundingMoved == rf;
    for (const RoundHost& h : hosts) ok = ok && h.wordOk(fb, rf);
    return ok;
  }
  bool fired() const { return fz() && round() && word(); }
};

// Runs the three controls. `need` is the fewest replays the caller must have made
// by now (its scenario count): zero replays, or fewer, is red.
inline ModeControls modeControls(uint64_t need) {
  ModeControls c;
  c.need = need;
  c.tally = gModeTally;   // first: the controls below replay too, and are not the gate's replays
  c.entered = horde2::flushToZeroState();
  c.enteredSeen = roundingSeen();
  c.fromOff = switchFrom(false);
  c.fromOn = switchFrom(true);
  horde2::flush_tools_only::withoutFlushToZero([&] {
    c.throwWord0 = horde2::fpControlWord();
    try {
      horde2::underFlushToZero([&] {
        c.throwInside = horde2::flushToZeroState();
        throw 0;
      });
    } catch (int) {
      c.threw = true;
    }
    c.throwWord1 = horde2::fpControlWord();
    c.throwAfter = horde2::flushToZeroState();
  });
  static const int kFe[4] = {FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD};
  static const horde2::Rounding kWant[4] = {horde2::Rounding::nearest, horde2::Rounding::towardZero, horde2::Rounding::upward,
                                            horde2::Rounding::downward};
  const int came = std::fegetround();
  uint64_t w[4];
  for (int i = 0; i < 4; i++) { std::fesetround(kFe[i]); w[i] = horde2::fpControlWord(); }
  std::fesetround(came);
  c.roundingMoved = (w[1] ^ w[0]) | (w[2] ^ w[0]) | (w[3] ^ w[0]);
  for (int i = 0; i < 4; i++) c.hosts[i] = roundHost(kFe[i], kWant[i], c.roundRef);
  c.flushMoved = c.hosts[0].wordIn ^ c.hosts[0].wordOff;
  return c;
}

// The three lines a gate prints. Returns whether every control fired.
inline bool printModeControls(uint64_t need) {
  const ModeControls c = modeControls(need);
  auto st = [](const horde2::FlushToZeroState& s) { return s.on() ? "on" : (s.off() ? "off" : "HALF-SET"); };
  auto ull = [](uint64_t v) { return static_cast<unsigned long long>(v); };
  auto fe = [](int m) {
    return m == FE_TONEAREST ? "to nearest" : m == FE_TOWARDZERO ? "toward zero" : m == FE_UPWARD ? "upward" : m == FE_DOWNWARD ? "downward" : "IN NO MODE";
  };
  auto rn = [](horde2::Rounding r) {
    return r == horde2::Rounding::nearest ? "to nearest" : r == horde2::Rounding::towardZero ? "toward zero" : r == horde2::Rounding::upward ? "upward" : "downward";
  };
  std::printf("%s  control FZ  flush-to-zero (h2/flush_to_zero.h, set inside h2engine_stream::replay): %llu of %llu replays ran with it on (need >= %llu); "
              "the switch from a host that enters off: %s, %s inside (bits %d, a subnormal result zero %d, a subnormal input zero %d), %s forced off inside it, "
              "%s again, %s after, control word 0x%llx, 0x%llx inside, 0x%llx after; from a host that enters on: %s, %s inside, %s forced off, %s again, %s after, "
              "control word 0x%llx, 0x%llx inside, 0x%llx after; a throw inside it: %s, %s inside, control word 0x%llx before, 0x%llx after, %s after; "
              "this thread is %s here (reported, not judged)\n",
              c.fz() ? "PASS" : "FAIL", ull(c.tally.flushed), ull(c.tally.replays), ull(c.need), st(c.fromOff.before), st(c.fromOff.inside), c.fromOff.inside.bits,
              c.fromOff.inside.results, c.fromOff.inside.inputs, st(c.fromOff.forcedOff), st(c.fromOff.backOn), st(c.fromOff.after), ull(c.fromOff.word0),
              ull(c.fromOff.wordIn), ull(c.fromOff.word1), st(c.fromOn.before), st(c.fromOn.inside), st(c.fromOn.forcedOff), st(c.fromOn.backOn),
              st(c.fromOn.after), ull(c.fromOn.word0), ull(c.fromOn.wordIn), ull(c.fromOn.word1), c.threw ? "thrown and caught" : "NOT THROWN",
              st(c.throwInside), ull(c.throwWord0), ull(c.throwWord1), st(c.throwAfter), st(c.entered));
  std::printf("%s  control ROUND  round-to-nearest (h2/flush_to_zero.h, set by the same switch): %llu of %llu replays ran rounding to nearest, ties to even "
              "(need >= %llu), %llu of them called from another rounding mode (the switch's own report: counted, not judged)",
              c.round() ? "PASS" : "FAIL", ull(c.tally.nearest), ull(c.tally.replays), ull(c.need), ull(c.tally.enteredOther));
  for (const RoundHost& h : c.hosts)
    std::printf("; a host that rounds %s%s: arithmetic rounds %s before, %s inside the switch (the header reads the field %s, ties to even %d, flush-to-zero %s), "
                "%s forced off inside it, the switch reports its caller %s, arithmetic rounds %s after and <cfenv> reads %s, a throw inside it %s",
                fe(h.fe), h.set ? "" : " (NOT SET)", fe(h.seenBefore), fe(h.seenInside), rn(h.inside.roundingField), h.inside.roundsToNearest, st(h.inside),
                h.forcedOff.nearest() ? "to nearest" : "NOT TO NEAREST", rn(h.reported), fe(h.seenAfter), fe(h.feAfter),
                h.threw ? "thrown and caught" : "NOT THROWN");
  if (gFirstClean.have)
    std::printf("; the first clean replay ('%s', %llu frames) made again from each host: through the switch, the bits this gate judged %d %d %d %d "
                "(must be 1 1 1 1); with NO switch, other bits than the same body rounding to nearest - %d %d %d (must be - 1 1 1; that body is %s the "
                "gate's bits, reported)",
                gFirstClean.sc.name.c_str(), ull(gFirstClean.out.size() / 2), c.hosts[0].pinnedBits, c.hosts[1].pinnedBits, c.hosts[2].pinnedBits,
                c.hosts[3].pinnedBits, c.hosts[1].bareDiffers, c.hosts[2].bareDiffers, c.hosts[3].bareDiffers,
                c.roundRef.bareIsPinned ? "bit for bit" : "NOT");
  else std::printf("; NO clean replay was made, so none could be made again from each host");
  std::printf("; this thread rounds %s here (reported, not judged)\n", fe(c.enteredSeen));
  std::printf("%s  control WORD  the switch writes its two fields and nothing else: the flush bits that move 0x%llx (declared 0x%llx), the rounding field "
              "<cfenv> moves 0x%llx (declared 0x%llx)",
              c.word() ? "PASS" : "FAIL", ull(c.flushMoved), ull(horde2::flush_tools_only::kFlushBits), ull(c.roundingMoved),
              ull(horde2::flush_tools_only::kRoundingField));
  for (const RoundHost& h : c.hosts)
    std::printf("; a host that rounds %s: control word 0x%llx, 0x%llx inside, 0x%llx forced off, 0x%llx after; with its other fields flipped (0x%llx as it "
                "came) 0x%llx, 0x%llx inside, 0x%llx forced off, 0x%llx after; around a throw 0x%llx, 0x%llx",
                fe(h.fe), ull(h.word0), ull(h.wordIn), ull(h.wordOff), ull(h.word1), ull(h.oBase), ull(h.oWord0), ull(h.oWordIn), ull(h.oWordOff),
                ull(h.oWord1), ull(h.tWord0), ull(h.tWord1));
  std::printf(" (inside must be the word before with the flush bits set and the rounding field cleared, forced off the same with the flush bits clear, "
              "after the word before)\n");
  return c.fired();
}

#undef H2S_NOINLINE

}  // namespace h2engine_stream
