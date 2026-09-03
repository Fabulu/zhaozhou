# THE DOCKET — every outstanding instruction, in one place

Owner instructions arrive as files in `reports/` and are easy to lose between
passes. **This is the index.** Before starting any wave, read this file, then
read the documents it names.

Last swept: **2026-09-03** — see *SWEEP 2026-09-03* below for what landed since.
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
**Measured:** `gpu_clk` 53.48 MHz against 100, TNS −6,566 ns
(`reports/composed/renderer-f8c2b32-.../RESULT.md`).

Owner's diagnosis: first-composition timing debt, not existential. Five textbook
18 ns structures named. Execution order is **his**, and it is deliberate —
measure, then cheap broad fixes, then the deep ones, **fitting each step rather
than batching**:

1. registered fit top; export the worst 100 setup paths; keep today's fit as the *before*
2. Early-Z full detection + Edgewalk registered steps / balanced popcount *(low risk, broad TNS)*
3. streamed Edgewalk row + cross pipelines
4. Fragment → read/shade/blend/finish/commit, with a small in-flight address CAM
5. Binner setup sequenced onto two DSPs
6. FBWRITE fixed 32-byte rows, precomputed addresses, ENGINE0-specialised guard
7. shell diagnostic reductions registered; **ENGINE0 route tripwire fixed**
8. targets: ~120 MHz isolated blocks, 110–115 MHz composed
9. **only then** add TMU v2 + cache + TEXJOIN + AUX + Field/Earth to the fit

**Architecture rule adopted:** *latency may grow; initiation rate and exact
arithmetic may not regress.*

### D1 progress, measured — and the note's ranking has been wrong three times

| round | change | `gpu_clk` | worst path found |
|---|---|---|---|
| 0 | — | 53.48 MHz | **all 400** in `RASTER.FRAGMENT` |
| 1 | modulation off the critical path (`c23a5ef`) | **62.89 MHz** | 90 of 100 in `RASTER.EARLYZ` |
| 2 | Early-Z ready-path skid (`ce84b10`) | 60.92 MHz | the **RMW loop**, ending at the tile store |

**Round 2 regressed and the skid was KEPT anyway** — reverting restores 62.89
*and* restores Early-Z as the ceiling, so it is prepaid work. Full reasoning in
`reports/composed/renderer-49ad539-.../RESULT.md`.

**The next surgery is quantified.** Reading the path detail (not the block
ranking) shows `RAM read -> blend -> DSP multiply (3.785 ns) -> carry chain ->
RAM write` in ONE cycle. The data path must fall **14.361 -> under 7.95 ns**,
i.e. be halved. That is `MHZArchitected` step 4 — Fragment split into
read/shade/blend/finish/commit with an in-flight address CAM.

**A sixth offender, not on the note's list at all:** `gpu_clk~CLKENA0` drives
**13,682 fanout** with 1.995 ns of launch/latch skew. No datapath pipelining
recovers skew. Measure it separately before assuming the gap to 100 MHz is all
logic. One multiplier layer per stage; register multiplier
outputs before fabric adders; no wide global reduction driving a large register
bank; no dynamic 64-bit barrel op on a hot path with a fixed protocol; no false
paths, no multicycle constraints, **no seed fishing**.

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
| c | `cppcheck_check` — signed-overflow finding in `render_pipe_directed.cpp` | in progress |
| d | `reel_sequence_crc` — `zhao-reel --check` fails | reproducing |

Note (b) is why the *passing* jobs also printed a red git warning; it is not
cosmetic, it is a committed mistake.

**cppcheck is not pinned as a devDependency** the way clang-format is, so the
local gate can silently skip. That is exactly the failure the standing memory
*"local gates must match CI"* records. Pin it — separate small item.

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

**A specification inconsistency to repair before lighting RTL freezes:** the old
law transformed each normal by each bone and blended the scalar Lambert results.
The live reference blends the *normal vector*, renormalises, **then** takes
Lambert — which removed bright patches at mixed-weight joints. **The live
reference becomes the law.**

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
| island as built | **18,497** | **28,143** | 10 | 25 |
| hard redline | 7,500 | 9,000 | 64 | 14 |
| the prototype it replaces | 15,749 | 25,123 | 11 | 16 |

**The rebuild is worse than the prototype on every axis except DSP.** The
prototype's diagnosis was state in flip-flops instead of memories; the rebuild
took registers from 25,123 to 28,143 with M10Ks from 11 to 10.
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
