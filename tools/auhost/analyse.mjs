/*
 * analyse.mjs — runs tools/patchspace/metrics.mjs on WAVs the offline AU host
 * (tools/auhost/auhost.cpp) wrote. HYPERSAW, 2026-10-01, ROADMAP B381 stage 1.
 *
 * A MEASUREMENT, NOT A GATE (Layer-E): it prints numbers and judges nothing.
 * It adds no metric of its own; every number is metrics.mjs's, so the gauntlet's
 * renders and an external reference are read by the same code.
 *
 * Usage (repo root):
 *   node tools/auhost/analyse.mjs <test.wav> [--note 60] [--from 0.25] [--to 2.25]
 *        [--ref <same script rendered at 4x the rate>.wav]
 * --note is the MIDI note root() is asked about (a chord's bass note is the honest
 * choice: a chord's root is that note, and root() reports the interval if not).
 * --ref enables aliasing(): the reference must be the SAME script rendered at
 * exactly 4x the test's rate (auhost --sr 192000 against --sr 48000). The two are
 * cut to the same seconds and transformed with the same bin width (N x4 at 4x the
 * rate, so the frames start at the same times), which is what aliasing() requires.
 * For a plug-in that cannot render at 4x, aliasing is not measured: say so, do not
 * substitute another reference.
 */
import { readFileSync } from 'node:fs';
import { analyse, aliasing, spectrum, mono } from '../patchspace/metrics.mjs';

/* float32 or 16/24-bit PCM WAV -> {sr, L, R}. Chunks are walked, never assumed. */
export function readWav(path) {
  const b = readFileSync(path);
  if (b.toString('ascii', 0, 4) !== 'RIFF' || b.toString('ascii', 8, 12) !== 'WAVE') throw new Error(`${path}: not a RIFF/WAVE file`);
  let p = 12, fmt = null, data = null;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4), n = b.readUInt32LE(p + 4);
    if (id === 'fmt ') fmt = { tag: b.readUInt16LE(p + 8), ch: b.readUInt16LE(p + 10), sr: b.readUInt32LE(p + 12), bits: b.readUInt16LE(p + 22) };
    if (id === 'data') data = { off: p + 8, n: Math.min(n, b.length - p - 8) };
    p += 8 + n + (n & 1);
  }
  if (!fmt || !data) throw new Error(`${path}: no fmt or data chunk`);
  const tag = fmt.tag === 0xfffe ? (fmt.bits === 32 ? 3 : 1) : fmt.tag;   // WAVE_FORMAT_EXTENSIBLE: trust the bit depth
  const bps = fmt.bits / 8, frames = Math.floor(data.n / (bps * fmt.ch));
  const L = new Float64Array(frames), R = new Float64Array(frames);
  const at = (i, c) => {
    const o = data.off + (i * fmt.ch + c) * bps;
    if (tag === 3 && bps === 4) return b.readFloatLE(o);
    if (tag === 1 && bps === 2) return b.readInt16LE(o) / 32768;
    if (tag === 1 && bps === 3) return (b.readIntLE(o, 3)) / 8388608;
    throw new Error(`${path}: unsupported format tag ${fmt.tag} / ${fmt.bits} bit`);
  };
  for (let i = 0; i < frames; i++) { L[i] = at(i, 0); R[i] = at(i, fmt.ch > 1 ? 1 : 0); }
  return { sr: fmt.sr, L, R };
}

const cut = (w, a, z) => { const i = Math.round(a * w.sr), j = Math.min(w.L.length, Math.round(z * w.sr)); return { L: w.L.subarray(i, j), R: w.R.subarray(i, j) }; };

function main(argv) {
  const opt = { note: 60, from: 0.25, to: 2.25, ref: null };
  const files = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--note') opt.note = Number(argv[++i]);
    else if (a === '--from') opt.from = Number(argv[++i]);
    else if (a === '--to') opt.to = Number(argv[++i]);
    else if (a === '--ref') opt.ref = argv[++i];
    else files.push(a);
  }
  if (files.length !== 1) { console.error('usage: node tools/auhost/analyse.mjs <test.wav> [--note 60] [--from s] [--to s] [--ref 4x.wav]'); return 2; }
  const w = readWav(files[0]), noteHz = 440 * Math.pow(2, (opt.note - 69) / 12);
  const t = cut(w, opt.from, opt.to);
  const m = analyse(t.L, t.R, w.sr, noteHz);
  delete m._S;
  const out = { file: files[0], sr: w.sr, window: [opt.from, opt.to], note: opt.note, metrics: m };
  if (opt.ref) {
    const r = readWav(opt.ref);
    if (r.sr !== 4 * w.sr) throw new Error(`--ref must be rendered at 4x the test's rate (${4 * w.sr}), got ${r.sr}`);
    const rc = cut(r, opt.from, opt.to);
    out.aliasing = aliasing(spectrum(mono(t.L, t.R), w.sr), spectrum(mono(rc.L, rc.R), r.sr, 4 * 8192));
    out.aliasingRef = opt.ref;
  }
  console.log(JSON.stringify(out, (k, v) => (typeof v === 'number' && !Number.isInteger(v) ? Number(v.toPrecision(5)) : v), 2));
  return 0;
}

if (import.meta.url === `file://${process.argv[1]}`) process.exitCode = main(process.argv.slice(2));
