/*
 * undo_check — the undo history's oracle (B84 / ADR-160, B186, B191, B222).
 * WIRED: ./verify full.
 *
 *   undo_check [state_fixtures_dir] [preset_dir] [seed]
 *              (defaults: tests/state_fixtures  docs/presets/factory  20260920)
 *
 * Five layers, because the design has parts that fail differently.
 *
 * LAYER 1 — UndoTree, pure. Ring eviction and re-parenting, fork-on-restore,
 * the undo/redo path, the dedup rule, the 200 cap. No plugin, no CLAP: these
 * are statements about a data structure and nothing else should be able to
 * make them red.
 *
 * LAYER 2 — the shell. A restore is BIT-IDENTICAL: for every fixture in the
 * B100 corpus, load it, snapshot it into the tree, move the instrument
 * somewhere else, restore, and require the shell's own stateJson to equal the
 * stored node byte for byte. A restore that is merely close is a restore that
 * silently loses a patch.
 *
 * LAYER 4 — every control marks (B191). A closed gesture bracket on ANY
 * declared parameter makes exactly one node, and a bracket that never closes
 * makes none — the shell half of "changing a parameter through its own control
 * produces exactly one history node". The GUI half (does the control emit a
 * balanced bracket at all, from the keyboard and through a native popup?) is
 * tools/labharness/gui_history_check.mjs; neither half is the property alone.
 *
 * LAYER 3 — the history-fidelity GAUNTLET (B186). Layers 1 and 2 each check
 * one node at a time. The property the player actually relies on is about
 * PAIRS: a tree of real depth and breadth, built by a seeded random sequence
 * of edits, global-preset loads, corner-preset loads, step-backs and
 * undo/redo, and then, for every ordered pair of nodes (a, b), visiting a,
 * then b, then a again must land on a's exact stored state, and visiting b
 * must never mutate what a stores. Same seed, same sequence, every time; the
 * seed is printed and overridable (argv[3]) so a red run is reproducible.
 *
 * LAYER 5 — history fidelity, round 3 (B222), on instances that PROCESS AUDIO,
 * because the morph field only acts inside process() and every layer above
 * drives one that never calls it. Three defects the human heard: the morph
 * toggle's node was named "Morph" and so folded into its neighbour on the
 * rail; switching morph on after any load reverted every edit since; and a
 * node sounded different depending on the node visited before it, because
 * the routing matrix was in no snapshot (B193). Its oracles do not share the
 * node's writer: the engine's own routing and corner readouts, and the audio.
 *
 * THE CONTROL, and its calibration. ADR-160 (3) rules that host parameter
 * events and automation NEVER create a node: history records the player's
 * states, and automation keeps writing over the top exactly as it does after
 * any manual edit. So 500 host param events plus 50 automation-shaped blocks
 * must produce ZERO nodes. On its own that assertion is worthless — a counter
 * wired to nothing also reads zero — so the same instance is then marked the
 * way the GUI marks it and must produce exactly one node. The zero is only
 * evidence because the one beside it proves the detector can see. Layer 3
 * inherits that rule rather than restating it: its edits deliver their values
 * through the very same host flush, so a flush that started marking would
 * show up as extra nodes in every recording row.
 *
 * THE PRESET'S IDENTITY (B174). The global preset's NAME is shell state as of
 * 2026-09-20, which puts it inside history's promise: it is written into the
 * snapshot, so it forks and restores with everything else. The rows at the end
 * are the human's own report expressed as a test — load X, edit, step back,
 * branch onto Y, return to the first branch, and the patch must still report X.
 * Before the name was state there was nothing in the node to report and the
 * page showed Y on both branches, so that row is RED on the binary this change
 * fixes, which is the only thing that makes its green mean anything (L0059).
 *
 * THE DECLARATION AT THE TOP IS NOW MACHINE-CHECKED. This paragraph used to
 * read "STANDALONE AND UNWIRED. Not run by ./verify", which had been false
 * since the human wired the gate on 2026-09-13 and survived two days because
 * test_table_check only tested that a marker EXISTED, never that it was true.
 * B190 layer 1 (2026-09-21) closed that: the header's one `WIRED:`/`UNWIRED:`
 * line is cross-checked against ./verify's own text, in both directions, and
 * this file is the case that motivated it. Free prose can still contradict the
 * gate list — only reading catches that — which is why the relationship claim
 * now lives in one line with a fixed grammar instead of in a paragraph.
 */

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "statefix_common.h"
#include "../src/force_core.h"   // rngNext: SPEC 5.7's one permitted stream
#include "../src/hypersaw_debug.h"
#include "../src/undo_tree.h"

namespace fs = std::filesystem;
using namespace statefix;
using hypersaw::UndoTree;

namespace
{

int g_failures = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_failures++;
}

std::string undoOp(const clap_plugin_t *p, const char *op, int arg = 0)
{
  return hypersaw_debug_undo(p, op, arg);
}
int undoInt(const clap_plugin_t *p, const char *op, int arg = 0)
{
  return std::atoi(undoOp(p, op, arg).c_str());
}
/* What a node taken NOW would store — the form every "does the instrument
   hold what the node holds" row compares against (B222). It is the preset
   JSON PLUS the routing matrix, which the preset JSON does not carry (B193),
   so a row that compared nodes against saveJson would be blind to exactly the
   state this history lost. saveJson stays where a row is about the PRESET
   transport (the round-trip evidence, the preset name). */
std::string liveJson(const clap_plugin_t *p) { return undoOp(p, "live"); }

// Distinct payloads without pretending to be real patches: layer 1 is about
// the graph, and a 35 KB string per node would only slow it down.
std::string payload(int i) { return "{\"n\":" + std::to_string(i) + "}"; }

/* ---------------- layer 1: the pure tree ---------------- */
void treeChecks()
{
  {
    UndoTree t;
    check(t.size() == 0 && t.current() == UndoTree::kNone, "tree: empty on construction");
    const int a = t.push("a", payload(1), 1);
    const int b = t.push("b", payload(2), 2);
    const int c = t.push("c", payload(3), 3);
    check(t.size() == 3 && t.current() == c, "tree: push makes a child and stands on it");
    check(t.node(c).parent == b && t.node(b).parent == a && t.node(a).parent == UndoTree::kNone,
          "tree: a straight edit run is a chain");
    check(t.undo() == b && t.undo() == a, "tree: undo walks to the parent");
    check(t.undo() == UndoTree::kNone && t.current() == a, "tree: undo at a root is a no-op");
    check(t.redo() == b && t.current() == b, "tree: redo follows the child");

    // FORK: standing on an old node and editing branches rather than truncating.
    t.restore(a);
    const int d = t.push("d", payload(4), 4);
    check(t.node(d).parent == a, "tree: push after restore FORKS from the restored node");
    check(t.liveAt(b) && t.liveAt(c), "tree: the abandoned branch survives the fork");
    t.restore(a);
    check(t.redo() == d, "tree: redo takes the branch most recently created");
  }

  {
    UndoTree t;
    t.push("a", payload(1), 1);
    const int n = t.size();
    const int again = t.push("b", payload(1), 2);
    check(t.size() == n && again == t.current(), "tree: a snapshot equal to current is no node");
  }

  {
    UndoTree t;
    for (int i = 0; i < 250; i++) t.push("x", payload(i), (uint64_t)i);
    int live = 0;
    for (int i = 0; i < t.capacity(); i++) live += t.liveAt(i) ? 1 : 0;
    check(t.capacity() == 200, "tree: the cap is 200");
    check(t.size() == 200 && live == 200, "tree: 250 pushes leave exactly 200 live nodes");
  }

  {
    /* EVICTION PROTECTS THE SPINE (ADR-160 A2, human ruling 2026-09-14). Root
       a, a fork d off a, then a straight run from a's other child b until the
       ring is full plus one. The only node off the spine is d, so d — not a,
       the oldest — is the one evicted; the spine keeps its root. */
    UndoTree t;
    const int a = t.push("a", payload(0), 0);
    t.restore(a);
    const int d = t.push("d", payload(2), 2);
    t.restore(a);
    const int b = t.push("b", payload(1), 1);
    for (int i = 3; i < 201; i++) t.push("fill", payload(i), (uint64_t)i);   // a + d + b + 198 = 201 -> ONE eviction
    check(!t.liveAt(d) || t.node(d).label != "d", "tree: the off-spine fork is evicted, not the oldest");
    check(t.liveAt(a) && t.node(a).label == "a" && t.node(a).parent == UndoTree::kNone,
          "tree: the spine's root survives eviction");
    check(t.liveAt(b) && t.node(b).parent == a, "tree: the spine's edges are intact after eviction");
  }

  {
    /* THE HUMAN'S SCENARIO (2026-09-14): a trunk of 189 edits, then edits
       190-220 as four branches off edit 6, the player standing on the last.
       220 states in a ring of 200 = 20 evictions. Under oldest-first, edit 6
       went at push 206 and the branches became unrelated roots; under the
       spine rule the abandoned trunk 7..26 goes and every branch still
       reaches edit 6. */
    UndoTree t;
    int slotOfEdit[221];
    for (int e = 1; e <= 189; e++) slotOfEdit[e] = t.push("trunk", payload(e), (uint64_t)e);
    const int base = slotOfEdit[6];
    int e = 190;
    for (int br = 0; br < 4; br++)
    {
      t.restore(base);
      for (int k = 0; k < 8 && e <= 220; k++, e++) slotOfEdit[e] = t.push("branch", payload(e), (uint64_t)e);
    }
    check(t.size() == 200, "scenario: the ring holds 200 after 220 edits");
    check(t.liveAt(base) && t.node(base).label == "trunk" && t.node(base).tick == 6,
          "scenario: edit 6 (the shared base) survives");
    bool spineOk = true;
    for (int k = 1; k <= 5; k++) spineOk = spineOk && t.liveAt(slotOfEdit[k]) && t.node(slotOfEdit[k]).tick == (uint64_t)k;
    check(spineOk, "scenario: edits 1-5 (the spine above the base) survive");
    bool trunkGone = true;
    for (int k = 7; k <= 26; k++) trunkGone = trunkGone && !(t.liveAt(slotOfEdit[k]) && t.node(slotOfEdit[k]).tick == (uint64_t)k);
    check(trunkGone, "scenario: the abandoned trunk 7-26 is what was evicted");
    bool reach = true;
    for (int k = 190; k <= 220; k++)
    {
      int n = slotOfEdit[k], g = 0;
      while (t.liveAt(n) && n != base && g++ < 300) n = t.node(n).parent;
      reach = reach && n == base;
    }
    check(reach, "scenario: every recent branch still reaches edit 6 — siblings keep their shared base");
    // CONTROL: the rule is real — an oldest-first ring would have taken edit 1.
    check(t.liveAt(slotOfEdit[1]) && t.node(slotOfEdit[1]).tick == 1, "scenario control: edit 1 is live (oldest-first would have evicted it)");
  }

  {
    /* The parent edge can never point at a slot whose occupant is YOUNGER than
       the child — the only way a recycled slot could masquerade as a parent. */
    UndoTree t;
    for (int i = 0; i < 400; i++)
    {
      t.push("x", payload(i), (uint64_t)i);
      if (i % 5 == 0) t.restore(i % t.capacity());
    }
    bool ok = true;
    for (int i = 0; i < t.capacity(); i++)
      if (t.liveAt(i) && t.node(i).parent != UndoTree::kNone)
        ok = ok && t.liveAt(t.node(i).parent) && t.node(t.node(i).parent).born < t.node(i).born;
    check(ok, "tree: no parent edge points at a slot recycled after the child was born");
  }

  {
    // The structural invariant, after a churn deep enough to have evicted the
    // whole ring twice and with forks scattered through it.
    UndoTree t;
    for (int i = 0; i < 500; i++)
    {
      t.push("x", payload(i), (uint64_t)i);
      if (i % 7 == 0) t.restore(i % t.capacity());   // forks, wherever they land
    }
    bool sane = true;
    int roots = 0;
    for (int i = 0; i < t.capacity(); i++)
    {
      if (!t.liveAt(i)) continue;
      const int par = t.node(i).parent;
      if (par == UndoTree::kNone) { roots++; continue; }
      if (!t.liveAt(par) || par == i) sane = false;
    }
    check(sane, "tree: after churn every parent edge points at a LIVE node or kNone");
    check(roots >= 1 && t.size() == 200, "tree: the churned forest is full and has a root");
  }
}

/* ---------------- layer 2: the shell ---------------- */

// Params the mutation may touch. NOT 178: ADR-147 rules the specimen toggle a
// GUI preference that stateJson writes and applyStateJson deliberately skips,
// so it is outside what a restore promises and would fail this check for a
// reason that is not a bug.
const clap_id kMutable[] = {4, 8, 14, 17, 100, 1004, 1008};

void mutate(const clap_plugin_t *p, double v)
{
  EvList ev;
  for (clap_id id : kMutable) ev.push(id, v);
  paramsOf(p)->flush(p, &ev.list, &kOut);
  drain(p);
}

size_t g_maxSnapshot = 0;

void fixtureChecks(const std::string &dir)
{
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(dir, ec))
  {
    const std::string n = e.path().filename().string();
    if (isChunk(n) || isJson(n)) names.push_back(n);
  }
  std::sort(names.begin(), names.end());
  check(!names.empty(), "fixtures: the corpus is not empty (" + dir + ")");

  for (const std::string &n : names)
  {
    std::string blob;
    if (!readFile(dir + "/" + n, blob)) { check(false, "fixtures: read " + n); continue; }

    const clap_plugin_t *p = makePlugin();
    const bool loaded = isChunk(n) ? loadChunk(p, blob) : loadJson(p, blob);
    drain(p);
    check(loaded, "fixtures: " + n + " loads");

    // The editor opening: one pump takes the root snapshot (and absorbs the
    // "host load" mark the chunk path left pending — they are the same state).
    undoOp(p, "service");
    const int root = undoInt(p, "current");
    check(undoInt(p, "size") == 1 && root >= 0, "fixtures: " + n + " -> ONE node on open");
    const std::string rootJson = undoOp(p, "json", root);
    g_maxSnapshot = std::max(g_maxSnapshot, rootJson.size());
    check(rootJson == liveJson(p), "fixtures: " + n + " root node IS the live state");

    // Move somewhere else, mark it, and check the tree actually forked forward.
    mutate(p, 0.371);
    undoOp(p, "mark", 1);
    undoOp(p, "service");
    check(undoInt(p, "size") == 2 && undoInt(p, "parent", undoInt(p, "current")) == root,
          "fixtures: " + n + " -> the edit is a child of the root");
    check(liveJson(p) != rootJson, "fixtures: " + n + " the mutation actually moved the state");

    // Restore, drain, and demand byte-for-byte.
    const bool back = undoOp(p, "restore", root) == "1";
    drain(p);
    check(back && undoInt(p, "current") == root, "fixtures: " + n + " restore lands on the node");
    check(liveJson(p) == rootJson, "fixtures: " + n + " restore is BIT-IDENTICAL");
    check(undoInt(p, "size") == 2, "fixtures: " + n + " restore creates NO node");

    p->destroy(p);
  }
}

