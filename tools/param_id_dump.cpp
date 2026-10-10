/*
 * param_id_dump — print every parameter the legacy shell shows a host, as a host
 * sees it (B448 C2, ADR-197 risk row 7: parameter ids append-only).
 *
 * WIRED: ./verify full (tools/param_id_lock_check.py --runtime runs it and
 * compares the dump with tools/param_id_lock.json and with the static extractor
 * that ./verify fast uses).
 *
 * One tab-separated line per parameter, in the enumeration order params_get_info
 * hands a host:  id  name  stepped  automatable  min  max
 * min/max are %.17g, so the text round-trips the double exactly and the lock
 * compares values, not roundings. Defaults are NOT printed: a default is a
 * different contract (a default change is its own ADR plus migration, ADR-197).
 *
 * The source of truth is the public CLAP params extension on the real plugin
 * instance, not the kParams array — the table is file-static, and the routing and
 * engine blocks are built at load time, so only the extension sees all of them.
 * Reuses statefix_common.h's stub host (L0005), as paramclass_check does.
 */
#include <cstdio>

#include "statefix_common.h"

int main()
{
  const clap_plugin_t *p = statefix::makePlugin();
  const clap_plugin_params_t *px = statefix::paramsOf(p);
  const uint32_t n = px->count(p);
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!px->get_info(p, i, &info))
    {
      std::fprintf(stderr, "param_id_dump: get_info(%u) failed with count %u\n", i, n);
      return 2;
    }
    std::printf("%u\t%s\t%d\t%d\t%.17g\t%.17g\n", (unsigned)info.id, info.name,
                (info.flags & CLAP_PARAM_IS_STEPPED) ? 1 : 0,
                (info.flags & CLAP_PARAM_IS_AUTOMATABLE) ? 1 : 0, info.min_value,
                info.max_value);
  }
  p->destroy(p);
  return 0;
}
