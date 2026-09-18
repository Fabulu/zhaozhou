# The missing-organ register — what has a CONTRACT and no RTL

Built 2026-09-18 by cross-referencing every non-software contract in
`design/contracts/` against every module in `fpga/rtl/`, then checking each
candidate by hand, because a name heuristic over-reports and an over-reported
"missing" list sends people to build things that exist.

**106 RTL contracts. Roughly a dozen have no module at all.** That is the real
extent, and it is the answer to "how much of the disaster is not yet even
counted" — because none of these appear in the 96,200-ALM census. They are debits
still to come.

## Genuinely absent — a contract, an envelope, and no RTL

| contract | envelope group | note |
|---|---|---|
| **PART.STATE** | Complete particles, 2,000 ALM | the particle core |
| **PART.UPDATE** | " | " |
| **PART.COLLIDE** | " | " |
| **PART.SPAWN** | " | " |
| **GEOM.LIGHT** | Geometry + lighting, 5,500 | the RGB/multi-light/emission producer R8 names |
| **GEOM.LOOM** | " | |
| **GEOM.WARP** | " | also the missing client-A producer for the shared projector |
| **FORGE.SHADOW** | Terrain/Forge, 4,500 | `forge_cliff`, `forge_jitter_rom`, `forge_prim` exist; shadow does not |
| **POST.COMPOSITE** | Post and 2D, 2,000 | only `zhao_post_gather` exists |
| **POST.ECHO** | " | the golden path charges the selected optional echo here |
| **SYS.PLL** | Backend/platform, 14,000 | the board wrapper |
| **SYS.RESET** | " | " |
| **MEASURE.HISTOGRAM** | Backend/platform | `measure_governor` and `measure_tokens` exist |
| **INPUT.SNAC** | Backend/platform | `input_rumble` and `input_snapshot` exist |
| **MATERIAL.LIQUID** | Backend/platform | `material_combine_v1/v2/v3` exist; liquid is separate |

**What the particle row means concretely:** `zhao_part_expand`,
`zhao_part_ladder`, `zhao_part_record` and `zhao_part_soft` exist. STATE,
UPDATE, COLLIDE and SPAWN do not. That is why the census shows 827 ALUT against
a 2,000 ALM envelope — 0.26× — and why the golden path insists *"its 2,000 is
owed, not banked."* R8 said the same thing from the contract side; the census
said it from the silicon side; they agree.

## Resolved — the heuristic was wrong, these exist

`MEM.SDRAM` → `zhao_sdram_ctrl`. `MEM.ENGINE1.RAWLAST.V2` →
`zhao_engine1_raw_last_v2`. `SYS.CDC` → `zhao_cdc_snapshot`.
`FIELD.SEQ.CORE` → `zhao_field_v2_core`.

## Unresolved — needs a read, not a guess

`MEM.UPLOAD`, `MATERIAL.RESOLVE`, and the `FIELD.SEQ.*` family
(EARTH/FLOW/FORMATION/STAMP/WARP). The FIELD ones are probably PROGRAMS run by
an executor that exists (`zhao_field_alu`, `zhao_field_exec_shared`,
`zhao_field_curve`) rather than modules of their own — but "probably" is not a
disposition, and this register should not pretend otherwise.

## What this does to the number

The census measures **96,200 ALM of what exists**, against a 37,500 portfolio.
Everything in the first table is outside that number. Using the golden path's
own envelopes as the only available estimate for unbuilt work:

```
particles      ~1,500 ALM owed (2,000 envelope, ~500 built)
board wrapper  unpriced -- SYS.PLL and SYS.RESET have no implementation at all
lighting       unpriced -- GEOM.LIGHT/LOOM/WARP
post           unpriced -- POST.COMPOSITE, POST.ECHO
Forge shadow   unpriced
```

**So the honest statement of the disaster is: 2.6× over on the parts that
exist, with at least four groups still to add work to.** The ratio gets worse
before any optimisation makes it better, and that is the fact the plan has to
start from.

## Build order, and the reasoning

Ordered by *how much the absence distorts the total*, not by difficulty:

1. **PART.STATE → UPDATE → COLLIDE → SPAWN.** Largest proportional gap, four
   contracts, and greenfield: nothing to adopt, nothing to supersede, no packet
   registration to re-accept. The cleanest possible place to start.
2. **SYS.PLL and SYS.RESET.** Small, but the board wrapper is explicitly charged
   to Backend/platform and is currently a zero in a group already at 2.2×.
3. **GEOM.WARP.** Doubles as the client-A producer that blocks adopting the
   shared projector — the one unbuilt organ that also unlocks a measured saving.
4. **GEOM.LIGHT, FORGE.SHADOW, POST.COMPOSITE/ECHO, the rest.**

