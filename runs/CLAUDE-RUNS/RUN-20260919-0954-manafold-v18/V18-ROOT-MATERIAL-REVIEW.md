# Manafold version 18 — Wave C root material independent review

**Date:** 2026-09-19
**Scope:** uncommitted `mkmanafoldpage.py` / generated `manafold_page.h` plus Wave-C reports and pictures
**Verdict:** **PASS after one evidence-only hash correction — selected generator/header are reproducible, legacy output is exact, and native motion supports 16/16 without an ink change**

## Findings

No production blocker survived review.

### P3 — legacy candidate renderer hash was mistyped in the implementation report

The implementation table originally omitted two hexadecimal characters from the legacy candidate renderer MD5. The actual isolated binary is:

`2FD9B451D8125DBEFAFAB7451BFBE570`

The report was corrected. Candidate bytes, page words and the art decision were unaffected.

## Generator and generated-asset truth

`tools/pack/mkmanafoldpage.py` remains the only author of `tools/reel/manafold_page.h`. Regenerating from checked-in defaults produced the exact working-tree SHA-256:

`95f90d46443502211ec44347eccc7b22061e6c01e03e20ce685a9a27cd939564`

The header truthfully identifies itself as a committed generated production asset and records `mode=bodyblend front_rows=16 rear_rows=16`.

A separately generated `legacy` header was compared against `df072d47:tools/reel/manafold_page.h` by parsing all generated arrays:

| Array | Words | Differences |
|---|---:|---:|
| `kU02Atlas` | 87,380 | 0 |
| `kU02Eye` | 5,461 | 0 |
| `kU02Star` | 5,461 | 0 |

Thus all **98,302 / 98,302** version-17 page words are exact in legacy mode, not merely the base atlas.

## Selected-mode isolation and boundaries

The base 256×256 atlas was compared before mip generation. Exactly 8,192 pixels change, all inside the loop band:

- front absolute atlas rows `136..151` (16 rows);
- rear absolute rows `185..200` (16 rows);
- zero changed pixels outside `LOOP_V0..LOOP_V1`;
- handoff rows 152 and 184 are byte-identical to the legacy loop field;
- eye and star arrays are exact between selected and legacy modes.

The front and rear fields cannot overlap: widths must be positive, each less than the 65-row band, and their sum must leave at least one free middle row. Selected 16/16 leaves the complete middle interval and uses endpoint-exact quintic weights. The loop field is generated first with its version-17 seed. The body-style root field uses its own deterministic `RandomState`, so adding it cannot advance global randomness or contaminate body, hinge, eye or star generators.

Atlas mip words that spatially downsample a changed loop row necessarily change too; this is the intended mip representation of the root transition, not a second source mutation. The source-level base atlas proves body/hinge/non-loop ownership unchanged, while complete protected render/gate results cover any real coarse-mip sampling consequence. The evidence must not overclaim that every deep shared-atlas mip texel outside a conceptual part remains byte-identical.

## CLI and diagnostics

Independent invalid invocations returned RC 2 for:

- unknown root mode;
- zero bodyblend width;
- overlapping `32/33` widths;
- nonnumeric width.

`legacy` intentionally ignores width values because the width operand is inactive in that mode; it still generates exact version-17 words. Shipping defaults are explicit constants and diagnostics cannot silently alter the committed header at runtime.

## Candidate and final-generation identity

Isolated candidate binary MD5s were independently re-read:

- legacy `2FD9B451D8125DBEFAFAB7451BFBE570`;
- narrow `B8F5C3E443D09BD430BE502C37939691`;
- selected medium `95080A676C3B5315A9DBEF58D9D10BDA`;
- broad `CCD06BB2EFDCD92F317A028C14C116C3`.

All 2,048 final working-tree frames are byte-identical to the reviewed medium candidate across Hover, Rest, Taunt, Taunt III and Trick. This independently ties the selected art pictures to the source being reviewed.

## Picture review

The complete four-rung native plate and enlarged root plate support the implementation verdict:

- legacy keeps a conspicuous dark/coarse strip through each body crossing;
- narrow softens the edge but hands back to coarse loop texture inside the connection;
- **medium 16/16** reads as body material through both visible roots while keeping the free antenna distinct;
- broad carries body treatment too far into the free chain and weakens antenna character.

The selected roots retain visible grain and shaping rather than becoming flat/plastic. No new outline hole, black-through-effect fault or coarse material step is visible in the five complete selected sheets.

A separate medium/no-body-inner-owner comparison was inspected at multiple orbit views. It removed accepted body/head contour away from the roots and did not materially improve the connection. The decision to leave `add_visible_body_inner_edge()` untouched is justified; no posed exclusion is currently earned.

## Protected contracts

The implementation matrix reports normal `mspan` and `msmooth` RC 0, 32/32 root/span controls and 16/16 effect controls attributed RC 1, plus green public/nodule/probe/mesh/outline/shell/QA/eye matrices. Zixxtrixx Idle remains 576/576 byte-identical. The source diff changes no geometry, weights, bones, swells, motion, ink, effects, site or RTL.

## Commit manifests

### Source — exactly two paths

- `tools/pack/mkmanafoldpage.py`
- `tools/reel/manafold_page.h`

### Curated evidence

- `V18-ROOT-MATERIAL-IMPLEMENTATION.md`
- `V18-ROOT-MATERIAL-REVIEW.md`
- `V18-ROOT-MATERIAL-LADDER-NATIVE.png`
- `V18-ROOT-MATERIAL-ROOTS-2X.png`
- `V18-ROOT-MATERIAL-HOVER-ALLFRAMES.png`
- `V18-ROOT-MATERIAL-REST-ALLFRAMES.png`
- `V18-ROOT-MATERIAL-TAUNT-ALLFRAMES.png`
- `V18-ROOT-MATERIAL-TAUNT3-ALLFRAMES.png`
- `V18-ROOT-MATERIAL-TRICK-ALLFRAMES.png`
- `TASK_LOG.md`

Keep candidate source snapshots, raw frames, the complete-inner-ink removal diagnostic and temporary review plates unstaged.
