/*
 * hypersaw_debug.h — the ONE declaration of the headless probe surface
 * (`hypersaw_debug_*`) that src/hypersaw_clap.cpp defines and tools/ calls.
 *
 * A NEW EXPORT IS DECLARED HERE FIRST; A TOOL THAT DECLARES ONE BY HAND IS A
 * DEFECT. That is not style: `extern "C"` suppresses name mangling, so a tool
 * whose hand-written prototype drifts from the definition LINKS CLEANLY and
 * reads garbage at runtime — the failure no oracle can see, because the oracle
 * is the thing holding the wrong prototype (repo audit 2026-09-19, H2; the
 * same shape ./verify:352 already names for accessors). Single-sourcing the
 * prototypes converts that silent UB into a compile error, and including this
 * header from the DEFINING translation unit makes a drifted definition an
 * error there too.
 *
 * These are not part of the CLAP surface and no host calls them: they are the
 * same calls the GUI's webview bridge makes, exported so a headless probe can
 * make them without a webview. The `hypersaw_test_*` hooks live separately in
 * src/hypersaw_clap_entry.h (they belong to the exported entry's contract).
 *
 * The "owner" named on each line is the check that would fail if the export
 * lied. AN EXPORT WITH NO OWNER IS DELETED, NOT MARKED: three (panic,
 * modpolarity, set_engine_revision) sat here marked NO CALLER until
 * 2026-09-19, which is a debug surface maintained for nobody — every one
 * of them still compiled, still linked, and still had to be kept honest
 * against a shell method it was the only reader of. Restore one from git
 * history the day a probe needs it.
 */
#pragma once

#include <cstdint>

#include <clap/clap.h>

