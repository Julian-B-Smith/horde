# 2026-11-06 — the human's rulings folded into the modulation spec draft (B630, ADR-371)

**What changed.**
- **`docs/proposals/modulation-spec-draft.md`**: every "QUESTION MQn" in the clause tables replaced by the ruled behaviour, citing ADR-371 in the row's source column (MD-13, MD-14, MD-19, MD-23, MD-26, MD-28, MD-29, MD-31, MD-34, MD-35, MD-39, MD-41 to MD-46, MD-48, MD-50, the LFO source row). Section 9 keeps all 18 questions and marks each "Ruled (ADR-371)". Three clauses added, numbered on from MD-59: **MD-60** (an LFO's envelope mode, instead of new envelopes), **MD-61** (the morph position is a target only from an LFO set to global), **MD-62** (MPE is a per-note source; bend range is B504's open setting; release velocity is no non-MPE source). Contradictions 2 (mapping scope) and 4 (polarity) marked resolved; short ADR-371 notes on 3 (clamp) and 6 (modulator settings as destinations). **MD-48 rewritten to per corner in 1.0**, reversing ADR-261 r3 and ADR-266 r4, with B396's five questions listed open. MD-33 rewritten (loops can now form), MD-49, MD-51, MD-55, MD-58, MD-15 and the section-7 rows brought into line. Header status, counts (62 clauses, 11 technical gaps) updated. A new gap row **T11** collects what ADR-371 does not decide.
- MD-59 and GN-40 untouched, still open for the morph round.
- Nothing else. ROADMAP, DECISIONS and `specs/` untouched.

**Why.** ADR-371 (the human's poll answers to MQ1 to MQ18, relayed by the lead 2026-11-06): the draft's questions are ruled, so the clauses must say what was chosen and the record must show what was asked and answered.

**Evidence consulted.** The lead's brief carrying ADR-371's rulings verbatim; the draft itself; `traces/2026-11-06-modulation-spec-draft.md` for the trace format. ADR-371 itself is the lead's to write (not read here).

**Gaps found, not guessed (all in T11 or the header).** MQ6's second half (a start-phase knob) is not answered by "locks to song position": MD-44 is left open. Also unruled: a new route's starting curve, envelope mode's end value and retrigger, the starting polarity of sources MQ13 does not name, and whether the morph position is one of the resolver-owned keys MD-16 refuses (MD-16 now carries an exception for MD-61, a reading).

**Verify.** `nice -n 10 ./verify fast` in the worktree; result in the report and the PR.
