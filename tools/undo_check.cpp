/*
 * undo_check — the undo history's oracle (B84 / ADR-160, B186).
 *
 *   undo_check [state_fixtures_dir] [preset_dir] [seed]
 *              (defaults: tests/state_fixtures  docs/presets/factory  20260920)
 *
 * Three layers, because the design has parts that fail differently.
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
 * WIRED (verify:239), and this paragraph is the correction of the one that
 * stood here until 2026-09-20: it read "STANDALONE AND UNWIRED. Not run by
 * ./verify", which had been false since the human wired the gate on
 * 2026-09-13. test_table_check's wired-or-explained rule could not catch it —
 * the rule looks for a literal `UNWIRED:` marker in the first 40 lines
 * (tools/test_table_check.py:82) and finds nothing to complain about in prose
 * that merely CLAIMS the opposite of the truth. Prose that contradicts the
 * gate list is invisible to a marker grep; only reading it catches it.
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
  std::string liveState() const { return saveJson(p); }
  void visit(int i)
  {
    undoOp(p, "restore", i);
    drain(p);   // the restore is QUEUED, exactly as the GUI's is
  }
};

/* THE SECOND KNOWN GAP, also found by this gauntlet on 2026-09-20 (seed
   20268020: "arriving at node 27 from 24 gave the wrong state: fx1type want 5
   got 1"), and also not history's — history is the messenger.

   MEASURED, with audio actually processed (fxCombGapEvidence below): a state
   load that must MOVE COMB between slots LOSES COMB ENTIRELY, in both
   directions. COMB is the rack's only singleton (fx_rack.h:270 — one shared
   KS bank, so a second Comb slot doubles every write) and the cap is enforced
   at the one choke point every type write passes (hypersaw_clap.cpp:6664).
   During the load the slot that currently holds COMB is rewritten first,
   which ARMS an 80 ms crossfade whose shadow still holds COMB (fx_rack.h:292,
   B117, deliberately); the incoming slot's write arrives microseconds later
   in the same drain, is refused against that shadow, and nobody retries once
   the fade ends. Preset load, DAW session reload and a history restore all
   drop it identically: save a patch with COMB in slot 2, move COMB to slot 4,
   reload the patch, and the rack comes back with no COMB at all.

   AND A HEADLESS CONSEQUENCE THIS GAUNTLET MUST RESPECT. `fadeLeft` decays
   only inside renderCrossfade (fx_rack.h:581), so in an instance that
   processes no audio the shadow NEVER clears: one COMB that ever left a slot
   blocks COMB everywhere, forever. So the gauntlet keeps COMB out of the
   instrument entirely — no edit writes type 5, and the two factory presets
   that place it (BS - Growl Bass, FX - Comb Throat) are dropped from the load
   pool and the drop is printed, never silent. The evidence row below is the
   one place that DOES process audio, so what it asserts is the product's
   behaviour and not the harness's.

   Fixing it is a ruling about what a load may do to a capped type — evict the
   holder first? apply types in freeing order? let a load outrank a shadow? —
   which is ADR-054 / B117 territory and deliberately NOT decided here. */
constexpr int kCombType = 5;
bool isFxTypeId(clap_id id) { return id >= 57 && id <= 63 && ((id - 57) & 1) == 0; }

// Does this global preset place COMB in any of the four slots?
bool placesComb(const std::string &blob)
{
  for (int s = 1; s <= 4; s++)
  {
    char key[16];
    std::snprintf(key, sizeof key, "\"fx%dtype\"", s);
    const size_t k = blob.find(key);
    if (k == std::string::npos) continue;
    const size_t c = blob.find(':', k);
    if (c != std::string::npos && std::atoi(blob.c_str() + c + 1) == kCombType) return true;
  }
  return false;
}

/* ---- the preset store, as the gauntlet's two load verbs ---- */

/* The two preset KINDS are told apart by what the file carries, not by which
   folder it sits in: a global names "params", a corner preset names
   "cornerPreset" (ADR-105). Sorted, so the seeded draw is reproducible. */
