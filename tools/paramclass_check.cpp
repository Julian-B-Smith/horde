/*
 * paramclass_check — B89 phase 1's oracle: every parameter carries a class
 * (morphable / structural / device) and the classification is consistent with
 * the structures that already exist in the shell.
 *
 * STANDALONE AND UNWIRED, by the charter's standing rule: wiring a gate into
 * ./verify is the human's decision, proposed in the PR that adds the gate
 * (ADR-171 is the route the nine wired checks took).
 *
 * It PRINTS the full table (id, key, name, class, reason) before it asserts,
 * because the classification is a judgement the human reviews — a green exit
 * code says the rule is self-consistent, not that the rule is right.
 *
 *   T1  every parameter is classified: 245 base rows, every twin, no -1.
 *   T2  no morphIds member is device. This is the load-bearing cross-check:
 *       paramClassOf never reads morphIds and morphInit never reads the class,
 *       so the two lists are independently authored. Derive one from the other
 *       and this assertion certifies nothing (L0032).
 *   T2c THE CONTROL. T2's scan, run against a lookup that lies about ONE
 *       morph-field id, must REPORT the violation. Without it a scan that
 *       silently examines nothing passes exactly as loudly as a correct one.
 *   T3  ids 151-158 (the morph controls) are device.
 *   T4  per-osc twins (id, id+1000) share a class. True by construction today
 *       — they share one ParamDef — so this pins the construction rather than
 *       discovering it, and fails the day an override is keyed on a twin id.
 *       ROUTING IDS ARE NOT TWINS and are excluded: the crosspoint block is a
 *       positional namespace of its own (`decodeRoutingId`, hypersaw_clap.cpp),
 *       so `id - 1000` under 10000+ names no parameter at all — 14 of the 24
 *       compared against a non-parameter and reported a class disagreement
 *       that was an artefact of the subtraction, not a classification fault.
 *   T4r THE REFUSAL (L0036): a global has no +1000 twin, so its twin is not a
 *       parameter and has no class at all (-1), not a defaulted one.
 *   T5  the B49 atomic FX groups are internally consistent: slot type is
 *       structural, amount and tone are morphable. A group whose members
 *       resolved in different tiers could not flip as one unit.
 *   T6  the printed table lists each id exactly once — no id carries two
 *       classes. LIMIT: this sees the table, not kParamClassOverrides, so a
 *       duplicated override id (shadowed by the first match) reads as
 *       consistent here. Named, not silently uncovered.
 *   T7  the class NUMBERS are anchored, one id per class, so a reordered enum
 *       cannot quietly re-caption this whole table.
 *
 * Reuses tools/statefix_common.h — one stub host, one boot — rather than
 * growing another copy of the CLAP scaffold.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "statefix_common.h"

using namespace statefix;

extern "C" int hypersaw_debug_paramclass(uint32_t id, const char **keyOut,
                                         const char **reasonOut);
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *p, int k);

namespace
{

int g_failures = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_failures++;
}

/* Captions for the exported class numbers. The NUMBER is the contract (see the
   enum's note in hypersaw_clap.cpp); T7 anchors one id per class so this map
   cannot drift away from it unnoticed. */
const char *className(int c)
{
  switch (c)
  {
    case 0: return "morphable";
    case 1: return "structural";
    case 2: return "device";
    default: return "UNCLASSIFIED";
  }
}

/* The live morph field, read the way morphlayout_check reads it: the corner
   JSON is keyed by id in morphIds order, so its keys ARE the field. */
std::vector<uint32_t> morphFieldIds(const clap_plugin_t *p)
{
  std::vector<uint32_t> ids;
  const char *j = hypersaw_debug_cornervals(p, 0);
  for (const char *c = j; (c = std::strchr(c, '"')) != nullptr;)
  {
    const char *e = std::strchr(c + 1, '"');
    if (!e) break;
    ids.push_back((uint32_t)std::strtoul(std::string(c + 1, e).c_str(), nullptr, 10));
    c = e + 1;
  }
  return ids;
}

/* T2's scan, parameterised by the lookup so T2c can feed it a liar. Returns
   the number of morph-field members that came back device, and names the
   first one. */
using ClassFn = int (*)(uint32_t);
int classOf(uint32_t id) { return hypersaw_debug_paramclass(id, nullptr, nullptr); }
int classOfLying(uint32_t id)
{
  if (id == 33) return 2;   // id 33 (Note Lag) is in the field and is morphable
  return classOf(id);
}
int deviceMembersOfField(const std::vector<uint32_t> &ids, ClassFn f, uint32_t *firstBad)
{
  int n = 0;
  for (uint32_t id : ids)
    if (f(id) == 2)
    {
      if (n == 0 && firstBad) *firstBad = id;
      n++;
    }
  return n;
}

}   // namespace

