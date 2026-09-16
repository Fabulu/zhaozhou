# Resource rescue roadmap — current state

Date: 2026-09-13

## Decision

The controlling implementation order is the memory-first rescue programme in
`ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt`, reconciled with the
texture packet plan in `reports/V3-REARCHITECTURE-ROADMAP.md`, the live
corrections in `reports/DOCKET.md`, and the newer 30,000-ALM / 85-DSP closure
rule in `reports/RESOURCE-CLOSURE-TARGET-20260912.md`.

The current work is not a new ALM campaign beginning at the 11,263-ALM shell
row. Earlier work had already taken an approximately 76K-ALM per-block census
toward a corrected 58,359-ALM selected mixed-evidence bill, then landed many
functional and local physical reductions without producing another connected
whole-machine fit. The new shell row prices only the protected legacy backend
subtree.

No clean connected whole-machine receipt exists. No current design may be
called below either closure target.

## Authority and vocabulary

For every item, keep these states separate:

- **built** — RTL exists;
- **installed** — the RTL is connected in a functional parent;
- **proven** — committed functional/formal evidence and fired controls exist;
- **mapped** — applicable current MapOnly structure exists;
- **fitted** — applicable current placement/resource/timing evidence exists;
- **adopted** — the selected connected production composition uses it and the
  replaced structure is absent;
- **banked** — a clean applicable physical receipt supports the claimed saving.

`fpga/rtl/prod/zhao_prod_top.sv` is a disconnected resource census harness. An
instance there is not functional installation or production adoption.

## Historical ALM and fit reconciliation

| evidence | result | permitted conclusion |
|---|---:|---|
| 2026-09-04 per-block ledger | 76,672 ALMs across 59 rows | approximately the owner's remembered 80K; upper-bound sum with stale rows, not one machine |
| 2026-09-09 A0 selected bill | 58,359 ALMs / 192 DSP / 147 M10K | corrected partial mixed-evidence selection; 34 DSP-unpriced and 45 ALM-unpriced roots |
| cache memory rescue | 5,903 -> 1,633 ALMs | genuine local fitted reduction; storage moved into M10K |
| texture P0-C Stage A | 13,615 -> 11,562 ALMs | genuine historical composed local reduction |
| texture Gate 2 `@g2-prod` | 15,483 -> 10,837 ALMs | genuine combined fit of packets 1-3 plus laboratory removal; still failed old redlines |
| PERSPUV matched leaves | 1,886 -> 794 ALMs | genuine matched leaf result; later installed, no post-adoption island fit |
| RCP matched NCTX12 leaves | 1,802 -> 986 ALMs; 6 -> 3 DSP | genuine matched leaf result; later installed, no post-adoption island fit |
| later pose/cull/bake/MATW/shared projection | structural/functional reductions | no current physical ALM endpoint |
| TEXJOIN census retirement | old row 3,824 ALMs | accounting correction only; it was not connected silicon |
| D3 legacy shell | whole wrapper 15,046; `u_shell` 11,263.4 ALMs | clean-source physical backend characterization; no texture V3, terrain, missing roots, or framework |

The local fitted reductions above overlap different specimens and cannot be
summed or subtracted from 58,359. There is no defensible post-58,359
whole-machine ALM number.

> **2026-09-16 — G8A's timing gate is CLOSED at the operating requirement.**
> `zhao_raster_texture_v3_fit_top@g8a-timing5`, clean commit `fd78352c`,
> `status: ok`: **108.37 MHz, setup TNS 0, WNS +0.772 ns**, 13,076 ALMs, 30 DSP,
> 71 RAM blocks, 92,964 memory bits, zero negative paths of 2,000. The
> progression across three fits was 90.96 → 94.46 → 108.37 MHz and
> −131.275 → −0.721 → 0 ns of setup TNS.
>
> What that does and does not settle:
>
> * **Does:** step 2's `G8A` gate. The subsystem meets 100 MHz with a real
>   reserve, on a clean committed tree, with DSP/RAM/memory-bit counts unchanged
>   throughout the timing campaign.
> * **Does not:** anything whole-machine. 13,076 ALMs is a SUBSYSTEM figure and
>   the owner's closure criterion — comfortably under 30,000 ALMs and 85 DSPs —
>   is whole-machine, answerable only by G8C/production composition.
> * **Does not:** COMFORTABLE. That needs 110 MHz with zero TNS; 108.37 is
>   1.63 MHz short, and the band now holds **12 paths** (6 island, 5
>   `zhao_raster_earlyz`, 1 `v3own`) against the 333 it held one fit earlier.
>
> Step 2 therefore advances to **lease/CDC and sibling shell V2**, then
> parameter-fixed terrain G8B, then combined G8C. Details and the per-family
> dispositions are in `G8A-TIMING4-DISPOSITION-20260916.md`.

