/*
 * gen_state_fixtures — writes the state-fixture corpus (B100 item 4).
 *
 *   gen_state_fixtures <dir>
 *
 * One curated patch, saved by THIS build through both transports, then
 * rewritten into every older wire shape the loader still accepts (older
 * schema numbers, keys that did not exist yet, no B100 header), and for each
 * blob a golden: 1 s of A3 rendered from a fresh instance that loaded it.
 * statefix_check asserts every blob still loads and renders bit-identical
 * to its golden — "load + render bit-identical forever".
 *
 * THE CORPUS IS APPEND-ONLY. An existing fixture or golden is never
 * overwritten (the tool skips it and says so): a fixture that changes is no
 * longer evidence about the past. New shapes get new names.
 *
 * The older-schema blobs are SYNTHETIC ARCHAEOLOGY: this build's blob with
 * the keys and header a pre-migration build would not have written, so each
 * migrator (ADR-103 glideMode, the pre-note-lane glide restore, the
 * pre-ADR-100 enable restore, ADR-138 route clearing) is exercised by a
 * blob that names exactly what it lacks. REAL blobs — the notice-001
 * inventory's sessions, a preset a user mailed in — are the corpus's intended
 * growth and go beside these with a date in the name; see the README.
 */

#include <filesystem>
#include "statefix_common.h"

namespace fs = std::filesystem;
using namespace statefix;

namespace
{

int g_failures = 0;
void check(bool ok, const char *what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what);
  if (!ok) g_failures++;
}

/* ---- string surgery on our own formats (no JSON library; the shell's own
   parser is a tolerant find(), and so is this) ---- */

// Erase `"key":value` and one adjoining comma from a flat JSON object.
std::string dropJsonKey(std::string s, const std::string &key)
{
  const std::string needle = "\"" + key + "\":";
  const size_t at = s.find(needle);
  if (at == std::string::npos) return s;
  size_t end = at + needle.size();
  if (end < s.size() && s[end] == '"')   // string value
    end = s.find('"', end + 1) + 1;
  else
    while (end < s.size() && s[end] != ',' && s[end] != '}') end++;
  size_t begin = at;
  if (begin > 0 && s[begin - 1] == ',') begin--;         // ",key:val"
  else if (end < s.size() && s[end] == ',') end++;       // "key:val,"
  return s.erase(begin, end - begin);
}

// Replace the first `"key":number` value.
std::string setJsonNumber(std::string s, const std::string &key, const std::string &value)
{
  const std::string needle = "\"" + key + "\":";
  const size_t at = s.find(needle);
  if (at == std::string::npos) return s;
  size_t end = at + needle.size();
  while (end < s.size() && s[end] != ',' && s[end] != '}') end++;
  return s.replace(at + needle.size(), end - at - needle.size(), value);
}

std::string dropJsonHeader(std::string s)
{
  s = dropJsonKey(s, "engine_revision");
  return dropJsonKey(s, "build");
}

// Erase the whole `key=...\n` line from a chunk.
std::string dropChunkLine(std::string s, const std::string &key)
{
  const std::string needle = "\n" + key + "=";
  const size_t at = s.find(needle);
  if (at == std::string::npos) return s;
  const size_t eol = s.find('\n', at + 1);
  return s.erase(at + 1, eol - at);
}

// Every `o<k>.` line — a version-1 chunk had no second oscillator.
std::string dropChunkTwins(const std::string &s)
{
  std::string out;
  size_t pos = 0;
  while (pos < s.size())
  {
    const size_t eol = s.find('\n', pos);
    const std::string line = s.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = eol == std::string::npos ? s.size() : eol + 1;
    const bool twin = line.size() > 3 && line[0] == 'o' && line[1] >= '0' && line[1] <= '9' &&
                      line.find('.') != std::string::npos && line.find('.') < line.find('=');
    if (!twin) out += line + "\n";
  }
  return out;
}

/* The JSON apply is QUEUED through a bounded ring; a fixture must survive the
   trip, so the generator re-dumps after the load and compares every numeric
   key of the source. A dropped message reads as "value differs" here, never
   as a quietly-wrong golden. */
