/*
 * fidelity_scan_check.mjs — B325's standing check: the composed engine adds no click or
 * noise over the SCALPEL oracle, beyond a stated tolerance, on a fixed seeded preset subset.
 * WIRED: ./verify full.
 * HYPERSAW, 2026-09-28, ROADMAP B325 (records PR #821, branch lead-records-128). The human:
 * "I'm also starting to notice more noise and clicks ... Let's make sure the DSP fidelity is
 * holding up." The audit (docs/patchspace/2026-09-28-fidelity-audit.md,
 * tools/patchspace/fidelity_audit.mjs) answered it once; this keeps the answer true.
 *
 * TWO LAYERS, because the audit showed one is not enough.
 *   NEU  the SENSITIVE layer. Where the two swarm laws coincide (fidelity.mjs NEUTRAL: K 0,
 *        the aligned start, the oracle's start moved by §1.6.6's ½, the oracle's voice law)
 *        the composed engine must equal RazorCore SAMPLE FOR SAMPLE: the audit measured
 *        max|Δ| = 0 exactly, over whole phrases, blades included. So TOL.neutral is 0 and any
 *        plumbing defect in the composition shows, however masked it would be in the mix. The
 *        audit's transient detector could NOT see the planted defects below (a 20 dB-over-
 *        local-baseline HF residual misses a step inside a bright chord: B325 report §3), which
 *        is why this layer exists and why the controls run through it.
 *   MIX  the COARSE layer, at the presets' own settings (both laws live): the composed render
 *        has no more transient events than the oracle's (fidelity.mjs clickEvents, 20 dB over
 *        the local baseline; the audit found ZERO in both engines over 83 presets × 4
 *        phrases), and its chord flatness (0.25..1.15 s) exceeds the oracle's by no more than
 *        the excess the audit MEASURED for that preset (LAW_DFLAT) plus TOL.flat. That excess
 *        is horde's coupling law (ACCOUNTING row 6, "Horde law wins"): at a preset's own K the
 *        oracle's law locks the swarm and the composed engine's beats (Harmonic stack: 0.0057
 *        vs 0.0756 with both starts aligned; at K 0 the gap falls to 0.022, the start law).
 *        Flatness is deterministic here (seeded), so the recorded excess is exact and the
 *        margin is for a NEW noise source, not for run-to-run noise. A flat tolerance above
 *        the worst law excess (0.076) was tried first and could not see C4: a stuck 60-cent
 *        drift reads Δ 0.045 on Quarter sync.
 * The rows compare the two engines, never the composed engine with a number of its own, so a
 * provider drop that moves the oracle moves both sides; each row prints both readings.
 * CONTROLS (must fail, LIBRARY L0032: a detector that shares the assumption it measures
 * confirms whatever you expect). Each plants a composed-engine defect of the class the check
 * exists for and runs it through the SAME comparison:
 *   C1  a split-integrator SLIP: once, 0.25 s in, every member's blade phase is re-read from
 *       the swarm with the wrong origin (0, not ½) — the bug two integrators invite.
 *   C2  a START WITHOUT ITS RAMP: a fresh voice starts at full envelope, not the attack.
 *   C3  a JITTER: member increments scaled by 1 ± 0.1% from a seeded draw every swarm tick.
 *   C4  a NOISE source for the coarse layer: the swarm's drift stuck on at 60 cents (a
 *       composed row that forgets the preset's drift 0); the flatness row must fail.
 *   Z   the must-read-zero twin: the oracle against itself reads 0 everywhere.
 * Deterministic: seeded mulberry32 around every instance, no clock in anything measured (the
 * section timer prints cost only, as composed_engine_check does). Phrases are cut short
 * (fidelity.mjs shortPhrase) for the budget: ~40 s of DSP on an idle machine, so verify full.
 * By hand:  node tools/patchspace/fidelity_scan_check.mjs   (exit 1 on any red row)
 */
import { engineClass, PHRASES, PRESETS, renderPhrase, clickEvents, clickEnergyDb, SR, NEUTRAL, neutralPair, diffStats, shortPhrase } from './fidelity.mjs';
import { spectrum, mono, flatness } from './metrics.mjs';