extern "C"
{
  /* --- Parameter classification (B89 phase 1) --------------------------- */

  /* Reads the static ParamClass of `id` (no handle: class is definition, not
     instance state); -1 if not a parameter. Owner: paramclass_check. */
  int hypersaw_debug_paramclass(uint32_t id, const char **keyOut, const char **reasonOut);

  /* --- State and the preset JSON path ----------------------------------- */

  /* Writes the shell's stateJson() into `out`. Owner: state_check, bank_check,
     corner_probe, gen_factory_bank, statefix_common.h. */
  void hypersaw_debug_state(const clap_plugin_t *p, char *out, uint32_t cap);

  /* B181 note 3: the SUB's published cycle — exactly what the GUI bridge's
     hzGetSubWave hands the wave display. Headless, so an oracle can ask
     whether the PICTURE is the engine's current configuration rather than a
     stale or default one (the law itself is shared by construction: the shell
     calls SubOscCore::shapeAt, which is what render() calls per sample).
     Owner: subosc_check. */
  void hypersaw_debug_subwave(const clap_plugin_t *p, char *out, uint32_t cap);

  /* B177 note: the LFOs' published cycles — exactly what the GUI bridge's
     hzGetLfoCycle hands the MOD page. The law is shared by construction (the
     shell calls its own `lfoShapeAt`, which is what the mod tick calls), so
     what this door is FOR is the other half: an oracle can walk the live
     source slot phase by phase and assert the picture is the sequence being
     generated, not merely a function that resembles it. Owner: lfoenv_check. */
  void hypersaw_debug_lfocycle(const clap_plugin_t *p, char *out, uint32_t cap);

  /* The GUI load button's applyStateJson, headless; the apply is QUEUED.
     Owner: state_check, preset_probe, morphlayout_check, penv_check,
     bank_check, statefix_common.h. */
  bool hypersaw_debug_apply(const clap_plugin_t *p, const char *json);

  /* B174: the same load, NAMED — one call that applies the patch and records
     which preset it is, so no history snapshot can land between the two.
     Owner: undo_check. */
  bool hypersaw_debug_apply_named(const clap_plugin_t *p, const char *json, const char *name);

  /* B174: true iff applying `json` would leave the patch exactly as it is —
     the GUI's edited-since-load asterisk, inverted. Owner: undo_check. */
  bool hypersaw_debug_presetmatches(const clap_plugin_t *p, const char *json);

  /* Reads the engine revision counter a state load bumps. Owner: state_check,
     statefix_common.h. */
  int hypersaw_debug_engine_revision(const clap_plugin_t *p);


  /* --- Morph corners, owners, exemptions --------------------------------- */

  /* The GUI's capture verb (bake the live values into corner `k`), headless.
     Owner: intent_check. */
  void hypersaw_debug_capture(const clap_plugin_t *p, int k);

  /* Corner `k`'s slot->value JSON; also publishes the slot->id order the
     intent exports are addressed by. Owner: state_check, intent_check,
     corner_probe, morphlayout_check, gen_factory_bank, paramclass_check,
     preset_probe. */
  const char *hypersaw_debug_cornervals(const clap_plugin_t *p, int k);

  /* Applies a corner's values as JSON (the GUI's corner-edit path).
     Owner: morphlayout_check, intent_check, gen_factory_bank. */
  bool hypersaw_debug_cornerapply(const clap_plugin_t *p, int k, const char *json);

  /* True iff corner `k` already equals `json` — the bank's idempotence check.
     Owner: morphlayout_check, bank_check. */
  bool hypersaw_debug_cornermatches(const clap_plugin_t *p, int k, const char *json);

  /* The four corner names as JSON. Owner: morphlayout_check. */
  const char *hypersaw_debug_cornernames(const clap_plugin_t *p);

  /* Renames corner `k`. Owner: morphlayout_check, gen_factory_bank. */
  void hypersaw_debug_cornername(const clap_plugin_t *p, int k, const char *n);

  /* Per-parameter morph ownership as JSON — the SAME export the GUI's colour
     coding reads (ADR-110), so a check's report is the player's report.
     Owner: intent_check, routing_check, undo_check, corner_probe. */
  const char *hypersaw_debug_ownersjson(const clap_plugin_t *p);

  /* True iff `id` is exempt from the morph toggle. Owner: state_check,
     intent_check, corner_probe. */
  bool hypersaw_debug_exempt(const clap_plugin_t *p, uint32_t id);

  /* The exemption set as JSON. Owner: state_check, corner_probe. */
  const char *hypersaw_debug_exemptjson(const clap_plugin_t *p);

  /* --- Mod matrix (ADR-136) ---------------------------------------------- */

  /* The GUI's own view of the route table as JSON, including each route's
     declared polarity. Owner: polarity_check, intent_check. */
  const char *hypersaw_debug_modroutes(const clap_plugin_t *p);

  /* One SOURCE SLOT's live value — `mod.src[slot]`, the number every route on
     that slot multiplies by depth. NaN for a slot outside [0, kMaxSources): a
     probe reading past the table must not silently read 0, which is also the
     value an unassigned slot legitimately holds. Owner: lfoenv_check. */
  double hypersaw_debug_modsrc(const clap_plugin_t *p, int slot);


  /* --- Routing matrix (ADR-088; `from` is a ROW, not a source index) ------ */

  /* The live cell amount processBlock multiplies by — not a readParam
     round-trip (L0032). Owner: routing_check. */
  double hypersaw_debug_routing(const clap_plugin_t *p, int from, int to);

  /* The live cell's enable bit. Owner: routing_check. */
  bool hypersaw_debug_routing_on(const clap_plugin_t *p, int from, int to);

  /* Destination slot `to`'s output amount. Owner: routing_check. */
  double hypersaw_debug_routing_out(const clap_plugin_t *p, int to);

  /* Destination slot `to`'s initial (unmodulated) value. Owner: routing_check. */
  double hypersaw_debug_routing_init(const clap_plugin_t *p, int to);

  /* Source row `from`'s emitted value. Owner: routing_check. */
  double hypersaw_debug_routing_srcout(const clap_plugin_t *p, int from);

  /* The `id,kind,from,to;` table the oracle addresses the cells by; no handle,
     the table is definition. Owner: routing_check. */
  const char *hypersaw_debug_routing_ids(void);

  /* --- Intent bus resolver (B89 phase 2c; addressed by morphIds SLOT) ----- */

  /* The resolver's TARGET for `slot` — the value intentApply hands the glide.
     Owner: intent_check. */
  double hypersaw_debug_intent_final(const clap_plugin_t *p, int slot);

  /* Which intent owns `slot`, -1 if none. Owner: intent_check. */
  int hypersaw_debug_intent_owner(const clap_plugin_t *p, int slot);

  /* Corner `corner`'s binding of `intent` to `slot`. Owner: intent_check. */
  double hypersaw_debug_intent_bind(const clap_plugin_t *p, int corner, int intent, int slot);

  /* Corner `corner`'s declared range for `slot`. Owner: intent_check. */
  bool hypersaw_debug_intent_range(const clap_plugin_t *p, int corner, int slot, double *lo,
                                   double *hi);

  /* The intent captions as a JSON array. Owner: intent_check. */
  const char *hypersaw_debug_intent_names(const clap_plugin_t *p);

  /* Corner `corner`'s home coordinates (an atom of its own, plan R10).
     Owner: intent_check. */
  bool hypersaw_debug_intent_home(const clap_plugin_t *p, int corner, double *x, double *y);

  /* Plants the pad once, returning the corner planted. Owner: intent_check. */
  int hypersaw_debug_intent_plant(const clap_plugin_t *p);

  /* Commits the plant (optionally forcing a corner). Owner: intent_check. */
  int hypersaw_debug_intent_commit(const clap_plugin_t *p, int forceCorner);

  /* Breaks `slot` out of its atom. Owner: intent_check. */
  bool hypersaw_debug_intent_break_atom(const clap_plugin_t *p, int slot);

  /* The performance pad's puck position (device state with no parameter).
     Owner: intent_check. */
  void hypersaw_debug_intent_puck(const clap_plugin_t *p, double *x, double *y);

  /* Which corner owns the home atom, -1 if none. Owner: intent_check. */
  int hypersaw_debug_intent_homeowner(const clap_plugin_t *p);

  /* The EDITOR'S gesture verb (Plugin::guiGesture), headless — the only way to
     observe "still held". Owner: intent_check. */
  void hypersaw_debug_gesture(const clap_plugin_t *p, uint32_t id, bool begin);

  /* --- Undo tree (B84; op-dispatched on purpose — one door, the GUI's) ---- */

  /* ops: service | tree | json <i> | label <i> | parent <i> | size | current |
     restore <i> | undo | redo | mark <i>. Owner: undo_check, intent_check. */
  const char *hypersaw_debug_undo(const clap_plugin_t *p, const char *op, int arg);

  /* --- Note law, bend, pitch envelope ------------------------------------ */

  /* The wheel lane's EMITTED value (what updateTuneAll multiplies by).
     Owner: anchor_check, preset_probe. */
  double hypersaw_debug_pitchbend(const clap_plugin_t *p);

  /* The bend's anchor key. Owner: anchor_check, preset_probe. */
  int hypersaw_debug_lastnotekey(const clap_plugin_t *p);

  /* Oscillator 0's note law as its core holds it, as text. Owner: preset_probe. */
  void hypersaw_debug_notelaw(const clap_plugin_t *p, char *out, uint32_t cap);

  /* ENV 2's GLOBAL projection (ADR-162), its winning slot's stage, and the
     global pitch lane. Owner: penv_check. */
  void hypersaw_debug_penv(const clap_plugin_t *p, double *env2, double *stage, double *pitchSm);

  /* ONE slot's pitch envelope and the semitones it contributes to that voice's
     noteTune. Owner: penv_check. */
  void hypersaw_debug_penv_slot(const clap_plugin_t *p, int slot, double *level, double *stage,
                                double *semis);

  /* --- Swarm observables and the voice table ----------------------------- */

  /* Oscillator `osc`'s order parameters read from cores[osc].focus() — the
     same place publishViz copies from, so GUI and bank can never disagree.
     All zero when nothing sounds. Owner: bank_check. */
  void hypersaw_debug_viz(const clap_plugin_t *p, int osc, double *R, double *RA, double *RB,
                          int *n, double *RN);

  /* Up to `cap` voice phases (0..1) of oscillator `osc`; returns the count
     written — lets a check compute gap uniformity, which R and RN cannot see.
     Owner: bank_check. */
  int hypersaw_debug_phases(const clap_plugin_t *p, int osc, double *out, int cap);

  /* The sounding voices as `slot,midi,gate,f0,f0cur,glideActive,noteTune;` —
     read POSITIONALLY by preset_probe, so fields are appended, never inserted.
     Owner: preset_probe, bank_check, penv_check. */
  void hypersaw_debug_voices(const clap_plugin_t *p, char *out, uint32_t cap);

}