> **2026-09-16, later — G8B IS MEASURED, AND IT IS THE MACHINE'S REAL TIMING
> PROBLEM.** Packet I built the parameter-fixed wrapper the target needed and
> `zhao_terrain_pipe_rpp3_matw18_fit_top@g8b` fitted from clean `968243b5`:
> **43.94 MHz**, setup WNS **−12.758 ns**, setup TNS **−7,360 ns**, and **all
> 2,000 exported paths negative**. 7,424 ALMs, 34 DSPs, 44 RAM blocks.
>
> Every resource gate passes. Only the clock fails, by 56 MHz.
>
> This reorders the remaining work. G8A's worst state was 90.96 MHz with 497
> slow paths; G8B is at 43.94 with every path slow, and had never been timed at
> all. The whole result traces to one combinational chain in
> `zhao_terrain_tess`: from the registered lattice response through two
> subtracts, a 17×34 multiply, two rescales and two saturating adds before
> anything is registered — 22.5 ns of data delay against 0.1 ns of skew.
>
> **G8C cannot be usefully attempted before this is cut.** Composing a 43.94 MHz
> subsystem with a 108.37 MHz one and fitting the pair would measure the terrain
> pipe's chain a second time and learn nothing new.
>
> **And it is not one chain.** Worst path per endpoint block says what each fix
> would actually buy: `zhao_terrain_tess` caps at 43.9 MHz, `zhao_project_core`
> at **50.5 MHz** behind it with 1,631 of the 2,000 negative endpoints, and the
> inferred RAMs at about 65 MHz behind that. Cutting the tessellator chain
> perfectly moves this subsystem to roughly 50 MHz and no further.
>
> G8A reached 108.37 MHz from −131 ns of TNS across eleven work packages and
> three fits. G8B starts from **−7,360 ns** with three ceilings stacked in
> series, so **it needs its own campaign, scoped and batched like Timing4** —
> not a patch and a re-fit. `zhao_project_core` is shared with the geometry
> client, so that part is not terrain-only work.
>
> Full measurement, per-block ceilings and diagnosis:
> `G8B-TERRAIN-FIRST-MEASUREMENT-20260916.md`.

## The live whole-machine position, 2026-09-16

Not a new census: this is `tools/budget/domain_scoreboard.py` run today, and it
is here because a roadmap that records only historical reconciliations leaves
the reader comparing a current file to an old measurement.

```
TOTAL   40,591.4 ALM / 36,000 objective      173 DSP / 88      94.0 M10K / 464
DEVICE  41,910                               112              553
```

### "FLOOR" IS WITHDRAWN. This is PARTIAL MIXED EVIDENCE.

*Corrected 2026-09-16, same day, by
`reports/Zhaozhou_DSP_Uncashed_Savings_Audit_2026-09-16.txt`.*

When this section was first written it said 40,591 was "a FLOOR and the true
number is higher", reasoning that 34 unpriced blocks contribute 0. **That is
wrong in one direction and right in the other at the same time, which is why it
is not a bound at all:** missing functions undercount, and *stale receipts for
blocks that have since been rewritten overcount*. The census source says so
itself. Neither figure is a lower or an upper bound on an optimised connected
console, and calling one a floor invites exactly the arithmetic the audit
forbids.

It is also not "fitted rows only" for DSP. That phrase describes how the ALM
column is built; **map-only evidence participates in the DSP total.**

**What the 173 actually is:** sixteen non-zero charges, several of them prices
for hardware the repository has already replaced. It still charges TWO separate
33-DSP projectors; it still carries the old 18-DSP pose and 15-DSP cull
measurements against RTL whose current defaults are one lane each and two lanes
respectively; it still carries the original 17-DSP bake while
`zhao_terrain_bake_v2` exists. It does not incorporate G8A or G8B as the
production composition, and texture deliberately selects `@packet-b-prod`, which
has no applicable receipt and so contributes nothing.

