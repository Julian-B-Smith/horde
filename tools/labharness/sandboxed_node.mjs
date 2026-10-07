/*
 * sandboxed_node.mjs — run a lab/packet JS harness under Node's permission model.
 *
 * WHY. These harnesses execute JavaScript that is not ours to trust: lab HTML
 * from agent PRs and from external packets (L3-M6, L4-M6, ADR-194 / B446 W3b).
 * `node:vm` is documented as not a security mechanism, and the lab-load context
 * is built from host-realm objects, so a script can reach the host `process`
 * (the documented vm limitation). The permission model is the layer that holds
 * when that happens: it is process-wide, so it still applies to code that got
 * out of the vm context.
 *
 * WHAT IT GRANTS. Reads of a short, named set of trees (BASE_READ plus the
 * per-script extras below); NO writes, NO child processes, NO addons, and
 * workers only for the scripts that spawn one. The child also gets a scrubbed
 * environment, because the permission model does not cover env vars.
 *
 * WHAT IT DOES NOT COVER (known, kept visible):
 *   - Network: Node 24 has no network permission. A script that reaches `http`
 *     can send out whatever it can read, which is why the read set is small.
 *   - Symlinks: Node follows a symlink inside a granted tree even when it points
 *     outside it. Untracked symlinks planted in a lab tree are not detected here.
 *   - Scripts not listed in PROFILES are refused, so nothing runs sandboxed by
 *     accident with a widened grant. To add one: add a PROFILES row and say why.
 *
 * HOW SCRIPTS GET HERE. The labharness scripts start with `import
 * './sandbox_guard.mjs'`, which re-runs them through this launcher when invoked
 * bare, so `node tools/labharness/x.mjs` is sandboxed however it is called.
 * Packet code we must not edit (reference/scalpel/verify/verify.js) is called
 * through this launcher directly in ./verify.
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
import { dirname, resolve, relative, sep } from 'node:path';
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

// One row per script allowed to run here. `read` adds to BASE_READ; `worker`
// allows `new Worker` (the two checks that judge planted labs in a worker so
// a runaway lab can be terminated). Each extra is a per-file exception.
const PROFILES = {
  'tools/labharness/lab_load_check.mjs': { tracked: true },
  'tools/labharness/fxlab_check.mjs': {},
  // Reads the presentation table beside the shipped GUI.
  'tools/labharness/gui_history_check.mjs': { read: ['src/param_presentation.tsv'] },
  // Both judge planted labs in a worker thread (a runaway lab is terminated).
  'tools/labharness/morph_editor_check.mjs': { worker: true },
  'tools/labharness/lab_wheel_scroll_check.mjs': { worker: true },
  // Default (check) mode only reads; its --sweep/--merge modes write and are
  // human-run unsandboxed, outside ./verify.
  'tools/labharness/fxmorph_check.mjs': { read: ['docs/design'] },
  // Externally authored packet code (ADR-184). Reads its own tree via require().
  'reference/scalpel/verify/verify.js': { read: ['reference/scalpel'] },
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

let args = scriptArgs;
if (!args.length && profile.tracked) {
  const r = spawnSync('git', ['ls-files', '-z', '--', 'docs/design', 'reference', 'src/gui'],
    { cwd: ROOT, encoding: 'utf8' });
  if (r.status !== 0) die('git ls-files failed; cannot build the tracked lab list');
  args = r.stdout.split('\0').filter(f => f.endsWith('.html')
    && LAB_HTML_DIRS.has(dirname(f)) && existsSync(resolve(ROOT, f))).sort();
  if (!args.length) die('git ls-files returned no lab HTML; refusing to report a vacuous green');
}

const reads = new Set([...BASE_READ, ...(profile.read || []), script]);
// Explicit file args are the files the script was asked to load; read-only.
for (const a of args) if (existsSync(resolve(ROOT, a))) reads.add(a);

const flags = [permFlag, ...[...reads].map(p => `--allow-fs-read=${resolve(ROOT, p)}`)];
if (profile.worker) flags.push('--allow-worker');

// Scrubbed env: the permission model does not cover process.env, and the
// shell holds tokens. LWS_* keeps the wheel-scroll check's verbose switch.
const env = { SANDBOXED_NODE_CHILD: '1' };   // lets a script detect "launched, but flags not applied"
for (const k of ['PATH', 'LANG', 'LC_ALL', 'TZ']) if (process.env[k] !== undefined) env[k] = process.env[k];
for (const k of Object.keys(process.env)) if (k.startsWith('LWS_')) env[k] = process.env[k];

const r = spawnSync(process.execPath, [...flags, resolve(ROOT, script), ...args],
  { cwd: ROOT, env, stdio: 'inherit' });
if (r.error) die(String(r.error));
process.exit(r.status === null ? 1 : r.status);
