/*
 * os_default_check.mjs — the os 1 default flip (B445, ADR-191) keeps state saved before it at os 2.
 * WIRED: ./verify fast.
 *
 * HYPERSAW, 2026-10-08, dispatched by the horde lead on ROADMAP B445. ADR-197 rule E: a default
 * change carries an ADR AND a migration. ADR-191 moved the device default from os 2 to os 1
 * (`LAB_DEF.os`, docs/design/scalpel-interface-lab.html); the engine core's own setOS(2) and every
 * pinned scenario stay at os 2.
 *
 * THE MARKER IS THE `os` FIELD ITSELF. A preset record plays at DEF with its own params written
 * over it (applyPreset: `P = Object.assign({}, DEF, pr.params, ...)`), so a record that carries
 * `os` is immune to a default change, and a record that does not takes whatever DEF says. The one
 * kind of state that was saved before the flip and can be reloaded is the porter's output
 * (tools/port_legacy_presets.mjs writes `legacy-presets.json`, git-ignored), and the porter writes
 * `os` on EVERY record (ctx.defaults has it; G5 sets it from the legacy Oversample row). The lab has
 * no save path, no download and no storage of patch state (T5 holds that), and horde 2's preset
 * format does not exist yet (B395): it MUST carry `os` or a format version, and this check is
 * where that obligation is pinned. Factory records (reference/scalpel/data/presets.json, the lab's
 * ENV_PRESETS) carry no `os` and play at the new default on purpose: the listening gate covered them.
 *
 * WHAT IS CHECKED
 *   T1 The lab's DEF now says os 1 while the oracle (and so the engine) default stays os 2.
 *   T2 A PRE-FLIP saved record (fixtures/os-default-preflip-ported-preset.json, frozen from the
 *      porter on 2026-10-08, a legacy preset whose Oversample was on) loads at os 2.
 *   T3 A POST-FLIP record with no `os` (an inline record, and the first bench preset) loads at os 1.
 *   T4 The LIVE porter still writes `os` on every record it ports (the future saves stay safe).
 *   T5 The lab has no surface that stores or exports patch state (a new one needs a migration).
 *   MUST-FAIL CONTROLS, each through the SAME detector: the pre-flip fixture with its `os` removed
 *   (the migration removed) loads at os 1 and T2 reads red; a lab still defaulting to os 2 reads red
 *   on T3; a porter record without `os` reads red on T4; a planted setItem of the patch, a
 *   download and a second Blob URL each read red on T5.
 *
 * KNOWN LIMIT. The lab's DEF is read as TEXT (LAB_DEF's literal) and composed the way the lab composes
 * it (anchored below), not by running the lab: running it needs the oracle, the audio graph and a DOM.
 * The in-page self-checks (`?check=1`) are the run-the-lab layer and are not part of ./verify.
 */
import './sandbox_guard.mjs';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import * as P from '../port_legacy_presets.mjs';

const ROOT = P.ROOT;
const LAB = readFileSync(join(ROOT, 'docs/design/scalpel-interface-lab.html'), 'utf8');
const ctx = P.loadContext(ROOT);
const FIXTURE = JSON.parse(readFileSync(join(ROOT, 'tools/labharness/fixtures/os-default-preflip-ported-preset.json'), 'utf8'));
const BENCH = JSON.parse(readFileSync(join(ROOT, 'reference/scalpel/data/presets.json'), 'utf8')).presets;

