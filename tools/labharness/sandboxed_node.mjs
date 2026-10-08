/*
 * sandboxed_node.mjs — run a lab/packet JS harness under Node's permission model.
 *
 * WHY. These harnesses execute JavaScript that is not ours to trust: lab HTML
 * from agent PRs and from external packets (L3-M6, L4-M6, ADR-194 / B446 W3b, W3c).
 * `node:vm` is documented as not a security mechanism, and the lab-load context
 * is built from host-realm objects, so a script can reach the host `process`
 * (the documented vm limitation); `new Function` and `require` of a lab file are
 * the host realm outright. The permission model is the layer that holds when that
 * happens: it is process-wide, so it still applies to code that got out of a vm
 * context, and to worker threads.
 *
 * WHAT IT GRANTS. Reads of a short, named set of trees (BASE_READ plus the
 * per-script extras below); NO child processes, NO addons, workers only for the
 * scripts that spawn one, and writes only where a script's row names them (a
 * generator's output directory, a report's one file), only in the modes that write
 * (`{ path, unless: '--selfcheck' }`). The child also gets a scrubbed environment,
 * because the permission model does not cover env vars.
 *
 * WHAT A CHILD PROCESS WAS FOR. Two harnesses ran `python3 tools/registry_decl.py`
 * and `git check-ignore`. A grant of child processes is a grant of everything, so
 * this launcher runs those two commands HERE, outside, and hands over the answers
 * (`facts` in a row; tools/labharness/sandbox_facts.mjs reads them).
 *
 * WHAT IT DOES NOT COVER (known, kept visible):
 *   - Network: Node 24 has no network permission. A script that reaches `http`
 *     can send out whatever it can read, which is why the read set is small.
 *   - Symlinks: Node follows a symlink inside a granted tree even when it points
 *     outside it. Untracked symlinks planted in a lab tree are not detected here.
 *   - Scripts not listed in PROFILES are refused, so nothing runs sandboxed by
 *     accident with a widened grant. To add one: add a PROFILES row and say why.
 *
 * HOW SCRIPTS GET HERE. Every file that evaluates lab text (`new Function`, `vm`,
 * `require` of a lab or prototype file) starts with `import './sandbox_guard.mjs'`
 * (any relative path), which re-runs the ENTRY script through this launcher when
 * invoked bare, so `node tools/x.mjs` is sandboxed however it is called and an
 * entry with no row here is refused. sandbox_check.mjs scans tools/ for an
 * evaluator without that first import. Packet code we must not edit
 * (reference/scalpel/verify/verify.js) is called through this launcher directly
 * in ./verify.
 *
 * Usage:  node tools/labharness/sandboxed_node.mjs <script> [args...]
 *   <script> is repo-relative and must have a PROFILES row. Explicit file args
 *   are granted READ (they are what the script was asked to load); with no args
 *   a profile that sets `tracked` is given the tracked-file list computed HERE,
 *   outside the sandbox (the sandbox cannot run git, and an untracked file
 *   dropped into a lab tree must not be executed — L3-M6).
 *
 * FLAG NAMES. `--permission` is the stable spelling (Node >= 23.5, and 22.13+);
 * older lines spell it `--experimental-permission`. We ask this node which it
 * knows rather than guess from the version. A node with neither FAILS CLOSED
 * (exit 2): silently running unsandboxed is the failure this file exists to stop.
 */
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, isAbsolute, join, resolve, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');

// Read access every profile gets: the harness directory (the script and any
// sibling it imports) and the lab trees the harnesses legitimately parse.
const BASE_READ = ['tools/labharness', 'docs/design', 'reference', 'src/gui'];

// Lab-tree directories whose *.html files lab_load_check sweeps (non-recursive,
// as before the move to git ls-files).
const LAB_HTML_DIRS = new Set([
  'docs/design', 'reference', 'reference/maw', 'reference/scalpel/prototype', 'src/gui',
]);

