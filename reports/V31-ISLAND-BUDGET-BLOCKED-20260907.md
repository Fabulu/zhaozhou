# §12.4's whole-island reconciliation: what it needs, and why it cannot be run yet

*2026-09-07. The recovery handoff makes this a precondition, not a nicety:
"If a correct reduced owner remains above 1,800, **reconcile the whole island
before** deciding another architecture change or asking for a resource
reallocation." The owner measures 5,678 ALM against 1,800, so that condition is
live. This is the attempt, and its result is a refusal with a named work list.*

*Analysis only. Nothing in the running `zhao_texture_v3own` fit's closure was
edited.*

---

## The composition, resolved rather than assumed

§12.4 warns that "**mutually exclusive variants, duplicate core instances, stale
fits, missing blocks and harness overlap** can make such sums meaningless", and
the ledger earns every word: **17 of its 103 rows are `@` variants**, several
mutually exclusive by construction — `MUL_LANES=1` against `=6`, `SQ_RADIX=2`
against `=4`, `lanes1-io`/`lanes2`/`lanes4-io`, `v3-before`/`after`/`after2`/
`full`/`nctx8`, `pre-rearch` against current. Summing the ledger is meaningless.

So the composition comes from what `zhao_texture_island_top` **actually
instantiates** — eleven children.

One of them had to be checked rather than believed. My first pass filed
`zhao_texture_material_combine_v2` as "no fitted row", which is the shape of the
error CLAUDE.md records — *"a block-id-to-module rule said 25 blocks were
unbuilt; three of them existed under a name the rule did not construct."* The
check inverted part of it and confirmed the rest:

* `zhao_texture_material_combine_v1` **is** in the ledger (ok, 1,475 ALM).
* The island instantiates **v2**, at line 1956 with `.NCTX(8)`. The one other
  mention of v1 in that file, at line 1915, is a comment.
* `design/prod_manifest.yml` line 205 says so deliberately: *"V1 is still the
  instantiated one"* — for the production **resource** top. That much is a
  declared divergence between two accounting domains, not a stale generated
  file.

  > **CORRECTED LATER THE SAME DAY, and against myself.** The sentence above is
  > right about `zhao_prod_top`, and I let it stand for the whole manifest
  > entry. It did not. The entry also claimed *"V2 is not instantiated by
  > anything yet … it goes into the island only after the release gate closes"*,
  > and justified declaring V2 absent because *"counting a block the machine
  > does not contain would inflate every budget it appears in"*.
  >
  > **The island contains it** — unconditionally, at line 1956, and there is no
  > `generate` block anywhere in that file. So the justification runs backwards:
  > the island's live combiner is **understated**, not inflated. Together with
  > having had no fit target until today, its cost was invisible in **both**
  > accounting domains simultaneously.
  >
  > The manifest is corrected. The production top still instantiates V1 and was
  > deliberately left alone — that file's own rule is that such a swap is *"a
  > decision to take deliberately rather than as a side effect"*.
  >
  > Worth naming the pattern: "this divergence is declared" was the comfortable
  > reading, it explained most of the evidence, and it stopped me reading the
  > rest of the entry. That is the failure mode this repository already has
  > written down.
* **No fit target defined v2 as a top at all.** It existed only as a *source*
  inside the island and the production list.

So the island's live combiner had never been fitted standalone. Fixed today —
`design/fit_targets.yml` now carries a target for it, closure resolved by
checking (v2 is a leaf: the only `zhao_` token outside its comments is its own
module declaration), with no invented ALM bound and the §3.4 DSP tripwire kept.

## `failed:structure` MEANS MEASURED

Two children carry that status, and it would be easy to discard them. The
harvest tool's own comment forbids it:

> a structure failure is a COMPLETED fit whose numbers violated a tripwire, and
> treating it the same way threw the measurement away.

It records the case that taught this: `zhao_texture_cache_pipe` fitted at 1,633
ALM after a 73% register reduction, failed `min_m10k`, and the report kept the
previous row showing the design four times worse than it is. So
`zhao_raster_perspuv_svc` and `zhao_texture_fragrob` carry real numbers.

## The refusal

With that settled, ten of eleven children had usable rows and the composed row
existed too. `tools/quartus/compare_rows.py` refused the sum anyway:

    zhao_texture_island_top           STALE by 4 commits
    zhao_raster_perspuv_svc           STALE by 2
    zhao_texture_aux_pipe             STALE by 2
    zhao_texture_cache_pipe           STALE by 2
    zhao_texture_fragrob              STALE by 5
    zhao_texture_rsp_dispatch         STALE by 1
    zhao_texture_tmu_plan             STALE by 1
    zhao_texture_material_combine_v2  NO ROW -- a missing row sums as zero,
                                      which flatters the total

**Eight blockers, not the two I had.** I had screened on the ledger's own
`rtlCleanAtHead` field, which reported `True` for five of these seven. That
field is not "current at HEAD"; the tool's commit-walk is. My screen was the
comfortable reading and the instrument caught it — the same asymmetry CLAUDE.md
names: *the defect always made the answer look better, smaller or simpler.*

**The tempting number is therefore withheld.** A leaf sum of 10,365 ALM against
a composed 16,192 gives a +5,827 delta that would read as a real composition
finding. It is not one: the composed row is four commits stale and the sum is
missing a child. Publishing it would repeat exactly the four void comparisons
this tool was written after.

## The work list this produces

Refit these eight, then the reconciliation is arithmetic:

| block | why |
|---|---|
| `zhao_texture_material_combine_v2` | never fitted; target added today |
| `zhao_texture_fragrob` | 5 commits stale |
| `zhao_texture_island_top` | 4 commits stale (the composed row) |
| `zhao_raster_perspuv_svc` | 2 |
| `zhao_texture_aux_pipe` | 2 |
| `zhao_texture_cache_pipe` | 2 |
| `zhao_texture_rsp_dispatch` | 1 |
| `zhao_texture_tmu_plan` | 1 |

Four children are already fresh and need nothing: `zhao_raster_rcp24_svc`,
`zhao_texture_bilerp_lane`, `zhao_texture_mosaic`, `zhao_texture_palette_res`.

Every one of the eight is a **texture-island** block, so this list is squarely
inside the owner's texture-first direction rather than a detour from it.

## A note on reading a tool's verdict

The refusal appeared to exit 0, which would make it invisible to any script.
It does not — it returns 2. The 0 was `tail`'s exit code, because the run was
piped. CLAUDE.md has that trap written down as *"read the build's exit code, not
the pipeline's"*, and it caught me while I was in the middle of auditing
somebody else's instrument. Verified unpiped: `RC=2`.
