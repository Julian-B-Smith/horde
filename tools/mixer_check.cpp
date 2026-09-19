/* mixer_check — per-oscillator mute/solo actually gate the mix.
 *
 * Green oracles prove nothing was BROKEN; they say nothing about whether a new
 * control does anything at all. A mute param that is read, stored, reported
 * back by readParam and never consulted by the render would pass every gate in
 * ./verify and be a dead switch in the mixer — which is L0023's failure mode
 * (a control with no effect) wearing the opposite mask.
 *
 * HOW THE TWO OSCILLATORS ARE TOLD APART. Oscillator 2 is transposed by a
 * TRITONE, so oscillator 1 sits at 440 Hz and oscillator 2 at 440*2^(6/12) =
 * 622.25 Hz, and a single-bin Goertzel at each frequency reads them
 * independently. Measuring total level instead would confuse "muted the right
 * one" with "muted either one", which is exactly the assertion that matters.
 *
 * THE INTERVAL IS LOAD-BEARING. The first version used an OCTAVE, and the
 * probe reported mute as broken: the 880 Hz bin fell to 67% when oscillator 1
 * was muted. That was the detector, not the mixer — these are SAWTOOTH
 * oscillators, so oscillator 1's 2nd harmonic lands exactly on oscillator 2's
 * fundamental (measured: 880 Hz baseline 0.2401 = oscillator 2's 0.1603 plus
 * oscillator 1's second harmonic 0.0803, and 0.1607/2 = 0.0803 to three
 * figures). Any harmonically related interval makes one bin read both sources.
 * The tritone is irrational (2^(1/2)), so no harmonic of either fundamental
 * lands on the other. Calibrate the detector for the signal class before
 * trusting it to accuse the code.
 * UNWIRED: reason not stated — see B159.
 */
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/hypersaw_clap_entry.h"

static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "mixer_check", "-", "-", "1.0",
                                  [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                                  [](const clap_host_t *) {}, [](const clap_host_t *) {},
                                  [](const clap_host_t *) {}};

struct EvList
{
  clap_input_events_t in{};
  std::vector<const clap_event_header_t *> ev;
};
static uint32_t evSize(const clap_input_events_t *l) { return (uint32_t)((EvList *)l->ctx)->ev.size(); }
static const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ev[i];
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

static int failures = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

static constexpr double kPi = 3.14159265358979323846;   // M_PI is absent under MSVC
static double goertzel(const std::vector<float> &x, double freq, double sr)
{
  const double w = 2.0 * kPi * freq / sr;
  const double c = 2.0 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
  const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
  return 2.0 * std::sqrt(re * re + im * im) / (double)x.size();
}

