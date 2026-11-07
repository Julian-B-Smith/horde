// B645: measures the demo bank, since nobody here can listen. Built on demand (preview-demo-render), never by build.sh
// or ./verify.
//   preview-demo-render <data/bank/presets.json> <demo_bank.json> <out dir> [--only <name>] [--target <LUFS>] [--runs <n>]
// For each demo it renders, through a fresh engine and the clean room's resolver driven as the plug-in drives them (the
// corners, D.2's settings and the modes at sample 0, then spans cut at every control interval):
// - each corner on its own (the pad on it, the morph on): the chord 60 64 67 71 at velocity 0.85 held 3 s, as OU-2's
//   levelling plays it, and released; a sound that dies while held is struck again every `render.retrigger` seconds;
// - a slow sweep of the pad A -> B -> D -> C -> A, 4 s an edge, the chord held (or struck again) throughout;
// - for a lead or a bass, the same with the single note `render.single`.
// Every render is a 16-bit WAV in <out dir>, and metrics.json holds what each one measured: peak, loudness (BS.1770's
// K-weighting, ungated, as OU-2), DC, non-finite samples, the quietest 10 ms window while notes are held, spectral
// centroid and brightness, the cost of each 512-sample block (thread CPU against its 11.6 ms, the least of --runs runs,
// 3 by default, which must also render identical audio), and each corner's
// out.level move to the target loudness with a peak of at most -1 dBFS (--target, the factory bank's median).
// Deterministic audio (fixed notes, fixed blocks, no clock read in the render); the CPU figures are measurements.
#include <time.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "demo_bank.h"
#include "flush_to_zero.h"
#include "horde_engine.h"
#include "morph_core.h"

