/*
 * harness.mjs — horde 2's engine harness as a robustness tool uses it. ROADMAP
 * B448 phase 2, P2 (ADR-209). The guide is h2/harness/README.md.
 *
 * A tool written on this file knows no engine. It asks for the enrolled engines
 * and their reference patches, says how to render (rate, host block size, seed,
 * length), and gets back the output and the pre-limiter bus:
 *
 *   import { enrolled, patchesFor, render, features } from '../h2/harness/harness.mjs';
 *   for (const engine of enrolled())
 *     for (const patch of patchesFor(engine)) {
 *       const r = render({ patch, sr: 96000, block: 7, seed: 1, seconds: 2, pre: true });
 *       // r.L r.R (the output), r.preL r.preR (the limiter's input), r.summary.digest
 *       const f = features(r, { from: 0.25, to: 1.25 });
 *       if (f.silent) fail(`${engine}/${patch.name}: silent`);   // the tool's own check goes here
 *     }
 *
 * The rendering is done by the C++ binary h2_harness_render (h2/harness/render_cli.cpp),
 * which holds one adapter per enrolled engine. This file writes it a job file and
 * reads its output file; many jobs go through one process (run()).
 *
 * FEATURES ARE tools/patchspace/metrics.mjs's, and only those: the proposal forbids a
 * second metrics library (section 5), and tools/auhost/analyse.mjs is the precedent
 * for measuring a C++ render with it. features() adds no number of its own.
 *
 * LONG RENDERS are read as they are written (runStream, renderStream; package P6):
 * the caller is handed one chunk at a time and nothing else is kept, so a render
 * of hours needs the same memory as one of seconds.
 *
 * DETERMINISM. No clock and no randomness here. The temporary directory's name is
 * the one thing that differs between runs, and it never reaches a result.
 *
 * NOT under the lab sandbox (tools/labharness/sandbox_guard.mjs), on purpose: this
 * file evaluates no lab or packet text (it parses two JSON files of ours and runs
 * our own binary), and the sandbox forbids the child process it needs.
 */
