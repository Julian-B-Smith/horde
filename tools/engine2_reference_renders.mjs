/*
 * engine2_reference_renders.mjs -- the new engine's reference renders: the bank and the 12 envelope
 * presets, each played on the four phrases of the phrase set by TODAY's engine, with the metadata and
 * the manifest of SHA-256 hashes that freeze them. ROADMAP B497 part 2 (ADR-223; ADR-226 item 9; spec
 * 10.1 and 10.3).
 *
 * WIRED: ./verify fast (--static-check only: the metadata against its sources, no engine, about 1 s).
 *        The rendering is not wired: 380 renders take minutes and need the built harness, so
 *        `--out` and `--verify` are run by hand (below) when the renders are made or checked.
 *
 *   node tools/engine2_reference_renders.mjs --out <dir> [--sr 48000] [--bin <h2_harness_render>] [--meta <dir>]
 *                                            [--presets "Name,Name"] [--phrases held,trill]
 *       Renders every (preset, phrase) into <dir> (WAV) and writes the manifest (manifest.json at the
 *       default 44.1 kHz, manifest-48k.json at --sr 48000) into <meta> (default <dir>); at 44.1 kHz it
 *       also writes parameters.json. A subset (--presets, --phrases) writes only to <dir>, never
 *       to the tracked metadata. The harness is built by h2/harness/README.md, "Building".
 *   node tools/engine2_reference_renders.mjs --verify <dir> [--sr 48000]
 *       Hashes the WAVs in <dir> (a fetched copy of the release asset, unzipped) against the tracked
 *       manifest of that rate. Exit 1 on any missing file or mismatched hash. Needs no engine.
 *   node tools/engine2_reference_renders.mjs --static-check
 *       The tracked manifest.json, manifest-48k.json and parameters.json against what this file would
 *       write from the bank, the envelope presets, the phrases and the spec's base values (everything
 *       but the hashes and levels, which only a render makes), and the phrase set's own consistency;
 *       then the must-fail controls. This is what ./verify fast runs.
 *
 * WHAT IS PLAYED (all of it is in this file and the three inputs; nothing is typed in per preset):
 *   - the input data is engine2/data/bank/presets.json (83), engine2/data/envelopes/presets.json (12)
 *     and engine2/data/phrases/phrases.json. A preset is resolved by Appendix B.1: every key the
 *     preset omits takes its base value, read from the spec's own sentence (a source that changes
 *     shape stops the run). A key today's engine does not have is recorded per preset
 *     (notExpressible), never guessed; the probe in main() asks the engine, one key at a time.
 *   - the level is the bank patches' default: gain = presetGain() (h2/harness/gen_patches.mjs), the
 *     level tools/h2_ab.mjs plays a bank preset at. The settings are the A/B tool's, "as the installed
 *     preview plays": os 1 (Standard), frameHold 1, stealFade 1, poly 8, no voice cap; host seed 1,
 *     host blocks of 128. Set in the A/B tool's order: the keys, gain, the three settings, snap, poly.
 *   - 44.1 kHz (or 48 kHz with --sr 48000), stereo, 24-bit PCM, UNNORMALISED, from sample 0 of the
 *     event list, for the phrase's length plus a 2 s tail (the tail is cut where it falls: a 5 s
 *     release is heard for 2 s).
 *   - the phrase file's samples are at 44.1 kHz. At another rate each event's sample is
 *     round(time_s * rate) (phrasesAt); the length is the last note-off's derived sample. The 48 kHz
 *     set is the same engine, settings and level, so it shares parameters.json with the 44.1 kHz set
 *     and writes no parameters of its own.
 *   - glide: today's engine glides in its mono modes (polyMode 1 and 2) between overlapping notes, or
 *     after every note with glideAlways, 95 % in `glide` ms; it has no glide in poly mode, whatever
 *     the stored value. The phrase events carry no glide; each render's `glides` says whether the
 *     engine's rule can act in that render, and the parameters say why.
 *
 * DETERMINISM. No clock and no randomness here: the host stream is seeded (mulberry32, seed 1 in the
 * engine), and the manifest holds the same bytes for the same engine build. The render time is
 * printed, never written. The hashes and the engine's own digest are keyed to the build that made
 * them (h2/README.md): another compiler may move a last bit, and --verify then says so by name.
 *
 * No dependency but Node's own, h2/harness/harness.mjs, h2/harness/gen_patches.mjs and the WAV writer
 * of tools/h2_ab.mjs.
 */
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { DEFAULT_BIN, ROOT, jobFor, mtof, run } from '../h2/harness/harness.mjs';
import { presetGain } from '../h2/harness/gen_patches.mjs';
import { decodeWav, encodeWav } from './h2_ab.mjs';

