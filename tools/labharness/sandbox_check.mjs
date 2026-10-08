/*
 * sandbox_check.mjs — lab JS cannot write, spawn, read outside its grant, or open
 * a worker when ANY harness that evaluates lab or prototype text runs it; and an
 * untracked HTML is never executed.
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
 * W3c (B446) EXTENDS THIS to every other harness that evaluates lab or prototype text
 * (`new Function`, `vm`, `require` of a lab file). Three layers, none of which edits
 * a protected lab file (the plants live in a scratch copy of the tree):
 *   1. COVERAGE, static: every file under tools/ that evaluates lab text must have
 *      sandbox_guard.mjs as its FIRST import, and every node entry that ./verify or a
 *      C++ check runs and that reaches such a file must have a PROFILES row. The scan is
 *      itself exercised on a planted unguarded evaluator (must be flagged) and on the
 *      same text with the guard (must pass).
 *   2. PLANTS, one family at a time, each with a payload that WRITES a file and one that
 *      SPAWNS a process (and, for the generators and the report tool, one that writes
 *      beside its own granted target): the golden generators (tools/golden), the
 *      fidelity/oracle checks (tools/labharness), the patch-space checks, the h2 parity
 *      renderers, the legacy-preset porter, and a manual report tool. Each runs by its
 *      real command (`node tools/x.mjs`, so the guard and launcher are on the path).
 *      Under the sandbox the run must fail with a permission refusal and leave no
 *      marker; the CONTROL for each is the same command under a node with every grant
 *      open (`--permission --allow-fs-read=* ...`), where the marker MUST appear, so
 *      "no marker" means "refused" and not "the plant cannot fire". Each payload ends in
 *      process.exit(0), so a control run stops at the plant instead of running the check.
 *   3. THE OTHER SIDE: a generator still writes its own directory and nothing under
 *      --selfcheck, and the report tool still writes its one file. Grants that are too
 *      tight would turn ./verify full red; this keeps them honest.
 *
 * Needs `process.getBuiltinModule` (Node 20.16+ / 22.3+) for the plants. If this
 * node lacks it the escape rows are SKIPPED, loudly; the launcher itself still
 * fails closed on a node with no permission model.
 */
import { spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, readdirSync, copyFileSync, cpSync, existsSync, rmSync, realpathSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve, relative } from 'node:path';
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

  // ============================================ W3c: the other harness families ==
  // ---- 1. coverage, static -------------------------------------------------------
  const SELF = new Set(['sandboxed_node.mjs', 'sandbox_guard.mjs', 'sandbox_facts.mjs', 'sandbox_check.mjs']);
  const jsFiles = dir => readdirSync(dir, { withFileTypes: true }).flatMap(e =>
    e.isDirectory() ? jsFiles(join(dir, e.name)) : /\.(mjs|cjs|js)$/.test(e.name) ? [join(dir, e.name)] : []);
  // Block comments and whole-line `//` comments are not code. Other `//` are kept on purpose:
  // stripping less can only flag more.
  const stripComments = src => src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
  // What evaluates lab text: new Function, node:vm, require() of a file (createRequire), eval,
  // and import() of a non-literal specifier. A literal import('./x.mjs') loads our own module.
  const EVALUATES = /\bnew Function\b|\bnode:vm\b|\brunIn(?:New|This)?Context\b|\bcreateRequire\b|\beval\(|\bimport\((?!\s*['"`])/;
  const firstImport = src => src.split('\n').find(l => /^import\s/.test(l)) || '';
  const unguarded = src => EVALUATES.test(stripComments(src)) && !/sandbox_guard\.mjs/.test(firstImport(src));

  check(unguarded("import { x } from 'y';\nconst f = new Function('return 1');\n"), 'CONTROL: the coverage scan did not flag an unguarded new Function');
  check(unguarded("import { createRequire } from 'node:module';\n"), 'CONTROL: the coverage scan did not flag an unguarded createRequire');
  check(unguarded("import vm from 'node:vm';\n"), 'CONTROL: the coverage scan did not flag an unguarded node:vm');
  check(!unguarded("import '../labharness/sandbox_guard.mjs';\nimport vm from 'node:vm';\nnew Function('1');\n"), 'the coverage scan flagged a guarded evaluator');
  check(unguarded("import { x } from 'y';\nimport '../labharness/sandbox_guard.mjs';\nnew Function('1');\n"), 'CONTROL: a guard that is not the FIRST import was accepted');
  check(!unguarded("import { x } from 'y';\n// new Function is described here\n/* and node:vm here */\nconst a = import('./own.mjs');\n"), 'the coverage scan flagged a comment or a literal import()');

  const toolsRoot = join(ROOT, 'tools');
  const files = jsFiles(toolsRoot).filter(f => !SELF.has(f.split('/').pop()));
  const evaluators = new Set(files.filter(f => EVALUATES.test(stripComments(readFileSync(f, 'utf8')))));
  check(evaluators.size > 25, `only ${evaluators.size} evaluators found under tools/; the scan is broken`);
  for (const f of evaluators) {
    check(!unguarded(readFileSync(f, 'utf8')),
      `${relative(ROOT, f)} evaluates lab text but its FIRST import is not sandbox_guard.mjs (B446 W3c)`);
  }

  // Every node entry that ./verify or a C++ check runs, and that reaches an evaluator, needs a row.
  const launcherSrc = readFileSync(join(ROOT, 'tools/labharness/sandboxed_node.mjs'), 'utf8');
  const hasProfile = rel => launcherSrc.split('\n').some(l => l.trim().startsWith(`'${rel}':`));
  check(hasProfile('tools/golden/gen_goldens.mjs') && !hasProfile('tools/zzz_unlisted.mjs'), 'CONTROL: the PROFILES lookup is broken');
  const importsOf = f => [...stripComments(readFileSync(f, 'utf8')).matchAll(/(?:from\s+|import\s+)['"](\.{1,2}\/[^'"]+)['"]/g)]
    .map(m => resolve(dirname(f), m[1])).filter(existsSync);
  const reaches = entry => {
    const seen = new Set(), stack = [entry];
    while (stack.length) { const f = stack.pop(); if (seen.has(f)) continue; seen.add(f); if (/\.(mjs|js)$/.test(f)) stack.push(...importsOf(f)); }
    return [...seen].some(f => evaluators.has(f));
  };
  const callers = [join(ROOT, 'verify'), ...readdirSync(toolsRoot).filter(f => f.endsWith('.cpp')).map(f => join(toolsRoot, f))];
  const entries = new Set();
  for (const c of callers) for (const m of readFileSync(c, 'utf8').matchAll(/\bnode (tools\/[A-Za-z0-9_./-]+\.(?:mjs|js))\b/g)) entries.add(m[1]);
  entries.delete('tools/labharness/sandboxed_node.mjs');
  check(entries.size > 25, `only ${entries.size} node entries found in ./verify and tools/*.cpp; the scan is broken`);
  for (const e of [...entries].sort()) {
    if (!existsSync(join(ROOT, e))) { check(false, `${e} is run by ./verify or a C++ check but does not exist`); continue; }
    if (reaches(join(ROOT, e))) check(hasProfile(e), `${e} reaches an evaluator but has no PROFILES row in sandboxed_node.mjs; it would be refused at run time`);
  }
  // An unlisted entry is refused, not run unsandboxed.
  const unl = run('node', [join(ROOT, 'tools/labharness/sandboxed_node.mjs'), 'tools/zzz_unlisted.mjs']);
  check(unl.status === 2 && /no profile/.test(unl.stderr), `an unlisted script was not refused (exit ${unl.status}): ${unl.stderr.slice(0, 160)}`);

  // ---- 2 + 3. plants by family, on a scratch copy of the tree ---------------------
  if (typeof process.getBuiltinModule === 'function') {
    const fx = join(base, 'fx');
    const isDir = p => { try { return readdirSync(p), true; } catch (_) { return false; } };
    const keepSources = src => isDir(src) || /\.(mjs|cjs|js|json|py)$/.test(src);
    cpSync(join(ROOT, 'tools'), join(fx, 'tools'), { recursive: true, filter: keepSources });
    cpSync(join(ROOT, 'reference'), join(fx, 'reference'), { recursive: true });
    mkdirSync(join(fx, 'docs/design'), { recursive: true });
    for (const f of ['scalpel-horde-engine.js', 'scalpel-interface-lab.html', 'bend-lab.html']) copyFileSync(join(ROOT, 'docs/design', f), join(fx, 'docs/design', f));
    mkdirSync(join(fx, 'docs/scalpel'), { recursive: true });
    copyFileSync(join(ROOT, 'docs/scalpel/ACCOUNTING.md'), join(fx, 'docs/scalpel/ACCOUNTING.md'));
    mkdirSync(join(fx, 'tests'), { recursive: true });
    copyFileSync(join(ROOT, 'tests/morph_order.txt'), join(fx, 'tests/morph_order.txt'));
    mkdirSync(join(fx, 'docs/reports'), { recursive: true });
    mkdirSync(join(fx, 'build-golden/force'), { recursive: true });   // a sibling generator's directory
    // The porter's registry comes from src/hypersaw_clap.cpp, which is not copied: a stub keeps the
    // run on its way to the plant.
    writeFileSync(join(fx, 'tools/registry_decl.py'), 'print("1\\tosc1.x\\t0")\n');

    const J = JSON.stringify;
    // `__w3cP` is the host process, reached the way any code in the host realm reaches it
    // (`Math` is a host object even inside a vm context). Each payload exits at the plant.
    const climb = 'const __w3cP = Math.constructor.constructor("return process")();\n';
    const PAYLOAD = {
      write: m => `${climb}__w3cP.getBuiltinModule('fs').writeFileSync(${J(m)}, 'x'); __w3cP.exit(0);`,
      spawn: m => `${climb}__w3cP.getBuiltinModule('child_process').execSync('touch ' + ${J(m)}); __w3cP.exit(0);`,
    };
    // Where the plant goes in each kind of file. Each returns the planted text; an unchanged text is a broken plant.
    const PLANT = {
      beforeAudioGraph: (src, js) => src.replace(/\/\* =+ Audio graph/, m => `${js}\n${m}`),   // inside the sliced DSP block
      prefix: (src, js) => `${js}\n${src}`,                                                     // top of a CommonJS file
      firstScript: (src, js) => src.replace('<script>', `<script>\n${js}\n`),                   // the lab's first <script>
    };
    // The permission flag is spelled `--experimental-permission` before Node 23.5 / 22.13 (the launcher asks the node it runs on, so do we).
    const PERM = process.allowedNodeEnvironmentFlags.has('--permission') ? '--permission' : '--experimental-permission';
    const OPEN = [PERM, '--allow-fs-read=*', '--allow-fs-write=*', '--allow-child-process', '--allow-worker'];
    const FAMILIES = [
      { fam: 'golden generator', file: 'reference/subosc.html', plant: 'beforeAudioGraph',
        cmd: ['tools/golden/gen_subosc_goldens.mjs'], beside: join(fx, 'build-golden/force/m_beside') },
      { fam: 'oracle/fidelity check', file: 'reference/subosc.html', plant: 'beforeAudioGraph',
        cmd: ['tools/labharness/subosc_check.mjs'] },
      { fam: 'patch-space check', file: 'reference/scalpel/prototype/razor-core.js', plant: 'prefix',
        cmd: ['tools/patchspace/metrics_check.mjs'] },
      { fam: 'h2 parity renderer', file: 'reference/scalpel/prototype/razor-core.js', plant: 'prefix',
        cmd: ['tools/h2_scalpel_render.mjs', '--bench'] },
      { fam: 'legacy preset porter', file: 'reference/scalpel/prototype/razor-core.js', plant: 'prefix',
        cmd: ['tools/labharness/port_legacy_presets_check.mjs'] },
      { fam: 'manual report tool', file: 'docs/design/bend-lab.html', plant: 'firstScript',
        cmd: ['tools/labharness/glide_roundup.mjs'], beside: join(fx, 'docs/reports/m_beside.html') },
    ];
    for (const F of FAMILIES) {
      const target = join(fx, F.file), orig = readFileSync(target, 'utf8');
      const kinds = [['write', join(base, `m_${F.fam.replace(/\W/g, '')}_write`), 'write'],
                     ['spawn', join(base, `m_${F.fam.replace(/\W/g, '')}_spawn`), 'spawn']];
      if (F.beside) kinds.push(['write beside its own target', F.beside, 'write']);
      try {
        for (const [id, mark, kind] of kinds) {
          const planted = PLANT[F.plant](orig, PAYLOAD[kind](mark));
          check(planted !== orig, `${F.fam}/${id}: the plant did not change ${F.file}`);
          writeFileSync(target, planted);
          rmSync(mark, { force: true });
          const script = join(fx, F.cmd[0]), rest = F.cmd.slice(1);

          // Control: every grant open. The payload must fire (and stop the run at the plant).
          run('node', [...OPEN, script, ...rest], { cwd: fx });
          check(existsSync(mark), `CONTROL ${F.fam}/${id}: no marker with every grant open; the plant cannot fire (so a refusal below proves nothing)`);
          rmSync(mark, { force: true });

          // The row: the real command, guard and launcher on the path.
          const r = run('node', [script, ...rest], { cwd: fx });
          check(r.status !== 0 && r.status !== null, `${F.fam}/${id}: the planted harness exited ${r.status}; expected a failure`);
          check(REFUSED.test(r.stdout + r.stderr), `${F.fam}/${id}: no permission refusal in the output: ${(r.stdout + r.stderr).slice(0, 240)}`);
          check(!existsSync(mark), `${F.fam}/${id}: the payload's marker exists; it ran`);
        }
      } finally { writeFileSync(target, orig); }
    }

    // ---- the other side: what each harness is allowed still works ----------------
    const sub = (...a) => run('node', [join(fx, 'tools/golden/gen_subosc_goldens.mjs'), ...a], { cwd: fx });
    let r = sub('--selfcheck');
    check(r.status === 0, `gen_subosc_goldens --selfcheck exited ${r.status}: ${(r.stdout + r.stderr).slice(-200)}`);
    check(!existsSync(join(fx, 'build-golden/subosc')), '--selfcheck created build-golden/subosc (it writes nothing)');
    r = sub();
    check(r.status === 0, `gen_subosc_goldens exited ${r.status}: ${(r.stdout + r.stderr).slice(-200)}`);
    check(existsSync(join(fx, 'build-golden/subosc/subosc-manifest.tsv')), 'gen_subosc_goldens did not write its manifest into its own directory');
    const deep = join(base, 'fresh'); mkdirSync(deep);   // build-golden absent altogether, as on a fresh clone
    cpSync(join(fx, 'tools'), join(deep, 'tools'), { recursive: true });
    cpSync(join(fx, 'reference'), join(deep, 'reference'), { recursive: true });
    r = run('node', [join(deep, 'tools/golden/gen_subosc_goldens.mjs')], { cwd: deep });
    check(r.status === 0 && existsSync(join(deep, 'build-golden/subosc/subosc-manifest.tsv')),
      `gen_subosc_goldens on a tree with no build-golden/ exited ${r.status}: ${(r.stdout + r.stderr).slice(-200)}`);
    r = run('node', [join(fx, 'tools/labharness/glide_roundup.mjs')], { cwd: fx });
    check(r.status === 0 && existsSync(join(fx, 'docs/reports/2026-08-06-glide-law-roundup.html')),
      `glide_roundup exited ${r.status}, or did not write its report: ${(r.stdout + r.stderr).slice(-200)}`);
    r = run('node', [join(fx, 'tools/labharness/subosc_check.mjs')], { cwd: fx });
    check(r.status === 0 && /GREEN/.test(r.stdout), `subosc_check did not run green sandboxed (exit ${r.status}): ${(r.stdout + r.stderr).slice(-200)}`);
  }
} finally {
  rmSync(base, { recursive: true, force: true });
}

if (fails.length) { console.log(`\nRED — sandbox_check: ${fails.length} failure(s)`); process.exit(1); }
console.log('sandbox_check: GREEN — lab JS is refused write/spawn/read-outside/binding/worker under the permission model, every plant fired outside it, an untracked lab is not executed, and every evaluator under tools/ is guarded (6 harness families planted)');