// A row's fields (all optional). Paths are repo-relative, or `~/` for the home dir.
//   read      extra READ grants beyond BASE_READ (a per-file exception each)
//   write     WRITE grants: 'path', or { path, if: '--flag' } / { path, unless: '--flag' }
//             (a flag test is "this argument is present")
//   readArgs / writeArgs / writeDirArgs   { '--flag': default | null }  the flag's VALUE
//             is granted (for writeArgs a file, granted as its directory tree; for
//             writeDirArgs a directory tree); the default (a path) when the flag is
//             absent, nothing when null
//   A trailing '/' on a path marks a directory TREE: granted as the path AND `path/*`.
//   Node decides "directory" by looking at the disk when the flag is parsed, so a plain
//   grant does not cover the children of a directory that does not exist yet, and `path/*`
//   alone does not cover a mkdir of `path` itself; the pair covers both.
//   worker    allows `new Worker` (a check that judges a planted lab in a worker, or
//             fans out over workers; workers inherit the model)
//   tracked   no args -> the tracked lab HTML list (lab_load_check)
//   facts     answers to commands the child may not run: 'registry' | 'git-ignored'
//   gitIgnored extra repo-relative paths to ask `git check-ignore` about
const GOLDEN = ['tools/golden'];
const PATCHSPACE = ['tools/patchspace'];
// A golden generator renders into build-golden/<dir>; --selfcheck writes nothing.
const gen = (dir, extra = {}) => ({ read: GOLDEN, write: [{ path: `build-golden/${dir}/`, unless: '--selfcheck' }], ...extra });
// A fidelity/oracle check that slices a lab through tools/golden/extract_core.mjs: reads only.
const check = (extra = {}) => ({ read: GOLDEN, ...extra });

