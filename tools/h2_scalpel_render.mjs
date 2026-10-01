/*
 * h2_scalpel_render.mjs — the JS half of tools/h2_scalpel_parity_check.cpp
 * (ROADMAP B332 phase 1a; ADR-187 item 6). It renders every parity scenario
 * through the PROTECTED oracle reference/scalpel/prototype/razor-core.js in Node,
 * seeded, and streams the scenario scripts, the rendered samples (float64) and
 * the oracle's blade-event digests to stdout. The C++ check replays each script
 * through h2/cores/scalpel/razor_core.h and compares. Nothing is written to the
 * repo: the fixtures are regenerated at check time, every time, so the oracle
 * stays the live source of truth (the gen_goldens.mjs idiom).
 *
 * Run by the check itself (it spawns `node tools/h2_scalpel_render.mjs` from the
 * repo root). By hand:
 *   node tools/h2_scalpel_render.mjs --list               scenario names
 *   node tools/h2_scalpel_render.mjs --only 'Crushed' > local/s.bin
 *   build-release/h2_scalpel_parity_check local/s.bin     replay one stream file
 *   node tools/h2_scalpel_render.mjs --bench              JS CPU per voice (Layer-E)
 *
 * THE ORACLE IS NEVER EDITED. Two copies are loaded from its text at run time:
 *   - PRISTINE: `require`d as is. ITS samples are the ones streamed and
 *     compared (critic review 2026-09-28: the parity target is the untouched
 *     file, not a copy of it).
 *   - INSTRUMENTED: an in-memory SCRATCH copy with seven literal insertions that
 *     report blade events (tryE and scan BLEP corrections, blade-window entries)
 *     with the oversampled tick and member id. Each insertion must match exactly
 *     once; if the oracle text moves, this fails loudly instead of counting
 *     nothing. It is rendered alongside every scenario for its event digest
 *     only, and its samples must equal the pristine ones bit for bit (the NI
 *     line; the check's NONINV row), so the counters provably change no
 *     arithmetic on any scenario.
 * The oracle's git blob hash is streamed too (ORACLE), pinning the parity
 * target by content (ADR-187 item 3).
 *
 * SEEDING. Math.random is replaced by mulberry32(seed) around every oracle
 * instance (composed_engine_check.mjs's convention); the C++ consumes the same
 * stream in the same order.
 *
 * CHAOTIC EXCLUSIONS (ADR-065's evidence rule, inherited by ADR-187 item 6). A
 * scenario may be excluded from the parity verdict ONLY by listing it in
 * CHAOTIC below with its reason, and the exclusion is re-justified every run:
 * the oracle is re-rendered with its inputs perturbed by one ULP (every
 * note-on frequency nudged to the next double up — the size of a last-bit libm
 * disagreement, applied to every voice, since a phrase can release the first
 * note before the sensitive passage), and the JS-vs-JS divergence is sent to
 * the check, which refuses the exclusion unless the JS alone breaks the same
 * gate, comparably.
 *
 * Workers: the renders are independent, so they run on worker threads; the
 * stream is written in scenario order regardless of completion order.
 */
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { availableParallelism } from 'node:os';
import { createHash } from 'node:crypto';
import { SR, BLK, mulberry32, loadInstrumented as loadInstrumentedAt, notesFor, presetCmds, on, blocks, PHRASES, bladeRows, rowScenario } from './h2_scenarios.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const ORACLE = join(root, 'reference/scalpel/prototype/razor-core.js');
const PRESETS = join(root, 'reference/scalpel/data/presets.json');
const require = createRequire(import.meta.url);

/* --------------------------------------------------------- the two oracles */
// The instrumented scratch copy's seven insertions live in tools/h2_scenarios.mjs,
// shared with the composed engine's renderer.
const loadInstrumented = () => loadInstrumentedAt(ORACLE);

/* --------------------------------------------------------------- scenarios */
// The ONLY way out of the parity verdict (see header). Each entry: name -> why.
// Every scenario not listed here is held to parity.
// The ONE entry today: the bench's own "(watch)" patch. Cross-member modulation
// at xm 0.7 feeds each member's phase from its neighbour's last output round the
// ring, a feedback loop that amplifies a last-bit difference; measured
// 2026-09-28, the oracle against itself with inputs one ULP apart diverges to
// max 1e-3..1e-2, 80x or more further than the C++ does (max 1.5e-6..2.6e-5), while
// the blade events still agree exactly. The check re-measures this every run.
const XM_RING = 'cross-member modulation ring (xm 0.7): last-bit feedback amplification (ADR-065 class)';
export const CHAOTIC = {
  'P/Starting points / Cross-mod ring (watch) :: chord': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: repeat': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: arp': XM_RING,
};