**Not before the baseline lands.** `@whole-console-sizing` is the denominator
for all of it, and adding organs while it runs would mean the first fitted
whole-machine number describes a machine that no longer exists.

---

# CORRECTIONS, same day, after reading the ledger and the oracle

Two things above are wrong, and one of them is a hard stop rather than a
re-ordering.

## 1. SYS.PLL and SYS.RESET are NOT ours to build

The build order above puts them at #2. **Both are `blocked_on: hardware` in
`design/blocks.yml`, and both contracts say so in every single section:**

> "Waiting on the board, not on a decision. Its clocks, reset sequencing and CDC
> constraints are properties of a PHYSICAL DEVICE -- pin assignments, PLL
> capabilities, measured electrical and bandwidth truth -- and every number
> written here before that device exists would be a guess wearing a
> specification's clothes. Owner ruling 2026-08-31 section 8: **"no owner
> action. They wait for the board."**"

Building them now would be authoring the specification the contract explicitly
refuses to author. The contract anticipated exactly this impulse and named it.

**So the board wrapper is unpriced by OWNER DECISION, not by our neglect** — and
that is a materially different fact for the plan. It is not a debit we can
retire by working harder; it is a debit that cannot be sized until the board
exists. Every ALM total we produce before then carries a hole of unknown size
where the platform group's wrapper goes, and the honest way to state the total
is *"X ALM plus an unpriced board wrapper"*, never a single number.

`MEASURE.HISTOGRAM` and `INPUT.SNAC` sit in the same Backend/platform group and
are NOT blocked — those remain buildable.

## 2. The missing organs are missing in BOTH lanes, and that was measured

The register above counted one thing: contracts with no RTL. An independent
count — **which `reference_model:` symbols does `design/blocks.yml` promise that
the reference oracle does not contain?** — lands on the same blocks.

95 blocks declare a reference model. 82 resolve. **11 do not** (two more,
`MEM.HPS.ARBITER` -> `rtl::zhao_hps_bridge` and `TEXTURE.FRAGROB` ->
`rtl::zhao_raster_texjoin_v2`, are `rtl::`-prefixed and were checked by hand:
both modules exist, so they are false alarms of a `reference/`-only search and
are excluded).

```
FORGE.SHADOW        zref::forge::shadow_hull
GEOM.LOOM           zref::TransformLoom
GEOM.WARP           zref::GeomWarp
INPUT.SNAC          zref::SnacAdapter
MEASURE.HISTOGRAM   zref::MeasureHistogram
PART.COLLIDE        zref::ParticleCollide
PART.SPAWN          zref::ParticleSpawn
PART.STATE          zref::ParticleState
PART.UPDATE         zref::ParticleUpdate
POST.COMPOSITE      zref::PostComposite
POST.ECHO           zref::PostEcho
```

**All eleven are rows of the table above. None is outside it.** Two censuses
taken different ways, from different files, agreeing exactly — which is the only
kind of corroboration this campaign has learned to trust.

The consequence is sharper than "more work". A block with no reference model has
**no differential test available**, so its directed test can only check that the
RTL is self-consistent and matches the contract's prose. That is a weaker
instrument than every finished block in this repo enjoys, and it reads the
flattering direction: a self-consistency test passes on a design that is
internally coherent and wrong.

The self-test discipline applies to this count too — the audit refuses to run
unless it can first resolve `zref::render::project_vertex`, a symbol known to
exist, because a search that matches nothing would report "all present".

## 3. But the particle FORMAT is ratified, and PART.STATE nearly missed it

`reference/include/zref/zref_particle.hpp` holds the **particle128 v1 codec
(qformats §10, amendment C2 / ruling R3)** — `Particle128`, `particle_pack`,
`particle_unpack`, with an exact field map (pos 3x18 signed at 0/18/36, vel 3x11
signed at 54/65/76, age 10 at 87, species 7 at 97, size 6 at 104, spin 6 at 110,
flags 4 at 116, variation 8 at 120), a format version, ratified flag bits
(`kPartStuck`, `kPartCollidedThisTick`, `kPartBornThisTick`, and a reserved bit
that is "zero in, preserved zero"), `particle_radius`, `particle_angle16` and the
representation ladder.

`zhao_part_state.sv` was written without citing it and its first directed test
hand-rolled the bit shuffling. The offsets happen to agree; the test's `age`
mask did not. **That is the "second implementation of ratified arithmetic"
failure this repo has now made often enough to have a law about it**, caught
only because the ledger audit above sent someone to open the header.

So the accurate statement for all four particle contracts is: **the record
FORMAT is ratified and must be consumed from the oracle; the BEHAVIOUR has no
oracle at all.**

## 4. The corrected build order

1. **PART.STATE -> UPDATE -> COLLIDE -> SPAWN** (unchanged; largest proportional
   gap, greenfield), every one of them packing through `zref::part`.