export const TAG = 'engine2-reference-renders-v1';
export const ASSET = 'engine2-reference-renders-v1.zip';
export const SR = 44100, TAIL_S = 2, SEED = 1, BLOCK = 128, BITS = 24;   // SR: the phrase file's rate and the default render rate
// the A/B tool's settings, "as the installed preview plays" (tools/h2_ab.mjs, makeSet and jobsFor)
const BEFORE_SNAP = [['os', 1], ['frameHold', 1], ['stealFade', 1]], POLY = 8;
const BANK = 'engine2/data/bank/presets.json', ENVELOPES = 'engine2/data/envelopes/presets.json';
const PHRASES = 'engine2/data/phrases/phrases.json', SPEC = 'specs/SPEC-ENGINE.md';
export const META = 'engine2/data/reference-renders';
const PARAMETERS = `${META}/parameters.json`;
// The rates the set is made at. 48 kHz is B497 part 3 (the test author's Q-TA-2): its own manifest and
// asset, the same tag. Any other rate is refused rather than given an improvised name.
export const RATES = {
  44100: { manifest: 'manifest.json', asset: ASSET },
  48000: { manifest: 'manifest-48k.json', asset: 'engine2-reference-renders-v1-48k.zip' },
};
export const PHRASE_TIMING = 'an event\'s sample is round(time_s * rate) from phrases.json (which gives samples at 44.1 kHz); a phrase\'s length is its last note-off\'s sample';
const rateOf = sr => { if (!RATES[sr]) throw new Error(`engine2_reference_renders: --sr ${sr}: the set is made at ${Object.keys(RATES).join(' and ')} Hz only`); return RATES[sr]; };
const manifestPath = sr => `${META}/${rateOf(sr).manifest}`;
const BATCH_PRESETS = 2;   // presets per engine process: 8 renders of about 1 MB of WAV, 60 MB of float64 held at once

const read = rel => readFileSync(join(ROOT, rel), 'utf8');
const sha256 = buf => createHash('sha256').update(buf).digest('hex');
const slug = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const round = (x, d) => Number(x.toFixed(d));
const dbfs = x => (x > 0 ? round(20 * Math.log10(x), 2) : '-inf');   // JSON has no infinity: a silent channel reads as the text -inf

/* ---------------------------------------------------------------- the inputs */
/* B.1's base values, from the spec's own sentence. The sentence is "... takes its base value: A 4, D 400,
   ... xm 0. A key with no listed base value ..."; kCustom's value holds a comma, so it is lifted out first. */
export function baseValues(specText) {
  const m = /an omitted key takes its base value: ([^\n]*?)\. A key with no listed base value/.exec(specText);
  if (!m) throw new Error(`engine2_reference_renders: ${SPEC}: B.1's list of base values was not found (did the sentence change shape?)`);
  const custom = /kCustom "([^"]*)"/.exec(m[1]);
  if (!custom) throw new Error(`engine2_reference_renders: ${SPEC}: B.1's base list has no kCustom "..."`);
  const pairs = m[1].replace(custom[0], 'kCustom @').replace(/−/g, '-').split(', ').map(s => s.split(' '));
  const out = [];
  for (const p of pairs) {
    if (p.length !== 2) throw new Error(`engine2_reference_renders: ${SPEC}: base entry "${p.join(' ')}" is not "key value"`);
    const v = p[1] === '@' ? custom[1] : Number(p[1]);
    if (typeof v === 'number' && !Number.isFinite(v)) throw new Error(`engine2_reference_renders: ${SPEC}: base value of ${p[0]} is not a number`);
    if (out.some(([k]) => k === p[0])) throw new Error(`engine2_reference_renders: ${SPEC}: base key ${p[0]} is listed twice`);
    out.push([p[0], v]);
  }
  if (out.length < 90) throw new Error(`engine2_reference_renders: ${SPEC}: only ${out.length} base values were read (expected about 100)`);
  return out;
}

