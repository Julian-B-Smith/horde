// Slice the effect labs' headless cores (FilterLab / PhaserLab / TimeLab)
// live out of their HTMLs — the HTML stays the single source of truth, no
// fork. Each lab's script block declares the same top-level names
// (NB_MAX, TICK, erbN, ...), so every class is evaluated in its OWN function
// scope via new Function rather than concatenated into one module.
//
// Slice: from the line containing `const NB_MAX` (the first line of the
// core constants) to the first UI-layer line — `const $ =` (the DOM helper,
// which precedes powerOn in all three labs) or `function powerOn()`.

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');

export function extractLab(htmlFile, className) {
  const html = readFileSync(join(root, htmlFile), 'utf8');
  const lines = html.split('\n');
  const start = lines.findIndex(l => l.includes('const NB_MAX'));
  const end = lines.findIndex((l, i) => i > start && (/^\s*const \$ =/.test(l) || l.includes('function powerOn()')));
  if (start < 0 || end < 0 || end <= start)
    throw new Error(`${htmlFile}: core slice markers not found (const NB_MAX / function powerOn)`);
  const src = lines.slice(start, end).join('\n');
  if (!src.includes(`class ${className}`))
    throw new Error(`${htmlFile}: slice does not contain class ${className}`);
  // Own scope per lab: name collisions between labs are impossible.
  return new Function(`${src}\nreturn ${className};`)();
}

export const FilterLab = extractLab('reference/swarmfilter.html', 'FilterLab');
export const PhaserLab = extractLab('reference/swarmphaser.html', 'PhaserLab');
export const TimeLab = extractLab('reference/swarmtime.html', 'TimeLab');
