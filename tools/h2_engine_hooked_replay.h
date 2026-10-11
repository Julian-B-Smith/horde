/*
 * h2_engine_hooked_replay.h — h2_engine_stream.h's replay with a hook around every
 * call a host block makes into the composed engine, and the loop that walks the
 * scenario stream. For the two probes of ROADMAP B448 phase 2, package P16
 * (ADR-209): tools/h2_engine_rtsan_probe.cpp wraps each call in RealtimeSanitizer's
 * realtime scope, tools/h2_engine_fz_probe.cpp switches flush-to-zero on around it.
 * Include AFTER engine.h and h2_engine_stream.h.
 *
 * WHY A COPY of h2engine_stream::replay, when that header exists so that no two
 * tools replay a script differently: its replay constructs the engine, grows
 * vectors and renders in one function, so a scope around it would report the
 * replay's own allocations, and it is compiled into four wired gates, which a
 * probe has no business editing. The copy cannot drift silently, because both
 * probes check it against the original on every run: the RealtimeSanitizer probe
 * demands the hooked samples equal h2engine_stream::replay's, bit for bit, in
 * every scenario (in the same binary), and the flush-to-zero probe demands the
 * hooked digests equal all the pinned self-digest rows before it measures anything.
 *
 * WHAT IS INSIDE THE HOOK: exactly one engine call (set, setString, snap, noteOn,
 * noteOff, retune, panic, setVoiceCap, setCapPolicy, render). The engine's
 * construction, seeding, the script's bookkeeping and the copy of the rendered
 * block into `out` stay outside, so a hook that forbids allocation judges the
 * engine and never the replay.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace h2engine_hooked {

using h2engine_stream::Cmd;
using h2engine_stream::ReplayInfo;
using h2engine_stream::Scenario;

// Which kind of call the hook is around: a probe counts them apart, because a
// shell may make the event calls on the audio thread or stage them elsewhere.
enum Call { kEvent = 0, kRender = 1 };

// `around(kind, fn)` must call fn() exactly once. `calls`, when given, receives the
// number of event and render calls made through the hook.
template <class Around>
inline void replay(const Scenario& sc, std::vector<double>& out, horde2::engine::EventLog* log, ReplayInfo* info, Around&& around,
                   uint64_t* calls = nullptr) {
  // No fault is armed: a fresh engine's own initialisers leave its must-fail hooks
  // off (engine.h), which is what h2engine_stream::replay's defaults set by hand.
  auto* c = new horde2::engine::Engine(sc.sr);
  c->events = log;
  c->seedRandom(sc.seed);
  out.clear();
  out.reserve(sc.js.size());
  std::vector<double> L, R;
  uint64_t made[2] = {0, 0};
  auto event = [&](auto&& fn) { made[kEvent]++; around(kEvent, fn); };
  for (const Cmd& m : sc.cmds) {
    if (m.op == "set" || m.op == "sets") {
      bool known = true;
      if (m.op == "set") event([&] { known = c->set(m.key.c_str(), m.a); });
      else event([&] { known = c->setString(m.key.c_str(), m.str.c_str()); });
      if (!known && info) { if (!info->unknownKeys++) info->firstUnknown = m.key; }
    }
    else if (m.op == "snap") event([&] { c->snap(); });
    else if (m.op == "on") event([&] { c->noteOn(static_cast<int>(m.a), m.b, m.c); });
    else if (m.op == "off") event([&] { c->noteOff(static_cast<int>(m.a)); });
    else if (m.op == "re") event([&] { c->retune(static_cast<int>(m.a), m.b); });
    else if (m.op == "panic") event([&] { c->panic(); });
    else if (m.op == "cap") event([&] { c->setVoiceCap(m.a); });
    else if (m.op == "capPolicy") event([&] { c->setCapPolicy(m.a); });
    else if (m.op == "render") {
      const int n = static_cast<int>(m.a), k = static_cast<int>(m.b);
      L.assign(n, 0); R.assign(n, 0);
      double* const l = L.data();
      double* const r = R.data();
      for (int b = 0; b < k; b++) {
        made[kRender]++;
        around(kRender, [&] { c->render(l, r, n); });
        for (int i = 0; i < n; i++) { out.push_back(L[i]); out.push_back(R[i]); }
      }
    }
  }
  if (info) { info->cnt[0] = c->culled(); info->cnt[1] = c->refused(); info->cnt[2] = c->stolen(); }
  if (calls) { calls[kEvent] += made[kEvent]; calls[kRender] += made[kRender]; }
  delete c;
}

// What the walk saw of the stream's frame. A probe judges nothing unless full().
struct Stream {
  long headerN = -1, endN = -1;
  int nodeMajor = -1, read = 0;
  bool ended = false, broke = false;
  bool full() const { return !broke && ended && endN == headerN && read == headerN && nodeMajor >= 0; }
};

// Walks tools/h2_engine_render.mjs's stream, as h2_engine_selfdigest_check's main
// does, and hands each scenario to `each`. The golden's JS samples are dropped:
// neither probe reads them. `stopAfter` > 0 stops after that many scenarios (the
// RealtimeSanitizer probe's plant uses one).
template <class Each>
inline Stream walk(FILE* f, Each&& each, int stopAfter = 0) {
  Stream s;
  std::string line;
  if (!h2engine_stream::readLine(f, line) || line.rfind("H2ENGINE 1", 0) != 0) { s.broke = true; return s; }
  s.headerN = std::strtol(line.c_str() + 10, nullptr, 10);
  while (h2engine_stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2engine_stream::word(line, p);
    if (op == "NODE") { s.nodeMajor = std::atoi(h2engine_stream::word(line, p).c_str()); continue; }
    if (op == "LIBM") {   // the parity check's libm probes: skipped
      h2engine_stream::word(line, p);
      const size_t n = std::strtoull(h2engine_stream::word(line, p).c_str(), nullptr, 10);
      std::vector<double> v(n * 3);
      if (std::fread(v.data(), sizeof(double), n * 3, f) != n * 3) { s.broke = true; break; }
      continue;
    }
    if (op == "ORACLE") continue;
    if (op == "END") { s.ended = true; s.endN = std::strtol(h2engine_stream::word(line, p).c_str(), nullptr, 10); break; }
    if (op != "SCN") { std::fprintf(stderr, "h2_engine stream: unexpected line '%s'\n", line.c_str()); s.broke = true; break; }
    Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { std::fprintf(stderr, "h2_engine stream: truncated in '%s'\n", sc.name.c_str()); s.broke = true; break; }
    sc.js.clear();
    sc.js.shrink_to_fit();
    s.read++;
    each(sc);
    if (stopAfter > 0 && s.read >= stopAfter) break;
  }
  return s;
}

}  // namespace h2engine_hooked
