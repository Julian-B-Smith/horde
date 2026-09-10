# LICENSE options — a human decision, prepared (B101)

*Prepared 2026-09-10 for the 1.0 definition of done. This file is NOT a
licence. Nothing is licensed until a `LICENSE` file exists at the repo root;
until then the default is all-rights-reserved and anyone reading the public
repo can see that. The choice below is the human's — two complete candidate
texts are given so the decision is a copy, not a search.*

## What is being licensed, and what is not

| Part of the tree | Author | Notes for the choice |
|---|---|---|
| `src/**` (cores, shell, GUI) · `tools/**` (oracles) | this project | The code the licence primarily governs. |
| `reference/*.html` — the seven prototypes + candidates | this project (spec-in-code, ADR-003) | They ARE the spec; parity oracles extract goldens from them. Any licence must cover them identically to `src/`, or the goldens' provenance becomes a licensing question. |
| `specs/SPEC-*.md`, `specs/ACCEPTANCE.md`, `docs/PRIOR-ART.md` | this project | Documentation. Both candidates below cover documentation; if a CC licence is wanted for prose instead, say so in `LICENSE` explicitly. |
| `specs/SPEC-STATION.md`, `specs/SPEC-FORMANT.md`, `specs/SPEC-INTENT-BUS.md`, `docs/received/**` | ingested from outside (ADR-091/122/152) | **Check authorship before licensing.** Ingested specs were written by the human or a sibling project; their terms are whatever the human sets, but a licence file claims the right to set them — confirm no third-party text is included verbatim. |
| `libs/clap` (MIT) · `libs/clap-wrapper` (MIT) · `libs/choc` (ISC) · VST3 SDK (fetched at configure; GPLv3 **or** Steinberg proprietary) | third parties | Submodules keep their own licences. **The VST3 SDK is the constraint:** shipping a VST3 binary under anything other than GPLv3 requires the Steinberg VST3 licence agreement (free, but signed). A permissive OSS choice for our code does not remove that obligation for the VST3 artifact; the CLAP and AU artifacts carry no such term. |
| Private-sibling material | aliased (ADR-014) | Never in the tree; nothing to license. |

## The two candidates

**Candidate A — MIT (permissive open source).** Anyone may use, modify,
sell, and embed, with attribution. Consequences: the coupled-oscillator
engines can be lifted into other products (including commercial synths)
without reciprocity; contributions arrive with no CLA; the CLAP/clap-wrapper
ecosystem is MIT, so the whole stack reads as one licence to a reviewer;
factory presets and the prototypes become freely reusable teaching material,
which is consistent with docs/PRIOR-ART.md's posture of citing rather than
owning the physics.

**Candidate B — PolyForm Noncommercial 1.0.0 (source-available).** Source is
public and anyone may use, modify, and share it for non-commercial purposes;
commercial use needs a separate grant from the licensor. Consequences: the
engines cannot be taken into a commercial product without a conversation; a
future paid release of horde needs no relicensing (the licensor is exempt
from their own licence); it is NOT open source by the OSI definition, so
package indexes, some distributions, and OSS-only contributors will decline
it; forks stay non-commercial. Business Source License 1.1 is the other
common source-available choice — it converts to an OSS licence on a date the
licensor sets (a "change date", typically 4 years) and needs an "Additional
Use Grant" line written by the licensor, so it is a form to fill in rather
than a text to copy; its canonical text is at
<https://mariadb.com/bsl11/> (SPDX id `BUSL-1.1`).