/* The ADR-160 (3) ruling, with its calibration. */
void automationControl()
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  p->start_processing(p);

  undoOp(p, "service");
  const int base = undoInt(p, "size");
  check(base == 1, "control: one root node before the host touches anything");

  // 500 host parameter events, the shape a control surface or a generic
  // "write all" produces.
  for (int i = 0; i < 500; i++)
  {
    EvList ev;
    ev.push(kMutable[i % (int)(sizeof kMutable / sizeof *kMutable)], 0.001 * (double)(i % 900));
    paramsOf(p)->flush(p, &ev.list, &kOut);
    undoOp(p, "service");
  }
  check(undoInt(p, "size") == base, "control: 500 host param events produce ZERO nodes");

  // 50 automation-shaped blocks: a value arriving INSIDE process(), every
  // block, the way a DAW lane feeds a moving parameter.
  std::vector<float> L(256), R(256);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = chans;
  ob.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = 256;
  proc.audio_outputs = &ob;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;
  for (int b = 0; b < 50; b++)
  {
    EvList ev;
    ev.push(4, 0.02 * (double)(b % 40));
    proc.in_events = &ev.list;
    p->process(p, &proc);
    undoOp(p, "service");
  }
  check(undoInt(p, "size") == base, "control: 50 automation blocks produce ZERO nodes");

  /* CALIBRATION (L0032): the two zeros above are only evidence if this
     instance's counter can move at all. Mark it the way the GUI's gesture-end
     latch marks it and require exactly one new node. */
  undoOp(p, "mark", 7);
  undoOp(p, "service");
  check(undoInt(p, "size") == base + 1,
        "control CALIBRATION: a GUI-shaped mark on the SAME instance does produce a node");

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

/* ---------------- the global preset's IDENTITY (B174) ---------------- */

/* A patch that is recognisably neither the default nor the other one, taken to
   a FIXED POINT of load-then-save: a parameter the loader clamps or quantises
   would otherwise make "a just-loaded patch matches its own preset" fail for a
   reason that is not this change's (the o1.tilt alias clamp B186 reported is
   exactly that shape, and it is out of scope here). A settled patch asks the
   asterisk the question it is for: has the PLAYER moved anything? */
std::string settledPatch(double v)
{
  const clap_plugin_t *p = makePlugin();
  mutate(p, v);
  const std::string once = saveJson(p);
  loadJson(p, once);
  const std::string j = saveJson(p);
  p->destroy(p);
  return j;
}

// The stored name, read out of a state blob the way the GUI's binding reads it.
std::string nameIn(const std::string &json)
{
  const size_t k = json.find("\"presetName\"");
  if (k == std::string::npos) return "";
  const size_t q0 = json.find('"', json.find(':', k) + 1);
  if (q0 == std::string::npos) return "";
  const size_t q1 = json.find('"', q0 + 1);
  return q1 == std::string::npos ? "" : json.substr(q0 + 1, q1 - q0 - 1);
}

void presetIdentityChecks()
{
  const std::string X = settledPatch(0.211), Y = settledPatch(0.733);
  check(X != Y && nameIn(X).empty() && nameIn(Y).empty(),
        "identity: two DIFFERENT patches, neither of them named");

  /* 1. THE NAME IS STATE — it reaches both blobs, comes back out of both, and
     an unnamed patch writes no key at all, which is what keeps every chunk
     saved before this change byte-for-byte what it was. */
  {
    const clap_plugin_t *p = makePlugin();
    check(saveJson(p).find("\"presetName\"") == std::string::npos &&
              saveChunk(p).find("presetname=") == std::string::npos,
          "identity: an unnamed patch writes NO key (existing chunks are bit-inert)");

    check(hypersaw_debug_apply_named(p, X.c_str(), "Squids"), "identity: a named load applies");
    drain(p);
    check(nameIn(saveJson(p)) == "Squids", "identity: the name is in the JSON chunk");
    const std::string blob = saveChunk(p);
    check(blob.find("\npresetname=Squids\n") != std::string::npos,
          "identity: the name is in the HOST chunk (it survives reopening the project)");

    // The JSON the GUI would SAVE carries its own name, so loading it back
    // needs no help from whatever listed it.
    const std::string named = saveJson(p);
    p->destroy(p);

    const clap_plugin_t *q = makePlugin();
    check(loadChunk(q, blob), "identity: the host chunk loads");
    drain(q);
    check(nameIn(saveJson(q)) == "Squids", "identity: the name round-trips the host chunk");

    const clap_plugin_t *r = makePlugin();
    check(hypersaw_debug_apply(r, named.c_str()), "identity: the saved JSON loads");
    drain(r);
    check(nameIn(saveJson(r)) == "Squids",
          "identity: an UNNAMED load takes the name the patch itself carries");

    // A load is a load: a patch that names no preset clears the last one,
    // rather than leaving a name describing values that are gone.
    check(hypersaw_debug_apply(r, Y.c_str()), "identity: the unnamed patch loads");
    drain(r);
    check(nameIn(saveJson(r)).empty(), "identity: loading an unnamed patch CLEARS the name");

    q->destroy(q);
    r->destroy(r);
  }

  /* 2. NO NAMING WINDOW. PR #703 found that a corner preset's name could be
     lost from history when a GUI frame landed between the load and the naming,
     because setCornerName only amends a mark that is still PENDING. The load
     here is a single call that names as it applies, so the worst-placed pump
     in the world — the one on the very next line — still finds the name. */
  {
    const clap_plugin_t *p = makePlugin();
    hypersaw_debug_apply_named(p, X.c_str(), "Squids");
    undoOp(p, "service");   // a frame at the worst possible moment
    drain(p);
    undoOp(p, "service");
    check(nameIn(undoOp(p, "json", undoInt(p, "current"))) == "Squids",
          "identity: the load's OWN node carries the name (no naming window)");
    p->destroy(p);
  }

  /* 3. THE HUMAN'S SCENARIO, at the DISPLAY level (2026-09-20: "loading a
     different preset on a second branch switched the original branch over to
     the new preset"). B186's gauntlet proved the VALUES were never
     contaminated, so this row asks the only question left: which preset does
     each branch say it is? RED before this change — the name was not in the
     snapshot to begin with. */
  {
    const clap_plugin_t *p = makePlugin();
    hypersaw_debug_apply_named(p, X.c_str(), "Squids");
    drain(p);
    undoOp(p, "service");
    const int nx = undoInt(p, "current");

    mutate(p, 0.371);                       // the player edits on top of X
    undoOp(p, "mark", 1);
    undoOp(p, "service");
    const int edit = undoInt(p, "current");
    check(edit != nx && undoInt(p, "parent", edit) == nx,
          "scenario: the edit is a child of the X node");

    check(undoOp(p, "restore", nx) == "1", "scenario: step back onto X");
    drain(p);
    hypersaw_debug_apply_named(p, Y.c_str(), "Grackle");   // the SECOND branch
    drain(p);
    undoOp(p, "service");
    const int ny = undoInt(p, "current");
    check(ny != edit && undoInt(p, "parent", ny) == nx,
          "scenario: Y is a second branch off X, not a continuation of the edit");
    check(nameIn(saveJson(p)) == "Grackle", "scenario: on the Y branch the patch reports Grackle");

    check(undoOp(p, "restore", edit) == "1", "scenario: the first branch is still reachable");
    drain(p);
    check(nameIn(saveJson(p)) == "Squids",
          "scenario: back on the FIRST branch the patch reports Squids, NOT Grackle");
    p->destroy(p);
  }

  /* 4. THE ASTERISK, with the control that makes its answer mean anything: a
     predicate wired to nothing also reports "clean" forever (L0032). */
  {
    const clap_plugin_t *p = makePlugin();
    hypersaw_debug_apply_named(p, X.c_str(), "Squids");
    drain(p);
    check(hypersaw_debug_presetmatches(p, X.c_str()),
          "dirty: a just-loaded patch MATCHES its preset (no asterisk)");
    check(!hypersaw_debug_presetmatches(p, Y.c_str()),
          "dirty CONTROL: the same instance does NOT match a different preset");
    mutate(p, 0.371);
    check(!hypersaw_debug_presetmatches(p, X.c_str()),
          "dirty: one edit and the patch stops matching (the asterisk appears)");
    hypersaw_debug_apply_named(p, X.c_str(), "Squids");
    drain(p);
    check(hypersaw_debug_presetmatches(p, X.c_str()),
          "dirty: re-loading clears it — dirty cannot get stuck on");
    p->destroy(p);
  }
}


/* ================== layer 3: the history-fidelity gauntlet (B186) =========
   The human, 2026-09-20: "loading a different preset on a second branch
   switched the original branch over to the new preset and made the changes on
   top of it instead of on top of the initial preset."

   Layers 1 and 2 check one node at a time, which is why neither could see
   that report: it is a statement about a PAIR of branches. What follows is
   the regime the human asked for — arbitrary edits, arbitrary step-backs,
   real branching, every node tested, and jumps between parallel branches —
   plus the two things that make a green worth anything: one copy of every
   assertion (so a control calibrates the code that judges the plugin, not a
   second copy of it), and a deliberately broken history to prove each
   assertion can go red at all (L0059: an acceptance criterion that passes on
   the unfixed binary is not a criterion). */

/* ---- the properties, stated once ---- */

/* WHAT A SWEEP MEASURES. Four buckets, because they are four different bugs:
   a restore that hands you the wrong state, a stored snapshot that changed
   under its node, a round trip a->b->a that does not come home, and
   navigation that quietly records a node. The human's report would land in
   the middle two. */
struct FidelityReport
{
  int visited = 0, pairs = 0;
  int nodeMismatch = 0;    // visiting a node did not reproduce what it recorded
  int storedMutated = 0;   // the history's own snapshot no longer matches the record
  int pairMismatch = 0;    // a -> b -> a did not land back on a
  int sizeDrift = 0;       // navigation created or destroyed a node
  std::string firstWhy;
  void note(int &bucket, const std::string &why)
  {
    bucket++;
    if (firstWhy.empty()) firstWhy = why;
  }
  int total() const { return nodeMismatch + storedMutated + pairMismatch + sizeDrift; }
};

/* The end of the value that starts at `v0` — past a balanced [...] or {...},
   or up to the next separator for a scalar. Without the balanced case an array
   value compares as its opening run and two different corner fields look
   equal, which is a diagnostic that lies. */
size_t valueEnd(const std::string &s, size_t v0)
{
  if (v0 >= s.size()) return std::string::npos;
  if (s[v0] == '[' || s[v0] == '{')
  {
    int depth = 0;
    for (size_t i = v0; i < s.size(); i++)
    {
      if (s[i] == '[' || s[i] == '{') depth++;
      else if (s[i] == ']' || s[i] == '}') { if (--depth == 0) return i + 1; }
    }
    return std::string::npos;
  }
  return s.find_first_of(",}", v0);
}

/* The first key where two snapshots disagree, with the value truncated. A
   failure that says "the states differ" costs the next session an hour of
   bisecting; one that names the field costs it a grep. Flat scan of our own
   writer's output, which is the same assumption applyStateJson makes about
   this schema. */
std::string scanDiff(const std::string &want, const std::string &got)
{
  auto brief = [](const std::string &v) {
    return v.size() > 72 ? v.substr(0, 72) + "..." : v;
  };
  size_t i = 0;
  while ((i = want.find('"', i)) != std::string::npos)
  {
    const size_t e = want.find('"', i + 1);
    if (e == std::string::npos) break;
    if (e + 1 >= want.size() || want[e + 1] != ':') { i = e + 1; continue; }
    const std::string key = want.substr(i, e - i + 1);   // quotes included
    const size_t v0 = e + 2;
    // Descend into an OBJECT (`"params":{...}`) so its scalars are compared
    // one by one; an ARRAY is compared whole, because a corner field's
    // interesting failure is "this array changed", not "element 143 did".
    if (v0 < want.size() && want[v0] == '{') { i = v0 + 1; continue; }
    const size_t v1 = valueEnd(want, v0);
    const std::string a = want.substr(v0, v1 - v0);
    std::string b = "(absent)";
    const size_t g = got.find(key);
    if (g != std::string::npos)
    {
      const size_t w0 = g + key.size() + 1, w1 = valueEnd(got, w0);
      b = got.substr(w0, w1 - w0);
    }
    if (a != b) return key + " want " + brief(a) + " got " + brief(b);
    if (v1 == std::string::npos) break;
    i = v1;
  }
  return "";
}

/* Both directions, because the schema has OPTIONAL keys — `modRoutes` and
   `intent` are written only when something has left its default — so "every
   key I have matches yours" does not mean the two states are the same. A
   one-directional diff would have reported "no key differs" on two blobs of
   visibly different lengths, which is a diagnostic that lies by omission. */
std::string firstKeyDiff(const std::string &want, const std::string &got)
{
  const std::string fwd = scanDiff(want, got);
  if (!fwd.empty()) return fwd;
  const std::string rev = scanDiff(got, want);
  if (!rev.empty()) return "EXTRA/CHANGED in the restored state: " + rev;
  return "(no key differs; lengths " + std::to_string(want.size()) + " vs " +
         std::to_string(got.size()) + ")";
}

/* THE SWEEP — the ONE copy of every fidelity assertion. Templated so the
   shell and the deliberately-broken model below are two BACKENDS for it: a
   control that ran a second implementation of these properties would certify
   the second implementation (L0005). A backend supplies live(), size(),
   visit(), liveState(), stored(i) and expected(i); nothing else. */