/* chosen to cover the blade paths without chaos: sync, two blades, crush + FM, reflected sync,
   slewed crush, collision (Collision chirps is the heaviest of these), a pluck, a bright stack,
   and the Hz-unit paths that exposed B325's first-sample gap (Formant pluck: lock 2; Crunch
   horde: mUnit 1). Mono presets with glide are NOT here: the swarm steps a glide at its
   16-sample tick where RazorCore glides per sample (the composition's G3, by design), so they
   cannot be exact. */
const NEU_SET = ['Quarter sync', 'Two blades', 'Crush vs FM', 'Reflected sync', 'Slewed crush', 'Blade pluck', 'Collision chirps', 'Harmonic stack', 'Formant pluck', 'Crunch horde'];
/* the coarse layer: the presets with the largest composed flatness excess in the audit, and Quarter sync (N 1, no law excess) */
const MIX_SET = ['Quarter sync', 'Two blades', 'Breath and bite', 'Harmonic stack', 'Mirror-image spreads', 'Cross-mod horde'];
export const TOL = { neutral: 0, events: 0, flat: 0.02 };
/* composed − oracle chord flatness at each MIX preset's own settings, measured 2026-09-28 at the
   commit that added this check (B325 audit): the coupling law's share, recorded, not tolerated
   blindly. Re-measure (and say why in the PR) when either engine's law changes on purpose. */
const LAW_DFLAT = { 'Quarter sync': -0.0002, 'Two blades': 0.0377, 'Breath and bite': 0.0661, 'Harmonic stack': 0.0695, 'Mirror-image spreads': 0.0757, 'Cross-mod horde': 0.0687 };
const PH = { chord: shortPhrase(PHRASES.chord, 0.5), legato: shortPhrase(PHRASES.legato, 1.0), repeat: shortPhrase(PHRASES.repeat, 0.8), hold: shortPhrase(PHRASES.chord, 1.2) };

let red = 0;
const rows = [];
function row(ok, id, text) { rows.push(`${ok ? 'PASS' : 'FAIL'}  ${id.padEnd(4)} ${text}`); if (!ok) red++; console.log(rows[rows.length - 1]); }
let lastT = process.uptime();
function section(t) { const n = process.uptime(); console.log(`${t}   [previous section ${(n - lastT).toFixed(1)} s]`); lastT = n; }
const params = name => { const p = PRESETS.find(x => x.name === name); if (!p) throw new Error('preset missing: ' + name); return p.params; };

/* ---------------------------------------------------------------- NEU */
const NP = neutralPair();
const neu = (Composed, name, ph, Oracle) => diffStats(renderPhrase(Oracle || NP.oracle, params(name), PH[ph], { over: NEUTRAL }), renderPhrase(Composed, params(name), PH[ph], { over: NEUTRAL }));
section('NEU — the neutral case: composed ≡ oracle, sample for sample (K 0, aligned, the oracle\'s voice law), ' + NEU_SET.length + ' presets × chord/legato/repeat');
for (const name of NEU_SET) {
  const r = ['chord', 'legato', 'repeat'].map(ph => [ph, neu(NP.composed, name, ph)]);
  row(r.every(([, d]) => d.max <= TOL.neutral), 'NEU', `${name.padEnd(20)} ` + r.map(([ph, d]) => `${ph} max|Δ| ${d.max.toExponential(1)}`).join(' · '));
}

