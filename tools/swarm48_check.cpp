/*
 * swarm48_check — src/swarm_core.h against the JS reference at 48 kHz, every scenario reported
 * (ROADMAP B382). The body, and what is checked, is tools/swarm_sr_parity.h; this TU only picks the
 * legacy core (h2_rules_check rule 4: one core per TU). Built like parity_check (-O2).
 * WIRED: ./verify full
 *   node tools/golden/gen_goldens_sr.mjs && build-release/swarm48_check build-golden/sr48000
 */
#include "../src/swarm_core.h"
#define SWARM_NS hypersaw
#define SWARM_TOOL "swarm48_check"
#include "swarm_sr_parity.h"