namespace {

constexpr double kRate = 44100;
constexpr int kBlock = 512;
constexpr std::int16_t kChord[4] = {60, 64, 67, 71};
constexpr double kVelocity = 0.85;
constexpr double kHold = 3.0, kTail = 1.5, kEdge = 4.0;
// Silent blocks before the first note, neither recorded nor timed: the engine is loaded with corner A, as the plug-in
// loads a demo, and the resolver's first resolution moves it to the pad's corner through PA-1's glide and PA-5's
// crossfades. In the plug-in that happens before anyone plays; struck at sample 0 it would colour the first strike.
constexpr std::size_t kPreroll = 22 * kBlock;

struct Point { double x, y; };
const Point kCorner[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};   // MO-4: A (0,0), B (1,0), C (0,1), D (1,1)

// The sweep's path A -> B -> D -> C -> A, one edge every kEdge seconds, linear along each edge.
Point sweep_at(double t) {
  const Point path[5] = {kCorner[0], kCorner[1], kCorner[3], kCorner[2], kCorner[0]};
  const double e = std::clamp(t / kEdge, 0.0, 4.0);
  const int k = std::min(3, int(e));
  const double f = e - k;
  return {path[k].x + (path[k + 1].x - path[k].x) * f, path[k].y + (path[k + 1].y - path[k].y) * f};
}

double thread_seconds() {
  timespec t;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  return double(t.tv_sec) + 1e-9 * double(t.tv_nsec);
}

struct Render {
  std::vector<float> l, r;
  std::vector<double> block_share;   // each block's thread CPU over its duration
  double held_s = 0;                 // notes are held (or struck again) up to here
};

// One render: the demo's four corners and settings in a fresh resolver beside a fresh engine loaded with corner A (as
// the plug-in loads a demo), the pad following `path`, `keys` struck at 0 (and every retrigger_s while held).
Render render(const preview::demo::Demo& d, const std::vector<int>& keys, double held_s, Point (*path)(double), int fixed_corner) {
  Render out;
  out.held_s = held_s;
  const std::size_t frames = std::size_t((held_s + kTail) * kRate);
  out.l.assign(frames, 0.0f);
  out.r.assign(frames, 0.0f);
  auto e = horde::make_engine();
  auto res = horde::make_morph_resolver();
  if (!e || !res || !e->prepare(kRate, kBlock) || !e->load_state(d.corners[0]).ok || !res->prepare(kRate)) {
    std::fprintf(stderr, "%s: the engine or resolver won't start\n", d.name.c_str());
    return out;
  }
  std::vector<double> by_id(horde::manifest_id_span(), 0.0);
  for (int c = 0; c < 4; ++c) {
    auto corner = horde::make_engine();
    corner->prepare(kRate, 64);
    corner->load_state(d.corners[c]);
    for (std::size_t i = 0; i < horde::manifest_size(); ++i) by_id[horde::manifest_param(i).id] = corner->param_v(horde::manifest_param(i).id);
    const std::string t0 = corner->ratios(0), t1 = corner->ratios(1);
    res->set_corner(c, by_id.data(), t0.c_str(), t1.c_str(), 0);
  }
  double v[preview::morph::kSettings];
  for (int i = 0; i < preview::morph::kSettings; ++i) v[i] = preview::morph::default_v(i);
  for (const auto& [key, value] : d.settings) v[preview::morph::find_setting(key)] = value;
  res->set_settings(preview::morph::decode(v), 0);
  for (const auto& m : d.modes) {
    horde::KeyMode mode;
    preview::morph::mode_of(m.mode, mode);
    const std::uint32_t id = m.key == "osc1.ratios" ? horde::kRatiosKeyId[0] : m.key == "osc2.ratios" ? horde::kRatiosKeyId[1] : horde::find_param(m.key)->id;
    res->set_key(id, mode, m.count == 2 ? horde::KeyCount::Two : m.count == 4 ? horde::KeyCount::Four : horde::KeyCount::AsMorph, 0);
  }
  Point at = fixed_corner >= 0 ? kCorner[fixed_corner] : path(0);
  res->set_position(at.x, at.y, 0);

  // The strikes: every key at each strike time, released at the next strike (or at held_s).
  struct Note { std::uint64_t on, off; int id, key; };
  std::vector<Note> notes;
  const double step = d.retrigger_s > 0 ? d.retrigger_s : held_s;
  int id = 1;
  for (double t = 0; t < held_s - 1e-9; t += step) {
    const double end = std::min(held_s, t + step);
    for (int k : keys) notes.push_back({std::uint64_t(std::llround(t * kRate)), std::uint64_t(std::llround(end * kRate)) - 1, id++, k});
  }

  for (Note& note : notes) note.on += kPreroll, note.off += kPreroll;
  const std::uint64_t ci = std::uint64_t(std::lround(kRate / 1500));   // PA-3
  std::vector<horde::MorphEvent> got(8192);
  std::vector<horde::Event> events;
  std::uint64_t clock = 0;
  float l[kBlock], r[kBlock];
  horde2::underFlushToZero([&] {
    for (std::size_t block = 0; block * kBlock < frames + kPreroll; ++block) {
      const std::uint32_t n = std::uint32_t(std::min<std::size_t>(kBlock, frames + kPreroll - block * kBlock));
      const double t0 = thread_seconds();
      if (fixed_corner < 0 && clock > kPreroll) {
        at = path(double(clock - kPreroll) / kRate);
        res->set_position(at.x, at.y, clock);
      }
      for (std::uint32_t pos = 0; pos < n;) {
        const std::uint32_t span = std::uint32_t(std::min<std::uint64_t>(n - pos, ci - clock % ci));
        events.clear();
        // The morph's events first at a boundary, then the notes (as the plug-in merges them: the resolver's at the
        // span's start, which a note at that same sample follows).
        const std::size_t got_n = res->process(span, got.data(), got.size());
        for (std::size_t i = 0; i < got_n; ++i) {
          const horde::MorphEvent& m = got[i];
          horde::Event ev;
          ev.sample = std::uint32_t(m.sample - clock);
          if (m.kind == horde::MorphEvent::Kind::Param) ev.type = horde::EventType::Param, ev.param_id = m.param_id, ev.value = m.value, ev.landing = m.landing;
          else if (m.kind == horde::MorphEvent::Kind::Level) ev.type = horde::EventType::MorphLevel, ev.integer = m.source, ev.value = m.value;
          else if (m.kind == horde::MorphEvent::Kind::Ratios) { e->set_ratios(m.osc, m.text); continue; }
          else continue;
          events.push_back(ev);
        }
        for (const Note& note : notes) {
          for (int edge = 0; edge < 2; ++edge) {
            const std::uint64_t when = edge ? note.off : note.on;
            if (when < clock || when >= clock + span) continue;
            horde::Event ev;
            ev.type = edge ? horde::EventType::NoteOff : horde::EventType::NoteOn;
            ev.sample = std::uint32_t(when - clock);
            ev.note_id = note.id, ev.channel = 0, ev.key = std::int16_t(note.key), ev.value = edge ? 0 : kVelocity;
            events.push_back(ev);
          }
        }
        std::stable_sort(events.begin(), events.end(), [](const horde::Event& a, const horde::Event& b) { return a.sample < b.sample; });
        e->process(events.data(), events.size(), nullptr, 0, l + pos, r + pos, int(span));
        pos += span;
        clock += span;
      }
      if (block * kBlock < kPreroll) continue;
      out.block_share.push_back((thread_seconds() - t0) / (double(n) / kRate));
      std::copy(l, l + n, out.l.begin() + std::ptrdiff_t(block * kBlock - kPreroll));
      std::copy(r, r + n, out.r.begin() + std::ptrdiff_t(block * kBlock - kPreroll));
    }
  });
  return out;
}

// The render run `g_runs` times: the audio must be identical every time (deterministic, or the tool says so and fails),
// and each block's cost is its least over the runs, so another process on the machine stealing a block in one run
// doesn't read as the patch's cost. The worst block is then the worst of those least costs.
int g_runs = 3;
bool g_deterministic = true;
Render timed(const preview::demo::Demo& d, const std::vector<int>& keys, double held_s, int fixed_corner) {
  Render best = render(d, keys, held_s, sweep_at, fixed_corner);
  for (int k = 1; k < g_runs; ++k) {
    const Render again = render(d, keys, held_s, sweep_at, fixed_corner);
    if (again.l != best.l || again.r != best.r) {
      std::fprintf(stderr, "%s: two renders differ\n", d.name.c_str());
      g_deterministic = false;
    }
    for (std::size_t b = 0; b < best.block_share.size() && b < again.block_share.size(); ++b) best.block_share[b] = std::min(best.block_share[b], again.block_share[b]);
  }
  return best;
}

// ---- measures ------------------------------------------------------------------------------------------------------
double db(double x) { return x > 0 ? 20 * std::log10(x) : -999; }

double peak_of(const Render& x) {
  double p = 0;
  for (std::size_t i = 0; i < x.l.size(); ++i) p = std::max({p, double(std::fabs(x.l[i])), double(std::fabs(x.r[i]))});
  return p;
}
std::size_t nonfinite(const Render& x) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < x.l.size(); ++i) n += !std::isfinite(x.l[i]) + !std::isfinite(x.r[i]);
  return n;
}

