# REFLECTIONS — dated, prunable at /wakeup

- [2026-10-29] The clean-room thread most likely to be dropped: the builder's soak on 5442272 (started 2026-10-29 02:11, stopped for a machine shutdown; restart it in full) and the lead's clean-export rerun of a554c31 and 5442272, held until the soak finishes so two heavy jobs never overlap. Rerun from `git archive` in the engine repo, cmake -j4 under nice; expect only 3.b (Crushed burst) and 9.b1 (four as-found presets) red.