// The phrases and the blade rows are tools/h2_scenarios.mjs's (PHRASES, bladeRows).

function buildScenarios() {
  const presets = JSON.parse(readFileSync(PRESETS, 'utf8')).presets;
  const out = [];
  for (const pr of presets) {
    const tag = `${pr.category} / ${pr.name}`, root = notesFor(pr.category);
    const phrases = pr.params.polyMode ? ['chord', 'repeat', 'arp', 'legato'] : ['chord', 'repeat', 'arp'];
    for (const ph of phrases) out.push({ name: `P/${tag} :: ${ph}`, sr: SR, seed: 0xC0FFEE, cmds: [...presetCmds(pr.params), ...PHRASES[ph](root)] });
  }
  for (const t of bladeRows()) out.push(rowScenario(t, 0xB332));
  return out;
}

/* ----------------------------------------------------------------- render */
function nextUp(x) {   // the next double above x (x > 0): a one-ULP perturbation
  const f = new Float64Array([x]), b = new BigUint64Array(f.buffer); b[0] += 1n; return f[0];
}
function renderWith(RC, sc, { instrument = false, perturb = false } = {}) {
  const SAVED = Math.random;
  Math.random = mulberry32(sc.seed >>> 0);
  const c = new RC(sc.sr);
  if (instrument) { c.__si = 0; c.voices.forEach((v, i) => { v.__vi = i; }); RC.__log = { count: [0, 0, 0, 0, 0], h1: 2166136261, h2: 0 }; }
  let total = 0;
  for (const cm of sc.cmds) if (cm[0] === 'render') total += cm[1] * cm[2];
  const buf = new Float64Array(total * 2);
  let pos = 0;
  for (const cm of sc.cmds) {
    switch (cm[0]) {
      case 'set': c.set({ [cm[1]]: cm[2] }); break;
      case 'sets': c.set({ [cm[1]]: cm[2] }); break;
      case 'snap': Object.assign(c.s, c.t); break;
      case 'on': c.noteOn(cm[1], perturb ? nextUp(cm[2]) : cm[2], cm[3]); break;
      case 'off': c.noteOff(cm[1]); break;
      case 're': c.msg({ t: 're', note: cm[1], freq: perturb ? nextUp(cm[2]) : cm[2] }); break;   // the bench's retune message
      case 'panic': c.msg({ t: 'panic' }); break;
      case 'render': {
        const L = new Float64Array(cm[1]), R = new Float64Array(cm[1]);
        for (let b = 0; b < cm[2]; b++) { c.render(L, R); for (let i = 0; i < cm[1]; i++) { buf[pos++] = L[i]; buf[pos++] = R[i]; } }
        break;
      }
      default: throw new Error('bad command ' + cm[0]);
    }
  }
  Math.random = SAVED;
  return { buf, log: instrument ? RC.__log : null };
}
function scriptText(i, sc) {
  const L = [`SCN ${i} ${sc.name}`, `SR ${sc.sr}`, `SEED ${sc.seed >>> 0}`];
  for (const cm of sc.cmds) L.push(cm[0] === 'sets' ? `sets ${cm[1]} ${cm[2]}` : cm.join(' '));
  return L.join('\n') + '\n';
}
function diff(a, b) {
  let e = 0, mx = 0;
  for (let i = 0; i < a.length; i++) { const d = Math.abs(a[i] - b[i]); e += d * d; if (d > mx || d !== d) mx = d !== d ? Infinity : d; }
  return { rms: Math.sqrt(e / a.length), max: mx };
}
// The samples streamed are the PRISTINE oracle's; the instrumented copy is
// rendered alongside only for its event digest, and its samples must equal the
// pristine ones bit for bit (NI 1) on every scenario, or the counters are not
// provably non-invasive.
function job(RCi, RCp, i, sc) {
  const { buf } = renderWith(RCp, sc);
  const ins = renderWith(RCi, sc, { instrument: true }), log = ins.log;
  let same = ins.buf.length === buf.length;
  for (let k = 0; same && k < buf.length; k++) if (!Object.is(buf[k], ins.buf[k])) same = false;
  let head = scriptText(i, sc);
  head += `EV ${log.count[1]} ${log.count[2]} ${log.count[3]} ${log.count[4]} ${log.h1} ${log.h2}\nNI ${same ? 1 : 0}\n`;
  if (CHAOTIC[sc.name]) {
    const p = renderWith(RCp, sc, { perturb: true });
    const d = diff(buf, p.buf);
    head += `EXCL ${CHAOTIC[sc.name]}\nSELF ${d.rms} ${d.max}\n`;
  }
  head += `DATA ${buf.length / 2}\n`;
  return { head, buf };
}

