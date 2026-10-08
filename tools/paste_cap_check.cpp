/*
 * paste_cap_check — state text over the size cap is refused WITHOUT allocating
 * (B446, critic MEDIUM-3 on PR #973).
 *
 * WIRED: ./verify fast, compiled and run by tools/paste_cap_check.py
 *
 * WHAT. Drives hypersaw::pasteStatus (src/input_guards.h), the status both
 * backends' hzPasteState return, with texts at the cap's edges, and counts
 * every global operator new while it runs:
 *   - empty text: 0, the apply callback not called;
 *   - exactly kMaxPastedStateBytes: the callback called, its answer returned
 *     (1 accepted, 2 refused);
 *   - one byte over, and far over: 2, the callback NOT called, and ZERO
 *     allocations during the call (the texts live in static storage, so the
 *     only thing that could allocate is the code under test or the callback,
 *     which copies its text into a std::string as the real backends do);
 *   - pastedStateFits at cap and cap + 1.
 *
 * MUST-FAIL CONTROLS. The same rows run against two faulty versions, each of
 * which must FAIL at least one row: copy-then-check (the text copied into a
 * std::string before the size test: it refuses correctly but allocates), and
 * no cap (over-cap text handed to the callback). A control that passes is RED.
 *
 * NOT SHOWN. The platform halves: that Windows' clipboardTextUtf8 sizes before
 * it grows its buffer, and that macOS asks for lengths before UTF8String. Those
 * are read in code (hypersaw_gui_win.cpp, hypersaw_gui.mm); the Windows half is
 * compiled only by CI. The hzApplyState door's `pastedStateFits` test is pinned
 * by tools/paste_cap_check.py by source.
 */
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <string_view>

#include "../src/input_guards.h"

namespace
{
long gNews = 0;   // global operator new calls since the last reset
}

void *operator new(std::size_t n)
{
  ++gNews;
  if (void *p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

namespace
{
using hypersaw::kMaxPastedStateBytes;

// Static storage: building the inputs allocates nothing.
char gBig[kMaxPastedStateBytes * 2 + 1];

struct Seen
{
  bool called = false;
};

int realStatus(std::string_view t, const std::function<bool(std::string_view)> &apply)
{
  return hypersaw::pasteStatus(t, apply);
}
int copyThenCheck(std::string_view t, const std::function<bool(std::string_view)> &apply)
{
  std::string copy(t);   // the fault: allocates before the size test
  if (copy.empty()) return 0;
  if (copy.size() > kMaxPastedStateBytes) return 2;
  return apply(copy) ? 1 : 2;
}
int noCap(std::string_view t, const std::function<bool(std::string_view)> &apply)
{
  if (t.empty()) return 0;
  return apply(t) ? 1 : 2;
}

using StatusFn = int (*)(std::string_view, const std::function<bool(std::string_view)> &);

// The number of rows `status` gets wrong; prints them when `loud`.
int run(StatusFn status, bool loud)
{
  int wrong = 0;
  auto expect = [&](bool ok, const char *what) {
    if (!ok)
    {
      ++wrong;
      if (loud) std::printf("  FAIL  %s\n", what);
    }
  };
  for (auto &c : gBig) c = 'x';
  const std::string_view atCap(gBig, kMaxPastedStateBytes);
  const std::string_view overByOne(gBig, kMaxPastedStateBytes + 1);
  const std::string_view farOver(gBig, sizeof gBig);
  long news = 0;
  // The backends' callback shape: copy the text into a std::string, then apply.
  // The std::function is built BEFORE the counted window opens.
  auto call = [&](std::string_view text, bool accept, Seen &seen) {
    const std::function<bool(std::string_view)> f = [&seen, accept](std::string_view t) {
      seen.called = true;
      std::string copy(t);
      return accept && !copy.empty();
    };
    const long mark = gNews;
    const int r = status(text, f);
    news = gNews - mark;
    return r;
  };
  {
    Seen s;
    expect(call(std::string_view(), true, s) == 0 && !s.called, "empty text: status 0, apply not called");
  }
  {
    Seen s;
    expect(call(atCap, true, s) == 1 && s.called, "exactly the cap, accepted: status 1");
    Seen r;
    expect(call(atCap, false, r) == 2 && r.called, "exactly the cap, refused by apply: status 2");
  }
  {
    Seen s;
    const int st = call(overByOne, true, s);
    expect(st == 2 && !s.called, "cap + 1: status 2, apply not called");
    expect(news == 0, "cap + 1: no allocation");
  }
  {
    Seen s;
    const int st = call(farOver, true, s);
    expect(st == 2 && !s.called, "2 x cap: status 2, apply not called");
    expect(news == 0, "2 x cap: no allocation");
  }
  expect(hypersaw::pastedStateFits(kMaxPastedStateBytes), "pastedStateFits(cap)");
  expect(!hypersaw::pastedStateFits(kMaxPastedStateBytes + 1), "!pastedStateFits(cap + 1)");
  return wrong;
}
constexpr int kRows = 9;
}  // namespace

int main()
{
  int failures = 0;
  std::printf("paste_cap_check:\n");
  const int wrong = run(realStatus, true);
  std::printf("  %s  pasteStatus: %d rows, %d wrong (cap %zu bytes)\n", wrong ? "FAIL" : "PASS", kRows, wrong,
              kMaxPastedStateBytes);
  failures += wrong;
  const struct { const char *name; StatusFn f; } controls[] = {
      {"copy-then-check (allocates before the size test)", copyThenCheck},
      {"no cap", noCap},
  };
  for (const auto &c : controls)
  {
    const int w = run(c.f, false);
    std::printf("  %s  control %s: %d row(s) caught it\n", w ? "PASS" : "FAIL", c.name, w);
    if (w == 0) ++failures;
  }
  if (failures)
  {
    std::printf("paste_cap_check: RED (%d)\n", failures);
    return 1;
  }
  std::printf("paste_cap_check: GREEN (%d rows; 2 controls red as designed)\n", kRows);
  return 0;
}
