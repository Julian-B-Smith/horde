/*
 * h2_engine_stream.h — reads the scenario stream tools/h2_engine_render.mjs
 * writes, and replays a scenario through horde 2's composed engine
 * (h2/engine/engine.h). Shared by the parity check (tools/h2_engine_parity_check.cpp)
 * and any CPU bench that replays the same scripts, so they can never replay a
 * script differently. Include AFTER engine.h (the check defines H2_ENGINE_FAULTS
 * first, which this file honours).
 *
 * EVERY REPLAY RUNS WITH FLUSH-TO-ZERO ON (ADR-211 item 1, ruled by the human
 * 2026-10-11: the horde 2 shell runs the engine in that mode). replay() below is
 * the whole replay inside h2/flush_to_zero.h's switch, the one copy the shell
 * will use: the engine's construction and seeding, every event call, every
 * render. So the parity check, its FMA control, both self-digest tools, the
 * RealtimeSanitizer probe and the CPU bench's replay mode all run ONE engine, the
 * one that ships; until 2026-10-11 the self-digests pinned the mode-on engine while parity
 * judged the mode-off one, and nothing tied the two together. A tool cannot
 * replay with the mode off by forgetting something: the only replay offered here
 * sets it, and a replay made any other way is missing from the count below.
 * Each replay also records the mode it really ran in (gModeTally), and
 * printModeControls() turns that into two must-fire lines a gate prints and judges:
 *   control FZ     every replay ran with flush-to-zero on; the switch works from a
 *                  host that enters with the mode off AND from one that enters
 *                  with it on; a throw inside it leaves the host's word as it was;
 *   control ROUND  every replay ran rounding to nearest, which the pinned bits
 *                  need and the switch does NOT set (h2/flush_to_zero.h).
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

// Whether arithmetic rounds to nearest RIGHT NOW on the calling thread, judged by
// two additions and not by reading the mode's field. Never inlined and its
// operands volatile, for horde2::flushToZeroState()'s reasons: the additions must
// run where it is called and must not be folded. 1 plus three quarters of its
// ULP reaches the next double only when rounding to nearest or upward; 1 plus a
// quarter stays 1 unless rounding upward. Nearest is the one mode that gives both.
H2S_NOINLINE inline bool roundsToNearest() noexcept {
  volatile double one = 1.0;
  volatile double threeQuarterUlp = 0x1.8p-53, quarterUlp = 0x1p-54;   // of 1.0, whose ULP is 2^-52
  const double a = one + threeQuarterUlp;
  const double b = one + quarterUlp;
  return a != 1.0 && b == 1.0;
}

// What every replay in this process really ran in, counted from INSIDE the switch
// (so a switch that silently does nothing is counted as it is, not as it was asked).
struct ModeTally { uint64_t replays = 0, flushed = 0, nearest = 0; };
inline ModeTally gModeTally;

// THE mode scope. replay() below and the hooked copy
// (tools/h2_engine_hooked_replay.h) both run their whole body through it, so no
// two replays can set the mode differently.
template <class F>
inline void inMode(F&& f) {
  horde2::underFlushToZero([&] {
    gModeTally.replays++;
    if (horde2::flushToZeroState().on()) gModeTally.flushed++;
    if (roundsToNearest()) gModeTally.nearest++;
    f();
  });
}

// The replay's body, with no mode of its own. NOT for tools: call replay(), which
// runs this inside the switch. A tool that called it directly would leave those
// replays out of gModeTally, and the FZ control's count would come up short.
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
}

// ---- the two mode controls (header; LIBRARY L0032) ----

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

struct ModeControls {
  // The state this thread is in where the controls run. REPORTED, never judged by
  // itself: a process entered with the mode already on is not a broken switch.
  // What IS judged is what each replay ran in (the tally) and what the switch does
  // from each known state.
  horde2::FlushToZeroState entered;
  bool enteredNearest = false;
  SwitchSeen fromOff, fromOn;
  // A throw inside the switch, from a host that entered off.
  bool threw = false;
  uint64_t throwWord0 = 0, throwWord1 = 0;
  horde2::FlushToZeroState throwInside, throwAfter;
  // roundsToNearest() read INSIDE the switch under each rounding mode a host may
  // call in with. It must tell them apart, and it shows that the switch leaves
  // the host's rounding mode in force.
  bool readNearest = false, readTowardZero = true, readUpward = true, readDownward = true;
  ModeTally tally;
  uint64_t need = 0;

  bool counted() const { return tally.replays >= need && tally.replays > 0; }
  bool fz() const {
    return counted() && tally.flushed == tally.replays && fromOff.ok(false) && fromOn.ok(true) && threw && throwInside.on() &&
           throwWord1 == throwWord0 && throwAfter.off();
  }
  bool round() const { return counted() && tally.nearest == tally.replays && readNearest && !readTowardZero && !readUpward && !readDownward; }
  bool fired() const { return fz() && round(); }
};

// Runs both controls. `need` is the fewest replays the caller must have made by
// now (its scenario count): zero replays, or fewer, is red.
inline ModeControls modeControls(uint64_t need) {
  ModeControls c;
  c.need = need;
  c.tally = gModeTally;
  c.entered = horde2::flushToZeroState();
  c.enteredNearest = roundsToNearest();
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
  // fesetround also writes the x87 control word on x86, which the switch's restore
  // does not hold: the host's rounding mode is put back by hand, last.
  const int hostRounding = std::fegetround();
  auto readUnder = [](int rounding) {
    std::fesetround(rounding);
    bool r = false;
    horde2::underFlushToZero([&] { r = roundsToNearest(); });
    return r;
  };
  c.readNearest = readUnder(FE_TONEAREST);
  c.readTowardZero = readUnder(FE_TOWARDZERO);
  c.readUpward = readUnder(FE_UPWARD);
  c.readDownward = readUnder(FE_DOWNWARD);
  std::fesetround(hostRounding);
  return c;
}

// The two lines a gate prints. Returns whether both controls fired.
inline bool printModeControls(uint64_t need) {
  const ModeControls c = modeControls(need);
  auto st = [](const horde2::FlushToZeroState& s) { return s.on() ? "on" : (s.off() ? "off" : "HALF-SET"); };
  auto ull = [](uint64_t v) { return static_cast<unsigned long long>(v); };
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
  std::printf("%s  control ROUND  %llu of %llu replays ran rounding to nearest (need >= %llu; the pinned bits need it and the switch does not set it); "
              "the reading inside the switch when the host rounds to nearest %d, toward zero %d, upward %d, downward %d (must be 1 0 0 0); "
              "this thread %s here (reported, not judged)\n",
              c.round() ? "PASS" : "FAIL", ull(c.tally.nearest), ull(c.tally.replays), ull(c.need), c.readNearest, c.readTowardZero, c.readUpward,
              c.readDownward, c.enteredNearest ? "rounds to nearest" : "does NOT round to nearest");
  return c.fired();
}

#undef H2S_NOINLINE

}  // namespace h2engine_stream
