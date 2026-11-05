# 2026-11-05 — The pictures froze after the self-checks (a GUI-33 bug)

**What changed.** `GUI3Shell.picturePace` resets when the clock goes backward. The shell's pace check adds that case.

**Why.** The human: "Everything is still frozen on all the osc views", after GUI-42 merged. The self-checks set a fake clock at 1e7 ms and more, every pace kept it as its last draw, and the real clock afterwards was never far enough past it.

**Evidence.** Headless Chrome over CDP, full load with self-checks (83 of 83), then two snapshots of every visible canvas 1.5 s apart. With the fix: the ring, spectrum, carpet and voice map change. On main without it: none of them change (the control). The browser pane couldn't show this: it was hidden, and a hidden pane gets no animation frames.

**Verify.** `./verify fast` on the commit this trace ships in.

**A gap, not closed here.** Nothing in `./verify` loads GUI 3 in a real browser and waits past its self-checks, which is how this shipped twice. A headless-browser gate would need a dependency, which is the human's call.