import { spawn, spawnSync } from 'node:child_process';
import { closeSync, constants as fsConstants, createReadStream, existsSync, mkdtempSync, openSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { analyse } from '../../tools/patchspace/metrics.mjs';

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
export const REGISTRY = 'docs/armor/engines.json';
export const PATCHES = 'h2/harness/patches.json';
// verify full's build directory; H2_HARNESS_BIN or run()'s `bin` option overrides it
export const DEFAULT_BIN = join(ROOT, 'build-release', 'h2_harness_render');
export const mtof = n => 440 * Math.pow(2, (n - 69) / 12);

const readJson = rel => JSON.parse(readFileSync(join(ROOT, rel), 'utf8'));

/* ---------------------------------------------------------------- what is enrolled */
export const loadRegistry = () => readJson(REGISTRY);
export const loadPatches = () => readJson(PATCHES);
/* the adapter ids of every enrolled engine and module, in registry order */
export function enrolled() {
  return loadRegistry().units.filter(u => u.kind === 'engine' || u.kind === 'module').map(u => u.id);
}
export function patchesFor(engine) { return loadPatches().patches.filter(p => p.engine === engine); }
export function patch(id) {
  const p = loadPatches().patches.find(q => q.id === id);
  if (!p) throw new Error(`harness: no reference patch "${id}" in ${PATCHES}`);
  return p;
}

/* ---------------------------------------------------------------- building a job */
const setCmd = ([k, v]) => (typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
/* A patch as commands, played the way patches.json says: its sets, then the caller's
   overrides (so an override starts AT its value, like the patch's own), then snap,
   then the cell's sets. */
export function patchCmds(p, overrides) {
  const cell = loadPatches().cell;
  return [...p.sets.map(setCmd), ...Object.entries(overrides || {}).map(setCmd), ['snap'], ...cell.sets.map(setCmd)];
}
/* One render request -> a job.
     patch      a reference patch (an object from patchesFor / patch(), or its id)
     engine     the adapter id; defaults to the patch's
     sr         sample rate (48000)        seed   the host random stream (1)
     block      host block size in frames (128), or { vary: max } for sizes drawn 1..max
     seconds    total length (1)           hold   seconds until the notes are released
                                                  (default: held to the end)
     notes vel  default to the cell's (the ledger's 8-voice chord)
     overrides  { key: value } set after the patch, before snap
     pre        also return the pre-limiter bus (false)
     cmds       REPLACES the default script after `begin` (patchCmds, notes, render):
                a tool that sweeps parameters or churns notes writes its own, from
                the command words in harness.h, and may start from patchCmds()   */
export function jobFor(req) {
  const p = typeof req.patch === 'string' ? patch(req.patch) : req.patch;
  const cell = loadPatches().cell;
  const sr = req.sr || 48000, seconds = req.seconds === undefined ? 1 : req.seconds;
  const block = req.block === undefined ? 128 : req.block;
  const job = {
    name: req.name || `${p ? p.id : 'script'} sr=${sr} block=${typeof block === 'object' ? 'vary' + block.vary : block} seed=${req.seed === undefined ? 1 : req.seed}`,
    engine: req.engine || (p && p.engine), sr, seed: (req.seed === undefined ? 1 : req.seed) >>> 0,
    block: typeof block === 'object' ? block.vary : block, vary: typeof block === 'object', pre: !!req.pre,
  };
  if (!job.engine) throw new Error('harness: a job needs an engine (or a patch that names one)');
  if (req.cmds) { job.cmds = req.cmds; return job; }
  if (!p) throw new Error('harness: a job needs a patch or its own cmds');
  const notes = req.notes || cell.notes, vel = req.vel === undefined ? cell.vel : req.vel;
  const total = Math.round(seconds * sr), held = req.hold === undefined ? total : Math.min(total, Math.round(req.hold * sr));
  job.cmds = [...patchCmds(p, req.overrides), ...notes.map(n => ['on', n, mtof(n), vel]), ['render', held]];
  if (held < total) job.cmds.push(...notes.map(n => ['off', n]), ['render', total - held]);
  return job;
}

/* the job language (harness.h). Numbers are JS's shortest round-trip spelling, which
   strtod reads back as the same double. */
export function jobText(jobs) {
  const out = ['H2HARNESS 1'];
  for (const j of jobs) {
    if (/[\r\n]/.test(j.name)) throw new Error('harness: a job name cannot hold a line break');
    out.push(`JOB ${j.name}`, `engine ${j.engine}`, `sr ${j.sr}`, `seed ${j.seed}`,
      `block ${j.vary ? 'vary ' : ''}${j.block}`, `pre ${j.pre ? 1 : 0}`, 'begin');
    for (const c of j.cmds) {
      if (c[0] === 'sets' && /[\r\n]/.test(c[2])) throw new Error('harness: a string value cannot hold a line break');
      out.push(c.join(' '));
    }
    out.push('END');
  }
  return out.join('\n') + '\n';
}

/* ---------------------------------------------------------------- running jobs */
/* the output file (render_cli.cpp's header) -> one result per job */
export function parseOutput(buf) {
  let p = 0;
  const line = () => {
    const e = buf.indexOf(0x0a, p);
    if (e < 0) throw new Error('harness: the output file ends mid-line (the render was cut short)');
    const s = buf.toString('utf8', p, e); p = e + 1; return s;
  };
  if (line() !== 'H2HARNESS-OUT 1') throw new Error('harness: not an h2_harness_render output file');
  const results = [];
  for (;;) {
    const head = line();
    if (head.startsWith('END ')) {
      if (Number(head.slice(4)) !== results.length) throw new Error(`harness: END says ${head.slice(4)} jobs, read ${results.length}`);
      return results;
    }
    if (!head.startsWith('JOB ')) throw new Error(`harness: expected JOB, read "${head.slice(0, 60)}"`);
    const chunks = [];
    let frames = 0, channels = 0, summary = null;
    for (;;) {
      const l = line();
      if (l.startsWith('DONE ')) { summary = JSON.parse(l.slice(5)); break; }
      const m = /^CHUNK (\d+) (\d+)$/.exec(l);
      if (!m) throw new Error(`harness: expected CHUNK or DONE, read "${l.slice(0, 60)}"`);
      const n = Number(m[1]), ch = Number(m[2]), bytes = n * ch * 8;
      if (p + bytes > buf.length) throw new Error('harness: the output file ends inside a chunk');
      // copied, so the Float64Array is aligned whatever the chunk's offset in the file
      chunks.push([n, new Float64Array(buf.buffer.slice(buf.byteOffset + p, buf.byteOffset + p + bytes))]);
      p += bytes; frames += n; channels = ch;
    }
    if (summary.frames !== frames) throw new Error(`harness: job "${head.slice(4)}" says ${summary.frames} frames, read ${frames}`);
    const r = { name: head.slice(4), summary, sr: summary.sr, L: new Float64Array(frames), R: new Float64Array(frames), preL: null, preR: null };
    if (channels === 4) { r.preL = new Float64Array(frames); r.preR = new Float64Array(frames); }
    let at = 0;
    for (const [n, d] of chunks) {
      for (let i = 0; i < n; i++) {
        r.L[at + i] = d[i * channels]; r.R[at + i] = d[i * channels + 1];
        if (channels === 4) { r.preL[at + i] = d[i * channels + 2]; r.preR[at + i] = d[i * channels + 3]; }
      }
      at += n;
    }
    results.push(r);
  }
}

export function binPath(opt) { return (opt && opt.bin) || process.env.H2_HARNESS_BIN || DEFAULT_BIN; }

/* Runs jobs through one h2_harness_render process; results are in job order. Throws
   if the binary is missing or refuses the request: a job that did not run must
   never read as a job that passed. */
export function run(jobs, opt) {
  const bin = binPath(opt);
  if (!existsSync(bin)) throw new Error(`harness: ${bin} is not built (see h2/harness/README.md, "Building")`);
  const dir = mkdtempSync(join(tmpdir(), 'h2harness-'));
  try {
    const jf = join(dir, 'jobs.txt'), of = join(dir, 'out.bin');
    writeFileSync(jf, jobText(jobs));
    const r = spawnSync(bin, ['--jobs', jf, '--out', of], { encoding: 'utf8' });
    if (r.status !== 0) throw new Error(`harness: h2_harness_render exited ${r.status}: ${(r.stderr || '').trim() || r.error}`);
    return parseOutput(readFileSync(of));
  } finally { rmSync(dir, { recursive: true, force: true }); }
}
export function render(req, opt) { return run([jobFor(req)], opt)[0]; }

/* ---------------------------------------------------------------- reading a render as it is written
 * B448 phase 2, P6 (the soak). run() holds a whole render in memory: 16 bytes a frame,
 * 32 with the pre-limiter bus, so four hours at 192 kHz would be 88 GB. runStream()
 * plays the same jobs through the same binary and hands the caller each CHUNK as it
 * arrives:
 *
 *   const done = await runStream(jobs, c => { ... }, { bin, digest: true });
 *     c.job c.name     which job (its index in `jobs`, and its name)
 *     c.at c.frames    frames of this job before this chunk, and in it
 *     c.L c.R          Float64Array views of the output
 *     c.preL c.preR    the limiter's input, or null if the job did not ask (`pre`)
 *   done[k] = { name, summary, frames, digestRead }   in job order
 *
 * THE VIEWS ARE VALID ONLY DURING THE CALL. They are windows on buffers this file
 * reuses for every chunk. A caller that wants samples later copies them.
 *
 * THE BOUND ON MEMORY. One chunk is at most STREAM_MAX_FRAMES frames (harness.h's
 * kStretch; a larger one stops the run). Held at any time: the chunk's bytes as they
 * arrive and the same samples de-interleaved, 2 x 32768 x 4 x 8 bytes = 2 MiB, plus
 * one read of the pipe (64 KiB). It does not grow with the length of the render or
 * the number of jobs. The callback is called synchronously and the pipe is read only
 * between calls, so a slow callback slows the render tool down and never queues its
 * output.
 *
 * HOW. The render tool writes to a path, and this file cannot change it. So the path
 * is a named pipe (mkfifo) in a temporary directory: nothing is written to disk. The
 * pipe is opened for reading the ordinary, blocking way, which returns only once the
 * tool has opened its end, and its end of file is the tool closing that end: no
 * early or missed end of file is possible. NOT ON WINDOWS: it needs mkfifo.
 * Two simpler ways were tried and do not work (2026-10-11): `/dev/fd/3` on an
 * ordinary child pipe (Node's child pipes are sockets, and Linux refuses to open a
 * socket by path), and reading the pipe through a non-blocking net.Socket (on macOS
 * the end of file never arrived).
 *
 * THE SAME CHECKS AS parseOutput, as the bytes arrive: the header, CHUNK and DONE
 * lines, each job's frame count against its summary, the END count. A stream that
 * stops early, a tool that exits non-zero and a tool that dies on a signal all
 * reject (the last with `.crash`, the signal's name): a render that was cut short
 * must never read as one that finished. An exception thrown by the callback stops
 * the render tool and rejects with it.
 *
 * opt.digest: also compute harness.h's digestFrames over the samples READ and
 * reject if it differs from the summary's digest: every output sample this file
 * handed out is then one the tool rendered, in order. It costs time (16 byte steps
 * a frame), so it is the caller's choice. */
export const STREAM_MAX_FRAMES = 1 << 15;   // harness.h kStretch: the largest chunk the tool writes

/* FNV-1a 64 over output bytes, as harness.h's digestFrames. The state is two 32-bit
   halves; h * 0x100000001b3 is (h << 40) + h * 0x1b3, and each product below is
   under 2^53, so exact in a double. `bytes` is the chunk as it arrived (float64 in
   the host's byte order, `stride` bytes a frame, L then R first), so no sample is
   re-encoded on the way to the digest. */
function fnvFrames(state, bytes, frames, stride) {
  let hi = state[0], lo = state[1];
  for (let f = 0, p = 0; f < frames; f++, p += stride) {
    for (let k = 0; k < 16; k++) {
      lo = (lo ^ bytes[p + k]) >>> 0;
      const m = lo * 0x1b3;
      hi = (hi * 0x1b3 + lo * 256 + Math.floor(m / 4294967296)) >>> 0;
      lo = m >>> 0;
    }
  }
  state[0] = hi; state[1] = lo;
}
const hex8 = x => x.toString(16).padStart(8, '0');

export function runStream(jobs, onChunk, opt) {
  const bin = binPath(opt), wantDigest = !!(opt && opt.digest);
  if (!existsSync(bin)) return Promise.reject(new Error(`harness: ${bin} is not built (see h2/harness/README.md, "Building")`));
  return new Promise((resolve, reject) => {
    const dir = mkdtempSync(join(tmpdir(), 'h2harness-'));
    let child = null, stream = null, settled = false;
    const finish = (err, val) => {
      if (settled) return;
      settled = true;
      // the tool first: a read still waiting on the pipe returns once the writing end is gone
      if (child && child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
      if (stream) stream.destroy();
      rmSync(dir, { recursive: true, force: true });
      if (err) reject(err); else resolve(val);
    };
    try {
      const jf = join(dir, 'jobs.txt'), pipe = join(dir, 'out.pipe');
      writeFileSync(jf, jobText(jobs));
      const mk = spawnSync('mkfifo', [pipe], { encoding: 'utf8' });
      if (mk.status !== 0) throw new Error(`harness: mkfifo failed (${(mk.stderr || '').trim() || mk.error}); the streaming read needs a named pipe`);

      // the parser: lines, and after a CHUNK line that many bytes
      const raw = new Uint8Array(new ArrayBuffer(STREAM_MAX_FRAMES * 4 * 8)), rawF = new Float64Array(raw.buffer);
      const L = new Float64Array(STREAM_MAX_FRAMES), R = new Float64Array(STREAM_MAX_FRAMES);
      const pL = new Float64Array(STREAM_MAX_FRAMES), pR = new Float64Array(STREAM_MAX_FRAMES);
      const results = [];
      let lineBytes = [], lineLen = 0, need = 0, have = 0, chFrames = 0, chChannels = 0;
      let state = 'header', job = -1, name = '', at = 0, ended = false, fnv = [0, 0];
      const fail = why => { throw new Error(`harness: ${why}`); };
      const onPayload = () => {
        const ch = chChannels, n = chFrames;
        if (ch === 4) for (let i = 0, k = 0; i < n; i++, k += 4) { L[i] = rawF[k]; R[i] = rawF[k + 1]; pL[i] = rawF[k + 2]; pR[i] = rawF[k + 3]; }
        else for (let i = 0, k = 0; i < n; i++, k += 2) { L[i] = rawF[k]; R[i] = rawF[k + 1]; }
        if (wantDigest) fnvFrames(fnv, raw, n, ch * 8);
        need = 0;
        onChunk({ job, name, at, frames: n, L: L.subarray(0, n), R: R.subarray(0, n), preL: ch === 4 ? pL.subarray(0, n) : null, preR: ch === 4 ? pR.subarray(0, n) : null });
        at += n;
      };
      const onLine = l => {
        if (state === 'header') { if (l !== 'H2HARNESS-OUT 1') fail('not an h2_harness_render output stream'); state = 'job'; return; }
        if (state === 'end') fail('bytes after END');
        if (state === 'job') {
          if (l.startsWith('END ')) {
            if (Number(l.slice(4)) !== results.length) fail(`END says ${l.slice(4)} jobs, read ${results.length}`);
            state = 'end'; ended = true; return;
          }
          if (!l.startsWith('JOB ')) fail(`expected JOB, read "${l.slice(0, 60)}"`);
          job++; name = l.slice(4); at = 0; fnv = [0xcbf29ce4, 0x84222325]; state = 'chunk'; return;   // the FNV-1a 64 offset basis
        }
        if (l.startsWith('DONE ')) {
          const summary = JSON.parse(l.slice(5));
          if (summary.frames !== at) fail(`job "${name}" says ${summary.frames} frames, read ${at}`);
          const digestRead = wantDigest ? hex8(fnv[0]) + hex8(fnv[1]) : null;
          if (wantDigest && digestRead !== summary.digest) fail(`job "${name}": the samples read digest to ${digestRead}, the tool's summary says ${summary.digest}`);
          results.push({ name, summary, frames: at, digestRead });
          state = 'job'; return;
        }
        const m = /^CHUNK (\d+) (\d+)$/.exec(l);
        if (!m) fail(`expected CHUNK or DONE, read "${l.slice(0, 60)}"`);
        chFrames = Number(m[1]); chChannels = Number(m[2]);
        if (chChannels !== 2 && chChannels !== 4) fail(`a chunk of ${chChannels} channels`);
        if (chFrames > STREAM_MAX_FRAMES) fail(`a chunk of ${chFrames} frames is over the bound of ${STREAM_MAX_FRAMES}`);
        need = chFrames * chChannels * 8; have = 0;
        if (need === 0) onPayload();
      };
      const feed = buf => {
        let p = 0;
        while (p < buf.length) {
          if (need > 0) {
            const take = Math.min(need - have, buf.length - p);
            raw.set(buf.subarray(p, p + take), have);
            have += take; p += take;
            if (have === need) onPayload();
            continue;
          }
          const e = buf.indexOf(0x0a, p);
          if (e < 0) {   // a line cut by the end of this read: keep its start (a copy; `buf` is the pipe's)
            lineBytes.push(Buffer.from(buf.subarray(p))); lineLen += buf.length - p;
            if (lineLen > 65536) fail('a line of more than 64 KiB: not an output stream');
            return;
          }
          const l = lineBytes.length ? Buffer.concat([...lineBytes, buf.subarray(p, e)]).toString('utf8') : buf.toString('utf8', p, e);
          lineBytes = []; lineLen = 0; p = e + 1;
          onLine(l);
        }
      };

      let stderr = '', exit = null, eof = false;
      const maybeDone = () => {
        if (exit === null || !eof) return;
        if (exit.code === null) return finish(Object.assign(new Error(`h2_harness_render died on ${exit.signal}`), { crash: exit.signal }));
        if (exit.code !== 0) return finish(new Error(`harness: h2_harness_render exited ${exit.code}: ${stderr.trim()}`));
        if (!ended || need > 0 || lineBytes.length) return finish(new Error('harness: the output stream ended before END (the render was cut short)'));
        finish(null, results);
      };
      let opened = false;
      // 64 KiB a read, no more: fs allocates a buffer of this size for EVERY read, and a pipe
      // hands over 8 to 64 KiB at a time. (1 MiB here cost five times the CPU, measured.)
      stream = createReadStream(pipe, { highWaterMark: 1 << 16 });
      stream.on('open', () => { opened = true; });
      stream.on('data', buf => { if (settled) return; try { feed(buf); } catch (e) { finish(e); } });
      stream.on('end', () => { eof = true; maybeDone(); });
      stream.on('error', e => finish(new Error(`harness: reading the output pipe failed: ${e.message}`)));
      // A tool that exits before it opens the pipe (a job file it cannot read) would leave
      // the blocking open above waiting for ever. Opening the writing end here, without
      // blocking, and closing it again lets that open return and read end of file. It is
      // refused (ENXIO) until the reading open has reached the kernel, hence the retry.
      const release = () => {
        if (opened || settled) return;
        try { closeSync(openSync(pipe, fsConstants.O_WRONLY | fsConstants.O_NONBLOCK)); } catch { setImmediate(release); }
      };
      child = spawn(bin, ['--jobs', jf, '--out', pipe], { stdio: ['ignore', 'ignore', 'pipe'] });
      child.stderr.on('data', d => { stderr += d; });
      child.on('error', e => { exit = { code: 2, signal: null }; stderr += `could not start: ${e.message}`; release(); maybeDone(); });
      child.on('close', (code, signal) => { exit = { code, signal }; release(); maybeDone(); });
    } catch (e) { finish(e); }
  });
}
/* One render request (jobFor's fields), streamed. Resolves to { name, summary, frames, digestRead }. */
export function renderStream(req, onChunk, opt) { return runStream([jobFor(req)], onChunk, opt).then(r => r[0]); }

/* the binary's own adapter table: [{ id, control }] */
export function listAdapters(opt) {
  const r = spawnSync(binPath(opt), ['--list'], { encoding: 'utf8' });
  if (r.status !== 0) throw new Error(`harness: h2_harness_render --list exited ${r.status}`);
  return r.stdout.trim().split('\n').map(l => { const [id, kind] = l.split(' '); return { id, control: kind === 'control' }; });
}

/* ---------------------------------------------------------------- shared features */
/* metrics.mjs's analyse() over [from, to) seconds of a result's OUTPUT (the whole
   render by default). noteHz is the note root() is asked about; the default is the
   lowest note of the cell's chord (analyse.mjs: "a chord's bass note is the honest
   choice"). What metrics.mjs has and has not is listed in the README. */
export function features(r, opt) {
  opt = opt || {};
  const a = Math.round((opt.from || 0) * r.sr), z = opt.to === undefined ? r.L.length : Math.min(r.L.length, Math.round(opt.to * r.sr));
  const noteHz = opt.noteHz || mtof(Math.min(...loadPatches().cell.notes));
  const m = analyse(r.L.subarray(a, z), r.R.subarray(a, z), r.sr, noteHz);
  delete m._S;
  return m;
}
