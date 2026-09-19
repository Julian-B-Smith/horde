/*
 * mod_check — invariants for mod_core.h (matrix increment 1).
 *
 * The core is small enough to look obviously correct, which is exactly when an
 * oracle earns its keep: the invariants below pin the CONTRACT (sum law, scope
 * segregation, refusal semantics) so a later "small" change that bends one is
 * caught by a red gate instead of by a player. Each check prints its evidence;
 * the must-fire style follows the repo's probe discipline.
 * UNWIRED: reason not stated — see B159.
 */
#include <cstdio>
#include <cmath>
#include <cstring>
#include "../src/mod_core.h"

static int fails = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("  %-4s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) fails++;
}

int main()
{
  using hypersaw::ModCore;
  char d[256];
  uint32_t dests[ModCore::kMaxRoutes];
  double deltas[ModCore::kMaxRoutes];

  // 1. Empty matrix: no destinations, ever.
  {
    ModCore m;
    m.src[0] = 1.0;
    const int n = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    std::snprintf(d, sizeof d, "sources hot, zero routes -> %d entries", n);
    check(n == 0, "empty matrix contributes nothing", d);
  }

  // 2. One route: delta = depth * source, linear in both.
  {
    ModCore m;
    m.addRoute(0, 42, 0.5, ModCore::kGlobal);
    m.src[0] = 0.8;
    m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    const double a = deltas[0];
    m.src[0] = 1.6;                       // double the source
    m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    const double b = deltas[0];
    std::snprintf(d, sizeof d, "0.5*0.8=%.3f, source doubled -> %.3f", a, b);
    check(std::fabs(a - 0.4) < 1e-12 && std::fabs(b - 0.8) < 1e-12,
          "one route is depth*source, linear", d);
  }

  // 3. Two routes to ONE destination sum; to TWO destinations stay separate.
  {
    ModCore m;
    m.addRoute(0, 42, 0.5, ModCore::kGlobal);
    m.addRoute(1, 42, -0.25, ModCore::kGlobal);
    m.addRoute(1, 43, 1.0, ModCore::kGlobal);
    m.src[0] = 1.0; m.src[1] = 1.0;
    const int n = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    double at42 = 0, at43 = 0;
    for (int i = 0; i < n; i++) (dests[i] == 42 ? at42 : at43) = deltas[i];
    std::snprintf(d, sizeof d, "%d entries; dest42=%.3f (0.5-0.25), dest43=%.3f", n, at42, at43);
    check(n == 2 && std::fabs(at42 - 0.25) < 1e-12 && std::fabs(at43 - 1.0) < 1e-12,
          "contributions sum per destination, destinations stay distinct", d);
  }

  // 4. Scope segregation: a per-note route is INVISIBLE to a global evaluate
  //    and vice versa — the B34 vocabulary made mechanical, with both
  //    directions asserted so neither is a silent superset of the other.
  {
    ModCore m;
    m.addRoute(0, 42, 1.0, ModCore::kGlobal);
    m.addRoute(0, 43, 1.0, ModCore::kPerNote);
    m.src[0] = 1.0;
    const int ng = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    const bool gOnly = ng == 1 && dests[0] == 42;
    const int np = m.evaluate(ModCore::kPerNote, dests, deltas, ModCore::kMaxRoutes);
    const bool pOnly = np == 1 && dests[0] == 43;
    std::snprintf(d, sizeof d, "global sees %d (dest %u), per-note sees %d (dest %u)",
                  ng, gOnly ? 42u : dests[0], np, pOnly ? 43u : dests[0]);
    check(gOnly && pOnly, "scopes are segregated, both directions", d);
  }

  // 5. Refusal semantics: the 65th route and an out-of-range source are both
  //    REFUSED (false, count unchanged) — never silently dropped.
  {
    ModCore m;
    for (int i = 0; i < ModCore::kMaxRoutes; i++)
      m.addRoute(0, (uint32_t)i, 1.0, ModCore::kGlobal);
    const bool over = m.addRoute(0, 999, 1.0, ModCore::kGlobal);
    const bool badSrc = m.addRoute(ModCore::kMaxSources, 1, 1.0, ModCore::kGlobal);
    std::snprintf(d, sizeof d, "65th add -> %s at n=%d; src=%d add -> %s",
                  over ? "ACCEPTED" : "refused", m.nRoutes,
                  ModCore::kMaxSources, badSrc ? "ACCEPTED" : "refused");
    check(!over && !badSrc && m.nRoutes == ModCore::kMaxRoutes,
          "full table and bad source are refused, not eaten", d);
  }

  // 6. Inactive routes contribute nothing (the toggle is a real mute).
  {
    ModCore m;
    m.addRoute(0, 42, 1.0, ModCore::kGlobal);
    m.routes[0].active = 0;
    m.src[0] = 1.0;
    const int n = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    std::snprintf(d, sizeof d, "deactivated route -> %d entries", n);
    check(n == 0, "an inactive route is silent", d);
  }

  // 7. removeRoute keeps order (route order is the sum order and the GUI's
  //    list order; a remove that reshuffled would lie to both).
  {
    ModCore m;
    m.addRoute(0, 10, 1.0, ModCore::kGlobal);
    m.addRoute(0, 20, 1.0, ModCore::kGlobal);
    m.addRoute(0, 30, 1.0, ModCore::kGlobal);
    m.removeRoute(1);
    std::snprintf(d, sizeof d, "after removing middle: [%u, %u], n=%d",
                  m.routes[0].dest, m.routes[1].dest, m.nRoutes);
    check(m.nRoutes == 2 && m.routes[0].dest == 10 && m.routes[1].dest == 30,
          "remove preserves order", d);
  }

  std::printf("mod_check: %s (%d failures)\n", fails ? "RED" : "GREEN", fails);
  return fails ? 1 : 0;
}