/* ---------------------------------------------------------------- MIX */
const O = engineClass('oracle'), C = engineClass('composed');
function mix(Cls, name) {
  const out = { events: 0, db: -200 };
  for (const ph of ['repeat', 'legato']) { const r = renderPhrase(Cls, params(name), PH[ph]), ev = clickEvents(r.L, r.R); out.events += ev.length; out.db = Math.max(out.db, clickEnergyDb(ev)); }
  const r = renderPhrase(Cls, params(name), PH.hold), a = Math.round(0.25 * SR), b = Math.round(1.15 * SR);
  out.flat = flatness(spectrum(mono(r.L.subarray(a, b), r.R.subarray(a, b)), SR));
  return out;
}
const cmpMix = (A, B, name) => [B.events - A.events <= TOL.events, B.flat - A.flat <= LAW_DFLAT[name] + TOL.flat,
  `events ${A.events}→${B.events} (Σ ${A.db.toFixed(1)}→${B.db.toFixed(1)} dB) · flatness ${A.flat.toFixed(4)}→${B.flat.toFixed(4)} (Δ ${(B.flat - A.flat).toFixed(4)}, law ${LAW_DFLAT[name].toFixed(4)} + ${TOL.flat})`];
section('MIX — both laws live: no more transient events than the oracle, flatness excess within tolerance, ' + MIX_SET.length + ' presets');
const mixBase = {};
for (const name of MIX_SET) {
  const a = mixBase[name] = mix(O, name), [c1, n1, t] = cmpMix(a, mix(C, name), name);
  row(c1 && n1, 'MIX', `${name.padEnd(20)} ${t}`);
}

/* ---------------------------------------------------------------- controls */
section('CONTROLS — planted composed-engine defects through the same comparisons (each must be caught)');
{
  const frac = x => x - Math.floor(x);
  const mb32 = a => () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  const Slip = class extends NP.composed {
    render(L, R) { if (this.nBase === 12032) for (const v of this.voices) if (v.active) for (const m of v.m) m.phi = frac(this.sw.swarms[v.si].phase[m.i]); super.render(L, R); }
  };
  const NoRamp = class extends NP.composed { startVoice(v, n, f, vel, fresh, re) { super.startVoice(v, n, f, vel, fresh, re); if (fresh) { v.env = 1; v.stage = 2; } } };
  const Jitter = class extends NP.composed {
    tickSwarm(v, S) { super.tickSwarm(v, S); const r = this.jr || (this.jr = mb32(0xC3)); for (let i = 0; i < this.d.N; i++) S.eff[i] *= 1 + 0.002 * (r() - 0.5); }
  };
  const Drift = class extends C { syncSwarm() { super.syncSwarm(); this.sw.p.driftDepth = 60; } };
  const d1 = neu(Slip, 'Two blades', 'chord');
  row(d1.max > TOL.neutral, 'C1', `CONTROL split-integrator slip at 0.25 s (Two blades, chord): max|Δ| ${d1.max.toExponential(1)}, step ${d1.step.toExponential(1)} at ${(d1.at / SR).toFixed(3)} s — the NEU row must fail`);
  const d2 = neu(NoRamp, 'Harmonic stack', 'repeat');
  row(d2.max > TOL.neutral, 'C2', `CONTROL fresh voice starts at full envelope (Harmonic stack, repeat): max|Δ| ${d2.max.toExponential(1)} at ${(d2.at / SR).toFixed(3)} s — the NEU row must fail`);
  const d3 = neu(Jitter, 'Quarter sync', 'chord');
  row(d3.max > TOL.neutral, 'C3', `CONTROL ±0.1% seeded increment jitter per tick (Quarter sync, chord): max|Δ| ${d3.max.toExponential(1)} — the NEU row must fail`);
  const [, n4, t4] = cmpMix(mixBase['Quarter sync'], mix(Drift, 'Quarter sync'), 'Quarter sync');
  row(!n4, 'C4', `CONTROL drift stuck at 60 cents (Quarter sync): ${t4} — the MIX flatness row must fail`);
  const z = neu(NP.oracle, 'Two blades', 'chord'), zm = cmpMix(mixBase['Two blades'], mix(O, 'Two blades'), 'Two blades');
  row(z.max === 0 && zm[0] && zm[1] && mixBase['Two blades'].events === 0, 'Z', `must-read-zero: the oracle against itself (Two blades): NEU max|Δ| ${z.max.toExponential(1)} · MIX ${zm[2]}`);
}

console.log(`\n${red ? 'RED' : 'GREEN'} — fidelity_scan_check: ${rows.length} rows, ${red} failed`);
process.exit(red ? 1 : 0);