template <class H>
FidelityReport sweep(H &h, uint32_t seed, size_t pairSample)
{
  FidelityReport r;
  uint32_t rng = seed;
  std::vector<int> ids = h.live();
  const int size0 = h.size();

  /* Shuffled, not sorted: "whichever node you came from" is part of the
     property, so the arrival order must not be the creation order. */
  for (size_t i = ids.size(); i > 1; i--)
  {
    size_t j = (size_t)(forcecore::rngNext(rng) * (double)i);
    if (j >= i) j = i - 1;
    std::swap(ids[i - 1], ids[j]);
  }

  // EVERY NODE: walking to it must produce what it stored.
  for (int i : ids)
  {
    h.visit(i);
    r.visited++;
    if (h.liveState() != h.expected(i))
      r.note(r.nodeMismatch, "node " + std::to_string(i) + " did not restore the state it recorded: " +
                                 firstKeyDiff(h.expected(i), h.liveState()));
    if (h.stored(i) != h.expected(i))
      r.note(r.storedMutated, "node " + std::to_string(i) + "'s own snapshot changed under it");
  }

  /* EVERY ORDERED PAIR, across branches: the reported bug, as a property.
     Bounded to a sample because the cost is quadratic in restores and each
     restore replays the whole parameter table; the sample is drawn from the
     already-shuffled order, so it is not the first N created. */
  std::vector<int> pool = ids;
  if (pool.size() > pairSample) pool.resize(pairSample);
  int prev = ids.empty() ? -1 : ids.back();   // where the node sweep left off
  for (int a : pool)
    for (int b : pool)
    {
      if (a == b) continue;
      r.pairs++;
      /* Each leg is kept, not just its verdict: "the round trip failed" sends
         the next session bisecting, "the SECOND leg failed and here is the
         key" does not. Three 9 KB copies per pair is nothing next to the
         restore each visit already costs. */
      h.visit(a);
      const std::string s1 = h.liveState();
      h.visit(b);
      const std::string s2 = h.liveState();
      h.visit(a);
      const std::string s3 = h.liveState();
      if (s1 != h.expected(a))
        r.note(r.pairMismatch, "arriving at node " + std::to_string(a) + " from " +
                                   std::to_string(prev) + " gave the wrong state: " +
                                   firstKeyDiff(h.expected(a), s1));
      else if (s2 != h.expected(b))
        r.note(r.pairMismatch, "arriving at node " + std::to_string(b) + " from " +
                                   std::to_string(a) + " gave the wrong state: " +
                                   firstKeyDiff(h.expected(b), s2));
      else if (s3 != h.expected(a))
        r.note(r.pairMismatch, "visiting " + std::to_string(b) + " cost node " +
                                   std::to_string(a) + " its state on the way back: " +
                                   firstKeyDiff(h.expected(a), s3));
      prev = a;
      if (h.stored(a) != h.expected(a) || h.stored(b) != h.expected(b))
        r.note(r.storedMutated, "visiting " + std::to_string(b) + " mutated what node " +
                                    std::to_string(a) + " stores");
    }

  if (h.size() != size0) r.note(r.sizeDrift, "navigation changed the node count");
  return r;
}

/* THE RECORDING RULE, as a pure predicate for the same reason: a rule that
   only ever runs against the real plugin cannot be shown to fail. A marked
   edit that MOVED the state is exactly one new node, a child of where the
   player stood, holding the state the instrument now has; a mark that moved
   nothing is no node at all (UndoTree::push's dedup). */
struct MarkObs
{
  int sizeBefore = 0, sizeAfter = 0, curBefore = -1, curAfter = -1, parentOfCur = -1;
  bool stateMoved = false;
  std::string liveJson, storedAtCur;
};
bool markIsFaithful(const MarkObs &o, std::string *why)
{
  auto no = [&](const char *m) {
    if (why) *why = m;
    return false;
  };
  if (o.stateMoved)
  {
    if (o.sizeAfter != o.sizeBefore + 1)
      return no("an edit that moved the state recorded no node, or more than one");
    if (o.curAfter == o.curBefore) return no("the player did not move onto the new node");
    if (o.parentOfCur != o.curBefore) return no("the new node hangs off the wrong parent");
    if (o.storedAtCur != o.liveJson)
      return no("the new node does not hold the state the instrument has");
    return true;
  }
  if (o.sizeAfter != o.sizeBefore) return no("a mark that changed nothing still recorded a node");
  if (o.curAfter != o.curBefore) return no("a mark that changed nothing moved the player");
  return true;
}

/* ---- the controls: a history built to be wrong ---- */

/* Four injected defects, one per bucket, the middle one being the human's
   report stated literally. Branch membership is parity of the node index —
   the model owes nothing to the shell's shape, only to the shape of the
   claim: two branches, and something that crosses between them. */
enum class Bug
{
  None,
  SharedBuffer,       // the two branches share one state buffer
  CrossBranchWrite,   // touching one branch rewrites what the other stores
  NavigationMarks     // merely looking at a node records one
};

struct ModelHistory
{
  Bug bug;
  int n;
  std::vector<std::string> rec, held;
  std::string liveJson;
  int cur = 0, last = -1, extra = 0;

  ModelHistory(Bug b, int nodes) : bug(b), n(nodes)
  {
    for (int i = 0; i < n; i++) rec.push_back(payload(i));
    held = rec;
  }
  int size() const { return n + extra; }
  std::vector<int> live() const
  {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(i);
    return v;
  }
  std::string expected(int i) const { return rec[(size_t)i]; }
  std::string stored(int i) const { return held[(size_t)i]; }
  std::string liveState() const { return liveJson; }
  void visit(int i)
  {
    if (bug == Bug::NavigationMarks) extra++;
    if (bug == Bug::SharedBuffer && last >= 0 && (last & 1) != (i & 1))
      liveJson = held[(size_t)last];   // you get the branch you came from
    else
      liveJson = held[(size_t)i];
    if (bug == Bug::CrossBranchWrite)
      for (int a = 0; a < n; a++)
        if ((a & 1) != (i & 1)) held[(size_t)a] = held[(size_t)i];
    last = cur = i;
  }
};

void gauntletControls()
{
  const uint32_t kControlSeed = 0x5EEDu;
  {
    ModelHistory m(Bug::None, 8);
    const FidelityReport r = sweep(m, kControlSeed, 8);
    check(r.total() == 0 && r.visited == 8 && r.pairs == 56,
          "gauntlet control: the sweep is GREEN on a faithful history (it is not red by construction)");
  }
  {
    ModelHistory m(Bug::SharedBuffer, 8);
    const FidelityReport r = sweep(m, kControlSeed, 8);
    check(r.nodeMismatch > 0 && r.pairMismatch > 0,
          "gauntlet control: the sweep FIRES when two branches share one state buffer");
  }
  {
    ModelHistory m(Bug::CrossBranchWrite, 8);
    const FidelityReport r = sweep(m, kControlSeed, 8);
    check(r.storedMutated > 0,
          "gauntlet control: the sweep FIRES when visiting one branch rewrites what the other "
          "stores (THE REPORTED BUG)");
  }
  {
    ModelHistory m(Bug::NavigationMarks, 8);
    const FidelityReport r = sweep(m, kControlSeed, 8);
    check(r.sizeDrift > 0, "gauntlet control: the sweep FIRES when navigation records a node");
  }

  // ... and the recording rule, four ways of being wrong plus the right one.
  MarkObs good;
  good.sizeBefore = 3;
  good.sizeAfter = 4;
  good.curBefore = 2;
  good.curAfter = 5;
  good.parentOfCur = 2;
  good.stateMoved = true;
  good.liveJson = "X";
  good.storedAtCur = "X";
  std::string why;
  check(markIsFaithful(good, &why), "gauntlet control: the recording rule PASSES a faithful edit");
  MarkObs silent = good;
  silent.sizeAfter = 3;
  silent.curAfter = 2;
  check(!markIsFaithful(silent, &why),
        "gauntlet control: the recording rule FIRES when an edit records nothing");
  MarkObs orphan = good;
  orphan.parentOfCur = 0;
  check(!markIsFaithful(orphan, &why),
        "gauntlet control: the recording rule FIRES on the wrong parent");
  MarkObs lying = good;
  lying.storedAtCur = "Y";
  check(!markIsFaithful(lying, &why),
        "gauntlet control: the recording rule FIRES when the node stores a state the instrument "
        "does not have");
  MarkObs ghost;
  ghost.sizeBefore = 3;
  ghost.sizeAfter = 4;
  ghost.curBefore = 2;
  ghost.curAfter = 2;
  check(!markIsFaithful(ghost, &why),
        "gauntlet control: the recording rule FIRES when a mark that changed nothing still records");
}

/* ---- the shell as a sweep backend ---- */

/* `rec` is the gauntlet's OWN record, taken from the instrument at the moment
   the node was created, and deliberately not read back from the tree. Two
   independent records is the whole point: a tree that quietly rewrote a node
   would still agree with itself. */
struct ShellHistory
{
  const clap_plugin_t *p;
  std::map<int, std::string> rec;

  int size() const { return undoInt(p, "size"); }
  std::vector<int> live() const
  {
    std::vector<int> v;
    for (const auto &kv : rec) v.push_back(kv.first);
    return v;
  }
  std::string expected(int i) const { return rec.at(i); }
  std::string stored(int i) const { return undoOp(p, "json", i); }
  std::string liveState() const { return liveJson(p); }
  void visit(int i)
  {
    undoOp(p, "restore", i);
    drain(p);   // the restore is QUEUED, exactly as the GUI's is
  }
};

/* COMB, THE RACK'S ONE SINGLETON — the gap this gauntlet found on 2026-09-20
   (seed 20268020: "arriving at node 27 from 24 gave the wrong state: fx1type
   want 5 got 1"), CLOSED 2026-09-21 by B188 and now asserted the other way up.

   What it was. COMB is capped at one instance (fx_rack.h — its eight KS lines
   are ONE rack-owned bank, so a second Comb slot doubles every write), and a
   slot's fade-out SHADOW holds its type until the fade ends (B117), which is
   right while both modules render. A load that had to MOVE COMB between slots
   therefore lost it entirely, in both directions: B192 resets every slot to
   Off before applying, which arms the outgoing slot's crossfade, and the
   arriving slot's write is refused against that shadow microseconds later in
   the same drain with nobody to retry once the fade ends. Preset load, DAW
   session reload and history restore all dropped it identically.

   What closed it. `FxRack::claimType` — a claim blocked ONLY by shadows ends
   those fades and proceeds; a claim blocked by a LIVE instance is still
   refused, so the cap is unchanged. The rows below are the gate, and they run
   on an instance that ACTUALLY PROCESSES AUDIO (settleFades), because
   `fadeLeft` decays only inside renderCrossfade — a headless statement about a
   crossfade is a statement about the harness.

   THE CORPUS THAT MADE IT RED (L0059). Not the factory bank: the bank holds no
   relocation, so a bank-only corpus passes on the unfixed binary. The case
   that distinguishes the behaviours is the save/move/reload sequence
   `fxRelocate` performs, and on the unfixed binary it left slot 1 holding type
   0 where the patch said 5. */
constexpr int kCombType = 5;
bool isFxTypeId(clap_id id) { return id >= 57 && id <= 63 && ((id - 57) & 1) == 0; }

/* ---- the preset store, as the gauntlet's two load verbs ---- */

/* The two preset KINDS are told apart by what the file carries, not by which
   folder it sits in: a global names "params", a corner preset names
   "cornerPreset" (ADR-105). Sorted, so the seeded draw is reproducible. */
void collectPresets(const std::string &root, std::vector<std::string> &globals,
                    std::vector<std::string> &corners)
{
  std::error_code ec;
  for (const auto &e : fs::recursive_directory_iterator(root, ec))
  {
    if (!e.is_regular_file()) continue;
    const std::string path = e.path().string();
    if (!isJson(path)) continue;
    std::string blob;
    if (!readFile(path, blob)) continue;
    // B188: the two presets that place COMB (BS - Growl Bass, FX - Comb Throat)
    // used to be held back here, because one COMB that left a slot blocked COMB
    // for ever in an instance that processes no audio. claimType resolves the
    // stale shadow on demand, so the whole bank is in the pool again.
    if (blob.find("\"params\"") != std::string::npos) globals.push_back(path);
    else if (blob.find("\"cornerPreset\"") != std::string::npos) corners.push_back(path);
  }
  std::sort(globals.begin(), globals.end());
  std::sort(corners.begin(), corners.end());
}

// Never the full path: this repo is public and an absolute root would print a
// machine's directory layout into a CI log.
std::string stemOf(const std::string &path) { return fs::path(path).stem().string(); }

/* THE ONE PARAMETER THAT DOES NOT SURVIVE A STATE ROUND TRIP, and it is not
   history's fault. Found by this gauntlet on 2026-09-20 (seed 23856, node 21):
   swarm_core.h:1438 maps the key "tilt" and swarm_core.h:1443 maps "toneTilt"
   to the SAME field `p.tilt`, and the shell mirrors unguarded keys into both
   cores by key (hypersaw_clap.cpp:290 says so in as many words). The two
   parameters declare DIFFERENT ranges — id 45/1045 "tilt" is [0.5, 2], id
   71/1071 "toneTilt" is [-1, 1] — so an osc-2 Amp Tilt above 1 is written to
   stateJson as an out-of-range `o1.toneTilt`, and on load the toneTilt apply
   clamps it to 1 and, aliasing the same field, overwrites the tilt value.
   Every transport loses it identically: preset load, DAW session reload and a
   history restore alike. That is a parameter-routing defect (which engine owns
   `o1.tilt`), not a history one; fixing it is an ADR-sized decision about the
   interface and is deliberately NOT done here.

   So the gauntlet excludes it — and proves the exclusion is still earned every
   run (roundTripGapEvidence below), so the day the alias is fixed that row
   goes RED and whoever fixed it deletes this line. A skip nobody re-checks is
   how a gap becomes permanent. */
constexpr clap_id kAliasGapId = 1045;

/* The parameters an edit may touch. NOT the whole table: id 178 is a GUI
   preference stateJson writes and applyStateJson deliberately skips (ADR-147),
   so it is outside what a restore promises — the same exclusion kMutable
   carries above, for the same reason. Everything else is fair game, at a
   random value inside its own declared range, because "arbitrary parameter
   changes" was the ask. */
struct ParamPick
{
  clap_id id;
  double lo, hi;
  bool fxType = false;   // an integer draw over the WHOLE type list, COMB included
};

/* One draw. An FX type is drawn as an integer over all ten types rather than
   scaled across the range, so the sweep really does visit each module; COMB
   (5) used to be skipped here and is not any more (B188). A refused claim is
   not a hole in the sweep — the gauntlet records what the INSTRUMENT holds
   after the write, so a cap refusal is recorded as the refusal it is. */