// BS.1770-4's K-weighting at 44.1 kHz (the filters' analog prototypes, bilinear), as OU-2 reads it: ungated, the mean of
// 400 ms blocks hopping 100 ms over [0, seconds).
double lufs(const Render& x, double seconds) {
  const double pi = 3.14159265358979323846;
  double K = std::tan(pi * 1681.974450955533 / kRate), Q = 0.7071752369554196;
  const double Vh = std::pow(10.0, 3.999843853973347 / 20), Vb = std::pow(Vh, 0.4996667741545416);
  double a0 = 1 + K / Q + K * K;
  const double pb[3] = {(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0};
  const double pa[3] = {1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0};
  K = std::tan(pi * 38.13547087602444 / kRate), Q = 0.5003270373238773;
  a0 = 1 + K / Q + K * K;
  const double rb[3] = {1, -2, 1};
  const double ra[3] = {1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0};
  const std::size_t n = std::min(x.l.size(), std::size_t(seconds * kRate));
  std::vector<double> sq(n, 0.0);
  for (const auto* ch : {&x.l, &x.r}) {
    double s1[2] = {0, 0}, s2[2] = {0, 0};
    for (std::size_t i = 0; i < n; ++i) {
      const double in = (*ch)[i];
      const double y1 = pb[0] * in + s1[0];
      s1[0] = pb[1] * in - pa[1] * y1 + s1[1];
      s1[1] = pb[2] * in - pa[2] * y1;
      const double y2 = rb[0] * y1 + s2[0];
      s2[0] = rb[1] * y1 - ra[1] * y2 + s2[1];
      s2[1] = rb[2] * y1 - ra[2] * y2;
      sq[i] += y2 * y2;
    }
  }
  const std::size_t win = std::size_t(0.4 * kRate), hop = std::size_t(0.1 * kRate);
  double sum = 0;
  int blocks = 0;
  for (std::size_t s = 0; s + win <= n; s += hop, ++blocks) {
    double m = 0;
    for (std::size_t i = s; i < s + win; ++i) m += sq[i];
    sum += m / double(win);
  }
  return blocks ? -0.691 + 10 * std::log10(sum / blocks) : -999;
}

// The quietest 10 ms window (mid of L and R, RMS, dBFS) in [from, to) seconds.
double quietest(const Render& x, double from, double to) {
  const std::size_t w = std::size_t(0.01 * kRate);
  double low = 1e9;
  for (std::size_t s = std::size_t(from * kRate); s + w <= std::min(x.l.size(), std::size_t(to * kRate)); s += w) {
    double m = 0;
    for (std::size_t i = s; i < s + w; ++i) m += 0.5 * (double(x.l[i]) * x.l[i] + double(x.r[i]) * x.r[i]);
    low = std::min(low, db(std::sqrt(m / double(w))));
  }
  return low;
}

// The loudest 10 ms window in each strike's first 150 ms, the quietest of those: a struck sound that sounds every time.
double weakest_strike(const Render& x, double step) {
  const std::size_t w = std::size_t(0.01 * kRate);
  double low = 1e9;
  for (double t = 0; t < x.held_s - 1e-9; t += step) {
    double high = -999;
    for (std::size_t s = std::size_t(t * kRate); s + w <= std::size_t((t + 0.15) * kRate) && s + w <= x.l.size(); s += w) {
      double m = 0;
      for (std::size_t i = s; i < s + w; ++i) m += 0.5 * (double(x.l[i]) * x.l[i] + double(x.r[i]) * x.r[i]);
      high = std::max(high, db(std::sqrt(m / double(w))));
    }
    low = std::min(low, high);
  }
  return low;
}

void fft(std::vector<std::complex<double>>& a) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2 * 3.14159265358979323846 / double(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w(1);
      for (std::size_t j = 0; j < len / 2; ++j, w *= wl) {
        const auto u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
      }
    }
  }
}

