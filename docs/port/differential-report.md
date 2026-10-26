# The differential report

> **Origin.** horde lead, 2026-10-26, ROADMAP B523 (ADR-247 audit P8; spec 10.6 item 3). How to run
> `tools/engine2_differential.mjs`, the tool that compares the new engine's bank renders with the current
> engine's and ranks them into the capped listening batch. **Reported, never a gate.** Run by the lead, in
> neither clean-room tree, once at the bank stage.

## What it is for

Before cutover the human hears about 20 A/B pairs: a bank preset rendered by the new engine and by the
current one, on one of the four phrases, level-matched and labelled (spec 10.6 item 3). Which 20 of the 332
(83 bank presets, 4 phrases) is not a matter of taste: this tool measures every pair and ranks them so the
most different come first. The score orders what a human listens to. It judges nothing, and no gate reads it.

## Inputs

1. **The legacy renders** (`--legacy`): the frozen reference renders of the current engine, a release asset,
   not in git (`engine2/data/reference-renders/README.md`). They are a **224 MB download, so it needs the
   human's OK** before the lead fetches it:

   ```
   gh release download engine2-reference-renders-v1 --pattern engine2-reference-renders-v1.zip --dir <empty dir>
   unzip <empty dir>/engine2-reference-renders-v1.zip -d <another empty dir>
   node tools/engine2_reference_renders.mjs --verify <another empty dir>     # optional: hashes against the manifest
   ```

   Use the 44.1 kHz set and the new renders at 44.1 kHz; for 48 kHz fetch `-48k.zip` the same way and render
   the new engine at 48 kHz (a rate that does not match is reported as an error for that pair).
   The unzipped directory holds `manifest.json`, which the tool reads to tell bank presets from envelope presets.

2. **The new engine's renders** (`--new`): the same (preset, phrase) pairs as WAVs with the **same file
   names**, `<preset id>__<phrase>.wav` (for example `bank-01-quarter-sync__held.wav`). 16-bit, 24-bit or
   32-bit float PCM, stereo, at the legacy rate. Unnormalised, like the legacy set: the tool measures loudness
   itself and a pre-normalised render would only hide the loudness delta.

## Run

```
nice -n 10 node tools/engine2_differential.mjs --legacy <legacy dir> --new <new dir> --out local/differential [--batch 20] [--seed 1]
```

`--out` must be under `local/` (gitignored) or outside the repository: the tool refuses a tracked path.
It writes, under `--out`:

| file | what |
|---|---|
| `report.md` | the scoring rule, the batch, the full ranked table, the missing, unmeasured and skipped pairs |
| `report.json` | every measured pair with all four measures, its four terms, score and rank, and the same lists |
| `batch/NN-a.wav`, `NN-b.wav` | each batch pair, level-matched, a/b assigned blind (seeded mulberry32, `--seed`) |
| `batch/key.json` | **the answer sheet**: which of a and b is the legacy render. Do not open it before listening. |

A run reads each WAV once for the measures and again for the batch; the click detector is about 0.1 s a
channel. A full 332-pair run is minutes. The self-test is `node tools/engine2_differential.mjs --selftest`
(no renders, a few seconds; `./verify fast` runs it).

## What the four measures mean

All four are new relative to legacy; the tool never calls a number good or bad.

- **Loudness delta (LU).** ITU-R BS.1770-4 integrated loudness, new minus legacy: positive means the new
  render is louder. A render too quiet for the gates (no 400 ms block over -70 LUFS) or shorter than 400 ms
  has no number; the pair is listed under *Unmeasured* and is neither ranked nor batched, because it cannot be
  level-matched. Look at those by hand: a silent new render is the loudest difference there is.
- **Spectral distance (dB).** Long-term power in the 29 third-octave bands from 25 Hz to 16 kHz, each render
  in dB relative to its own K-weighted loudness, so a level difference cancels and what is left is timbre.
  The report gives the mean absolute band difference and the worst band with its centre frequency. A band
  more than 90 dB under the render's loudness is floored (it is quantisation noise, not music), so two empty
  bands never differ.
- **Envelope correlation.** The RMS envelope in 10 ms frames, in dB with a -90 dB floor, over the common
  length, the new render scaled to the legacy loudness first. Pearson r: 1 is the same shape, 0 or below is
  an unrelated or opposite one. Also the largest frame difference in dB and when it happens (a long release
  that one engine cuts and the other does not shows here).
- **Click-count delta.** Events of `tools/click_detector.mjs` (its ratified defaults, ADR-227), per
  channel summed, new minus legacy. The detector does not judge the first 0.12 s, and every phrase begins
  with a note-on at sample 0, so **a click at the onset of either engine is not counted**.

## The score and the batch

The rule is in the header of the tool, repeated in `report.md`. In one line: each measure's value is replaced
by its rank-normalised position over all measured pairs (0 least different, 1 most), the four are added, and a
click delta counts only when the new engine has more clicks. Ranks, because the four measures have four
units and no weighting between them has been measured; equal weights, because that is a choice not a finding.

The batch is the top `--batch` pairs in score order, **at most one per preset** (its highest-scoring phrase).
Per pair, the new render is scaled to the legacy integrated loudness. If that would pass full scale, both
files are scaled down by the same factor (the loudness difference stays 0) and the report says so. Both files
are written by the same 24-bit PCM writer so the file format does not give the answer away; lengths are the
renders' own.

## Not covered

The optional seeded fuzz of random patches through both engines (B523's "optionally") is not built. Neither is
the real run: it waits for the new engine's bank renders.
