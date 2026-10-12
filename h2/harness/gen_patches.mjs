/*
 * gen_patches.mjs — writes h2/harness/patches.json, the harness's reference
 * patches. ROADMAP B448 phase 2, P2 (ADR-209): "the seven ledger presets of
 * docs/port/cpu-ledger.md plus extreme corners".
 *
 * NOTHING HERE IS TYPED IN BY HAND. Every value is read from a file that already
 * owns it, and each patch records which:
 *   the seven ledger NAMES   the kLedger array of tools/measure_h2_engine.cpp, the
 *                            frozen protocol's implementation (docs/port/cpu-ledger.md)
 *   the presets' VALUES      reference/scalpel/data/presets.json (the bank), played as
 *                            the parity scenarios play them: the params in the bank's
 *                            order, then gain 0.35 (tools/h2_scenarios.mjs presetCmds,
 *                            restated as PRESET_GAIN because that file evaluates lab
 *                            text under the lab sandbox and this one must not).
 *                            "defaults" is the engine with no preset, as in the ledger.
 *   the CELL                 the ledger's 8-voice cell: poly 8, keys 48 + 3k, velocity
 *                            0.85 (tools/measure_h2_engine.cpp, "THE CELL").
 *   the corners' RANGES      tools/patchspace/dependency_tree.json, which carries the
 *                            declared range of every parameter the engine voices (the
 *                            lab's own table; `./verify full` keeps that file fresh).
 *                            The C++ engine declares defaults (h2/engine/blade.h) and no
 *                            ranges, so this table is the one declaration there is.
 *
 * THE CORNERS are whole-table rules, so no parameter is picked by judgement:
 *   corner/continuous-max              every continuous parameter at its declared maximum
 *   corner/continuous-max-toggles-on   the same, and every toggle on (blade 2 sounds)
 *   corner/continuous-min              every continuous parameter at its declared minimum
 * Enumerations keep the engine's defaults: an enumeration has no extreme. The last
 * corner sets gain to 0, its declared minimum, so it must render silence: it is the
 * set's must-read-zero patch, and `expect` says so. A patch's `expect` is a claim the
 * harness selftest tests whenever it runs (h2/harness/selftest.mjs).
 *
 * Usage (repo root):
 *   node h2/harness/gen_patches.mjs            rewrite h2/harness/patches.json
 *   node h2/harness/gen_patches.mjs --check    exit 1 if the committed file is stale
 * Deterministic: no clock, no randomness; the same sources give the same bytes.
 */
import { readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
export const OUT = 'h2/harness/patches.json';
const BANK = 'reference/scalpel/data/presets.json';
const TREE = 'tools/patchspace/dependency_tree.json';
const LEDGER = 'tools/measure_h2_engine.cpp';
const ENGINE = 'composed';
const PRESET_GAIN = 0.35;   // tools/h2_scenarios.mjs presetCmds: "render-goldens.js's gain override"

const read = rel => readFileSync(join(ROOT, rel), 'utf8');
const slug = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');

/* The one occurrence of `anchor` in `text`, or a throw: a source that changed shape
   must stop the generator, never be sliced in the wrong place. */
function once(text, anchor, what) {
  const n = text.split(anchor).length - 1;
  if (n !== 1) throw new Error(`gen_patches: ${what}: anchor "${anchor}" found ${n} times (want 1)`);
  return text.indexOf(anchor);
}

function ledgerNames(src) {
  const a = once(src, 'const char* const kLedger[] = {', `${LEDGER} kLedger`), z = src.indexOf('};', a);
  const names = [...src.slice(a, z).matchAll(/"([^"]+)"/g)].map(m => m[1]);
  if (names.length !== 7) throw new Error(`gen_patches: ${LEDGER} kLedger has ${names.length} names; the brief and the ledger say seven`);
  return names;
}
function ledgerCell(src) {
  // the cell's three facts, each read from the line that implements it
  const poly = /e\[k\]->set\("poly", (\d+)\);/.exec(src);
  const key = /const int note = (\d+) \+ (\d+) \* v;/.exec(src);
  const vel = /->noteOn\(note, 440 \* std::pow\(2, \(note - 69\) \/ 12\.0\), ([0-9.]+)\);/.exec(src);
  if (!poly || !key || !vel) throw new Error(`gen_patches: ${LEDGER}: the cell's poly, key or velocity line was not found`);
  const voices = Number(poly[1]);
  return {
    source: `${LEDGER} runCell (the frozen protocol's 8-voice cell; docs/port/cpu-ledger.md, Protocol)`,
    sets: [['poly', voices]],
    notes: Array.from({ length: voices }, (_, v) => Number(key[1]) + Number(key[2]) * v),
    vel: Number(vel[1]),
  };
}

export function build() {
  const bank = JSON.parse(read(BANK)).presets, tree = JSON.parse(read(TREE)), ledgerSrc = read(LEDGER);
  const patches = [];

  for (const name of ledgerNames(ledgerSrc)) {
    if (name === 'defaults') {
      patches.push({ id: 'ledger/defaults', engine: ENGINE, kind: 'ledger', name, expect: 'loud',
        source: `the engine with no preset, as the ledger's "defaults" row (${LEDGER}: "defaults" is no preset)`, sets: [] });
      continue;
    }
    const hits = bank.filter(p => p.name === name);
    if (hits.length !== 1) throw new Error(`gen_patches: ledger preset "${name}" is in ${BANK} ${hits.length} times (want 1)`);
    const sets = Object.entries(hits[0].params);
    sets.push(['gain', PRESET_GAIN]);
    patches.push({ id: `ledger/${slug(name)}`, engine: ENGINE, kind: 'ledger', name, expect: 'loud',
      source: `${BANK} "${name}" (${hits[0].category}): its params in the bank's order, then gain ${PRESET_GAIN} (tools/h2_scenarios.mjs presetCmds)`, sets });
  }

  const params = Object.entries(tree.params);
  const cont = params.filter(([, p]) => p.kind === 'c');
  // b1on is the lab's mapping (off means w 0), not an engine key: on is the engine as it is
  const toggles = params.filter(([k, p]) => p.kind === 't' && k !== 'b1on');
  for (const [, p] of cont) if (typeof p.range.min !== 'number' || typeof p.range.max !== 'number') throw new Error('gen_patches: a continuous parameter has no declared range');
  const corner = (id, end, withToggles, why) => {
    const sets = cont.map(([k, p]) => [k, p.range[end]]);
    if (withToggles) for (const [k] of toggles) sets.push([k, 1]);
    // gain multiplies the bus just before the limiter (engine.h: tanh(y * gain * 1.6)), so gain 0 is silence
    const silent = sets.some(([k, v]) => k === 'gain' && v === 0);
    patches.push({ id, engine: ENGINE, kind: 'corner', name: id.split('/')[1], expect: silent ? 'silent' : 'loud',
      source: `${TREE} params: ${why}; every other key at the engine's default` +
        (silent ? '. gain is 0 here (its declared minimum), so the engine must render silence' : ''), sets });
  };
  corner('corner/continuous-max', 'max', false, `all ${cont.length} continuous parameters (kind c) at range.max`);
  corner('corner/continuous-max-toggles-on', 'max', true,
    `all ${cont.length} continuous parameters at range.max and every toggle (kind t: ${toggles.map(t => t[0]).join(', ')}) on`);
  corner('corner/continuous-min', 'min', false, `all ${cont.length} continuous parameters (kind c) at range.min`);

  const doc = {
    schema: 'horde2.harness.patches/1',
    note: 'GENERATED by h2/harness/gen_patches.mjs. Regenerate it; never edit it by hand. tools/engine_enrolment_check.py and h2/harness/selftest.mjs read it.',
    // File names only, no content hashes: --check regenerates and compares the whole
    // text, so a source change that alters a patch is caught, and one that does not
    // (a comment in the ledger tool) costs nobody a re-pin.
    sources: { bank: BANK, ranges: TREE, ledger: LEDGER },
    how_a_patch_is_played: 'set every entry of `sets` in order (a string value is a setString), then snap, then the cell\'s `sets`; then the cell\'s notes go on together at the cell\'s velocity. h2/harness/harness.mjs jobFor() does exactly this.',
    cell: ledgerCell(ledgerSrc),
  };
  // One patch per line: a changed patch is one changed line in a diff, and the file
  // stays a few dozen lines instead of three per parameter.
  const head = JSON.stringify(doc, (k, v) => (k === 'cell' ? JSON.stringify(v) : v), 1).replace(/^ "cell": "(.*)"$/m, (_, s) => ` "cell": ${JSON.parse(`"${s}"`)}`);
  return head.replace(/\n}$/, ',\n "patches": [\n' + patches.map(p => '  ' + JSON.stringify(p)).join(',\n') + '\n ]\n}\n');
}

function main(argv) {
  const text = build();
  if (argv.includes('--check')) {
    let have = null;
    try { have = readFileSync(join(ROOT, OUT), 'utf8'); } catch { /* absent reads as stale */ }
    if (have !== text) {
      console.error(`gen_patches: ${OUT} is STALE (a source changed, or it was edited by hand): run node h2/harness/gen_patches.mjs`);
      return 1;
    }
    console.log(`gen_patches: ${OUT} is current (${JSON.parse(text).patches.length} patches)`);
    return 0;
  }
  writeFileSync(join(ROOT, OUT), text);
  console.log(`gen_patches: wrote ${OUT} (${JSON.parse(text).patches.length} patches)`);
  return 0;
}

// Run only as the entry script. Two file URLs made the same way are compared: a hand-built
// `file://${argv[1]}` differs from import.meta.url whenever the path holds a space or is
// reached through a symlink, and then this file ran nothing and exited 0.
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) process.exitCode = main(process.argv.slice(2));
