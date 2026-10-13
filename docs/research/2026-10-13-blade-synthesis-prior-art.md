# Blade synthesis and its neighbours: a prior-art and patent read

**Date** 2026-10-13 · **Origin** a research sub-agent of the horde lead session ·
**Trigger** ROADMAP B472 and ADR-219 (the human's ruling that 1.0 ships under the GPL,
possibly with pay-what-you-want builds) · **Scope** one document; no code and no other
document is changed.

> **This is not legal advice.** It was written by an AI research assistant, not a lawyer,
> from public sources read in one afternoon. It does not say that anything infringes, or
> does not infringe, any patent or trademark. Three things are kept apart throughout and
> labelled: **[source]** what a document says; **[inference]** what the author concludes
> from it; **[attorney]** what only a patent attorney can decide. A *patent claim* is one
> numbered sentence at the end of a patent that defines what the owner can stop others
> doing; an *independent* claim stands alone, a *dependent* claim narrows another.

## Summary

1. **MOK holds one US patent on the Waverazor oscillator** (US 10,262,646, granted 2019, runs to January 2038 if its fees are paid; no other country). Its claims describe several source oscillators feeding a pitch-locked switch that splices their outputs into one cycle. SCALPEL is built differently (one phase per swarm member, and a window that adds a gated transformation), but its simplest setting produces a similar result, so nine of ten claim elements come out "unclear": they turn on what the claim's words mean.
2. **A second live US patent turned up that nobody asked about, and it is the closer one:** Korg's US 7,589,272 (to July 2028) on removing aliasing by adding a small zero-mean, odd-symmetric correction pulse around each waveform jump. On a plain reading its elements appear present in the PolyBLEP correction that SCALPEL uses, and that the legacy engine already in this public tree uses too.
3. **Everything else in the neighbourhood is old or academic:** hard sync, Casio's windowed resonance (1982 priority), FM (1977), VOSIM (1978), FOF (1984), pulsar synthesis (2001), windowed sync in mainstream soft synths (2014). No live patent was found for the swarm, the formant engine, the PM engine or the gravity modulator within the searches listed in §2.G. For the morph grid, one expired Apple patent and one live patent written around motorised analog hardware turned up; its proper search did not run.
4. **How sure this is:** a keyword, assignee and inventor search of one database (Google Patents) plus web search, US-centred, and cut short when the database began refusing requests. It is a first read, not a clearance. Neither patent's fee status could be confirmed at the USPTO.
5. **Next:** (a) take both patents and the tables in §2.C to a patent attorney before any build is offered, Korg first; (b) check both patents' fee status at the USPTO, which is free: MOK's next fee falls due on 16 October 2026; (c) approve or amend the `docs/PRIOR-ART.md` addition in §3, and stop describing the technique as "Waverazor-style" in user-facing text.

## 1. What SCALPEL does

The comparison in §2.C is made against this list. References are to
`specs/SPEC-SCALPEL.md` (§) and `reference/scalpel/prototype/razor-core.js` (`:line`), the
parity oracle. Terms: a *phase* is a position in the waveform's cycle, from 0 to 1; a
*phase accumulator* is the counter that advances it each sample.

| # | Feature | Where |
|---|---|---|
| F1 | **One phase accumulator per swarm member.** A voice is 1 to 9 members in the bench (up to 32 per oscillator in horde, ADR-184 H6). Each member has one phase φ; the *base wave* (sine, triangle, saw, reverse saw, square) is a closed-form function of φ. | §2, §3; `:13-29`, `:174`, `:657` |
| F2 | **A blade is a window over the cycle**: a centre `c` and a width `w` as fractions of the cycle. A sample is "inside" when `frac(φ − (c − w/2)) < w`. A blade narrower than 0.004 is bypassed. | §4.1; `:176-179` |
| F3 | **Inside the window the output is `base + g·depth·(hot − base)`**: a crossfade from the base toward a transformed signal `hot`, by a depth of 0 to 1 and an edge gate `g`. Outside, the output is the base. A blade's contribution is this difference, and contributions add, so blades may overlap. | §2; `:237`, `:255-279` |
| F4 | **The blade's own wave has no accumulator of its own in sync mode.** Its phase is `hp = kk·e`, a multiple (the *cut rate*) of the position inside the window, so it restarts at every blade entry by construction. Cut rate units: per cycle, per blade (`k/w`), or Hz (`k_Hz/fᵢ`). Every rate is capped below the oversampled Nyquist limit. | §4.1; `:180-187`, `:825-826` |
| F5 | **Seven modes** set what `hot` is: *sync* (the blade wave, restarted at entry); *FM reset* and *FM free* (the blade wave phase- or pitch-modulated; in *free* the modulator runs on its own phase, `:659`); *noise* (sample-and-hold random values); *fold* (a sine waveshaper applied to the base); *ring* (base times blade wave); *crush* (the base held in box-averaged steps). | §4.2, §4.4; `:185-226` |
| F6 | **Edges** are hard (an abrupt change) or soft (a raised-cosine fade over a set fraction of each end). Crush uses a step slew instead. | §4.1; `:230-236`, `:240-246` |
| F7 | **Mirror**: *reflect* plays the blade as a palindrome; *twin −/+* adds the same blade half a cycle later, inverted or not. | §4.5; `:182`, `:259-264` |
| F8 | **Two blades.** Blade 2 has its own settings, each defaulting to "same as blade 1". By default the two contributions add. In *serial* interplay the upper blade transforms `base + λ·(lower blade)`; *collision* speeds the upper blade's wave or deepens its FM while the two windows overlap. | §4.6, §13; `:248-253`, `:281-303`, `:681-689` |
| F9 | **Where a blade sits** is `position + rotation + member rotation + spread offset + (swarm frame ? member's lead on the swarm : 0)`. *Rotation* moves the blade round the cycle at up to ±4 Hz, independent of pitch. | §4.1, §6; `:784-803`, `:821-822` |
| F10 | **The swarm.** Member phases are coupled by a Kuramoto law (each member's rate is nudged by the group's mean phase, updated every 32 samples), so member frequencies are not constant. Position, cut rate, width, depth, shape, FM index and rotation can each be spread across members by a law (gradient, random, drift, alternate, swarm) and, for cut rate, a rule (harmonic, undertone, octaves, and so on). In horde 2 the member trajectories come from horde's own swarm law (ROADMAP B298, ADR-187). | §3, §5; `:79-143`, `:453-481` |
| F11 | **Cross-member modulation and feedback**: a member's blade-wave phase is pushed by its neighbour's, or its own, previous output sample. | §6; `:809-815` |
| F12 | **Blade envelopes** scale cut rate and width over the note. | §6; `:763-776` |
| F13 | **Anti-aliasing.** *Aliasing* is the false tones a digital waveform makes when it jumps between two samples. SCALPEL locates each jump inside the sample (base-wave jumps, blade and twin edges, blade-wave wraps, crush steps), measures its height `h`, and adds a two-sample *PolyBLEP* correction: `−h/2·τ²` to the current sample and `+h/2·(1 − τ)²` to the previous one, where τ is how far through the sample the jump fell. It also oversamples 2× and caps blade-wave bandwidth. horde 2 can swap the blade-wave corrections for a different method (ADAA, ADR-189 D1; flags default off). | §7; `:520-540`, `:610-653`, `:692-722`, `:866-869` |
| F14 | **DC correction**: the blade's mean contribution per cycle is estimated and subtracted, with an 8 Hz blocking filter as a fallback. | §7; `:542-608`, `:841-859`, `:874-877` |