int jsonRoundTripMismatches(const std::string &src, const std::string &back)
{
  int bad = 0;
  size_t pos = src.find("\"params\":{");
  if (pos == std::string::npos) return 1;
  pos += 10;
  while (pos < src.size() && src[pos] != '}')
  {
    const size_t q1 = src.find('"', pos + 1);
    const std::string key = src.substr(pos + 1, q1 - pos - 1);
    size_t v0 = q1 + 2, v1 = v0;
    while (v1 < src.size() && src[v1] != ',' && src[v1] != '}') v1++;
    const double want = std::atof(src.c_str() + v0);
    const std::string needle = "\"" + key + "\":";
    const size_t at = back.find(needle);
    if (key == "specimen") { /* ADR-147: written, never read back */ }
    else if (at == std::string::npos || std::atof(back.c_str() + at + needle.size()) != want)
    {
      std::printf("     %s: wanted %.17g, got %s\n", key.c_str(), want,
                  at == std::string::npos ? "(absent)" : back.substr(at + needle.size(), 24).c_str());
      bad++;
    }
    pos = v1 == src.size() ? v1 : (src[v1] == ',' ? v1 + 1 : v1);
  }
  return bad;
}

struct Fixture
{
  std::string name;   // file name incl. extension
  std::string blob;
  double glideModeWant;   // what the loader must leave param 90 at (ADR-103)
};

/* What the patch MEANS, asserted after the load — independent of which keys
   the blob happens to name. The key-echo check above cannot see a value the
   blob omits and a MIGRATOR supplies (the schema-1 blob has no `enable`, so a
   dropped restore of it echoed nothing); this can. The patch has both
   oscillators on, and its glideMode lands where the schema says. */
int intentMismatches(const clap_plugin_t *p, const Fixture &f)
{
  auto get = [&](clap_id id) {
    double v = -1;
    paramsOf(p)->get_value(p, id, &v);
    return v;
  };
  int bad = 0;
  if (get(150) != 1) { std::printf("     enable: wanted 1, got %g\n", get(150)); bad++; }
  if (isChunk(f.name) && f.blob.rfind("hypersaw-state 1\n", 0) == 0) { /* v1: no osc 2 */ }
  else if (get(1150) != 1) { std::printf("     o1.enable: wanted 1, got %g\n", get(1150)); bad++; }
  if (get(90) != f.glideModeWant)
  { std::printf("     glideMode: wanted %g, got %g\n", f.glideModeWant, get(90)); bad++; }
  return bad;
}