// The mean power spectrum of the mid over [from, to): 4096-point Hann frames hopping 2048.
std::vector<double> spectrum(const Render& x, double from, double to) {
  constexpr std::size_t N = 4096;
  std::vector<double> p(N / 2, 0.0);
  int frames = 0;
  for (std::size_t s = std::size_t(from * kRate); s + N <= std::min(x.l.size(), std::size_t(to * kRate)); s += N / 2, ++frames) {
    std::vector<std::complex<double>> a(N);
    for (std::size_t i = 0; i < N; ++i) a[i] = 0.5 * (x.l[s + i] + x.r[s + i]) * (0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * double(i) / double(N)));
    fft(a);
    for (std::size_t k = 0; k < N / 2; ++k) p[k] += std::norm(a[k]);
  }
  for (auto& q : p) q /= std::max(1, frames);
  return p;
}
double centroid(const std::vector<double>& p) {
  double num = 0, den = 0;
  for (std::size_t k = 1; k < p.size(); ++k) num += double(k) * kRate / 8192 * p[k], den += p[k];
  return den > 0 ? num / den : 0;
}
double brightness(const std::vector<double>& p) {   // the share of power above 2 kHz
  double hi = 0, all = 0;
  for (std::size_t k = 1; k < p.size(); ++k) (double(k) * kRate / 8192 >= 2000 ? hi : all) += p[k];
  return hi + all > 0 ? hi / (hi + all) : 0;
}
// Third-octave band levels (dB, 50 Hz to 16 kHz), each normalised by the total, for comparing two corners' colour.
std::vector<double> bands(const std::vector<double>& p) {
  double total = 0;
  for (std::size_t k = 1; k < p.size(); ++k) total += p[k];
  std::vector<double> out;
  for (double f = 50; f < 16000; f *= std::pow(2.0, 1.0 / 3)) {
    double sum = 0;
    for (std::size_t k = 1; k < p.size(); ++k) {
      const double hz = double(k) * kRate / 8192;
      if (hz >= f && hz < f * std::pow(2.0, 1.0 / 3)) sum += p[k];
    }
    out.push_back(10 * std::log10(std::max(sum / std::max(total, 1e-30), 1e-9)));
  }
  return out;
}
double band_distance(const std::vector<double>& a, const std::vector<double>& b) {   // mean |dB| over bands within 60 dB of the top
  double sum = 0;
  int n = 0;
  const double top = std::max(*std::max_element(a.begin(), a.end()), *std::max_element(b.begin(), b.end()));
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::max(a[i], b[i]) < top - 60) continue;
    sum += std::fabs(std::max(a[i], top - 60) - std::max(b[i], top - 60));
    ++n;
  }
  return n ? sum / n : 0;
}

