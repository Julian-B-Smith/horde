// Not run by ./verify: it links the clean-room engine for its manifest, which isn't in this public repo or in CI yet
// (B568). build.sh runs it.
//
// The bridge's pure half (bridge_core.h, B628) held to section 7's rows of docs/proposals/gui-on-engine.md, headless:
// no plug-in, no window. Each check has a control that must read the other way, so a check that can't fail can't pass.
//   bridge_test
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "bridge_core.h"
#include "horde_engine.h"

namespace {

int failures = 0;
void check(bool ok, const std::string& what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok) ++failures;
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

using horde::Law;
using horde::ParamClass;
using horde::Rate;
using namespace preview::bridge;

// A fixture manifest of one row per law and class, plus a reserved row. Its hash below was computed separately, by
// Python's '%.17g' and hashlib over the same rule (section 3), so this C++ is held to another implementation, not to
// itself.
std::vector<horde::ParamInfo> fixture() {
  std::vector<horde::ParamInfo> rows = {
      {1, "out.level", -60, 6, 0, Law::Db, ParamClass::Morphable, Rate::Control, false, 0, false},
      {2, "voice.mode", 0, 2, 0, Law::Step, ParamClass::Structural, Rate::Stepped, false, 3, false},
      {3, "env1.decay", 0.005, 30, 0.4, Law::Log, ParamClass::Morphable, Rate::Control, true, 0, false},
      {4, "osc1.unison", 1, 16, 5, Law::Step, ParamClass::Structural, Rate::Stepped, false, 16, false},
      {5, "noise.sampleStart", 0, 0, 0, Law::Lin, ParamClass::Structural, Rate::Control, false, 0, true},
      {6, "osc1.phase", 0, 1, 0.25, Law::LinWraps, ParamClass::Device, Rate::Control, false, 0, false},
      {7, "osc1.blade1.cut", 0.004, 20, 1, Law::LogOffZone, ParamClass::Morphable, Rate::Control, true, 0, false},
      {8, "mix.osc1", 0, 1, 0.7, Law::Pow, ParamClass::Morphable, Rate::Control, true, 0, false},
  };
  return rows;
}
constexpr const char* kFixtureHash = "8c156ad5a6835b728bdef866765a37a67fd5acf89e87df864f114707e5fd2e44";

std::vector<const horde::ParamInfo*> pointers(const std::vector<horde::ParamInfo>& rows) {
  std::vector<const horde::ParamInfo*> out;
  for (const auto& r : rows) out.push_back(&r);
  return out;
}

}  // namespace

