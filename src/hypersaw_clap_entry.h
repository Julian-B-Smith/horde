/*
 * hypersaw_clap_entry.h — extern entry methods implemented by the impl static
 * library (src/hypersaw_clap.cpp) and consumed by the exported clap_entry in
 * hypersaw_clap_entry.cpp, per the clap-first idiom (see
 * libs/clap-wrapper/tests/clap-first-example).
 */
#pragma once

#include <cstdint>

#include <clap/clap.h>

extern "C"
{
  bool hypersaw_entry_init(const char *plugin_path);
  void hypersaw_entry_deinit(void);
  const void *hypersaw_entry_get_factory(const char *factory_id);

  /* TEST HOOK — not part of the CLAP surface and not called by the exported
     entry. It lets a headless oracle trigger the same forensic dump the GUI
     panic button triggers, so what is under test is the REAL capture path and
     not a reimplementation of it (a test that rebuilds the mechanism it is
     checking spans the wrong layer — L0031). Returns a pointer valid until the
     next call, or NULL if the dump could not be written. */
  const char *hypersaw_test_dump_forensics(const clap_plugin_t *p, const char *why);

  /* TEST HOOK — runs the GUI panic button's exact path (capture, then clear) so
     an oracle can assert the ORDERING, which is the part that makes the capture
     worth having. Returns the dump path, or NULL. */
  const char *hypersaw_test_panic(const clap_plugin_t *p);

  /* TEST HOOK — the host-misconfiguration hint, empty when there is nothing to
     say. Exposed so the DETECTION can be gated; the GUI presentation cannot be. */
  const char *hypersaw_test_host_hint(const clap_plugin_t *p);

  /* TEST HOOKS — B448 B1, the last-line output guard's latch. The first four
     read or clear it (reset is the ONLY thing that clears the latch); the
     fifth runs the shipped guard (Plugin::guardOutput, the call process() ends
     with) on the caller's buffers, zeroing non-finite samples and returning how
     many it replaced — the one way a headless probe can plant a NaN, since the
     event-boundary guards stop real ones upstream. */
  uint64_t hypersaw_test_nonfinite_samples(const clap_plugin_t *p);
  uint64_t hypersaw_test_nonfinite_blocks(const clap_plugin_t *p);
  bool hypersaw_test_nonfinite_latched(const clap_plugin_t *p);
  void hypersaw_test_nonfinite_reset(const clap_plugin_t *p);
  uint32_t hypersaw_test_guard_output(const clap_plugin_t *p, float *l, float *r, uint32_t n);

  /* TEST HOOKS — note-bookkeeping introspection for the FOUNDATIONS
     note-lifecycle conformance suite (their R5). Read-only windows onto the tag
     table and voice gate, plus ONE shipped mutator (retireTag). Deliberately
     NOT a second note-on path: the suite's notes arrive as real CLAP events
     through the real process(), so what is certified is the shell we ship. */
  int hypersaw_test_poly(void);
  bool hypersaw_test_tag_at(const clap_plugin_t *p, int slot, int32_t *note_id, int16_t *port,
                            int16_t *channel, int16_t *key);
  bool hypersaw_test_retire_slot(const clap_plugin_t *p, int slot, int32_t *note_id, int16_t *port,
                                 int16_t *channel, int16_t *key);
  bool hypersaw_test_slot_gated(const clap_plugin_t *p, int slot);

  /* TEST HOOKS — ADR-136 mod-matrix route surface. The GUI reaches routes
     through the webview bridge, which no headless oracle can drive; these call
     the SAME shell functions the bridge calls (modAddRoute / removeRoute), so
     what is under test is the shipped path and not a reimplementation. The
     third returns the shell's last APPLIED value for a destination — the one
     number that distinguishes "modulating" from "readback lying". */
  bool hypersaw_test_mod_add(const clap_plugin_t *p, uint32_t srcSlot, uint32_t destId);
  void hypersaw_test_mod_remove(const clap_plugin_t *p, int idx);
  double hypersaw_test_mod_applied(const clap_plugin_t *p, uint32_t destId);
}
