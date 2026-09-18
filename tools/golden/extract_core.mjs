/*
 * extract_core.mjs — load a prototype's headless DSP core WITHOUT forking it.
 *
 * The prototype HTMLs are the reference implementation (ADR-003) and are
 * protected paths; a copied-out core would silently diverge the first time
 * the reference updates (the file-drop lesson, LIBRARY L0001). So the golden
 * generator evals the DSP section straight out of the HTML at run time.
 *
 * Extraction is marker-based, not line-based: every prototype delimits its
 * core with banner comments —
 *     /* ================= DSP: ... =================
 *     ... constants + core class ...
 *     /* ================= Audio graph =================
 * Line numbers drift across reference updates; the banners have been stable
 * across v1→v2 of two prototypes. If a banner disappears, fail loudly.
 *
 * TWO BANNER CONVENTIONS EXIST, so the pair is a named argument rather than a
 * constant (B152). The reference/ prototypes use `DSP: <name>` … `Audio graph`;
 * nine of the ten docs/design labs — bend, cooperator, ensemble, filter, mod,
 * reverb, shape, spectra, width — use `DSP` … `UI`. The tool takes both
 * because the labs' own naming is the majority house convention there and is
 * reasonable; renaming a lab's banners to fit the tool would edit the artefact
 * to suit its instrument. Default is `reference`, so existing callers are
 * unchanged. The two start patterns are mutually exclusive by construction
 * (`DSP:` cannot match `DSP =`), so a mis-named pair fails loudly rather than
 * silently slicing the wrong span.
 */

import { readFileSync } from 'node:fs';

export const BANNERS = {
  reference: { start: /\/\* =+ DSP:/,   end: /\/\* =+ Audio graph/, name: 'DSP:/Audio graph' },
  design:    { start: /\/\* =+ DSP =+/, end: /\/\* =+ UI\b/,        name: 'DSP/UI' },
};

export function extractCore(htmlPath, className, bannerSet = 'reference') {
  const html = readFileSync(htmlPath, 'utf8');

  const b = BANNERS[bannerSet];
  if (!b) throw new Error(`extract_core: unknown banner set '${bannerSet}' — ` +
                          `known: ${Object.keys(BANNERS).join(', ')}`);

  const start = html.search(b.start);
  const end = html.search(b.end);
  if (start < 0 || end < 0 || end <= start) {
    throw new Error(`extract_core: ${b.name} banners not found in ${htmlPath} — ` +
                    `the reference's structure changed, or it uses the other banner set; ` +
                    `update the extraction markers deliberately.`);
  }

  const src = html.slice(start, end);
  if (!src.includes(`class ${className}`)) {
    throw new Error(`extract_core: ${className} not inside the DSP section of ${htmlPath}`);
  }

  // Evaluate in this module's scope; the DSP sections are self-contained
  // (constants + pure class, no DOM/window references — that is what makes
  // the prototypes headless-testable per ADR-003).
  const factory = new Function(`"use strict";\n${src}\nreturn ${className};`);
  return factory();
}