/* ------------------------------------------------------------------ main */
const argv = process.argv.slice(2);
const arg = (k, dflt) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : dflt; };

if (!isMainThread) {
  const RCi = loadInstrumented(), RCp = require(ORACLE);
  const all = buildScenarios();
  parentPort.on('message', i => {
    if (i < 0) { process.exit(0); }
    const r = job(RCi, RCp, i, all[i]);
    parentPort.postMessage({ i, head: r.head, buf: r.buf }, [r.buf.buffer]);
  });
} else if (argv.includes('--list')) {
  buildScenarios().forEach((s, i) => console.log(`${i}\t${s.name}`));
} else if (argv.includes('--bench')) {
  bench();
} else {
  await main();
}

async function main() {
  const t0 = process.hrtime.bigint();
  const all = buildScenarios();
  const only = arg('--only', null), re = only ? new RegExp(only) : null;
  const idx = all.map((_, i) => i).filter(i => !re || re.test(all[i].name));
  const out = process.stdout;
  const write = data => new Promise(res => { if (out.write(data)) res(); else out.once('drain', res); });
  await write(`H2SCALPEL 1 ${idx.length}\n`);

  // libm probes (Layer-E, printed by the check, never judged): V8's value of
  // each transcendental the oracle calls, on inputs in the ranges it calls them
  // with, so the check can report how often the platform libm agrees to the bit.
  {
    const r = mulberry32(0x11B3), U = (a, b) => a + (b - a) * r(), n = 20000;
    const probes = {
      sin: () => { const x = r() < 0.5 ? U(-10, 10) : Math.floor(U(0, 70000)) * 127.1 + 311.7; return [x, 0, Math.sin(x)]; },   // phases; hash() arguments
      cos: () => { const x = U(-10, 10); return [x, 0, Math.cos(x)]; },
      exp: () => { const x = U(-60, 5); return [x, 0, Math.exp(x)]; },
      log: () => { const x = r() < 0.5 ? U(1e-9, 1) : U(1, 10); return [x, 0, Math.log(x)]; },
      pow: () => { const b = U(0.01, 4), e = U(-8, 8); return [b, e, Math.pow(b, e)]; },
      // Math.pow(2, x): the core's constant-base-2 sites (spreads, envelopes,
      // cents) which clang lowers to exp2 (non-integral x) or ldexp; the check
      // evaluates this row with std::exp2, i.e. as the core is compiled.
      pow2: () => { const e = U(-8, 8); return [2, e, Math.pow(2, e)]; },
      atan2: () => { const y = U(-1, 1), x = U(-1, 1); return [y, x, Math.atan2(y, x)]; },
      asin: () => { const x = U(0, 0.995); return [x, 0, Math.asin(x)]; },
      tanh: () => { const x = U(-4, 4); return [x, 0, Math.tanh(x)]; },
      sqrt: () => { const x = U(0, 10); return [x, 0, Math.sqrt(x)]; },
      hypot: () => { const x = U(-1, 1), y = U(-1, 1); return [x, y, Math.hypot(x, y)]; },
    };
    for (const [fn, gen] of Object.entries(probes)) {
      const v = new Float64Array(n * 3);
      for (let i = 0; i < n; i++) v.set(gen(), 3 * i);
      await write(`LIBM ${fn} ${n}\n`);
      await write(Buffer.from(v.buffer));
    }
  }

  // The parity target, pinned by content: the oracle file's git blob hash
  // (ADR-187 item 3). The check prints it; h2/README.md's status row pins it and
  // tools/h2_rules_check.py fails when the two disagree.
  {
    const bytes = readFileSync(ORACLE);
    const blob = createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
    await write(`ORACLE reference/scalpel/prototype/razor-core.js ${blob}\n`);
  }

  const W = Math.max(1, Math.min(Number(arg('--workers', availableParallelism() - 1)), idx.length));
  const done = new Map();
  let next = 0, emit = 0, wake = null;
  const workers = [];
  const feed = w => { if (next < idx.length) w.postMessage(idx[next++]); else w.postMessage(-1); };
  // Completion is "every scenario WRITTEN", never "every worker exited": the
  // workers finish long before a slow reader has drained the stream, and an END
  // written early truncates the check (the first version did exactly that).
  let failed = null;
  for (let k = 0; k < W; k++) {
    const w = new Worker(fileURLToPath(import.meta.url), { argv });
    w.on('message', m => { done.set(m.i, m); feed(w); if (wake) { const f = wake; wake = null; f(); } });
    w.on('error', e => { failed = e; if (wake) { const f = wake; wake = null; f(); } });
    workers.push(w); feed(w);
  }
  while (emit < idx.length) {
    if (failed) throw failed;
    const i = idx[emit];
    if (!done.has(i)) { await new Promise(r => { wake = r; }); continue; }
    const m = done.get(i); done.delete(i);
    await write(m.head);
    await write(Buffer.from(m.buf.buffer, m.buf.byteOffset, m.buf.byteLength));
    emit++;
    if (emit % 25 === 0) process.stderr.write(`h2_scalpel_render: ${emit}/${idx.length} scenarios streamed\n`);
  }
  await write(`END ${idx.length}\n`);
  process.stderr.write(`h2_scalpel_render: ${idx.length} scenarios, ${W} workers, ${(Number(process.hrtime.bigint() - t0) / 1e9).toFixed(1)} s\n`);
}

