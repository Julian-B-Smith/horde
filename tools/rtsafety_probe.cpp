/*
 * rtsafety_probe — does the audio thread allocate?
 *
 * The charter's real-time rule ("process() allocates nothing, no locks") has
 * been enforced by discipline and code review alone. Discipline does not scale:
 * one std::vector growth, one std::string, one std::function capture inside
 * process() is a priority-inversion glitch in a live set, and it is INVISIBLE
 * to every existing oracle because the audio is still bit-correct. It only
 * shows up as a click on someone else's machine.
 *
 * This makes it deterministic. Global operator new/delete are replaced with
 * counting versions; a flag marks the window where process() runs; any
 * allocation inside that window is a failure with a count. The plugin is driven
 * through the real CLAP path — activate, note on/off, param events, varied
 * block sizes — because the allocation you care about is the one a HOST
 * provokes, not the one a unit test avoids.
 *
 * NB deliberately: it counts allocations, it does not abort on them, so one run
 * reports every offender instead of the first.
 *
 * EVERY PATH THAT SHIPS MUST BE INSIDE THE ARMED WINDOW, or the probe reports
 * on code it never ran. B172's SUB OSC block is gated OFF by default, so the
 * event stream below switches it ON — otherwise sixteen per-voice cores, their
 * render and their control path would all sit outside this measurement while it
 * printed GREEN.
 */
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"

namespace
{
std::atomic<bool> g_armed{false};
std::atomic<long> g_allocs{0};
std::atomic<long> g_frees{0};
std::atomic<size_t> g_bytes{0};
}  // namespace

void *operator new(size_t n)
{
  if (g_armed.load(std::memory_order_relaxed))
  {
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    g_bytes.fetch_add(n, std::memory_order_relaxed);
  }
  void *p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void *operator new[](size_t n) { return operator new(n); }
void operator delete(void *p) noexcept
{
  if (g_armed.load(std::memory_order_relaxed)) g_frees.fetch_add(1, std::memory_order_relaxed);
  std::free(p);
}
void operator delete[](void *p) noexcept { operator delete(p); }
void operator delete(void *p, size_t) noexcept { operator delete(p); }
void operator delete[](void *p, size_t) noexcept { operator delete(p); }

static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "rtsafety", "-", "-", "1.0",
                                  [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                                  [](const clap_host_t *) {}, [](const clap_host_t *) {},
                                  [](const clap_host_t *) {}};

struct EvList
{
  clap_input_events_t in{};
  std::vector<const clap_event_header_t *> ev;
};
static uint32_t evSize(const clap_input_events_t *l)
{
  return (uint32_t)((EvList *)l->ctx)->ev.size();
}
static const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ev[i];
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

int main()
{
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, 44100.0, 32, 2048);
  p->start_processing(p);

  // Varied block sizes on purpose: a buffer sized for one block length is a
  // classic place to grow on the next one.
  const int blocks[] = {64, 128, 256, 512, 1024, 2048, 33, 127};
  std::vector<float> L(2048), R(2048);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;

  clap_event_note_t on{};
  on.header.size = sizeof(on); on.header.type = CLAP_EVENT_NOTE_ON;
  on.header.space_id = CLAP_CORE_EVENT_SPACE_ID; on.note_id = -1;
  on.port_index = 0; on.channel = 0; on.velocity = 0.8;
  clap_event_note_t off = on;
  off.header.type = CLAP_EVENT_NOTE_OFF;
  clap_event_param_value_t pv{};
  pv.header.size = sizeof(pv); pv.header.type = CLAP_EVENT_PARAM_VALUE;
  pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID; pv.note_id = -1;
  pv.port_index = -1; pv.channel = -1; pv.key = -1;

  // B172: the SUB OSC block's gate and a swept row from its 4000-block.
  clap_event_param_value_t subOn = pv, subP = pv;
  subOn.param_id = 4015;
  subOn.value = 1.0;

  EvList evl;
  evl.in.ctx = &evl; evl.in.size = evSize; evl.in.get = evGet;
  clap_output_events_t outEv{nullptr, outPush};

  long processedBlocks = 0;
  for (int round = 0; round < 40; round++)
  {
    for (int bi = 0; bi < 8; bi++)
    {
      const uint32_t n = (uint32_t)blocks[bi];
      evl.ev.clear();
      // notes, param sweeps and voice churn — the events a host really sends
      on.header.time = 0; on.key = 40 + (round * 3 + bi) % 30;
      off.header.time = n / 2; off.key = 40 + (round * 2 + bi) % 30;
      pv.header.time = n / 3;
      pv.param_id = 1 + (uint32_t)((round * 7 + bi) % 99);
      pv.value = 0.25 + 0.5 * ((round + bi) % 3) / 3.0;
      evl.ev.push_back(&on.header);
      evl.ev.push_back(&off.header);
      evl.ev.push_back(&pv.header);
      /* B172: the SUB OSC engine block. Without these two events the block's
         gate stays off, so renderSubSpan returns at its first line and the
         sixteen per-voice cores are never inside the armed window at all —
         the probe would print GREEN for a path it did not run. The gate is
         re-sent every block (idempotent) and one block row is swept per block,
         so both the render path and the control path (setParam -> recalc on
         all sixteen instances) are measured. */
      subOn.header.time = 0;
      subP.header.time = n / 4;
      subP.param_id = 4000 + (uint32_t)((round * 5 + bi) % 15);
      subP.value = 0.3 + 0.4 * ((round + bi) % 2);
      evl.ev.push_back(&subOn.header);
      evl.ev.push_back(&subP.header);

      clap_process_t proc{};
      proc.frames_count = n;
      proc.audio_outputs = &out; proc.audio_outputs_count = 1;
      proc.audio_inputs = nullptr; proc.audio_inputs_count = 0;
      proc.in_events = &evl.in; proc.out_events = &outEv;
      proc.steady_time = processedBlocks * 128;

      g_armed.store(true, std::memory_order_relaxed);   // ---- audio window ----
      p->process(p, &proc);
      g_armed.store(false, std::memory_order_relaxed);  // ----------------------
      processedBlocks++;
    }
  }
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);

  const long a = g_allocs.load(), f = g_frees.load();
  const size_t b = g_bytes.load();
  std::printf("rtsafety_probe: %ld process() calls, block sizes 33..2048\n", processedBlocks);
  std::printf("  allocations inside process(): %ld  (%zu bytes)\n", a, b);
  std::printf("  frees inside process():       %ld\n", f);
  std::printf("rtsafety_probe: %s\n",
              (a == 0 && f == 0) ? "GREEN (audio thread is allocation-free)"
                                 : "RED (the audio thread allocates)");
  return (a == 0 && f == 0) ? 0 : 1;
}
