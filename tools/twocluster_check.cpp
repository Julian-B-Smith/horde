/* twocluster_check — negative K means something in two-cluster mode (ADR-164;
   human 2026-09-14: "with A/B balance turned on, it doesn't seem like K is
   actually going negative"). Drives SwarmCore directly like trajectory_check
   (DYN base: dist 0, lpOut 0, n 24, detune 0.2, retrig 0), reads R_A / R_B
   after 3 s. Exit 1 on failure.
   T1 K=+0.8 balance 0: both clusters sync (the unchanged corner).
   T2 K=-0.8 balance 0: neither cluster syncs — repulsive within both.
   T3 K=-0.8 balance 1: A dissolved, B synced — the MIRROR of T4.
   T4 K=+0.8 balance 1: A synced, B dissolved (L0-23's corner, unchanged).
   T5 CONTROL: K=0 balance 0 reads like T2 (no coupling) — so T2's low R is
      not a coincidence of K<0 merely being "off": T2 and T5 must both be
 * WIRED: ./verify full.
      low, and T3 must differ from T5 in B. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include "../src/swarm_core.h"
using hypersaw::SwarmCore;
namespace {
constexpr double kSR = 44100.0; constexpr int kBlock = 1024; constexpr int kMidi = 57;
double mtof(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
int fails = 0;
void expect(bool ok, const char *w, double a, double b) { std::printf("  %s  %s  (R_A %.3f, R_B %.3f)\n", ok ? "ok  " : "FAIL", w, a, b); if (!ok) fails++; }
std::pair<double, double> run(double K, double balance) {
  SwarmCore c(kSR);
  c.setParam("dist", 0); c.setParam("lpOut", 0); c.setParam("n", 24); c.setParam("detune", 0.2); c.setParam("retrig", 0);
  c.setParam("topo", 2); c.setParam("mu", 0.3); c.setParam("detune", 0.3); c.setParam("K", K); c.setParam("balance", balance);
  c.noteOn(kMidi, mtof(kMidi));
  std::vector<float> L(kBlock), R(kBlock);
  for (long off = 0; off < (long)(3 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
  return { c.focus()->RA, c.focus()->RB };
}
}
int main() {
  const auto t1 = run(0.8, 0.0), t2 = run(-0.8, 0.0), t3 = run(-0.8, 1.0), t4 = run(0.8, 1.0), t5 = run(0.0, 0.0);
  expect(t1.first >= 0.9 && t1.second >= 0.9, "T1 K=+0.8 balance 0: both clusters sync", t1.first, t1.second);
  expect(t2.first < 0.5 && t2.second < 0.5, "T2 K=-0.8 balance 0: neither cluster syncs (repulsive)", t2.first, t2.second);
  expect(t3.first < 0.5 && t3.second >= 0.9, "T3 K=-0.8 balance 1: A dissolved, B synced (mirror of T4)", t3.first, t3.second);
  expect(t4.first >= 0.9 && t4.second < 0.5, "T4 K=+0.8 balance 1: A synced, B dissolved", t4.first, t4.second);
  expect(t5.first < 0.5 && t5.second < 0.5 && (t3.second - t5.second) > 0.4, "T5 control: K=0 is low too, and T3's B is a real sync, not K merely off", t5.first, t5.second);
  std::printf("twocluster_check: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
