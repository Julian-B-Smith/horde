# c2a_remainder (B667, CONTRACT 7 C2a)

Measures what one swarm oscillator costs in the clean-room engine apart from its members' blade reads (the remainder), on the
Glass horde pad translated (PF-1's patch), 8 voices, PF-3's chord, Standard quality. Reported, never a gate; not wired into `./verify`.

    tools/c2a_remainder/build.sh [engine commit]     # default 6cd98b5; set HORDE_C2A_OUT to build outside $TMPDIR
    <out>/build-<commit>/c2a_remainder --count       # instructions and cycles retired: safe to run any time
    <out>/build-<commit>/c2a_remainder --timed       # PF-3's protocol (load guard, calibration, best of 5): quiet window only
    <out>/build-<commit>/c2a_remainder --timed --smoke   # plumbing check, one repeat, NOT a measurement

Cells A to G are in `main.cpp`. Instruction counts come from `proc_pid_rusage` (no root); they are a floor on a time ratio, not
the PF-1 figure. The per-member slope holds the base-wave read AND the member's swarm law; the cells cannot split them.
