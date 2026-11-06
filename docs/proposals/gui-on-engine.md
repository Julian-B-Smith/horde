# GUI 3 on the clean-room engine, in the test plug-in (B628)

> **Origin.** horde lead session, 2026-11-05. The human asked: "would it make sense during the build to wire the actual engine into a standalone mock-up of the GUI for quicker testing?" The lead recommended the test plug-in's window over a browser. The human: "yes, plan the GUI-on-engine build with the GUI session". The GUI session answered the lead's six questions the same day; their answers are cited here as "the GUI session".

**Status: RULED and building (2026-11-05).** The human ruled all three decisions in section 6. The GUI session signed section 3 with eight changes, all of them below. Last verified 2026-11-05 against `origin/main` after #1403, and the builder's `1ad270a`.

## 1. What it is

GUI 3's page runs in the test plug-in's window (`tools/engine-preview`, B624), wired to the clean-room engine. It also builds as a standalone app (clap-wrapper's standalone format), so it can be tried without Live.

**What the human gets:**
- **The real engine and the real controls,** where the plain AppKit window has today's 49 sliders.
- **Pictures** drawn from the engine's own snapshot (RC-8) instead of the page's sample swarm.
- **CPU** read in a real host.

**What we get:** the seam between the page and the engine, tested now and not at cutover. This bridge is most of horde 2's shell bridge (B398), so it's planned as the shell's first slice, not as a throwaway. The AppKit window stays as a fallback, behind a build switch, until the page has been tried in Live.

## 2. Who owns what

| Part | Owner | Where |
|---|---|---|
| The page, its transport, `ReplayTransport`, the inert state's look, the browser and seam tests | GUI session | `docs/design/gui3/`, `tools/labharness/` |
| The C++ bridge: host the page, map keys to ids, apply messages, publish frames | lead | `tools/engine-preview/` |
| The schema both sides speak | both, signed in this file (section 3) | here |
| What the page expects from the engine | GUI session | `docs/design/gui3/ENGINE-NEEDS.md` (GN-n) |
| Engine additions the page needs (section 5) | builder, by spec round, on the human's ruling | clean room |

## 3. The seam

The page's existing transport (`docs/design/gui3/engine-stub.js`, `GUI3Engine.TRANSPORT`) has two methods, `send(msg)` and `onSnapshot(fn)`. The bridge implements both over the web view's bind and evaluate calls, so nothing above the transport changes (the GUI session).

**Messages from the page.** The stub's messages: `set {seq, values:{key:v}}`, `gesture {seq, key, phase}`, `morph {seq, settings}`, `position {seq, x, y}`, `modes {seq, modes, counts}`, `rack {seq, values}`, `midi {seq, values}`, `clear {seq, id}`. Plus three new ones:
- `want {seq, osc: [1, 2], members: bool, cycles: bool}`. It names the parts of RC-8 the page wants, never its pictures: the page maps pictures to parts on its side, so a new picture never changes the schema.
- `preset {seq, index}`.
- `plugin {seq, voices}`: the test plug-in's own Voices setting, which sits outside §5. The page gives it one small control.

**Every `seq` is acknowledged:** applied, or held and dropped. None is ever an error. The stub throws on `morph`, `position`, `modes`, `rack` and `midi` when nothing stands behind them; the bridge acknowledges and drops them instead.

**Snapshots to the page come in two parts.** Every 1/30 s a frame carries `ack` (the last `seq` applied: ADR-319's mirror and confirm) and `observe`. `params`, `events` and `presets: {names, index}` ride only when something changed. `state` rides only after a load or a save, never per frame: the stub's SyncTransport puts saveState in every snapshot, and the bridge mustn't copy that.

**`observe`'s decoded shape, pinned** so the bridge, `ReplayTransport` and the fixture can't drift apart. Frames are binary or batched (GN-46), and decoding lives inside the transport. Above it, the page sees:
`{sounding, voices: [{level, env: [4 stages], notes: {key, vel, gate}, osc: [{R, psi, frame, phases: Float32Array, levels: Float32Array, blades: [{pos, width}]}]}], meters: {osc1, osc2, sub, noise, out: {peak, clip}}, latches: {clip, nonFinite}, counters: {IF-9's}}`.
- Notes come from the plug-in's own events.
- GN-47's feed is derived from this on the page side (`device.js`).
- `phases` and `levels` ride only when `want.members` is set, and member cycles only when `want.cycles` is (GN-41, not in the first slice).
- The GUI session estimates about 2.5 KB a frame with members, about 75 KB/s at 30 fps for 4 voices, 2 oscillators and 32 members.

**The manifest hash.** The page and the bridge each compute `manifest_hash`, and they hash the same thing:
- **Which rows:** every row whose target is the engine and that isn't reserved.
- **Each row's line:** `key`, `class`, `law`, `lo`, `hi`, `steps`, `default`, joined by tabs, ending in a newline.
  - `class` is `morphable`, `structural` or `device`.
  - `law` is `lin`, `log`, `pow`, `linwraps`, `logoffzone`, `db` or `step`.
  - `steps` is the option count, or 0.
  - `default` is in the row's unit, and a step row's default is its option index.
  - Numbers are written as C's `%.17g` does.
- **The hash:** lines sorted by key, bytewise; the hash is SHA-256 over the UTF-8 text.

`shell-data.js` carries it, and the bridge computes it from the engine's manifest at load. On a mismatch the window shows a banner and a per-key diff, never a silent play.

**Rules the bridge enforces, each with a test:**
1. **Keys, never ids.** The page names rows by key. At load, the bridge maps each key to its id from the engine's manifest. **A key the manifest lacks is reported, never sent** (the GUI session's risk a). The bridge checks the page's spec revision against the engine's at load, and shows a mismatch rather than playing on.
2. **By target.** Only rows whose target is `engine` go out as Param events. `rack`, `midi` and `resolver` rows never reach the engine (risk b), and neither does a `gesture` on one of their keys. The page holds their values: the bridge omits those keys from its snapshots, and since `device.js` walks only the keys a snapshot carries, nothing reverts.
3. **Engine display text.** Displayed values come from the engine's law, or a test holds the page's local `lawX`/display equal to it (risk c).
4. **The real-time rule.** Nothing on the audio thread allocates or blocks. Messages reach audio through a lock-free queue, as the plug-in's window edits already do. Frames are published from the engine's snapshot, which any thread may read (RC-8).
5. **Silence.** The plug-in's silent skip (OU-4) stays. A frame during silence carries the last snapshot and "nothing sounding".

## 4. What works in the first slice

**Served by the engine as it is** (the GUI session's mapping):
- GN-32 and GN-45, from RC-8.
- GN-15 in part: source and output meters.
- GN-13 and GN-47: notes and gates, from the plug-in's own note events plus RC-8's levels and stages.
- GN-4: the summed output, tapped in the plug-in.
- GN-46: the bridge itself.
- GN-9 in part: the 83-preset bank lists and loads.

**Inert, because the engine has none of it yet:**
- the morph: GN-6, 7, 8, 11, 18, 19, 21, 28, 29, 35, 36, 40, 51;
- modulation: GN-5, 10, 14, 20, 23, 27, 30, 31, 39, 49;
- filters and FX: GN-1, 2, 3, 16, 17, 33, 50.

**Not the engine's:** GN-12, 24, 25, 26, 44.

## 5. Engine work the page would like, none of it in the first slice

These widen RC-8 or add taps, so each is a spec change: a spec round and the human's word, delivered to both clean-room repositories.
- GN-41: member cycles.
- GN-42: member pan and detune.
- GN-43: a per-oscillator spectrum tap.
- GN-48: the sub after its chain.

Until then, those pictures stay on the page's sample, with its SAMPLE tag. They're asked for only after the CPU pass, and their own cost is measured.

## 6. Decisions for the human

All three were ruled by the human on 2026-11-05.
1. **How a control looks when this engine can't act on it: "Dimmed, still visible".** The state comes from each row's `target` and the engine's manifest, and the GUI session builds it from the existing idle styling, with a small "not in this engine yet" note. A more designed look can be labbed later, for the design master.
2. **What the plug-in saves: "Current settings only".** Every engine setting changed, plus the preset, through the existing small chunk. Corners, routes and undo (GN-34, with GN-22, 37 and 38) wait for the morph and for B398's state schema, so this slice makes no decision that B398 must undo.
3. **Timing: "build it now, I want it for sound testing".**

## 7. Tests

| Test | Owner | What must fail it |
|---|---|---|
| The seam's message schema, at Layer-0 speed (node) | GUI session | a message with an unknown type; a `set` naming a key outside §5 |
| The page under `ReplayTransport` from recorded frames: pictures move, meters read the recorded peaks | GUI session | a frozen replay |
| The bridge's key map against the engine's manifest | lead | a page key the manifest lacks must be reported and not sent |
| The bridge on a host: the smoke test drives the plug-in, a `set` reaches the engine, `ack` returns its `seq` | lead | a `set` dropped on the audio path |
| Frames during silence | lead | a frame that claims sound while the engine is silent |
| The seam: a held message (`rack`, `morph`) is acknowledged | GUI session | a held message left unacknowledged |
| Frames carry only what `want` asked for | lead | a frame carrying member cycles that `want` didn't ask for |
| The page's display text equals the engine's, for a sample of v per row, from the fixture | GUI session | one row's display changed |

**The recorded frames** are dumped from the engine at a pinned commit and committed as a fixture, so the page's side runs in CI while the engine can't (B568). That fixture is new, and **editing it later is a human gate under the charter**, like goldens.

## 8. Not in this plan

The morph, modulation, filters and FX. B398's identity, state schema and migrations. GUI 3's design rulings. Any change to the clean room's inputs: the bridge reads the engine's committed API, from an archive of a named commit, as the test plug-in already does.

**FX device (ADR-254).** The bridge is per instrument. The FX device would host the same page with a rack-only target set, so the bridge's by-target split (rule 2) is the seam it would reuse.