/* A phrase set that does not say what it claims is refused before anything renders on it. */
export function phraseProblems(doc, sr = SR) {
  const bad = [];
  // at another rate the samples are derived by rounding time_s, so time_s is within half a sample of them
  const tol = sr === SR ? 5e-7 : 0.5 / sr + 5e-7;
  if (doc.sample_rate !== sr) bad.push(`sample_rate ${doc.sample_rate}, not ${sr}`);
  if (!Array.isArray(doc.phrases) || doc.phrases.map(p => p.id).join() !== 'held,chord,fast_line,trill') bad.push('the phrases are not held, chord, fast_line, trill in that order');
  for (const p of doc.phrases || []) {
    const ons = new Map(), offs = new Map();
    let last = -1, lastType = null, end = 0;
    p.events.forEach((e, i) => {
      if (!Number.isInteger(e.sample) || e.sample < 0) bad.push(`${p.id} event ${i}: sample ${e.sample}`);
      if (Math.abs(e.time_s - e.sample / sr) > tol) bad.push(`${p.id} event ${i}: time_s ${e.time_s} is not sample ${e.sample} at ${sr}`);
      if (e.sample < last || (e.sample === last && lastType === 'note_on' && e.type === 'note_off')) bad.push(`${p.id} event ${i}: out of order (by sample; key-ups before key-downs)`);
      last = e.sample; lastType = e.type;
      if (e.type === 'note_on') {
        if (ons.has(e.note_id)) bad.push(`${p.id}: note ${e.note_id} starts twice`);
        ons.set(e.note_id, e);
        if (!(e.velocity > 0 && e.velocity <= 1)) bad.push(`${p.id} note ${e.note_id}: velocity ${e.velocity}`);
        // Relative 1e-13, not exact: Math.pow differs in the last bit between macOS and Linux (CI
        // read keys 51 and 63 as off by one ULP). 1e-13 is ~450 ULP; the planted control's +1e-9 Hz
        // is >= 3e-12 relative on every note here, so it still reads red.
        if (!(Math.abs(e.hz - mtof(e.key)) <= 1e-13 * mtof(e.key))) bad.push(`${p.id} note ${e.note_id}: hz ${e.hz} is not 12-TET from key ${e.key}`);
      } else if (e.type === 'note_off') {
        if (offs.has(e.note_id)) bad.push(`${p.id}: note ${e.note_id} ends twice`);
        offs.set(e.note_id, e); end = Math.max(end, e.sample);
      } else bad.push(`${p.id} event ${i}: type ${e.type}`);
    });
    for (const [id, on] of ons) {
      const off = offs.get(id);
      if (!off) bad.push(`${p.id}: note ${id} never ends`);
      else if (off.key !== on.key || off.sample <= on.sample) bad.push(`${p.id}: note ${id} ends before it starts or on another key`);
    }
    if (ons.size !== offs.size) bad.push(`${p.id}: ${ons.size} note-ons, ${offs.size} note-offs`);
    if (p.length_samples !== end) bad.push(`${p.id}: length_samples ${p.length_samples} is not its last note-off, ${end}`);
    if (Math.abs(p.length_s - end / sr) > tol) bad.push(`${p.id}: length_s ${p.length_s} is not ${end} samples`);
  }
  return bad;
}

/* The phrase set at a render rate. The file's samples are at 44.1 kHz and its time_s is the same instant
   in seconds, so at another rate each event's sample is round(time_s * rate) and the length is the last
   note-off's. Rounding is monotone and two events one 44.1 kHz sample apart stay at least one sample
   apart at a higher rate, so the order (key-ups before key-downs at one sample) survives; phraseProblems
   checks it anyway. At the file's own rate the document is returned as it is. */
export function phrasesAt(doc, sr) {
  rateOf(sr);
  if (sr === doc.sample_rate) return doc;
  return {
    ...doc, sample_rate: sr,
    phrases: doc.phrases.map(p => {
      const events = p.events.map(e => ({ ...e, sample: Math.round(e.time_s * sr) }));
      return { ...p, events, length_samples: Math.max(...events.filter(e => e.type === 'note_off').map(e => e.sample)) };
    }),
  };
}

/* Does the engine's mono glide rule act in this phrase? A note-on while another note is held glides
   (simultaneous notes included: a chord in mono glides up its notes); with glideAlways the next note
   glides from the previous one's tail, which sounds while its release runs. The stored glide time must
   exceed 1 ms (engine.h monoOn). Poly mode never glides. */
export function glideActs(params, phrase) {
  if (!(params.polyMode === 1 || params.polyMode === 2) || !(params.glide > 1)) return false;
  const ons = phrase.events.filter(e => e.type === 'note_on');
  const held = new Set();
  let overlap = false;
  for (const e of phrase.events) {
    if (e.type === 'note_on') { if (held.size) overlap = true; held.add(e.note_id); } else held.delete(e.note_id);
  }
  return overlap || (!!params.glideAlways && ons.length > 1);
}