/* ------------------------------------------------------------ Layer-E: CPU */
// JS cost per voice as a fraction of real time, on the classes the brief names:
// the heavy one (Crushed bells: N 6 plus two blades) and a light one (Quarter
// sync: N 1, one blade). One voice held for BENCH_S seconds at 48 kHz, 2x
// oversampling (the oracle's default). A calibration loop of known work is timed
// alongside so a reader can tell a slow machine from a slow engine (B236). The
// C++ half is tools/measure_h2_scalpel.cpp, which replays the same scripts.
function benchScenarios() {
  const presets = JSON.parse(readFileSync(PRESETS, 'utf8')).presets;
  const BENCH_S = 4, nb = Math.round(BENCH_S * SR / BLK);
  return ['Crushed bells', 'Quarter sync'].map(name => {
    const pr = presets.find(p => p.name === name);
    return { name: `BENCH/${name}`, sr: SR, seed: 0xB332, cmds: [...presetCmds(pr.params), on(57), blocks(nb)] };
  });
}
function bench() {
  const RC = require(ORACLE), sc = benchScenarios();
  if (argv.includes('--emit')) {   // the scripts, for the C++ half
    process.stdout.write(`H2SCALPEL 1 ${sc.length}\n`);
    for (const [i, s] of sc.entries()) {
      const { buf } = renderWith(RC, s);
      process.stdout.write(scriptText(i, s) + `EV 0 0 0 0 0 0\nDATA ${buf.length / 2}\n`);
      process.stdout.write(Buffer.from(buf.buffer));
    }
    process.stdout.write(`END ${sc.length}\n`);
    return;
  }
  let x = 0; const c0 = process.hrtime.bigint();
  for (let i = 0; i < 1e8; i++) x = x * 1.0000001 + 1e-9;
  const cal = Number(process.hrtime.bigint() - c0) / 1e6;
  console.log(`calibration: 1e8 dependent multiply-adds in ${cal.toFixed(1)} ms (x=${x.toFixed(3)})`);
  for (const s of sc) {
    renderWith(RC, s);   // warm-up (JIT)
    const best = [];
    for (let r = 0; r < 3; r++) { const t = process.hrtime.bigint(); renderWith(RC, s); best.push(Number(process.hrtime.bigint() - t) / 1e9); }
    const secs = Math.min(...best), audio = s.cmds.filter(c => c[0] === 'render').reduce((a, c) => a + c[1] * c[2], 0) / s.sr;
    console.log(`${s.name.padEnd(22)} JS  ${(100 * secs / audio).toFixed(2)} % of real time per voice (${secs.toFixed(3)} s for ${audio.toFixed(2)} s, best of 3)`);
  }
}
