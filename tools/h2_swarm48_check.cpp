/*
 * h2_swarm48_check — the lifted swarm core (h2/cores/swarm/) against the JS reference at 48 kHz, every
 * scenario reported (ROADMAP B382). The body, and what is checked, is tools/swarm_sr_parity.h; this TU
 * only picks the h2 copy (h2_rules_check rule 4: one core per TU), built as every h2 target is
 * (-O2 -ffp-contract=off, rule 2).
 * WIRED: ./verify full
 *   node tools/golden/gen_goldens_sr.mjs && build-release/h2_swarm48_check build-golden/sr48000
 */
#include "../h2/cores/swarm/swarm_core.h"
#define SWARM_NS horde2::swarm::hypersaw
#define SWARM_TOOL "h2_swarm48_check"
#include "swarm_sr_parity.h"
