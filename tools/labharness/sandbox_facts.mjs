/*
 * sandbox_facts.mjs — the two answers a sandboxed harness used to get from a child
 * process, now computed by tools/labharness/sandboxed_node.mjs OUTSIDE the sandbox
 * and handed down in the environment (B446 W3c, ADR-194).
 *
 * WHY. The permission model's child-process switch is all or nothing: granting it so
 * that a harness can run `git check-ignore` also lets a lab payload run anything.
 * The harness therefore asks here. Under the launcher the answer is in the env;
 * with no launcher (the env is unset: a test importing the module under a wide-open
 * node, say) the module runs the command itself, as the harness always did.
 *
 * An answer that is absent inside the sandbox is NOT "yes": a command that cannot run
 * throws, and the git helper reads a throw as "not ignored" -- the refusing side of a
 * privacy rule (a directory git would track is never written to).
 *
 * Keys are exactly the arguments the harness used to pass, so the launcher and the
 * consumers cannot disagree about what was asked: `[--no-index ]<repo-relative path>`.
 */
import { execFileSync } from 'node:child_process';
import { join } from 'node:path';

/** stdout of `python3 tools/registry_decl.py` (id \t key \t global, one per line). */
export function registryText(root) {
  if (process.env.HORDE_FACT_REGISTRY !== undefined) return process.env.HORDE_FACT_REGISTRY;
  return execFileSync('python3', [join(root, 'tools/registry_decl.py')], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
}

/** Does git ignore `rel` (repo-relative)? `noIndex` is `git check-ignore --no-index`. */
export function gitIgnored(root, rel, noIndex = false) {
  const key = `${noIndex ? '--no-index ' : ''}${rel}`;
  if (process.env.HORDE_FACT_GIT_IGNORED !== undefined) {
    const given = JSON.parse(process.env.HORDE_FACT_GIT_IGNORED);
    if (key in given) return given[key];
  }
  try {
    execFileSync('git', ['-C', root, 'check-ignore', '-q', ...(noIndex ? ['--no-index'] : []), rel], { stdio: 'ignore' });
    return true;
  } catch (_) { return false; }
}
