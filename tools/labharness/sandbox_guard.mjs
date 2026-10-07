/*
 * sandbox_guard.mjs — `import './sandbox_guard.mjs'` as the FIRST import of a lab
 * harness makes `node tools/labharness/<script>.mjs` run under Node's permission
 * model, whoever invokes it (B446 W3b, ADR-194; why and what is granted:
 * sandboxed_node.mjs).
 *
 * Unsandboxed, it hands the whole run to the launcher with the same arguments and
 * exits with the launcher's status, so no other statement of the importing file
 * runs outside the sandbox. Already sandboxed (the launcher's child, or a worker
 * the child started — workers inherit the model), it does nothing.
 *
 * Why a guard and not a changed call in ./verify: the commands stay what they were
 * (`node tools/labharness/x.mjs`), which the playbook and the wiring check read
 * literally, and a hand-run harness is sandboxed too.
 *
 * `process.permission` exists only when the model is on, and a lab script cannot
 * make it appear. SANDBOXED_NODE_CHILD marks the launcher's child: seeing the mark
 * WITHOUT the model means the flags were not applied, so stop rather than loop or
 * run unsandboxed.
 */
import { spawnSync } from 'node:child_process';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

if (!process.permission) {
  if (process.env.SANDBOXED_NODE_CHILD) {
    console.error('sandbox_guard: launched by sandboxed_node.mjs but the permission model is off; refusing');
    process.exit(2);
  }
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
  const r = spawnSync(process.execPath,
    [resolve(root, 'tools/labharness/sandboxed_node.mjs'), relative(root, resolve(process.argv[1])),
     ...process.argv.slice(2)], { stdio: 'inherit' });
  process.exit(r.status === null ? 1 : r.status);
}
