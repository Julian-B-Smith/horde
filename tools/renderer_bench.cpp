/*
 * renderer_bench — ADR-006 spike: oscillator bank vs iFFT additive renderer.
 *
 * Workload per ROADMAP Phase 0: 128 partials x 5 cloud voices x 4 notes
 * = 2560 sinusoids at 44.1 kHz, frequencies updated at the control tick
 * (16 samples, 2756/s) for the bank and per-hop for the iFFT path.
 *
 * Methodology notes (bounds, not products):
 * - Bank: 4096-entry sine table + linear interp (SPEC §6.6), phase
 *   accumulators in flat float arrays so -O3 can vectorize. This is the
 *   realistic production shape of a bank renderer.
 * - iFFT: standard spectral additive — per hop, splat each sinusoid's
 *   windowed mainlobe (9-point Blackman-Harris kernel approximation) into a
 *   4096-bin spectrum, inverse real FFT (Accelerate vDSP on macOS), 75%
 *   overlap-add. Phase advanced per oscillator per hop. Production would use
 *   pffft/kissfft cross-platform; vDSP bounds the platform-best case here.
 * - Deterministic: mulberry32-seeded frequencies, no wall-clock inside the
 *   render paths (the timer wraps them).
 *
 * Output: seconds-of-audio-per-second-of-CPU (x realtime) for each path at
 * the target load, plus the prototype-ceiling load (24 partials) for scale.
 */

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#include <algorithm>
#endif

namespace
{
constexpr double kSR = 44100.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kTick = 16;
constexpr int kTableBits = 12;
constexpr int kTableSize = 1 << kTableBits;  // 4096, per SPEC §6.6
constexpr double kRenderSeconds = 8.0;

uint32_t mulberry32(uint32_t &s)
{
  s += 0x6D2B79F5u;
  uint32_t t = s;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}
double rng01(uint32_t &s) { return mulberry32(s) / 4294967296.0; }

struct Workload
{
  int partials, voicesPerPartial, notes;
  int total() const { return partials * voicesPerPartial * notes; }
};

/* Shared oscillator set: frequency (Hz) and amplitude per sinusoid, seeded. */
struct OscSet
{
  std::vector<double> freq, amp;
  OscSet(const Workload &w, uint32_t seed)
  {
    const double f0s[4] = {110.0, 146.83, 220.0, 293.66};
    for (int n = 0; n < w.notes; ++n)
      for (int p = 1; p <= w.partials; ++p)
        for (int v = 0; v < w.voicesPerPartial; ++v)
        {
          double base = f0s[n % 4] * p;
          double detune = 1.0 + (rng01(seed) - 0.5) * 0.004;  // ±0.2% cloud spread
          double f = base * detune;
          if (f > kSR * 0.45) f = kSR * 0.45;  // clamp near Nyquist like a real engine
          freq.push_back(f);
          amp.push_back(1.0 / p / w.voicesPerPartial / w.notes);
        }
  }
};

/* ---- Path A: oscillator bank ---- */
double runBank(const OscSet &osc, double seconds, float *sink)
{
  const int N = (int)osc.freq.size();
  std::vector<float> table(kTableSize + 1);
  for (int i = 0; i <= kTableSize; ++i)
    table[i] = (float)std::sin(2.0 * kPi * i / kTableSize);

  std::vector<float> phase(N), inc(N), amp(N);
  for (int i = 0; i < N; ++i)
  {
    phase[i] = 0.0f;
    inc[i] = (float)(osc.freq[i] / kSR);
    amp[i] = (float)osc.amp[i];
  }

  const long total = (long)(seconds * kSR);
  float accum = 0.0f;
  auto t0 = std::chrono::steady_clock::now();
  for (long s = 0; s < total; s += kTick)
  {
    // control tick boundary: in the real engine freqs update here (coupling);
    // the update itself is control-rate work outside this spike's scope.
    float block[kTick] = {0};
    for (int i = 0; i < N; ++i)
    {
      float ph = phase[i];
      const float in = inc[i], a = amp[i];
      for (int k = 0; k < kTick; ++k)
      {
        float x = ph * (float)kTableSize;
        int i0 = (int)x;
        float fr = x - (float)i0;
        block[k] += a * (table[i0] + fr * (table[i0 + 1] - table[i0]));
        ph += in;
        if (ph >= 1.0f) ph -= 1.0f;
      }
      phase[i] = ph;
    }
    accum += block[kTick - 1];
  }
  auto t1 = std::chrono::steady_clock::now();
  *sink = accum;
  return std::chrono::duration<double>(t1 - t0).count();
}

/* ---- Path B: iFFT spectral additive (macOS: vDSP) ---- */
#ifdef __APPLE__
double runIFFT(const OscSet &osc, double seconds, float *sink)
{
  const int N = (int)osc.freq.size();
  constexpr int kFFT = 4096, kHop = 1024;  // 75% overlap
  constexpr int kHalf = kFFT / 2;

  FFTSetup setup = vDSP_create_fftsetup(12, kFFTRadix2);
  std::vector<float> re(kHalf), im(kHalf), frame(kFFT), window(kFFT), ola(kFFT * 2, 0.0f);
  // Blackman-Harris analysis/synthesis window
  for (int i = 0; i < kFFT; ++i)
  {
    double t = 2.0 * kPi * i / kFFT;
    window[i] =
        (float)(0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t));
  }
  // 9-point BH mainlobe kernel sampled at bin offsets -4..4 (precomputed magnitudes)
  const float kernel[9] = {0.0003f, 0.0088f, 0.1105f, 0.4938f, 0.7930f,
                           0.4938f, 0.1105f, 0.0088f, 0.0003f};

