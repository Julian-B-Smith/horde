# b241-modulated-readback — a modulated parameter was saved, captured and adopted at its MODULATED value; measured, confirmed, fixed

- **Queue item:** B241 (row carried in PR #737, `lead-records-85`; dispatched by the horde lead). The claim came from the B233 integration playbook (PR #740, `docs/playbooks/integrating-a-source.md` §13 Q4), which read it from code and did not measure it.
- **Why:** ADR-136's comment says readback reports a destination's BASE. But `readParam` looked up the base only after its early returns for routing cells, engine rows and about thirty shell-owned ids. It also keyed that lookup on `d->id`, the BASE def, so oscillator 2's rows asked oscillator 1's question. Every persistence path reads through `readParam`, so a row under a route was persisted at wherever the modulator happened to be.

## Measured first (before any source edit, commit `9532eb1` = origin/main)

`tools/modreadback_check.cpp`. sub.fine is set to base 20. LFO 1 is a 1 Hz square (exactly +1 in the first half-cycle), routed at 0.25 depth, so the applied value is exactly 70. There is a held note, and 20 mod ticks are rendered.

| path | ROUTED | NO ROUTE (must read zero) | detune id 4, ROUTED (positive control, base 0.3) |
|---|---|---|---|
| params_get_value (host) | **70** | 20 | 0.3 |
| state_save chunk | **70** | 20 | 0.3 |
| stateJson (preset save) | **70** | 20 | 0.3 |
| history node (undo `live`) | **70** | 20 | 0.3 |
| corner capture | **70** | 20 | 0.3 |
| reload saved chunk, drop the route: base | **70** | 20 | 0.3 |
| morph-on adoption, first-ever branch | **70** | 20 | 0.3 |
| …then morph off, drop the route: base | **70** | 20 | 0.3 |
| morph-on adoption, B222 editor branch | **70** | 20 | 0.3 |

Controls: the "must move" control confirmed the applied value really is 70 (and 0.55 for detune).

The sweep routed LFO 1 to each parameter the matrix accepts, one at a time:

| family | routable | moved | read back modulated |
|---|---|---|---|
| engine (3000–9999) | 11 | 11 | 11 |
| osc 1 + global (<1000) | 145 | 145 | 78: every one a shell-owned id returned before the old lookup (inertia 11/70, fine 37, bend 38, bend and note laws, step rate and grid, morph X/Y/temp/coupling/glide, FX mixes, the rack's time and delay rows, and crossfade time); the check prints each id |
| osc 2 (1000–2999) | 54 | 54 | 54 (the `d->id` key) |
| routing (≥10000) | 29 | 29 | 29, plus `routing=` and the history node's `routing` key, which read the matrix directly |

**Confirmed.** Morph-on made it worse than a bad save. Adoption wrote the modulated value into the corners, then the field wrote it back as the new BASE. After morph off and removing the route, the knob stayed at 70.

## Fix (`src/hypersaw_clap.cpp`, readback and base paths only; `morphStep` untouched)

1. `readParam`: the `modDestFor` base lookup is now the first branch, keyed on the full `id`. The old lookup is removed.
2. `routingChunk`: reads through `readParam` instead of `getRoutingParam`.
3. The ADR-136 base intercept in `applyParam`. The sweep's second law caught these once (1) had made them visible: a host write under the route must read back what the same write reads with no route.
   - Inertia (11): the intercept stored the TAPERED value. A write of 0.1 became a base of 0.0032. It now stores `inertiaKnob`.
   - Step Grid (148): it is snapped after the intercept, so the intercept now stores the snapped value.
   - Inertia Curve (70): it returned before the intercept, so the next tick undid the write. It now lands its base.

Result: 0 leaked, 0 write-lost, every path reads 20. `modreadback_check` fails on origin/main with 14 failures (172 leaked, 156 write-lost, and the 3 routing-chunk rows) and passes with the fix.

## Human gates checked, none tripped

- **State FORMAT:** unchanged. The same keys and lines are written; only the value of a modulated row changes, from modulated to base.
- **Automation recording:** unchanged. Host out-events echo the editor-authored value (`drainQueue`, `ev.value = m.value`), not `readParam`, and the plugin never requests a value rescan. Only `get_value` changes, from modulated to base, which is what ADR-136 promised. The GUI's knob for a modulated engine, shell or routing row now shows the base with the halo (`modLiveJson`), as swarm rows always did.
- **Presets:** none of the 52 factory JSON files carries a `modRoutes` key, so none could have been saved modulated through a route. Two state fixtures (`chunk-v2-noheader.txt`, `chunk-v2-rev1.txt`) carry `modroutes=0:4:0.5;`, ENV 1 → detune. Id 4 is an instrument-table row that the intercept always covered. No file changed. `bank_check` and `statefix_check` are green.

- **Evidence consulted:** `src/hypersaw_clap.cpp`: `readParam`, `applyParam`'s intercept, `modStep`'s generic-destination loop, `morphCapture`, `morphAdoptUncontested`, the morph-on branch of `applyParam(151)`, `routingChunk`, `state_save`, `stateJson`, `historyJson` and `drainQueue`. Also `tools/lfoenv_check.cpp` and `notefuzz_scaffold.inc` (the rig shape), LIBRARY L0032 and L0063, and the B241 row.
- **Alternatives rejected:**
  - Fixing each persistence caller: there are six-plus call sites and one ordering bug. The next caller would reopen it.
  - Refusing routes to engine, shell and routing rows: that is a behaviour change and a gate.
  - Snapping 148 before the morph hook, as 23 does: it would change what morph corners store, and that is the concurrent `morphStep` work's territory.
- **Verify:** `./verify full` exit 0 at `c9fc74a` (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"c9fc74a"}`). `modreadback_check` PASS (0 failures) inside it.
- **Open questions:**
  - A session saved before this fix with a route on an affected row holds the modulated value as its stored base, and it will load that way. This cannot be detected from the chunk alone.
  - Per-host display: whether a VST3/AU host through clap-wrapper shows a modulated knob moving was not measured. It is not a CLAP-level change.