**Four large replacements already in the tree** cover 72–75 DSP of
historical-to-candidate difference — the shared G8B projection group (72 → 34,
physically fitted), pose (18 → 4), cull (15 → 6) and bake (17 → 3–6), the last
three structural and unadopted. Restoring the omitted raster/texture scope
(−9 old raster subtree, +30 G8A) gives an **incomplete, mixed-evidence planning
subtotal of roughly 119–122** — not a bound, not a forecast, and not a
completed-console claim. A further documented packing portfolio is worth roughly
another 30 under its stated baselines, largely unimplemented. Complete FIELD and
the other missing functions enter as POSITIVE costs.

The audit is the authority on all of this and lists what must not be
double-counted; read it before quoting any DSP number.

**What still stands, and is the point of R1–R9:** five of eight domains are
**OVER** their section-2 allocation, "Projection and result arenas" worst at
12,267 against 4,500, and the machine is a long way from comfortably under
30,000 ALM and 85 DSP on any reading. That gap dwarfs the G8B timing campaign:
G8B's whole subsystem is 7,807 ALM and the entire T1/T1b package cost +383 of
them to buy +29.65 MHz.

**Both things are true and neither substitutes for the other.** 100 MHz is a
closure requirement and the terrain pipe was at 43.94; that had to be fixed and
now reads 73.59. But no amount of timing work moves the area bill, and the area
bill is the larger breach.

## Current R0-R9 completion matrix

| stage | current state | completion verdict |
|---|---|---|
| **R0 — freeze accepted texture** | V3 contains RCP12, paired PERSPUV, owner and read-late combiner; Packet A seam types landed. **Packets B–H are landed and green — 199/199 across `packet-b` (88), `packet-c` (4), `packet-d` (13), `packet-e` (26), `packet-f` (14), `packet-g` (22), `packet-h` (45), measured 2026-09-16 — and G8A's timing gate is closed at 108.37 MHz with zero setup TNS (23/23 on `g8a`).** V3 is still a separate selected subsystem, not connected to the shell: there is no post-adoption composed fit and no current Checkpoint C. **Packets I/G8B, J/G8C and K have no tests at all** (`packet-i`, `packet-j`, `packet-k`, `g8b`, `g8c` each return 0), so they are not started rather than in progress. | **Open — B–H done, I/J/K absent** |
| **R1 — reusable ROM/hybrid arithmetic** | Embedded terrain quarter-square and dual-18 calibration exist. Reusable quarter-square primitive, 26+6 full-width hybrid, coefficient-table primitive, and the promised MapOnly bundle do not. Dual-18 proves narrow mapped ownership/routes only. | **Open; partial prerequisite work** |
| **R2 — arena/replay identity and trace** | Non-power-of-two address repair, dense seal, three-copy terrain replay, 106-bit `w` carriage, ModeVtx/ModeRef, 81-entry topology proof, and tested lifecycle exist. Exact 2x1089/3x17 gates, configuration/deformation epochs, selected RAM mapping, and real workload trace remain open. | **Most advanced stage; substantially built, not accepted** |
| **R3 — shared projection service** | One service/core is composed with the terrain candidate and functionally proven. Real geometry client, NORMALS, DEPTHQUANT/canonical depth, selected nine-DSP backend, current map/fit, and atomic adoption remain open. | **Candidate built, unfitted and unadopted** |
| **R4 — colour/fog memory** | Hard arithmetic baselines exist for material combine, bilerp, pixel fog, and vertex fog. The promised quarter-square/coefficient-memory replacements are absent. Fog is not connected end to end. | **Open** |
| **R5 — pose/skin/normals** | Pose advanced structurally from the stale 18-DSP map toward a four-DSP schedule; 4->3 temporal sharing is only analysis. Legal skin baseline is 9 DSP; one-lane is throughput-illegal and no two-lane parameter exists. ROM normal transform, one-DSP square farm, root bank, and shared palette owner are absent. | **Open; strong baselines, endpoint unbuilt** |
| **R6 — cull/attributes** | Two-lane cull is built and functionally proven at a structural six-DSP target, but unfitted and its workload row remains disputed. ATTRSETUP/ATTRINTERP/ATTRSTEP baselines exist; coefficient memories, final tie law, mapping, composition, and adoption do not. | **Open** |
| **R7 — terrain maintenance** | Major world/load organs, TESS replay modes, shared normals, current LOD, bake-v2 pieces, shade/detail leaves, and shared projection pipe exist with substantial functional evidence. Tagged TESS timing rebuild, NORMALS/DEPTHQUANT pipe integration, zero-DSP LOD, complete separable bake/DDA, world-to-draw composition, current fits, and adoption remain. | **Substantially built in pieces, not accepted** |
| **R8 — lighting and remaining functions** | Shared scalar shade core, fog leaves, some particle/Forge leaves, and FIELD providers exist. GEOM.LIGHT RGB/multi-light shell, emission carriage, particle STATE/UPDATE/COLLIDE/SPAWN, FORGE.SHADOW, selected FIELD executor, provider calendars, and physical prices remain absent. | **Open; several mandatory organs unbuilt/unpriced** |
| **R9 — selected-console closure** | Manifest/accounting infrastructure and legacy shell exist. `zhao_shell_top_v2`, G8A/G8B/G8C, connected V3/terrain/geometry selection, real board/framework, PLLs/pins/SDRAM integration, and a clean margin receipt do not. | **Open; final closure absent** |

