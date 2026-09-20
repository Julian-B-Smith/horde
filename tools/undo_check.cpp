/*
 * undo_check — the undo history's oracle (B84 / ADR-160).
 *
 *   undo_check [state_fixtures_dir]      (default: tests/state_fixtures)
 *
 * Two layers, because the design has two halves that fail differently.
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
 * THE CONTROL, and its calibration. ADR-160 (3) rules that host parameter
 * events and automation NEVER create a node: history records the player's
 * states, and automation keeps writing over the top exactly as it does after
 * any manual edit. So 500 host param events plus 50 automation-shaped blocks
 * must produce ZERO nodes. On its own that assertion is worthless — a counter
 * wired to nothing also reads zero — so the same instance is then marked the
 * way the GUI marks it and must produce exactly one node. The zero is only
 * evidence because the one beside it proves the detector can see.
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
 * WIRED. ./verify runs this (fast, beside state_check); the "standalone and
 * unwired" note this header carried until 2026-09-20 described the day it was
 * written, not the gate it has been since.
 */

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "statefix_common.h"
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
    check(rootJson == saveJson(p), "fixtures: " + n + " root node IS the live state");

    // Move somewhere else, mark it, and check the tree actually forked forward.
    mutate(p, 0.371);
    undoOp(p, "mark", 1);
    undoOp(p, "service");
    check(undoInt(p, "size") == 2 && undoInt(p, "parent", undoInt(p, "current")) == root,
          "fixtures: " + n + " -> the edit is a child of the root");
    check(saveJson(p) != rootJson, "fixtures: " + n + " the mutation actually moved the state");

    // Restore, drain, and demand byte-for-byte.
    const bool back = undoOp(p, "restore", root) == "1";
    drain(p);
    check(back && undoInt(p, "current") == root, "fixtures: " + n + " restore lands on the node");
    check(saveJson(p) == rootJson, "fixtures: " + n + " restore is BIT-IDENTICAL");
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

}   // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "tests/state_fixtures";

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
    const size_t worst = saveJson(p).size();
    std::printf("     stateJson (%u params at full precision + morph field): %zu bytes"
                " (UndoTree reserves %zu per node, x%d = %zu KB)\n",
                nParams, worst, UndoTree::kJsonReserve, UndoTree::kUndoCap,
                (UndoTree::kJsonReserve * (size_t)UndoTree::kUndoCap) / 1024);
    check(worst < UndoTree::kJsonReserve, "adversarial snapshot fits the per-node reservation");
    p->destroy(p);
  }

  treeChecks();
  fixtureChecks(dir);
  automationControl();
  presetIdentityChecks();

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