int main()
{
  const double SR = 44100.0;
  const int BLK = 512;
  const uint32_t S = 1000;                       // ADR-082 oscillator id stride
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, SR, 32, 2048);
  p->start_processing(p);

  std::vector<float> L(BLK), R(BLK);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans; out.channel_count = 2;
  EvList evl;
  evl.in.ctx = &evl; evl.in.size = evSize; evl.in.get = evGet;
  clap_output_events_t outEv{nullptr, outPush};

  // Params are queued into a stable store: the event list holds raw pointers,
  // so a reallocating vector would dangle mid-block.
  std::vector<clap_event_param_value_t> store;
  store.reserve(64);
  auto param = [&](clap_id id, double v) {
    clap_event_param_value_t pv{};
    pv.header.size = sizeof(pv); pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
    pv.param_id = id; pv.value = v;
    store.push_back(pv);
    evl.ev.push_back(&store.back().header);
  };

  auto run = [&](int blocks, std::vector<float> *capture) {
    for (int b = 0; b < blocks; b++)
    {
      clap_process_t proc{};
      proc.audio_inputs_count = 0; proc.audio_outputs_count = 1; proc.audio_outputs = &out;
      proc.frames_count = BLK; proc.in_events = &evl.in; proc.out_events = &outEv;
      proc.steady_time = (int64_t)b * BLK;
      p->process(p, &proc);
      evl.ev.clear();
      if (capture)
        for (int i = 0; i < BLK; i++) capture->push_back(0.5f * (L[i] + R[i]));
    }
  };

  for (uint32_t o = 0; o < 2; o++)
  {
    param(o * S + 1, 1);      // voices = 1
    param(o * S + 4, 0);      // detune = 0
    param(o * S + 14, 0);     // width = 0
    param(o * S + 9, 0);      // drift = 0
    param(o * S + 150, 1);    // ADR-100 A3: enable is the silent-default gate now
    param(o * S + 17, 0.4);   // volume (explicit, so the assertion is about MUTE)
  }
  param(S + 36, 6);           // oscillator 2 up a tritone -> 622.25 Hz (see header)
  run(2, nullptr);

  clap_event_note_t on{};
  on.header.size = sizeof(on); on.header.type = CLAP_EVENT_NOTE_ON;
  on.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  on.note_id = 5; on.port_index = 0; on.channel = 0; on.key = 69; on.velocity = 1.0;
  evl.ev.push_back(&on.header);
  run(25, nullptr);

  const double f1 = 440.0, f2 = 440.0 * 1.4142135623730951;   // A4 and its tritone
  // Settle generously after each change: the gate is one-pole smoothed (~8 ms)
  // on purpose, so an immediate reading would catch the ramp, not the state.
  auto measure = [&](double *o1, double *o2) {
    run(20, nullptr);
    std::vector<float> buf;
    run(20, &buf);
    *o1 = goertzel(buf, f1, SR);
    *o2 = goertzel(buf, f2, SR);
  };

  double a1, a2;
  measure(&a1, &a2);
  char d[192];
  std::snprintf(d, sizeof(d), "440Hz %.4f, 622Hz %.4f", a1, a2);
  check(a1 > 1e-3 && a2 > 1e-3, "baseline: both oscillators audible and separable", d);
  const double base1 = a1, base2 = a2;

  param(104, 1);                                  // mute oscillator 1
  measure(&a1, &a2);
  std::snprintf(d, sizeof(d), "440Hz %.4f (%.1f%%), 622Hz %.4f (%.1f%%)", a1, 100 * a1 / base1,
                a2, 100 * a2 / base2);
  check(a1 < 0.02 * base1 && a2 > 0.8 * base2, "mute silences ONLY that oscillator", d);

  param(104, 0);                                  // unmute, then solo oscillator 2
  param(S + 105, 1);
  measure(&a1, &a2);
  std::snprintf(d, sizeof(d), "440Hz %.4f (%.1f%%), 622Hz %.4f (%.1f%%)", a1, 100 * a1 / base1,
                a2, 100 * a2 / base2);
  check(a1 < 0.02 * base1 && a2 > 0.8 * base2, "solo silences every NON-soloed oscillator", d);

  param(105, 1);                                  // solo oscillator 1 as well
  measure(&a1, &a2);
  std::snprintf(d, sizeof(d), "440Hz %.4f (%.1f%%), 622Hz %.4f (%.1f%%)", a1, 100 * a1 / base1,
                a2, 100 * a2 / base2);
  check(a1 > 0.8 * base1 && a2 > 0.8 * base2, "soloing both restores both", d);

  param(104, 1);                                  // mute must beat solo
  measure(&a1, &a2);
  std::snprintf(d, sizeof(d), "440Hz %.4f (%.1f%%), 622Hz %.4f (%.1f%%)", a1, 100 * a1 / base1,
                a2, 100 * a2 / base2);
  check(a1 < 0.02 * base1 && a2 > 0.8 * base2, "mute wins over that oscillator's own solo", d);

  /* ADR-099 — the silent-oscillator skip must UN-skip on the volume path, not
     only on solo/mute. This drops oscillator 2's own vol (id 1017) to its
     shipped default 0 — the state every fresh patch is in, where the skip is
     active — and raises it again MID-NOTE. If un-skip fails on this path, every
     patch that starts from the default and turns oscillator 2 up gets silence,
     and the solo-based restore above would never see it. */
  param(104, 0); param(105, 0); param(1105, 0);   // clear mute/solo state
  param(1017, 0.0);                                // osc2 to its shipped default
  measure(&a1, &a2);
  const double gone = a2;
  param(1017, 0.4);                                // raise it mid-note
  measure(&a1, &a2);
  std::snprintf(d, sizeof(d), "622Hz muted-by-vol %.4f, raised %.4f (%.1f%%)",
                gone, a2, 100 * a2 / base2);
  check(gone < 0.02 * base2 && a2 > 0.8 * base2,
        "vol 0 -> 0.4 mid-note un-skips the oscillator", d);

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  std::printf("mixer_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
