# THE DOCKET — every outstanding instruction, in one place

Owner instructions arrive as files in `reports/` and are easy to lose between
passes. **This is the index.** Before starting any wave, read this file, then
read the documents it names.

Last swept: **2026-09-04** — see *SWEEP 2026-09-03* below for what landed since.
The 2026-08-31 sweep followed `reports/OWNER-RULINGS-COMPLETE-20260831.md`, which
answered **all 28 open questions**. Read that file before this one; it is the
authority and this is only the index.

**The blocked list is no longer "zero of 92 buildable".** Three blocks gained
complete contracts (`GEOM.MESHFETCH`, `GEOM.VDECODE`, `GEOM.LOOM`), three were
closed by decision (`GEOM.WARP`, `INPUT.SNAC`, `MEASURE.HISTOGRAM`), and the ten
behaviour blocks have their semantics.

**Rule:** when an item is finished, move it to DONE with the commit that did it.
When a new owner document lands, add it here in the same pass that reads it.

---

## P0 — the console cannot ship without these

### D1. The 100 MHz timing surgery  ·  `reports/MHZArchitected`
**Measured, and this line was three days stale:** `gpu_clk` is **85.62 MHz**
against 100 (`reports/composed/renderer-b3bd69b-20260901T090000Z/RESULT.md`,
round 9). It opened at 53.48 MHz with TNS −6,566 ns.

Owner's diagnosis: first-composition timing debt, not existential. Five textbook
18 ns structures named. Execution order is **his**, and it is deliberate —
measure, then cheap broad fixes, then the deep ones, **fitting each step rather
than batching**:

1. ~~registered fit top; export the worst 100 setup paths~~ — **done**
2. ~~Early-Z full detection + Edgewalk registered steps / balanced popcount~~ — **done (r4, r8, r9)**
3. **streamed Edgewalk row + cross pipelines — THE ONE STEP LEFT**
4. ~~Fragment → read/shade/blend/finish/commit + in-flight address CAM~~ — **done (r3, `fc6395fd`)**
5. ~~Binner setup sequenced onto two DSPs~~ — **DO NOT DO.** Never appeared in nine fits.
6. ~~FBWRITE fixed 32-byte rows~~ — **DO NOT DO.** Never appeared in nine fits.
7. ~~shell diagnostic reductions; ENGINE0 route tripwire~~ — **done (`c23a5ef`, D2)**
8. targets: ~120 MHz isolated blocks, 110–115 MHz composed — **at 85.62**
9. **only then** add TMU v2 + cache + TEXJOIN + AUX + Field/Earth to the fit

Items 5 and 6 are struck on the note's **own** instruction to let the report
decide, not against it. Two of the five "textbook 18 ns structures" it named
turned out never to appear in a worst-100 at all — which is the note working as
intended, and worth remembering the next time a list of named offenders looks
authoritative before it is measured.

**Architecture rule adopted:** *latency may grow; initiation rate and exact
arithmetic may not regress.*

### D1 progress, measured — **NINE ROUNDS DONE, and this section was six behind**

**Refreshed 2026-09-04 from `reports/composed/renderer-b3bd69b-20260901T090000Z`,
which is the newest composed fit in the tree.** The table here stopped at round
2 and still named the Fragment RMW split as "the next surgery" — it landed in
round 3, three days ago. Anyone reading this docket to choose the next surgery
was being sent at finished work.

| | r0 | r3 | r4 | r6 | r8 | **r9** |
|---|---|---|---|---|---|---|
| **`gpu_clk`** | 53.48 | 64.66 | 79.22 | 84.97 | 81.00 | **85.62 MHz** |
| worst setup | −8.697 | −5.466 | −2.623 | −1.769 | −2.345 | **−1.679 ns** |
| endpoints | — | 1,673 | 984 | 808 | 586 | **430** |
| ALMs | 12,569 | 12,755 | 12,794 | 12,698 | 12,693 | **12,707** |
| DSPs | 16 | 16 | 16 | 16 | 16 | **16** |

**+60 % overall, 81 % of the original violation closed, for +138 ALMs — 0.3 % of
the device — and not one extra DSP across nine fits.**

### Every named offender is accounted for, and two of them never appeared

| offender | outcome |
|---|---|
| 1 EDGEWALK wide row + popcount | registered steps (r4) + CSD columns (r8) |
| 2 FRAGMENT RAM→2 multiplier layers→RAM | RMW split three ways (r3, `fc6395fd`) |
| 3 EARLY-Z 256-bit feedback cone | unique-coverage counting (r9) |
| 4 BINNER six parallel products | **never appeared in nine fits** |
| 5 FBWRITE dynamic byte-mask | **never appeared in nine fits** |

### ONE step of the plan remains, and the measurements still confirm it

    edgewalk | sx0_r[7]~DUPLICATE  ->  edgewalk | pend_r[6]     −1.679 ns

    zhao_raster_edgewalk   69 rows in the worst 100
    zhao_raster_earlyz     19

`sx0_r` is round 4's step register and round 8's CSD columns are already in
front of it, so what is left is the **fill test → `row_cov` → `pend_r` tail**
rather than the arithmetic. That is **`MHZArchitected` step 3 — "install the
full streamed Edgewalk row and cross pipelines"**, and it is the only step of
the original six that the evidence still supports.

**Steps 5 and 6 are explicitly NOT the answer.** Binner and FBWRITE have not
appeared in a single worst-100 across nine consecutive fits; doing them would be
following the note against its own instruction to let the report decide. The
shell is not the answer either — `ShellFixes.md`'s three items are done and none
of `starve_samp`, `starvation`, `cdc_err`, the DMA or the framer appears either.

**A sixth offender named in the note and still not measured:** `gpu_clk~CLKENA0`
drives **13,682 fanout** with 1.995 ns of launch/latch skew. No datapath
pipelining recovers skew, so the remaining 14 MHz should not be assumed to be
all logic until this is measured separately.

**A possible free win on the RMW loop, to MEASURE at the next composed fit.**
The path ends at `RASTER.TILESTORE`'s RAM write, and on 2026-09-04 that block's
`ram0_q`/`ram1_q` left the reset list (`70f05f31`) so its two banks could infer
as M10K. A read register with a reset cannot be the M10K's own output register,
so the fabric flop that used to sit at the end of this path may now be inside
the block. That was done for AREA and its timing effect is unmeasured — it is
listed here so the next fit is read with it in mind, not as a claim.

**Architecture rule adopted:** *latency may grow; initiation rate and exact
arithmetic may not regress.*

### D2. Shell route-integrity bug — **DONE** (`c23a5ef`)  ·  `reports/MHZArchitected`
The downstream check still rejects any write whose client is not `BLIT_DMA`, but
the guard now legitimately admits `ENGINE0` as framebuffer writer — **so every
real renderer burst latches `shell_err_route_o`**. Should be
`expected_writer = fb_writer_i ? ENGINE0 : BLIT_DMA`. Small, concrete, and a
correctness bug rather than a timing one.