double drawValue(const ParamPick &q, double r01)
{
  if (!q.fxType) return q.lo + (q.hi - q.lo) * r01;
  int v = (int)(r01 * 10.0);
  return v > 9 ? 9 : v;
}
std::vector<ParamPick> editablePool(const clap_plugin_t *p)
{
  std::vector<ParamPick> pool;
  auto *pp = paramsOf(p);
  const uint32_t n = pp->count(p);
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!pp->get_info(p, i, &info)) continue;
    if (info.id == 178) continue;             // ADR-147
    if (info.id == kAliasGapId) continue;     // see kAliasGapId
    pool.push_back({info.id, info.min_value, info.max_value, isFxTypeId(info.id)});
  }
  return pool;
}

/* Does `id` survive being written, snapshotted, disturbed and reloaded? One
   parameter, one fresh instance, the GUI's own load door — the smallest
   statement the alias gap can be made in. */
bool roundTrips(clap_id id, double lo, double hi)
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  EvList set;
  set.push(id, lo + (hi - lo) * 0.4372);
  paramsOf(p)->flush(p, &set.list, &kOut);
  drain(p);
  const std::string snap = saveJson(p);
  EvList disturb;
  disturb.push(id, lo + (hi - lo) * 0.8111);
  paramsOf(p)->flush(p, &disturb.list, &kOut);
  drain(p);
  hypersaw_debug_apply(p, snap.c_str());
  drain(p);
  const bool ok = saveJson(p) == snap;
  p->destroy(p);
  return ok;
}

/* THE EXCLUSION, RE-EARNED EVERY RUN. The second row is the must-read-zero
   control: id 45 is the SAME parameter on oscillator 1, where the collision
   is guarded, so a probe that called everything broken would fail it. */
void roundTripGapEvidence()
{
  check(!roundTrips(kAliasGapId, 0.5, 2.0),
        "known gap (NOT history's): id 1045 o1.tilt still loses values above 1 to the toneTilt "
        "alias, so the gauntlet excludes it — WHEN THIS ROW GOES RED THE ALIAS IS FIXED: delete "
        "kAliasGapId");
  check(roundTrips(45, 0.5, 2.0),
        "known gap control: id 45, the same Amp Tilt on oscillator 1, DOES round-trip (the probe "
        "is not calling everything broken)");
}

/* ~230 ms of silence through the real process path, which is the only thing
   that retires an armed FX crossfade (fadeLeft decays in renderCrossfade,
   fx_rack.h:581). Without it a headless instance holds every shadow forever
   and the measurement below would be the harness's, not the product's. */
void settleFades(const clap_plugin_t *p)
{
  std::vector<float> L(256), R(256);
  float *ch[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = ch;
  ob.channel_count = 2;
  EvList none;
  clap_process_t pr{};
  pr.frames_count = 256;
  pr.audio_outputs = &ob;
  pr.audio_outputs_count = 1;
  pr.out_events = &kOut;
  pr.in_events = &none.list;
  p->start_processing(p);
  for (int i = 0; i < 40; i++) p->process(p, &pr);
  p->stop_processing(p);
}

/* Can an FX type be put back where the snapshot had it? Slot `from` gets
   `type`, that state is saved, the type is MOVED to slot `to`, and the saved
   state is loaded again. Returns what slot `from` actually holds afterwards.
   Fades are settled at every step, so this is the DAW's timeline. */
double fxRelocate(int type, int from, int to)
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  const clap_id idFrom = (clap_id)(57 + 2 * from), idTo = (clap_id)(57 + 2 * to);
  EvList a;
  a.push(idFrom, type);
  paramsOf(p)->flush(p, &a.list, &kOut);
  drain(p);
  settleFades(p);
  const std::string snap = saveJson(p);
  EvList clear;
  clear.push(idFrom, 0);
  paramsOf(p)->flush(p, &clear.list, &kOut);
  drain(p);
  settleFades(p);   // the outgoing module is really gone before the new one lands
  EvList set;
  set.push(idTo, type);
  paramsOf(p)->flush(p, &set.list, &kOut);
  drain(p);
  settleFades(p);
  hypersaw_debug_apply(p, snap.c_str());
  drain(p);
  settleFades(p);
  double got = -1;
  paramsOf(p)->get_value(p, idFrom, &got);
  p->deactivate(p);
  p->destroy(p);
  return got;
}

/* Two slots, both live, both asking for `type`. The second write must be
   REFUSED and its slot left Off — this is what makes the gate below a
   statement about a MOVE and not a licence to hold two. */
double fxSecondInstance(int type)
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  EvList a;
  a.push(57, type);
  paramsOf(p)->flush(p, &a.list, &kOut);
  drain(p);
  settleFades(p);   // the first instance is LIVE, with no shadow anywhere
  EvList b;
  b.push(61, type);
  paramsOf(p)->flush(p, &b.list, &kOut);
  drain(p);
  double got = -1;
  paramsOf(p)->get_value(p, 61, &got);
  p->deactivate(p);
  p->destroy(p);
  return got;
}

/* B188 — THE GATE, in place of the exclusion it replaces. Red on the binary
   this change fixes (measured 2026-09-21: slot 1 came back holding type 0),
   green after, with three controls around it so it can be neither a tautology
   nor a licence:
     - the SAME relocation with the uncapped Notch, which round-tripped on the
       unfixed binary too, so the loss was the cap and not the probe;
     - the reverse direction, because the fade shadow and the live holder swap
       places with the slot order;
     - a genuine SECOND instance, which must still be refused — the cap is what
       stops the shared KS bank being written twice, and a "fix" that let this
       through would have broken the audio the cap exists to protect. */
void fxCombGapEvidence()
{
  const double back = fxRelocate(kCombType, 0, 2);
  std::printf("     COMB relocation: slot 1 -> slot 3 -> reload the slot-1 patch leaves slot 1 "
              "holding type %.0f (wanted %d)\n", back, kCombType);
  check(back == (double)kCombType,
        "B188: a state load that must MOVE COMB between slots restores it (the arriving claim "
        "resolves the outgoing slot's fade shadow instead of losing the module)");
  check(fxRelocate(kCombType, 2, 0) == (double)kCombType,
        "B188 (other direction): slot 3 -> slot 1 -> reload the slot-3 patch restores COMB too");
  check(fxSecondInstance(kCombType) == 0.0,
        "B188 control: the cap is NOT weakened — a second LIVE Comb is still refused and its slot "
        "stays Off (one shared KS bank)");
  check(fxSecondInstance(6) == 6.0,
        "B188 control: the uncapped Notch DOES take a second slot (the refusal above is the cap, "
        "not a probe that refuses everything)");
  check(fxRelocate(6, 0, 2) == 6.0,
        "B188 control: the SAME relocation with Notch (uncapped) round-trips exactly — it did on "
        "the unfixed binary too, which is what proved the loss was the cap and not the probe");
}

/* ---- the gauntlet itself ---- */

struct GauntletResult
{
  int nodes = 0;
  int markBad = 0, navBad = 0, navSizeBad = 0;
  std::string firstMarkWhy, firstNavWhy;
  FidelityReport fid;
};

GauntletResult historyGauntlet(uint32_t seed, int steps, const std::vector<std::string> &globals,
                               const std::vector<std::string> &corners, size_t pairSample)
{
  GauntletResult g;
  uint32_t rng = seed;
  const clap_plugin_t *p = makePlugin();
  // Activate first: morphInit runs there, so every snapshot from here on
  // carries the corner field and the corner-load steps below are visible in it.
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  undoOp(p, "service");   // the editor opening: the root

  ShellHistory h{p, {}};
  h.rec[undoInt(p, "current")] = liveJson(p);
  const std::vector<ParamPick> pool = editablePool(p);

  auto pick = [&](size_t n) { return (size_t)(forcecore::rngNext(rng) * (double)n); };

  // ONE recording assertion, applied to every step that marks.
  auto closeMark = [&](MarkObs &o, const std::string &was, int step) {
    drain(p);
    undoOp(p, "service");
    o.liveJson = liveJson(p);
    o.stateMoved = o.liveJson != was;
    o.sizeAfter = undoInt(p, "size");
    o.curAfter = undoInt(p, "current");
    o.parentOfCur = undoInt(p, "parent", o.curAfter);
    o.storedAtCur = undoOp(p, "json", o.curAfter);
    std::string why;
    if (!markIsFaithful(o, &why))
    {
      g.markBad++;
      if (g.firstMarkWhy.empty()) g.firstMarkWhy = "step " + std::to_string(step) + ": " + why;
      return;
    }
    if (o.stateMoved)
    {
      h.rec[o.curAfter] = o.liveJson;
      g.nodes++;
    }
  };

  // ONE navigation assertion, likewise.
  auto closeNav = [&](int sizeBefore, int step, const std::string &what) {
    drain(p);
    const int c = undoInt(p, "current");
    if (h.rec.find(c) == h.rec.end() || liveJson(p) != h.rec[c])
    {
      g.navBad++;
      if (g.firstNavWhy.empty())
        g.firstNavWhy = "step " + std::to_string(step) + ": " + what + " did not land on node " +
                        std::to_string(c) + "'s recorded state";
    }
    if (undoInt(p, "size") != sizeBefore)
    {
      g.navSizeBad++;
      if (g.firstNavWhy.empty())
        g.firstNavWhy = "step " + std::to_string(step) + ": " + what + " created or lost a node";
    }
  };

  for (int s = 0; s < steps; s++)
  {
    const double roll = forcecore::rngNext(rng);
    MarkObs o;
    o.sizeBefore = undoInt(p, "size");
    o.curBefore = undoInt(p, "current");
    const std::string was = liveJson(p);

    if (roll < 0.36)
    {
      /* an arbitrary edit: 1-4 parameters, anywhere in their range. The VALUE
         arrives through the host flush (which marks nothing, ADR-160 (3)); the
         MARK is the same door automationControl above calibrates. */
      const int k = 1 + (int)(forcecore::rngNext(rng) * 4.0);
      EvList ev;
      for (int i = 0; i < k; i++)
      {
        const ParamPick &q = pool[pick(pool.size())];
        ev.push(q.id, drawValue(q, forcecore::rngNext(rng)));
      }
      paramsOf(p)->flush(p, &ev.list, &kOut);
      undoOp(p, "mark", s);
      closeMark(o, was, s);
    }
    else if (roll < 0.52 && !globals.empty())
    {
      std::string blob;
      readFile(globals[pick(globals.size())], blob);
      hypersaw_debug_apply(p, blob.c_str());   // the GUI's load button
      closeMark(o, was, s);
    }
    else if (roll < 0.66 && !corners.empty())
    {
      const int corner = (int)pick(4);
      const std::string &file = corners[pick(corners.size())];
      std::string blob;
      readFile(file, blob);
      hypersaw_debug_cornerapply(p, corner, blob.c_str());
      // the GUI names the corner immediately after the load (B122), and the
      // name is shell state that rides the morph chunk into the snapshot.
      hypersaw_debug_cornername(p, corner, stemOf(file).c_str());
      closeMark(o, was, s);
    }
    else if (roll < 0.86)
    {
      /* step back to an ARBITRARY point — not necessarily an ancestor. The
         next edit forks from wherever this lands, which is what gives the
         tree breadth rather than one long line. */
      const std::vector<int> ids = h.live();
      const int t = ids[pick(ids.size())];
      undoOp(p, "restore", t);
      closeNav(o.sizeBefore, s, "restore to " + std::to_string(t));
    }
    else
    {
      const bool fwd = forcecore::rngNext(rng) < 0.5;
      undoOp(p, fwd ? "redo" : "undo");
      closeNav(o.sizeBefore, s, fwd ? "redo" : "undo");
    }
  }

  /* The regime stays under the ring's cap ON PURPOSE: a recycled slot would
     make `rec` address a different node than the tree does, and eviction is
     layer 1's subject, tested there against the structure directly. */
  g.fid = sweep(h, seed ^ 0x9E3779B9u, pairSample);
  p->destroy(p);
  return g;
}

void gauntletChecks(uint32_t seed, const std::vector<std::string> &globals,
                    const std::vector<std::string> &corners)
{
  check(globals.size() >= 2,
        "gauntlet: the preset store offers at least two distinct global presets");
  check(!corners.empty(), "gauntlet: the preset store offers at least one corner preset");
  // B188 retired the "held back" count: no preset is dropped from the pool any
  // more, so the whole bank is loaded and the number printed is the whole bank.
  std::printf("     gauntlet: seed %u (argv[3] overrides), %zu global + %zu corner presets\n",
              (unsigned)seed, globals.size(), corners.size());

  /* Four seeds, one regime. Determinism is per-seed; the spread is there
     because one random walk is one shape of tree, and the property is about
     all of them. */
  for (int run = 0; run < 4; run++)
  {
    const uint32_t s = seed + (uint32_t)run * 7919u;
    const GauntletResult g = historyGauntlet(s, 48, globals, corners, 12);
    const std::string tag = "gauntlet[seed " + std::to_string(s) + "]";
    std::printf("     %s: %d nodes, %d visits, %d ordered pairs\n", tag.c_str(), g.nodes,
                g.fid.visited, g.fid.pairs);
    check(g.nodes >= 10, tag + ": the walk built a tree worth testing (>= 10 nodes)");
    check(g.markBad == 0,
          tag + ": every edit, global load and corner load recorded EXACTLY the node it owed" +
              (g.markBad ? " [" + g.firstMarkWhy + "]" : ""));
    check(g.navBad == 0 && g.navSizeBad == 0,
          tag + ": every step-back, undo and redo landed on that node's recorded state, and "
                "recorded nothing" +
              (g.navBad || g.navSizeBad ? " [" + g.firstNavWhy + "]" : ""));
    check(g.fid.nodeMismatch == 0,
          tag + ": EVERY node restores byte-identically, from wherever you came from" +
              (g.fid.nodeMismatch ? " [" + g.fid.firstWhy + "]" : ""));
    check(g.fid.storedMutated == 0,
          tag + ": visiting one branch never mutated what another branch stores" +
              (g.fid.storedMutated ? " [" + g.fid.firstWhy + "]" : ""));
    check(g.fid.pairMismatch == 0,
          tag + ": a -> b -> a comes home to a's exact state, for every ordered pair" +
              (g.fid.pairMismatch ? " [" + g.fid.firstWhy + "]" : ""));
    check(g.fid.sizeDrift == 0, tag + ": the whole sweep created no node");
  }
}

