// Not run by ./verify: it links the clean-room engine for its manifest, which isn't in this public repo or in CI yet
// (B568). build.sh runs it.
//
// The bridge's pure half (bridge_core.h, B628) held to section 7's rows of docs/proposals/gui-on-engine.md, headless:
// no plug-in, no window. Each check has a control that must read the other way, so a check that can't fail can't pass.
//   bridge_test
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bridge_core.h"
#include "flush_to_zero.h"
#include "horde_engine.h"
#include "rack/descriptor.h"
#include "rack/slot_table.h"
#include "choc/text/choc_JSON.h"

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
    // B639: MO-21's list, its law and MO-24's lock
    const Message pins = parse_message("{\"type\":\"waypoints\",\"seq\":21,\"key\":\"env1.decay\",\"points\":[{\"x\":0.5,\"y\":0.25,\"v\":0.75},{\"x\":0.25,\"y\":0.5,\"v\":1.5}]}", keys);
    check(pins.kind == Kind::Waypoints && pins.readable && pins.key == keys.find("env1.decay") && pins.points.size() == 2 && pins.points[1].v == 1.5,
          "waypoints: one key's list, in order, as sent (the resolver clamps and judges it, MO-21)");
    check(!parse_message("{\"type\":\"waypoints\",\"seq\":22,\"key\":\"env1.decay\",\"points\":[{\"x\":0.5,\"y\":0.25}]}", keys).readable,
          "control: a waypoint without its value makes the list unreadable, so nothing acts");
    const Message clear = parse_message("{\"type\":\"waypoints\",\"seq\":23,\"key\":\"env1.decay\",\"points\":[]}", keys);
    check(clear.readable && clear.points.empty(), "an empty list is readable: it clears the key's waypoints");
    const Message law = parse_message("{\"type\":\"waypointLaw\",\"seq\":24,\"law\":\"tin\",\"clamp\":false}", keys);
    check(law.kind == Kind::WaypointLaw && law.readable && law.law == horde::WaypointLaw::Tin && !law.clamp, "waypointLaw: TIN, Clamp off");
    check(!parse_message("{\"type\":\"waypointLaw\",\"seq\":25,\"law\":\"spline\",\"clamp\":true}", keys).readable, "control: a law MO-21 lacks is unreadable");
    const Message lock = parse_message("{\"type\":\"lock\",\"seq\":26,\"key\":\"osc1.ratios\",\"offsets\":[0.5,-1,2]}", keys);
    check(lock.kind == Kind::Lock && lock.key == kRatiosKey[0] && lock.has_offsets && lock.offsets[1] == -1, "lock: a key's three offsets, a ratios text's included");
    const Message unlock = parse_message("{\"type\":\"lock\",\"seq\":27,\"key\":\"out.level\",\"offsets\":null}", keys);
    check(unlock.readable && !unlock.has_offsets, "lock: null offsets is no lock");
    check(!parse_message("{\"type\":\"lock\",\"seq\":28,\"key\":\"out.level\",\"offsets\":[1,2]}", keys).readable, "control: two offsets aren't a lock");
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
    const double levels[4] = {1, 0.5, 0, NAN};
    horde::ResolverCounters counters;
    counters.refused_waypoints = 2;
    f.levels = levels;
    f.counters = &counters;
    const std::string with_levels = morph_json(f);
    check(has(with_levels, "\"levels\":[1,0.5,0,null]") && has(with_levels, "\"counters\":{\"refusedWaypoints\":2,\"refusedLocks\":0"),
          "the heard levels ride per source (an engine without them reads null), and the resolver's counters");
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
    check(has(mtext, "horde-engine-preview 4\n") && read && mback.values.size() == 1 && mb.x == 0.1 + 0.2 && mb.y == 1 && mb.settings.size() == 2 &&
              mb.settings[1].second == 0.123456789 && mb.corners.size() == 2 && mb.corners[1].corner == 3 && mb.corners[1].id == 8 &&
              mb.corners[1].v == 0.1 + 0.7 && mb.texts.size() == 1 && mb.texts[0].text == morphed.morph.texts[0].text && mb.modes.size() == 2 &&
              mb.modes[0].choice == (KeyChoice{horde::KeyMode::Quantum, horde::KeyCount::Two}) && mb.modes[1].id == horde::kRatiosKeyId[1],
          "the morph's corners, position, settings, texts and modes round-trip exactly (version 4)");
    SavedState pinned = morphed;
    pinned.morph.waypoints = {{3, {{0.5, 0.25, 0.1 + 0.2}, {0.75, 0.75, 1}}}, {8, {{0.25, 0.5, 0}}}};
    pinned.morph.law = horde::WaypointLaw::Tin;
    pinned.morph.clamp = false;
    pinned.morph.locks = {{3, {0.5, -0.25, 1e-3 + 0.5}}, {horde::kRatiosKeyId[0], {0, 0, 0}}};
    const std::string ptext = write_state(pinned);
    SavedState pback;
    const auto& pb = pback.morph;
    check(read_state(ptext, pback) && pb.waypoints.size() == 2 && pb.waypoints[0].points.size() == 2 && pb.waypoints[0].points[0].v == 0.1 + 0.2 &&
              pb.waypoints[1].id == 8 && pb.law == horde::WaypointLaw::Tin && !pb.clamp && pb.locks.size() == 2 &&
              pb.locks[0].offsets[2] == 1e-3 + 0.5 && pb.locks[1].id == horde::kRatiosKeyId[0],
          "version 4: each key's waypoints in order, the law and its clamp, and the locks round-trip exactly");
    std::printf("     the waypoints and locks add %zu bytes\n", ptext.size() - mtext.size());
    SavedState v3;
    check(read_state("horde-engine-preview 3\npreset 5\nmorph position 0.5 0.5\n", v3) && v3.morph.x == 0.5 && v3.morph.waypoints.empty() &&
              v3.morph.law == horde::WaypointLaw::Tps && v3.morph.clamp,
          "a version-3 chunk still loads, with no waypoints or locks and the default law");
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

  // ---- the rack's part of the chunk (B714): the fx section, the migration of versions 5 and 6, the loud refusal ------
  // The table is this build's (the rack's registry: tools/rack/modules.json's default modules); the lines below are the
  // pre-B714 chunk lines themselves, written out from filter_core.h and reverb_core.h as they were.
  const rack::SlotTable& table = rack::registered_table();
  const auto slot_of = [&](const std::string& module) {
    for (std::size_t s = 0; s < table.slots.size(); ++s)
      if (table.slots[s].module == module) return int(s);
    return -1;
  };
  {
    check(table.refused.empty() && !table.slots.empty(), "the rack's table builds (" + std::to_string(table.slots.size()) + " slots)");
    SavedState plain;
    plain.preset = 3;
    const std::string before = write_state(plain);
    check(!has(before, "\nfx") && has(before, kStateMagic2), "control: a dry rack writes no fx section and no new version: the chunk it always was");
    SavedState on = plain;
    rack::FxRecord r;
    r.n = table.slots[0].n;
    r.module = table.slots[0].module;
    r.version = table.slots[0].module_version;
    for (int c = 0; c < 4; ++c) {
      r.on[c] = true;
      r.keys[c] = {{table.slots[0].keys[0].address, 0.1 + 0.2, 0.25}};
    }
    on.fx.slots.push_back(r);
    const std::string text = write_state(on);
    SavedState back;
    LoadReport report;
    check(has(text, kStateMagic7) && has(text, "\nfx 1\n") && has(text, "\nfx.end\nprefs 1\nprefs.end\n") && read_state(text, back, &table, &report) &&
              back.fx.slots.size() == 1 && back.fx.slots[0].keys[2].size() == 1 && back.fx.slots[0].keys[2][0].units == 0.1 + 0.2 && back.fx.slots[0].keys[2][0].v == 0.25 && back.fx.slots[0].on[3] &&
              report.version == 7 && !report.migrated && report.refused.empty(),
          "the fx section round trips in version 7, four corners and the device preferences' (empty) section, its units exactly");
    check(write_state(back) == text, "save, load, save is byte-identical");
    SavedState old;
    old.fx = on.fx;   // a stale state a load must not keep
    check(read_state(before, old, &table) && old.fx.empty(), "a chunk with no fx section loads dry");
  }
  {
    const int weir = slot_of("fxfilter"), scape = slot_of("scape");
    SavedState v5, v6, hostile;
    LoadReport r5, r6, rh;
    check(read_state(std::string(kStateMagic5) + "\npreset 3\nfilter 1 3 0.45 0.6 0.25 0.75\n", v5, &table, &r5) && r5.version == 5 && r5.migrated == (weir >= 0) &&
              v5.fx.slots.size() == (weir >= 0 ? 1u : 0u) && (weir < 0 || (v5.fx.slots[0].on[0] && v5.fx.slots[0].keys[0].size() == 5)),
          "a version 5 chunk's line migrates into fx through the ledger's frozen table: the switch and five keys");
    check(read_state(std::string(kStateMagic6) + "\npreset 3\nscape on 1\nscape 10 6\nscape 27 -3\n", v6, &table, &r6) && r6.migrated == (scape >= 0) &&
              v6.fx.slots.size() == (scape >= 0 ? 1u : 0u) && (scape < 0 || (v6.fx.slots[0].keys[0].size() == 1 && v6.fx.slots[0].device.size() == 1)),
          "a version 6 chunk's lines migrate into fx: a morph key in every corner, a Device key once, outside them");
    check(read_state(std::string(kStateMagic6) + "\nfilter nan 9 2 -1 5 7\nscape 29 1\nscape -1 2\nscape 9 nan\nscape 9 inf\n", hostile, &table, &rh) &&
              hostile.fx.empty() && !rh.migrated,
          "control: a legacy line with a non-finite value, or an id outside its range, is no line at all");
    SavedState no_table;
    check(read_state(std::string(kStateMagic5) + "\nfilter 1 3 0.45 0.6 0.25 0.75\n", no_table) && no_table.fx.empty(), "control: with no table, a legacy line is read as no line");
  }
  {
    SavedState x;
    LoadReport r;
    check(!read_state("horde-engine-preview 8\npreset 1\n", x, &table, &r) && has(r.refused, "unknown") && has(load_json(r), "\"refused\":{\"reason\":"),
          "a chunk version this build does not know is refused, with its reason, for the frame's load member");
    check(!read_state("something else\npreset 3\n", x, &table, &r) && has(r.refused, "not a horde-engine-preview chunk"), "an unknown magic is refused, with its reason");
    LoadReport ok;
    check(read_state(std::string(kStateMagic4) + "\npreset 1\n", x, &table, &ok) && load_json(ok) == "{\"version\":4,\"migrated\":false}",
          "control: a known version reads, and its load member carries no refusal");
  }

  // ---- the rack's keys on the page (B711, B714): MO-28's names and the ledger's pre-B714 aliases -----------------------
  {
    const std::vector<TestRow> tests = test_rows(table);
    const TestKeys tk(tests);
    // What each key must name, derived from the table apart from test_rows(): a switch's id and each key's.
    std::vector<std::pair<std::string, std::uint32_t>> expect;
    for (const auto& row : table.slots) {
      expect.emplace_back(rack::switch_name(row.n), row.on_id);
      for (const auto& k : row.keys) expect.emplace_back(rack::key_name(row.n, k.address), k.host_id);
    }
    for (const auto& a : table.aliases)
      expect.emplace_back(a.preview, a.key < 0 ? table.slots[std::size_t(a.slot)].on_id : table.slots[std::size_t(a.slot)].keys[std::size_t(a.key)].host_id);
    bool same = tests.size() == expect.size();
    for (std::size_t i = 0; same && i < tests.size(); ++i) same = tests[i].key == expect[i].first && tests[i].id == expect[i].second;
    std::size_t published = 0;
    for (const TestRow& r : tests) published += r.published ? 1 : 0;
    check(same && published == table.aliases.size(), "one row per switch and key by MO-28's name, and one per ledger alias, each at its own host id (" + std::to_string(tests.size()) +
                                                          " rows, " + std::to_string(published) + " of them the pre-B714 keys)");
    bool shaped = true;
    for (const TestRow& r : tests)
      shaped = shaped && (r.stepped ? int(r.labels.size()) == int(std::lround(r.max - r.min)) + 1 || r.units : r.labels.empty()) && r.min <= r.def && r.def <= r.max;
    check(shaped, "each row carries its range and default, and a stepped one a label per option");
    // The pre-B714 forms, data from the ledger: a host-form row (the old filter's) is the host value itself; a plain row
    // (the old Scape's) is lin in module units, reaching the host through the key's law.
    const int decay_at = tk.find("preview.scape.decay"), cutoff_at = tk.find("preview.filter.cutoff");
    if (decay_at >= 0) {
      const TestRow& decay = tests[std::size_t(decay_at)];
      const double scaled = 0.375 * (decay.max - decay.min);
      check(decay.units && decay.min == 0.2 && decay.max == 12 && decay.def == 2.2 && decay.unit == "s" && test_host(decay, 0.375) == rack::host_of_units(decay.law, decay.min + scaled) &&
                test_v(decay, rack::host_of_units(decay.law, 12)) == 1,
            "an old Scape row is published as it was (0.2 to 12 s, 2.2), and its v reaches the host through the key's law");
    }
    if (cutoff_at >= 0) {
      const TestRow& cutoff = tests[std::size_t(cutoff_at)];
      check(!cutoff.units && cutoff.min == 0 && cutoff.max == 1 && test_host(cutoff, 0.25) == 0.25 && test_v(cutoff, 0.5) == 0.5,
            "an old filter row is published as it was, its v the host value itself (the cutoff's place on its log law)");
    }

    const std::string body = "{\"type\":\"set\",\"seq\":40,\"values\":{\"" + expect[0].first + "\":1,\"" + expect[1].first +
                             "\":0.5,\"preview.nope\":0.5,\"fx/99/none\":0.5,\"out.level\":0.25}}";
    const Message set = parse_message(body, keys, &tk);
    check(set.test_values.size() == 2 && tk.row(set.test_values[0].row).id == expect[0].second && set.test_values[1].v == 0.5 && set.values.size() == 1 &&
              set.unknown == std::vector<std::string>{"preview.nope", "fx/99/none"},
          "a `set` sends each rack key to its own row at the page's v, beside the engine rows, and reports an unknown rack key");
    const Message dropped = parse_message(body, keys);
    check(dropped.test_values.empty() && dropped.unknown.size() == 4 && dropped.values.size() == 1,
          "control: with no rack keys, every such key is reported unknown and nothing reaches them");
    const Message begin = parse_message("{\"type\":\"gesture\",\"seq\":41,\"key\":\"" + expect[1].first + "\",\"phase\":\"begin\"}", keys, &tk);
    const Message stray = parse_message("{\"type\":\"gesture\",\"seq\":42,\"key\":\"fx/99/none\",\"phase\":\"begin\"}", keys, &tk);
    check(begin.gesture_test >= 0 && tk.row(begin.gesture_test).id == expect[1].second && begin.gesture_row < 0 && stray.gesture_test < 0,
          "a gesture on a rack key opens that row's gesture, never an engine row's; an unknown one reaches nothing");

    const auto audit = [&](const TestKeys& map) {
      int wrong = 0;
      for (const auto& [key, id] : expect) {
        const Message m = parse_message("{\"type\":\"set\",\"seq\":44,\"values\":{\"" + key + "\":0.5}}", keys, &map);
        wrong += m.test_values.size() == 1 && map.row(m.test_values[0].row).id == id ? 0 : 1;
      }
      return wrong;
    };
    std::vector<TestRow> rotated = tests;
    for (std::size_t i = 0; i < rotated.size(); ++i) rotated[i].key = tests[(i + 1) % tests.size()].key;
    const int shifted_wrong = audit(TestKeys(rotated));
    check(audit(tk) == 0, "every rack key, sent through the bridge, reaches its own host id");
    check(shifted_wrong == int(expect.size()), "control: with the key map one row off, every key misses (" + std::to_string(shifted_wrong) + ")");

    const std::string build = "engine x, build 1", hash = "abc", slots = "{\"ledger\":\"x\",\"refused\":[],\"slots\":[]}";
    Pieces with;
    with.build = &build;
    with.engine_hash = &hash;
    with.test_rows = &tests;
    with.slots = &slots;
    const std::string reply = snapshot_json(with);
    Pieces without = with;
    without.test_rows = nullptr;
    without.slots = nullptr;
    bool parsed = false;
    std::size_t listed = 0;
    try {
      const choc::value::Value doc = choc::json::parse(reply);
      parsed = doc["hello"]["preview"]["value"].getString() == std::string_view(kTestValueRule) && doc["hello"]["slots"]["ledger"].getString() == std::string_view("x");
      listed = doc["hello"]["preview"]["rows"].size();
    } catch (const std::exception&) {}
    check(parsed && listed == published, "the hello reply's `preview` is JSON with the value rule and one descriptor per pre-B714 key, beside `slots`");
    check(!has(snapshot_json(without), "preview") && has(snapshot_json(without), "\"hello\":{\"build\":\"engine x, build 1\",\"manifest_hash\":\"abc\"}"),
          "control: a reply without the rack's keys is the one it always was");

    // hello.preview is main's, byte for byte (fixtures/hello-preview-main.json: the `preview` member a build of main
    // 3159a196 published), but for the ONE ruled difference: the filter cutoff's default is the module's own 1000 Hz
    // (Weir's manifest), not State{}'s 0.56628 (999.70 Hz). It is put back here before the comparison, and only there.
    {
      std::ifstream f(std::string(HORDE_PREVIEW_FIXTURES) + "/hello-preview-main.json");
      std::stringstream ss;
      ss << f.rdbuf();
      const std::string main_preview = ss.str();
      std::string ours = test_rows_json(tests);
      const std::size_t row = ours.find("\"key\":\"preview.filter.cutoff\"");
      const std::size_t at = row == std::string::npos ? row : ours.find("\"default\":", row);
      bool moved = false;
      if (at != std::string::npos) {
        const std::size_t from = at + 10, to = ours.find(',', from);
        moved = ours.substr(from, to - from) != "0.56628";
        ours.replace(from, to - from, "0.56628");
      }
      check(!main_preview.empty() && ours == main_preview && moved,
            "hello.preview is byte for byte main's (names, labels, ranges, units, order), the cutoff's ruled default apart (" + std::to_string(main_preview.size()) + " bytes)");
    }
  }

  // ---- what each module does, through the rack (B660's and B710's DSP checks, kept) ----------------------------------
  // Acceptance (1) bans a per-module branch in the HOSTING code; a test may know what a module is for. A small table per
  // module, each value set on its slot by address: the filter's LP at 200 Hz takes the high band down by more than 30 dB
  // and is not a mute, LP at 20 kHz leaves it (its no-op control: the slot off reads no drop at all); Scape at its
  // defaults rings on after a burst (-50 dB of the burst, measured -37 dB at 8350fb8; its no-op control: the slot off
  // reads exact silence there). The thresholds are the ones registered for these checks before B714.
  {
    constexpr double kRate = 44100;
    const auto slot_of_module = [&](const char* module) {
      for (std::size_t s = 0; s < table.slots.size(); ++s)
        if (table.slots[s].module == module) return int(s);
      return -1;
    };
    const auto key_index = [&](int s, const char* address) {
      for (std::size_t k = 0; k < table.slots[std::size_t(s)].keys.size(); ++k)
        if (table.slots[std::size_t(s)].keys[k].address == address) return int(k);
      return -1;
    };
    using Setting = std::pair<const char*, double>;   // address, units
    // A fresh rack with one slot on (or none, `on` false) at these settings, the input run through it in blocks of
    // `block`, under flush-to-zero as the plug-in runs it. Mono in, both channels out.
    const auto run = [&](int s, bool on, const std::vector<Setting>& settings, const std::vector<float>& in, std::size_t block,
                         std::vector<float>* right = nullptr) {
      rack::Rack r;
      r.attach(table);
      r.prepare(kRate);
      r.set_on(s, on);
      for (const auto& [address, units] : settings) r.set_units(s, key_index(s, address), units);
      std::vector<float> l = in, rr = in;
      horde2::underFlushToZero([&] {
        for (std::size_t at = 0; at < l.size(); at += block) r.process(l.data() + at, rr.data() + at, std::uint32_t(std::min(block, l.size() - at)), false);
      });
      if (right) *right = rr;
      return l;
    };
    const auto high_band = [](const std::vector<float>& x) {   // second difference: a crude high-pass, +12 dB/oct
      double e = 0;
      for (std::size_t i = 2; i < x.size(); ++i) { const double d = double(x[i]) - 2.0 * double(x[i - 1]) + double(x[i - 2]); e += d * d; }
      return e;
    };
    const auto rms = [](const std::vector<float>& x) {
      double e = 0;
      for (float v : x) e += double(v) * double(v);
      return std::sqrt(e / double(x.size()));
    };
    const int weir = slot_of_module("fxfilter");
    if (weir < 0) check(false, "this build hosts no filter module, so the filter's behaviour checks did not run");
    else {
      std::vector<float> noise(44100);   // one second at kRate
      std::uint32_t lcg = 12345;
      for (auto& x : noise) { lcg = lcg * 1664525u + 1013904223u; x = float(double(lcg >> 8) / double(1 << 24) - 0.5); }
      const std::vector<Setting> lp = {{"fxfilter.mode", 0}, {"fxfilter.cutoff", 200}, {"fxfilter.resonance", 0}, {"fxfilter.morph", 0}, {"fxfilter.mix", 1}};
      std::vector<float> right;
      const std::vector<float> filtered = run(weir, true, lp, noise, 512, &right);
      const double drop = high_band(filtered) / high_band(noise);
      check(drop < 1e-3 && rms(filtered) > 1e-3, "the filter's LP at 200 Hz takes the high band down by more than 30 dB and is not a mute (drop " + std::to_string(drop) + ")");
      std::vector<Setting> open = lp;
      open[1].second = 20000;
      const double wide = high_band(run(weir, true, open, noise, 512)) / high_band(noise);
      check(wide > 0.1, "control: LP at 20 kHz leaves the high band (a filter that always cuts would fail here)");
      check(run(weir, false, lp, noise, 512) == noise, "control: the slot off, a no-op, reads no drop at all: the input bit for bit (the drop above is the filter's)");
      std::vector<Setting> dry = lp;
      dry[4].second = 0;
      check(run(weir, true, dry, noise, 512) == noise, "control: mix 0 hands the dry signal back bit for bit");
      check(filtered == right, "both channels get the same filter (a mono input stays mono)");
      check(run(weir, true, lp, noise, 64) == filtered && run(weir, true, lp, noise, 4096) == filtered && run(weir, true, lp, noise, 100) == filtered,
            "the filter's output doesn't depend on how the host cuts the blocks (64, 100, 512, 4096)");
    }
    const int scape = slot_of_module("scape");
    if (scape < 0) check(false, "this build hosts no reverb module, so the reverb's behaviour checks did not run");
    else {
      constexpr std::size_t kN = 44100 * 4, kBurst = 44100 / 4;
      std::vector<float> burst(kN, 0.0f);
      std::uint32_t lcg = 777;
      for (std::size_t i = 0; i < kBurst; ++i) { lcg = lcg * 1664525u + 1013904223u; burst[i] = float(double(lcg >> 8) / double(1 << 24) - 0.5); }
      const auto energy = [&](const std::vector<float>& x, double from_s, double to_s) {
        double e = 0;
        for (std::size_t i = std::size_t(from_s * kRate); i < std::size_t(to_s * kRate) && i < x.size(); ++i) e += double(x[i]) * double(x[i]);
        return e;
      };
      std::vector<float> right;
      const std::vector<float> wet = run(scape, true, {}, burst, 512, &right);
      bool finite = true;
      for (std::size_t i = 0; i < kN; ++i) finite = finite && std::isfinite(wet[i]) && std::isfinite(right[i]);
      const double tail = energy(wet, 0.5, 1.5);
      std::printf("     the reverb at its defaults: tail energy 0.25 s to 1.25 s after the burst %.4g (the burst itself %.4g)\n", tail, energy(burst, 0, 0.25));
      check(finite && tail > 1e-5 * energy(burst, 0, 0.25), "at the reverb's defaults the burst rings on after its input stops (a tail), and every sample is finite");
      check(energy(burst, 0.5, 1.5) == 0 && energy(run(scape, false, {}, burst, 512), 0.5, 1.5) == 0,
            "control: the input itself is silent there, and the slot off, a no-op, reads exact silence: the energy is the reverb's");
      check(run(scape, true, {{"verb.mix", 0}}, burst, 512) == burst, "control: mix 0 hands the input back bit for bit (its exact dry)");
      const double long_tail = energy(run(scape, true, {{"verb.decay", 8}}, burst, 512), 2, 3), short_tail = energy(run(scape, true, {{"verb.decay", 0.2}}, burst, 512), 2, 3);
      std::printf("     tail energy 1.75 s to 2.75 s after the burst: decay 8 s %.4g, decay 0.2 s %.4g\n", long_tail, short_tail);
      check(long_tail > 1000 * short_tail && long_tail > 0, "the values reach the reverb: decay 8 s rings far longer than decay 0.2 s");
      check(wet != right, "a mono input comes out wide (the reverb decorrelates its tail: changes_image, declared)");
    }
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
