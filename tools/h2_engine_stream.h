/*
 * h2_engine_stream.h — reads the scenario stream tools/h2_engine_render.mjs
 * writes, and replays a scenario through horde 2's composed engine
 * (h2/engine/engine.h). Shared by the parity check (tools/h2_engine_parity_check.cpp)
 * and any CPU bench that replays the same scripts, so they can never replay a
 * script differently. Include AFTER engine.h (the check defines H2_ENGINE_FAULTS
 * first, which this file honours).
 *
 * The stream is line-oriented text with binary payloads:
 *   H2ENGINE 1 <n>                                    header
 *   LIBM <fn> <n> + n*3 float64 (x, y, JS f(x[, y]))  libm probes (optional)
 *   ORACLE <path> <git blob sha1>                     one per golden file
 *   SCN <i> <name> / SR / SEED / commands / EV ... / NI <0|1> / [EXCL / SELF]
 *       / DATA <frames> + frames*2 float64 (the PRISTINE golden's samples)
 *   END <n>
 * Commands: set <key> <number> | sets <key> <string> | snap | on <note> <freq> <vel>
 *   | off <note> | re <note> <freq> | panic | render <block> <count> | cap <n>
 *   | capPolicy <n>. Numbers are JS's shortest round-trip spelling, so strtod
 *   recovers the exact double the golden used.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace h2engine_stream {

struct Cmd { std::string op, key, str; double a = 0, b = 0, c = 0; };
struct Scenario {
  int index = -1;
  std::string name, excl;
  double sr = 48000;
  uint32_t seed = 0;
  std::vector<Cmd> cmds;
  uint64_t ev[5] = {0, 0, 0, 0, 0};
  uint32_t h1 = 0, h2 = 0;
  int ni = -1;   // 1: the instrumented copy's samples equal the pristine golden's
  bool hasSelf = false;
  double selfRms = 0, selfMax = 0;
  std::vector<double> js;   // the golden's samples, interleaved L/R
};

inline bool readLine(FILE* f, std::string& out) {
  out.clear();
  int ch;
  while ((ch = std::fgetc(f)) != EOF) { if (ch == '\n') return true; out.push_back(static_cast<char>(ch)); }
  return !out.empty();
}
inline std::string word(const std::string& s, size_t& p) {
  while (p < s.size() && s[p] == ' ') p++;
  const size_t b = p;
  while (p < s.size() && s[p] != ' ') p++;
  return s.substr(b, p - b);
}
inline std::string rest(const std::string& s, size_t p) { return p < s.size() ? s.substr(p + 1) : std::string(); }

// Reads from the line after `SCN ...` through the DATA payload.
inline bool readScenario(FILE* f, const std::string& first, Scenario& sc) {
  size_t p = 0;
  word(first, p);
  sc.index = std::atoi(word(first, p).c_str());
  sc.name = rest(first, p);
  std::string line;
  for (;;) {
    if (!readLine(f, line)) return false;
    p = 0;
    const std::string op = word(line, p);
    if (op == "SR") sc.sr = std::strtod(word(line, p).c_str(), nullptr);
    else if (op == "SEED") sc.seed = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
    else if (op == "set") { Cmd c; c.op = op; c.key = word(line, p); c.a = std::strtod(word(line, p).c_str(), nullptr); sc.cmds.push_back(c); }
    else if (op == "sets") { Cmd c; c.op = op; c.key = word(line, p); c.str = rest(line, p); sc.cmds.push_back(c); }
    else if (op == "snap" || op == "off" || op == "on" || op == "render" || op == "re" || op == "panic" || op == "cap" || op == "capPolicy") {
      Cmd c; c.op = op;
      c.a = std::strtod(word(line, p).c_str(), nullptr);
      c.b = std::strtod(word(line, p).c_str(), nullptr);
      c.c = std::strtod(word(line, p).c_str(), nullptr);
      sc.cmds.push_back(c);
    }
    else if (op == "EV") {
      for (int k = 1; k <= 4; k++) sc.ev[k] = std::strtoull(word(line, p).c_str(), nullptr, 10);
      sc.h1 = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
      sc.h2 = static_cast<uint32_t>(std::strtoul(word(line, p).c_str(), nullptr, 10));
    }
    else if (op == "NI") sc.ni = std::atoi(word(line, p).c_str());
    else if (op == "EXCL") sc.excl = rest(line, p);
    else if (op == "SELF") { sc.hasSelf = true; sc.selfRms = std::strtod(word(line, p).c_str(), nullptr); sc.selfMax = std::strtod(word(line, p).c_str(), nullptr); }
    else if (op == "DATA") {
      const size_t frames = std::strtoull(word(line, p).c_str(), nullptr, 10);
      sc.js.resize(frames * 2);
      return std::fread(sc.js.data(), sizeof(double), frames * 2, f) == frames * 2;
    }
    else { std::fprintf(stderr, "h2_engine stream: unknown line '%s'\n", line.c_str()); return false; }
  }
}

// Replays `sc` through a fresh engine, seeded as the golden was. `out` receives
// the interleaved samples. `fault` / `eps` plant a must-fail control where the
// engine was compiled with H2_ENGINE_FAULTS; `log` receives the blade events.
inline void replay(const Scenario& sc, std::vector<double>& out, int fault = 0, horde2::engine::EventLog* log = nullptr,
                   double eps = 0) {
  auto* c = new horde2::engine::Engine(sc.sr);   // 114 KB (arm64): the heap, not a small stack
#ifdef H2_ENGINE_FAULTS
  c->fault = fault;
  c->faultEps = eps;
  c->armFaults();
#else
  (void)fault; (void)eps;
#endif
  c->events = log;
  c->seedRandom(sc.seed);
  out.clear();
  out.reserve(sc.js.size());
  std::vector<double> L, R;
  for (const Cmd& m : sc.cmds) {
    if (m.op == "set") { if (!c->set(m.key.c_str(), m.a)) std::fprintf(stderr, "  (unknown key %s ignored, as the golden does)\n", m.key.c_str()); }
    else if (m.op == "sets") c->setString(m.key.c_str(), m.str.c_str());
    else if (m.op == "snap") c->snap();
    else if (m.op == "on") c->noteOn(static_cast<int>(m.a), m.b, m.c);
    else if (m.op == "off") c->noteOff(static_cast<int>(m.a));
    else if (m.op == "re") c->retune(static_cast<int>(m.a), m.b);
    else if (m.op == "panic") c->panic();
    else if (m.op == "cap") c->setVoiceCap(m.a);
    else if (m.op == "capPolicy") c->setCapPolicy(m.a);
    else if (m.op == "render") {
      const int n = static_cast<int>(m.a), k = static_cast<int>(m.b);
      L.assign(n, 0); R.assign(n, 0);
      for (int b = 0; b < k; b++) { c->render(L.data(), R.data(), n); for (int i = 0; i < n; i++) { out.push_back(L[i]); out.push_back(R[i]); } }
    }
  }
  delete c;
}

}  // namespace h2engine_stream
