# The test plug-in skips a silent engine, says Mindlathe, and labels each build (B624)

**What changed.** `tools/engine-preview/`: the process call skips the engine while it reports silent and nothing arrives (OU-4). The vendor is "Mindlathe", and the CLAP id and bundle id are `com.mindlathe.horde-engine-preview`. Every build carries "engine <commit>, build <n>" in its window, description and log. The smoke test checks that skipped blocks are counted and that the output stays exact zero.

**Why.** The human reported an ambient load of "possibly up to 10%" with nothing sounding. On 1ad270a, a scratch measurement read a silent `process()` at 7 to 13 % of a core per 512-sample block per instance, against 0.1 to 0.4 % for `skip()`. The human also saw "Mind Lathe" under the VST3 while the AU said "Mindlathe". The convention is one word (autonomous doctrine CONVENTIONS, Decision 80), and the lead hadn't read it before the first build. The human also asked that test builds be marked and announced.

**Evidence consulted.** The scratch idle probe on 1ad270a: Trance jitter, Glass horde pad and Crushed bells, with and without transport. The smoke and window tests on the new build (GREEN, 2,007 idle blocks skipped).

**Consequences.** A Live set that holds the old identity won't find the new one: the human re-adds it. The engine's own silent cost is with the builder (batch 3, item 4).

**Verify.** `./verify fast` on this tree; see the PR.
