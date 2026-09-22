/*
 * parity_check — L0-1: C++ SwarmCore vs JS reference goldens.
 *
 * Reads build-golden/manifest.tsv (written by tools/golden/gen_goldens.mjs —
 * the single source of truth for the scenario matrix), re-renders each
 * scenario on the C++ core under the IDENTICAL protocol (A3 = midi 57, 4 s at
 * 44.1 kHz, 1024-sample blocks, note-off before the first block at >= 3 s,
 * params applied in the manifest's serialized order), and reports RMS error
 * against the golden Float32LE stereo-interleaved render.
 *
 * Pass: every scenario RMS < 1e-6 (ACCEPTANCE L0-1). Exit nonzero otherwise.
 *
 * Usage: parity_check <build-golden-dir>
 * WIRED: ./verify full.
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "../src/swarm_core.h"

namespace
{
constexpr double kSR = 44100.0;
constexpr int kSeconds = 4;
constexpr int kNoteOffAt = 3;
constexpr int kBlock = 1024;
constexpr int kMidi = 57;
constexpr double kEps = 1e-6;

double mtof(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
}  // namespace

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "usage: parity_check <build-golden-dir>\n");
    return 64;
  }
  const std::string dir = argv[1];
  std::ifstream tsv(dir + "/manifest.tsv");
  if (!tsv)
  {
    std::fprintf(stderr, "parity_check: cannot open %s/manifest.tsv (run gen_goldens first)\n",
                 dir.c_str());
    return 1;
  }

  int failures = 0, count = 0;
  double worst = 0;
  std::string worstName;
  std::string line;
  while (std::getline(tsv, line))
  {
    if (line.empty()) continue;
    // name \t engine \t file \t seed \t k=v,k=v,... \t note+note
    std::vector<std::string> cols;
    {
      std::stringstream ss(line);
      std::string c;
      while (std::getline(ss, c, '\t')) cols.push_back(c);
    }
    if (cols.size() != 6)
    {
      std::fprintf(stderr, "parity_check: malformed manifest line: %s\n", line.c_str());
      return 1;
    }
    const std::string &name = cols[0], &file = cols[2], &paramList = cols[4];
    std::vector<int> notes;
    {
      std::stringstream ss(cols[5]);
      std::string c;
      while (std::getline(ss, c, '+')) notes.push_back(std::atoi(c.c_str()));
    }

    hypersaw::SwarmCore core(kSR);
    {
      std::stringstream ss(paramList);
      std::string kv;
      while (std::getline(ss, kv, ','))
      {
        const auto eq = kv.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = kv.substr(0, eq);
        const double val = std::atof(kv.c_str() + eq + 1);
        if (!core.setParam(key, val))
        {
          std::fprintf(stderr, "parity_check: unknown param '%s' in %s\n", key.c_str(),
                       name.c_str());
          return 1;
        }
      }
    }
    for (int m : notes) core.noteOn(m, mtof(m));

    std::ifstream gf(dir + "/" + file, std::ios::binary);
    if (!gf)
    {
      std::fprintf(stderr, "parity_check: missing golden %s\n", file.c_str());
      return 1;
    }

    const long total = (long)(kSeconds * kSR);
    std::vector<float> L(kBlock), R(kBlock), golden(kBlock * 2);
    double errSum = 0;
    long errN = 0;
    bool noteOffDone = false;
    for (long off = 0; off < total; off += kBlock)
    {
      if (!noteOffDone && off >= (long)(kNoteOffAt * kSR))
      {
        for (int m : notes) core.noteOff(m);
        noteOffDone = true;
      }
      core.render(L.data(), R.data(), kBlock);
      const long nf = std::min((long)kBlock, total - off);
      gf.read(reinterpret_cast<char *>(golden.data()), nf * 2 * sizeof(float));
      if (gf.gcount() != (std::streamsize)(nf * 2 * sizeof(float)))
      {
        std::fprintf(stderr, "parity_check: golden %s truncated\n", file.c_str());
        return 1;
      }
      for (long i = 0; i < nf; i++)
      {
        const double dl = (double)L[i] - (double)golden[i * 2];
        const double dr = (double)R[i] - (double)golden[i * 2 + 1];
        errSum += dl * dl + dr * dr;
        errN += 2;
      }
    }
    const double rms = std::sqrt(errSum / (double)errN);
    const bool ok = rms < kEps;
    if (!ok) failures++;
    if (rms > worst)
    {
      worst = rms;
      worstName = name;
    }
    std::printf("%s %-24s rms=%.3e\n", ok ? "OK  " : "FAIL", name.c_str(), rms);
    count++;
  }

  std::printf("parity_check: %d/%d scenarios within eps=%.0e (worst %.3e @ %s)\n",
              count - failures, count, kEps, worst, worstName.c_str());
  return failures ? 1 : 0;
}
