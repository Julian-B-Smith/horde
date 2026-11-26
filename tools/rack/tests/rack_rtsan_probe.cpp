// rack_rtsan_probe — does the rack's audio path do anything a real-time thread must not? (ROADMAP B714; critic
// finding 7; ADR-197 risk row 1.) Built under RealtimeSanitizer (-fsanitize=realtime, Homebrew LLVM) with every
// module's DSP compiled by the same compiler, and run by tools/rack_check.py in ./verify full. Every audio-thread call
// the shell makes on the rack runs inside one realtime scope: process (sounding and idle path, every slot on, off,
// toggled, n = 1 to 4096 and past it), set_host for every key at seeded values, reset, clear_meters. Main-thread
// calls (attach, prepare, the chunk) run outside it, as the shell runs them.
//   rack_rtsan_probe --plant    ONE malloc inside the scope: must be reported, or nothing else here is believed
//   rack_rtsan_probe --control  an empty scope: must read zero
//   rack_rtsan_probe            the schedule above: must read zero
// The must-fail for the rack itself is the build with HORDE_RACK_PLANT_ALLOC (rack.cpp), whose process allocates.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "rack/rack.h"
#include "rack/slot_table.h"

// Exported by the runtime (the pair [[clang::nonblocking]] functions use), so the scope needs no annotation in the
// code under test. tools/rtsan_probe.cpp uses the same.
extern "C" {
void __rtsan_realtime_enter(void);
void __rtsan_realtime_exit(void);
}

namespace {
std::uint32_t mulberry32(std::uint32_t& a) {
  std::uint32_t t = a += 0x6D2B79F5u;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}
template <class F>
void realtime(F&& f) {
  __rtsan_realtime_enter();
  f();
  __rtsan_realtime_exit();
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--plant")) {
    realtime([] {
      void* p = std::malloc(64);
      std::printf("rack_rtsan_probe: planted malloc %p\n", p);
      std::free(p);
    });
    return 0;
  }
  if (argc > 1 && !std::strcmp(argv[1], "--control")) {
    realtime([] {});
    std::printf("rack_rtsan_probe: empty scope\n");
    return 0;
  }
  const rack::SlotTable& table = rack::registered_table();
  rack::Rack r;
  r.attach(table);   // main thread
  r.prepare(44100);  // main thread: modules allocate here
  std::vector<std::uint32_t> ids;
  for (const auto& row : table.slots) {
    ids.push_back(row.on_id);
    for (const auto& k : row.keys) ids.push_back(k.host_id);
  }
  std::vector<float> l(8192), rr(8192);
  std::uint32_t seed = 714;
  std::uint64_t blocks = 0;
  const std::uint32_t sizes[] = {1, 7, 64, 512, 4096, 5000};
  for (int round = 0; round < 40; ++round) {
    for (std::size_t i = 0; i < l.size(); ++i) {
      const bool quiet = round % 5 == 4;   // every fifth round silence: the idle path and the tail rule
      l[i] = quiet ? 0.0f : float(double(mulberry32(seed)) / 4294967296.0 - 0.5);
      rr[i] = quiet ? 0.0f : float(double(mulberry32(seed)) / 4294967296.0 - 0.5);
    }
    const std::uint32_t n = sizes[std::size_t(round) % (sizeof sizes / sizeof sizes[0])];
    realtime([&] {
      for (std::uint32_t id : ids) r.set_host(id, double(mulberry32(seed) % 1000) / 999.0);   // switches land 0 or 1
      if (round % 7 == 3) r.reset();
      if (round % 3 == 0) r.clear_meters();
      for (int b = 0; b < 4; ++b, ++blocks) r.process(l.data(), rr.data(), n, round % 5 == 4);
    });
  }
  std::printf("rack_rtsan_probe: %llu blocks over %zu slots and %zu host ids, every audio-thread call in the realtime scope\n", (unsigned long long)blocks,
              table.slots.size(), ids.size());
  return 0;
}
