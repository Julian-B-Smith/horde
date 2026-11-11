# b677-unstated-rows — every UNSTATED ROADMAP row gets a status, judgements tagged for review

- **Queue item:** B677 (ADR-394 item 2; the human 2026-11-11: "merged 1497, go ahead with B677"). Opened B678.
- **Why:** B674 left 404 lead rows UNSTATED rather than guess; a queue no tool can read for open work is the audit's finding 6.
- **Evidence consulted:** each row's own text (ROADMAP.md at 129dca9b); `gh pr list` merged (1489), closed-unmerged (8) and open PRs; `git log origin/main --no-merges` (2562 commits).
- **Method:** the 404 rows split into six chunks of about 158k characters (the 16 multi-class rows first, in chunk 1); six Sonnet readers (pinned), one brief: pick one closed-list word per row, the last dated state wins, `plain:true` only when the row's words or one unambiguous merged PR settle it, anything else `plain:false` with its reason. Five readers read every row in full; the chunk-4 reader (B190 to B309, 70 rows) read each row's dated tail and the start of long rows. Verdicts were applied by a script that changes only the leading `UNSTATED` of cell 3 on the named line and refuses any id or line mismatch (0 refused).
- **Lead review:** ten plain verdicts sampled (seed 677) against the row tails, all held (B111, B93, B138, B423, B405, B124, B212, B118, B322, B508). Every judged verdict read by reason (list below); one changed: B246 WONTDO to SUPERSEDED ("moot per B257" is superseded, not a ruled drop). The rest are accepted as the best reading and stay tagged.
- **Alternatives rejected:** leaving judged rows UNSTATED (the queue would still not read); writing judged rows with no mark (ADR-394: a status is read, never guessed, so a guess must be visible as one); ruling the 140 pre-horde-2 rows SUPERSEDED here (that is a scope ruling, the human's: B678).
- **Verify:** see the end.

## Result
404 rows, UNSTATED 0: DONE 192, OPEN 147, BLOCKED 18, PROPOSED 14, PARKED 13, SUPERSEDED 8, IN PROGRESS 6, QUEUED 5, WONTDO 1.
Plain 147; judged 257, written `STATUS · judged (B677) · …`. Confirming a judged row = deleting its tag.

## B678: the 140 pre-horde-2 rows judged live by silence
B1 B2 B3 B6 B7 B8 B9 B10 B11 B19 B20 B21 B22 B24 B25 B26 B15 B42 B70 B73 B75 B78 B80 B81 B87 B88 B90 B91 B96 B97 B98 B100 B112 B113 B128 B136 B137 B143 B144 B145 B147 B147 B152 B163 B164 B165 B166 B167 B168 B169 B170 B187 B189 B193 B213 B216 B219 B220 B221 B218 B217 B215 B214 B223 B231 B242 B243 B244 B245 B247 B248 B250 B251 B239 B230 B229 B254 B256 B257 B258 B259 B260 B264 B272 B273 B275 B278 B279 B284 B206 B205 B204 B201 B200 B199 B196 B197 B194 B190 B185 B180 B179 B175 B173 B101 B103 B109 B108 B82 B83 B85 B86 B68 B66 B67 B65 B63 B62 B57 B56 B55 B53 B46 B45 B41 B39 B288 B289 B290 B31 B30 B29 B303 B304 B37 B35 B34 B32 B33 B17

## Every judged row (id@line: status — the reader's reason)

### DONE (81)
- B52@7305: Latest dated state CLOSED AGAIN 2026-08-27, briefs delivered; standing recurring condition so judgement.
- B351@7391: Merged #853; follow-ups (regenerate P3 report, T1) listed without stated ownership.
- B364@7404: Merged #858; default-style question left open.
- B460@7481: D8 spreadRamp merged #1087, flag off by default; human's second listen and default choice left open.
- B18@7055: (a) ratified, (b) measured in #394.
- B43@7068: Design recorded; undo history built as B84/ADR-160, PRs #551/#560/#563.
- B74@7075: v3 ADR-144 shipped as MAIN visualizer; deferred extras named.
- B76@7077: Batched frame call built (commit fc481807, e63f3613); push option left open.
- B79@7080: Executed via ADR-146 (#505); human screenshot confirming exit criterion pending.
- B89@7085: Phases 1-2d merged (#602,#629,#654,#660,#662), ADR-176 ratified; migration phase 3 not stated.
- B94@7090: Measured and rulings ratified (damping floor); implementation of relabel not stated.
- B126@7112: Lab, regulators, profile, render wins merged; golden not cut, ORBITAL not on 1.0 roster.
- B127@7113: Placement ruled C; spun up as Sluice; intake record only.
- B132@7118: Standard ruled and applied #579; more presets owed later.
- B142@7128: Built and merged #627; only a one-line comment follow-up named.
- B146@7133: bassMonoPos built #636 and second source built #646, both merged; status cell is stale (says Src 2 blocked).
- B150@7137: Built #650 merged; output pole deliberately left (ADR-177 s2) as a separate ruling.
- B153@7140: Layers 1-2 merged (#661); port moved to B162 (phase 1 #678). Station out of 1.0.
- B155@7142: Lab, spec, BUMP, promotion, R7 merged (#643,#657,#664); port as source row belongs to later rows.
- B155@7143: Earlier duplicate of 7142; lab+spec draft merged #643.
- B157@7145: Built #659 merged; WKWebView confirmation by human still owed.
- B160@7148: Built #677 merged; several open questions for human remain (spring etc).
- B222@7179: Built and merged #732 after critic rework; some rulings still owed as notes.
- B234@7189: Lab round 2 merged; open human questions (listen switch, role set) remain.
- B235@7190: Waypoints lab merged; open human questions on law/caps; GUI-20 later carried waypoints into device.
- B236@7191: Cost bench built and merged with verdict; open items are rulings (scope, regression bar).
- B237@7192: Lab round 3 merged; naming and law choices left open for human.
- B238@7206: Placeholder for incoming prototype; it arrived as SCALPEL, ingested; integration continues under ADR-184/B252.
- B263@7218: Lab merged #753; model accepted; lab acceptance not individually verified.
- B266@7221: Fuzz built and merged #758; open human choices on I7/orders remain.
- B269@7224: Round 3 merged; open human questions remain.
- B253@7243: Lab built #747; open questions on gain/latch/verify remain.
- B207@7244: Lab built and merged; open human questions remain.
- B208@7245: Lab built and merged; open human questions remain.
- B209@7246: Lab built and merged; open human questions remain.
- B210@7247: Lab built and merged; open human questions remain.
- B211@7248: Lab built and merged; open human questions remain.
- B171@7273: Built #686; follow-ups (unruled LFO phase/reseed items) listed
- B172@7274: Phases 1 and 2 built (#685, #690); morphIds ruling owed
- B102@7276: Split into B129 and B130; both merged
- B69@7291: Increments 1-6 shipped; header also says gate proposed
- B49@7307: FX slot morph landed as ADR-124 #461; FOUNDATIONS brief answered
- B47@7309: Resolved in mechanism; corner diff still worth a look
- B40@7315: Bottom modulator bank shipped in B584 slice 3, inferred
- B287@7318: Phases A (#771) and B (#775) merged; latency note open
- B293@7327: Lab round built; open questions listed
- B294@7328: Round 3 built; open questions listed
- B295@7329: Round 4 built; open questions listed
- B299@7333: Round 6 built; open questions listed
- B301@7335: Merged; open modifier questions listed
- B305@7339: ADR-186 ratified, horde 2 proceeded; discussion row
- B309@7343: Round 7 built; open items listed
- B310@7344: #799 merged; row notes steal-click not measured, a minor open caveat.
- B312@7346: Tool merged #804; lab-side loading and corners category remain stated open parts.
- B325@7364: Audit landed in #829 (B323+B325); row text still says the per-preset audit continues.
- B327@7366: Lineup ruled and amended (ADR-190 A6); ADR-387 (wavetables replace blades for 1.0) may supersede, not stated in row.
- B330@7369: Archive ruling recorded in PARKED 23; lab markers done via B407; could be read PARKED.
- B335@7375: Merged #835; engine only, voiceEnv law awaits ratification.
- B347@7387: Human's answers recorded and relayed; some items provisional or later corrected by ADR-188.
- B350@7390: #846 closed unmerged after red hold; THRESH v2 content apparently landed via #853 (depends on #846); not confirmed.
- B407@7428: #897 merged; regression reverted in #905, one lab left Unsorted pending human re-pin.
- B414@7435: Research report delivered; distilled docs/research summary still to follow the human's read.
- B422@7443: Brief answered and ratified; ci.yml paths-ignore edit still waits for loupe P6; ball loupe.
- B435@7456: Contract merged #918, approved; notices and mixer binding (B402) follow.
- B438@7459: Approved and Bulwark answered; master strip build goes to B402.
- B442@7463: Ruled ADR-169 A3, spec amended; replies/FYI notices were still owed at last note.
- B443@7464: Ruled ADR-192 and phrase alignment; spec amendment, intent-slots lab and phrase list noted as open.
- B445@7466: Flip merged #984; small open items (digest headers, patch-space os).
- B456@7477: Built, PR #1034 merged 2026-10-10 with acceptance evidence; one stated open item (auval on test AU needs install with human's say-so).
- B458@7479: Preview built, installed, human said it loads and plays; later rebuilds repeatedly built-not-installed awaiting go; judged as ongoing artefact. Acceptance met but row still evolves.
- B499@7520: Decision row ruled option (a) (ADR-231), rule in spec OU-2 applied at bank stage; option (b) split to B503. Application to bank not shown finished.
- B511@7532: Ruled 4 % (ADR-237 item 6); acceptance also asks bench cases and module notices, not shown done.
- B544@7555: Built and published as design system v19; awaiting the human's look; open placeholders listed.
- B545@7556: Built and published v21; awaiting human's look; open items listed.
- B548@7559: Built, published v25/v26; awaiting the human's look.
- B549@7560: Built, published v27; awaiting the human's look; scale waits on B402.
- B561@7572: PR #1232 merged 2026-10-27 delivering the check; slot rows stay with B398 and a Scape PR #82 re-run is noted as follow-up.
- B566@7577: PR #1243 merged 2026-10-28 meeting criteria 1-8; five rulings left for the human's ears.
- B621@7670: Ruled 2026-11-04, blocker goes in (ADR-365, round 58); spec change applied by ruling, build not shown.
- B628@7677: Bridge #1414 and page #1415 merged and installed; loads follow-up is its own PR; human's playing in Live not confirmed.
- B393@7738: Roster ruled and spun out into sibling projects; OTT lab and Kuramoto chorus work moved elsewhere (B400, B423).

### OPEN (136)
- B1@7043: No landing in merge record; row says recommended next fold.
- B2@7044: Labs done not folded; may be overtaken by hosted-module rack, unconfirmed.
- B6@7049: Two labs built; SPECTRA and quantum morph parts not tracked later.
- B7@7050: Standing convention backlog; no end state.
- B8@7051: Unblocked, not built per row; no merge found.
- B9@7052: Request only; no landing found.
- B10@7053: Deferred until interface settles; no ruling naming it parked.
- B11@7054: Increments 1,2 shipped; GUI and B20 tiers remain.
- B19@7056: Core shipped; shell integration remaining per row; later glide fold unclear.
- B20@7057: Oscillator tier shipped; corner/patch tier wiring deferred.
- B21@7058: Lab-tested; tempo sync and C++ fold remaining.
- B22@7059: Wants an ADR before building; none found.
- B24@7061: Increments 1,2 shipped; per-osc pan law unruled remains.
- B25@7062: Macro not built per row; only scaling rules recorded.
- B26@7063: Design only; B70 relates; no landing found.
- B15@7067: Gate change awaiting human call; no ruling found.
- B42@7069: Discussion, not committed; no ruling found.
- B70@7071: Depth modulation filed; no landing found (later modulation spec work).
- B73@7074: Increment A shipped; B and C remaining; Echo became standard delay later.
- B75@7076: Investigation not run to a decision; gated on human VST judgement.
- B78@7079: Deletion of legacy bundles is the human's; ADR-145 fixed install only.
- B80@7081: Trivial mods shipped; two structural builds and additions not recorded done.
- B81@7082: Inc 1 and inc 2 slice 1 shipped; shell integration, filter env, FILT page remain.
- B87@7083: Mechanics and choice filed (#507); no build found.
- B88@7084: Vision, not decomposed.
- B90@7086: Rate mode ruled; builds not found.
- B91@7087: Roadmap only; no build found.
- B96@7092: Not built per merge record.
- B97@7093: Rides B89 classification; no build found.
- B100@7095: Items 1,2,4 shipped (#535); item 3 two golden sets remains.
- B112@7098: Gate edit human-gated; nothrow probe fix not found in tools.
- B113@7099: Gate edit human-gated; not found landed.
- B128@7114: Scope only; partly answered for added envelopes by ADR-349.
- B136@7122: Ratified ADR-169 A4 roles retired; waits on B89 classification PR; C++ parity check and brief not shown built.
- B143@7129: Workshop/demo sheet; human picks design; no build found in log.
- B144@7130: Design stated, candidate list first; no landing found.
- B147@7132: Layers 1-2 built (#634,#635,#649 merged); layer 3 optimise remains. Duplicate id: earlier copy of row at 7134.
- B147@7134: Earlier duplicate of 7132 text (asks ruling on which checks join verify full); layer 3 remains.
- B152@7139: Layers 1-2 merged (#639); layer 3 port waits on rack module interface.
- B163@7151: Scout for auditor; no resolving commit found.
- B164@7152: (a) built #681 merged; (b) per-macro spring/polarity not built, waits B170.
- B166@7154: Rename/gain wanted; no landing found in merged PRs or commits.
- B168@7156: Small, bundle with B164/B170; double-click reset later landed in GUI rows but not tied to this row.
- B170@7158: Build batch described, acceptance to be written; no landing found.
- B189@7163: Reported not fixed; needs human RT-discipline decision.
- B193@7165: Routing not in stateJson; B222 put routing in history only, literal fix refused as preset-format change.
- B216@7171: Not measured, no check or fix found.
- B219@7172: (1) fix wanted now; (2)-(4) fold into B213 Phase 3; none landed.
- B220@7173: Fix wanted; no landing found.
- B221@7174: Delete or correct dead code; human-gated; not done.
- B217@7176: Rulings owed per parameter; none found.
- B215@7177: Parity gap and golden not yet done; not urgent while E1 parked.
- B214@7178: Comment correction and decision pending; no landing found.
- B223@7180: Designed in B227 lab (#731 merged) but port to gui2 not done.
- B231@7186: Defect and fix wanted; no PR or landing note; state-format change awaits human ruling.
- B242@7195: Wanted coverage extension with no landing note; no PR found.
- B243@7196: Gate widening and doc corrections wanted; no PR found; later work unknown.
- B244@7197: GUI fix wanted; no PR found; later work unknown.
- B245@7198: Wanted GUI treatment; no landing note or PR found.
- B247@7200: Fix wanted per B241 ruling; no PR or landing note found.
- B248@7201: Low-severity fix wanted, tied to B239; no landing note.
- B250@7203: Fix needs a revision-3 law or amendment; no landing note.
- B251@7204: Check and landing-site decision wanted; no landing note.
- B239@7205: Control not built; no PR found; later GUI work unknown.
- B230@7207: Latent wrong law; no fix landing note found.
- B229@7208: Redesign scoped with open Q2-Q4; not dispatched; later envelope rulings not checked.
- B254@7209: Measurement and check wanted; no landing note found.
- B256@7211: CI gap fix wanted; no PR found; later work unknown.
- B257@7212: Sequenced plan: bank built after engine settles; completion unknown.
- B258@7213: Discussion owed before any lab; no human answer recorded (may be absorbed by B263).
- B259@7214: Wanted source, lands with source-cap growth; no landing note.
- B260@7215: MIDI sidechain part deferred after 1.0 (ADR-237); global-scale part status unknown.
- B264@7219: Source wanted, "linked" undefined; no landing note.
- B272@7227: Law-as-setting design; GUI-20 commit mentions B272 but no engine landing confirmed.
- B273@7228: Sluice notices landed as ongoing exchange; no end state; network not integrated.
- B275@7230: Phase 0 done, decision (b) ruled; later phases not shown; programme row.
- B206@7249: Decision wanted on block present from construction; no landing note.
- B205@7250: Fix refused pending a critic and design question; no landing note.
- B204@7251: Measured, candidates ruled out; remaining measurement not taken.
- B201@7252: Ruling wanted on morph slot-change path; no landing note.
- B200@7253: Human rules once measurement exists; no landing note; ADR-180 may have settled it.
- B199@7254: Measurement wanted, not shown built; later work unknown.
- B196@7256: Sub joining bend lanes planned; no PR found; later work unknown.
- B197@7257: Lab-first drive proposed; no landing note.
- B194@7258: Parameter-table question unresolved; no landing note.
- B190@7260: Layer 1 built (#712); layers 2 and 3 await human nod
- B185@7262: Nothing ported; later wavetable engine (ADR-387/389) may have overtaken it
- B179@7266: Rename dispatch waited on #695; no build PR found; slot add offered
- B101@7275: Engineering slice merged #532; LICENSE, tag, Apple credentials still human acts
- B103@7277: Slice shipped #533; clips, video, listening note still owed by human
- B109@7278: Finding feeding goldens-v2 workshop; no fix landed
- B108@7284: Gate edit, human-gated; no fix PR found
- B82@7286: Build wanted; no per-note mod PR found
- B83@7287: Meters and headroom wanted; no PR found
- B85@7289: Do when touching pad code; later morph-pad-everywhere ADR may cover
- B86@7290: Fix is an append plus ADR, awaiting ruling; none found
- B68@7292: Delay built (ADR-142) but the A/B outcome is unrecorded
- B66@7293: Probe described; no fix PR found
- B67@7294: Markers now, lanes later; no PR found
- B65@7296: Rule ratified but ADOPT build not shown landed
- B62@7298: Plan for morph timing; build not shown
- B57@7300: Revived into mod lab B392; this row not closed
- B55@7302: Human-driven session not shown held
- B53@7303: Adapter not started
- B46@7310: Ruling needed; option 1 withdrawn #483; headroom later ADR-221 unclear link
- B45@7311: Standing roster list; filters not all built
- B41@7312: Audit done; budget programme standing
- B39@7314: Feasible; no build found
- B288@7319: Migration ruled; build not dispatched
- B289@7320: SVF port not dispatched; B287 now merged
- B31@7322: Proposal; no build found
- B29@7324: Candidates named; nothing built
- B303@7337: Architecture exists; increment 2 not dispatched
- B304@7338: Ruled; build not shown
- B308@7342: Follow-ups partly done (H3 ADR-325), rest not dispatched
- B316@7350: P1-P3 merged (#814); P4 fit and P5 random button/fuzz mode not shown as built in the row.
- B37@7356: Increments 0-5 shipped; page compositions, bundled fonts, section 7 still remaining. Legacy shell frozen (ADR-186) may supersede; not stated.
- B35@7358: Wanted randomize/initialize corner; no landing found in merges; ADR-160 says it becomes optional once undo exists.
- B34@7359: Pilot per-note K scope; no merge found naming it; general scope waits for matrix (B82 per-note modulation exists).
- B32@7360: Candidate lab fix (glissando mode) described; no merged work or ruling found.
- B33@7372: Filter page/routing ownership wanted; gate lifted, no landing found; B81 FILT may overlap.
- B448@7469: Phase 1 and Wave 1 landed, phase 2 ruled, later packages P7-P19 unstarted and parked behind B458; multi-wave programme with no end state.
- B449@7470: Investigation and rulings done; R1-R11 are to go into horde 2 GUI/shell, which is not built. Row never says implemented.
- B453@7474: Section 1 ruled and answered; section 2 stays open as input; Sluice preset ingest waits on rack and its PR.
- B565@7576: Both briefs answered, but acceptance needs titles in horde's labs at S4; ball HYPERSAW at S4.
- B599@7648: Ruled in part (ADR-344); teed direct rule in mono still open; built when Arp's core lands.
- B611@7659: No status word; acceptance (a check via the relay) not shown delivered.
- B612@7660: No status word; ADR-359 ruled but Layer-0 checks and host test not shown done.
- B614@7663: Fix ruled (ADR-362, round 55) for the builder's next batch; test owed under B618; not shown built.
- B616@7665: Schedule of needs for spec round; B631 takes list to human; no completion stated.
- B617@7666: GUI half landed; charter line waits on human; lead's check status not stated (maybe built).
- B618@7667: Test author reopened to write owed tests; no completion stated.
- B620@7669: Lead to read hits and tell human; no completion stated; tool not to be run until then.
- B519@7724: Standing rule in force with no end; a recurring duty, not a deliverable.
- B390@7735: Post-1.0 integration, but seam room required now in horde 2; ongoing constraint.
- B17@7749: Early design ask with no status text or dated notes; no merge evidence found (not checked in PR logs).

### BLOCKED (14)
- B3@7045: Says deliberately blocked on rotor axes; likely stale, no later evidence.
- B98@7094: Waits on rename and upstream sequencing; migrate decision at open.
- B137@7123: Became its own project (ADR-170 A2); horde's port waits on consuming Maw's module ABI; no horde port PR found.
- B145@7131: Lives in resolver B89 phase 2c; no build found.
- B165@7153: Sequenced after B170, which is unbuilt.
- B167@7155: Needs B170 binding editor.
- B169@7157: Same batch as B167/B170.
- B187@7161: One-line fix is an interface change awaiting human; lead recommends; not applied.
- B213@7169: Phase 0 inventory merged (#728); D1-D6 await human rulings; later phases not started.
- B218@7175: To be ruled with B213 D6; unruled.
- B175@7270: Awaits human ruling on remap; no ruling or build found
- B63@7297: Told not to start before B50 slot-contract ruling; OTT later conditional
- B30@7323: Awaits human ruling on drag mode
- B311@7345: Triaged; ingest awaits human approval and D15/D17 rulings; no v1.2 files in reference/scalpel. Later engine work may have made it moot.

### IN PROGRESS (5)
- B302@7336: Gate amended: GUI 3 starts now as B584; completion unknown
- B331@7370: Order S0-S6 approved (ADR-292); stages proceed one at a time, S4 pending; standing sequence with no end.
- B446@7467: P0, Tier A, Waves 1-3 landed; further tiers and Tier C items continue; last note 2026-10-09.
- B609@7658: No status word; ADR-356/358 ruled, builder takes the batch, second table round pending; no completion shown.
- B627@7676: Ruled, round 60 committed (6c2b5aee); test author's cpu check change still pending ('with the test author').

### QUEUED (5)
- B173@7271: Records say queued; no build PR found; acceptance written
- B503@7524: Planned lab, ordered after B402's horde 2 port; not started; no row says B402 finished.
- B504@7525: Planned after engine stage 2 (accepted 2026-10-21 as stage 6 later); poly glide deferred by human; no start recorded.
- B658@7695: ADR-388 ruled; next is spec rework and generator spec, after B656/B657; no start recorded.
- B662@7703: Carried into spec rework with B658; B666 amendment merged may partly cover it but row not updated.

### PROPOSED (6)
- B278@7233: Row says PROPOSED but classed a defect 2026-09-27; later fix not checked.
- B279@7234: Row says PROPOSED; #1032 host tempo guard is different; later fix not checked.
- B284@7239: Row says PROPOSED; #996 NaN latch is different; later fix not checked.
- B180@7265: Assessment and ruling request, not built; no later PR found
- B56@7301: Manual agent not yet started; awaits human
- B290@7321: Ruled, then re-ruling owed on C6 geometry

### PARKED (6)
- B13@7066: Granular module out of 1.0 by ADR-190; row itself gated on sibling.
- B44@7070: STATION out of 1.0 since ADR-182 (charter), integration deferred.
- B162@7150: Phase 1 built (#678,#683) but phase 2 port deferred past 1.0 by ADR-182; Station out of 1.0.
- B249@7202: Fix deferred until SPECTRA returns; SPECTRA parked, row not itself ruled.
- B542@7553: Post-1.0 design recorded; room left in 1.0. Deferred by ruling though not literally a parking word.
- B559@7570: Planned after 1.0; deferred by ruling (ADR-293), only room left now.

### SUPERSEDED (4)
- B228@7185: Row says "Likely" superseded by B238; hedged, not ruled; never dispatched.
- B246@7199: Moot per B257 (lead, B677 review: superseded by B257, not a ruled drop). Moot per B257; WONTDO is closest word, not a formal drop.
- B332@7371: Phase 1b replaced by clean composed engine (B385) and the port retired by ADR-223/B494 per B331; row's own header said stays open.
- B399@7744: Discharged by B514 (Sub and noise workshop, DONE); one first question never asked as such.

## Multi-class rows (read first)
B365, B466, B467, B493, B514, B526, B537, B562, B522, B396 DONE (plain); B487, B491 PARKED (plain, ADR-223); B52, B351, B364, B460 DONE (judged: a standing condition or an open follow-up).
