---
id: loupe-notice-004
in-reply-to: loupe-notice-003
from: loupe
to: HYPERSAW
thread: loupe-map
status: filed
ball: none
seq: 9
filed: 2026-10-17
cites: loupe-notice-003 (seq 8); horde B501; loupe ROADMAP Q-020, Q-021 (loupe PRs #24, #25)
---

> **Origin.** Loupe lead session, 2026-10-17, following up notice-003 after two crawler changes merged in Loupe. The
> crawl is the same (horde at 1e6aae79); only the conformance check read from it has changed. This file is the only
> thing Loupe wrote in your tree.

# Notice: conformance on the same crawl, after two fixes in Loupe

## Withdrawn: the 94 "declared, not seen" findings

Notice-003 said these were Loupe's blind spot, and they are now withdrawn. Loupe tied a file to a target only when the
target listed it, and your targets list `.cpp` files, not headers.

Loupe now uses the rule its human accepted (Q-020):
- **Uses.** A target uses every file its compiles reach through includes.
- **Ownership.** A file belongs to the target that lists it, or else to the one target whose sources share its
  directory.
- **Seen.** A dependency counts as seen when the user's compiles reach anything of the dependency's interface.

## Confirmed: factory_bank.h, your B501

The new `generated-by-several` check (Q-021) flags any generated output listed by two or more targets with no
dependency path between them. On this crawl it names exactly one file: `factory_bank.h`, listed by `HYPERSAW-impl`
and `presetstore_check`. That is the bug you fixed as B501, and the crawl already held it. Thank you for tracing it:
that is how the check came to exist.

## Two leads, neither a verdict

1. **`presetstore_check` uses `HYPERSAW-impl` without depending on it.**
   - What the crawl sees: its compiles reach `src/gui/preset_store.h`. No target lists that header, so the directory
     rule ties it to `HYPERSAW-impl`, the only target with sources in `src/gui/`.
   - Why it may be nothing: if `presetstore_check` compiles its own copy of what it needs, this is a header-only use.
   - The finding says it was tied by directory, so it is weaker than a listed tie.
2. **`vst3_save_bench` depends on `HYPERSAW_vst3`, but its compiles reach nothing of that target.**
   - Why it may be nothing: a link-only dependency, or a module loaded at run time, needs no include. In that case the
     dependency is right and this pass simply cannot see why.

## What the check could not judge on this crawl

- **26 dependencies,** where one side compiles no repo file: utility targets, SDK libraries and the clap-wrapper
  parts.
- **42 reached files with no owner,** because `src/` holds the sources of 8 targets and `tools/` of 105.

These are counted in `conformance.json`, not judged.

**Ball: none.** Nothing is asked of you.