const PROFILES = {
  // ---- W3b: lab load, FX/GUI/morph harnesses, the packet oracle ----
  'tools/labharness/lab_load_check.mjs': { tracked: true },
  'tools/labharness/fxlab_check.mjs': {},
  // Reads the presentation table beside the shipped GUI.
  'tools/labharness/gui_history_check.mjs': { read: ['src/param_presentation.tsv'] },
  // Both judge planted labs in a worker thread (a runaway lab is terminated).
  'tools/labharness/morph_editor_check.mjs': { worker: true },
  'tools/labharness/lab_wheel_scroll_check.mjs': { worker: true },
  // Default (check) mode reads only. --sweep writes a chunk file into --out;
  // --merge DIR reads the chunks, and with --write replaces the committed gallery.
  'tools/labharness/fxmorph_check.mjs': {
    read: ['docs/design'],
    readArgs: { '--merge': null },
    writeDirArgs: { '--out': null },
    write: [{ path: 'docs/design/fx-chain-morph-gallery.json', if: '--write' }],
  },
  // Externally authored packet code (ADR-184). Reads its own tree via require().
  'reference/scalpel/verify/verify.js': { read: ['reference/scalpel'] },

  // ---- W3c: the golden generators (tools/golden/*) ----
  // Writes its files at the top of build-golden, so its grant is the directory.
  'tools/golden/gen_goldens.mjs': { read: GOLDEN, write: [{ path: 'build-golden/', unless: '--selfcheck' }] },
  'tools/golden/gen_goldens_sr.mjs': gen('sr48000'),
  'tools/golden/gen_filter_goldens.mjs': gen('filter'),
  'tools/golden/gen_force_goldens.mjs': gen('force'),
  'tools/golden/gen_glide_goldens.mjs': gen('glide'),
  'tools/golden/gen_intent_goldens.mjs': gen('intent'),
  'tools/golden/gen_notch_goldens.mjs': gen('notch'),
  'tools/golden/gen_spectra_goldens.mjs': gen('spectra'),
  'tools/golden/gen_station_goldens.mjs': gen('station'),
  'tools/golden/gen_subosc_goldens.mjs': gen('subosc'),
  'tools/golden/gen_swarmalator_goldens.mjs': gen('swarmalator'),
  'tools/golden/gen_time_goldens.mjs': gen('time'),

  // ---- W3c: fidelity / oracle checks that evaluate a lab (read only) ----
  'tools/labharness/station_check.mjs': check(),
  'tools/labharness/subosc_check.mjs': check(),
  'tools/labharness/reverb_check.mjs': check(),
  'tools/labharness/filter_fidelity_check.mjs': check(),
  // The composed engine: also the C++ header it cross-reads, and metrics.mjs.
  'tools/labharness/composed_engine_check.mjs': check({ read: [...GOLDEN, ...PATCHSPACE, 'h2/cores/swarm'] }),
  // The ledger (docs/port) names evidence files; the check asserts each exists, and
  // existsSync on an ungranted path THROWS under the model, so the evidence trees are read-granted.
  'tools/labharness/divergence_ledger_check.mjs': {
    read: ['docs/port', 'docs/patchspace', 'traces', 'tools/swarm_sr_parity.h', ...GOLDEN, ...PATCHSPACE],
  },
  // The porter's check: fixtures only, but loadContext() needs the registry and the
  // ignore answers that were child processes (facts, above).
  'tools/labharness/port_legacy_presets_check.mjs': {
    read: ['tools/port_legacy_presets.mjs', 'docs/scalpel', 'tests/morph_order.txt'],
    facts: ['registry', 'git-ignored'], gitIgnored: ['local/legacy-presets', 'tools'],
  },

  // B445 (ADR-197): the os-default migration check. Fixtures only, through the porter, so the
  // porter's grant (its source, ACCOUNTING, the registry and ignore facts) and nothing more.
  'tools/labharness/os_default_check.mjs': {
    read: ['tools/port_legacy_presets.mjs', 'docs/scalpel', 'tests/morph_order.txt'],
    facts: ['registry', 'git-ignored'], gitIgnored: ['local/legacy-presets', 'tools'],
  },

  // ---- W3c: manual tools (not in ./verify) ----
  // Each writes one report file; the file is the whole write grant.
  'tools/labharness/glide_roundup.mjs': { write: ['docs/reports/2026-08-06-glide-law-roundup.html'] },
  'tools/labharness/modlab_sweep_report.mjs': { write: ['docs/reports/2026-08-05-mod-matrix-sweep.html'] },
  'tools/labharness/modlab_probe.mjs': {},
  'tools/labharness/modlab_reach.mjs': {},
  'tools/labharness/modlab_sweep.mjs': {},
  'tools/feedback_scan.mjs': {},
  // Reads the human's private legacy store, writes the porter's output directory.
  // Both default to the same places the tool uses; an explicit --store/--out moves the grant.
  'tools/port_legacy_presets.mjs': {
    read: ['docs/scalpel', 'tests/morph_order.txt'],
    readArgs: { '--store': '~/Library/Application Support/LiftedTruck/HYPERSAW' },
    writeDirArgs: { '--out': 'local/legacy-presets' },
    facts: ['registry', 'git-ignored'],
  },

  // ---- W3c: h2 parity renderers (spawned by C++ checks as `node tools/h2_*_render.mjs`) ----
  // Stream to stdout, fan out over workers.
  'tools/h2_engine_render.mjs': { worker: true, read: ['tools/h2_scenarios.mjs'] },
  'tools/h2_scalpel_render.mjs': { worker: true, read: ['tools/h2_scenarios.mjs'] },

  // ---- W3c: patch-space checks (./verify full) and tools ----
  'tools/patchspace/metrics_check.mjs': { read: PATCHSPACE },
  'tools/patchspace/fidelity_scan_check.mjs': { read: PATCHSPACE },
  'tools/patchspace/listening_pass_check.mjs': { read: PATCHSPACE },
  'tools/patchspace/dependency_tree_check.mjs': { read: PATCHSPACE, worker: true },
  // Manual: each writes under local/patchspace (git-ignored) or one named file.
  'tools/patchspace/gen_dependency_tree.mjs': {
    read: PATCHSPACE, worker: true,
    write: [{ path: 'tools/patchspace/dependency_tree.json', unless: '--stdout' }],
  },
  'tools/patchspace/gauntlet.mjs': { read: [...PATCHSPACE, 'local/patchspace/'], worker: true, write: ['local/patchspace/'] },
  'tools/patchspace/gauntlet_report.mjs': {
    read: [...PATCHSPACE, 'local/patchspace/'], writeArgs: { '--out': null },
    write: [{ path: 'docs/patchspace/', unless: '--out' }],
  },
  'tools/patchspace/fidelity_audit.mjs': { read: [...PATCHSPACE, 'local/patchspace/'], worker: true, write: ['local/patchspace/fidelity/'] },
  'tools/patchspace/alias_sources.mjs': { read: [...PATCHSPACE, 'local/patchspace/'], worker: true, write: ['local/patchspace/alias_sources/'] },
  'tools/patchspace/antialias_scope.mjs': {
    read: PATCHSPACE, write: [{ path: 'docs/patchspace/2026-09-29-b355-d3-scope.md', if: '--write' }],
  },
  'tools/patchspace/listening_sample.mjs': {
    read: [...PATCHSPACE, 'local/patchspace/'],
    write: ['docs/design/listening-pass.json'],
  },
  'tools/patchspace/calibrate.mjs': { read: [...PATCHSPACE, 'local/patchspace/'] },
  'tools/patchspace/os_quality.mjs': {
    read: PATCHSPACE, writeDirArgs: { '--out': 'local/listening/b445' }, facts: ['git-ignored'],
  },
};