### Completion estimate

These percentages are planning estimates, not evidence or budget arithmetic:

| view | estimate |
|---|---:|
| problem discovery and architecture | 65-75% |
| functional candidate implementation | 45-55% |
| mapped/fitted/adopted rescue endpoints | 20-30% |
| R0-R9 stages with every acceptance question closed | 0 of 10 |
| final connected `<30,000 ALM / <85 DSP` proof | 0% demonstrated |

If one number is unavoidable, the ordered R0-R9 delivery programme is roughly
**40% complete**. A 60-65% estimate describes understanding and candidate work,
not installed physical completion.

## Correct immediate order

1. Close the already-paid D3 shell-characterization receipt. Preserve it as a
   legacy backend measurement; do not substitute it for a connected machine.
2. Finish **R0** through the accepted A-K texture/shell plan:
   Packet B, C, D, E, then G8A; lease/CDC and sibling shell V2; parameter-fixed
   terrain G8B; combined G8C; only then Packet K adoption.
3. Finish actual **R1** primitives/maps. Dual-18 is one useful calibration, not
   the whole stage.
4. Close **R2** identity/epoch/trace/RAM gates, then choose the arena profile.
5. Close **R3** geometry/NORMALS/DEPTHQUANT/backend gaps and fit one real-neighbour
   shared projection subsystem before adoption.
6. Execute R4-R8 in order, reusing already-built organs and avoiding a clean-sheet
   rewrite. Price every external provider and mandatory absent function.
7. Enter R9 only after the complete selected map has no unknown required DSP cost;
   then add board/framework logic and close resource, timing, CDC, bandwidth, and
   workload margins together.

## Current optimization implications

- Edgewalk is a valid contained backend ALM candidate at 1,997.4 inclusive ALMs,
  but it does not displace R0-R3. Its factored-row proposal remains unimplemented
  and unmeasured.
- The new shell fit's binner path is real for that placement, but an earlier shell
  epoch reached 100 MHz without the binner rewrite. Do not act on the Astra
  biased-accumulator proposal until the corrected wrapper measurement confirms the
  path survives and the exact cycle miter fires on its mutants.
- Projector 24->11 remains a full structural target. The first safe production
  step is row-family 24->15; viewport 15->11 is separate. Signed/mixed vendor
  arithmetic and placement remain HOLD.
- GEOM.LOD is signed 33x32, structurally 6->3; later dual-18 overlap is only 3->2.
- Pose 4->3 is plausible only with zero added cycles.
- Material quarter-square 2->0 and material dual-18 2->1 are mutually exclusive
  alternatives and cannot both be counted.
- The one-lane skin point is struck permanently unless the workload law changes.

## Closure statement

The project has already completed extensive expensive fits and substantial rescue
work. The remaining task is not to rediscover the machine or build another
measurement framework. It is to finish the ordered compositions, migrate the
selected replacements, and spend fits only at the named boundaries that convert
candidate work into current physical evidence.