void collectPresets(const std::string &root, std::vector<std::string> &globals,
                    std::vector<std::string> &corners, int &combSkipped)
{
  std::error_code ec;
  for (const auto &e : fs::recursive_directory_iterator(root, ec))
  {
    if (!e.is_regular_file()) continue;
    const std::string path = e.path().string();
    if (!isJson(path)) continue;
    std::string blob;
    if (!readFile(path, blob)) continue;
    if (blob.find("\"params\"") != std::string::npos)
    {
      // see kCombType: a COMB that ever leaves a slot blocks COMB forever in
      // an instance that processes no audio, which is this gauntlet's.
      if (placesComb(blob)) { combSkipped++; continue; }
      globals.push_back(path);
    }
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
  bool fxType = false;   // draw from [0,9] minus COMB — see kCombType
};

// One draw, honouring the one excluded value. Kept here rather than at the
// call site so "what may an edit write?" has a single answer.
double drawValue(const ParamPick &q, double r01)
{
  if (!q.fxType) return q.lo + (q.hi - q.lo) * r01;
  int v = (int)(r01 * 9.0);
  if (v > 8) v = 8;
  return v >= kCombType ? v + 1 : v;
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

/* THE SECOND EXCLUSION, RE-EARNED EVERY RUN — same discipline as the alias
   gap. Moving COMB back DOWN a slot is refused at the cap; moving an
   uncapped type the identical distance is not, which is the control that
   stops this probe from being a tautology. */
void fxCombGapEvidence()
{
  const double back = fxRelocate(kCombType, 0, 2);
  std::printf("     COMB relocation: slot 1 -> slot 3 -> reload the slot-1 patch leaves slot 1 "
              "holding type %.0f (wanted %d)\n", back, kCombType);
  check(back != (double)kCombType,
        "known gap (NOT history's): a state load that must MOVE COMB between slots loses it "
        "entirely (fade shadow + singleton cap), so the gauntlet never writes COMB. WHEN THIS ROW "
        "GOES RED THE LOAD PATH IS FIXED: delete the kCombType exclusion");
  check(fxRelocate(6, 0, 2) == 6.0,
        "known gap control: the SAME relocation with Notch (uncapped) round-trips exactly, so the "
        "loss above is the cap and not the probe");
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
  h.rec[undoInt(p, "current")] = saveJson(p);
  const std::vector<ParamPick> pool = editablePool(p);

  auto pick = [&](size_t n) { return (size_t)(forcecore::rngNext(rng) * (double)n); };

  // ONE recording assertion, applied to every step that marks.
  auto closeMark = [&](MarkObs &o, const std::string &was, int step) {
    drain(p);
    undoOp(p, "service");
    o.liveJson = saveJson(p);
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
    if (h.rec.find(c) == h.rec.end() || saveJson(p) != h.rec[c])
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
    const std::string was = saveJson(p);

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
                    const std::vector<std::string> &corners, int combSkipped)
{
  check(globals.size() >= 2,
        "gauntlet: the preset store offers at least two distinct global presets");
  check(!corners.empty(), "gauntlet: the preset store offers at least one corner preset");
  std::printf("     gauntlet: seed %u (argv[3] overrides), %zu global + %zu corner presets"
              " (%d global preset%s held back: see kCombType)\n",
              (unsigned)seed, globals.size(), corners.size(), combSkipped,
              combSkipped == 1 ? "" : "s");

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

    const std::string before = saveJson(p);
    EvList ev;
    ev.push(q.id, v);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    if (saveJson(p) == before)
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
     Two families of parameter accept a value, read it back, and leave the
     snapshot byte-identical, so history cannot carry them:

       * EVERY ROUTING ID (>= 10000, hypersaw_clap.cpp:982). stateJson emits
         kParams, the per-oscillator copies, the engine blocks, morph, modRoutes
         and intent — and no routing coefficient (hypersaw_clap.cpp:5773). The
         binary chunk has a `routing=` section; the JSON path, which IS what
         UndoTree stores, does not. So the matrix is outside undo/redo entirely.
         That is a state-serialisation question, not a history one, and closing
         it is an ADR-sized decision about the preset schema — deliberately NOT
         done here (B191 is about marking).
       * id 1043. Base id 43 is dispatched by RAW id to a shared object
         (gui_reach's patch-scope derivation), so the oscillator-2 twin is
         declared but reaches nothing: the value written above reads back
         UNCHANGED, which no other parameter in the table does.

     Pinned as a set, not a count, so a new hole in either direction is red. */
  std::vector<clap_id> instrumentInert;
  int routingInert = 0, routingDeclared = 0;
  for (const ParamPick &q : pool)
    if (q.id >= 10000) routingDeclared++;
  for (clap_id id : inert)
    (id >= 10000) ? (void)routingInert++ : instrumentInert.push_back(id);
  check(instrumentInert.size() == 1 && instrumentInert[0] == 1043,
        "layer 4 boundary: id 1043 is the ONLY non-routing parameter whose write leaves the "
        "snapshot unchanged (base 43 is patch-scope, so the osc-2 twin reaches nothing) — " +
            std::to_string(instrumentInert.size()) + " found");
  check(routingInert == routingDeclared && routingDeclared > 0,
        "layer 4 boundary: ALL " + std::to_string(routingDeclared) + " routing ids are absent "
        "from stateJson, so the routing matrix is outside undo/redo (" +
            std::to_string(routingInert) +
            " inert) — WHEN THIS ROW GOES RED ROUTING HAS ENTERED THE STATE: delete this "
            "boundary and let the rows above cover it");
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
  const std::string recX = saveJson(p);

  // 2. edit on top of X
  {
    EvList ev;
    for (clap_id id : kMutable) ev.push(id, 0.617);
    paramsOf(p)->flush(p, &ev.list, &kOut);
  }
  undoOp(p, "mark", 1);
  undoOp(p, "service");
  const int nXEdit = undoInt(p, "current");
  const std::string recXEdit = saveJson(p);
  check(nXEdit != nX && undoInt(p, "parent", nXEdit) == nX,
        "scenario: the edit is a child of the node preset X made");

  // 3. step back to X, 4. load Y there — the fork
  undoOp(p, "restore", nX);
  drain(p);
  check(saveJson(p) == recX, "scenario: stepping back to X restores X");
  hypersaw_debug_apply(p, blobY.c_str());
  drain(p);
  undoOp(p, "service");
  const int nY = undoInt(p, "current");
  const std::string recY = saveJson(p);
  check(undoInt(p, "parent", nY) == nX, "scenario: loading Y on the second branch FORKS from X");
  check(recY != recX, "scenario control: Y's state really differs from X's");

  // THE CLAIM, in the state. Nothing on the first branch may have moved.
  check(undoOp(p, "json", nX) == recX, "scenario: loading Y did NOT rewrite what the X node stores");
  check(undoOp(p, "json", nXEdit) == recXEdit,
        "scenario: loading Y did NOT rewrite what the edit-on-X node stores");

  // 5. return to the first branch
  undoOp(p, "restore", nXEdit);
  drain(p);
  check(saveJson(p) == recXEdit,
        "scenario: returning to the first branch lands on the EDIT ON TOP OF X, byte for byte");
  check(saveJson(p) != recY, "scenario control: that landing is not Y's state wearing X's label");

  /* 6. and a further edit there sits on X, not on Y — which is exactly the
        state assertion above plus the recording rule, so it is stated as one. */
  {
    EvList ev;
    for (clap_id id : kMutable) ev.push(id, 0.213);
    paramsOf(p)->flush(p, &ev.list, &kOut);
  }
  const std::string before = saveJson(p);
  undoOp(p, "mark", 2);
  undoOp(p, "service");
  const int nX3 = undoInt(p, "current");
  check(undoInt(p, "parent", nX3) == nXEdit && undoOp(p, "json", nX3) == saveJson(p),
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
  check(undoOp(p, "json", node) == saveJson(p),
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
  check(saveJson(p) == undoOp(p, "json", node),
        "corner reference: that round trip is byte-identical");

  p->destroy(p);
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
    int combSkipped = 0;
    collectPresets(presetDir, globals, corners, combSkipped);
    gauntletChecks(seed, globals, corners, combSkipped);
    reportedScenario(globals);
    cornerReferenceScenario(corners);
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