/* LAB_DEF's `os`, from the literal with its comments stripped (the B445 comment quotes `os: 2`). */
function labDefOS(text) {
  const m = text.match(/const LAB_DEF = \{([\s\S]*?)\};/);
  if (!m) return { err: 'no `const LAB_DEF = {...};` in the lab' };
  const body = m[1].replace(/\/\*[\s\S]*?\*\//g, '');
  const o = body.match(/\bos:\s*(\d+)/);
  return { os: o ? Number(o[1]) : undefined };
}
/* The lab's composition, mirrored: DEF = oracle defaults (os = the engine's) over-written by LAB_DEF;
   a record plays as Object.assign({}, DEF, record.params). The anchors fail when the lab stops saying so. */
const ANCHORS = ['const DEF = Object.assign(oracleDefaults(), LAB_DEF);', 'P = Object.assign({}, DEF, pr.params,',
  "if (REF) return Object.assign({}, REF.t, REF.d, { os: REF.os });"];
const anchorFails = (text) => ANCHORS.filter((a) => !text.includes(a)).map((a) => `lab no longer contains: ${a}`);
function playsAtOS(record, text) {
  const d = labDefOS(text);
  const def = Object.assign({ os: ctx.defaults.os }, d.os === undefined ? {} : { os: d.os });
  return Object.assign({}, def, record.params).os;
}

const T1 = (text) => {
  const f = anchorFails(text), d = labDefOS(text);
  if (d.err) f.push(d.err);
  else if (d.os !== 1) f.push(`LAB_DEF.os is ${d.os}, want 1 (the device default, ADR-191)`);
  if (ctx.defaults.os !== 2) f.push(`the oracle/engine default os is ${ctx.defaults.os}, want 2 (the core stays at os 2)`);
  return f;
};
const T2 = (rec, text) => (playsAtOS(rec, text) === 2 ? [] : [`pre-flip record "${rec.name}" plays at os ${playsAtOS(rec, text)}, want 2`]);
const T3 = (recs, text) => recs.filter((r) => playsAtOS(r, text) !== 1).map((r) => `post-flip record "${r.name}" (no os) plays at os ${playsAtOS(r, text)}, want 1`);
const T4 = (ported) => ported.filter((p) => !('os' in p.params)).map((p) => `ported record "${p.name}" carries no os`);

/* T5: the lab's persistence/export surface is exactly the logo style key and the worklet's Blob URL. */
const FORBIDDEN = /sessionStorage|indexedDB|document\.cookie|clipboard|showSaveFilePicker|\.download\s*=|history\.(?:push|replace)State|location\.hash\s*=/g;
function T5(text) {
  const f = [];
  for (const m of text.matchAll(/localStorage\.setItem\(\s*([^,)]+)/g)) if (m[1].trim() !== 'LOGO_STYLE_KEY') f.push(`localStorage.setItem(${m[1].trim()}…) stores something other than the logo style`);
  for (const m of text.matchAll(FORBIDDEN)) f.push(`persistence/export surface in the lab: ${m[0]}`);
  const urls = [...text.matchAll(/createObjectURL\(([^)]*\))/g)].map((m) => m[1]);
  if (urls.length !== 1 || !urls[0].includes('workletSource()')) f.push(`createObjectURL sites are not exactly the worklet's: ${JSON.stringify(urls)}`);
  return f;
}

/* The live porter on the three Oversample states a legacy preset can be in (absent, off, on). */
const ported = [{}, { oversample: 0 }, { oversample: 1 }].map((x, i) => P.portPreset({ schema: 3, params: { n: 5, ...x } }, ctx, `syn ${i}`));
const bench1 = BENCH[0];
const inline = { name: 'post-flip record without os', params: { N: 5, detune: 28 } };

let bad = 0;
const run = (name, fails) => {
  if (fails.length) { bad++; console.log(`FAIL ${name}:`); for (const x of fails.slice(0, 8)) console.log('   ' + x); }
  else console.log(`ok   ${name}`);
};
run('T1 LAB_DEF.os is 1 and the oracle/engine default is still 2', T1(LAB));
run('T2 a pre-flip saved record (os 2, frozen fixture) loads at os 2', T2(FIXTURE, LAB));
if ('os' in bench1.params) { bad++; console.log(`FAIL T3 precondition: bench preset "${bench1.name}" carries os (the factory presets were meant to carry none)`); }
run('T3 a post-flip record with no os loads at os 1 (inline record and bench preset 1)', T3([inline, bench1], LAB));
run('T4 the live porter writes os on every record (Oversample absent, off, on)', T4(ported) .concat(ported.map((p) => p.params.os).join() === '1,1,2' ? [] : [`ported os were ${ported.map((p) => p.params.os)}, want 1,1,2`]));
run('T5 the lab stores and exports no patch state (logo style key, worklet Blob URL only)', T5(LAB));

/* MUST-FAIL CONTROLS: the same detectors on planted faults. */
const noOS = (rec) => ({ ...rec, params: Object.fromEntries(Object.entries(rec.params).filter(([k]) => k !== 'os')) });
const controls = [
  ['the migration removed: the pre-flip fixture without its os loads at os 1', T2(noOS(FIXTURE), LAB)],
  ['a lab still defaulting to os 2', T3([inline], LAB.replace(/(const LAB_DEF = \{[\s\S]*?\bos:\s*)1/, (_, head) => head + '2'))],
  ['a lab whose LAB_DEF lost the flip altogether', T1(LAB.replace(/(const LAB_DEF = \{[\s\S]*?)\bos:\s*1/, '$1'))],
  ['a porter record without os', T4([noOS(ported[2])])],
  ['a planted localStorage.setItem of the patch', T5(LAB + "\nlocalStorage.setItem('hypersaw.patch', JSON.stringify(P));")],
  ['a planted download of the patch', T5(LAB + '\na.download = "patch.json";')],
  ['a planted second Blob URL', T5(LAB + '\nconst u = URL.createObjectURL(new Blob([JSON.stringify(P)]));')],
];
for (const [name, fails] of controls) {
  if (fails.length) console.log(`ok   control caught: ${name} (${fails[0]})`);
  else { bad++; console.log(`FAIL control NOT caught: ${name}`); }
}
console.log(`os_default_check: ${bad ? 'FAIL' : 'PASS'} — 5 oracles, ${controls.length} must-fail controls`);
process.exit(bad ? 1 : 0);