### D3. Fit-top split  ·  `reports/MHZArchitected`
3,214 virtual pin bits and 1,608 ALMs holding them make the current top a poor
characterisation vehicle. Split into `zhao_shell_core` / `..._sim_top` /
`..._fit_top` (registered LFSR sources, chunked MISR sink) / `zhao_board_top`.
Then four controlled fits: core / +binner / +tile pipeline / +full renderer, for
**attribution instead of one before-after mystery**.

### D17. GitHub CI fails every push
> *"github fails all tests right now, we should fix"* — Fabian, 2026-08-31.

Run `33400920093`: **3 of 344 fast tests fail**, and a fourth problem warns on
every job.

| | failure | status |
|---|---|---|
| a | `format_check` — 17 files drifted from the pinned clang-format | **fixed** `fdc57ca` |
| b | six stray gitlinks under `runs/*/work/` with no `.gitmodules`, so every checkout exits 128 | **fixed** `fdc57ca` |
| c | `cppcheck_check` — signed-overflow finding in `render_pipe_directed.cpp` | **fixed** `d93bf0b3` |
| d | `reel_sequence_crc` — `zhao-reel --check` fails | **not a defect — see below** |
| e | the gate itself SKIPPED silently when cppcheck was absent | **fixed** 2026-09-04 |

Note (b) is why the *passing* jobs also printed a red git warning; it is not
cosmetic, it is a committed mistake.

**(e) is why (c) was found by CI instead of locally.** `cppcheck_check` printed
STATUS and returned when the tool was missing, and CTest was configured to read
that as a SKIP — so local was green and CI red, which is exactly the standing
memory *"local gates must match CI"*. Absence now fails loudly, with the choco
line and an explicit `ZHAO_ALLOW_MISSING_CPPCHECK=1` opt-out that still prints.

**(d) is expected churn in another lane, not a bug to fix here.** `zhao-reel
--check` re-renders every reel subject and fails on any sequence-CRC drift.
`tools/reel/` is under active animation work — `22d61057` Stage 0 through
`ada4c105` Stage 4 of a retime skeleton — and every one of those commits moves
the animation, so the CRCs drift by design. **Re-baselining them belongs to
whoever is authoring the motion**, and doing it from this side would erase the
evidence that lane depends on. Checked 2026-09-04 rather than reproduced; the
docket said "reproducing" and the honest answer is that there is nothing here
to reproduce.

**There is no npm package to pin cppcheck with**, unlike clang-format, so the
version is checked instead of pinned — and the drift is live: **CI pins 2.19.0,
this machine has 2.20.0, and (c)'s finding does not reproduce on 2.20.0 at
all.** Same command, same file, different answer. A mismatch warns rather than
errors; a newer cppcheck is usually a better one, but it is never silent.

---

## SWEEP 2026-09-04 — decision-bearing documents that were not indexed

A sweep of `reports/` against this file found **70 of 98 files unindexed**. Most
are creature-lane working notes and belong to that lane, not here. **Three carry
decisions**, and one of those is still open — which is the failure this index
exists to prevent, since an unindexed decision is an undelivered one.

### D19. `min_m10k: 8` on TEXTURE.CACHE cannot be met — **OPEN, needs a ruling**
`reports/RAM-INFERENCE-RANKED-20260904.md`, final section.

The storage rework worked: `"cannot regroup"` is gone and all four lanes'
`data_r` became `altsyncram` at 2,048 bits each, against 128 block memory bits
on the previous fit. But `tag_r` is **16 deep x 24 wide = 384 bits per lane**,
below the size Quartus will ever put in an M10K — flip-flops are the correct
answer for it. So the block has **four** arrays worth being memory, not eight,
and island brief S3.4's `min_m10k: 8` will fail a correct design.

**Not changed**, because lowering a gate so the thing passes is the failure the
gate exists to prevent. Two possible rulings: the tripwire is wrong (capacity
floor says 4), or the cache is half the size it was meant to be and `LINES`
should grow — a hit-rate decision, not a syntax one. **Until this is ruled, a
failing `min_m10k` on this block is not evidence of an RTL defect.**

### D20. The eight fundamentals rulings — **answered, and the authority**
`reports/OWNER-RULINGS-20260903-FUNDAMENTALS.md`, with the questions as posed in
`reports/FUNDAMENTALS-DECISIONS-NEEDED.md`. All eight are ruled and each is
recorded in the contract it governs. Indexed here because the rulings file is
the authority and was reachable only by knowing it existed.

### D21. CREATURE.LIGHT's additive term — **provisionally accepted**
`reports/CREATURE-LIGHT-ADDITIVE-COST-JUDGEMENT.md`. Owner: *"We will
provisionally try to make the light thing happen ... our budget is fucked but we
can always go back on this."* Written into the specs on that basis; the
reversal, if it comes, is a spec edit and not a rebuild.

---

## P1 — the game's main thing

### D4. The 8 km world is unbuilt, not unarchitected  ·  `reports/Missingterrain`
> *"We really need to get to implementing the main thing of our game at some
> point."*

The terrain **format** is architected for a sparse streamed 8 km world; the
**composition** can only process patches a harness hands it. Missing: the world
pager, patch-residency manager, composed-height cache, and command→terrain
pipeline.

Three sizes are being confused and the doc separates them: raster tile 16×16
**pixels**; terrain patch 32×32 cells = **64 m**; island = a sparse directory of
patches in ±32 km fx16. `TERRAIN.LOD` already does ~784 clocks/patch ≈ 2,100
patch decisions a frame against 256 live patches — it was never a one-patch toy.

### D5. Creature presentation lane  ·  `reports/CREATURESANDLIGHTS`
> *"read this one to fill holes, clarify, unify, and show you how it should be
> done"* — this is the unifying document; read it **after** the others.

One lane, collapsing work at the right level: motion per bone on HPS; point-light
geometry reduced per creature/meshlet; lighting accumulated per unique vertex;
toon quantisation once per surviving cel fragment; outlines once per view from an
explicit mask.

**A specification inconsistency to repair before lighting RTL freezes** —
**DONE 2026-09-04.** `spec/creature_rules.md` §2.x carried
`lam = (w0·clamp(N·L_b0) + w1·clamp(N·L_b1) + 32) >> 6` as adopted LAW, with the
note *"no renormalisation anywhere, which is the cheap form the silicon
increment would build"*. Verified against the code, not the summary:
`skin_normal_lambert` blends the normal VECTOR, renormalises once via
`isqrt_u64`, and takes Lambert last. **The spec had a ratified law its own
oracle never implemented.**

Repaired in new §2.x.1, old text struck rather than deleted. **Nothing had been
built to it** — no RTL in the tree does normal skinning — so this landed before
the freeze rather than after.