/* the model: presets, resolved, and the renders owed. Pure: reads the inputs and builds. */
export function model() {
  const base = baseValues(read(SPEC));
  const phrases = JSON.parse(read(PHRASES));
  const problems = phraseProblems(phrases);
  for (const sr of Object.keys(RATES).map(Number)) problems.push(...phraseProblems(phrasesAt(phrases, sr), sr).map(p => (sr === SR ? p : `at ${sr} Hz: ${p}`)));
  if (problems.length) throw new Error(`engine2_reference_renders: ${PHRASES} is inconsistent:\n  ${problems.join('\n  ')}`);
  const bank = JSON.parse(read(BANK)).presets, envelopes = JSON.parse(read(ENVELOPES)).presets;
  const gain = presetGain();
  const presets = [];
  const add = (source, list) => list.forEach((p, i) => {
    const id = `${source === 'bank' ? 'bank' : 'env'}-${String(i + 1).padStart(2, '0')}-${slug(p.name)}`;
    const params = {};
    for (const [k, v] of Object.entries(p.params)) params[k] = v;
    const fromBase = [];
    for (const [k, v] of base) if (!(k in params)) { params[k] = v; fromBase.push(k); }
    const unlisted = Object.keys(p.params).filter(k => !base.some(([b]) => b === k));
    if (unlisted.length) throw new Error(`engine2_reference_renders: "${p.name}" has keys with no base value (${unlisted.join(', ')}): B.1 says they take the default of their row, which this file does not know`);
    const mono = params.polyMode === 1 || params.polyMode === 2;
    presets.push({
      id, source, category: p.category, name: p.name, ...(p.shape ? { shape: p.shape } : {}), params, fromBase,
      glide: {
        storedMs: params.glide, glideAlways: params.glideAlways, polyMode: params.polyMode,
        actsInEngine: mono && params.glide > 1,
        why: mono ? (params.glide > 1 ? 'mono/legato with a glide time over 1 ms: the engine glides between overlapping notes, and after every note with glideAlways' : 'mono/legato, but the glide time is 1 ms or less: no glide')
          : 'poly mode: the engine has no glide here, whatever the stored time',
      },
    });
  });
  add('bank', bank); add('envelope', envelopes);
  const ids = new Set(presets.map(p => p.id));
  if (ids.size !== presets.length) throw new Error('engine2_reference_renders: two presets share a file stem');
  return { base, phrases, presets, gain, inputs: { bank: sha256(read(BANK)), envelopes: sha256(read(ENVELOPES)), phrases: sha256(read(PHRASES)), spec_base_values: sha256(JSON.stringify(base)) } };
}

const frames = (ph, sr) => ph.length_samples + TAIL_S * sr;   // ph from phrasesAt(.., sr)
const stem = (preset, ph) => `${preset.id}__${ph.id}`;

/* The settings every render got, as the engine received them. */
export const settings = gain => ({
  gain, ...Object.fromEntries(BEFORE_SNAP), poly: POLY, hostSeed: SEED, hostBlock: BLOCK, voiceCap: 'none',
  order: 'the preset keys, gain, os, frameHold, stealFade, snap, poly, then the phrase (as tools/h2_ab.mjs plays a preset)',
});

/* parameters.json, and the static half of every manifest entry: all of it, without the engine. */
export function expected(m, sr = SR) {
  const parameters = {
    schema: 'horde.engine2.reference-renders.parameters/1',
    note: 'The keys today\'s engine received for each preset: the preset\'s own keys in file order, then the keys it omits at B.1\'s base values (fromBase). Settings are the same for every render. A key listed in notExpressible is one the engine does not have; none was found. Generated by tools/engine2_reference_renders.mjs; do not edit by hand.',
    settings: settings(m.gain),
    presets: m.presets.map(p => ({ ...p, notExpressible: [] })),
  };
  const entries = [];
  for (const p of m.presets) for (const ph of phrasesAt(m.phrases, sr).phrases) {
    entries.push({ file: `${stem(p, ph)}.wav`, preset_id: p.id, preset: p.name, source: p.source, phrase: ph.id, rate: sr, frames: frames(ph, sr), glides: glideActs(p.params, ph) });
  }
  return { parameters, entries };
}