int main() {
  const auto rows = fixture();
  const KeyMap keys(pointers(rows));

  // ---- the manifest hash ----------------------------------------------------------------------------------------
  const std::string hash = manifest_hash(pointers(rows));
  check(hash == kFixtureHash, "the fixture manifest hashes to the value Python computed (" + hash + ")");
  check(!has(manifest_text(pointers(rows)), "noise.sampleStart"), "a reserved row is outside the hash");
  {
    auto changed = rows;
    changed[2].default_value = 0.5;   // env1.decay's default
    const std::string other = manifest_hash(pointers(changed));
    check(other != hash, "control: one row's default changed reads as another hash");
    Message hello = parse_message("{\"type\":\"hello\",\"seq\":1,\"manifest_hash\":\"" + hash + "\",\"manifest\":" +
                                      quoted(manifest_text(pointers(rows))) + "}",
                                  keys);
    const Mismatch m = check_hello(hello, other, manifest_text(pointers(changed)));
    check(m.any && m.expected == other && m.got == hash, "a page whose hash differs from the engine's reads as a mismatch");
    check(m.keys.size() == 1 && m.keys[0] == "env1.decay", "and its diff names exactly the row that changed");
    check(!check_hello(hello, hash, manifest_text(pointers(rows))).any, "control: the same manifest is no mismatch");
    Message silent = parse_message("{\"type\":\"hello\",\"seq\":2}", keys);
    check(!check_hello(silent, other, "").any, "a page that sends no hash isn't checked (the stand-in page)");
  }

  // ---- every message type is acknowledged ----------------------------------------------------------------------
  // The plug-in acknowledges every message that parses with a seq (smoke.cpp holds the plug-in to that); here, every
  // type the seam names parses with its seq and the kind the bridge acts on.
  const char* const kTypes[] = {"set", "gesture", "morph", "position", "modes", "rack", "midi", "clear", "want", "preset", "plugin", "hello"};
  std::int64_t seq = 100;
  for (const char* type : kTypes) {
    const Message m = parse_message("{\"type\":\"" + std::string(type) + "\",\"seq\":" + std::to_string(++seq) + "}", keys);
    check(m.kind != Kind::Invalid && m.seq == seq, std::string(type) + ": parsed with its seq, so it is acknowledged");
  }
  for (const char* type : {"rack", "midi", "clear"}) {
    const Message m = parse_message("{\"type\":\"" + std::string(type) + "\",\"seq\":1,\"values\":{\"out.level\":0.5}}", keys);
    check(m.kind == Kind::Held && m.values.empty() && !m.unknown_type, std::string(type) + ": held and dropped, nothing sent");
  }
  {
    const Message m = parse_message("{\"type\":\"teleport\",\"seq\":7}", keys);
    check(m.kind == Kind::Held && m.unknown_type && m.seq == 7, "a type nobody knows is acknowledged, held, and reported");
    check(parse_message("{\"type\":\"set\",\"values\":{}}", keys).kind == Kind::Invalid, "control: a message with no seq can't be acknowledged");
    check(parse_message("not json", keys).kind == Kind::Invalid, "control: unreadable text is refused, never thrown");
  }

  // ---- the morph's messages (B636): each parses to what the morph system acts on ---------------------------------
  {
    using namespace preview::morph;
    const Message morph = parse_message("{\"type\":\"morph\",\"seq\":11,\"settings\":{\"morph.on\":1,\"morph.glide\":1.5,\"morph.x\":0.3}}", keys);
    bool on = false, glide = false;
    for (const auto& [i, v] : morph.settings) {
      on = on || (i == kOn && v == 1);
      glide = glide || (i == kGlide && v == 1);   // clamped to [0, 1], as PA-7 clamps a host value
    }
    check(morph.kind == Kind::Morph && morph.seq == 11 && on && glide && morph.settings.size() == 2,
          "morph: D.2's settings by key reach the morph system, a value past 1 clamped");
    check(morph.unknown.size() == 1 && morph.unknown[0] == "morph.x", "morph: a key D.2 lacks (morph.x, until round 61) is reported, never guessed at");
    const Message as_set = parse_message("{\"type\":\"set\",\"seq\":12,\"values\":{\"morph.on\":1}}", keys);
    check(as_set.values.empty() && as_set.unknown.size() == 1, "control: a morph setting sent as `set` is no engine row: reported, not sent");

    const Message pos = parse_message("{\"type\":\"position\",\"seq\":13,\"x\":0.25,\"y\":1.5}", keys);
    check(pos.kind == Kind::Position && pos.has_position && pos.x == 0.25 && pos.y == 1, "position: x and y, clamped to the pad");
    check(!parse_message("{\"type\":\"position\",\"seq\":14,\"x\":0.25}", keys).has_position, "control: a position without y moves nothing");

    const Message modes = parse_message("{\"type\":\"modes\",\"seq\":15,\"modes\":{\"out.level\":\"quantum\",\"osc1.ratios\":\"exempt\","
                                        "\"no.such.key\":\"quantum\",\"env1.decay\":\"wobble\",\"noise.sampleStart\":\"quantum\"},\"counts\":{\"out.level\":2}}",
                                        keys);
    bool level = false, ratios = false;
    for (const auto& e : modes.modes) {
      level = level || (e.key == keys.find("out.level") && e.choice.mode == horde::KeyMode::Quantum && e.choice.count == horde::KeyCount::Two);
      ratios = ratios || (e.key == kRatiosKey[0] && e.choice.mode == horde::KeyMode::Exempt && e.choice.count == horde::KeyCount::AsMorph);
    }
    check(modes.kind == Kind::Modes && modes.modes.size() == 2 && level && ratios, "modes: each key's mode and count, a ratios text's included (MO-14 item 3)");
    check(modes.unknown.size() == 2 && modes.reserved.size() == 1, "modes: an unknown key, a mode the menu lacks and a reserved row are each reported");

    const Message corner = parse_message("{\"type\":\"corner\",\"seq\":16,\"index\":2,\"values\":{\"out.level\":0.75,\"fx1.mix\":0.5}}", keys);
    check(corner.kind == Kind::Corner && corner.index == 2 && corner.values.size() == 1 && corner.values[0].v == 0.75 && corner.unknown.size() == 1,
          "corner: values for one corner, by key, an unknown key reported");
    check(parse_message("{\"type\":\"corner\",\"seq\":17,\"index\":4,\"values\":{}}", keys).index == -1, "control: a corner past D names none");
    const Message save = parse_message("{\"type\":\"save\",\"seq\":18,\"index\":3}", keys);
    check(save.kind == Kind::Save && save.index == 3, "save: Save current sound names its corner (MO-25)");
    const Message want = parse_message("{\"type\":\"want\",\"seq\":19,\"morph\":true}", keys);
    check(want.want.morph && !parse_message("{\"type\":\"want\",\"seq\":20}", keys).want.morph, "want.morph asks for the morph member; a want without it doesn't");
  }

  // ---- D.2's rows, transcribed: their v defaults are the page's, and they decode to D.2's units ------------------
  {
    using namespace preview::morph;
    // The page's own v defaults (docs/design/gui3/shell-data.js, each morph row's default_v, 2026-11-06), a second
    // reading of D.2 made by another generator: exactly the same doubles.
    const double page[kSettings] = {0.0, 0.0, 0.5, 1.0, 0.7383519587043745, 0.3, 0.10232046409281856, 0.5714285714285714, 0.0,
                                    0.14285714285714285, 0.0, 0.0, 0.04};
    bool same = true;
    for (int i = 0; i < kSettings; ++i) same = same && default_v(i) == page[i];
    check(same, "D.2's thirteen defaults as v are the page's, bit for bit");
    double v[kSettings];
    for (int i = 0; i < kSettings; ++i) v[i] = default_v(i);
    const horde::MorphSettings d = decode(v);
    // The continuous ones go through their row's law (horde::value_of, the harness's own), so what is held here is the
    // rows themselves, against the page's (shell-data.js: lo, hi, law); their decoded units are printed for a reader.
    std::printf("     D.2's defaults decoded: temperature %.17g, cohesion %.17g, step rate %.17g, glide %.17g\n", d.temperature, d.cohesion, d.step_rate, d.glide);
    check(!d.on && d.audible == 0 && d.law == horde::MorphLaw::Quantum && d.count == 4 && d.seed == 1024 && !d.step_sync && d.step_division == 3 &&
              !d.step_triplet && !d.step_dotted,
          "the stepped defaults decode to D.2's: off, A, Quantum, Four, seed 1024, unsynced, 1/16, no triplets or dots");
    const auto row_is = [&](int i, horde::Law l, double lo, double hi) { return setting_row(i).law == l && setting_row(i).lo == lo && setting_row(i).hi == hi; };
    check(row_is(kTemperature, horde::Law::Log, 0.02, 4) && row_is(kCohesion, horde::Law::Lin, 0, 1) && row_is(kStepRate, horde::Law::Log, 0.25, 32) &&
              row_is(kGlide, horde::Law::Pow, 0, 5),
          "the continuous rows are the page's: temperature log 0.02 to 4, cohesion lin 0 to 1, step rate log 0.25 to 32, glide pow 0 to 5");
    v[kLaw] = 0;
    v[kSeed] = 1;
    v[kAudible] = 1;
    v[kOn] = 1;
    const horde::MorphSettings e = decode(v);
    check(e.law == horde::MorphLaw::Blend && e.seed == 9999 && e.audible == 3 && e.on, "control: law 0 is Blend, the top seed 9,999, audible 1 is D");
  }

  // ---- the morph member of a frame (B636) --------------------------------------------------------------------------
  {
    MorphFrame f;
    f.x = 0.5;
    f.y = 0.25;
    const std::string bare = morph_json(f);
    check(bare == "{\"x\":0.5,\"y\":0.25,\"dropped\":0,\"unknown\":0}", "a morph member with nothing changed carries the position and the refusals only");
    std::vector<std::pair<std::string, double>> corner1 = {{"out.level", 0.5}};
    std::vector<HeardGroup> sets(1);
    sets[0].group = "osc1/kernel";
    sets[0].voice = 2;
    sets[0].n = 2;
    sets[0].id[1] = 1;
    sets[0].target[0] = 0.25, sets[0].target[1] = 0.75, sets[0].heard[0] = 0.5, sets[0].heard[1] = 0.5;
    f.corners[1] = &corner1;
    f.sets = &sets;
    const std::string full = morph_json(f);
    check(has(full, "\"corners\":[{},{\"out.level\":0.5},{},{}]"), "a changed corner rides in its own place among four");
    check(has(full, "\"sets\":[{\"group\":\"osc1/kernel\",\"voice\":2,\"settings\":[{\"id\":0,\"target\":0.25,\"heard\":0.5},{\"id\":1,\"target\":0.75,\"heard\":0.5}]}]"),
          "a heard set carries each setting's id, target and heard weight (IF-10)");
  }

  // ---- keys, never ids: an unknown key is reported, never sent ------------------------------------------------
  {
    const Message m = parse_message("{\"type\":\"set\",\"seq\":3,\"values\":{\"no.such.key\":0.5,\"out.level\":0.25,\"noise.sampleStart\":0.1}}", keys);
    bool sent_unknown = false, sent_known = false;
    for (const auto& v : m.values) {
      sent_unknown = sent_unknown || std::string(keys.row(v.row).key) != "out.level";
      sent_known = sent_known || (std::string(keys.row(v.row).key) == "out.level" && v.v == 0.25);
    }
    check(!sent_unknown && m.unknown.size() == 1 && m.unknown[0] == "no.such.key", "a key the manifest lacks is reported and not sent");
    check(m.reserved.size() == 1 && m.reserved[0] == "noise.sampleStart", "a reserved row's key is reported and not sent");
    check(sent_known, "control: the known key in the same message is sent, at the page's v exactly");
  }

  // ---- by target: rack, midi and resolver keys never become Params ---------------------------------------------
  {
    const Message rack = parse_message("{\"type\":\"rack\",\"seq\":4,\"values\":{\"fx1.mix\":0.5}}", keys);
    check(rack.values.empty(), "a rack message emits no Param");
    const Message midi = parse_message("{\"type\":\"midi\",\"seq\":5,\"values\":{\"arp.rate\":0.5}}", keys);
    check(midi.values.empty(), "a midi message emits no Param");
    const Message stray = parse_message("{\"type\":\"set\",\"seq\":6,\"values\":{\"fx1.mix\":0.5}}", keys);
    check(stray.values.empty() && stray.unknown.size() == 1, "a rack key sent as `set` is reported, not sent");
    const Message gesture = parse_message("{\"type\":\"gesture\",\"seq\":8,\"key\":\"fx1.mix\",\"phase\":\"begin\"}", keys);
    check(gesture.gesture_row < 0, "a gesture on a rack key reaches neither the engine nor the host");
    const Message ours = parse_message("{\"type\":\"gesture\",\"seq\":9,\"key\":\"out.level\",\"phase\":\"begin\"}", keys);
    check(ours.gesture_row >= 0 && ours.gesture_begin, "control: a gesture on an engine key does");
  }

  // ---- frames carry only what `want` asked for, and silence claims no sound ------------------------------------
  {
    static horde::GuiFrame frame;   // static: tens of KB
    frame.sequence = 1;
    frame.voice[2].sounding = true;
    frame.voice[2].level = 0.5f;
    frame.voice[2].member_phase[0][0] = 0.125f;
    horde::GuiFrame* f = &frame;
    NoteSlot notes[horde::kMaxVoices];
    notes[2] = {true, 64, 0.85, true};
    FrameInputs in;
    in.frame = f;
    in.notes = notes;
    in.engine_silent = false;
    in.unison[0] = in.unison[1] = 3;
    Want want;
    const std::string plain = observe_json(in, want);
    check(!has(plain, "\"phases\"") && !has(plain, "\"levels\""), "a frame carries no members when `want` didn't ask");
    want.members = true;
    want.cycles = true;
    const std::string members = observe_json(in, want);
    check(has(members, "\"phases\":[0.125,0,0]"), "control: with want.members it carries them, one per unison member");
    check(!has(members, "cycles"), "member cycles never ride in this slice, even when asked (GN-41)");
    check(has(members, "\"notes\":{\"key\":64,\"vel\":0.85,\"gate\":true}"), "a voice carries its note from the plug-in's own events");
    want.osc[1] = false;
    check(has(observe_json(in, want), "]},null]"), "an oscillator `want` didn't name rides as null");
    check(has(plain, "\"sounding\":true"), "control: a frame with a sounding voice while the engine plays says so");
    in.engine_silent = true;
    const std::string quiet = observe_json(in, Want{});
    check(has(quiet, "\"sounding\":false") && has(quiet, "\"voices\":[]"), "a frame while the engine is silent claims no sound, from the last frame");
  }

  // ---- the saved state ------------------------------------------------------------------------------------------
  {
    const std::vector<std::uint32_t> ids = {1, 3, 8};
    const std::vector<double> loaded = {0.5, 0.25, 0.7};
    SavedState s;
    s.preset = 12;
    s.values = differences(ids, {0.5, 0.1 + 0.2, 0.7}, loaded);
    check(s.values.size() == 1 && s.values[0].first == 3, "only the row that differs from the preset is kept");
    const std::string text = write_state(s);
    SavedState back;
    check(!has(text, "voices") && read_state(text, back) && back.preset == 12 && back.values.size() == 1 && back.values[0].first == 3 &&
              back.values[0].second == 0.1 + 0.2,
          "the state round-trips, its v exactly");
    SavedState defaults;
    defaults.preset = 12;
    defaults.values = differences(ids, loaded, loaded);
    const std::string small = write_state(defaults);
    std::printf("     a defaults-only state is %zu bytes\n", small.size());
    check(defaults.values.empty() && small.size() < 64, "a state holding only the preset's own values stays small");
    SavedState old;
    check(read_state("horde-engine-preview 1\npreset 3\nvoices 2\n4097 0.5\n", old) && old.preset == 3 && old.values.size() == 1,
          "a version-1 chunk still reads, its retired Voices line ignored");
    check(read_state("horde-engine-preview 2\npreset 5\nvoices 6\n4097 0.25\n", old) && old.preset == 5 && old.values.size() == 1 && old.values[0].second == 0.25,
          "a version-2 chunk carrying Voices still loads, ignoring it");
    check(!read_state("something else\npreset 3\n", old), "control: a chunk that isn't ours is refused");

    // The morph's lines (B636): version 3 only when the morph holds something, and it round-trips exactly.
    check(text == "horde-engine-preview 2\npreset 12\n3 0.30000000000000004\n", "a state with no morph in it is the version-2 chunk, byte for byte");
    SavedState morphed = s;
    using preview::morph::KeyChoice;
    morphed.morph.x = 0.1 + 0.2;
    morphed.morph.y = 1;
    morphed.morph.settings = {{preview::morph::kOn, 1}, {preview::morph::kTemperature, 0.123456789}};
    morphed.morph.corners = {{1, 3, 0.25}, {3, 8, 0.1 + 0.7}};
    morphed.morph.texts = {{2, 1, "1, 3/2 \"odd\" \\ \x01"}};
    morphed.morph.modes = {{3, {horde::KeyMode::Quantum, horde::KeyCount::Two}}, {horde::kRatiosKeyId[1], {horde::KeyMode::Exempt, horde::KeyCount::AsMorph}}};
    const std::string mtext = write_state(morphed);
    SavedState mback;
    const bool read = read_state(mtext, mback);
    const auto& mb = mback.morph;
    check(has(mtext, "horde-engine-preview 3\n") && read && mback.values.size() == 1 && mb.x == 0.1 + 0.2 && mb.y == 1 && mb.settings.size() == 2 &&
              mb.settings[1].second == 0.123456789 && mb.corners.size() == 2 && mb.corners[1].corner == 3 && mb.corners[1].id == 8 &&
              mb.corners[1].v == 0.1 + 0.7 && mb.texts.size() == 1 && mb.texts[0].text == morphed.morph.texts[0].text && mb.modes.size() == 2 &&
              mb.modes[0].choice == (KeyChoice{horde::KeyMode::Quantum, horde::KeyCount::Two}) && mb.modes[1].id == horde::kRatiosKeyId[1],
          "the morph's corners, position, settings, texts and modes round-trip exactly (version 3)");
    std::string cut = mtext;
    const std::size_t line = cut.find("morph corner 3");
    cut.erase(line, cut.find('\n', line) - line + 1);
    SavedState cut_back;
    check(read_state(cut, cut_back) && cut_back.morph.corners.size() == 1, "control: a chunk missing a corner line reads back without it");
    SavedState v2;
    check(read_state("horde-engine-preview 2\npreset 5\n4097 0.25\n", v2) && v2.morph.empty() && v2.values.size() == 1,
          "a version-2 chunk still loads, with the morph at its defaults");

    // A four-corner patch as the human might leave one: 40 rows apart in each of B, C and D, the morph on, a few modes.
    SavedState four = defaults;
    four.morph.x = 0.3125;
    four.morph.y = 0.6875;
    four.morph.settings = {{preview::morph::kOn, 1}, {preview::morph::kLaw, 0}};
    for (int c = 0; c < 4; ++c)
      for (std::uint32_t r = 0; r < 40; ++r) four.morph.corners.push_back({c, 4096 + r * 7, 0.123456789012345 + 0.01 * r});
    for (std::uint32_t r = 0; r < 6; ++r) four.morph.modes.push_back({4096 + r, {horde::KeyMode::Quantum, horde::KeyCount::AsMorph}});
    const std::string four_text = write_state(four);
    std::printf("     a four-corner chunk (160 corner rows, 6 modes) is %zu bytes\n", four_text.size());
    check(four_text.size() < 16384, "a four-corner chunk stays well under the size a host drops silently");
  }

  // ---- the polyphony cap ----------------------------------------------------------------------------------------
  {
    // Shaped as the engine's voice.polyphony: a Step of the counts 1 to 64. Every bank preset asks for 16, so the
    // bank can't show a preset left alone; this does.
    const horde::ParamInfo poly{9, "voice.polyphony", 1, 64, 15, Law::Step, ParamClass::Device, Rate::Stepped, false, 64, false};
    const auto v = [](int count) { return (count - 1) / 63.0; };
    check(capped_count_v(poly, v(16), 8) == v(8), "a load asking for 16 voices gets 8");
    check(capped_count_v(poly, v(8), 8) == v(8) && capped_count_v(poly, v(6), 8) == v(6), "control: a load asking for 8 or fewer keeps its own");
  }

  // ---- the engine's own manifest --------------------------------------------------------------------------------
  {
    std::vector<const horde::ParamInfo*> all;
    for (std::size_t i = 0; i < horde::manifest_size(); ++i) all.push_back(&horde::manifest_param(i));
    const KeyMap engine(all);
    bool every = !all.empty();
    int reserved = 0;
    for (const auto* p : all) {
      const int at = engine.find(p->key);
      every = every && at >= 0 && engine.row(at).id == p->id;
      reserved += p->reserved ? 1 : 0;
    }
    check(every, "every engine row's key maps to its own id");
    const std::string ours = manifest_hash(all);
    std::printf("     engine manifest: %zu rows, %d reserved, hash %s\n", all.size(), reserved, ours.c_str());
    // Reported, not gated: the page's own hash, from its shell-data.js, as last read. A stepped row of consecutive
    // integers (octave, unison, ...) gives its first and last numbers (horde_engine.h, ParamInfo::lo), which GUI 3's
    // generator follows since 2026-11-05. The page checks its hash against the hello reply at run time; this line
    // only shows whether the copy here has gone stale.
    constexpr const char* kPageHash = "ca64bf73f67efe067c87cfd739a2d4d007bfa9cd72c29d314913b5879f38fd29";   // shell-data.js, gui-plugin-mode, 2026-11-05
    std::printf("     GUI 3's page hash %s: %s\n", kPageHash, ours == kPageHash ? "the same" : "DIFFERENT from the engine's (reported, not gated)");
  }

  std::printf("%s (%d failed)\n", failures ? "bridge_test: RED" : "bridge_test: GREEN", failures);
  return failures ? 1 : 0;
}
