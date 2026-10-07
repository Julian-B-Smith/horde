/*
 * sandbox_check.mjs — lab JS cannot write, spawn, read outside its grant, or open
 * a worker when the harnesses run it; and an untracked HTML is never executed.
 *
 * WIRED: ./verify fast.
 *
 * WHY (B446 W3b, ADR-194, L3-M6 / L4-M6). The lab-load harness runs lab and packet
 * JavaScript in a `node:vm` context built from host-realm objects, and a script can
 * reach the host `process` from inside such a context (the documented vm
 * limitation). tools/labharness/sandboxed_node.mjs therefore runs these harnesses
 * under Node's permission model. A guard nobody exercises rots, so this plants labs
 * that get out of the context that way and then try to write a file, spawn a process, read outside the grant, call
 * process.binding, and start a worker. Each must come back as a refusal.
 *
 * MUST-FAIL CONTROLS. A refusal only counts next to proof the plant works:
 *   - every payload is first run through a bare `vm` context under plain `node`
 *     (no permission model) and MUST produce its marker, so a missing marker under
 *     the sandbox means "refused", not "the payload cannot fire";
 *   - a benign lab loads GREEN through the same path, so a red is not launcher
 *     breakage;
 *   - the tracked-only rule runs in a scratch git repo: an untracked HTML that
 *     throws is NOT executed; the same file once `git add`ed IS (and is reported).
 *
 * Needs `process.getBuiltinModule` (Node 20.16+ / 22.3+) for the plants. If this
 * node lacks it the escape rows are SKIPPED, loudly; the launcher itself still
 * fails closed on a node with no permission model.
 */
import { spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, writeFileSync, copyFileSync, existsSync, rmSync, realpathSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const fails = [];
const check = (ok, what) => { if (!ok) { fails.push(what); console.log(`FAIL  ${what}`); } };

const base = realpathSync(mkdtempSync(join(tmpdir(), 'sandbox_check-')));
const run = (cmd, args, opts = {}) => spawnSync(cmd, args, { encoding: 'utf8', timeout: 60000, ...opts });
const lab = (js) => `<!doctype html><title>plant</title><script>\n${js}\n</script>\n`;
const REFUSED = /restricted|ERR_ACCESS_DENIED|process\.binding|addons is disabled/;

try {
  // ------------------------------------------------------------ escape rows --
  if (typeof process.getBuiltinModule !== 'function') {
    console.log(`SKIP  escape rows: node ${process.version} has no process.getBuiltinModule`);
  } else {
    const secret = join(base, 'secret.txt'); writeFileSync(secret, 'SECRET');
    const climb = 'const P = Math.constructor.constructor("return process")();\n';
    // The repo's own lab-load context, as far as these payloads care.
    const rows = [
      { id: 'write a file', mark: 'w',
        js: `${climb}P.getBuiltinModule('fs').writeFileSync(${JSON.stringify(join(base, 'm_w'))}, 'x');` },
      { id: 'spawn a process', mark: 's',
        js: `${climb}P.getBuiltinModule('child_process').execSync('touch ' + ${JSON.stringify(join(base, 'm_s'))});` },
      { id: 'read outside the grant', mark: 'r',
        js: `${climb}const fs = P.getBuiltinModule('fs');\n` +
            `fs.writeFileSync(${JSON.stringify(join(base, 'm_r'))}, fs.readFileSync(${JSON.stringify(secret)}));` },
      { id: 'process.binding', mark: 'b',
        js: `${climb}P.binding('fs');\nP.getBuiltinModule('fs').writeFileSync(${JSON.stringify(join(base, 'm_b'))}, 'x');` },
      { id: 'start a worker', mark: 'k',
        js: `${climb}const W = P.getBuiltinModule('worker_threads');\n` +
            `new W.Worker("require('fs').writeFileSync(" + ${JSON.stringify(JSON.stringify(join(base, 'm_k')))} + ", 'x')", { eval: true });` },
    ];
    // Benign lab: a green through the same path.
    const benign = join(base, 'benign.html'); writeFileSync(benign, lab('var x = 1;'));
    const b = run('node', [join(ROOT, 'tools/labharness/lab_load_check.mjs'), benign]);
    check(b.status === 0 && /^OK\s+benign\.html/m.test(b.stdout), `CONTROL: a benign lab did not load green (exit ${b.status}) ${b.stdout.slice(-200)}`);

    for (const row of rows) {
      const html = join(base, `plant_${row.mark}.html`); writeFileSync(html, lab(row.js));
      const mark = join(base, `m_${row.mark}`);

      // Control: the same payload in a bare vm context under plain node must fire.
      const bare = run('node', ['-e',
        `const vm=require('node:vm'),fs=require('node:fs');` +
        `const src=fs.readFileSync(${JSON.stringify(html)},'utf8').match(/<script>([\\s\\S]*?)<\\/script>/)[1];` +
        `vm.runInNewContext(src,{Math});`]);
      void bare;   // exit status is irrelevant; only the marker matters
      check(existsSync(mark), `CONTROL: "${row.id}" payload produced no marker outside the sandbox; the plant cannot fire`);
      rmSync(mark, { force: true });

      // The row: through the real path (lab_load_check -> sandboxed_node.mjs).
      const r = run('node', [join(ROOT, 'tools/labharness/lab_load_check.mjs'), html]);
      check(r.status === 1, `"${row.id}": expected the lab to be reported broken (exit 1), got exit ${r.status}`);
      check(REFUSED.test(r.stdout + r.stderr), `"${row.id}": no permission refusal in the output: ${(r.stdout + r.stderr).slice(0, 240)}`);
      check(!existsSync(mark), `"${row.id}": the payload's marker exists; it ran`);
    }
  }

  // ------------------------------------------- tracked-only default sweep --
  const fx = join(base, 'repo');
  mkdirSync(join(fx, 'tools/labharness'), { recursive: true });
  mkdirSync(join(fx, 'docs/design'), { recursive: true });
  for (const f of ['sandboxed_node.mjs', 'sandbox_guard.mjs', 'lab_load_check.mjs']) copyFileSync(join(ROOT, 'tools/labharness', f), join(fx, 'tools/labharness', f));
  writeFileSync(join(fx, 'docs/design/ok.html'), lab('var ok = 1;'));
  writeFileSync(join(fx, 'docs/design/planted.html'), lab('throw new Error("PLANTED_FILE_EXECUTED");'));
  const git = (...a) => run('git', a, { cwd: fx, env: { ...process.env, GIT_CONFIG_GLOBAL: '/dev/null', GIT_CONFIG_SYSTEM: '/dev/null' } });
  git('init', '-q'); git('add', 'tools', 'docs/design/ok.html');
  const sweep = () => run('node', [join(fx, 'tools/labharness/lab_load_check.mjs')], { cwd: fx });

  const u = sweep();
  check(u.status === 0 && /GREEN — 1 labs loaded/.test(u.stdout), `untracked plant: expected GREEN over the 1 tracked lab, got exit ${u.status}: ${u.stdout.slice(-200)}`);
  check(!/PLANTED_FILE_EXECUTED|planted\.html/.test(u.stdout + u.stderr), 'an untracked HTML in docs/design was executed or listed');
  git('add', 'docs/design/planted.html');
  const t = sweep();
  check(t.status === 1 && /PLANTED_FILE_EXECUTED/.test(t.stdout), `CONTROL: the same file once tracked should be executed and reported (exit ${t.status}); the sweep cannot see a plant`);
} finally {
  rmSync(base, { recursive: true, force: true });
}

if (fails.length) { console.log(`\nRED — sandbox_check: ${fails.length} failure(s)`); process.exit(1); }
console.log('sandbox_check: GREEN — lab JS is refused write/spawn/read-outside/binding/worker under the permission model, every plant fired outside it, and an untracked lab is not executed');
