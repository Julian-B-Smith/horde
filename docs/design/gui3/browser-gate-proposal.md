# A browser gate for GUI 3 (proposal, GB24)

The human asked for this on 2026-11-05 ("Yes, propose it"), after the third report that GUI 3's pictures froze.

## The gap

GUI 3 has about 83 self-checks, and none of them runs in `./verify`. They run only when a page loads in a browser, and no gate loads one:

- `gui3_seam_check` reads the shell's text and runs its pure parts in Node.
- `lab_load_check` loads each lab in a stub that runs no external script.

So two kinds of bug reach main unseen, and both did on 2026-11-05:

- **A failing self-check.** Every check passed when I ran them by hand, but nothing would have stopped a red one from merging.
- **A fault the self-checks cause.** They draw the pictures on a clock 1e7 ms ahead. Every picture's pace kept that as its last draw, so after the checks no picture was due again. The checks themselves all passed, and loading with `?fast` skipped them, which is why hand tests saw motion. Three "fixed" reports were wrong.

## The check, built and not wired

`docs/design/gui3/browser-check-prototype.mjs`, a prototype outside `tools/` until this is ruled (an unwired check under `tools/` would count as weakening the gate set, which is yours to approve):

1. It serves `docs/` read-only and drives a headless Chrome over the DevTools protocol, with Node's own WebSocket and fetch, and no npm module.
2. It loads `gui3.html` without `?fast`, waits out the self-checks, and is RED on any FAILED line or uncaught exception.
3. It opens Osc 1 and compares every picture 1.5 s apart. It's RED unless the phase ring, carpet and voice map moved, since those move every frame by design.
4. **The control:** the same load with the frame loop stopped after the checks must read frozen.
5. **A missing browser is a HOLE** (exit 3), never a pass.

**Measured.**

| Tree | Result |
|---|---|
| This branch (with #1401) | GREEN in 32 s on this Mac: 83 self-checks pass, three pictures move, and the control reads frozen |
| `origin/main` without #1401 | RED: "frozen after the self-checks: Phase ring, Phase carpet, Voice map" |

## The dependency

It needs a headless Chrome. This Mac already has Playwright's (`chromium_headless_shell-1243`, in `~/Library/Caches/ms-playwright`), and `HORDE_CHROME` points it at any other. It needs no npm package. A clean machine or CI gets the browser with `npx playwright install chromium-headless-shell`, a ~90 MB download.

## Options

| | Where it runs | Cost | What it can't catch |
|---|---|---|---|
| **A** (recommended) | `./verify fast`, on any machine with the browser. Where there's none, it prints HOLE and doesn't fail, and the dashboard shows the hole (ADR-197). | about 30 s locally | Nothing on CI until C |
| **B** | `./verify full` only, macOS-local and human-paced | nothing in `fast` | A broken GUI 3 merges unless someone runs `full` |
| **C** | A, plus CI installs the browser so `fast` runs it everywhere | about a minute and a 90 MB download per CI run | Nothing in this list |

**Why A.** It closes the gap where the work happens, on this Mac, at once and for 30 s, and a missing browser stays visible rather than silent. C is A plus one CI step, and it can follow when the CI budget allows.

## Afterwards, if ruled

- The same harness can boot each lab that loads the shell (`sub-noise-lab.html` stayed stuck on its busy line for a day after GUI-35, with no error).
- It can also walk every page once and fail on any view the shell marks failed. That's how Erode took the Dial down.
