/*
 * bridge_utf8_check — text that crosses the GUI bridge is valid UTF-8, and
 * every name cut falls on a character boundary (B446 P1).
 *
 *   bridge_utf8_check <gui-dir>     (repo-relative: src/gui)
 *
 * WHY. The webview's JSON escaper (choc) requires well-formed UTF-8 — its own
 * documented precondition, checked only by asserts that Release compiles out.
 * Names used to be cut by BYTE count, which can split a character (59 letters
 * and an "é" cut at 60 bytes), and a session chunk or preset file can carry any
 * bytes. The guard is hypersaw::utf8Clean
 * (src/input_guards.h): at ingest every name is cut at a character boundary and
 * made valid; at egress every string the bridge returns goes through ONE helper,
 * bridgeStr, so a raw file read is covered too.
 *
 * Rows:
 *   CONTROL  the precondition matters on this toolchain: the raw escaper given
 *            {'a',0xC3,0,'Z','Z',0} (input that breaks it) does not stop at the
 *            first terminator, so it emits "ZZ", and this check can see that.
 *            If it does NOT, every later row is blind and the check is RED.
 *   CLEAN    utf8Clean over a hostile corpus: output valid (judged by a strict
 *            validator written independently of the cleaner, and by choc's own),
 *            within its byte budget, and run through the same buffer layout as
 *            CONTROL it emits no "ZZ"; the escaped JSON parses back to exactly
 *            the cleaned text. Valid input that fits comes back byte-identical.
 *   SHELL    the real plugin: a 60-byte name typed through setCornerName and the
 *            preset loader's name, a crafted host chunk and a crafted preset
 *            whose names end mid-sequence. Every name the bridge would return
 *            (corner names, state JSON, the saved chunk) is valid UTF-8, and
 *            the typed names are cut to 59 bytes, not 60.
 *   STATIC   in <gui-dir>, `createString(` appears only inside bridgeStr — the
 *            construction is what holds, not a reviewer remembering. The scanner
 *            is itself calibrated on a planted extra call that must be caught.
 *
 * Deterministic, no sanitizer needed: the CONTROL buffer is a single array, so
 * everything the escaper reads stays inside memory this file owns.
 * WIRED: ./verify full.
 */

/* Mirror the Release build's choc: its asserts are plain assert(), compiled out
   under NDEBUG, and the CONTROL row is about what the SHIPPED code does. In a
   build without NDEBUG the assert would abort the control instead of letting it
   show the over-read. */
#define CHOC_ASSERT(x) ((void)0);

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "../libs/choc/choc/text/choc_JSON.h"
#include "../src/input_guards.h"
#include "statefix_common.h"

namespace fs = std::filesystem;

