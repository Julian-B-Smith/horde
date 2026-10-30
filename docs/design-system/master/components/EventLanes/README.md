# Event lanes

Events are not levels: the trigger's actions, the input that SAMPLE ON TRIGGER samples, and four lanes that show an event turned back into a level.

**The consumer provides**
- `act`, the seven actions on or off: SAMPLE ON TRIGGER, MOD ENV RESTART, LFO RESTART, KURO RESET, ORBITAL KICK, ORBITAL RESET, HOSTED: RANDOMIZE UNLOCKED.
- `inputs [[id, label]]` for "Sample in" (`rand` is the action's own seeded stream), and `shIn`.
- `seed`, plus `onInput(id)`, `onSeed(n)`, `onChange(act)`, `onCommit`.
- Per frame: `push({gate, sh, env}, fired)`, then `draw(info)`, where `info` is the one-line count (`hint`, ellipsis).

**The rule** a continuous source has a value at every sample, and a route scales it. An event is a count at a sample, so it fires an action.
- SAMPLE ON TRIGGER holds its input at the trigger, as a stepped level. "S&H" is the LFO shape's name alone; the two share one hold rule and one draw rule.
- MOD ENV restarts the envelope from the level it has reached and releases when the gate falls. Under FIRST, an overlapping note does not restart it.

**Anatomy**
- Fires: the action pills (toggles, `aria-pressed`, ink when on).
- Sample in: a select. Seed: a mono number field, 84 px.
- A Gate lanes glass with GATE (a level), TRIGGER (ticks), SAMPLE ON TRIGGER (stepped, ±1) and MOD ENV (a level).
- The count line.

**Placeholders**
- HOSTED: RANDOMIZE UNLOCKED is shown but posts nothing: whether a trigger may cross the module boundary is open (the lab's round 2, question 1, with its latency, undo and automation sub-questions).
- Whether the trigger defaults to EVERY or FIRST is open (question 2).
- The action list is the lab's.

**Sources** docs/design/shape-lab-mod.html :570 (the card), :1284 (`ACTIONS`), :2561 (`buildActs`), :2575 (`paintEv`); ADR-266 ruling 6.