const permFlag = process.allowedNodeEnvironmentFlags.has('--permission') ? '--permission'
  : process.allowedNodeEnvironmentFlags.has('--experimental-permission') ? '--experimental-permission'
  : null;

function die(msg) { console.error(`sandboxed_node: ${msg}`); process.exit(2); }

if (!permFlag) die(`node ${process.version} has no permission model; refusing to run lab code unsandboxed`);

const [scriptArg, ...scriptArgs] = process.argv.slice(2);
if (!scriptArg) die('usage: sandboxed_node.mjs <script> [args...]');
const script = relative(ROOT, resolve(ROOT, scriptArg)).split(sep).join('/');
const profile = PROFILES[script];
if (!profile) die(`${script} has no profile in PROFILES; add one deliberately`);

const abs = p => (p.startsWith('~/') ? join(homedir(), p.slice(2)) : resolve(ROOT, p));
// A path ending in '/' is a directory tree: the path and `path/*` (see the schema note above).
const grant = (p, tree = p.endsWith('/')) => (tree ? [abs(p), abs(p) + '/*'] : [abs(p)]);
const hasFlag = f => scriptArgs.includes(f);
const flagValue = f => { const i = scriptArgs.indexOf(f); return i >= 0 && i + 1 < scriptArgs.length ? scriptArgs[i + 1] : undefined; };

let args = scriptArgs;
if (!args.length && profile.tracked) {
  const r = spawnSync('git', ['ls-files', '-z', '--', 'docs/design', 'reference', 'src/gui'],
    { cwd: ROOT, encoding: 'utf8' });
  if (r.status !== 0) die('git ls-files failed; cannot build the tracked lab list');
  args = r.stdout.split('\0').filter(f => f.endsWith('.html')
    && LAB_HTML_DIRS.has(dirname(f)) && existsSync(resolve(ROOT, f))).sort();
  if (!args.length) die('git ls-files returned no lab HTML; refusing to report a vacuous green');
}