2. **GEOM.WARP** — promoted from #3. Doubles as the client-A producer that blocks
   adopting the shared projector, the one unbuilt organ that also unlocks a
   measured saving.
3. **MEASURE.HISTOGRAM, INPUT.SNAC** — the buildable half of Backend/platform.
4. **GEOM.LIGHT, GEOM.LOOM, FORGE.SHADOW, POST.COMPOSITE/ECHO, MATERIAL.LIQUID.**
5. **SYS.PLL, SYS.RESET — NOT SCHEDULED.** They wait for the board, by owner
   ruling. Removing them from the queue is the correction, not a deferral.

---

## CORRECTION TO THE CORRECTION — section 2 above overclaimed

Section 2 presents the reference-model audit as if nobody had ever looked.
**Somebody had.** `reports/PHANTOM_REFERENCES.md`, 2026-09-12, 23 KB, is a
careful hand audit of exactly this question, and it already lists **ten of the
eleven** — every particle phantom, `TransformLoom`, `GeomWarp`, `PostComposite`,
`PostEcho`, `SnacAdapter`, `MeasureHistogram`. It even registers
`zref::ParticleCollide` as its phantom #10.

That was checked only because the PART.COLLIDE agent pushed back on a **separate
error in the same message** — I had told it `design/blocks.yml` declares no
`reference_model` for PART.COLLIDE. It declares one, at line 4431, and my own
tool had already printed it. I had the right output and wrote the wrong sentence.

Both mistakes lean the same way, which is the point: *"nobody has read this"* and
*"the ledger declares nothing here"* are both the comfortable version, and this
file has a chapter on why that version arrives first.

**What survives the correction, and it is not nothing:**

1. The finding is **corroborated, not novel** — and corroboration between a hand
   audit and a mechanical one, six days apart, is worth more than either alone.
2. The **probe** was missing, not the knowledge. That register is prose:
   hand-made, unreproducible, dated. The repo's own law is *commit the probe*,
   and the precedent is `mutant_copy_drift.py`, written after a "REGENERATE IT"
   instruction sat unread through two commits.
3. **The prose had already gone stale, and the tool proves it.** `FORGE.SHADOW`
   -> `zref::forge::shadow_hull` appears **nowhere** in `PHANTOM_REFERENCES.md`.
   One new phantom in six days is a drift rate a hand audit cannot see, and it is
   exactly how ten became eleven without anyone noticing.

So the corrected claim is: *the count was already known; what was missing was
something that would notice the eleventh.* `tools/budget/refmodel_liveness.py`
is that, and its header now says all of this instead of claiming a discovery.

**Still owed:** ctest registration (deferred only because agents are writing
`tests/CMakeLists.txt`), and a cross-link from `PHANTOM_REFERENCES.md` to the
tool so the next reader finds the live count rather than the dated one.

---

# THE DISPOSITION TABLE — what this register should have been from the start

The table at the top of this file lists fifteen rows as "genuinely absent". It
never asked the one question that decides whether an absence is a problem:
**is it absent because nobody got to it, or because somebody decided?**

`design/blocks.yml` answers that mechanically, per block, in fields that were
there the whole time — `deferred`, `cut_order`, `blocked_on`. Nothing read them.
This is the register committing the exact error its own header warns about:
*"a name heuristic over-reports and an over-reported 'missing' list sends people
to build things that exist."*

| contract | disposition | evidence |
|---|---|---|
| PART.STATE | **BUILT** 2026-09-18 | `deferred: false` |
| PART.UPDATE | **BUILT** 2026-09-18 | `deferred: false` |
| PART.COLLIDE | **BUILT** 2026-09-18 | `deferred: false` |
| PART.SPAWN | **BUILT** 2026-09-18 | `deferred: false` |
| GEOM.LIGHT | **BUILDABLE — remaining work** | `deferred: false`, contract written |
| GEOM.LOOM | **BUILDABLE — remaining work** | `deferred: false`, contract written |
| FORGE.SHADOW | **BUILDABLE — remaining work** | `deferred: false`, contract written |
| POST.COMPOSITE | **BUILDABLE — remaining work** | `deferred: false`, contract written |
| GEOM.WARP | **DEFERRED by owner ruling** | `deferred: true`, cut-order 5; ruling 2026-08-31 §6.3; v1 definition line 136; contract "Deliberately unwritten" in all 12 sections |
| POST.ECHO | **DEFERRED by owner ruling** | `deferred: true`, **cut-order 1** |
| MEASURE.HISTOGRAM | **DEFERRED by owner ruling** | `deferred: true` |
| INPUT.SNAC | **DEFERRED by owner ruling** | `deferred: true` |
| SYS.PLL | **WAITS FOR THE BOARD** | `blocked_on: hardware`; ruling 2026-08-31 §8 |
| SYS.RESET | **WAITS FOR THE BOARD** | `blocked_on: hardware`; ruling 2026-08-31 §8 |
| MATERIAL.LIQUID | **NOT A BLOCK — this row was a false positive** | its own contract: *"material law, not a block — no RTL, no ledger row of its own"*, ruled D-7 2026-09-03. Liquids are ordinary triangulated world surfaces through the main renderer. |