**And it was RECOSTED**, because the struck law was chosen for being cheap and
the cost bullet still priced it: ~27 multiplies and **one square root per
vertex**, plus 3 multiplies and a divide per light, against six multiplies per
light and no per-vertex work before.

**The reference's per-light repetition is explicitly NOT law** (owner: *"the
hardware should not reproduce that structure"*). Transform, blend and
renormalise once per VERTEX; each light is then one dot and one divide. That is
the one place where being bit-exact with the reference's *structure* would be
wrong — bit-exactness is owed to its result.

Also: `GEOM.SKIN` outputs positions, **not normals** — normal skinning is
reference-only. And `GEOM.SKIN` fits at 89.65 MHz with 9 DSPs and one weighted
vertex per 12 clocks; **nothing more may be bolted onto its output.**

### D6. Cape / secondary motion  ·  `reports/CapeProvisions.md`
No cloth processor. Reserve **6 bones** (waist/thigh) or **8** (long, dramatic),
two columns × three or four rows so left and right react independently.
150–300 triangles. Two-weight skinning covers it.

**The one genuinely missing feature: per-instance pose overrides.** `GEOM.POSE`
caches by `{type, clip, frame}`, which is right for armies and wrong for a cape
in wind — a sparse per-instance patch over the shared palette's cape bones.

### D18. Mana territory — the economy  ·  `Upheaval/docs/MANA-TERRITORY.md`
Owner direction 2026-08-31, recorded in full in the **game** repo (`450acc4`).
Wells are taps driven into the island; **claimed terrain conducts mana**;
availability is a **local field**, not a global number; a cell only counts with
a continuous claimed path to a well; **destroyed or newly created terrain begins
neutral**; and spell tier bounds how economically consequential a wound is
allowed to be while terrain decides where it lands.

**Console-side consequence, and it is small:** the claim is *gameplay-grid*
state (`owner_id` + `strength`, coarse, deterministic integer propagation) and
its presentation is **one more terrain material input** — not a lighting model,
not a renderer feature. Nothing here is authorised console work yet; it depends
on **D4** and showcases with **D8**, and **it does not reorder D1.**

Five numbers/questions in that document are explicitly the owner's and must not
be invented.

---

## P2 — stretch, wanted

### D7. Sunder  ·  `reports/SUNDER.md`
> *"A stretch goal, but one we really want. Shouldn't cost too much."*

Mantle can already **represent** the cut: `remaining_top = max(bottom, min(old_top, cut_y))`
for a plane `cut_y = a·x + b·z + c`. Flat nub at `a=b=0`, sloped otherwise, and
the result is still one top per (x,z) so the dual-heightfield likes it.

What it cannot do is turn severed material into an **independently moving
terrain body** — that needs a new runtime/world-object layer, but **not** voxels,
not CSG, not a terrain rewrite.

### D8. Double-helix tornado + site refresh  ·  `reports/DoubleHelixTornado.md`
> *"try to get a render out of it after finishing the 53 MHz and whatever else
> important follows right after."*

**Explicitly sequenced after D1.** Level 9 spell, two tornadoes orbiting a
travelling centre, feet issuing persistent terrain stamps that carve an
intertwining helix. Hybrid — no single subsystem builds the tornado.

---

## P3 — development environment

### D9. Parallel PC + console development  ·  `reports/Future.md`
> *"Please start implement these before you finish the goal. That way the goal
> will remain unfinished until you finish implementing these issues."*

Three parts: (a) PC and console versions that **do not diverge**, PC carrying
online multiplayer, higher resolution, an authentic mode, "the works";
(b) **ZEMU running and playing the game** as the console dev environment;
(c) the programming language — *"past the normal basics it should only grow when
developing the game, but when a feature is needed, it gets implemented in the
language as a first-class feature."*

Note this sits against the standing memory *"hardware first; Nanquan is
provisional — stop compiler overengineering"*. The owner has now asked for the
language to grow **demand-driven from the game**, which is compatible: it grows
only when the game needs a feature.

---

## Carried over from earlier waves

| | item | where |
|---|---|---|
| D10 | depth profiles proved but nothing consumes them; 5 mechanical steps + 1 ABI decision | `DEPTH_PROFILE_NEXT_STEPS.md` |
| D11 | `GEOM.PARAMBUF` — external geometry parameter buffer; supersedes growing the M10K arena | `OWNER-RULINGS-20260831.md` #4 |
| D12 | cel-material **fog ordering** contradicts the general per-vertex law — Class C | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D13 | pose palettes must not live in M10K (1,344 B/pose) | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D14 | `TILESTORE.INK` + `POST.INK` — the hard creature feature | `ZIXXTRIXX_CEL_IN_HARDWARE.md` |
| D15 | seven stub contracts; `MEASURE.HISTOGRAM` deliberately refused | `CONSOLE_REMAINING.md` |
| D16 | TMU target closure | `fpga/rtl/texture/OWNER-DIRECTION-TMU-TARGET-CLOSURE.md` |

---

## SWEEP 2026-09-03

**Two new owner documents, both added here in the pass that read them, per the
rule above.**

### D19. The production-only resource count  ·  `reports/WeNeedSomeMeasurements.md`  — P0
> *"The genuinely alarming thing would be continuing to build for several more
> days without producing the production-only hierarchical resource report. That
> report should now be treated as a near-term gate."*