void write_wav(const std::string& path, const Render& x) {
  std::ofstream f(path, std::ios::binary);
  const std::uint32_t n = std::uint32_t(x.l.size()), bytes = n * 4;
  auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  f.write("RIFF", 4); u32(36 + bytes); f.write("WAVEfmt ", 8); u32(16); u16(1); u16(2); u32(44100); u32(44100 * 4); u16(4); u16(16);
  f.write("data", 4); u32(bytes);
  for (std::uint32_t i = 0; i < n; ++i)
    for (float s : {x.l[i], x.r[i]}) {
      const double c = std::isfinite(s) ? std::clamp(double(s), -1.0, 1.0) : 0.0;
      u16(std::uint16_t(std::int16_t(std::lround(c * 32767))));
    }
}

std::string num(double x) {
  char b[40];
  std::snprintf(b, sizeof b, "%.4g", x);
  return b;
}
std::string slug(const std::string& name) {
  std::string s;
  for (char c : name) s += std::isalnum(static_cast<unsigned char>(c)) ? char(std::tolower(static_cast<unsigned char>(c))) : '-';
  return s;
}

struct Measured { double peak_db, lufs, dc, quiet, strike, centroid, bright, cpu_worst, cpu_p99, cpu_mean; std::size_t bad; std::vector<double> bands; };
Measured measure(const Render& x, double retrigger) {
  Measured m{};
  m.peak_db = db(peak_of(x));
  m.lufs = lufs(x, std::min(kHold, x.held_s));
  double dl = 0, dr = 0;
  const std::size_t n = std::min(x.l.size(), std::size_t(x.held_s * kRate));
  for (std::size_t i = 0; i < n; ++i) dl += x.l[i], dr += x.r[i];
  m.dc = std::max(std::fabs(dl), std::fabs(dr)) / double(std::max<std::size_t>(n, 1));
  m.quiet = retrigger > 0 ? NAN : quietest(x, 1.0, x.held_s);
  m.strike = retrigger > 0 ? weakest_strike(x, retrigger) : NAN;
  const auto p = spectrum(x, retrigger > 0 ? 0 : 0.5, x.held_s);
  m.centroid = centroid(p);
  m.bright = brightness(p);
  m.bands = bands(p);
  std::vector<double> s = x.block_share;
  std::sort(s.begin(), s.end());
  m.cpu_worst = s.empty() ? 0 : s.back();
  m.cpu_p99 = s.empty() ? 0 : s[std::size_t(0.99 * double(s.size() - 1))];
  double sum = 0;
  for (double q : s) sum += q;
  m.cpu_mean = s.empty() ? 0 : sum / double(s.size());
  m.bad = nonfinite(x);
  return m;
}
std::string json_of(const std::string& file, const Measured& m) {
  auto f = [](double x) { return std::isfinite(x) ? num(x) : std::string("null"); };
  return "{\"file\":\"" + file + "\",\"peak_db\":" + f(m.peak_db) + ",\"lufs\":" + f(m.lufs) + ",\"dc\":" + f(m.dc) + ",\"quietest_db\":" + f(m.quiet) +
         ",\"weakest_strike_db\":" + f(m.strike) + ",\"centroid_hz\":" + f(m.centroid) + ",\"brightness\":" + f(m.bright) + ",\"cpu_worst\":" + f(m.cpu_worst) +
         ",\"cpu_p99\":" + f(m.cpu_p99) + ",\"cpu_mean\":" + f(m.cpu_mean) + ",\"nonfinite\":" + std::to_string(m.bad) + "}";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "usage: preview-demo-render <presets.json> <demo_bank.json> <out dir> [--only <name>] [--target <LUFS>]\n"); return 2; }
  std::string only;
  double target = NAN;
  for (int i = 4; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--only")) only = argv[i + 1];
    if (!std::strcmp(argv[i], "--target")) target = std::atof(argv[i + 1]);
    if (!std::strcmp(argv[i], "--runs")) g_runs = std::max(1, std::atoi(argv[i + 1]));
  }
  auto slurp = [](const char* path) { std::ifstream in(path); std::stringstream s; s << in.rdbuf(); return s.str(); };
  std::vector<horde::BankPreset> factory;
  if (!horde::translate_bank(slurp(argv[1]), factory)) { std::fprintf(stderr, "no factory bank\n"); return 1; }
  std::vector<preview::demo::Demo> demos;
  std::string error;
  if (!preview::demo::read(slurp(argv[2]), factory, demos, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
  const std::string dir = argv[3];
  std::string all = "[\n";
  bool first = true;
  int failures = 0;
  for (const auto& d : demos) {
    if (!only.empty() && d.name != only) continue;
    const std::string base = slug(d.name);
    std::vector<int> chord(kChord, kChord + 4);
    std::string entry = "{\"name\":\"" + d.name + "\",\"description\":\"" + d.description + "\",\"retrigger\":" + num(d.retrigger_s) + ",\"corners\":[";
    Measured corner[4];
    std::vector<int> unison[2];
    for (int c = 0; c < 4; ++c) {
      auto e = horde::make_engine();
      e->prepare(kRate, 64);
      e->load_state(d.corners[c]);
      for (int o = 0; o < 2; ++o) {
        const horde::ParamInfo* row = horde::find_param(o ? "osc2.unison" : "osc1.unison");
        unison[o].push_back(int(row->lo + std::round(e->param_v(row->id) * double(row->steps - 1))));
      }
      const Render x = timed(d, chord, kHold, c);
      corner[c] = measure(x, d.retrigger_s);
      const std::string file = base + "-" + "abcd"[c] + ".wav";
      write_wav(dir + "/" + file, x);
      entry += std::string(c ? "," : "") + json_of(file, corner[c]);
    }
    entry += "],";
    const Render sweep = timed(d, chord, 4 * kEdge, -1);
    const Measured sw = measure(sweep, d.retrigger_s);
    write_wav(dir + "/" + base + "-sweep.wav", sweep);
    entry += "\"sweep\":" + json_of(base + "-sweep.wav", sw);
    if (d.single_key >= 0) {
      std::vector<int> one{d.single_key};
      entry += ",\"single\":[";
      for (int c = 0; c < 4; ++c) {
        const Render x = timed(d, one, kHold, c);
        const std::string file = base + "-single-" + "abcd"[c] + ".wav";
        write_wav(dir + "/" + file, x);
        entry += std::string(c ? "," : "") + json_of(file, measure(x, d.retrigger_s));
      }
      const Render s1 = timed(d, one, 4 * kEdge, -1);
      write_wav(dir + "/" + base + "-single-sweep.wav", s1);
      entry += "],\"single_sweep\":" + json_of(base + "-single-sweep.wav", measure(s1, d.retrigger_s));
    }
    // How far apart the corners are: the smallest pair's colour distance, and the spread of centroid and loudness.
    double closest = 1e9, cmin = 1e9, cmax = 0, lmin = 1e9, lmax = -1e9;
    std::string pair;
    for (int a = 0; a < 4; ++a) {
      cmin = std::min(cmin, corner[a].centroid), cmax = std::max(cmax, corner[a].centroid);
      lmin = std::min(lmin, corner[a].lufs), lmax = std::max(lmax, corner[a].lufs);
      for (int b = a + 1; b < 4; ++b) {
        const double apart = band_distance(corner[a].bands, corner[b].bands);
        if (apart < closest) closest = apart, pair = std::string(1, "ABCD"[a]) + "ABCD"[b];
      }
    }
    entry += ",\"closest_pair\":\"" + pair + "\",\"closest_pair_db\":" + num(closest) + ",\"centroid_ratio\":" + num(cmax / std::max(cmin, 1.0)) + ",\"lufs_spread\":" + num(lmax - lmin);
    entry += ",\"unison\":[[" + std::to_string(unison[0][0]) + "," + std::to_string(unison[0][1]) + "," + std::to_string(unison[0][2]) + "," +
             std::to_string(unison[0][3]) + "],[" + std::to_string(unison[1][0]) + "," + std::to_string(unison[1][1]) + "," + std::to_string(unison[1][2]) +
             "," + std::to_string(unison[1][3]) + "]]";
    // The level move each corner needs: to the target loudness, then down to a -1 dBFS peak, the sweep's peak included.
    if (std::isfinite(target)) {
      entry += ",\"level_move_db\":[";
      for (int c = 0; c < 4; ++c) {
        double move = target - corner[c].lufs;
        move = std::min(move, -1.0 - corner[c].peak_db);
        entry += std::string(c ? "," : "") + num(move);
      }
      entry += "],\"sweep_peak_after_db\":" + num(sw.peak_db);
    }
    entry += "}";
    all += std::string(first ? "" : ",\n") + entry;
    first = false;

    // The gates, printed: silence, clipping, NaN, DC, CPU, unison, and corners that differ.
    double worst = sw.cpu_worst;
    bool silent = false, clip = sw.peak_db > -1, bad = sw.bad > 0, dc = sw.dc > 0.01;
    auto quiet = [&](const Measured& m) { return d.retrigger_s > 0 ? m.strike < -45 : m.quiet < -60; };
    silent = quiet(sw);
    int max_unison = 0;
    for (int c = 0; c < 4; ++c) {
      worst = std::max(worst, corner[c].cpu_worst);
      silent = silent || quiet(corner[c]);
      clip = clip || corner[c].peak_db > -1;
      bad = bad || corner[c].bad > 0;
      dc = dc || corner[c].dc > 0.01;
      max_unison = std::max({max_unison, unison[0][std::size_t(c)], unison[1][std::size_t(c)]});
    }
    const bool apart = closest >= 2.0;
    std::printf("%-22s worst block %5.1f %%  p99 %5.1f %%  sweep quiet %6.1f dB  peaks %s/%s/%s/%s sweep %s  LUFS %s/%s/%s/%s  centroids %s/%s/%s/%s  closest %s %4.1f dB  %s%s%s%s%s%s%s\n",
                d.name.c_str(), 100 * worst, 100 * sw.cpu_p99, d.retrigger_s > 0 ? sw.strike : sw.quiet, num(corner[0].peak_db).c_str(), num(corner[1].peak_db).c_str(),
                num(corner[2].peak_db).c_str(), num(corner[3].peak_db).c_str(), num(sw.peak_db).c_str(), num(corner[0].lufs).c_str(), num(corner[1].lufs).c_str(),
                num(corner[2].lufs).c_str(), num(corner[3].lufs).c_str(), num(corner[0].centroid).c_str(), num(corner[1].centroid).c_str(), num(corner[2].centroid).c_str(),
                num(corner[3].centroid).c_str(), pair.c_str(), closest, silent ? "SILENCE " : "", clip ? "CLIP " : "", bad ? "NAN " : "", dc ? "DC " : "",
                worst > 0.7 ? "CPU " : "", max_unison > 9 ? "UNISON " : "", apart ? "" : "ALIKE ");
    failures += silent || clip || bad || dc || max_unison > 9 || !apart;
  }
  all += "\n]\n";
  std::ofstream(dir + "/metrics.json") << all;
  if (!g_deterministic) std::printf("RED: a render differed between runs\n");
  return failures || !g_deterministic ? 1 : 0;
}
