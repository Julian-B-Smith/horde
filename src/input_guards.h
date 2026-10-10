/*
 * input_guards.h — the boundary guards for values that arrive from OUTSIDE the
 * plugin: text that crosses the GUI bridge, and the note, expression, tempo
 * and sample-rate values a host delivers. Each guard sits at the one place the value enters, so
 * the code behind it may assume a well-formed value rather than re-checking it.
 *
 * Dependency-free on purpose (standard library only, no choc, no CLAP), the
 * preset_store.h idiom: the shell, the GUI backends and a headless check all
 * include this one header, and tools/bridge_utf8_check.cpp and
 * tools/hostile_events_check.cpp test the same functions the shipped code calls.
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace hypersaw
{

/* ---- text: valid UTF-8 at every cut and at the bridge -------------------- */

/* Length of the well-formed UTF-8 sequence that starts at s[i], or 0 when the
   bytes there are not one (RFC 3629: no overlong forms, no UTF-16 surrogates,
   nothing above U+10FFFF, no sequence cut short by the end of the input). */
inline size_t utf8SeqLen(std::string_view s, size_t i)
{
  const unsigned char b0 = (unsigned char)s[i];
  if (b0 < 0x80) return 1;
  size_t n;
  uint32_t cp, lowest;
  if (b0 >= 0xC2 && b0 <= 0xDF) { n = 2; cp = b0 & 0x1Fu; lowest = 0x80; }
  else if ((b0 & 0xF0) == 0xE0) { n = 3; cp = b0 & 0x0Fu; lowest = 0x800; }
  else if (b0 >= 0xF0 && b0 <= 0xF4) { n = 4; cp = b0 & 0x07u; lowest = 0x10000; }
  else return 0;   // a continuation byte, C0/C1 (always overlong), or F5..FF
  if (n > s.size() - i) return 0;
  for (size_t k = 1; k < n; k++)
  {
    const unsigned char c = (unsigned char)s[i + k];
    if ((c & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (c & 0x3Fu);
  }
  if (cp < lowest || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
  return n;
}

/* The input as valid UTF-8 of at most `maxBytes` bytes. Each byte that does not
   begin a well-formed sequence becomes U+FFFD, and the cut falls BEFORE the
   first character that would not fit, never inside one.

   Why: the webview library's string iteration requires well-formed UTF-8 (its
   own precondition, choc_UTF8.h: "make sure the source gets validated before
   iterating"), and its checks of that are asserts, absent from Release. A cut by
   byte count (`substr(0, 60)`) can split a character, and a session or preset
   file can carry any bytes, so text is made valid before it reaches the
   library. Valid input that fits comes back byte-identical, so every existing
   name is unchanged. */
inline std::string utf8Clean(std::string_view in, size_t maxBytes = (size_t)-1)
{
  static constexpr char kReplacement[] = "\xEF\xBF\xBD";   // U+FFFD
  std::string out;
  out.reserve(in.size() < maxBytes ? in.size() : maxBytes);
  for (size_t i = 0; i < in.size();)
  {
    const size_t n = utf8SeqLen(in, i);
    const size_t emit = n ? n : 3;
    if (emit > maxBytes - out.size()) break;
    if (n) out.append(in.data() + i, n);
    else out.append(kReplacement, 3);
    i += n ? n : 1;
  }
  return out;
}

/* ---- state text from the page or the clipboard: one size cap -------------- */

/* The most bytes of state JSON the GUI accepts from the page (hzApplyState) or
   from the clipboard (hzPasteState, both backends). One constant, so the three
   doors cannot drift apart (critic MEDIUM-3, PR #973).
   Derivation: 4 x the largest factory preset's state, rounded up to a power of
   two. The largest is docs/presets/factory/lead/LD - Glass Reed.json at 9614
   bytes (2026-10-08); 4 x 9614 = 38456; the next power of two is 65536.
   tools/paste_cap_check re-derives this from the bank on every `verify fast`,
   so a bank that outgrows it turns red rather than refusing a factory patch.
   The shell's own state paths (undo, history, the debug door) are not capped:
   they never carry outside text. */
constexpr size_t kMaxPastedStateBytes = 65536;

constexpr bool pastedStateFits(size_t bytes) { return bytes <= kMaxPastedStateBytes; }

/* PASTE's status, the contract both backends' hzPasteState return to the page:
   0 nothing to paste, 1 applied, 2 not a patch (over the cap, or refused by
   `apply`). The cap is checked on the view's SIZE before `apply` runs, so an
   oversized text is refused with nothing copied; `apply` receives the view and
   does its own copy. tools/paste_cap_check counts allocations to show it. */
template <typename Apply>
int pasteStatus(std::string_view text, Apply &&apply)
{
  if (text.empty()) return 0;
  if (!pastedStateFits(text.size())) return 2;
  return apply(text) ? 1 : 2;
}

/* ---- host note, expression and tempo values ------------------------------- */

/* CLAP's note key range (clap/events.h: "0..127, same as MIDI1 Key Number, -1
   for wildcard"). A note-on must name a real key; -1 is meaningful only on
   events that MATCH existing notes (note-off, choke, expressions). An event
   outside this is dropped at the boundary rather than clamped: a clamped key
   20000 would play key 127, a note nobody asked for. */
constexpr int kNoteKeyMax = 127;
inline bool noteKeyInRange(int key, bool wildcardOk)
{
  return (key >= 0 && key <= kNoteKeyMax) || (wildcardOk && key == -1);
}

/* CLAP's TUNING expression range (clap/events.h: "Relative tuning in
   semitones, from -120 to +120"). Unclamped, a large value made the voice
   frequency infinite and the voice output non-finite. */
constexpr double kTuningSemisMax = 120.0;

/* A host expression value made safe to apply: false when it is not finite (the
   event is then dropped, leaving the last good value in place), else `out` is
   the value clamped to [lo, hi]. Finite in-range values pass through exactly. */
inline bool finiteClamp(double v, double lo, double hi, double &out)
{
  if (!std::isfinite(v)) return false;
  out = v < lo ? lo : (v > hi ? hi : v);
  return true;
}

/* The lowest host tempo the shell takes, in BPM. Not a new number: it is the
   bound the tempo's readers already share (`bpm > 1`, in the shell's LFO and
   quantise-time sync, fx_rack.h setTempo and delay_core.h setTempo). Named
   once here so the entry site applies the readers' rule, not a second one.
   The code defines no upper bound for a tempo, so none is applied. */
constexpr double kHostTempoFloorBpm = 1.0;

/* Is a host tempo one the shell can take: finite and above the floor. The
   value is tested, never altered — a tempo that passes is stored exactly. */
inline bool hostTempoUsable(double bpm)
{
  double finite = 0;
  return finiteClamp(bpm, kHostTempoFloorBpm, std::numeric_limits<double>::max(), finite) &&
         bpm > kHostTempoFloorBpm;
}

/* The host sample rates the shell activates at, in Hz (B455).
   STRUCTURAL bounds (what can be a real rate at all), NOT the certified
   range, which stays 44.1 to 192 kHz (docs/ROBUSTNESS.md).
   The one buffer sized from the rate is the rack's comb bank (fx_rack.h
   setSampleRate: 8 lines x 2 channels of rate/20 samples), about 2.5 MB of
   floats at the upper bound; every other line is a fixed buffer that clamps.
   A rate inside the bounds is taken exactly as sent. */
constexpr double kHostSampleRateMin = 8000.0;
constexpr double kHostSampleRateMax = 768000.0;

/* Is a host sample rate one the shell activates at: finite and inside the
   bounds above. The value is tested, never altered (hostTempoUsable's rule):
   finiteClamp returns the rate itself only when it was already in range. */
inline bool hostSampleRateUsable(double sr)
{
  double inRange = 0;
  return finiteClamp(sr, kHostSampleRateMin, kHostSampleRateMax, inRange) && inRange == sr;
}

/* The last line before the host's bus: any sample that is not finite becomes
   0. Defence in depth — the boundary guards above should leave nothing for it
   to catch — because one NaN on a DAW bus can silence the whole mix until the
   plugin is reset. Finite samples are never written, so output is bit-identical
   whenever there is nothing to catch. Returns how many samples it replaced. */
inline uint32_t zeroNonFinite(float *buf, uint32_t n)
{
  uint32_t hits = 0;
  for (uint32_t i = 0; i < n; i++)
    if (!std::isfinite(buf[i])) { buf[i] = 0.0f; hits++; }
  return hits;
}

}  // namespace hypersaw