bool emit(const fs::path &dir, const Fixture &f)
{
  const fs::path blobPath = dir / f.name;
  const fs::path goldPath = goldenFor(blobPath.string());
  if (fs::exists(blobPath) || fs::exists(goldPath))
  {
    std::printf("SKIP %s (exists — fixtures are never rewritten)\n", f.name.c_str());
    return true;
  }
  const clap_plugin_t *p = makePlugin();
  const bool loaded = isChunk(f.name) ? loadChunk(p, f.blob) : loadJson(p, f.blob);
  if (!loaded)
  {
    std::printf("FAIL %s: does not load\n", f.name.c_str());
    p->destroy(p);
    return false;
  }
  if (isJson(f.name))
  {
    const int bad = jsonRoundTripMismatches(f.blob, saveJson(p));
    if (bad)
    {
      std::printf("FAIL %s: %d param(s) did not survive the JSON apply\n", f.name.c_str(), bad);
      p->destroy(p);
      return false;
    }
  }
  if (const int bad = intentMismatches(p, f))
  {
    std::printf("FAIL %s: %d intent check(s) failed after load\n", f.name.c_str(), bad);
    p->destroy(p);
    return false;
  }
  std::vector<float> audio;
  render(p, audio);
  p->destroy(p);
  const double level = rms(audio);
  if (!(level > 1e-4))
  {
    // A silent golden would pass a silent regression: refuse it.
    std::printf("FAIL %s: render is silent (rms %.3g)\n", f.name.c_str(), level);
    return false;
  }
  if (!writeFile(blobPath.string(), f.blob.data(), f.blob.size()) ||
      !writeFile(goldPath.string(), audio.data(), audio.size() * sizeof(float)))
  {
    std::printf("FAIL %s: cannot write\n", f.name.c_str());
    return false;
  }
  std::printf("OK   %s (%zu bytes, golden rms %.4f)\n", f.name.c_str(), f.blob.size(), level);
  return true;
}

}  // namespace

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: gen_state_fixtures <dir>\n");
    return 2;
  }
  const fs::path dir = argv[1];
  fs::create_directories(dir);
  hypersaw_entry_init("");

  /* THE PATCH. Every param the shell has is written by the serializer, so the
     blobs carry the full key set; these are the ones moved off default, chosen
     so each migrator has something to act on and the render is not silent:
       n / seed / detune / K / width — a plainly audible swarm;
       glide 0.12 s with glideMode 1 — the ADR-103 migrator (schema<2: 1 -> 2)
         and the pre-note-lane restore (glide named, noteLawLink absent);
       oscillator 2 detuned — the o<k>. twin namespace (ADR-082);
       one generic mod route — the ADR-138 chunk. */
  const clap_plugin_t *p = makePlugin();
  auto *params = paramsOf(p);
  EvList evs;
  evs.push(1, 9);        // n
  evs.push(3, 4242);     // seed
  evs.push(4, 0.35);     // detune
  evs.push(6, 0.6);      // K
  evs.push(14, 1.0);     // width
  evs.push(33, 0.12);    // glide (s)
  evs.push(90, 1);       // glideMode
  evs.push(1004, 0.22);  // o1.detune
  params->flush(p, &evs.list, &kOut);
  // applyStateJson's return means "a PARAM was applied"; the route lands
  // regardless, and the saved preset is where it is asserted below.
  loadJson(p, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{},\"modRoutes\":\"0:4:0.5;\"}");

  const std::string chunk = saveChunk(p);
  const std::string json = saveJson(p);
  p->destroy(p);
  check(chunk.rfind("hypersaw-state 2\n", 0) == 0, "source chunk is version 2");
  check(chunk.find("\nengine_revision=1\n") != std::string::npos, "source chunk carries the B100 header");
  check(json.find("\"engine_revision\":1") != std::string::npos, "source preset carries the B100 header");
  check(json.find("\"modRoutes\":\"0:4:0.5;\"") != std::string::npos, "source preset carries the route");

  std::vector<Fixture> fx;
  // ---- host chunk (no migrators on this transport: glideMode stays 1) ----
  fx.push_back({"chunk-v2-rev1.txt", chunk, 1});
  {
    std::string v2 = dropChunkLine(dropChunkLine(chunk, "engine_revision"), "build");
    fx.push_back({"chunk-v2-noheader.txt", v2, 1});
    std::string v1 = dropChunkTwins(v2);
    v1.replace(0, std::strlen("hypersaw-state 2"), "hypersaw-state 1");
    v1 = dropChunkLine(v1, "modroutes");
    fx.push_back({"chunk-v1.txt", v1, 1});
  }
  // ---- JSON preset (ADR-103: schema<2 migrates a stored glideMode 1 -> 2) ----
  fx.push_back({"json-s3-rev1.json", json, 1});
  {
    const std::string s3 = dropJsonHeader(json);
    fx.push_back({"json-s3-noheader.json", s3, 1});
    const std::string s2 = dropJsonKey(setJsonNumber(s3, "schema", "2"), "modRoutes");
    fx.push_back({"json-s2.json", s2, 1});
    std::string s1 = setJsonNumber(s2, "schema", "1");
    for (const char *k : {"noteLawLink", "o1.noteLawLink", "enable", "o1.enable"})
      s1 = dropJsonKey(s1, k);
    fx.push_back({"json-s1.json", s1, 2});
  }
  for (const auto &f : fx)
    if (!emit(dir, f)) g_failures++;

  hypersaw_entry_deinit();
  std::printf("gen_state_fixtures: %s (%d failure%s)\n", g_failures ? "RED" : "GREEN", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