/* ---------------------------------------------------------------- rendering */
function phraseCmds(ph, sr) {
  const cmds = [];
  let at = 0;
  for (const e of ph.events) {
    if (e.sample > at) { cmds.push(['render', e.sample - at]); at = e.sample; }
    cmds.push(e.type === 'note_on' ? ['on', e.key, e.hz, e.velocity] : ['off', e.key]);
  }
  if (frames(ph, sr) > at) cmds.push(['render', frames(ph, sr) - at]);
  return cmds;
}
const setCmd = ([k, v]) => (typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
export function jobOf(preset, ph, gain, sr = SR) {
  const cmds = [...Object.entries(preset.params).map(setCmd), setCmd(['gain', gain]), ...BEFORE_SNAP.map(setCmd), ['snap'], setCmd(['poly', POLY]), ...phraseCmds(ph, sr)];
  return jobFor({ engine: 'composed', sr, seed: SEED, block: BLOCK, cmds, name: stem(preset, ph) });
}

/* peak and rms per channel, of the samples the WAV holds */
function levels(wav) {
  const out = { peak: [], rms: [] };
  for (const ch of [wav.L, wav.R]) {
    let pk = 0, ss = 0;
    for (let i = 0; i < ch.length; i++) { const a = Math.abs(ch[i]); if (a > pk) pk = a; ss += ch[i] * ch[i]; }
    out.peak.push(pk); out.rms.push(Number(Math.sqrt(ss / ch.length).toPrecision(9)));
  }
  return out;
}

function gitOut(args) { return execFileSync('git', args, { cwd: ROOT, encoding: 'utf8' }).trim(); }

/* Asks the engine, one key at a time, which of the keys it does not have. */
function probeKeys(keys, bin, sr) {
  const jobs = keys.map(k => jobFor({ engine: 'composed', sr, seed: SEED, block: BLOCK, name: `probe ${k}`,
    cmds: [k === 'kCustom' ? ['sets', k, '1'] : ['set', k, 1], ['render', 1]] }));
  const res = run(jobs, { bin });
  return new Set(keys.filter((k, i) => res[i].summary.unknownKeys > 0));
}

export function renderAll(m, { out, bin, meta, presetFilter, phraseFilter, sr = SR }) {
  const { asset } = rateOf(sr);
  if (!existsSync(bin)) throw new Error(`engine2_reference_renders: ${bin} is not built (h2/harness/README.md, "Building")`);
  const dirty = gitOut(['status', '--porcelain', '--', 'h2']);
  if (dirty) throw new Error(`engine2_reference_renders: the engine's sources are not committed, so the engine hash would lie:\n${dirty}`);
  mkdirSync(out, { recursive: true });
  const exp = expected(m, sr);
  const presets = m.presets.filter(p => !presetFilter || presetFilter.includes(p.name));
  const phrases = phrasesAt(m.phrases, sr).phrases.filter(p => !phraseFilter || phraseFilter.includes(p.id));
  if (!presets.length || !phrases.length) throw new Error('engine2_reference_renders: --presets / --phrases matched nothing');
  const full = presets.length === m.presets.length && phrases.length === m.phrases.phrases.length;
  if (!full && meta) throw new Error('engine2_reference_renders: --meta is for the full set; a subset writes its metadata into --out only');

  const allKeys = [...new Set([...m.presets.flatMap(p => Object.keys(p.params)), 'gain', ...BEFORE_SNAP.map(([k]) => k), 'poly'])];
  const unknown = probeKeys(allKeys, bin, sr);
  const entries = [];
  const t0 = Date.now();
  for (let a = 0; a < presets.length; a += BATCH_PRESETS) {
    const batch = presets.slice(a, a + BATCH_PRESETS);
    const jobs = [], tags = [];
    for (const p of batch) for (const ph of phrases) { jobs.push(jobOf(p, ph, m.gain, sr)); tags.push([p, ph]); }
    const res = run(jobs, { bin });
    res.forEach((r, i) => {
      const [p, ph] = tags[i], file = `${stem(p, ph)}.wav`;
      const want = [...Object.keys(p.params), 'gain', ...BEFORE_SNAP.map(([k]) => k), 'poly'].filter(k => unknown.has(k)).length;
      if (r.summary.unknownKeys !== want) throw new Error(`${file}: the engine did not know ${r.summary.unknownKeys} key(s), the probe says ${want}`);
      if (r.L.length !== frames(ph, sr)) throw new Error(`${file}: ${r.L.length} frames, expected ${frames(ph, sr)}`);
      for (const ch of [r.L, r.R]) for (let j = 0; j < ch.length; j++) if (!Number.isFinite(ch[j])) throw new Error(`${file}: a non-finite sample at frame ${j}`);
      const buf = encodeWav(r.L, r.R, sr);
      writeFileSync(join(out, file), buf);
      const lv = levels(decodeWav(buf));
      entries.push({ file, preset_id: p.id, preset: p.name, source: p.source, phrase: ph.id, rate: sr, frames: frames(ph, sr), glides: glideActs(p.params, ph),
        sha256: sha256(buf), peak: lv.peak, peak_dbfs: lv.peak.map(dbfs), rms: lv.rms, rms_dbfs: lv.rms.map(dbfs), engine_digest: r.summary.digest });
    });
    process.stdout.write(`  ${Math.min(a + BATCH_PRESETS, presets.length)}/${presets.length} presets, ${((Date.now() - t0) / 1000).toFixed(0)} s\n`);
  }
  const parameters = exp.parameters;
  parameters.presets.forEach(p => { p.notExpressible = Object.keys(p.params).filter(k => unknown.has(k)); });
  const tree = gitOut(['rev-parse', 'HEAD:h2/engine']);
  const manifest = {
    schema: 'horde.engine2.reference-renders.manifest/1',
    note: `The frozen reference renders of the bank (83) and the envelope presets (12) on the four phrases, by today\'s engine${sr === SR ? '' : `, at ${sr} Hz (the same engine, settings and level as the ${SR} Hz set, whose manifest.json is unchanged; the parameters are parameters.json's)`}. The audio is the release asset below, not in git. Generated by tools/engine2_reference_renders.mjs; \`--verify <dir>${sr === SR ? '' : ` --sr ${sr}`}\` checks a copy of the audio against this file. Levels are of the 24-bit samples the WAV holds: peak and rms are linear, per channel (left, right); the dBFS columns are 20*log10 of them, rounded to 0.01.`,
    asset: { release_tag: TAG, name: asset, contents: `the WAV files named below, flat, and a copy of this ${rateOf(sr).manifest}` },
    format: { rate: sr, channels: 2, bits: BITS, normalised: false, tail_s: TAIL_S, length: sr === SR ? 'each phrase\'s length_samples (its last note-off) plus the tail' : 'each phrase\'s length (its last note-off) plus the tail', ...(sr === SR ? {} : { phrase_timing: PHRASE_TIMING }) },
    engine: { git_tree: tree, git_path: 'h2/engine', harness: 'h2/harness/render_cli.cpp, the composed engine\'s adapter, built -O3 -ffp-contract=off', commit: gitOut(['rev-parse', 'HEAD']), toolchain: `${process.platform}-${process.arch}` },
    settings: parameters.settings,
    inputs: m.inputs,
    counts: { presets: presets.length, phrases: phrases.length, renders: entries.length },
    renders: entries,
  };
  if (full) manifest.complete = true;
  const dirs = new Set([out, meta ? resolve(meta) : null].filter(Boolean));
  for (const d of dirs) {
    mkdirSync(d, { recursive: true });
    writeFileSync(join(d, rateOf(sr).manifest), JSON.stringify(manifest, null, 1) + '\n');
    if (sr === SR) writeFileSync(join(d, 'parameters.json'), JSON.stringify(parameters, null, 1) + '\n');   // the other rates share these
  }
  return { manifest, seconds: (Date.now() - t0) / 1000 };
}

/* ---------------------------------------------------------------- the checks */
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

/* a manifest of the given rate against the model; [] when it agrees */
export function manifestProblems(m, manifest, sr = SR) {
  const bad = [];
  const exp = expected(m, sr);
  const { asset } = rateOf(sr);
  const hz = sr === SR ? '' : ` (${sr} Hz)`;
  if (!same(manifest.inputs, m.inputs)) bad.push(`manifest.inputs${hz}: the bank, the envelope presets, the phrases or the spec's base values changed since the renders were made (regenerate them)`);
  if (!same(manifest.settings, exp.parameters.settings)) bad.push(`manifest.settings${hz} differ from what the generator plays`);
  if (manifest.asset.release_tag !== TAG || manifest.asset.name !== asset) bad.push(`manifest.asset${hz} is not the ruled tag and asset name`);
  if (!same(manifest.format, { ...manifest.format, rate: sr, channels: 2, bits: BITS, normalised: false, tail_s: TAIL_S })) bad.push(`manifest.format${hz} differs from the ruling (${sr} Hz, stereo, 24 bit, unnormalised, 2 s tail)`);
  if (sr !== SR && manifest.format.phrase_timing !== PHRASE_TIMING) bad.push(`manifest.format${hz}: phrase_timing is not the rule the generator uses`);
  const got = manifest.renders, want = exp.entries;
  if (got.length !== want.length) bad.push(`manifest${hz} holds ${got.length} renders, the inputs owe ${want.length} (${m.presets.length} presets x ${m.phrases.phrases.length} phrases)`);
  want.forEach((w, i) => {
    const g = got[i];
    if (!g) return;
    for (const k of Object.keys(w)) if (!same(g[k], w[k])) { bad.push(`manifest${hz} render ${i} (${w.file}): ${k} is ${JSON.stringify(g[k])}, the inputs say ${JSON.stringify(w[k])}`); break; }
    if (!/^[0-9a-f]{64}$/.test(g.sha256 || '')) bad.push(`manifest${hz} render ${i} (${w.file}): no sha256`);
    if (!(g.peak && g.peak.length === 2 && g.rms && g.rms.length === 2)) bad.push(`manifest${hz} render ${i} (${w.file}): peak and rms are not per channel`);
  });
  return bad;
}

/* the 48 kHz set is the same engine as the 44.1 kHz set: one tree, or one of them is stale */
export function sameEngineProblems(base, other) {
  return same(base.engine.git_tree, other.engine.git_tree) ? [] : [`manifest-48k.json was made by engine tree ${other.engine.git_tree}, manifest.json by ${base.engine.git_tree} (the two sets must come from one engine)`];
}

export function parametersProblems(m, parameters) {
  const bad = [];
  const exp = expected(m);
  const gp = parameters.presets, wp = exp.parameters.presets;
  if (!same(parameters.settings, exp.parameters.settings)) bad.push('parameters.settings differ from what the generator plays');
  if (gp.length !== wp.length) bad.push(`parameters hold ${gp.length} presets, the inputs have ${wp.length}`);
  wp.forEach((w, i) => { const g = gp[i]; if (g && !same({ ...g, notExpressible: [] }, w)) bad.push(`parameters preset ${i} (${w.name}) differ from the resolved inputs`); });
  return bad;
}

/* the 44.1 kHz metadata against the model; [] when it agrees */
export function staticProblems(m, tracked) {
  return [...manifestProblems(m, tracked.manifest, SR), ...parametersProblems(m, tracked.parameters)];
}

const loadManifest = sr => JSON.parse(read(manifestPath(sr)));
const loadTracked = () => ({ manifest: loadManifest(SR), parameters: JSON.parse(read(PARAMETERS)) });

function staticCheck() {
  const m = model();
  const tracked = loadTracked();
  const m48 = loadManifest(48000);
  const problems = staticProblems(m, tracked);
  let ok = true;
  const line = (pass, what, why) => { console.log(`${pass ? 'PASS' : 'FAIL'}  ${what}${pass || !why ? '' : '\n    ' + why}`); if (!pass) ok = false; };
  line(!problems.length, `reference renders: the tracked metadata agrees with its inputs (${tracked.manifest.renders.length} renders, ${tracked.parameters.presets.length} presets)`, problems.slice(0, 12).join('\n    '));
  const problems48 = [...manifestProblems(m, m48, 48000), ...sameEngineProblems(tracked.manifest, m48)];
  line(!problems48.length, `reference renders: the tracked 48 kHz manifest agrees with its inputs and the 44.1 kHz set's engine (${m48.renders.length} renders)`, problems48.slice(0, 12).join('\n    '));
  line(!phraseProblems(m.phrases).length, 'phrase set: consistent (samples, order, 12-TET pitch, lengths)');
  line(!phraseProblems(phrasesAt(m.phrases, 48000), 48000).length, 'phrase set at 48 kHz: consistent (samples derived from time_s, order, lengths)');

  // the must-fail controls: each plant must read red, with the reason it is planted for
  const clone = x => JSON.parse(JSON.stringify(x));
  const plant = (what, mutate) => { const t = clone(tracked); mutate(t); line(staticProblems(m, t).length > 0, `control: ${what} is refused`); };
  plant('a changed parameter value', t => { const p = t.parameters.presets[0]; const k = Object.keys(p.params)[0]; p.params[k] += 1; });
  plant('a render with the wrong frame count', t => { t.manifest.renders[5].frames += 1; });
  plant('a dropped render', t => { t.manifest.renders.pop(); });
  plant('a wrong glide flag', t => { const r = t.manifest.renders.find(x => x.glides); r.glides = false; });
  plant('an edited input hash', t => { t.manifest.inputs.bank = '0'.repeat(64); });
  plant('a changed setting', t => { t.manifest.settings.os = 2; });
  plant('a missing hash', t => { delete t.manifest.renders[0].sha256; });
  const plant48 = (what, mutate) => { const t = clone(m48); mutate(t); line(manifestProblems(m, t, 48000).length + sameEngineProblems(tracked.manifest, t).length > 0, `control (48 kHz): ${what} is refused`); };
  plant48('a render with the wrong frame count', t => { t.renders[5].frames += 1; });
  plant48('a missing hash', t => { delete t.renders[0].sha256; });
  plant48('a dropped render', t => { t.renders.pop(); });
  plant48('a 44.1 kHz render among them', t => { t.renders[7].rate = SR; });
  plant48('a changed setting', t => { t.settings.os = 2; });
  plant48('the 44.1 kHz asset name', t => { t.asset.name = ASSET; });
  plant48('an edited input hash', t => { t.inputs.phrases = '0'.repeat(64); });
  plant48('another engine tree', t => { t.engine.git_tree = '0'.repeat(40); });
  line(manifestProblems(m, m48, SR).length > 0, 'control (48 kHz): the 48 kHz manifest does not pass as the 44.1 kHz one');
  for (const [what, mutate] of [
    ['a note that never ends', d => { d.phrases[0].events.pop(); }],
    ['a time that is not its sample', d => { d.phrases[1].events[0].time_s = 0.5; }],
    ['a pitch off 12-TET', d => { d.phrases[2].events[0].hz += 1e-9; }],
    ['events out of order', d => { const e = d.phrases[3].events; [e[0], e[1]] = [e[1], e[0]]; }],
  ]) { const d = clone(m.phrases); mutate(d); line(phraseProblems(d).length > 0, `control: ${what} in a phrase is refused`); }
  { const d = clone(phrasesAt(m.phrases, 48000)); d.phrases[1].events[0].sample += 5;
    line(phraseProblems(d, 48000).length > 0, 'control (48 kHz): a derived sample that is not round(time_s * rate) is refused'); }
  line(glideActs({ polyMode: 0, glide: 500, glideAlways: 1 }, m.phrases.phrases[3]) === false, 'control: a poly preset with a stored glide does not glide');
  line(glideActs({ polyMode: 1, glide: 40, glideAlways: 0 }, m.phrases.phrases[3]) === true && glideActs({ polyMode: 1, glide: 40, glideAlways: 0 }, m.phrases.phrases[2]) === false,
    'control: a mono preset glides on the trill\'s overlaps and not on the detached fast line without glideAlways');
  console.log(`engine2_reference_renders --static-check: ${ok ? 'GREEN' : 'RED'}`);
  return ok ? 0 : 1;
}

function verifyDir(dir, sr) {
  const manifest = loadManifest(sr);
  const bad = [];
  for (const r of manifest.renders) {
    const p = join(dir, r.file);
    if (!existsSync(p)) { bad.push(`missing  ${r.file}`); continue; }
    const h = sha256(readFileSync(p));
    if (h !== r.sha256) bad.push(`mismatch ${r.file}: ${h}, the manifest says ${r.sha256}`);
  }
  console.log(bad.length ? bad.slice(0, 40).join('\n') + (bad.length > 40 ? `\n... and ${bad.length - 40} more` : '') : '');
  console.log(`engine2_reference_renders --verify${sr === SR ? '' : ` --sr ${sr}`}: ${bad.length ? `RED, ${bad.length} of ${manifest.renders.length} renders do not match` : `GREEN, all ${manifest.renders.length} renders match the manifest`}`);
  return bad.length ? 1 : 0;
}

/* ---------------------------------------------------------------- the command line */
const arg = (argv, name) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : undefined; };
export function main(argv) {
  const sr = arg(argv, '--sr') === undefined ? SR : Number(arg(argv, '--sr'));
  rateOf(sr);
  if (argv.includes('--static-check')) return staticCheck();
  if (argv.includes('--verify')) return verifyDir(resolve(arg(argv, '--verify') || ''), sr);
  const out = arg(argv, '--out');
  if (!out) {
    console.error('usage: node tools/engine2_reference_renders.mjs --out <dir> [--sr 48000] [--bin <h2_harness_render>] [--meta <dir>] [--presets "A,B"] [--phrases held,trill]\n' +
      '       node tools/engine2_reference_renders.mjs --verify <dir> [--sr 48000]\n       node tools/engine2_reference_renders.mjs --static-check');
    return 2;
  }
  const m = model();
  const { manifest, seconds } = renderAll(m, {
    out: resolve(out), bin: resolve(arg(argv, '--bin') || DEFAULT_BIN), meta: arg(argv, '--meta'), sr,
    presetFilter: arg(argv, '--presets') ? arg(argv, '--presets').split(',') : null,
    phraseFilter: arg(argv, '--phrases') ? arg(argv, '--phrases').split(',') : null,
  });
  console.log(`engine2_reference_renders: ${manifest.renders.length} renders at ${sr} Hz in ${seconds.toFixed(1)} s -> ${resolve(out)}`);
  return 0;
}

// Run only as the entry script (the same test as tools/h2_ab.mjs: two file URLs made the same way)
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); } catch (e) { console.error(`engine2_reference_renders: ${e.message}`); process.exitCode = 2; }
}