namespace
{
int g_fail = 0;
void row(bool ok, const char *tag, const std::string &what)
{
  std::printf("%s  %-7s %s\n", ok ? "PASS" : "FAIL", tag, what.c_str());
  if (!ok) g_fail++;
}

/* A strict validator, deliberately NOT the cleaner's algorithm: it walks the
   well-formed byte-sequence table of the Unicode Standard (Table 3-7) by
   ranges, where utf8SeqLen decodes a code point and range-checks it. Two
   formulations agreeing is evidence; one formulation checking itself is not. */
bool strictValid(const std::string &s)
{
  auto in = [](unsigned char c, int lo, int hi) { return c >= lo && c <= hi; };
  const size_t n = s.size();
  for (size_t i = 0; i < n;)
  {
    const unsigned char a = (unsigned char)s[i];
    auto at = [&](size_t k) -> unsigned char { return i + k < n ? (unsigned char)s[i + k] : 0; };
    if (a <= 0x7F) { i += 1; continue; }
    if (in(a, 0xC2, 0xDF) && in(at(1), 0x80, 0xBF)) { i += 2; continue; }
    if (a == 0xE0 && in(at(1), 0xA0, 0xBF) && in(at(2), 0x80, 0xBF)) { i += 3; continue; }
    if ((in(a, 0xE1, 0xEC) || in(a, 0xEE, 0xEF)) && in(at(1), 0x80, 0xBF) && in(at(2), 0x80, 0xBF)) { i += 3; continue; }
    if (a == 0xED && in(at(1), 0x80, 0x9F) && in(at(2), 0x80, 0xBF)) { i += 3; continue; }
    if (a == 0xF0 && in(at(1), 0x90, 0xBF) && in(at(2), 0x80, 0xBF) && in(at(3), 0x80, 0xBF)) { i += 4; continue; }
    if (in(a, 0xF1, 0xF3) && in(at(1), 0x80, 0xBF) && in(at(2), 0x80, 0xBF) && in(at(3), 0x80, 0xBF)) { i += 4; continue; }
    if (a == 0xF4 && in(at(1), 0x80, 0x8F) && in(at(2), 0x80, 0xBF) && in(at(3), 0x80, 0xBF)) { i += 4; continue; }
    return false;
  }
  return true;
}
// Valid by both opinions. choc's is lax (it accepts overlongs and surrogates)
// but it is a second lineage on the truncation question, which is the one here.
bool valid(const std::string &s)
{
  return strictValid(s) && choc::text::findInvalidUTF8Data(s.data(), s.size()) == nullptr;
}

/* The CONTROL layout: the text, its terminator, then a canary the escaper can
   only reach by reading past that terminator. */
bool escaperOverreads(const std::string &text)
{
  std::vector<char> buf(text.begin(), text.end());
  buf.push_back('\0');
  buf.push_back('Z');
  buf.push_back('Z');
  buf.push_back('\0');
  const std::string esc = choc::json::addEscapeCharacters(choc::text::UTF8Pointer(buf.data()));
  return esc.find("ZZ") != std::string::npos;
}

std::string hexOf(const std::string &s)
{
  std::string o;
  char b[8];
  for (unsigned char c : s)
  {
    if (c >= 0x20 && c < 0x7F) { o += (char)c; continue; }
    std::snprintf(b, sizeof b, "\\x%02X", c);
    o += b;
  }
  return o;
}

/* ---- STATIC: createString( only inside bridgeStr ---------------------------- */
/* Every occurrence must sit in the body of `bridgeStr(`'s definition: after its
   signature line and before the next line that is a lone `}`. Returns the
   offending "file:line" list. */
std::vector<std::string> strayCreateString(const std::string &name, const std::string &text)
{
  std::vector<std::string> out;
  const std::regex call(R"(createString\s*\()");
  const std::regex def(R"(^\s*inline\s+choc::value::Value\s+bridgeStr\s*\()");
  std::istringstream in(text);
  std::string line;
  bool inside = false;
  for (int ln = 1; std::getline(in, line); ln++)
  {
    if (std::regex_search(line, def)) { inside = true; continue; }
    if (inside && line == "}") { inside = false; continue; }
    if (std::regex_search(line, call) && !inside) out.push_back(name + ":" + std::to_string(ln));
  }
  return out;
}

bool readAll(const fs::path &p, std::string &out)
{
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

/* The value of `key=` in a host chunk, or of the JSON corner-name array. */
std::string chunkLine(const std::string &chunk, const std::string &key)
{
  const size_t at = chunk.find("\n" + key + "=");
  if (at == std::string::npos) return "";
  const size_t b = at + key.size() + 2, e = chunk.find('\n', b);
  return chunk.substr(b, e == std::string::npos ? std::string::npos : e - b);
}
}  // namespace

int main(int argc, char **argv)
{
  if (argc < 2) { std::fprintf(stderr, "usage: bridge_utf8_check <gui-dir>\n"); return 2; }
  const std::string a59(59, 'a');
  const std::string e_acute = "\xC3\xA9";

  /* ---- CONTROL -------------------------------------------------------------- */
  {
    const std::string lone = "a\xC3";
    const bool over = escaperOverreads(lone);
    row(over, "CONTROL", "raw escaper on {'a',0xC3,0,'Z','Z',0} does not stop at the first terminator (emits ZZ)");
    if (!over)
    {
      std::printf("bridge_utf8_check: RED — the control did not trip, so no row below can fail\n");
      return 1;
    }
    row(!valid(lone), "CONTROL", "the strict validator rejects \"a\\xC3\" (the validator is not blind)");
    row(!valid(a59 + "\xC3"), "CONTROL", "a byte cut of 59 letters + \"é\" at 60 is invalid UTF-8");
  }

  /* ---- CLEAN ---------------------------------------------------------------- */
  {
    struct Case { std::string in; size_t max; std::string want; bool exact; };
    const std::string fffd = "\xEF\xBF\xBD";
    const std::vector<Case> cases = {
        {"a\xC3", 60, "a" + fffd, true},                        // truncated 2-byte
        {"\xF0", 60, fffd, true},                               // lone 4-byte lead
        {"\xFF", 60, fffd, true},                               // never valid
        {"x\xF0\x9F\x8E", 60, "x" + fffd + fffd + fffd, true},   // truncated 4-byte: one per byte
        {"\xC0\xAF", 60, fffd + fffd, true},                    // overlong "/"
        {"\xE0\x80\xAF", 60, fffd + fffd + fffd, true},         // overlong, 3-byte
        {"\xED\xA0\x80", 60, fffd + fffd + fffd, true},         // UTF-16 surrogate
        {"\xF4\x90\x80\x80", 60, fffd + fffd + fffd + fffd, true},   // above U+10FFFF
        {"\x80\xBF", 60, fffd + fffd, true},                    // stray continuations
        {a59 + e_acute, 60, a59, true},                         // the typing case: cut BEFORE é
        {std::string(58, 'a') + "\xE2\x82\xAC", 60, std::string(58, 'a'), true},   // € does not fit
        {std::string(56, 'a') + "\xF0\x9F\x8E\xB9", 60, std::string(56, 'a') + "\xF0\x9F\x8E\xB9", true},
        {"caf\xC3\xA9 \xE2\x86\x90 \xF0\x9F\x8E\xB9", 60, "caf\xC3\xA9 \xE2\x86\x90 \xF0\x9F\x8E\xB9", true},
        {"plain ascii, quotes \" and \\ and tabs\t", 60, "plain ascii, quotes \" and \\ and tabs\t", true},
        {std::string(57, 'a') + "\xC3", 60, std::string(57, 'a') + fffd, true},   // FFFD fits exactly
        {std::string(58, 'a') + "\xC3", 60, std::string(58, 'a'), true},          // FFFD does not fit
        {std::string(5000, 'q') + "\xC3", (size_t)-1, std::string(5000, 'q') + fffd, true},   // a raw file
    };
    int n = 0;
    for (const Case &c : cases)
    {
      const std::string got = hypersaw::utf8Clean(c.in, c.max);
      const bool ok = valid(got) && got.size() <= c.max && got == c.want && !escaperOverreads(got);
      // The escaped JSON must parse back to exactly the cleaned text: nothing
      // swallowed (the closing-quote loss), nothing added.
      bool rt = false;
      try
      {
        const auto v = choc::json::parseValue(choc::json::toString(choc::value::createString(got)));
        rt = v.isString() && std::string(v.getString()) == got;
      }
      catch (...) { rt = false; }
      row(ok && rt, "CLEAN", "\"" + hexOf(c.in.size() > 72 ? c.in.substr(c.in.size() - 8) : c.in) +
                                 "\" -> \"" + hexOf(got.size() > 72 ? got.substr(got.size() - 8) : got) + "\"");
      n++;
    }
    // Identity on valid input is the "every existing name is unchanged" claim.
    const std::string ok = "MO - Quantum Morph";
    row(hypersaw::utf8Clean(ok, 60) == ok && hypersaw::utf8Clean(ok) == ok, "CLEAN",
        "valid text that fits is returned byte-identical");
  }

  /* ---- SHELL ---------------------------------------------------------------- */
  {
    using namespace statefix;
    // Typed: setCornerName and the preset loader's name, 59 letters + "é".
    {
      const clap_plugin_t *p = makePlugin();
      hypersaw_debug_cornername(p, 0, (a59 + e_acute).c_str());
      const std::string names = hypersaw_debug_cornernames(p);
      row(valid(names) && names.find("\"" + a59 + "\"") != std::string::npos, "SHELL",
          "setCornerName(59 letters + é): corner names valid, the name cut to 59 bytes");
      // applyStateJson refuses text with no "params" key, so the minimal patch names one.
      hypersaw_debug_apply_named(p, "{\"params\":{}}", (a59 + e_acute).c_str());
      drain(p);
      const std::string chunk = saveChunk(p), json = saveJson(p);
      row(chunkLine(chunk, "presetname") == a59 && valid(chunk), "SHELL",
          "preset name (59 letters + é): saved name is the 59 letters, chunk valid");
      row(valid(json), "SHELL", "state JSON after the typed names is valid UTF-8");
      p->destroy(p);
    }
    // Crafted host chunk: names that END mid-sequence, one behind an escape.
    {
      const clap_plugin_t *p = makePlugin();
      const std::string blob = std::string("hypersaw-state 2\n") + "presetname=abc\xC3\n" +
                               "morph={\"cornerNames\":[\"x\xF0\",\"\\\xC3\",\"y\xE2\x86\",\"z\"]}\n";
      loadChunk(p, blob);
      drain(p);
      const std::string names = hypersaw_debug_cornernames(p);
      const std::string chunk = saveChunk(p), json = saveJson(p);
      const std::string pn = chunkLine(chunk, "presetname");
      row(valid(names), "SHELL", "crafted chunk: corner names valid UTF-8 (" + hexOf(names) + ")");
      row(valid(pn) && pn.rfind("abc", 0) == 0, "SHELL", "crafted chunk: preset name valid (" + hexOf(pn) + ")");
      row(valid(chunk) && valid(json), "SHELL", "crafted chunk: saved chunk and state JSON valid UTF-8");
      p->destroy(p);
    }
    // Crafted preset JSON: the loader's other entry.
    {
      const clap_plugin_t *p = makePlugin();
      loadJson(p, "{\"params\":{},\"presetName\":\"" + std::string(30, 'p') + "\xF0\x9F\x8E" +
                      "\",\"cornerNames\":[\"\xED\xA0\x80\",\"\",\"\",\"\xC3\"]}");
      const std::string names = hypersaw_debug_cornernames(p);
      const std::string chunk = saveChunk(p), json = saveJson(p);
      row(valid(names) && valid(chunk) && valid(json), "SHELL",
          "crafted preset JSON: corner names, chunk and state JSON valid UTF-8");
      p->destroy(p);
    }
  }

  /* ---- STATIC --------------------------------------------------------------- */
  {
    const std::string plant = "inline choc::value::Value bridgeStr(std::string_view s)\n{\n"
                              "  return choc::value::createString(hypersaw::utf8Clean(s));\n}\n"
                              "x = choc::value::createString (raw);\n";
    const auto caught = strayCreateString("plant", plant);
    row(caught.size() == 1 && caught[0] == "plant:5", "STATIC",
        "scanner calibration: a planted createString outside bridgeStr is caught, the helper's own is not");
    int files = 0, defs = 0;
    std::vector<std::string> stray;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(argv[1], ec))
    {
      const std::string ext = e.path().extension().string();
      if (ext != ".h" && ext != ".mm" && ext != ".cpp") continue;
      std::string text;
      if (!readAll(e.path(), text)) continue;
      files++;
      if (text.find("inline choc::value::Value bridgeStr(") != std::string::npos) defs++;
      for (auto &s : strayCreateString(e.path().filename().string(), text)) stray.push_back(s);
    }
    std::string where;
    for (auto &s : stray) where += " " + s;
    row(files > 0 && defs == 1, "STATIC", "bridgeStr is defined exactly once in " + std::string(argv[1]) +
                                              " (" + std::to_string(files) + " files scanned)");
    row(stray.empty(), "STATIC", stray.empty() ? std::string("every createString( is inside bridgeStr")
                                               : "createString( outside bridgeStr at" + where);
  }

  if (g_fail)
  {
    std::printf("bridge_utf8_check: RED — %d row(s) failed\n", g_fail);
    return 1;
  }
  std::printf("bridge_utf8_check: GREEN\n");
  return 0;
}