The repository-wide **185-DSP figure is a SOURCE INVENTORY, not the machine** —
it adds every top-level `.sv`, so it counts old and new caches together, serial
and scheduled reciprocals together, probes and leaf-fit wrappers. The owner's
own verdict: DSP panic is a bookkeeping mirage (~41 of 112 estimated); **fabric
is the real concern** (~30,141 ALM ≈ 72% before integration glue, against a
practical ceiling of 37,719 with the charter's 10% reserve).

Gates the owner set: **37,719 ALM / 100 DSP / ~497 M10K.**

Built today: `design/prod_manifest.yml` (one chosen implementation per logical
block; all 168 modules either counted, inside something counted, or excluded
with a reason — enforced by `tools/quartus/check_prod_manifest.py`),
`tools/quartus/gen_prod_top.py`, and the fit is running.

### D20. Terrain detail normal maps  ·  `reports/NORMALMAP-ARCHITECTURE.md`  — P1
> *"But we make it and see how bad it is. We'll optimize and cut after we have
> the number anyway ... Normal maps would be a huge gain though."*

Architected today. **The finding that reframes it: production terrain has no
lighting at all** — `zhao_terrain_normals` is instantiated only by a leaf-fit
probe, nothing in `prod_manifest.yml` consumes it, and `TERRAIN.PROJECT` carries
no colour port. So the work splits:

* **TERRAIN.SHADE** — the per-triangle base `dot(n,L)/|n|`. ~730 ALM, 10 DSP.
  **Not cuttable: it is the terrain's light.** Needed with or without normal maps.
* **TERRAIN.NORMALMAP** — the detail delta. ~380 ALM, 2 DSP, 8 M10K, and
  cuttable cleanly. `RASTER.FRAGMENT`, `TEXJOIN` and the TMU are all untouched.

Gate before RTL, per the art law: the amended oracle goes in the zref renderer
and **the owner looks at the island under a moving sun first.**

### Ten blocks built since the last sweep
`TERRAIN.MIPGEN`, `TERRAIN.RESIDENCY` v2, `PART.RECORD`, `PART.LADDER`,
`GEOM.VDECODE`, `GEOM.PARAMBUF`, `POST.GATHER`, `TWOD.SPRITE`, `TWOD.PLANE`,
`FORGE.PRIM`. So D15's "seven stub contracts" and the docket's "zero of 92
buildable" are both out of date; `tools/ledger/remaining.py` now derives the
real list instead of it being audited by hand.

### The texture island answered its fit question
`reports/TEXTURE-ISLAND-FIT.md`: three or four blocks were genuinely limited by
their own logic, not ten. `perspuv_svc` 62.67 → 99.14, `texjoin_v2` 61.66 →
93.12, `cache_pipe` 98.66 reported / 109.05 internal **with the RAMs still RAMs**
(2 M10K, 0 DSP) — which was X7's actual acceptance question, not Fmax.

### Ten blocks could not be synthesised by the pinned toolchain at all
60 Quartus 17.0.2 syntax errors across ten blocks that Verilator and slang both
accept — every one "verified" in simulation and never through the fitter.
**Three of the five causes were already written in `reports/QUARTUS_GOTCHAS.md`
and were rediscovered anyway.** Two new ones (`foreach`; unary minus on a size
cast) are now recorded there, with the meta-lesson: read that file before
touching RTL only Verilator has ever seen.


### D21. The texture island is 2.5x its own redline  ·  `reports/islandrearchitecture5.md`  — P0
> *"Agent please read, full brief!"* (2026-09-03 08:04)

**Supersession chain, recorded so it is not re-read in the wrong order:**
`Islandrearchitect.md` (06:53) -> `Islandrearchitect2.md` (07:19) ->
`Islandrearchitect3.md` (07:24) -> `islandrearchitecture4.md` (07:56, since
replaced) -> **`islandrearchitecture5.md` (08:04, THE LIVE ONE)**. Later
supersedes earlier, per owner. Note "island" here is the **texture-survivor
island**, NOT the 8 km terrain island -- two different things with one word.

It is a **resource recovery specification** with numeric tripwires, not advice.
Measured against it today (see `reports/TEXTURE-ISLAND-FIT.md` addendum):

| | ALM | reg | M10K | DSP |
|---|---|---|---|---|
| island as built | **16,576** | **27,793** | 10 | 19 |
| hard redline | 7,500 | 9,000 | 64 | 14 |
| the prototype it replaces | 15,749 | 25,123 | 11 | 16 |

**The rebuild is worse than the prototype on every axis except DSP.** The
prototype's diagnosis was state in flip-flops instead of memories; the rebuild
took registers from 25,123 to 28,143 with M10Ks from 11 to 10.
(Corrected: an earlier total of 18,497/28,143/25 double-counted
`zhao_texture_tmu`, superseded at `prod_manifest.yml:162`. Even the corrected
total is INCOMPLETE -- `zhao_texture_tmu_pipe`, the production sampler, is
committed unfinished and has never been fitted, and the brief's S3.3 has no
budget line for the sampler datapath at all.)

`zhao_texture_cache_pipe` -- the brief's "ALM RECOVERY CENTRE" -- is 5,903 ALM
/ 11,328 reg / 2 M10K against tripwires of 1,500 / 2,000 / >=8, and against a
predecessor that was 1,087 / 1,737 / 4. **Its 98.66 MHz was reported as a pass
this morning; by the brief's own rule it is not one.**

The brief's REWRITE BEFORE INTEGRATION list already named `cache_pipe` storage
and hit path, TEXJOIN wide storage, PERSPUV's token table and RCP's scans. The
fits confirm all four. Phases and per-component budgets are in the brief; the
C-numbered acceptance gates (C1-C26) are its checklist.

**Standing constraint from the same brief:** *"REJECT: adding terrain/Field RTL
faster than the texture fit can be closed."*

### D22. Animation banks live in HPS DDR  ·  `ZHAOZHOU_ANIMATION_MEMORY_ARCHITECTURE.md` + `..._HPS_RESIDENCY_ARCHITECTURE.md`  — P1
> *"These are two important files for animation architecture ... add them to the
> queue at least what hardware is concerned."* (2026-09-03 14:13)

**Binding.** Too much high-quality animation to fit local RAM, and demand is
known far enough ahead to stream it. So: cartridge is cold; **HPS/ARM DDR owns
the loaded animation library**; local 128 MB SDRAM holds only a pinned render
working set; `GEOM.POSE` sees only complete, immutable, locally resident clip
pages. Supersedes the old wording *"VRAM stores clips compressed."*

**Hardware consequence is deliberately near-zero:** no new render-time hardware
path, `GEOM.POSE` gains no dependency on HPS latency or Linux scheduling, and
the FPGA never sees a null pointer, an HPS address posing as a VRAM address, a
partial upload, a stale generation, or a request meaning "stall until Linux
answers". A residency miss must never become a blocking FPGA fetch: the frame
is not published, the previous complete frame repeats under the hard-60-Hz
late-frame law, and a deadline fault is recorded. Unchanged: 30 Hz keys, hard
cuts, event tags, quantized quaternions, the decoded-pose cache, pose sharing,
no per-limb upload.

Prefetch policy freezes **who guarantees residency**, not an algorithm, and
explicitly prefers whole-bank residency first -- pages only after traces show a
real local-SDRAM cost.

### D23. `SaveTheRendered.md` (repo root)  — P1, explicitly AFTER the islands
> *"Agent please read. After the islands, this is next."* (2026-09-03 10:23)

Sequenced by the owner behind the island work. Not yet read in detail.

### D24. ZEMU, the omniscient development machine  ·  `reports/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md`  — P3
> *"Put this jewel where it belongs and make sure it never gets forgotten.
> Emulator directory or something, I think we already have one."* (2026-09-03 09:44)

3,027 lines. Pairs with D9/`Future.md` (ZEMU running and playing the game).
**Placement is an explicit instruction and is still outstanding** -- it is
sitting in `reports/` where the owner said it should not stay.


### D26. The boring 3D fundamentals audit  ·  `reports/BORING_3D_FUNDAMENTALS_AUDIT.md`  — P0
> Owner, 2026-09-03: *"turns out we have some critical gaps man ... we forgot
> normal maps. What other basic shit did we forget?"*

**Ten gaps, and the pattern is one sentence:** *"we built impressive endpoints
and sometimes forgot the boring organ connecting them."* Normal maps exposed it
because terrain had a normal generator and nothing consumed the normals. The
reassurance is equally specific: these are **connective tissue, not another
giant computation island** — only lighting has real capacity teeth, and even
there the answer is bounded per-vertex work, not a shader core.

**The four with NO NAMED OWNER, which is worse than unfinished:**

* **R1 mesh index → triangle assembly.** VERIFIED against the tree:
  `zhao_geom_vdecode.sv` accepts neither `index_offset` nor `triangle_count`;
  `zhao_geom_setup.sv` wants a complete triangle; `tri_ax_i` is driven only
  from the shell harness; and the ledger has **zero** `GEOM.(ASSEMBLE|INDEX|TRI)`
  blocks. Nobody turns a meshlet's index stream into triangles. **Must not be
  allowed to emerge as miscellaneous logic inside `GEOM.PARAMBUF`.**
* **R3 `MATERIAL.RESOLVE`.** All the nouns exist — `material_set` on DrawForm,
  `material_id` on meshlets, FRAGROB expecting everything already resolved —
  and no verb turns one into the other.
* **R4 the cartridge has no generic texture/material kind**, while three
  subsystems already assume one.
* **R7 the fog carrier.** The arithmetic is specified; `RASTER.FRAGMENT` says
  colour arrives already fogged; `GEOM.PROJECT` has no colour input to have
  fogged it with.

**The rest:** R2 the full lighting route (terrain caught this session,
creatures/meshes still open — `GEOM.PROJECT` as "projection + lighting" is
aspirational, the source does projection); R5 cache coherence as upload's
missing second half; R6 the depth disagreement (verified: producer emits
Q16.16 1/w, twelve files consume `invw24`, no `depth_profile` port exists);
R8 a frozen cheap contact-shadow law, ~zero hardware and high visual return;
R9 local spell lighting; R10 water/lava, or an explicit refusal.

**The audit's own rule**, and the reason the file exists: every capability gets
one row across the complete chain from authoring bytes to production manifest
entry, and **a row is RED if even one arrow is `???`**. All six of today's
discoveries would have been red rows.

**Priority order is the owner's** and is in the file. Note that items 1–7 are
almost entirely contract and format work, so they do **not** need the Quartus
toolchain and can proceed alongside fits.


---

## ISLAND RECOVERY PROGRESS — 2026-09-03 evening

Against D21. The owner's sequence for the session was: fix what is known
wrong, Save the Renderer, island recovery, resource count LAST.

### The gate that makes every later fit honest — DONE `3bc25633`
The brief's §3.4 tripwires existed only as prose, which is why a cache with
2 M10K where its predecessor had 4 was reported as a pass at 98.66 MHz.
`design/fit_targets.yml` now carries `rules:`, the runner enforces them, and
`tools/quartus/check_fit_rules.ps1` audits every recorded row with no Quartus
at all. It fails 4 of 4 on today's numbers, including the one that passed.

### The root cause, fixed — `97d3b637`
`zhao_texture_cache_pipe` reported `blockMemoryBits: 128` against its
predecessor's 8,192. Reads were correctly synchronous; the **writes** sat in
the async-reset process, and an M10K has no reset port. Moved to a clock-only
process; behaviour identical, 10 checks, throughput unchanged at 1.02
clocks/access. **The fit answering this is the session's open question.**

### TEX.FRAGROB, the other half of step 1 — `5f4fec80` … `a6676536`
Built **beside** v2 per §6.1 rather than editing it, so v2 stays the
behavioural oracle. Control state in flops on purpose; payload in banks keyed
by sample index; every bank written and read only in a clock-only process.

The differential found **three real bugs in RTL that was already lint-clean and
committed** — allocation order vs slot order, the lost-update fault for the
fifth time in this repository, and a single AUX pending register that dropped
the second request and deadlocked. 280 retirements compared, 0 divergence, plus
five directed refusal cases a differential cannot reach.

`wq_overflow_o` turned out to be **structurally unreachable** (16 slots × 3
samples = 48 against WQN 64, and 64 is the smallest power of two above 48 that
the pointer arithmetic allows). Recorded as a proof with an assertion rather
than left as a planned test that could never pass or fail.

### A static check for the whole defect class — `9159cb73`
`tools/quartus/check_ram_inference.py` finds the three constructs that stop an
array becoming memory, validated against the block known to be wrong before
being trusted. **It predicts that cache_pipe's `data_r`/`tag_r` and all eight
of FRAGROB's banks are now clean.** The fits will settle whether it is right.

### Save the Renderer — `97d3b637`, `e814b772`
TilePipe's cursor registered, removing the `column encode -> address -> 256:1
presence` path that makes Early-Z critical; 74 + 12 + 16 checks including the
pixel-CRC path. And `run_shell_fit.ps1 -GpuPeriodNs` so the 105 MHz experiment
can be run at 9.52381 ns instead of deriving an Fmax from a 10 ns placement.

### Still true
`REJECT: adding terrain/Field RTL faster than the texture fit can be closed.`
Two terrain blocks were CONTRACTED this evening (`TERRAIN.SHADE`,
`TERRAIN.NORMALMAP`) and neither got RTL, deliberately.

---

## RECON SWEEP 2026-09-03 — five lanes over the ~85 unread documents

Five agents read disjoint slices of `reports/` in full and wrote digests to
`reports/digests/`. What follows is the cross-lane integration; the digests
carry the detail. Every claim below was verified against the tree before being
written here.

### The senior document was not the one being followed

`reports/OWNER-RULINGS-BUILDABILITY-20260902.md` (09-02 21:44) is the senior
ruling set of its group, and it contains a section **"THE 8 KM TERRAIN WORLD —
BINDING RULINGS" (T1–T12) that answers all ten open questions** the terrain
architecture document left hanging. **D4 above is therefore two documents out of
date and must be re-swept.** `reports/bandwidth`, cited as a brief, is an empty
directory containing a `.gitkeep` — a phantom citation.

### The root cause of the island regression is ONE construct

`zhao_texture_cache_pipe` reports `blockMemoryBits: 128`; the block it replaces
reports **8,192**. Its reads at `:302-306` are correctly synchronous. Its
**writes** at `:412` and `:416` sit inside the async-reset process opened at
`:309`. **An M10K has no reset port**, so an array written from an
asynchronously-reset process cannot be one — whether or not it appears in the
reset branch.

**This was already diagnosed, measured and written down in the file being
replaced.** `zhao_texture_cache.sv:495-523` records the A/B: making the lane
index static changed nothing (5,402 → 5,373 ALM, zero M10K both times), and the
clock-only process with per-lane flat arrays inferred 4 M10K at 1,087 ALM /
1,737 registers. The rebuild reintroduced the exact defect its predecessor had
documented in a comment.

TEXJOIN has the same disease plus a combinational read at `:332-341` and two
dynamic write addresses into one array (`:390`, `:468`), which the brief's §5.3
forbids by name.

**Why it was reportable as a pass:** `design/fit_targets.yml` carries no
`min_m10k` and no `max_registers` for the block. The brief's tripwires exist
only as prose, so the tooling produced a frequency and nothing checked the
storage law.

**DO FIRST, and it costs no fit and no RTL:** put the resource rules into
`fit_targets.yml`. Then the cache storage port — a coding-style change, not a
rewrite; C0–C4, the replay, the multicast and the counters all stay. Then
FRAGROB banking. Those two alone are ~79% of the ALM and ~75% of the register
recovery, and the full ordered ledger lands on §3.3's nominal 6,600 / 6,050.

### Timing is essentially done; the island is the whole remaining job

`MHZArchitected` and `ShellFixes.md` are substantially complete and do not
conflict: 53.48 → 96.87 MHz, all three ShellFixes items answered and two
improved on the document. **At 96.9 ± 4.6 MHz no block limits the renderer**, so
further local timing surgery has unmeasurable expected value. The texture island
is absent from the composed fit entirely.

### The animation ruling's hardware impact is NOT zero

"No new arithmetic, no new render-time path" holds for the **datapath**. The
**control plane** needs five things, none of which exist:

* **`zhao_geom_pose_cache.sv` has no `sub` in its tag.** The tag is
  `{lru, frame, clip, type}` and `acquire` matches three fields, while the
  reference `zref::creature` carries `uint8_t sub` — the half-key phase. With
  baked 60 Hz data, which the ruling permits for any creature, **a key and its
  midpoint alias and the cache returns the wrong palette.** Mandatory RTL edit.
* **No HPS→VRAM upload path exists.** `CMD.DMA` does "no VRAM writes at all";
  the only block that reads HPS and writes SDRAM is `DEBUG.FRAMEBLIT` — right
  shape (64 B bursts, CRC, guarded writes), wrong block (debug, canvas-length
  locked, holds the framebuffer lease).
* `MEM.HPS.ARBITER` is two-client over a one-burst bridge; the uploader is a
  third.
* `MEM.GUARD` needs an appended region **and** generation rejection.
* `CMD.SCHEDULER` needs a resource-pin table.

Client 6 `TERRAIN_BUILD` is reserved for exactly this — and SUNDER claims it too.

**The two animation documents differ in one place, and it needs an owner
ruling.** On a residency miss, MEMORY §9 says the frame is simply not published
and the previous repeats ("the base architecture requires no such fallback").
RESIDENCY §6.1 requires the HPS to pick a deterministic **per-instance**
degradation before sealing, from a ladder (hold previous pose / bind pose /
splat / glint / omit instance / decline). Whole-frame versus per-instance.
Recommendation: MEMORY's as v1 law — it needs no frame-packet field and matches
what `CMD.SCHEDULER` already does.

### ZEMU belongs in `emulator/`

The directory already exists (root README line 31, a `zemu` CMake target) and
holds two files. `reports/` is a 90-file pile, and CLAUDE.md's own durability
law says durable direction belongs beside what it governs. Recommended:
`emulator/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md`, plus a pointer line in
`design/contracts/SW.ZEMU.md` (ZH-078) and the root README. **Not yet moved —
the owner's placement instruction is still outstanding.**

ZEMU is not a debugger with windows: it is a whole-machine executable oracle
intended to become the place the game is normally developed. "Omniscient" is six
concrete mechanisms, of which the load-bearing one is **component substitution**
— any block swappable between optimised software, ZRef, Verilated RTL and
physical FPGA, which is what makes automatic blame-bisection possible. It keeps
ZRef as semantic authority and never redefines a scalar law. Its own §103 warns
against itself: build observability in response to real active lanes.

### PC/console parity is compiler-enforced, not conventional

Shared truth core plus a semantic command ABI, with every platform difference
confined to the `present` domain and gated by a cross-target conformance suite
on sim-hash-chain and frame-CRC identity. Form's truth/form split type-checks the
rule, so higher resolution, ultrawide and extra effects are **form** and are
divergence-free by construction. Three of the four gates already have contract
text. **Online multiplayer is the one feature that does not fit** — remote pads
as journal entries is the recommendation, and rollback needs a ruling.

### Four things found rotten in the tree

* **the compiler has FORKED** between `zhaozhou/compiler/src` and `nanquan/src`
* no `.zpak` exists anywhere
* `runtime/mister/` is a `.gitkeep`
* `demos/wound_lab/` — frozen decision 18's permanent integration test — is one
  marker file

### Blockers, and two version skews

`REMAINING_BLOCKERS.md` stopped on 08-28 and two ruling sets landed after it, so
its "six blocks blocked on specification" wall is largely demolished:
`GEOM.MESHFETCH`, `GEOM.VDECODE` format 0, `GEOM.LOOM` and `FORGE.PRIM` are
buildable now, and `MEASURE.HISTOGRAM` is closed-as-refused. **25 blockers still
real, 25 fixed or superseded.**

**`QFMT_VERSION` disagrees with itself**, split cleanly along generator lines:
**3** in `zref_tables.hpp`, `tools/fixgen/src/fixp.ts` and
`compiler/src/generated/tables.ts`; **2** in `runtime/include/zhao_abi.h`,
`fpga/rtl/generated/zhao_abi_pkg.sv` and `compiler/src/generated/abi.ts`. R3
ordered the 2→3 bump and the `abi` generator never got it. This is a
capture-visible numeric law disagreeing across the hardware/software boundary.

**`MATERIAL_RECIPE_VERSION = 1` (R9) exists only in prose** — no header, no
package, no table.

### The smallest high-value unbuilt piece is depth

Depth is ruled, generated, proved and oracle'd, and **twelve RTL files consume
`invw24`** — while `zhao_geom_project.sv` emits `out_d_o`, "Q16.16 1/w", and
**no `depth_profile` port exists anywhere in the RTL tree**.
`DEPTH_PROFILE_NEXT_STEPS` steps 5–6 are open and step 5 is described there as
"the only thing that was ever actually blocked". The docket lists steps 1–4 DONE
and is silent on 5–6, so it reads closed. It is not.

### A scheduling conflict that is the owner's to resolve

**Three 09-03 briefs now compete for one Quartus toolchain with no stated
order**: the island recovery (D21), `SaveTheRendered.md` (D23 — it is "Save the
Renderer", 99.50 → 105 MHz), and the production resource count (D19).

### The pattern of the day

Three separate times today, the answer was already written down and was not
read: `QUARTUS_GOTCHAS.md` gotchas 1/4/8 rediscovered with a synthesis probe;
the old cache's own lab note on M10K inference reintroduced as a defect by its
replacement; and the T1–T12 terrain rulings sitting unread while D4 described
those questions as open.

### D25. Normal maps stay; the animation path is viable  ·  `reports/BRO-20260903-NORMALMAP-AND-ANIMATION-PATH.md`  — P1
> Owner, relaying bro, 2026-09-03 late: *"Do you think we can squeeze the normal
> map block in there? Our terrain desperately needs it ... check how viable the
> planned path for animations going to the big RAM is. Right now we don't have a
> connection. Can we make it?"*

**Both answers are yes, with conditions.** Full text in the report; the parts
that change work:

* **Protect the feature, discard the draft.** The detail organ is ~380 ALM /
  2 DSP / 8 M10K -- about **4% of the ~9,000 ALM the texture recovery is already
  targeting**. TERRAIN.SHADE (~730 ALM, 10 DSP) is the terrain's missing
  ORDINARY light and is needed with or without normal maps. *"Normal maps are
  not the thing presently threatening it. The broken texture storage structures
  are."*
* **A Pareto test before accepting 10 DSP:** SHADE is specified at II=1 while
  its producer delivers ~1 triangle per 3 clocks. Fit II=1 AND II=3. **Do not
  cut the 2-DSP normal-map delta first** -- it runs per fragment and needs the
  throughput; the base-light block has the unused parallelism.
* **Order:** repair cache + TEXJOIN storage -> refit the island against its
  tripwires -> put the terrain-light and normal-detail law in ZRef and **look at
  it under a moving sun at 240p** -> fit SHADE at both points -> add NORMALMAP
  separately so its delta cannot be confused with base lighting.
* **The animation route is real, not speculative.** MiSTer's `DDRAM_*` interface
  exposes HPS DDR to the core over the Cyclone V FPGA-to-HPS SDRAM ports;
  `zhao_hps_bridge` already speaks the shape; `DEBUG.FRAMEBLIT` already
  demonstrates burst read -> buffer -> guarded write -> CRC -> atomic publish.
  Six finite pieces remain, listed in the report. **Linux is never in the frame
  loop:** once a bank is resident, playback generates zero HPS traffic.

**TWO CORRECTIONS to `MEM.UPLOAD`, both applied today:**

1. **A generation bit does NOT make an in-place upload atomic.** Writing new
   bytes over a slot that still advertises the old generation lets a consumer
   read a MIXTURE -- so my "old bytes survive CRC failure" guarantee was not
   implementable as written. The upload now lands in a **fresh unpinned
   unpublished slot**, and publication is a MAPPING update.
2. **The source address disagreed and was unguarded.** `hps_addr` was u64 while
   the bridge exports 32 bits; the rule is now `hps_addr[63:32] == 0` or REFUSE
   -- never truncate -- plus an **HPS source-arena guard**, because the first
   version checked the destination only, which is a capability hole.

**Unproven and flagged as unproven:** sustained board bandwidth, and the actual
HPS-DDR capacity on this SuperStation One -- *"the board must be probed before
the software commits to a numerical HPS arena size."*

---

## DONE

| item | commit |
|---|---|
| `RASTER.ATTRSTEP` — exact stepping, 15.1× fewer divides | `01e8ac4` |
| `RASTER.TOON` — cel band, 4.11 clk/fragment | `17cb574` |
| depth profiles derived and proved | `fa5cbc5` |
| first completed composed fit + numbers | `1d229a9` |
| virtual-pin parity check | `823e703` |
| `terrain_project_chain` regression fixed | `4c76318` |
| worst-path export — FRAGMENT named offender #1 | `78aee73` |
| CI format tier + the exit-128 gitlinks | `fdc57ca`, `a9aeb07` |
| cppcheck signed-overflow | `d93bf0b` |
| reel re-pinned; **CI fully green** | `4a436a0` |
| **D1 round 1 — 53.48 → 62.89 MHz** | `c23a5ef`, `6e549ef` |
| **D2 route tripwire consults the lease** | `c23a5ef` |
| D1 round 2 — skid, kept despite regression | `ce84b10`, `43bf8a0` |
| the RMW-loop path analysis + the clock-skew finding | `adeaa52` |
| **all 28 owner questions answered** | `f5d1653` |
| **depth ABI — `SetView.flags[1:0]`** (D10 step 3) | `ca7b328` |
| D10 steps 1, 2, 4 — generated table, spec §8, oracle | `4a436a0`, `fea3b3e`, `cc10167` |
| `GEOM.MESHFETCH` contract | `16e8f44`, `eade724` |
| `GEOM.VDECODE` contract | `d366654` |
| `GEOM.LOOM` contract | `a4309b9` |
| WARP / SNAC / HISTOGRAM closed by decision | `699daf3` |
| `TERRAIN.PATCH` + `GEOM.WCACHE` → UNIT_VERIFIED | `2728467` |
| mana territory recorded, rescued to Upheaval main | `450acc4`, `2ad25aa` |
| active-v9 lane unblocked | `7c646b0` |
| `TERRAIN.MIPGEN`, `TERRAIN.RESIDENCY` v2 | 2026-09-03 |
| `PART.RECORD`, `PART.LADDER` | 2026-09-03 |
| `GEOM.VDECODE`, `GEOM.PARAMBUF` | 2026-09-03 |
| `POST.GATHER`, `TWOD.SPRITE`, `TWOD.PLANE` | 2026-09-03 |
| `FORGE.PRIM` — six families, oracle written | `c3dcd49e` |
| texture island fit answered; `cache_pipe` keeps its M10Ks | `62467567` |
| ten blocks made synthesisable by Quartus 17.0.2 | `62467567` |
| production manifest + resource top | `0e8b1c9d` |

## THE MHz WORK NEEDS **TWO** OF BRO'S PLANS, NOT ONE

Found 2026-09-01 by reading `reports/` properly instead of working from one
document. **`reports/ShellFixes.md` is a second, separate timing-closure plan**
— "gimme your elaborate and full expert solution at fixing the shell MHz" — and
it had never been read during this effort.

It measures the SHELL, from the earlier shell-only fit at 83.4 MHz:

| path | slack at 100 MHz | ceiling |
|---|---|---|
| raw starvation-counter CDC | −1.991 ns | misleading 83.4 MHz |
| **CMD.DMA header-validation** | **−0.875 ns** | **~92 MHz** |
| **record-framer wide-write** | **−0.765 ns** | **~93 MHz** |

Its prescription: one CDC repair, one "nearly trivial" CMD.DMA dependency cut,
and one proper rewrite of the record framer as streaming hardware rather than
one giant expression. Plus process rules — refit before touching the next
candidate, keep fitter settings boring, **no fake timing fixes**, and an
acceptance bar higher than WNS = +0.001 ns.

**Checked against the current composed fit: NONE of those three appear in the
worst 100.** `starve_samp`, `starvation`, `cdc_err`, the DMA and the framer are
all absent; the renderer owns every failing path.

### CORRECTION, same day: all three were ALREADY DONE

The paragraph above originally said this was "required work, not optional". That
was written before checking the RTL, and it was wrong. Reading the source:

| item | status |
|---|---|
| 1 starvation-counter CDC | **done** — rewritten as a snapshot mailbox |
| 2 CMD.DMA header → `crc_pay_r` | **done** — in `M_SEED_PREP`, NOT bro's suggested `M_HCRC` |
| 3 record-framer wide-write | **partly** — `pkt_len − 4` hoisted to a registered copy; the streaming-parser rewrite is NOT done |

**Two of them improved on the document rather than following it**, and both
recorded why in the source:

* **Item 2's suggested home measured worse.** Seeding at the end of `M_HCRC`
  took −0.423 → −0.621 ns and 16 → 60 failing endpoints, because that state runs
  `crc_hdr_r <= fold_o` and seeding there put the write in the CRC fold's
  shadow — trading a ladder for a fold. `M_SEED_PREP` was chosen on the
  measurement.
* **Item 1's justification is stronger than the document's.** The crossing's
  hold slack read **−0.952 / +0.254 / +0.259 / −0.728 across four fits that
  touched nothing in that path** — 1.2 ns of swing on placement alone, making
  the shell's verdict NONDETERMINISTIC. That is worse than permanently red: a
  real regression arriving on a lucky fit is indistinguishable from luck. It had
  already hidden the true −0.875 ns worst path behind its −1.991.

**So the shell is in better shape than the document implies.** What remains open
is item 3's full streaming-parser rewrite, and whether it is needed at all
depends on where the shell lands once the renderer stops dominating — which is
not measurable until the renderer does.

The lesson for this docket: **check the RTL before recording a document's items
as outstanding.** A plan written against an older fit may already have been
answered, and in this case answered better than it asked.

---

## THE AGREED SEQUENCE (Fabian, 2026-09-01)

> *"After we finish bro's instructions and finish the 100 MHz target, finishing
> the conventional renderer's timing closure, we should solve the external
> parameter/binner capacity architecture against real 256-creature traces, that
> comes before more hardware."*

Same order the ruling itself sets. Written here because a sequence agreed in
conversation is not a sequence anyone can find later.

| # | | gate to the next |
|---|---|---|
| 1 | **finish `MHZArchitected` + close the conventional renderer's timing** | a composed fit at the note's 110–115 MHz target, not just 100 |
| 2 | **`GEOM.PARAMBUF` + binner capacity, against REAL 256-creature traces** | the traces exist and the capacity is derived from them |
| 3 | more hardware — the 8 km world, creature lane, spectacle | — |

**Step 2 is blocked on a trace, not on a design.** The ruling is explicit that
the external parameter buffer and tile-reference storage *"must be sized from
real traces of this content tier"* — 256 creatures, 128 per player in Duo,
32,768 particles, one Level-9 spectacle.

**The analytic numbers already computed are NOT that trace** and must not be
mistaken for it:

* `GEOM.VDECODE` ~494,000 vertices ≈ 37 % of a frame, ~15.8 MB/frame at full mesh;
* `PART.STATE` 1 MiB/tick ≈ 63 MB/s at 32,768 particles;
* `POST.COMPOSITE` five full-screen passes ≈ 35 % of a frame.

Those are arithmetic against stated capacities. They say what to *watch*; they
do not say what a real frame *does*, and the binner's own study
(`BINNER_CAPACITY_FOR_8KM_MAPS.md`) exists precisely because the shipped
capacities — 128 triangles, 1,024 references — are two orders short of a game
frame and nobody should discover that during an integration.

**Note also what the current fit does NOT contain:** TMU v2, texture cache,
TEXJOIN, AUX, Field/Earth. 45 sources in the QSF, zero for any of them. So a
composed number today is the frequency of *part* of the machine, which is why
the note's target is 110–115 rather than 100.

---

## Still open after the ruling

| | item |
|---|---|
| **D1** | the Fragment RMW split + address CAM; then the clock-enable fanout |
| **D4** | the 8 km world — pager, residency, cache, command pipeline |
| **D5/D6** | creature presentation lane; cape bones; per-instance pose overrides |
| **D7/D8** | Sunder; the tornado (explicitly after D1) |
| **D9** | PC/console parallel dev, ZEMU, the language |
| **D11** | `GEOM.PARAMBUF` — now sizeable, the ruling gives 256 creatures |
| **D14** | `TILESTORE.INK` + `POST.INK` |
| — | contracts for the ten behaviour blocks (semantics now decided) |
| — | RTL for the three blocks whose contracts are written |
| worst-path export — FRAGMENT named as offender #1 | `78aee73` |
| CI format tier + the exit-128 gitlinks | `fdc57ca` |
| mana-territory design recorded (Upheaval) | `450acc4` |

---

## PRIORITY CORRECTION 2026-09-02 (owner): fit the texture island first

Owner, verbatim: *"the important bit is actually fitting all the texture stuff
to see if the 99.5 MHz renderer and full fitted console actually holds up or if
it needs more reingeneering. Keep your eyes on the prize. But work on terrain
when you have time in between shell stuff."*

**This is a correction and it is right.** Ten texture-island blocks were built,
lint-clean and functionally verified against shipped oracles — and **not one of
them has been fitted.** Their timing is entirely unmeasured. Everything claimed
about them so far is throughput and exactness, never Fmax.

That matters because the brief sets standalone targets they may not meet:

    perspuv / rcp24 island        120 MHz min, 140-150 desirable
    TMU / cache / AUX island      120-125 MHz min
    texture cache                 125 MHz
    full composition              105 acceptance floor, 110 objective

If those do not close, the island needs re-architecting and any terrain work
done first is spent on a machine that is about to change shape.

### The order, and why

Fits are SERIAL on this machine (one Quartus at a time) and cost ~1.5 h each
including the clean-HEAD snapshot. Ten blocks at three seeds is not affordable,
so the five substantial blocks are fitted first, one seed each, to find a
disaster early:

    1. zhao_texture_tmu_plan      five stages of mode decode, wrap, addressing
    2. zhao_texture_cache_pipe    tag compare across four lanes + fill FSM
    3. zhao_raster_rcp24_svc      a 32x64 multiply and eight contexts
    4. zhao_raster_perspuv_svc    a VARIABLE shift, which is the expensive part
    5. zhao_raster_texjoin_v2     16 entries x 3 samples of storage

The small ones — aux_div6, bilerp_lane, rsp_dispatch, palette_res, aux_pipe —
follow only if the big five behave.

### Gap work rule, adopted

Terrain work between fits must **not add unfitted RTL faster than it can be
measured**. Building more hardware while ten blocks sit unmeasured is
accumulating exactly the risk the owner just named. So gap work prefers:
tests against RTL that already exists, architecture documents, and rulings —
things that reduce uncertainty rather than add to it.