/* ============ layer 4: every control marks the history (B191) ===========
   The human, 2026-09-21: "I changed the shape value and it didn't make a node."
   The cause was in the GUI — a <select>'s bracket was opened from `pointerdown`
   and closed from a `pointerup` a native popup swallows — but the PROPERTY the
   player relies on spans both sides of the bridge and so does its gate:

     (1) the control emits exactly one BALANCED bracket around its value change,
         whatever the input modality — tools/labharness/gui_history_check.mjs,
         which executes gui2's own wiring;
     (2) one closed bracket on that id produces exactly one node — here, driven
         through the shell's real gesture verb for EVERY declared parameter.

   Neither half is the property alone. This half is TOTAL over the parameter
   table (editablePool, the same pool and the same two exclusions layer 3 uses),
   so a parameter added tomorrow is covered on its first run rather than when
   somebody remembers.

   WHY A VALUE IS WRITTEN FIRST. UndoTree::push drops a snapshot identical to
   the one under foot — a gesture that ended where it began is not an edit — so
   a bracket with no value change correctly makes no node, and asserting "+1"
   without moving the parameter would be asserting the wrong thing. The value
   is driven to whichever END of the declared range is further from where the
   parameter already sits, so it is guaranteed to move for any real range. */
void controlMarkChecks()
{
  const clap_plugin_t *p0 = makePlugin();
  const std::vector<ParamPick> pool = editablePool(p0);
  p0->destroy(p0);
  check(pool.size() > 100, "layer 4: the parameter pool is the whole table (" +
                               std::to_string(pool.size()) + " params)");

  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  undoOp(p, "service");

  int unmarked = 0, covered = 0;
  clap_id firstBad = 0;
  std::vector<clap_id> inert;          // wrote a live value, left no trace in the snapshot
  for (const ParamPick &q : pool)
  {
    /* The ring is 200 slots and the pool is larger, so a wrapped tree stops
       reporting growth in `size`. A fresh instance every 150 parameters keeps
       the observable honest without paying for one per parameter. */
    if (undoInt(p, "size") >= 150)
    {
      p->stop_processing(p);
      p->deactivate(p);
      p->destroy(p);
      p = makePlugin();
      p->activate(p, kSampleRate, 32, 1024);
      drain(p);
      undoOp(p, "service");
    }

    double cur = 0;
    paramsOf(p)->get_value(p, q.id, &cur);
    const double v = (cur - q.lo) > (q.hi - cur) ? q.lo : q.hi;

    const std::string before = liveJson(p);
    EvList ev;
    ev.push(q.id, v);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    if (liveJson(p) == before)
    {
      // The write left no trace in the snapshot, so a node for it would carry
      // nothing and this parameter cannot say anything about history. Recorded
      // and pinned below (L0033: a boundary is recorded, never silently
      // skipped), never just skipped.
      inert.push_back(q.id);
      continue;
    }

    const int was = undoInt(p, "size");
    hypersaw_debug_gesture(p, q.id, true);
    hypersaw_debug_gesture(p, q.id, false);
    /* The gesture verb ENQUEUES its bracket for the audio thread, and
       undoService refuses to snapshot a queue it has not seen drained. Without
       this the size never moves and every row below would pass or fail for the
       harness's reason instead of the product's. */
    drain(p);
    undoOp(p, "service");
    covered++;
    if (undoInt(p, "size") != was + 1)
    {
      if (!unmarked) firstBad = q.id;
      unmarked++;
    }
  }
  check(unmarked == 0, "layer 4: EVERY parameter's closed bracket makes exactly one node (" +
                           std::to_string(covered) + " of " + std::to_string(pool.size()) +
                           " params, " + std::to_string(unmarked) + " unmarked" +
                           (unmarked ? ", first id " + std::to_string(firstBad) : "") + ")");

  /* THE COVERAGE BOUNDARY, PINNED AND RE-EARNED EVERY RUN — the same discipline
     as kAliasGapId, and found the same way (by this check, on its first run).
     ONE parameter accepts a value and leaves the history snapshot unchanged:

       * id 1043. Base id 43 is dispatched by RAW id to a shared object
         (gui_reach's patch-scope derivation), so the oscillator-2 twin is
         declared but reaches nothing: the value written above reads back
         UNCHANGED, which no other parameter in the table does.

     RETIRED 2026-09-23 (B222), by its own instruction: a second family, EVERY
     ROUTING ID, used to sit here — "ALL 29 routing ids are absent from
     stateJson, so the routing matrix is outside undo/redo ... WHEN THIS ROW
     GOES RED ROUTING HAS ENTERED THE STATE: delete this boundary and let the
     rows above cover it". It went red when historyJson began carrying the
     matrix, so the 29 routing ids are now inside `covered` above and must each
     make their node like any other parameter. What remains is STRICTER than
     what stood: the inert set must be exactly {1043}, so a routing id that
     fell out of the snapshot again would be red here, not re-pinned.

     Pinned as a set, not a count, so a new hole in either direction is red. */
  check(inert.size() == 1 && inert[0] == 1043,
        "layer 4 boundary: id 1043 is the ONLY parameter whose write leaves the history snapshot "
        "unchanged (base 43 is patch-scope, so the osc-2 twin reaches nothing; the routing matrix "
        "has been inside the snapshot since B222) — " + std::to_string(inert.size()) + " found");
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

/* THE HUMAN'S BUG, AT THE SHELL, with its calibration (L0032). An OPEN bracket
   is not an edit: the node is made where the bracket CLOSES, so a control whose
   release never arrives writes the parameter and leaves the history untouched.
   The zero below is the defect the GUI had; the one after it is what makes the
   zero evidence rather than a counter wired to nothing. */
void unclosedBracketControl()
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  undoOp(p, "service");
  const int base = undoInt(p, "size");
  check(base == 1, "unclosed: one root node before anything is touched");

  const clap_id id = 4000;   // sub.wave — the control the human reported
  EvList ev;
  ev.push(id, 5);
  paramsOf(p)->flush(p, &ev.list, &kOut);
  drain(p);
  hypersaw_debug_gesture(p, id, true);          // ... and the popup eats the release
  drain(p);                                     // see controlMarkChecks: an undrained
  undoOp(p, "service");                         // queue would make this zero for free
  check(undoInt(p, "size") == base,
        "unclosed CONTROL: a bracket that opens and never closes makes ZERO nodes "
        "(the reported bug: the sub's Wave changed, the history did not)");

  hypersaw_debug_gesture(p, id, false);         // the same bracket, now closed
  drain(p);
  undoOp(p, "service");
  check(undoInt(p, "size") == base + 1,
        "unclosed CALIBRATION: closing that SAME bracket makes exactly one node");

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

/* ---- the reported scenario, run literally ---- */

/* load X, edit, step back, load Y on the fork, return to the first branch.
   TWO different failures live here and the player can only see one of them:
   what the STATE holds, and what the INTERFACE says is loaded. This gates the
   first and PRINTS the second, because they are not the same bug and B174
   (the global preset's name is not shell state at all) owns the second. */
void reportedScenario(const std::vector<std::string> &globals)
{
  if (globals.size() < 2)
  {
    check(false, "scenario: needs two global presets");
    return;
  }

  /* Two presets that genuinely differ. A scenario about "switched to the
     other preset" is vacuous if the two states are equal — the must-read-zero
     control for the whole section. */
  std::string blobX, blobY, nameX, nameY;
  {
    const clap_plugin_t *probe = makePlugin();
    probe->activate(probe, kSampleRate, 32, 1024);
    std::string first;
    for (const std::string &f : globals)
    {
      std::string blob;
      if (!readFile(f, blob)) continue;
      hypersaw_debug_apply(probe, blob.c_str());
      drain(probe);
      const std::string st = saveJson(probe);
      if (blobX.empty())
      {
        blobX = blob;
        nameX = stemOf(f);
        first = st;
        continue;
      }
      if (st != first)
      {
        blobY = blob;
        nameY = stemOf(f);
        break;
      }
    }
    probe->destroy(probe);
  }
  check(!blobX.empty() && !blobY.empty(),
        "scenario control: two global presets that load to DIFFERENT states");
  if (blobY.empty()) return;

  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  undoOp(p, "service");

  // 1. load X
  hypersaw_debug_apply(p, blobX.c_str());
  drain(p);
  undoOp(p, "service");
  const int nX = undoInt(p, "current");
  const std::string recX = liveJson(p);

  // 2. edit on top of X
  {
    EvList ev;
    for (clap_id id : kMutable) ev.push(id, 0.617);
    paramsOf(p)->flush(p, &ev.list, &kOut);
  }
  undoOp(p, "mark", 1);
  undoOp(p, "service");
  const int nXEdit = undoInt(p, "current");
  const std::string recXEdit = liveJson(p);
  check(nXEdit != nX && undoInt(p, "parent", nXEdit) == nX,
        "scenario: the edit is a child of the node preset X made");

  // 3. step back to X, 4. load Y there — the fork
  undoOp(p, "restore", nX);
  drain(p);
  check(liveJson(p) == recX, "scenario: stepping back to X restores X");
  hypersaw_debug_apply(p, blobY.c_str());
  drain(p);
  undoOp(p, "service");
  const int nY = undoInt(p, "current");
  const std::string recY = liveJson(p);
  check(undoInt(p, "parent", nY) == nX, "scenario: loading Y on the second branch FORKS from X");
  check(recY != recX, "scenario control: Y's state really differs from X's");

  // THE CLAIM, in the state. Nothing on the first branch may have moved.
  check(undoOp(p, "json", nX) == recX, "scenario: loading Y did NOT rewrite what the X node stores");
  check(undoOp(p, "json", nXEdit) == recXEdit,
        "scenario: loading Y did NOT rewrite what the edit-on-X node stores");

  // 5. return to the first branch
  undoOp(p, "restore", nXEdit);
  drain(p);
  check(liveJson(p) == recXEdit,
        "scenario: returning to the first branch lands on the EDIT ON TOP OF X, byte for byte");
  check(liveJson(p) != recY, "scenario control: that landing is not Y's state wearing X's label");

  /* 6. and a further edit there sits on X, not on Y — which is exactly the
        state assertion above plus the recording rule, so it is stated as one. */
  {
    EvList ev;
    for (clap_id id : kMutable) ev.push(id, 0.213);
    paramsOf(p)->flush(p, &ev.list, &kOut);
  }
  const std::string before = liveJson(p);
  undoOp(p, "mark", 2);
  undoOp(p, "service");
  const int nX3 = undoInt(p, "current");
  check(undoInt(p, "parent", nX3) == nXEdit && undoOp(p, "json", nX3) == liveJson(p),
        "scenario: the further edit is recorded on the first branch, holding what the instrument has");
  check(before != recY, "scenario control: the further edit was made on top of X's branch, not Y's");

  /* THE DISPLAY HALF, printed rather than gated. Corner preset NAMES are
     shell state (B122) and ride the morph chunk, so history restores them —
     which the byte-identity rows above already assert. The GLOBAL preset's
     name is not in the shell at all (B174, specced and unbuilt), so no
     snapshot can carry it: after this sequence the instrument's STATE is
     X-plus-two-edits while the editor's own idea of "loaded preset" is still
     whatever it last set, which is Y. A player reading the header sees the
     reported bug; the parameters under it are correct. That gap is B174's,
     not history's. */
  const bool nameInState =
      recX.find(nameX) != std::string::npos || recY.find(nameY) != std::string::npos;
  std::printf("     scenario: X=%s Y=%s - the STATE is history's and restores exactly;\n"
              "     the global preset NAME is in no snapshot (found in state: %s) - B174, not history.\n"
              "     corner names ARE state and travel with it: %s\n",
              nameX.c_str(), nameY.c_str(), nameInState ? "yes" : "no",
              hypersaw_debug_cornernames(p));

  p->destroy(p);
}


/* ---- the other half of "references to loaded presets" (B186) ----

   The human's report opened with "It has to do with references to loaded
   presets". The GLOBAL preset's name is not shell state at all (B174), so
   history cannot lose it — there is nothing to lose. The CORNER preset's name
   IS shell state (B122): it rides the morph chunk into every snapshot, and
   the GUI sets it in a SECOND webview round trip right after the load
   (gui2.html ~2956 — `await hzMorphCornerApply` then `await hzMorphCornerName`).
   Between those two awaits the browser's event loop is free to run a frame,
   and hzFrame services the pending mark FIRST thing (hypersaw_gui_common.h:487).

   So this is the load sequence with the frame landing in the gap — not an
   exotic interleaving, just the one an `await` permits. */
void cornerReferenceScenario(const std::vector<std::string> &corners)
{
  if (corners.empty())
  {
    check(false, "corner reference: needs a corner preset");
    return;
  }
  std::string blob;
  if (!readFile(corners[0], blob))
  {
    check(false, "corner reference: corner preset reads");
    return;
  }
  const std::string name = stemOf(corners[0]);

  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  drain(p);
  undoOp(p, "service");
  const int root = undoInt(p, "current");
  check(undoOp(p, "json", root).find("\"" + name + "\"") == std::string::npos,
        "corner reference control: the root node names no corner preset (the detector reads zero "
        "when there is nothing to find)");

  hypersaw_debug_cornerapply(p, 1, blob.c_str());
  drain(p);
  undoOp(p, "service");   // THE FRAME, landing between the two awaits
  hypersaw_debug_cornername(p, 1, name.c_str());
  drain(p);
  undoOp(p, "service");

  const int node = undoInt(p, "current");
  check(undoOp(p, "json", node) == liveJson(p),
        "corner reference: after naming the corner the instrument stands on a node that HOLDS its "
        "state");
  check(undoOp(p, "json", node).find("\"" + name + "\"") != std::string::npos,
        "corner reference: the node the player stands on records WHICH preset the corner came from");

  // and the consequence the player sees: navigate away and back.
  undoOp(p, "restore", root);
  drain(p);
  undoOp(p, "restore", node);
  drain(p);
  check(std::string(hypersaw_debug_cornernames(p)).find("\"" + name + "\"") != std::string::npos,
        "corner reference: the corner's preset name survives a round trip through history");
  check(liveJson(p) == undoOp(p, "json", node),
        "corner reference: that round trip is byte-identical");

  p->destroy(p);
}

/* ============ layer 5: history fidelity, round 3 (B222) ====================
   The human, 2026-09-23: "'morph on' doesn't seem to get a history entry on
   its own. Also I think turning on morph reverts the routing matrix to its
   initial state ... Sometimes when you go back to a past node and then revert
   to the future node, the future node sounds different depending on the past
   node you visited."

   Three defects, one section, because two of them are the same seam seen from
   two sides: the morph field writes EVERY morphable parameter, the routing
   matrix included, and the routing matrix was in no history snapshot (B193).

   WHY THESE ROWS RUN AN INSTANCE THAT PROCESSES AUDIO. The morph field acts
   only inside process() (morphStep, on the 256-sample grid), and every layer
   above drives an instance that never calls it — which is exactly how the
   gauntlet walked 120 seeds green past a defect the human heard in a minute.
   A statement about the field made by an instance whose field never ticks is
   a statement about the harness (the same lesson as settleFades above). */

// The routing ids from the shell's own enumeration, never re-derived here
// (the id-layout comment asks nobody to make a second copy of decodeRoutingId).
std::vector<clap_id> routingIds()
{
  std::vector<clap_id> v;
  const std::string s = hypersaw_debug_routing_ids();
  for (size_t pos = 0; pos < s.size();)
  {
    v.push_back((clap_id)std::strtoul(s.c_str() + pos, nullptr, 10));
    pos = s.find(';', pos);
    if (pos == std::string::npos) break;
    pos++;
  }
  return v;
}
double valueOf(const clap_plugin_t *p, clap_id id)
{
  double v = 0;
  paramsOf(p)->get_value(p, id, &v);
  return v;
}
/* THE MATRIX AS THE ENGINE REPORTS IT, through CLAP get_value — deliberately
   NOT through any JSON writer. A row that compared history's snapshot against
   history's own writer would agree with itself about whatever that writer
   leaves out (L0032: the detector must not share the assumption); this
   readout is how the routing rows below can be red on a build whose snapshot
   has no routing in it at all. */
std::string routingReadout(const clap_plugin_t *p, const std::vector<clap_id> &ids)
{
  std::string out;
  char b[48];
  for (clap_id id : ids)
  {
    std::snprintf(b, sizeof b, "%u:%.17g,", (unsigned)id, valueOf(p, id));
    out += b;
  }
  return out;
}

// A processing instance's clock: n blocks of 256, `ev` delivered in the first.
struct Blocks
{
  const clap_plugin_t *p;
  std::vector<float> L = std::vector<float>(256), R = std::vector<float>(256);
  float *ch[2];
  clap_audio_buffer_t ob{};
  clap_process_t pr{};
  explicit Blocks(const clap_plugin_t *pp) : p(pp)
  {
    ob.channel_count = 2;
    pr.frames_count = 256;
    pr.audio_outputs_count = 1;
    pr.out_events = &kOut;
  }
  void run(int n, EvList *ev = nullptr)
  {
    // Re-aimed every call, and the empty list is local (EvList points at
    // itself): the struct is returned by value, and a copy's pointers would
    // otherwise still aim at the original's buffers.
    EvList none;
    ch[0] = L.data();
    ch[1] = R.data();
    ob.data32 = ch;
    pr.audio_outputs = &ob;
    for (int i = 0; i < n; i++)
    {
      pr.in_events = (i == 0 && ev) ? &ev->list : &none.list;
      p->process(p, &pr);
    }
  }
};

/* The editor's morph checkbox, in the order gui2's bracket emits it (the
   GATE:BRACKET block; gui_history_check pins that order): begin, value, end.
   The value goes through the bridge's own setParam body (Plugin::guiSetParam),
   and `blocks` lets the audio thread drain the whole bracket. */
void editorMorph(const clap_plugin_t *p, Blocks &b, int on, int blocks)
{
  hypersaw_debug_gesture(p, 151, true);
  undoOp(p, "setmorph", on);
  hypersaw_debug_gesture(p, 151, false);
  b.run(blocks);
}

/* ---- defect 1: the toggle's node ----
   The shell has always made ONE node per toggle. What it did not do is NAME
   it: guiSetParam marks "morph on"/"morph off", and the bracket's END, which
   gui2 has emitted AFTER the value since B191 made every control bracket its
   own value change, re-marked it with the parameter's display name, "Morph".
   The history rail coalesces a chain of same-label nodes into one row
   (gui2 histRowsOf), so on-then-off drew as ONE row, "Morph ×2" — the ON had
   no entry of its own. The comment that stood at the setParam seam said the
   opposite ("the checkbox's own pointerup already marked"), which was true of
   the pre-B191 order and has been false since. */
void morphToggleChecks()
{
  const clap_plugin_t *p = makePlugin();
  p->activate(p, kSampleRate, 32, 1024);
  p->start_processing(p);
  Blocks b(p);
  b.run(2);
  undoOp(p, "service");

  const char *want[4] = {"morph on", "morph off", "morph on", "morph off"};
  bool eachOne = true, named = true;
  std::string got;
  for (int k = 0; k < 4; k++)
  {
    const int was = undoInt(p, "size");
    editorMorph(p, b, k % 2 == 0 ? 1 : 0, 2);
    undoOp(p, "service");
    if (undoInt(p, "size") != was + 1) eachOne = false;
    const std::string lb = undoOp(p, "label", undoInt(p, "current"));
    if (lb != want[k]) named = false;
    got += (k ? " / " : "") + lb;
  }
  check(eachOne, "morph toggle: on, off, on, off from the editor make exactly ONE node each");
  check(named, "morph toggle: each node is named for the direction it went, so the rail cannot "
               "coalesce an ON into an OFF (got: " + got + ")");

  /* THE CONTROL, ADR-160 (3): the SAME parameter moved by the host makes no
     node. The four +1s above are its calibration — this instance can count. */
  const int was = undoInt(p, "size");
  EvList ev;
  ev.push(151, 1);
  b.run(2, &ev);
  undoOp(p, "service");
  check(undoInt(p, "size") == was && valueOf(p, 151) > 0.5,
        "morph toggle CONTROL: the host switching Morph on moves the parameter and makes ZERO nodes");

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

/* ---- defect 2: morph-on destroys the patch ----
   CAUSE, measured before the fix (the rows below, red): switching morph on
   AFTER ANY STATE LOAD reverted every morphable edit made since that load —
   the routing matrix, which is what the human saw, AND ordinary parameters
   (detune here), which nobody had reported. Not stale ROUTING corners in
   particular: stale corners, full stop.

   The mechanism. Morph-on already had a non-destructive rule — "if the corners
   are still the seed, adopt the live patch into all four" — keyed on
   `morphCornersAuthored`. Every state load that carries a morph chunk sets
   that flag, and every stateJson carries one: the Init patch, any factory
   preset, every history restore. So after any load the corners counted as
   authored even when all four were identical, the adoption was skipped, and
   the field wrote the load-time values over everything edited since. The
   lead's hypothesis (routing rides the field, the corners predate the edit)
   is CONFIRMED as to mechanism and REFUTED as to scope: it is not routing.

   THE RULE NOW (applyParam, case 151). On the EDITOR's off -> on:
     * a morph GROUP whose four corners hold the SAME values (the field has no
       opinion about it — it would pin one value wherever the puck sits) adopts
       the LIVE values into all four: morph-on changes nothing you can hear;
     * a group whose corners DIFFER is the field's: it plays the owning
       corner's values (the whole routing block as one unit, ADR-176 §3). Your
       live edits to it are not kept, because keeping them would mean writing
       over a corner you authored — capture them into a corner first.
   Host automation of the same parameter keeps its pre-B222 behaviour, so an
   existing session plays back exactly as it did; that boundary is pinned. */
void morphOnKeepsPatchChecks()
{
  const std::vector<clap_id> rids = routingIds();
  check(!rids.empty(), "morph-on: the routing matrix declares its cells");
  const clap_id kCell = rids.front();   // Src 1 -> Slot 1: the serial chain's first link
  const clap_id kDetune = 4, kWidth = 14;

  enum Prep { Fresh, AfterLoad, AfterRestore };
  struct Inst
  {
    const clap_plugin_t *p;
    Blocks b;
  };
  auto open = [&](Prep prep) {
    const clap_plugin_t *p = makePlugin();
    p->activate(p, kSampleRate, 32, 1024);
    p->start_processing(p);
    Inst in{p, Blocks(p)};
    in.b.run(2);
    undoOp(p, "service");
    if (prep == AfterLoad)
    {
      // Its OWN state through the preset door: nothing about the patch
      // changes, only that a load happened — which is all the defect needs.
      const std::string self = saveJson(p);
      hypersaw_debug_apply(p, self.c_str());
      in.b.run(2);
      undoOp(p, "service");
    }
    if (prep == AfterRestore)
    {
      const int root = undoInt(p, "current");
      EvList ev;
      ev.push(kWidth, 1.1);
      in.b.run(2, &ev);
      undoOp(p, "mark", 1);
      undoOp(p, "service");
      undoOp(p, "restore", root);
      in.b.run(2);
    }
    return in;
  };
  auto close = [](Inst &in) {
    in.p->stop_processing(in.p);
    in.p->deactivate(in.p);
    in.p->destroy(in.p);
  };
  // The player's work: a routing cell and a parameter, edited with morph off.
  auto edit = [&](Inst &in, double cell, double detune) {
    EvList ev;
    ev.push(kCell, cell);
    ev.push(kDetune, detune);
    in.b.run(4, &ev);
  };

  const char *names[3] = {"on a fresh instance", "after a preset load", "after a history restore"};
  for (int prep = Fresh; prep <= AfterRestore; prep++)
  {
    Inst in = open((Prep)prep);
    const double cell0 = valueOf(in.p, kCell);
    edit(in, 0.25, 0.777);
    const bool moved = valueOf(in.p, kCell) == 0.25 && valueOf(in.p, kDetune) == 0.777 && cell0 != 0.25;
    editorMorph(in.p, in.b, 1, 40);
    check(moved && valueOf(in.p, kCell) == 0.25 && valueOf(in.p, kDetune) == 0.777,
          std::string("morph-on ") + names[prep] + ": the routing edit AND the parameter edit "
          "survive switching morph on (cell " + std::to_string(valueOf(in.p, kCell)) +
          ", detune " + std::to_string(valueOf(in.p, kDetune)) + ")");
    close(in);
  }

  /* MUST READ ZERO: the same patch, the same edits, 40 blocks — and NO toggle.
     Nothing else in the run may move these values, or the rows above would be
     measuring something other than the toggle. */
  {
    Inst in = open(AfterLoad);
    edit(in, 0.25, 0.777);
    in.b.run(40);
    check(valueOf(in.p, kCell) == 0.25 && valueOf(in.p, kDetune) == 0.777,
          "morph-on CONTROL: without the toggle nothing moves the edited values (the detector reads zero)");
    close(in);
  }

  /* MUST FIRE, and the stated rule for a patch whose corners DO differ: corner
     A holds the cell at 0.25, corner B at 1.5, the puck sits on B. Edit the
     cell to 0.6 and width to 1.2 with morph off, then switch it on. The
     routing block is the field's (A and B disagree), so it plays B's 1.5 —
     the detector CAN see morph-on move a value. Width was never captured
     apart — all four corners agree on it — so the edit is adopted and kept. */
  {
    Inst in = open(AfterLoad);
    edit(in, 0.25, 0.28);
    hypersaw_debug_capture(in.p, 0);
    edit(in, 1.5, 0.28);
    hypersaw_debug_capture(in.p, 1);
    EvList ev;
    ev.push(152, 1.0);   // morph X: B is top-right (corner order A B / C D)
    ev.push(153, 0.0);
    ev.push(kCell, 0.6);
    ev.push(kWidth, 1.2);
    in.b.run(4, &ev);
    editorMorph(in.p, in.b, 1, 40);
    check(valueOf(in.p, kCell) == 1.5,
          "morph-on RULE, corners that DIFFER: the routing block plays the owning corner (B = 1.5, got " +
              std::to_string(valueOf(in.p, kCell)) + ") — and this is the must-fire: the detector "
              "above can see morph-on move a value");
    check(valueOf(in.p, kWidth) == 1.2,
          "morph-on RULE, corners that AGREE: in the same patch, width (never captured apart) keeps "
          "the live edit (got " + std::to_string(valueOf(in.p, kWidth)) + ")");
    close(in);
  }

  /* BOUNDARY, PINNED (L0036): the HOST switching morph on keeps the old
     behaviour — after a load, a live edit is handed back to the corners. That
     is what every saved session automating Morph has always done, and changing
     how an existing session sounds is the human's call, not this change's. If
     this row goes red, that call was made; say so in the PR. */
  {
    Inst in = open(AfterLoad);
    const double cell0 = valueOf(in.p, kCell);
    edit(in, 0.25, 0.777);
    EvList ev;
    ev.push(151, 1);
    in.b.run(40, &ev);
    check(valueOf(in.p, kCell) == cell0,
          "morph-on BOUNDARY: HOST automation of Morph after a load still plays the corners "
          "(existing sessions unchanged; the editor rule above is the editor's)");
    close(in);
  }
}

/* ---- defect 3: a restore that depends on the road taken ----
   THE GAUNTLET PROPERTY, with the independent oracles the earlier layers did
   not have: for every node N, arriving from several different nodes must land
   on (a) N's recorded snapshot byte for byte, (b) N's recorded routing and
   morph corners AS THE ENGINE REPORTS THEM (routingReadout, cornerReadout —
   neither goes through the node's own writer, L0032), and (c) the same AUDIO
   over a fixed render whichever node you came from.

   CAUSE, measured before the fix: (a) held on every seed — the snapshot really
   was restored — while (b) and (c) failed. The routing matrix was in no
   snapshot (B193), so a restore left it as it was; and a node with morph ON
   drives the matrix from its corners the moment the audio thread runs. So
   visiting such a node REWROTE the matrix, and the matrix it wrote then
   survived the return to any node whose snapshot could not put it back:
   "the future node sounds different depending on the past node you visited",
   exactly. The walk below therefore renders at every visit, so the field at
   the past node actually runs.

   THE FIXED RENDER STARTS FROM SETTLED SILENCE (settledRender): 0.25 s of
   silent blocks, then statefix::render's one second of A3. Without the settle,
   every road differed for its first ~4600 frames and then agreed exactly —
   the master-volume declick (masterVolSm, an 8 ms one-pole that
   plug_activate does not snap to its target) gliding from the PREVIOUS
   render's gain. That is a transient left in flight, not state, and the
   settle is identical for every road, so it cannot hide a difference that
   persists: the pre-fix routing leak is red through it (verified).

   CAUSE TWO, found by this section once the first was fixed: the node's
   writer rounded the corner arrays to %.6g (the persisted format's
   precision), so a node recorded with morph ON came back with its corners
   rounded and the field played slightly different values than the player had
   heard. (b)'s corner readout is %.10g, independent of the node writer, which
   is why it can see a %.6g rounding the snapshot comparison (same writer on
   both sides) cannot. historyJson is lossless now.

   WHAT (c) DELIBERATELY DOES NOT CLAIM, measured rather than assumed: that a
   node sounds bit-identical to the moment it was RECORDED. The first cut of
   this section had that row and it caught the %.6g rounding above; after
   that, its only residue was in-flight MOTION, not state: the morph field's
   glide cache (`morphCur`, the one-pole carrying each slot toward its target
   at Morph Glide, id 158) and the master-volume declick. A restore lands the
   field ON its targets (the 151 off->on transition every load performs
   resets the cache), while the instrument at record time was still gliding
   toward them — up to 0.063 peak difference on a node recorded mid-glide
   under a loaded patch's long Morph Glide, and ~1e-7 where the glide had
   converged to its 1e-9 deadband. Replaying a glide in flight is not a
   property of a patch; it is reported to the lead as an open question rather
   than smuggled into a tolerance here.

   THE OTHER CANDIDATES, each tested rather than assumed:
     * a restore path that skips initState: it does not — undoGoTo and
       undoStep both replay through applyStateJson, whose first act is
       initState. The "restore IS a load" row pins it.
     * state the snapshot omits: the host chunk is the most complete writer
       the shell has, so every audio difference is classified against it. Two
       arrivals whose chunks are identical must render identically; after the
       routing fix the ONLY residue anywhere in the sweep is the ensemble-
       timing stream (`ens=`, B149) — see ensBoundaryEvidence. Engine
       internals, the morph runtime and the LFOs (which activate() reseeds)
       leave nothing.
     * B189's dropped marks: a RECORDING defect (an edit never becomes a
       node), not a restore one; (b) and (c) close without touching it. */
struct PathReport
{
  int nodes = 0, arrivals = 0, stateBad = 0, routeBad = 0, cornerBad = 0, audioPath = 0,
      ctlBad = 0, distinct = 0, silent = 0, loadBad = 0, ensOnly = 0;
  // The first reason PER BUCKET: one shared "first why" printed beside every
  // red row names the wrong failure on all but one of them.
  std::map<const int *, std::string> first;
  void note(int &bucket, const std::string &w)
  {
    bucket++;
    if (!first.count(&bucket)) first[&bucket] = w;
  }
  std::string why(const int &bucket) const
  {
    const auto it = first.find(&bucket);
    return it == first.end() ? "" : " [" + it->second + "]";
  }
};

constexpr int kSettleBlocks = 43;   // 0.25 s: masterVolSm lands on target in ~0.11 s
void settledRender(const clap_plugin_t *p, std::vector<float> &out)
{
  p->activate(p, kSampleRate, 32, 1024);
  p->start_processing(p);
  Blocks(p).run(kSettleBlocks);
  p->stop_processing(p);
  p->deactivate(p);
  render(p, out);
}

/* The four corners as the engine's own armed-view readout prints them
   (%.10g, morphCornerValsJson) — a second writer, so a node writer that
   rounds cannot agree with itself here. */
std::string cornerReadout(const clap_plugin_t *p)
{
  std::string out;
  for (int k = 0; k < 4; k++) out += std::string(hypersaw_debug_cornervals(p, k)) + "\n";
  return out;
}

/* The host chunk with its ensemble-timing lines (`ens=`, `o<k>.ens=`) taken
   out: the one difference the sweep is allowed to explain, and only by
   showing that it is the ONLY difference. */
std::string withoutEns(const std::string &chunk)
{
  std::string out;
  size_t pos = 0;
  while (pos < chunk.size())
  {
    size_t eol = chunk.find('\n', pos);
    if (eol == std::string::npos) eol = chunk.size();
    const std::string line = chunk.substr(pos, eol - pos);
    const std::string key = line.substr(0, line.find('='));
    const bool ens = key == "ens" || (key.size() > 4 && key.compare(key.size() - 4, 4, ".ens") == 0);
    if (!ens) out += line + "\n";
    pos = eol + 1;
  }
  return out;
}

/* Two renders that differ are EXPLAINED only when the host chunks they
   started from differ in the ensemble stream and in nothing else. Anything
   else — including identical chunks and different audio — is the defect. */
enum class AudioVerdict { Same, EnsOnly, Unexplained };
AudioVerdict classify(const std::vector<float> &a, const std::vector<float> &b,
                      const std::string &chunkA, const std::string &chunkB)
{
  if (a == b) return AudioVerdict::Same;
  if (chunkA != chunkB && withoutEns(chunkA) == withoutEns(chunkB)) return AudioVerdict::EnsOnly;
  return AudioVerdict::Unexplained;
}

PathReport historyPaths(uint32_t seed, const std::vector<std::string> &globals,
                        const std::vector<std::string> &corners)
{
  PathReport r;
  uint32_t rng = seed;
  auto pick = [&](size_t n) {
    size_t k = (size_t)(forcecore::rngNext(rng) * (double)n);
    return k >= n ? n - 1 : k;
  };
  const std::vector<clap_id> rids = routingIds();
  const clap_plugin_t *p = makePlugin();
  std::vector<float> scratch, a, b;
  render(p, scratch);   // activate once: morphInit, so every snapshot carries the field
  drain(p);
  undoOp(p, "service");

  std::map<int, std::string> rec, recRoute, recCorner;
  std::map<int, std::vector<float>> recAudio;
  auto record = [&]() {
    drain(p);
    undoOp(p, "service");
    const int c = undoInt(p, "current");
    if (rec.count(c)) return;
    rec[c] = liveJson(p);
    recRoute[c] = routingReadout(p, rids);
    recCorner[c] = cornerReadout(p);
    settledRender(p, recAudio[c]);   // ... and the field at this node runs
  };
  auto tally = [&](AudioVerdict v, int &bucket, const std::string &why) {
    if (v == AudioVerdict::EnsOnly) r.ensOnly++;
    if (v == AudioVerdict::Unexplained) r.note(bucket, why);
  };
  record();

  for (int s = 0; s < 28; s++)
  {
    const double roll = forcecore::rngNext(rng);
    if (roll < 0.24)
    {
      EvList ev;
      const int k = 1 + (int)pick(2);
      for (int i = 0; i < k; i++) ev.push(rids[pick(rids.size())], -1.0 + 2.5 * forcecore::rngNext(rng));
      paramsOf(p)->flush(p, &ev.list, &kOut);
      undoOp(p, "mark", s);
      record();
    }
    else if (roll < 0.34)
    {
      EvList ev;
      ev.push(4, forcecore::rngNext(rng));    // detune
      ev.push(152, forcecore::rngNext(rng));  // the puck, so corners get to win
      ev.push(153, forcecore::rngNext(rng));
      paramsOf(p)->flush(p, &ev.list, &kOut);
      undoOp(p, "mark", s);
      record();
    }
    else if (roll < 0.46)
    {
      // The editor's toggle, idle-instance form: the bracket and the value
      // drain through the host's flush, as the preset door's writes do.
      hypersaw_debug_gesture(p, 151, true);
      undoOp(p, "setmorph", valueOf(p, 151) > 0.5 ? 0 : 1);
      hypersaw_debug_gesture(p, 151, false);
      record();
    }
    else if (roll < 0.56)
    {
      hypersaw_debug_capture(p, (int)pick(4));
      record();
    }
    else if (roll < 0.64 && !globals.empty())
    {
      std::string blob;
      readFile(globals[pick(globals.size())], blob);
      hypersaw_debug_apply(p, blob.c_str());
      record();
    }
    else if (roll < 0.70 && !corners.empty())
    {
      std::string blob;
      const std::string &f = corners[pick(corners.size())];
      readFile(f, blob);
      const int k = (int)pick(4);
      hypersaw_debug_cornerapply(p, k, blob.c_str());
      hypersaw_debug_cornername(p, k, stemOf(f).c_str());
      record();
    }
    else
    {
      // Visit a past node and PLAY it: the field at that node runs.
      std::vector<int> ids;
      for (const auto &kv : rec) ids.push_back(kv.first);
      undoOp(p, "restore", ids[pick(ids.size())]);
      drain(p);
      settledRender(p, scratch);
    }
  }

  std::vector<int> ids;
  for (const auto &kv : rec) ids.push_back(kv.first);
  r.nodes = (int)ids.size();
  for (int n : ids)
  {
    if (rms(recAudio[n]) == 0) r.silent++;
    if (recAudio[n] != recAudio[ids.front()]) r.distinct++;
    std::vector<float> first;
    std::string firstChunk;
    for (int t = 0; t < 3; t++)
    {
      const int from = ids[pick(ids.size())];
      undoOp(p, "restore", from);
      drain(p);
      settledRender(p, scratch);   // play the past node
      undoOp(p, "restore", n);
      drain(p);
      r.arrivals++;
      const std::string tag = "node " + std::to_string(n) + " from " + std::to_string(from);
      if (liveJson(p) != rec[n])
        r.note(r.stateBad, tag + ": snapshot differs — " + firstKeyDiff(rec[n], liveJson(p)));
      if (routingReadout(p, rids) != recRoute[n])
        r.note(r.routeBad, tag + ": the ENGINE's routing differs from what the node recorded");
      if (cornerReadout(p) != recCorner[n])
        r.note(r.cornerBad, tag + ": the ENGINE's morph corners differ from what the node recorded");
      const std::string chunk = saveChunk(p);
      settledRender(p, b);
      if (t == 0)
      {
        first = b;
        firstChunk = chunk;
        /* MUST READ ZERO: the same road twice renders the same bytes (up to
           the named stream), so a difference between roads below is the
           road, not the render. */
        undoOp(p, "restore", from);
        drain(p);
        settledRender(p, scratch);
        undoOp(p, "restore", n);
        drain(p);
        const std::string again = saveChunk(p);
        settledRender(p, a);
        tally(classify(b, a, chunk, again), r.ctlBad, tag + ": the same road twice rendered different bytes");
      }
      else
        tally(classify(first, b, firstChunk, chunk), r.audioPath,
              tag + ": audio depends on the node visited before it");
    }
    /* A RESTORE IS A LOAD: from the SAME disturbed state, the restore and the
       preset door fed the node's own JSON land on the same state and audio.
       Both doors start from one disturbance, so a gap either door shares
       cannot make this row red — it is about the two DOORS, not about what
       the snapshot holds (the rows above are).
       ONE DELIBERATE DIFFERENCE (B222 S4): only the restore reads the node's
       `routing` key; the preset door leaves the matrix alone, as it does on
       main. So the preset leg starts from the node's own matrix (a restore
       first, then a disturbance that touches no routing cell), and the two
       doors are compared on everything else. */
    {
      auto disturb = [&]() {
        EvList ev;
        for (clap_id id : kMutable) ev.push(id, 0.444);
        paramsOf(p)->flush(p, &ev.list, &kOut);
        drain(p);
      };
      disturb();
      undoOp(p, "restore", n);
      drain(p);
      const std::string viaRestore = liveJson(p), chunkR = saveChunk(p);
      settledRender(p, a);
      undoOp(p, "restore", n);   // the node's matrix, which the preset door keeps
      drain(p);
      disturb();
      const std::string node = undoOp(p, "json", n);
      hypersaw_debug_apply(p, node.c_str());
      drain(p);
      const std::string viaLoad = liveJson(p), chunkL = saveChunk(p);
      settledRender(p, b);
      if (viaLoad != viaRestore)
        r.note(r.loadBad, "node " + std::to_string(n) + ": its JSON through the preset door did "
                          "not land where the restore did — " + firstKeyDiff(viaRestore, viaLoad));
      else
        tally(classify(a, b, chunkR, chunkL), r.loadBad,
              "node " + std::to_string(n) + ": the preset door and the restore render differently");
      undoOp(p, "restore", n);   // the door marked a node; stand back on N
      drain(p);
    }
  }
  p->destroy(p);
  return r;
}

void historyPathChecks(uint32_t seed, const std::vector<std::string> &globals,
                       const std::vector<std::string> &corners)
{
  for (int run = 0; run < 3; run++)
  {
    const uint32_t s = seed + 104729u * (uint32_t)run;
    const PathReport r = historyPaths(s, globals, corners);
    const std::string tag = "paths[seed " + std::to_string(s) + "]";
    std::printf("     %s: %d nodes, %d arrivals, %d nodes render distinct from the root, "
                "%d audio differences explained by the ens= stream alone\n",
                tag.c_str(), r.nodes, r.arrivals, r.distinct, r.ensOnly);
    check(r.nodes >= 8, tag + ": the walk built a tree worth testing (>= 8 nodes)");
    check(r.ctlBad == 0, tag + " CONTROL: the same road twice renders identical bytes (must read zero)" +
                             r.why(r.ctlBad));
    check(r.distinct > 0 && r.silent == 0,
          tag + " CONTROL: nodes render audibly and DIFFERENTLY (the audio detector can see a "
                "difference; " + std::to_string(r.silent) + " silent)");
    check(r.stateBad == 0, tag + ": every arrival lands on the node's snapshot byte for byte" +
                               r.why(r.stateBad));
    check(r.routeBad == 0, tag + ": every arrival lands on the node's ROUTING as the engine reports it" +
                               r.why(r.routeBad));
    check(r.audioPath == 0,
          tag + ": the fixed render does not depend on which node you came from" +
              r.why(r.audioPath));
    check(r.cornerBad == 0,
          tag + ": every arrival lands on the node's morph CORNERS as the engine reports them" +
              r.why(r.cornerBad));
    check(r.loadBad == 0, tag + ": a restore IS a load (the node's JSON through the preset door "
                                "lands on the same state and audio)" + r.why(r.loadBad));
  }
}

/* ---- the critic's rework (PR #732 review) ---- */

/* B1 — CORNERS THAT AGREE AT THE SAVED PRECISION ARE CORNERS THAT AGREE.
   The critic's probe, run as a row. A host chunk (and a preset) writes corner
   values %.6g, while a corner captured live keeps full precision; so an
   ordinary save + reopen leaves corners A and B holding 0.123457 and
   0.123456789 for a cell the player never morphed. Compared with `!=` that
   marked the whole routing block as split, and morph-on then played corner
   A's routing over the player's live edit — the human's defect, back through
   a session reload. The row is the reopen; its control is the identical
   sequence without the reopen, which must keep the edit on either build. */
void savedPrecisionAgreementChecks()
{
  const std::vector<clap_id> rids = routingIds();
  const clap_id X = rids[1], Y = rids[2];
  bool kept[2] = {false, false}, splitSeen = false;
  for (int reload = 0; reload < 2; reload++)
  {
    const clap_plugin_t *p = makePlugin();
    p->activate(p, kSampleRate, 32, 1024);
    p->start_processing(p);
    Blocks b(p);
    b.run(2);
    undoOp(p, "service");
    const std::string self = saveJson(p);
    hypersaw_debug_apply(p, self.c_str());   // a load: the corners count as authored
    b.run(2);
    {
      EvList ev;
      ev.push(X, 0.123456789);
      b.run(4, &ev);
    }
    for (int k = 0; k < 4; k++) hypersaw_debug_capture(p, k);
    if (reload)
    {
      const std::string c = saveChunk(p);
      p->stop_processing(p);
      p->deactivate(p);
      loadChunk(p, c);
      p->activate(p, kSampleRate, 32, 1024);
      p->start_processing(p);
      b.run(4);
    }
    hypersaw_debug_capture(p, 1);
    if (reload)
      splitSeen = std::string(hypersaw_debug_cornervals(p, 0)) != hypersaw_debug_cornervals(p, 1);
    {
      EvList ev;
      ev.push(Y, 0.5);
      b.run(4, &ev);
    }
    editorMorph(p, b, 1, 40);
    kept[reload] = valueOf(p, Y) == 0.5;
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
  check(splitSeen, "B1 PRECONDITION: after the reopen, corners A and B differ in their stored "
                   "digits (0.123457 vs 0.123456789) — the case the row is about exists");
  check(kept[0], "B1 CONTROL: without the reopen, the routing edit survives morph-on");
  check(kept[1], "B1: after an ordinary save + reopen, the routing edit STILL survives morph-on "
                 "(corners equal at the saved %.6g precision agree)");
}

/* NOTE 4 — THE ADOPTION UNIT IS THE FIELD'S OWN. Corners A and B disagree on
   routing cell Y and agree on cell X; the player edits X with morph off and
   switches morph on. In BLEND the field computes each continuous cell on its
   own, so X — whose corners agree — keeps the edit. In QUANTUM the routing
   block is one unit (ADR-176 §3): it plays the owning corner whole, X
   included, which is the stated rule and the must-fire beside the blend row. */
void adoptionUnitChecks()
{
  const std::vector<clap_id> rids = routingIds();
  const clap_id X = rids[1], Y = rids[2];
  for (int blend = 0; blend < 2; blend++)
  {
    const clap_plugin_t *p = makePlugin();
    p->activate(p, kSampleRate, 32, 1024);
    p->start_processing(p);
    Blocks b(p);
    b.run(2);
    const std::string self = saveJson(p);
    hypersaw_debug_apply(p, self.c_str());
    b.run(2);
    const double x0 = valueOf(p, X);
    auto set = [&](clap_id id, double v) {
      EvList ev;
      ev.push(id, v);
      b.run(4, &ev);
    };
    set(157, blend);   // morph mode: 0 quantum, 1 blend
    set(Y, 0.25);
    hypersaw_debug_capture(p, 0);
    set(Y, 1.5);
    hypersaw_debug_capture(p, 1);
    set(152, 1.0);     // the puck on corner B
    set(153, 0.0);
    set(X, 0.6);
    editorMorph(p, b, 1, 40);
    const double x = valueOf(p, X), y = valueOf(p, Y);
    if (blend)
      check(x == 0.6 && y == 1.5,
            "NOTE 4, BLEND: a routing cell whose corners agree keeps its live edit although a "
            "sibling cell's corners differ (X " + std::to_string(x) + " want 0.6; Y " +
                std::to_string(y) + " = corner B)");
    else
      check(x == x0 && y == 1.5,
            "NOTE 4, QUANTUM (the stated group rule, and the must-fire): the routing block plays "
            "corner B whole, so X returns to B's " + std::to_string(x0) + " (got " +
                std::to_string(x) + ")");
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
}

/* S3 — A MOD-ROUTE DEPTH SURVIVES A HISTORY ROUND TRIP EXACTLY. The route
   enters through the preset door's `modRoutes` key (the door
   gen_state_fixtures already uses), with a depth %.6g cannot hold. The node
   must carry the depth's %.17g text — asserted against the text, not against
   the node writer's own output, because a writer that rounded would agree
   with itself (L0032). */
void modRouteDepthChecks()
{
  const clap_plugin_t *p = makePlugin();
  std::vector<float> scratch;
  render(p, scratch);
  drain(p);
  undoOp(p, "service");
  loadJson(p, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{},\"modRoutes\":\"0:4:0.123456789;\"}");
  undoOp(p, "service");
  const int n = undoInt(p, "current");
  char exact[64];
  std::snprintf(exact, sizeof exact, "0:4:%.17g;", 0.123456789);
  const std::string node = undoOp(p, "json", n);
  check(node.find(exact) != std::string::npos,
        std::string("S3: the history node holds the route depth EXACTLY (") + exact + ")");
  loadJson(p, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{}}");   // routes cleared
  undoOp(p, "service");
  const bool cleared = undoOp(p, "json", undoInt(p, "current")).find(exact) == std::string::npos;
  undoOp(p, "restore", n);
  drain(p);
  check(cleared && liveJson(p) == node,
        "S3: the depth comes back through a restore (and the load between really cleared it)");
  p->destroy(p);
}

/* S4 — A PRESET CANNOT CARRY ROUTING. The `routing` key is the HISTORY
   snapshot's; the preset door must ignore it exactly as main does, because
   reading it would decide B193's key name and a preset-load behaviour, both
   human-gated. The control is the other door: a history restore of a node
   that holds the matrix DOES move it — so the readout can move. */
void presetRoutingKeyChecks()
{
  const std::vector<clap_id> rids = routingIds();
  const clap_id cell = rids[1];
  const clap_plugin_t *p = makePlugin();
  std::vector<float> scratch;
  render(p, scratch);
  drain(p);
  undoOp(p, "service");
  {
    EvList ev;
    ev.push(cell, 0.7);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
  }
  const std::string preset = "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"routing\":\"" +
                             std::to_string(cell) + ":0.25\",\"params\":{}}";
  hypersaw_debug_apply(p, preset.c_str());
  drain(p);
  check(valueOf(p, cell) == 0.7,
        "S4: a preset carrying a `routing` key, loaded through the preset door, leaves the matrix "
        "untouched (cell " + std::to_string(valueOf(p, cell)) + ", want 0.7)");
  undoOp(p, "service");
  {
    EvList ev;
    ev.push(cell, 0.25);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
  }
  undoOp(p, "mark", 1);
  undoOp(p, "service");
  const int n = undoInt(p, "current");
  {
    EvList ev;
    ev.push(cell, 0.7);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
  }
  undoOp(p, "restore", n);
  drain(p);
  check(valueOf(p, cell) == 0.25,
        "S4 CONTROL: a history restore DOES put the node's matrix back (the readout can move)");
  p->destroy(p);
}

/* THE ONE STATE HISTORY STILL DOES NOT RESTORE, PINNED (L0033/L0036), and the
   reason the rows above may explain a difference at all.
   `ens=` is the ADR-077/078 ensemble-timing stream: each note's onset offsets
   are corrected FROM the notes before it, so it is the memory of the phrase
   played, not a setting. The host chunk carries it (B149) so a reopened
   session resumes the phrase; the preset JSON and therefore history do not,
   and a load deliberately does not reset it (initState: "a separate question
   about stream identity across a load, and no row asks it yet"). B222 is that
   row: with Onset Scatter on, a node sounds different after one more note
   than before it, whichever way you arrived. Whether a restore should REWIND
   the phrase to where the node was recorded is the human's call, not this
   change's — carrying `ens` in history would make the rows above exact, and
   would also make every restore replay the same scatter. WHEN THIS ROW GOES
   RED, history restores the stream: delete classify's EnsOnly exemption.
   The control beside it is the same sequence with Onset Scatter off, which
   must read zero — so the difference is the stream and not the sequence. */
void ensBoundaryEvidence()
{
  auto run = [](double scatterMs, bool &audioDiffers, bool &onlyEns) {
    const clap_plugin_t *p = makePlugin();
    std::vector<float> scratch, a, b;
    render(p, scratch);   // morphInit
    EvList ev;
    ev.push(91, scatterMs);   // onsetScatter
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    undoOp(p, "mark", 1);
    undoOp(p, "service");
    const int n = undoInt(p, "current");
    undoOp(p, "restore", n);
    drain(p);
    const std::string c1 = saveChunk(p);
    settledRender(p, a);
    settledRender(p, scratch);   // one more phrase played here...
    undoOp(p, "restore", n);     // ... and back to the SAME node
    drain(p);
    const std::string c2 = saveChunk(p);
    settledRender(p, b);
    audioDiffers = a != b;
    onlyEns = c1 != c2 && withoutEns(c1) == withoutEns(c2);
    p->destroy(p);
  };
  bool diff = false, ens = false;
  run(30.0, diff, ens);
  check(diff && ens, "BOUNDARY (B222, the human's call): with Onset Scatter on, the ensemble-timing "
                     "stream is NOT restored by history — the same node renders differently after "
                     "more notes, and the host chunks differ ONLY in ens= — WHEN THIS ROW GOES RED "
                     "delete classify's EnsOnly exemption");
  run(0.0, diff, ens);
  check(!diff, "BOUNDARY CONTROL: the same sequence with Onset Scatter OFF renders identically "
               "(the difference above is the stream, not the sequence)");
}

}   // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "tests/state_fixtures";
  /* Layer 3's two inputs. The preset root is repo-relative by the same
     convention every other check's directory argument uses (bank_check takes
     docs/presets/factory); the seed is FIXED so ./verify is deterministic and
     overridable so a red run is reproducible from its printed number. */
  const std::string presetDir = argc > 2 ? argv[2] : "docs/presets/factory";
  const uint32_t seed = argc > 3 ? (uint32_t)std::strtoul(argv[3], nullptr, 10) : 20260920u;

  /* THE NUMBER THE 48 KB RESERVATION IS SIZED AGAINST — measured, not assumed,
     and printed every run so it cannot creep up unseen. Three cases, because
     they differ by nearly an order of magnitude: a default instance (most
     values are 0 or 1 and %.17g prints those in one character), the corpus
     patches below, and the ADVERSARIAL case here — every parameter driven to
     an irrational fraction of its range so every one of them costs its full
     17 significant digits, plus the morph field forced into existence so its
     4 x ~224 corner array is in the blob. A real patch cannot exceed it. */
  {
    const clap_plugin_t *p = makePlugin();
    std::printf("     stateJson (defaults): %zu bytes\n", saveJson(p).size());

    auto *pp = paramsOf(p);
    EvList ev;
    const uint32_t nParams = pp->count(p);
    for (uint32_t i = 0; i < nParams; i++)
    {
      clap_param_info_t info{};
      if (!pp->get_info(p, i, &info)) continue;
      if (info.flags & CLAP_PARAM_IS_STEPPED) continue;   // integers stay short, honestly
      ev.push(info.id, info.min_value +
                           (info.max_value - info.min_value) * 0.31415926535897932);
    }
    pp->flush(p, &ev.list, &kOut);
    drain(p);
    (void)hypersaw_debug_ownersjson(p);   // forces morphInit, so the corners are in the blob
    /* B222: a node is the HISTORY snapshot — lossless corners plus the routing
       matrix — so the adversarial case captures the full-precision patch into
       all four corners, which is the largest thing a node can now hold. */
    for (int k = 0; k < 4; k++) hypersaw_debug_capture(p, k);
    const size_t worst = liveJson(p).size();
    std::printf("     history snapshot (%u params at full precision + 4 full-precision corners"
                " + routing): %zu bytes (UndoTree reserves %zu per node, x%d = %zu KB)\n",
                nParams, worst, UndoTree::kJsonReserve, UndoTree::kUndoCap,
                (UndoTree::kJsonReserve * (size_t)UndoTree::kUndoCap) / 1024);
    check(worst < UndoTree::kJsonReserve, "adversarial snapshot fits the per-node reservation");
    p->destroy(p);
  }

  treeChecks();
  fixtureChecks(dir);
  automationControl();
  presetIdentityChecks();

  /* LAYER 4 (B191). The control runs first, for layer 3's reason: a zero that
     cannot be shown to move is not evidence. */
  unclosedBracketControl();
  controlMarkChecks();

  /* LAYER 3 (B186). The controls run FIRST and unconditionally: they prove
     the sweep and the recording rule can go red before anything green below
     is allowed to mean something (L0059). */
  gauntletControls();
  roundTripGapEvidence();
  fxCombGapEvidence();
  {
    std::vector<std::string> globals, corners;
    collectPresets(presetDir, globals, corners);
    gauntletChecks(seed, globals, corners);
    reportedScenario(globals);
    cornerReferenceScenario(corners);

    /* LAYER 5 (B222). Each group carries its own must-read-zero control and
       its own must-fire row beside the property, rather than a shared prelude:
       the three defects have three different detectors. */
    morphToggleChecks();
    morphOnKeepsPatchChecks();
    savedPrecisionAgreementChecks();   // B1 (critic, PR #732)
    adoptionUnitChecks();              // NOTE 4
    modRouteDepthChecks();             // S3
    presetRoutingKeyChecks();          // S4
    ensBoundaryEvidence();   // first: it is what licenses the one exemption below
    historyPathChecks(seed, globals, corners);
  }

  /* The reservation's real headroom. A DEFAULT instance is the small case; a
     loaded patch with an authored morph field is the big one, and it is the
     big one the 48 KB must clear. Printed, not merely asserted, so the day it
     starts creeping toward the reservation someone sees it. */
  std::printf("     largest fixture snapshot: %zu bytes (reservation %zu)\n", g_maxSnapshot,
              UndoTree::kJsonReserve);
  check(g_maxSnapshot > 0 && g_maxSnapshot < UndoTree::kJsonReserve,
        "largest corpus snapshot fits the per-node reservation");

  std::printf("undo_check: %s (%d failure%s)\n", g_failures ? "RED" : "GREEN", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
