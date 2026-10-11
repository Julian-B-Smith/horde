/*
 * h2_engine_rtsan_probe — does horde 2's composed engine (h2/engine/engine.h) do
 * anything a realtime thread must not? ROADMAP B448 phase 2, package P16 (ADR-209);
 * risk row R1 of docs/strategy/blind-spot-armor.md, the engine-level slice.
 *
 * Built with Clang's RealtimeSanitizer and run by tools/h2_engine_rtsan_check.py,
 * which carries the wiring declaration, the compiler rule (ADR-199) and the
 * verdict. This file is the probe only; it is not a CMake target, because the
 * product build stays on Apple clang.
 *
 * WHAT IS PROBED. Every scenario of the engine parity gate's stream
 * (tools/h2_engine_render.mjs, 543 today) is replayed through a fresh engine, and
 * every call a host block makes into it runs inside the realtime scope: render,
 * and the event calls (set, setString, snap, noteOn, noteOff, retune, panic,
 * setVoiceCap, setCapPolicy). Construction and seeding stay outside (see
 * tools/h2_engine_hooked_replay.h). Inside the scope RealtimeSanitizer reports any
 * malloc or free, lock, blocking call or system call, with its stack, which
 * tools/h2_engine_rtsan_check.py parses from stderr.
 *
 * NO WARM-UP. Each scenario's first render runs cold on a fresh engine, and the
 * process's first render runs cold too, so a function-local static that
 * initialises behind a guard lock on first use is reported (the legacy probe
 * found exactly that, tools/rtsan_check.py).
 *
 * WHAT "JUDGED" MEANS. A scenario counts as judged only when its hooked replay
 * finished AND its samples, blade events and load readouts equal those of
 * h2engine_stream::replay (the replay the parity gate uses) in this same binary.
 * So the count cannot be reached by a hooked replay that quietly renders
 * something else, or nothing.
 *
 * MODES.
 *   --plant    the first scenario only, with ONE malloc made inside the realtime
 *              scope straight after its first render call, through the same
 *              wrapper every engine call uses. It MUST be reported.
 *   --control  the first scenario only, each call made OUTSIDE the scope and the
 *              scope entered and left around nothing. It must read zero.
 *   (neither)  every scenario.
 *
 * Usage: h2_engine_rtsan_probe [--plant|--control] --full-from FILE
 * Exit: 0 the run finished (violations are in stderr, not in the exit code),
 *       2 the run broke (no stream, a cut stream).
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"
#include "h2_engine_hooked_replay.h"

/* Not in the installed rtsan_interface.h, but exported by the runtime: the pair
   that [[clang::nonblocking]] functions are instrumented with (tools/rtsan_probe.cpp
   uses the same). The engine's source is not annotated, and must not be. */
extern "C" {
void __rtsan_realtime_enter(void);
void __rtsan_realtime_exit(void);
}

namespace {

using horde2::engine::EventLog;
using h2engine_stream::ReplayInfo;
using h2engine_stream::Scenario;

/* THE realtime scope. Every engine call goes through this one function, and so
   does the plant, so "the plant was reported" proves this wrapper arms the
   sanitizer. */
template <class F>
void inRealtime(F&& f) {
  __rtsan_realtime_enter();
  f();
  __rtsan_realtime_exit();
}

bool sameLog(const EventLog& a, const EventLog& b) {
  return std::memcmp(a.count, b.count, sizeof a.count) == 0 && a.h1 == b.h1 && a.h2 == b.h2;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  bool plant = false, control = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--full-from") == 0 && i + 1 < argc) path = argv[++i];
    else if (std::strcmp(argv[i], "--plant") == 0) plant = true;
    else if (std::strcmp(argv[i], "--control") == 0) control = true;
    else { std::fprintf(stderr, "usage: h2_engine_rtsan_probe [--plant|--control] --full-from FILE\n"); return 2; }
  }
  FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
  if (!f) { std::fprintf(stderr, "h2_engine_rtsan_probe: cannot open the stream '%s'\n", path.c_str()); return 2; }

  // volatile: an allocation nothing reads back is elided, and plants nothing
  // (tools/rtsan_check.py, CALIBRATION).
  static void* volatile sink;
  bool planted = false;
  int judged = 0, differ = 0;
  uint64_t calls[2] = {0, 0};
  std::string firstDiffer;

  const h2engine_hooked::Stream s = h2engine_hooked::walk(f, [&](const Scenario& sc) {
    std::vector<double> out, ref;
    EventLog log, refLog;
    ReplayInfo info, refInfo;
    h2engine_hooked::replay(sc, out, &log, &info, [&](h2engine_hooked::Call kind, auto&& call) {
      if (control) { call(); inRealtime([] {}); return; }
      if (plant && kind == h2engine_hooked::kRender && !planted) {
        planted = true;
        inRealtime([&] { call(); sink = std::malloc(64); });
        std::free(sink);
        return;
      }
      inRealtime(call);
    }, calls);
    if (plant || control) return;
    // Outside any scope: the parity gate's own replay of the same script.
    h2engine_stream::replay(sc, ref, 0, &refLog, 0, &refInfo);
    const bool same = out.size() == ref.size() && !out.empty() && std::memcmp(out.data(), ref.data(), out.size() * sizeof(double)) == 0 &&
                      sameLog(log, refLog) && std::memcmp(info.cnt, refInfo.cnt, sizeof info.cnt) == 0;
    if (same) judged++;
    else if (!differ++) firstDiffer = sc.name;
  }, plant || control ? 1 : 0);
  std::fclose(f);

  if (plant || control) {
    std::printf("h2_engine_rtsan_probe: %s done (%d scenario, %llu render and %llu event calls through the scope%s)\n", plant ? "plant" : "control", s.read,
                static_cast<unsigned long long>(calls[h2engine_hooked::kRender]), static_cast<unsigned long long>(calls[h2engine_hooked::kEvent]),
                plant ? (planted ? ", one malloc planted" : ", NOTHING PLANTED") : ", every one empty");
    return s.read == 1 && (!plant || planted) ? 0 : 2;
  }
  std::printf("h2_engine_rtsan_probe: stream header %ld, END %s, %d scenarios read\n", s.headerN, s.ended ? std::to_string(s.endN).c_str() : "ABSENT", s.read);
  std::printf("h2_engine_rtsan_probe: %llu render calls and %llu event calls inside the realtime scope\n",
              static_cast<unsigned long long>(calls[h2engine_hooked::kRender]), static_cast<unsigned long long>(calls[h2engine_hooked::kEvent]));
  if (differ) std::printf("h2_engine_rtsan_probe: %d scenario(s) NOT judged: the hooked replay differs from h2engine_stream::replay, first: %s\n", differ, firstDiffer.c_str());
  std::printf("h2_engine_rtsan_probe: judged %d\n", judged);
  return s.full() ? 0 : 2;
}