**Recommendation (one paragraph, the lead's, not a ruling).** Choose by the
question "is the engine the product, or is the product the product?" If the
plan is a paid plugin whose value is the coupled-oscillator engine itself,
Candidate B protects that while keeping the reference prototypes public —
which is the honest posture for a repo whose spec is code. If the plan is the
research and the instrument as a public artifact, with income (if any) from
services, presets, or a hosted layer, Candidate A costs nothing and buys
contributions and citations. Either way: the VST3 SDK row above applies to
the shipped VST3 regardless of choice, and the ingested-spec row needs a
five-minute authorship check before any `LICENSE` is committed. Undecided
stays all-rights-reserved, and reviewers of a 1.0 will read that as a
decision too.

## How to apply the choice

1. Copy the chosen text below into `LICENSE` at the repo root, verbatim
   (fill the `<year>` / `<copyright holders>` placeholders in Candidate A;
   Candidate B has no placeholders — add a one-line
   `Copyright (c) 2026 <holder>` header above it).
2. Add the SPDX line to `README.md` (`SPDX-License-Identifier: MIT` or
   `PolyForm-Noncommercial-1.0.0`) and strike the TODO in `CHANGELOG.md`.
3. If the prototypes or specs get different terms, say so IN `LICENSE`
   (a "Documentation and prototypes:" paragraph), never in a second file a
   reader might not find.
4. Record the choice as an ADR (DECISIONS.md) — it is irreversible in the
   sense that a permissive grant, once published, cannot be withdrawn from
   copies already taken.

---

## Candidate A — MIT License (complete text, SPDX `MIT`)

```
MIT License

Copyright (c) <year> <copyright holders>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.
```

---

## Candidate B — PolyForm Noncommercial License 1.0.0 (complete text, SPDX `PolyForm-Noncommercial-1.0.0`)

```
# PolyForm Noncommercial License 1.0.0

<https://polyformproject.org/licenses/noncommercial/1.0.0>

## Acceptance

In order to get any license under these terms, you must agree
to them as both strict obligations and conditions to all
your licenses.

## Copyright License

The licensor grants you a copyright license for the
software to do everything you might do with the software
that would otherwise infringe the licensor's copyright
in it for any permitted purpose.  However, you may
only distribute the software according to [Distribution
License](#distribution-license) and make changes or new works
based on the software according to [Changes and New Works
License](#changes-and-new-works-license).

## Distribution License

The licensor grants you an additional copyright license
to distribute copies of the software.  Your license
to distribute covers distributing the software with
changes and new works permitted by [Changes and New Works
License](#changes-and-new-works-license).

## Notices

You must ensure that anyone who gets a copy of any part of
the software from you also gets a copy of these terms or the
URL for them above, as well as copies of any plain-text lines
beginning with `Required Notice:` that the licensor provided
with the software.  For example:

> Required Notice: Copyright Yoyodyne, Inc. (http://example.com)

## Changes and New Works License

The licensor grants you an additional copyright license to
make changes and new works based on the software for any
permitted purpose.

## Patent License

The licensor grants you a patent license for the software that
covers patent claims the licensor can license, or becomes able
to license, that you would infringe by using the software.

## Noncommercial Purposes

Any noncommercial purpose is a permitted purpose.

## Personal Uses

Personal use for research, experiment, and testing for
the benefit of public knowledge, personal study, private
entertainment, hobby projects, amateur pursuits, or religious
observance, without any anticipated commercial application,
is use for a permitted purpose.

## Noncommercial Organizations

Use by any charitable organization, educational institution,
public research organization, public safety or health
organization, environmental protection organization,
or government institution is use for a permitted purpose
regardless of the source of funding or obligations resulting
from the funding.

## Fair Use

You may have "fair use" rights for the software under the
law. These terms do not limit them.

## No Other Rights

These terms do not allow you to sublicense or transfer any of
your licenses to anyone else, or prevent the licensor from
granting licenses to anyone else.  These terms do not imply
any other licenses.

## Patent Defense

If you make any written claim that the software infringes or
contributes to infringement of any patent, your patent license
for the software granted under these terms ends immediately. If
your company makes such a claim, your patent license ends
immediately for work on behalf of your company.

## Violations

The first time you are notified in writing that you have
violated any of these terms, or done anything with the software
not covered by your licenses, your licenses can nonetheless
continue if you come into full compliance with these terms,
and take practical steps to correct past violations, within
32 days of receiving notice.  Otherwise, all your licenses
end immediately.

## No Liability

***As far as the law allows, the software comes as is, without
any warranty or condition, and the licensor will not be liable
to you for any damages arising out of these terms or the use
or nature of the software, under any kind of legal claim.***

## Definitions

The **licensor** is the individual or entity offering these
terms, and the **software** is the software the licensor makes
available under these terms.

**You** refers to the individual or entity agreeing to these
terms.

**Your company** is any legal entity, sole proprietorship,
or other kind of organization that you work for, plus all
organizations that have control over, are under the control of,
or are under common control with that organization.  **Control**
means ownership of substantially all the assets of an entity,
or the power to direct its management and policies by vote,
contract, or otherwise.  Control can be direct or indirect.

**Your licenses** are all the licenses granted to you for the
software under these terms.

**Use** means anything you do with the software requiring one
of your licenses.
```

*Texts above are the SPDX license-list canonical copies, fetched 2026-09-10.*
