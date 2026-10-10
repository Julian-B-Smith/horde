// scenario_floor.h -- the floor under a golden-parity gate's scenario count.
//
// Added under ADR-180 §1 (B455 H1, docs/audits/2026-10-10-repo-audit.md). Included
// by filter_check, notch_check, spectra_check, swarmalator_check, time_check,
// station_check and subosc_check (parity_check cannot include it:
// tools/parity_check.cpp is the h2 lift's pinned source, so its floor lives in
// tools/parity_floor_check.py instead, which also plants the must-fail controls).
//
// WHY. These gates loop over a manifest the golden generator writes, count
// failures, and go red only when a failure was counted. A manifest with no
// scenarios counts none, so a deleted scenario block, or an empty manifest, read
// GREEN ("0 failures") while the gate compared nothing. Each gate pins today's
// scenario count; fewer is a failure.
//
// The floor ratchets: a count above it is allowed and is reported, so the human
// raising a scenario count raises the floor in the same PR. It never goes down
// without a recorded decision (lowering it is a gate-weakening event).
#pragma once

#include <cstdio>

// True when `count` meets `floor`. Prints one line either way; the caller counts a
// false return as a failure of its own.
inline bool scenarioFloorHolds(const char *gate, int count, int floor)
{
  if (count < floor)
  {
    std::printf("FAIL %s: %d scenarios, below the pinned floor of %d (a scenario was dropped "
                "from the generator, or the manifest is empty)\n",
                gate, count, floor);
    return false;
  }
  if (count > floor)
    std::printf("NOTE %s: %d scenarios (floor %d): raise the floor in the same PR\n", gate, count,
                floor);
  else
    std::printf("OK   %s: %d scenarios (floor %d)\n", gate, count, floor);
  return true;
}