  std::vector<double> phase(N, 0.0);
  const long hops = (long)(seconds * kSR / kHop);
  float accum = 0.0f;
  auto t0 = std::chrono::steady_clock::now();
  for (long h = 0; h < hops; ++h)
  {
    std::fill(re.begin(), re.end(), 0.0f);
    std::fill(im.begin(), im.end(), 0.0f);
    for (int i = 0; i < N; ++i)
    {
      const double binF = osc.freq[i] * kFFT / kSR;
      const int bin = (int)(binF + 0.5);
      const float a = (float)osc.amp[i];
      const float c = a * (float)std::cos(phase[i]), s = a * (float)std::sin(phase[i]);
      const int lo = bin - 4 < 1 ? 1 : bin - 4;
      const int hi = bin + 4 >= kHalf ? kHalf - 1 : bin + 4;
      for (int b = lo; b <= hi; ++b)
      {
        const float k = kernel[b - bin + 4];
        re[b] += c * k;
        im[b] += s * k;
      }
      phase[i] += 2.0 * kPi * osc.freq[i] * kHop / kSR;
      if (phase[i] > 1e9) phase[i] = std::fmod(phase[i], 2.0 * kPi);
    }
    DSPSplitComplex sc{re.data(), im.data()};
    vDSP_fft_zrip(setup, &sc, 1, 12, FFT_INVERSE);
    vDSP_ztoc(&sc, 1, (DSPComplex *)frame.data(), 2, kHalf);
    // window + overlap-add
    for (int i = 0; i < kFFT; ++i) ola[(h * kHop + i) % (kFFT * 2)] += frame[i] * window[i];
    accum += ola[(h * kHop) % (kFFT * 2)];
  }
  auto t1 = std::chrono::steady_clock::now();
  vDSP_destroy_fftsetup(setup);
  *sink = accum;
  return std::chrono::duration<double>(t1 - t0).count();
}
#endif

void report(const char *name, const Workload &w, double cpuSeconds)
{
  printf("  %-18s %4d part x %d vox x %d notes (%5d osc): %6.3f s CPU for %.0f s audio -> %6.1fx realtime\n",
         name, w.partials, w.voicesPerPartial, w.notes, w.total(), cpuSeconds, kRenderSeconds,
         kRenderSeconds / cpuSeconds);
}

}  // namespace

int main()
{
  printf("ADR-006 renderer spike — %0.f s render, single thread, 44.1 kHz\n", kRenderSeconds);
  float sink = 0;

  for (Workload w : {Workload{128, 5, 4}, Workload{24, 5, 4}, Workload{64, 5, 8}})
  {
    OscSet osc(w, 1234);
    double tBank = runBank(osc, kRenderSeconds, &sink);
    report("bank", w, tBank);
#ifdef __APPLE__
    double tFFT = runIFFT(osc, kRenderSeconds, &sink);
    report("iFFT(vDSP)", w, tFFT);
#endif
  }
  // keep the accumulator observable so -O3 can't dead-code the render loops
  fprintf(stderr, "(sink %g)\n", sink);
  return 0;
}