The same two-sample PolyBLEP is in the engines already in this tree:
`src/swarm_core.h:1064`, `src/subosc_core.h:454-460`, `src/station_core.h:626`, and
`reference/swarmsaw.html` (cited at `src/subosc_core.h:449-450`). That matters for §2.B.

## 2. Findings

### A. Waverazor and MOK

**The product, in its maker's words.** [source] MOK (Media Overkill, LLC, of Washington
state) sells Waverazor, a software synthesizer with three wave-slicing oscillators; each
can "slice together up to 16 separate waveforms"
([mok.com/waverazor.php](https://mok.com/waverazor.php)), with tuning, level, phase and DC
offset set per segment, hard sync resettable per segment (MOK calls it Multi-Sync), and a
per-segment amplitude effect (Mutant AM). It was first shown in January 2017
([Sonic State, NAMM 2017](https://sonicstate.com/news/2017/01/20/namm-2017-media-overkill-waverazor-from-taiho-yamada/))
and is on sale today. A Eurorack hardware module with 1010music followed in 2019
([Synthtopia](https://www.synthtopia.com/content/2019/08/12/mok-1010musics-waverazor-dual-oscillator-eurorack-module-now-available/)).
MOK's founders, by its own page, are Rob Rampley, Taiho Yamada and Chris Compton
([mok.com/about.php](https://mok.com/about.php)); the same page calls the oscillator
design patented.

**The patent.** One family was found.

| | |
|---|---|
| Number, title | **US 10,262,646 B2**, "Multi-source switched sequence oscillator waveform compositing system" ([Google Patents](https://patents.google.com/patent/US10262646B2/en); [USPTO copy](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10262646)) |
| Inventors, owner | Robert Rampley, Taiho Yamada; assigned to Media Overkill, LLC (recorded 2018-01-09) |
| Dates | *Priority date* (the day the idea was first filed, which fixes what counts as earlier work) 2017-01-09, provisional application 62/444,270. Filed 2018-01-09 (application 15/866,405). Published 2018-07-12. Granted 2019-04-16. |
| Status | [source] Google Patents lists it as Active. Its legal events show the first maintenance fee paid on 2022-10-16. Term adjustment on the patent's face: 0 days. |
| Expected expiry | [inference] 2038-01-09: twenty years from filing, if every fee is paid. |
| Next fee | [inference] US patents lapse unless a fee is paid by 3.5, 7.5 and 11.5 years after grant, each with a six-month late period ([USPTO](https://www.uspto.gov/patents/maintain)). The 7.5-year date is **2026-10-16**; the late period ends 2027-04-16. Whether it has been paid could not be checked (§2.G). A lapsed patent can be revived on petition if the delay was unintentional. |
| Other countries | [source] An international (PCT) application, WO 2018/129558 A1, was filed the same day; Google Patents marks it "Ceased" and lists no other national patents in the family. [inference] US only. |
| Claims | 4. Claims 1, 2 and 3 are independent; claim 4 depends on 3. |

**The independent claims, element by element** (close paraphrase, not quotation; words in
*italics* are the claim's own terms).

- **Claim 1** (a stored program). Software that makes a processor generate, at audio rate, a *phase coherent concatenated waveform* from a *switched sequence of multiple source oscillators*, by: (1) receiving *performance data*; (2) setting a *switch compositor* with at least two *switcher divisions*; (3) setting a phase position for each division; (4) setting a number of source oscillators corresponding to the number of divisions; (5) setting the frequency and phase properties of each source oscillator and of the compositor from the performance data; (6) starting a *master pitch clock*, synchronised with both the oscillators and the compositor, at a rate that follows the performance data and keeps phase and pitch coherence with the source oscillators; (7) selecting *sample frames* from each source oscillator; (8) traversing each division at the rate the master pitch clock sets; (9) maintaining constant pitch and phase coherence of the source oscillators; (10) outputting the phase coherent concatenated waveform.
- **Claim 2** (a system). A memory and a processor running software configured to do the same ten things.
- **Claim 3** (a method). (1) receive performance data; (2) set properties of at least two source oscillators; (3) set a number of switcher divisions corresponding to the number of source oscillators; (4) clock the oscillators and the switcher rate synchronously, so that they are coupled in both phase and frequency; (5) select sample frames from the source oscillators; (6) output a phase coherent concatenated waveform.
- **Claim 4** (dependent on 3). The frequency is below 20 Hz, which is the use as a slow modulation source.

**What the description adds.** [source] The drawings show four oscillators feeding a
clocked switch and a multiplexer under a master pitch and clock controller. Performance
data is the note's pitch and velocity. At each switch point a source oscillator can be
hard-synced, and an amplitude ramp or filter can smooth the join. A segment's oscillator
may be detuned or left free-running, and the number of segments need not fit the cycle.
The background says of the established synthesis methods: "None of these waveform
synthesis techniques provide phase coherent wave switching at audio rate."

**Searches for more.** [source] By owner "Media Overkill": 1 result, this patent. By
inventor "Robert Rampley": 1, this patent. "Taiho Yamada": 1, this patent. "Rob Rampley":
6, all assigned to Line 6 with a 2001 priority date, on e-commerce, multimedia lessons and
a guitar interface; none on oscillators. "Chris Compton" with "synthesizer": 0. The phrase
"wave slicing" with "oscillator synthesizer": 0. "switched sequence" with oscillator terms:
1, the PCT application. [inference] No continuation or second family was found. No lawsuit
or licence involving this patent was found in one web search.

### B. The neighbourhood

*Prior art* is anything public before a patent's priority date; it limits what the patent
can validly cover. This table serves two purposes: what might constrain SCALPEL, and what
shows the ideas are older than any live patent. Expiry dates marked [inference] are
computed from the dates on the patent, not looked up.

| Technique and date | What it does | Patent position | Bears on |
|---|---|---|---|
| **Hard sync**: analog synthesizers of the 1970s ([Wikipedia](https://en.wikipedia.org/wiki/Oscillator_sync)); alias-free digital form, Brandt, ICMC 2001 ([paper](http://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf)) | One oscillator's cycle restarts another's | None found on the idea | F4, sync mode |
| **Casio phase distortion and its "resonance" waveforms**: CZ-101, November 1984 ([Wikipedia: CZ](https://en.wikipedia.org/wiki/Casio_CZ_synthesizers), [Wikipedia: PD](https://en.wikipedia.org/wiki/Phase_distortion_synthesis)) | Reads a stored wave at a rate that varies within the cycle; resonance is a faster sine restarted each cycle and windowed | [US 4,658,691](https://patents.google.com/patent/US4658691A/en), Casio, Japanese priority December 1982, granted 1987-04-21. [inference] Expired by 2004 | F4 with F6: a windowed, synced wave inside the cycle |
| **Casio CZ, two waveforms in series** (1984, same source) | Plays one waveform for a cycle, then another, alternating, locked to pitch | As above | Switching between waveforms in step with pitch |
| **Yamaha windowed hard sync**, Japanese patent 3399272 (dated February 2003 on the face of Korg's patent) | [source] Described in Korg's patent as multiplying the synced wave by a window that depends on the master's phase | [inference] Expired by 2023 at the latest; not looked up | F6 |
| **Earlier waveform-generator patents named on the face of MOK's patent**: Nippon Gakki (Yamaha) US 4,036,096 (1977, "Musical tone waveshape generator") and US 4,249,447 (1981), Roland US 5,060,179 (1991), among 41 references | Not read beyond their titles; listed because MOK's claims were allowed over them | [inference] All expired by age | What the examiner already weighed |
| **Clocked sequential switches at audio rate**: the Doepfer A-151 ([manual](https://doepfer.de/a100_man/A151_man.pdf), undated; its maker dates version 2 to about 2005) | A four-way switch stepped by a trigger. The manual's first example switches between one oscillator's waveform outputs, clocked by that oscillator's own square wave, fast enough that the result is "in effect a new waveform" | A product, not a patent | Waveforms into one switch, clocked by the pitch |
| **VOSIM**, Kaegi and Tempelaars, JAES 1978 ([paper](https://kaegi.nl/werner/userfiles/downloads/vosim-system.pdf)) | Each period is a burst of sine-squared pulses, then silence | None looked for | A shaped event occupying part of the period |
| **FOF**, Rodet, Computer Music Journal 1984 ([CHANT paper](https://www.ee.columbia.edu/~dpwe/papers/RodetPB84-CHANT.pdf)) | One formant as a short decaying grain, restarted every period | None found | As above; the formant engine (§2.D) |
| **Phase-aligned formant (PAF)**, Puckette, JAES 1995 ([author's page](http://msp.ucsd.edu/techniques/v0.11/book-html/node96.html)) | A formant placed on a carrier locked to the fundamental | [source] A 1994 French patent owned by IRCAM. [inference] Twenty years have passed | Formant engine |
| **Pulsar synthesis**, Roads, JAES 2001 ([AES](https://aes2.org/publications/elibrary-page/?id=10198)) | Each period holds a *pulsaret* (any waveform, for a chosen fraction of the period, at its own rate) and then silence | None found (search: 0 results) | The closest academic ancestor of "width `w`, cut rate `k`" (F2, F4) |
| **Windowed sync and cycle warps in mainstream soft synths**: Xfer Serum manual, version 1.0.1, 2014 ([copy](https://s3.amazonaws.com/decembercymatics/Serum_Manual.pdf), pp. 14 to 15) | Lists self sync, windowed sync, a polarity flip at a chosen point in the cycle, mirror, FM, AM, ring modulation and quantize as oscillator warp modes | A product | F5, F6, F7 as commonplace features |
| **Phaseshaping**: Kleimola and others, "Vector Phaseshaping Synthesis", DAFx 2011 ([paper](https://www.dafx.de/paper-archive/details.php?id=pnPMKb4L_ZBFzl-725e6QQ)) | Bends a sine oscillator's phase with breakpoints inside the cycle, extending Casio's method | Academic; search for the term: 0 results | Treating parts of a cycle differently |
| **Segment-wise waveform construction in computer music**: Koenig's SSP, early 1970s ([koenigproject.nl](https://koenigproject.nl/ssp/)); Xenakis's dynamic stochastic synthesis ([Luque 2006](https://sonology.org/wp-content/uploads/2021/04/Luque-Stochastic_Synthesis-Origins_and_Extensions.pdf)) | Compose the waveform itself from ordered segments | Academic | "Segments" as an idea |
| **Wavetable scanning** (PPG, about 1979, [Wikipedia](https://en.wikipedia.org/wiki/Wavetable_synthesis)); **wave sequencing** (Korg Wavestation, [Wikipedia](https://en.wikipedia.org/wiki/Korg_Wavestation)); **wave terrain** (Mitsuhashi, JAES 1982, [AES](https://aes.org/e-lib/browse.cfm?elib=3815)) | Move between whole stored cycles over time, or read a surface along an orbit | Old | Not blade synthesis: they change the cycle between cycles, not within one |
| **FM**: Chowning, [US 4,018,121](https://patents.google.com/patent/US4018121A/en), granted 1977-04-19 | One oscillator modulates another's frequency | [source] Expired in the mid-1990s ([Wikipedia](https://en.wikipedia.org/wiki/Frequency_modulation_synthesis)) | FM modes; the PM engine |
| **Wavefolding**: the analog Buchla 259, modelled in a DAFx 2017 paper ([paper](https://www.dafx17.eca.ed.ac.uk/papers/DAFx17_paper_82.pdf)); ring modulation, sample-and-hold and step-holding are older still | Generic transformations | None looked for | Fold, ring, noise and crush modes |
| **Anti-aliasing by step correction**: Stilson and Smith 1996 ([paper](https://ccrma.stanford.edu/~stilti/papers/blit.pdf)); Brandt 2001 (above); Orban, [US 6,937,912](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6937912), filed 2000; Välimäki and Huovilainen, "PolyBLEP", IEEE 2007 ([Aalto](https://research.aalto.fi/en/publications/antialiasing-oscillators-in-subtractive-synthesis/)); ADAA, Parker and others, DAFx 2016 ([paper](https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf)) | Fix the samples either side of a jump | Orban: [inference] expired 2020. **Korg US 7,589,272: live on its face. See below** | F13 |

**The second live patent: Korg, US 7,589,272 B2**, "Bandlimited digital synthesis of analog
waveforms" ([Google Patents](https://patents.google.com/patent/US7589272B2/en);
[USPTO copy](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7589272);
its front page, its first six columns and all 60 claims were read for this document).
Words in *italics* are the patent's own terms.

- [source] Inventors Andrew B. Leary and Charles T. Bright; assigned to Korg, Inc. Filed 2005-01-03 (application 11/028,383). Granted 2009-09-15. 60 claims. A certificate of correction dated 2010-12-14 sets its term extension at 1278 days.
- [inference] Expiry **2028-07-04**: twenty years from filing plus 1278 days, if its fees were paid; the last of the three fell due in 2021. A search-engine summary of the Google Patents page reported it as active on 2026-10-13; the page itself could not be loaded to confirm (§2.G).
- [source, second-hand] A search-engine summary of the Google Patents record names a Japanese counterpart, JP 4549967 B2 (application 2005-375217, dated 2005-12-27). [inference] Twenty years from that filing ended in December 2025. Not verified first-hand; no other counterpart is known to this read.
- [source] What it describes: generate the plain samples of a waveform that has a jump; work out where inside the sample the jump fell; add, to the samples either side, the samples of an *alias reducing pulse* sized by the jump. The pulse is a band-limited step with the ideal step subtracted, so that it begins and ends at zero and "has essentially no DC bias"; in the examples it comes from a stored table. The patent applies the same idea to hard sync, where the synced wave's jump is sized from its values before and after the reset. Its background sets it apart from Brandt's minimum-phase step, which it says has a DC bias.
- [source] Independent claims, paraphrased. **Claim 1** (method, in a waveform synthesizer): generate samples of a waveform with a fundamental frequency and a discontinuity whose phase lies inside a particular sampling interval; sum the samples around the discontinuity with a set of samples of an alias-reducing pulse that is based on the discontinuity's phase and has essentially no DC bias. **Claim 2**: the same, where the pulse is odd-symmetric about the discontinuity's phase instead. **Claim 15**: a system (memory, instructions, logic) doing what claim 2 does. **Claim 60**: claim 2's method plus converting the output to an analog waveform. **Claims 28 and 43** (method and system, hard sync): a first waveform with an event at a first phase; a second waveform with a discontinuity whose phase depends on the first, and whose values before and after depend on that phase; an odd-symmetric pulse based on that phase and those two values, summed with the second waveform's samples. **Claims 58 and 59**: as 28 and 43 without the odd-symmetry requirement.
- [source] Table lookup appears only in dependent claims (8, 9, 21, 22, 36, 37, 51, 52).
- [source] No lawsuit or licensing report involving this patent was found in one web search. [inference] That is not evidence that none exists, or that none will.

### C. The comparison

Cells say **present in SCALPEL**, **absent**, or **unclear**, and what they rely on. None
of this is a conclusion on infringement. *Claim construction*, deciding what a claim's
words legally mean, is done from the patent and its examination record by an attorney, and
finally by a court. "Unclear" usually means exactly that it depends on claim construction.

**C1. MOK, US 10,262,646: claims 1 and 2** (the same ten elements).

| # | Element (paraphrase) | SCALPEL | Relied on |
|---|---|---|---|
| 1 | Receive performance data | **Present** | `noteOn(note, freq, vel)`, `:368`; §2 |
| 2 | A *switch compositor* with at least two *switcher divisions* | **Unclear** | There is no switch or multiplexer. A blade is a window inside which the output moves toward a transformed signal by `g·depth` (F2, F3). One blade does divide the cycle into "inside" and "outside" |
| 3 | A phase position for each division | **Unclear** (follows 2) | Centre and width are cycle fractions (§4.1; `:177`), but the centre also moves with rotation and the swarm (F9) |
| 4 | As many *source oscillators* as divisions | **Unclear** | One accumulator per member (`:657`). The base and the blade wave are both formulas of that one phase (`:174`, `:183`). Fold, crush and noise blades have no second waveform at all (`:194-226`). Whether a second wave shape evaluated at a derived phase is a *source oscillator* is [attorney] |
| 5 | Frequency and phase of each source oscillator and of the compositor set from performance data | **Unclear** | Cut rate is a ratio of the member's pitch by default, so the blade wave tracks the note (§4.1); in Hz units it does not (`:826`). Blade position does not depend on the note |
| 6 | A *master pitch clock* synchronised with both | **Unclear** | No separate clock. The member's phase drives both the window test and the blade wave (`:178`, `:183`). With rotation, swarm frame or free FM the parts are deliberately not locked (`:659`, `:786-803`, `:822`) |
| 7 | Select *sample frames* from each source oscillator | **Unclear** | Each sample is computed as `base + g·depth·(hot − base)` (`:237`). At depth 1 with hard edges that equals taking the blade wave inside and the base outside; otherwise it is a mix |
| 8 | Traverse each division at the master clock's rate | **Unclear** (follows 2, 6) | The window is crossed once per member cycle when rotation is 0 |
| 9 | Maintain constant pitch and phase coherence of the source oscillators | **Unclear** | In sync mode the blade wave restarts at every entry by construction (`:183-187`). Member frequencies are not constant: the coupling changes them every 32 samples (§3; `:479`, `:752`) |
| 10 | Output a *phase coherent concatenated waveform* | **Unclear** | The output is a sum of 1 to 9 members (up to 32 in horde), each corrected, DC-adjusted and filtered (§2; `:860-879`). One member with one hard, full-depth sync blade is a two-piece waveform. Soft edges, partial depth, overlapping or serial blades are not piece-by-piece joins |

**C2. MOK, claim 3.** (1) receive performance data: **present**. (2) properties of at
least two source oscillators: **unclear**, as row 4. (3) as many divisions as oscillators:
**unclear**, as rows 2 and 4. (4) oscillators and switcher clocked synchronously, coupled
in phase and frequency: **unclear**, as row 6; in the default setting the window and the
blade wave are locked because both are computed from one phase. (5) select sample frames:
**unclear**, as row 7. (6) output: **unclear**, as row 10. **Claim 4** (below 20 Hz):
**absent** from the spec as it stands, which specifies an audio oscillator only.

**Headline for MOK.** [inference] One element is plainly present, none is plainly absent
in every setting, and the rest depend on whether a gated, additive window computed from a
single phase is what the claims mean by a switch between clocked source oscillators. The
setting nearest the patent's description is one blade in sync or FM-reset mode, depth 1,
hard edges, cut rate in per-cycle units, no rotation, member frame. The settings furthest
from it are fold, crush, noise and ring (which transform the base and select nothing),
soft edges, partial depth, rotation, swarm frame, serial interplay and collision. [attorney]
would need: the examination record of application 15/866,405, including the PCT search
report and written opinion of 2018-04-05 named on the patent's face, to see how the owner
distinguished earlier work; the meaning of *switch compositor*, *source oscillator*,
*master pitch clock* and *concatenated*; the earlier work in §2.B (Casio's alternating
waveforms, clocked switches, the 1977 to 1991 segment patents); and the engine that will
actually ship (`h2/engine/blade.h`, `h2/cores/scalpel/razor_core.h`,
`docs/design/scalpel-horde-engine.js`), not only the bench.

**C3. Korg, US 7,589,272**, against F13.

| Claims | Element (paraphrase) | SCALPEL | Relied on |
|---|---|---|---|
| 1, 2, 15, 60 | Generate samples of a waveform that has a fundamental frequency and a discontinuity whose phase lies inside a sampling interval | **Present** | The uncorrected sample is computed at `:690`; each jump is located as a fraction τ of the sample (`:650-653`, `:641`); §7 lists the jumps |
| 1, 2, 15, 60 | Sum the samples around the discontinuity with a set of samples of an alias-reducing pulse based on the discontinuity's phase | **Present** on a plain reading [inference] | `addE` adds `−h/2·τ²` to the current sample and `+h/2·(1−τ)²` to the delayed one (`:645-649`, `:720-721`; §7). They are two values from a formula, not a table. Whether that is a set of samples of a pulse in the claim's sense is [attorney] |
| 1 | The pulse has essentially no DC bias | **Unclear** | [inference] The correction, as a continuous curve, averages to zero across the jump. The two sampled values of one event do not sum to zero. Which of these the words mean is [attorney] |
| 2, 15, 60 | The pulse is odd-symmetric about the discontinuity | **Present** [inference] | The two terms are mirror images with opposite sign either side of the jump |
| 15 | A system with stored instructions and logic that does this | **Present** when the plugin runs | — |
| 60 | Convert the digital output to an analog waveform | **Unclear** | The plugin passes digital audio to its host; conversion happens in the user's hardware. Who performs a step, and whether that matters, is [attorney] |
| 28, 43, 58, 59 | A first waveform with an event at a first phase; a second waveform with a discontinuity whose phase depends on it | **Unclear** | [inference] The blade wave restarts at blade entry, an event located inside the sample (`:697`, §4.2). Whether the member's cycle and a blade wave that exists only inside a window are a first and a second waveform, each with a fundamental frequency, is [attorney] |
| 28, 43, 58, 59 | The second waveform's values before and after depend on that phase; the pulse is based on the phase and on both values; sum | **Present** on a plain reading [inference] | The jump height is the composite just after minus just before (`:520-540`); §7 "Step heights" |
| 28, 43 | The pulse is odd-symmetric | **Present** [inference] | As above |

**Headline for Korg.** [inference] On a plain reading most elements of claims 1, 2 and 15
appear present, not only in SCALPEL but in every PolyBLEP site listed under §1, including
the legacy engine. The open points are what *a set of digital samples of an alias reducing
pulse* and *essentially no DC bias* mean when the correction is a two-sample polynomial,
and who performs claim 60's last step. [source] The same two-sample polynomial is taught
in the open literature from 2007 and published in widely copied open-source code, for
example [stmlib's `polyblep.h`](https://github.com/pichenettes/stmlib/blob/master/dsp/polyblep.h)
(MIT licence, 2017). [inference] Wide use is context an attorney will weigh; it is not a
licence. [attorney] would need: the examination record of application 11/028,383 (what
was said about Brandt 2001 and Stilson and Smith 1996, both named in the patent); whether
the claims are valid over that work and over Orban's 2000 filing; the fee status; and
whether the corrections horde could use instead (oversampling alone, ADAA as in ADR-189
D1, a minimum-phase step, band-limited tables) fall inside or outside each claim,
especially 58 and 59, which lack the zero-DC and odd-symmetry limits.

### D. The other engines (flags only)

**The Kuramoto swarm as an oscillator bank.** None found in this search. "Kuramoto" or
"coupled oscillators" with sound-synthesis terms: 7 results, none on synthesis by coupled
oscillators. "Kuramoto" with audio terms: 576 results, the first 25 read, none relevant.
The same within the musical-instrument class (CPC G10H): 3, none relevant. No patent
notice was found for Chiral Audio's Foxfire in one web search. `docs/PRIOR-ART.md` §1 and
§6 already cover the products and the research.

**The per-partial variant (SPECTRA, parked).** None found, but the only searches that
touch it are the Kuramoto ones above; a dedicated search was queued and did not run
(§2.G). `docs/PRIOR-ART.md` §4 names NI Razor as the commercial per-partial precedent.

**The formant engine (FOF and pulsar grains).** None live found; the one unrestricted
search for FOF terms returned unusable noise (53,322 results). FOF (1984), VOSIM (1978)
and pulsar synthesis (2001) are published academic methods (§2.B); "pulsar synthesis" as a
patent search term: 0 results. One patent is known in the family, IRCAM's 1994 French
patent on PAF, which Puckette's own pages name; [inference] its twenty years have run. A
class-restricted formant search was queued and did not run.

**The 3-operator phase-modulation engine (STATION, out of 1.0).** Chowning's US 4,018,121
was granted on 1977-04-19 and has expired; Wikipedia gives 1995. Search by inventor
"Chowning": 3 results, that patent, its German counterpart and an unrelated 1972 patent.
Later patents on particular FM implementations were not searched; [inference] any from the
1980s would be past a twenty-year term. No search was made for patents on noise generators
or small wave memories.

**The N-body gravity modulator (ORBITAL).** None found in this search. Gravity, n-body,
orbit or particle simulation with modulation terms, within G10H, priority after 2005: 82
results, the first 25 read, none on a simulated physical system used as a modulation
source. Not searched: game-audio and general physics-simulation classes outside G10H.

**The morph grid.** Two patents turned up by web search; the database search is thin. One
unrestricted search for morphing between presets on a pad returned 5,193 results, the
first 25 read, none relevant (mostly video coding); the class-restricted version was
queued and did not run.

- [source] Apple, [US 7,042,472](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7042472), "Method and apparatus for morphing" (filed 2004-01-14, granted 2006-05-09, 44 days' extension): fixed points on a display, each holding a set of parameter values, and a movable point whose position weights them. [inference] On those dates it expired in February 2024.
- [source] Lafayette College, [US 10,770,048](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10770048), on patch morphing for an analog synthesizer (filed 2018-05-14, granted 2020-09-08, 144 days' extension; [inference] to about October 2038 if maintained). Both of its independent claims (1 and 14) recite a controller connected to rotate a physical knob. horde is software with no such hardware. This is a flag for the attorney's list, not an analysis.

Known earlier work besides: four-source vector mixing by joystick (Sequential Prophet VS,
1986, per [Wikipedia](https://en.wikipedia.org/wiki/Vector_synthesis)) and, nearest to the
quantum mode's "each parameter picks one parent", patch breeding by crossover and mutation
(Dahlstedt, ICMC 2001, [paper](https://quod.lib.umich.edu/i/icmc/bbp2372.2001.006/1)).
After §2.B this is where a proper search matters most, because here this one effectively
did not happen: nothing was searched on assigning each parameter whole to one of several
stored patches.

### E. Trademarks

A *trademark* protects a name used to sell goods, not a technique. This is a search-engine
and database skim, not a clearance.

- **WAVERAZOR.** [source] US registration 5,402,757 (serial 87237301, filed 2016-11-15, registered 2018-02-13, owner Media Overkill, LLC, class 9: sound synthesis software and hardware) was **cancelled on 2024-08-23**; the USPTO record reads: "Registration cancelled because registrant did not file an acceptable declaration under Section 8." ([USPTO TSDR](https://tsdr.uspto.gov/statusview/sn87237301), read 2026-10-13.) [inference] The federal registration is dead, but MOK still sells under the name, and in the US ownership of a mark comes from using it, with registration adding wider protection ([USPTO](https://www.uspto.gov/trademarks/basics/what-trademark)). Whether MOK has rights, or has re-filed, is [attorney]; no register outside the US was checked.
- **Where the word appears in tracked files** (`git grep -n -i waverazor`, 2026-10-13): `CLAUDE.md:104`, `reference/scalpel/README.md:3`, `specs/SPEC-SCALPEL.md:11`, and `reference/scalpel/prototype/scalpel-bench.html:170`. All four say "Waverazor-style". The first three are internal documents. The fourth is the opening line of a lab page a visitor reads.
- **Usual safe practice.** [source] Referring to someone else's product by name is generally allowed when the product cannot readily be identified otherwise, no more of the mark is used than needed, and nothing suggests sponsorship or endorsement ([INTA fact sheet](https://www.inta.org/fact-sheets/fair-use-of-trademarks-intended-for-a-non-legal-audience/)). [inference] In practice: name Waverazor and MOK where earlier work is being credited (`docs/PRIOR-ART.md`, history); describe horde's own technique in generic words everywhere a user looks ("blade synthesis", "a windowed, sectional oscillator"); do not use "X-style" as a selling line; and where the name does appear, say plainly that horde is not affiliated with MOK.
- **Internal names.** [source] `RazorCore`, `razor-core.js` and `razor_core.h` appear in 102 tracked files. They are identifiers, not names a user is sold, and the packet's staged decision D1 (recorded in ADR-184) already advises against "RAZOR" as the engine's name. "Razor" is also a Native Instruments product, named in `docs/PRIOR-ART.md` §4.
- **SCALPEL.** Existing audio products: Airwindows "Scalpel", a free EQ plugin from 2007 ([airwindows.com](https://www.airwindows.com/scalpel/)); WMD "SCLPL", a Eurorack EQ whose manual gives the name as "scalpel" ([manual](https://cdn.shopify.com/s/files/1/0977/3366/files/SCLPL_Manual_v0.5_2.pdf)). A trademark aggregator lists 69 US records for the word, the one seen in class 9 being for software development tools ([Trademarkia](https://www.trademarkia.com/search/trademarks?query=scalpel)). SCALPEL is an engine's code name; if it becomes a name users see, both EQs are neighbours in the same market.
- **horde.** No audio software or instrument under this name was found in two web searches. The word is common outside audio. No trademark register was searched by name.
- **Sluice.** No audio product under this name was found in two web searches. No register was searched.

### F. What the GPL does and does not change

[source] Under GPLv3 section 11, each contributor licenses the contributor's own
*essential patent claims* to everyone who receives the program, and section 10 bars a
licensee from suing over it
([GPLv3](https://www.gnu.org/licenses/gpl-3.0.html), read through the
[OSI copy](https://opensource.org/license/gpl-3-0);
[FSFE summary](https://fsfe.org/activities/gplv3/patents-and-gplv3.en.html)).
[inference] Those terms bind people who distribute or contribute. A patent owner who does
neither, such as MOK or Korg, is not a party to the licence, so the GPL gives horde and
its users no rights under their patents and no shield against them. [source] The US statute's basic rule draws no line between paid and free: liability attaches to whoever "makes, uses, offers
to sell, or sells any patented invention"
([35 U.S.C. 271(a)](https://www.law.cornell.edu/uscode/text/35/271)); [attorney] decides
what charging changes in practice. [source] Section 12 adds a constraint in the other
direction: if an agreement or court order imposes terms that contradict the licence, "you
may not convey it at all", and section 11 has a parallel rule for anyone who knowingly
relies on a patent licence that downstream recipients do not get. [inference] So a private
patent licence covering only the human's own builds would not, by itself, fit GPL
distribution. The brief for this document says "the GPL" without a version; this paragraph reads version 3.

### G. Searches run, and what could not be obtained

All on 2026-10-13. Patent searches used Google Patents' public search; result counts are
in §2.A, §2.B and §2.D.

**Run:** owner "Media Overkill"; inventors "Robert Rampley", "Taiho Yamada", "Rob Rampley",
"Chris Compton"; "wave slicing"; "switched sequence"; oscillator with segment, slice or
window and "hard sync" or "phase reset", priority after 2004 (14 results: the MOK PCT
application, Korg US 7,589,272, and two not examined, Moog US 10,229,664 on a
voltage-controlled oscillator core and Music Unit FR 3148489 / EP 4706036 on a spatial synthesizer); Casio
with "phase distortion", before 1988 (37); the Kuramoto and gravity searches of §2.D;
"phaseshaping" (0); "pulsar synthesis" (0); inventor "Chowning" (3). Web searches for each
row of §2.B, for litigation on both patents, and for the three names in §2.E. USPTO
trademark status by serial number for WAVERAZOR.

**Could not be obtained.**

- **Fee status of either patent at the USPTO.** The fee site requires an interactive sign-in page, and this brief allowed no browser automation. The human can check in minutes at the USPTO's patent maintenance fees page with patent 10262646 / application 15866405, and patent 7589272 / application 11028383.
- **Korg's legal-event history and foreign counterparts.** Google Patents returned "503 Service Unavailable" to every request from mid-session onward. The patent's text, claims and term correction were read from the USPTO's own copy instead.
- **Ten queued searches that did not run** for the same reason, retried repeatedly over about forty minutes: within the musical-instrument class (G10H), morphing between presets on a pad; presets combined by random or per-parameter selection; per-cycle segment switching; "wave slicing" and its synonyms; formant grains; aliasing and discontinuity correction; antiderivative anti-aliasing; coupled partials; simulated physics as a modulation source; and "Kuramoto" alone. Three web searches stood in for the anti-aliasing, morphing and Korg-family ones; they found the two morph patents of §2.D and Korg's Japanese counterpart, and nothing else. The anti-aliasing class is the one most likely to hold something else live.
- **The examination records** (file wrappers) of both patents, which show what each owner gave up to get the claims allowed. They are public at the USPTO's Patent Center, which needs a browser.
- **Registers outside the US**, for patents (beyond Google's family listing) and for trademarks. **Name searches of any trademark register.**
- **Court and patent-office dockets.** Litigation was looked for by web search only.

## 3. Proposed addition to `docs/PRIOR-ART.md`

For the human to approve and the lead to apply. `docs/PRIOR-ART.md` is a protected path
and was not edited. The existing §7 would be renumbered or left in place, as the lead
prefers; the text below is written to follow it.

```markdown
## 8. Blade synthesis (SCALPEL, horde 2's main engine) (added 2026-10-13)

**The debt, stated first.** Cutting a waveform's cycle into parts and treating the parts differently is not ours. **MOK's Waverazor** (shown January 2017) is the shipping instrument built on it: up to 16 slices per oscillator, each with its own wave, tuning, level and phase, with hard sync resettable per slice. MOK holds **US 10,262,646** (granted 2019; one family, US only) on a "multi-source switched sequence oscillator": several source oscillators, a pitch-locked switch, one spliced output. SCALPEL's own spec describes it as Waverazor-style. horde is not affiliated with MOK, and "Waverazor" is MOK's name: we cite it here as prior art and do not use it to describe or sell horde.

**It is older than Waverazor, too.** A wave restarted and windowed inside each cycle is Casio's phase-distortion "resonance" (CZ-101, 1984; US 4,658,691, long expired). A shaped event occupying a chosen fraction of the period, at its own rate, is VOSIM (1978), FOF (1984) and pulsar synthesis (Roads, 2001). Alternating whole waveforms in step with pitch is also Casio's. Windowed sync, a flip at a point in the cycle, mirror, FM, ring and quantize are stock oscillator warps in Serum (2014). Phaseshaping with breakpoints inside the cycle is Kleimola and others (2011).

**Our position.** SCALPEL is not a switch between oscillators. Each swarm member has one phase; a **blade** is a window over the cycle that adds a gated transformation of the base wave (sync, FM, noise, fold, ring, crush) at a depth, with hard or soft edges, a mirror, and a second blade that can stack on the first. What the 2026-10-13 read found no analogue for (it was a patent read, not a product survey): the blade riding a **coupled-oscillator swarm**, so that the same Kuramoto law that sets detune also sets where each member's cut lands (spread laws, cut rules, the swarm frame, cross-member modulation), plus serial interplay and collision between two blades. Claim those, measured; never a whole new kind of synthesis.

## 9. Risk flag: two live US patents (added 2026-10-13)

Neither has been reviewed by a lawyer. Full read: `docs/research/2026-10-13-blade-synthesis-prior-art.md`.

- **MOK, US 10,262,646** (to January 2038 if maintained). Its claims are written around source oscillators and a clocked switch; SCALPEL's architecture differs, but its simplest setting (one hard-edged, full-depth sync blade) gives a similar result. Most claim elements read "unclear": they depend on what the claim's words mean.
- **Korg, US 7,589,272** (to July 2028 if maintained): anti-aliasing by adding a zero-mean, odd-symmetric correction pulse around each waveform jump. On a plain reading this is close to the two-sample PolyBLEP used by SCALPEL and by the legacy swarm, sub and STATION cores. PolyBLEP is in the open literature from 2007 and in much open-source code; that is context, not a licence.

Commission a real freedom-to-operate review of both before any build is offered. The GPL does not help here: its patent terms cover contributors' own patents, not a third party's. As with §7, this file is an engineering positioning document, not legal advice.
```

## 4. Questions for a patent attorney

*Freedom to operate* means being able to make and distribute something without infringing
a patent that is in force.

1. Are US 10,262,646 and US 7,589,272 in force today, with all fees paid? Are there continuations, foreign counterparts or related patents this search missed?
2. For Korg's claims 1, 2 and 15: does a two-sample polynomial correction computed from a formula (PolyBLEP) fall within *a set of digital samples of an alias reducing pulse* that is odd-symmetric or has *essentially no DC bias*? What did Korg say during examination about Brandt 2001 and Stilson and Smith 1996?
3. Are those claims likely valid over the earlier work in §2.B, including Orban's 2000 filing?
4. Which alternatives would fall outside each Korg claim: oversampling alone, antiderivative anti-aliasing, a minimum-phase step, band-limited tables? Do claims 58 and 59, which lack the zero-DC and odd-symmetry limits, reach a blade wave that restarts at blade entry?
5. Korg's patent expires in July 2028 if maintained. What exposure is there for source code that is already public, and for builds released before that date?
6. For MOK's claims: is a window that adds a gated transformation, computed from one phase accumulator, a *switch compositor* with *switcher divisions* fed by *source oscillators* under a *master pitch clock*? Does the answer change between SCALPEL's simplest setting and its others (§2.C)? Does the doctrine of equivalents reach it?
7. Does it matter that SCALPEL's own spec and lab page describe it as "Waverazor-style"?
8. Does publishing source code, as opposed to distributing a built instrument, engage either patent? Does asking for money, even optionally, change the exposure or only the damages?
9. If a licence from either owner were wanted, can it be had on terms the GPL's sections 11 and 12 allow, that is, passing to everyone downstream?
10. May the project keep citing Waverazor by name in its prior-art file, given the cancelled US registration and MOK's continued use? Should a non-affiliation line be added?
11. Is a clearance search worth commissioning for the names horde, SCALPEL and Sluice before 1.0?
12. Does Lafayette College's US 10,770,048, whose independent claims recite a controller that turns physical knobs, have any bearing on a software morph pad?
13. What would a proper freedom-to-operate search cost for the whole instrument, and which parts should it cover first? (This read suggests anti-aliasing, blade synthesis and the morph grid, in that order.)

## 5. Risk in plain words

**What is known.** Two US patents are live on their face and touch things horde does. MOK's
covers the product SCALPEL was modelled on, and SCALPEL's own documents say so. Korg's
claims a way of removing aliasing that, on a plain reading, looks like the one horde's
oscillator cores use. Both were
found in an afternoon, which says something about how much a longer search might find.

**What is not known.** Whether either patent is still being paid for. What their claims
legally mean, and so whether SCALPEL or PolyBLEP is inside them: for MOK that is open
element by element; for Korg the plain reading is uncomfortably close, and the questions
are narrower. Whether either claim would survive the earlier work listed here. Whether
either owner has ever enforced, or would against a one-person GPL project; nothing found
says they have. What a search outside the US, or a complete one inside it, would add: a
third live patent, on morphing an analog synthesizer's knobs, surfaced only at the end.

**What would change the picture.** A lapse: MOK's fee is due on 16 October 2026, and the
company let its WAVERAZOR trademark registration lapse in 2024. Time: on the dates read here, Korg's patent
ends in July 2028 at the latest. An attorney's reading of the claims and the examination
records, which could move several "unclear" cells either way. A change of anti-aliasing
method, which horde 2 has begun for some blade waves, for other reasons (ADR-189 D1). And the reverse:
a continuation, a foreign counterpart, or a third patent in the anti-aliasing class that
this search was cut off before reaching.

**The plain position.** Nothing here says horde must stop, and nothing here says it is
safe. Releasing under the GPL does not move either patent. Offering builds for money does
not create the question, which exists as soon as an instrument is built and used, but it
raises the cost of having left it unasked. The sensible order is: confirm the two fee
statuses, then one conversation with a patent attorney with this document in hand, before
the first build is offered to anyone.