const reads = new Set([...BASE_READ, ...(profile.read || []), script].flatMap(p => grant(p)));
// Explicit file args are the files the script was asked to load; read-only.
for (const a of args) if (existsSync(resolve(ROOT, a))) reads.add(resolve(ROOT, a));
for (const [flag, def] of Object.entries(profile.readArgs || {})) {
  const v = flagValue(flag) ?? def;
  if (v) for (const g of grant(v)) reads.add(g);
}

const writes = new Set();
for (const w of profile.write || []) {
  const { path, if: ifFlag, unless } = typeof w === 'string' ? { path: w } : w;
  if (ifFlag && !hasFlag(ifFlag)) continue;
  if (unless && hasFlag(unless)) continue;
  for (const g of grant(path)) writes.add(g);
}
const writeDirs = [];   // the --out style grants, kept to ask git about them below
for (const [flag, def] of Object.entries(profile.writeArgs || {})) {
  const v = flagValue(flag) ?? def;
  // The file's DIRECTORY, not the file: the tool mkdir -p's the parent, and a file grant listed
  // before a grant on its own directory makes Node refuse that mkdir (verified on node 24.10).
  if (v) for (const g of grant(dirname(abs(v)) + '/')) writes.add(g);
}
for (const [flag, def] of Object.entries(profile.writeDirArgs || {})) {
  const v = flagValue(flag) ?? def;
  // Read too: existsSync on the output directory is a read, and throws without the grant.
  if (v) { for (const g of grant(v, true)) { writes.add(g); reads.add(g); } writeDirs.push(abs(v)); }
}

const flags = [permFlag,
  ...[...reads].map(p => `--allow-fs-read=${p}`),
  ...[...writes].map(p => `--allow-fs-write=${p}`)];
if (profile.worker) flags.push('--allow-worker');

// Scrubbed env: the permission model does not cover process.env, and the
// shell holds tokens. LWS_* keeps the wheel-scroll check's verbose switch.
const env = { SANDBOXED_NODE_CHILD: '1' };   // lets a script detect "launched, but flags not applied"
for (const k of ['PATH', 'LANG', 'LC_ALL', 'TZ']) if (process.env[k] !== undefined) env[k] = process.env[k];
for (const k of Object.keys(process.env)) if (k.startsWith('LWS_')) env[k] = process.env[k];

// ---- facts: commands the child may not run, answered here (see header) ----
const facts = new Set(profile.facts || []);
if (facts.has('registry')) {
  const r = spawnSync('python3', [join(ROOT, 'tools/registry_decl.py')], { encoding: 'utf8', cwd: ROOT });
  if (r.status === 0) env.HORDE_FACT_REGISTRY = r.stdout;   // on failure leave it unset: the child then fails on its own
}
if (facts.has('git-ignored')) {
  // Keys are exactly what sandbox_facts.mjs asks: `[--no-index ]<repo-relative path>`.
  const probes = [];
  for (const rel of profile.gitIgnored || []) probes.push([rel, false], [`${rel}/probe.json`, true]);
  for (const d of writeDirs) {
    const rel = relative(ROOT, d);
    if (rel.startsWith('..') || isAbsolute(rel)) continue;   // outside the repo is not git's business
    probes.push([rel, false], [`${rel}/probe.json`, true]);
  }
  const answers = {};
  for (const [rel, noIndex] of probes) {
    const r = spawnSync('git', ['-C', ROOT, 'check-ignore', '-q', ...(noIndex ? ['--no-index'] : []), rel], { stdio: 'ignore' });
    answers[`${noIndex ? '--no-index ' : ''}${rel}`] = r.status === 0;
  }
  env.HORDE_FACT_GIT_IGNORED = JSON.stringify(answers);
}

const r = spawnSync(process.execPath, [...flags, resolve(ROOT, script), ...args],
  { cwd: ROOT, env, stdio: 'inherit' });
if (r.error) die(String(r.error));
process.exit(r.status === null ? 1 : r.status);