**Four remain. Not eight, and never fifteen.**

## How this was found, which matters more than the table

Not by me. A subagent was briefed to BUILD GEOM.WARP, on my claim that it was
"the missing client-A producer that blocks the shared-projector saving". It read
the neighbouring contracts first — as this repo's own law requires and as its
brief instructed — and **refused the assignment**, with five independent
citations. Both halves of my claim were wrong:

* GEOM.WARP is deferred and excluded from v1;
* it is not the client-A producer at all. `GEOM.PROJECT.md` line 67 says
  vertices arrive from GEOM.WCACHE **or** GEOM.WARP — an alternative source, not
  a series stage — and `zhao_geom_proj_lane.sv` already IS the client-A lane.

Building it would have added area to a design ~15× over budget and unlocked
nothing. It built `zhao_geom_group_seq` instead — the producer
`prod_manifest.yml`'s own entry names as *"still absent"* — 41/41 directed,
a committed mutant firing `seal_early_o`, lint positive-controlled.

**The correction that removes work is the most valuable kind**, and this is the
second time today the instruction to read sibling contracts first has paid.

## The ALM consequence, restated honestly

The earlier correction said the total must be stated as *"X ALM plus an unpriced
board wrapper"*. That still holds, but the unpriced remainder is **smaller than
this register implied**: six of the fifteen rows are not v1 debits at all (four
deferred, MATERIAL.LIQUID not a block, and the two board blocks explicitly
outside our reach). The owed work is four contracts, plus wiring in the four
already built.

## And the thing this table does NOT fix

**The four blocks built today are adopted nowhere.** Zero references in
`design/prod_manifest.yml`, `design/fit_targets.yml` and
`fpga/rtl/prod/zhao_prod_top.sv`. `check_prod_manifest.py` returns RC=1 with all
four plus `zhao_geom_group_seq` UNACCOUNTED. They contribute **0 ALM** to any
number until one coordinated declaration wires them in — three separate acts,
per CLAUDE.md, and deliberately held until the baseline fit lands.

---

# OWNER REVOKES THE DEFERRALS — 2026-09-18

> "I don't want to defer any unfinished blocks now" — Fabian, 2026-09-18

The disposition table above is superseded in its middle section. **DEFERRED is
no longer a disposition.** The four blocks cut by owner ruling 2026-08-31 §6.3
are revived; that ruling was the owner's to make and is the owner's to revoke,
and each deferred contract already names the condition — *"Fill this in only if
an evidence-backed revival happens."* This is the revival.

## The queue, split by whether a spec exists

| block | contract | status |
|---|---|---|
| GEOM.LIGHT | 499 lines, written | **build now** |
| GEOM.LOOM | 276 lines, written | **build now** |
| POST.COMPOSITE | 230 lines, written | **build now** |
| MEASURE.HISTOGRAM | 186 lines, written | **build now** (was deferred) |
| FORGE.SHADOW | 142 lines, written | **build now** |
| GEOM.WARP | 71 lines, **"Deliberately unwritten" ×15** | contract must be AUTHORED first |
| INPUT.SNAC | 71 lines, **"Deliberately unwritten" ×15** | contract must be AUTHORED first |
| POST.ECHO | 85 lines, **"Deliberately unwritten" ×9** | contract must be AUTHORED first |

Five have real specifications and can be built against them today. Three were cut
*before* their contracts were written, so their twelve sections all read
"Deliberately unwritten" — building one means authoring its clocks, packet
layouts, throughput and test plan first. That is a different and slower job than
the other five, and pretending otherwise would produce RTL implementing a
specification nobody wrote down.

## The two rows the revocation does NOT reach, and why that is not a deferral

* **SYS.PLL and SYS.RESET** are `blocked_on: hardware`, not `deferred`. The
  obstacle is not a decision to postpone — it is that pin assignments, PLL
  capabilities and measured electrical truth are properties of a board that does
  not exist yet. They can be built the moment it does. Writing them now would
  produce numbers no measurement backs, which is the one thing their contracts
  say in every section.
* **MATERIAL.LIQUID is not a block.** Its own contract: *"material law, not a
  block — no RTL, no ledger row of its own."* There is nothing to un-defer.

Both were already listed above as distinct from deferral; the revocation is
recorded here as reaching exactly the four it reaches.
