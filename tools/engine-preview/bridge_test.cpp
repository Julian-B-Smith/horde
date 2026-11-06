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
  for (const char* type : {"morph", "position", "modes", "rack", "midi", "clear"}) {
    const Message m = parse_message("{\"type\":\"" + std::string(type) + "\",\"seq\":1,\"values\":{\"out.level\":0.5}}", keys);
    check(m.kind == Kind::Held && m.values.empty() && !m.unknown_type, std::string(type) + ": held and dropped, nothing sent");
  }
  {
    const Message m = parse_message("{\"type\":\"teleport\",\"seq\":7}", keys);
    check(m.kind == Kind::Held && m.unknown_type && m.seq == 7, "a type nobody knows is acknowledged, held, and reported");
    check(parse_message("{\"type\":\"set\",\"values\":{}}", keys).kind == Kind::Invalid, "control: a message with no seq can't be acknowledged");
    check(parse_message("not json", keys).kind == Kind::Invalid, "control: unreadable text is refused, never thrown");
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
