/*
 * undo_tree.h — the undo history as a pure data structure (ADR-160, B84).
 *
 * Framework-free by the architecture default: no CLAP, no GUI, no clock, no
 * allocation after construction. The shell owns one of these, decides WHEN a
 * node is created (ADR-160: gesture end, preset load, corner apply/capture,
 * morph toggle, host state load — never a host parameter event), and hands in
 * the snapshot string. This file only knows how the nodes relate.
 *
 * SHAPE. A fixed ring of kUndoCap slots written in creation order. `current`
 * is where the player is standing; `push` makes a child of it and moves there;
 * `undo`/`redo` walk the edges; `restore` teleports. Restoring an old node and
 * then editing FORKS — the next push becomes that node's child, which is the
 * whole reason this is a tree and not a stack.
 *
 * EVICTION MAKES A FOREST, ON PURPOSE. When the ring wraps, the oldest node is
 * overwritten and its children are re-parented to ITS parent. If that parent
 * was already gone the children become roots, so the structure is a forest,
 * not a tree. The alternative — dropping the whole subtree — would delete
 * states the player can still see in the list, and pretending the survivors
 * share a root would draw an edge that does not exist.
 *
 * NO CLOCK. `tick` is a counter the shell supplies (SPEC §5.7 bans wall-clock
 * reads anywhere in the core). It orders nodes for display; `born` orders them
 * for redo, and is separate because eviction reuses slots.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hypersaw
{

class UndoTree
{
 public:
  static constexpr int kUndoCap = 200;
  static constexpr int kNone = -1;
  /* 48 KB per slot. MEASURED 2026-09-13 by undo_check (B84), not estimated:
     a default instance's stateJson is 5,269 bytes and the ADVERSARIAL case —
     all 323 continuous params at full %.17g precision, morph field present —
     is 10,774. ADR-160's "~35 KB" was the pessimistic guess; the reservation
     stays at 48 KB anyway, as headroom for the parameter table to keep growing
     without a push ever starting to allocate. The whole ring is 9.6 MB of
     main-thread memory, reserved once at construction; the audio thread never
     touches any of it. Falsifier: undo_check's reservation assertion. */
  static constexpr std::size_t kJsonReserve = 48u * 1024u;
  static constexpr std::size_t kLabelReserve = 96u;

  struct Node
  {
    bool live = false;
    int parent = kNone;
    uint64_t tick = 0;   // shell-supplied monotone counter, NOT a clock
    uint64_t born = 0;   // creation order; redo follows the LARGEST among children
    std::string label;
    std::string json;
  };

  UndoTree()
  {
    for (Node &n : nodes_)
    {
      n.json.reserve(kJsonReserve);
      n.label.reserve(kLabelReserve);
    }
  }

  int capacity() const { return kUndoCap; }
  int size() const { return count_; }
  int current() const { return cur_; }
  bool liveAt(int i) const { return i >= 0 && i < kUndoCap && nodes_[i].live; }
  const Node &node(int i) const { return nodes_[i]; }

  /* Create a child of `current` and stand on it. Returns the new index, or the
     unchanged `current` when the snapshot is identical to the one already
     under foot — a mark that changed nothing is not a state worth an entry
     (ADR-160: the unit is the gesture, and a gesture that ended where it began
     is not an edit). */
  int push(const char *label, const std::string &json, uint64_t tick)
  {
    if (cur_ != kNone && nodes_[cur_].json == json) return cur_;

    const int slot = (int)(next_ % (uint64_t)kUndoCap);
    int parent = cur_;
    if (nodes_[slot].live)
    {
      // Evict the oldest: its children adopt its parent, so a subtree survives
      // its ancestor (and becomes a root when the ancestor was one).
      const int gp = nodes_[slot].parent;
      for (int i = 0; i < kUndoCap; i++)
        if (nodes_[i].live && nodes_[i].parent == slot) nodes_[i].parent = gp;
      if (parent == slot) parent = gp;   // standing on the node being evicted
      nodes_[slot].live = false;
      count_--;
    }

    Node &n = nodes_[slot];
    n.live = true;
    n.parent = parent;
    n.tick = tick;
    n.born = ++births_;
    n.label.assign(label ? label : "");
    n.json.assign(json);
    count_++;
    next_++;
    cur_ = slot;
    return slot;
  }

  // Move to the parent. No-op (returns kNone) at a root or on an empty tree.
  int undo()
  {
    if (cur_ == kNone) return kNone;
    const int p = nodes_[cur_].parent;
    if (p == kNone || !nodes_[p].live) return kNone;
    cur_ = p;
    return p;
  }

  // Move to the most recently CREATED child — the branch last taken from here.
  int redo()
  {
    const int c = newestChild(cur_);
    if (c == kNone) return kNone;
    cur_ = c;
    return c;
  }

  // Navigation only: no node is created, so the next push forks from here.
  bool restore(int i)
  {
    if (!liveAt(i)) return false;
    cur_ = i;
    return true;
  }

  int newestChild(int parent) const
  {
    int best = kNone;
    uint64_t bestBorn = 0;
    for (int i = 0; i < kUndoCap; i++)
      if (nodes_[i].live && nodes_[i].parent == parent && nodes_[i].born > bestBorn)
      {
        best = i;
        bestBorn = nodes_[i].born;
      }
    return best;
  }

  // Depth from the node's root, for the GUI's depth columns. Bounded by the
  // node count so a cycle (impossible by construction) cannot hang the loop.
  int depthOf(int i) const
  {
    int d = 0;
    for (int k = i; liveAt(k) && d < kUndoCap; d++) k = nodes_[k].parent;
    return d > 0 ? d - 1 : 0;
  }

 private:
  Node nodes_[kUndoCap];
  int cur_ = kNone;
  int count_ = 0;
  uint64_t next_ = 0;     // write cursor; next_ % kUndoCap is always the oldest slot
  uint64_t births_ = 0;
};

}   // namespace hypersaw
