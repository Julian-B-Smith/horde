/*
 * render_cli.cpp — h2_harness_render: plays harness jobs through the enrolled
 * adapters and writes what they rendered. ROADMAP B448 phase 2, P2 (ADR-209).
 * The job language and the adapter contract are in harness.h; the users' guide is
 * h2/harness/README.md. Tools reach this through h2/harness/harness.mjs.
 *
 * BUILD. This is h2 code, so every build carries -ffp-contract=off (h2 rule 7: the
 * shipped build is the tested build). By hand, from the repo root:
 *   c++ -std=c++20 -O3 -ffp-contract=off h2/harness/render_cli.cpp -o <dir>/h2_harness_render
 *
 * MODES.
 *   h2_harness_render                (no arguments)
 *       A smoke run: every adapter in the table plays one short note at varying
 *       block sizes and must return the frames asked for at a level in range; the tone
 *       control must be loud and the silent control exactly zero. It exists because
 *       CI's sanitizer job (tools/sanitize_oracles.sh) runs every "$build_dir/<name>"
 *       that ./verify names, bare: this is what puts each engine's render under
 *       AddressSanitizer, UBSan and ThreadSanitizer on Linux.
 *   h2_harness_render --list
 *       One line per adapter in the table: `<id> engine` or `<id> control`.
 *   h2_harness_render --jobs <file> --out <file>
 *       Runs every job in the file, in order. The output file is:
 *         H2HARNESS-OUT 1
 *         JOB <name>
 *         CHUNK <frames> <channels>      then frames*channels float64, the host's
 *                                        byte order (little-endian on every target),
 *                                        interleaved L R, or L R preL preR with `pre 1`
 *         ...                            one CHUNK per stretch of at most 32768 frames
 *         DONE {"engine":…,"sr":…,"seed":…,"block":…,"vary":…,"channels":…,
 *               "frames":…,"unknownKeys":…,"firstUnknown":…,"digest":"<16 hex>"}
 *         END <jobs>
 *       Chunked, with each job's summary AFTER its samples, so the file can be read
 *       as it is written and a long render needs constant memory on both sides.
 *       `digest` is harness.h's digestFrames over the output samples.
 *   h2_harness_render --pre-roundtrip <id> <x>...
 *       For each x: `<x> <tanh x> <preLimiter(tanh x)>` at 17 significant digits.
 *       A known-value probe of an adapter's pre-limiter recovery (selftest.mjs).
 *
 * EXIT. 0 = every job ran. 2 = the request was wrong or a file failed (an unknown
 * adapter id, a line the job language does not have, a write error): nothing is
 * skipped, because a job that is quietly not run would read as a job that passed.
 * This tool measures and judges nothing; the verdicts belong to the checks.
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Named by its h2/ path ON PURPOSE: tools/h2_rules_check.py decides that a target
// "compiles an h2 core" by finding the text h2/engine/ in its source file, and then
// demands -ffp-contract=off on it. Reaching the engine only through adapters.h
// would hide this target from that rule.
#include "../../h2/engine/engine.h"

#include "adapters.h"
#include "harness.h"

namespace {

using namespace horde2::harness;

// Writes each stretch as one CHUNK. A failed write latches and the run exits 2.
class FileSink final : public Sink {
 public:
  explicit FileSink(FILE* file) : f(file) {}
  bool failed = false;
  void write(const double* L, const double* R, const double* preL, const double* preR, int n) override {
    const int ch = preL ? 4 : 2;
    buf.resize(static_cast<size_t>(n) * static_cast<size_t>(ch));
    for (int i = 0; i < n; i++) {
      double* o = buf.data() + static_cast<size_t>(i) * static_cast<size_t>(ch);
      o[0] = L[i]; o[1] = R[i];
      if (preL) { o[2] = preL[i]; o[3] = preR[i]; }
    }
    if (std::fprintf(f, "CHUNK %d %d\n", n, ch) < 0) failed = true;
    if (std::fwrite(buf.data(), sizeof(double), buf.size(), f) != buf.size()) failed = true;
  }

 private:
  FILE* f;
  std::vector<double> buf;
};

// A key from the job file goes into the summary's JSON: escape what JSON must.
std::string jsonString(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back(static_cast<char>(c)); }
    else if (c < 0x20) { char u[8]; std::snprintf(u, sizeof u, "\\u%04x", c); o += u; }
    else o.push_back(static_cast<char>(c));
  }
  return o + "\"";
}

int usage() {
  std::fprintf(stderr, "usage: h2_harness_render --list\n"
                       "       h2_harness_render --jobs <file> --out <file>\n"
                       "       h2_harness_render --pre-roundtrip <adapter id> <x>...\n");
  return 2;
}

int list() {
  for (const AdapterRow& r : kAdapters) std::printf("%s %s\n", r.id, r.control ? "control" : "engine");
  return 0;
}

int preRoundtrip(int argc, char** argv) {
  if (argc < 4) return usage();
  const std::unique_ptr<Adapter> a = makeAdapter(argv[2]);
  if (!a) { std::fprintf(stderr, "h2_harness_render: no adapter '%s'\n", argv[2]); return 2; }
  for (int i = 3; i < argc; i++) {
    const double x = std::strtod(argv[i], nullptr), y = std::tanh(x);
    std::printf("%.17g %.17g %.17g\n", x, y, a->preLimiter(y));
  }
  return 0;
}

int runJobs(const char* jobsPath, const char* outPath) {
  FILE* in = std::fopen(jobsPath, "rb");
  if (!in) { std::fprintf(stderr, "h2_harness_render: cannot read %s\n", jobsPath); return 2; }
  FILE* out = std::fopen(outPath, "wb");
  if (!out) { std::fprintf(stderr, "h2_harness_render: cannot write %s\n", outPath); std::fclose(in); return 2; }
  std::string line, err;
  int jobs = 0, rc = 0;
  if (!readLine(in, line) || line != "H2HARNESS 1") { err = "the job file does not start with 'H2HARNESS 1'"; rc = 2; }
  else std::fprintf(out, "H2HARNESS-OUT 1\n");
  FileSink sink(out);
  while (rc == 0 && readLine(in, line)) {
    if (line.empty()) continue;
    size_t p = 0;
    if (word(line, p) != "JOB") { err = "expected a JOB line, read '" + line + "'"; rc = 2; break; }
    Job job;
    job.name = rest(line, p);
    if (!readJob(in, job, err)) { rc = 2; break; }
    const std::unique_ptr<Adapter> a = makeAdapter(job.engine.c_str());
    if (!a) { err = "job '" + job.name + "': no adapter '" + job.engine + "' (see --list)"; rc = 2; break; }
    std::fprintf(out, "JOB %s\n", job.name.c_str());
    JobInfo info;
    runJob(*a, job, sink, info);
    std::fprintf(out, "DONE {\"engine\":%s,\"sr\":%.17g,\"seed\":%lu,\"block\":%d,\"vary\":%s,\"channels\":%d,"
                      "\"frames\":%llu,\"unknownKeys\":%d,\"firstUnknown\":%s,\"digest\":\"%016llx\"}\n",
                 jsonString(job.engine).c_str(), job.sr, static_cast<unsigned long>(job.seed), job.block,
                 job.vary ? "true" : "false", job.pre ? 4 : 2, static_cast<unsigned long long>(info.frames),
                 info.unknownKeys, jsonString(info.firstUnknown).c_str(), static_cast<unsigned long long>(info.digest));
    jobs++;
  }
  if (rc == 0) std::fprintf(out, "END %d\n", jobs);
  std::fclose(in);
  if (std::fclose(out) != 0 || sink.failed) { if (rc == 0) err = std::string("writing ") + outPath + " failed"; rc = 2; }
  if (rc != 0) { std::fprintf(stderr, "h2_harness_render: %s\n", err.c_str()); return rc; }
  std::printf("h2_harness_render: %d job(s) -> %s\n", jobs, outPath);
  return 0;
}

// What the smoke run keeps of a render: how much, and how loud the output was.
struct SmokeSink final : Sink {
  uint64_t frames = 0;
  double sumSq = 0;
  void write(const double* L, const double* R, const double*, const double*, int n) override {
    for (int i = 0; i < n; i++) sumSq += L[i] * L[i] + R[i] * R[i];
    frames += static_cast<uint64_t>(n);
  }
};

int smoke() {
  constexpr int kNote = 4800;
  int bad = 0, n = 0;
  for (const AdapterRow& r : kAdapters) {
    Job job;
    job.name = "smoke";
    job.engine = r.id;
    job.seed = 1;
    job.block = 64;
    job.vary = true;
    job.pre = true;
    Cmd on;  on.op = "on";  on.a = 60; on.b = 261.6255653005986; on.c = 0.8;
    Cmd run; run.op = "render"; run.a = kNote;
    Cmd off; off.op = "off"; off.a = 60;
    job.cmds = {on, run, off, run};
    const std::unique_ptr<Adapter> a = r.make();
    SmokeSink sink;
    JobInfo info;
    runJob(*a, job, sink, info);
    const double rms = std::sqrt(sink.sumSq / (2.0 * static_cast<double>(sink.frames ? sink.frames : 1)));
    std::string why;
    if (sink.frames != 2u * kNote) why = "returned the wrong number of frames";
    // An engine's output ends in a soft limiter, so its level is under 1. A level that is
    // not (a NaN or an infinity in the sum reads so too) means the render is broken.
    // Controls are exempt: control/block answers with block sizes, not audio.
    else if (!r.control && !(rms <= 1.0)) why = "output level out of range";
    else if (std::strcmp(r.id, "control/tone") == 0 && !(rms > 1e-3)) why = "the tone control is not loud";
    else if (std::strcmp(r.id, "control/silent") == 0 && sink.sumSq != 0.0) why = "the silent control is not silent";
    std::printf("%s  smoke %-16s %llu frames, rms %.6g%s%s\n", why.empty() ? "PASS" : "FAIL", r.id,
                static_cast<unsigned long long>(sink.frames), rms, why.empty() ? "" : "  -- ", why.c_str());
    if (!why.empty()) bad++;
    n++;
  }
  std::printf("h2_harness_render smoke: %s (%d adapters, %d failed)\n", bad ? "RED" : "GREEN", n, bad);
  return bad ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) return smoke();
  if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) return list();
  if (argc >= 2 && std::strcmp(argv[1], "--pre-roundtrip") == 0) return preRoundtrip(argc, argv);
  const char* jobs = nullptr;
  const char* out = nullptr;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--jobs") == 0 && i + 1 < argc) jobs = argv[++i];
    else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
    else return usage();
  }
  if (!jobs || !out) return usage();
  return runJobs(jobs, out);
}