int main()
{
  const clap_plugin_t *p = makePlugin();
  const clap_plugin_params_t *params = paramsOf(p);

  /* ---- the table ---- */
  std::printf("PARAMETER CLASSIFICATION (B89 phase 1 — QM-4 §3.2)\n");
  std::printf("%-6s %-16s %-28s %-11s %s\n", "id", "key", "name", "class", "reason");
  std::printf("%-6s %-16s %-28s %-11s %s\n", "------", "----------------",
              "----------------------------", "-----------", "------");

  std::map<uint32_t, int> tableClass;   // id -> class, as printed
  int counts[3] = {0, 0, 0};            // every host-exposed id, twins included
  int baseCounts[3] = {0, 0, 0};        // the 245 kParams rows — the definition
  int unclassified = 0, duplicated = 0, baseRows = 0;
  std::vector<uint32_t> allIds;

  const uint32_t n = params->count(p);
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!params->get_info(p, i, &info)) continue;
    allIds.push_back(info.id);
    const char *key = "?";
    const char *reason = "?";
    const int c = hypersaw_debug_paramclass(info.id, &key, &reason);
    if (c < 0) unclassified++;
    else counts[c]++;
    if (info.id >= 1000) continue;   // the 245 kParams rows ARE the definition
    baseRows++;
    if (c >= 0) baseCounts[c]++;
    if (tableClass.count(info.id)) duplicated++;
    tableClass[info.id] = c;
    std::printf("%-6u %-16s %-28s %-11s %s\n", (unsigned)info.id, key, info.name,
                className(c), reason);
  }

  const int morphable = counts[0], structural = counts[1], device = counts[2];
  std::printf("\nbase rows printed: %d   host-exposed ids: %u\n", baseRows, n);
  std::printf("kParams rows (the definition) — morphable %d  structural %d  device %d\n",
              baseCounts[0], baseCounts[1], baseCounts[2]);
  std::printf("all host-exposed ids (twins too) — morphable %d  structural %d  device %d\n\n",
              morphable, structural, device);

  /* ---- the assertions ---- */
  // 245 since B89 phase 2b appended intentBus (id 266). The pin is the point: it is
  // meant to be moved deliberately, by the change that adds the row.
  check(baseRows == 245, "T1a the table is the 245 frozen kParams rows");
  check(unclassified == 0, "T1b every host-exposed id carries a class (no -1)");

  const std::vector<uint32_t> field = morphFieldIds(p);
  uint32_t bad = 0;
  const int devInField = deviceMembersOfField(field, classOf, &bad);
  check(!field.empty(), "T2a the morph field was read (non-empty)");
  check(devInField == 0,
        devInField == 0
            ? std::string("T2b no morphIds member is device")
            : "T2b no morphIds member is device — FIRST OFFENDER id " + std::to_string(bad));

  uint32_t ctlBad = 0;
  const int ctlHits = deviceMembersOfField(field, classOfLying, &ctlBad);
  check(ctlHits == 1 && ctlBad == 33,
        "T2c CONTROL: the same scan over a lookup that calls id 33 device reports exactly "
        "that one violation");

  bool morphCtlDevice = true;
  for (uint32_t id = 151; id <= 158; id++) morphCtlDevice = morphCtlDevice && classOf(id) == 2;
  check(morphCtlDevice, "T3 ids 151-158 (morph position + controls) are device");

  /* The ROUTING block's base (`kRoutingIdBase` in the shell). Restated here
     rather than included because this tool links the entry, not the shell's
     internals; it is a bound, not a second decoder — every cell's membership
     still comes from the host's own id list. */
  constexpr uint32_t kRoutingIdBase = 10000;
  int twinsChecked = 0;
  bool twinsAgree = true;
  for (uint32_t id : allIds)
  {
    if (id < 1000 || id >= kRoutingIdBase) continue;   // routing ids are not twins
    twinsChecked++;
    if (classOf(id) != classOf(id - 1000)) twinsAgree = false;
  }
  check(twinsChecked > 0 && twinsAgree,
        "T4 every per-osc twin shares its base id's class (" + std::to_string(twinsChecked) +
            " twins; true by construction — they share one ParamDef)");
  check(hypersaw_debug_paramclass(1151, nullptr, nullptr) == -1,
        "T4r REFUSAL: a global's +1000 twin is not a parameter and gets no class");

  bool fxOk = true;
  for (int slot = 0; slot < 4; slot++)
  {
    fxOk = fxOk && classOf((uint32_t)(57 + 2 * slot)) == 1;   // type: structural
    fxOk = fxOk && classOf((uint32_t)(58 + 2 * slot)) == 0;   // amount: morphable
    fxOk = fxOk && classOf((uint32_t)(96 + slot)) == 0;       // tone: morphable
  }
  check(fxOk, "T5 every B49 FX group is type=structural, amount=morphable, tone=morphable");

  check(duplicated == 0, "T6 no id appears twice in the table");

  check(classOf(4) == 0 && classOf(1) == 1 && classOf(152) == 2,
        "T7 class numbers anchored: 4 Detune=morphable(0), 1 Voices=structural(1), "
        "152 Morph X=device(2)");

  p->destroy(p);
  std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
