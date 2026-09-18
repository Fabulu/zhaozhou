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

> ### UPDATE 2026-09-18 — the whole machine has numbers for the first time
>
> **The sentence above is still true and is no longer the whole story.** There
> is still no fitted whole-machine receipt. There is now a whole-machine
> SYNTHESIS: `zhao_prod_top@whole-machine-map-probe`, `map_only`, 1,180 s, whose
> only previous row in its entire history was `failed:quartus_map.exe` at 59.9 s.
>
> ```
> estimate of ALMs needed   100,709      device  41,910
> total DSP blocks              123      device     112
> registers                 105,818
> block memory bits       1,029,005      device 5,662,720 (18.2%)
> ```
>
> Calibrated against the composed shell's measured map-to-fit error of **+8.6%**
> (31,456 estimated → 28,959 fitted, DSP exact): **≈92,700 ALM, about 2.2× the
> device, and 123 DSP is over the physical part rather than merely over the 85
> objective.** `zhao_prod_top` is exactly the adopted set — 63 adopted tops all
> present, zero `superseded` modules among its 64 children — so this is the
> SELECTED machine, not a composition inflated by duplicates.
>
> **The single largest line item is the duplicated projector:**
> `zhao_terrain_project` + `zhao_geom_project` ≈ 15,600 ALM against a 5,000
> envelope, which is the uncashed cheque CLAUDE.md documents, measured at about
> 2.6× the "~6,000 ALM" it has been quoted as for weeks.
>
> **And its DSP figure corrects the scoreboard in the OTHER direction**, which is
> worth saying because I quoted the old one earlier today. Read out of the same
> hierarchy:
>
> ```
> zhao_geom_project      10,263 ALUT    22 DSP
> zhao_terrain_project   14,289 ALUT    11 DSP
> zhao_geom_wcache        5,132 ALUT     0 DSP   230,868 memory bits
> ```
>
> | | scoreboard, summed per-block rows | whole-machine synthesis |
> |---|---:|---:|
> | projection ALM | 12,267 | ≈15,600 — **worse** |
> | projection DSP | 66 | **33** — half |
>
> The 66 came from two stale rows each asserting 33; the machine actually uses
> 22 and 11. So **deduplicating the projector is worth roughly −7,800 ALM and
> −11 to −22 DSP, not −33** — which would take the machine from 123 DSP to about
> 101–112, i.e. onto or just over the physical part rather than comfortably
> inside the 85 objective. The ALM case for doing it is stronger than recorded
> and the DSP case is weaker, and both were wrong in the same document an hour
> apart because both were sums of expired receipts.
>
> **And the closure authority was unindexed.**
> `reports/Zhaozhou_conditional_golden_path.md` (owner, 2026-09-14, pinned to
> this branch by name) defines closure as a 10% fabric reserve, a 37,719 working
> limit and a 37,500 portfolio across seven groups — *"do not call a 40.5k
> result closure merely because it is below 41,910."* It is now indexed in both
> `DOCKET.md` and `OWNER-DOCUMENT-INDEX.md`. Every one of its seven envelopes is
> currently breached; the total is **2.6×**.
>
> Meanwhile the composed sibling shell went **29,044 ALM / 54.12 MHz → 27,601 /
> 66.03** across three fits in one day, with no uninferred array left in it.
>
> #### Reading order for 2026-09-18, because this file grew by 1,258 lines
>
> The day's sections were INSERTED next to what they correct rather than
> appended, so the file reads by topic and not by clock. That is right for a
> reference and wrong for a narrative, so here is the narrative:
>
> | # | fit / event | result |
> |---|---|---|
> | 1 | `@packet-h-m10k` (before today) | 29,044 ALM · 54.12 MHz · TNS −29,689 |
> | 2 | path DETAIL read properly | the M10K is 0.192 ns; `attrgrad` binds, not the resolver |
> | 3 | `@packet-h-timing` — tree + legality + CRC verdict | 28,959 · **61.52** · −19,985 |
> | 4 | destinations split | 512 of 516 RAM paths were the CRC scan, not the legality cone |
> | 5 | `@packet-h-uvw` — one read register moved | 27,601 · **66.03** · −8,851 · **area bought clock** |
> | 6 | `zhao_prod_top` maps, first time ever | ≈92,700 ALM · 123 DSP — **2.2× the device** |
> | 7 | golden path found unindexed | defines closure; every envelope breached, total 2.6× |
> | 8 | `@packet-h-mulstage` — multiply split | *running at time of writing* |
>
> **Predictions are written before each fit and left standing afterwards**, which
> is why several read as wrong: the `uvw` Fmax prediction said "unchanged" and it
> moved 4.5 MHz. Those are kept deliberately — a prediction edited after the
> measurement is not a prediction.

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
> > **SUPERSEDED 2026-09-18. That cut landed and this blocker is gone.** G8B
> > closed at **102.19 MHz** on physical pins with zero virtual pins, zero total
> > negative slack and a clean tree (`@g8b-t11-pins-s2`, `status: ok`, commit
> > `861816b0`); seeds 2 and 4 close and seed 1 reads 98.90, which the closing
> > commit says rather than quoting the best row.
> >
> > So the argument above — *composing a 43.94 MHz subsystem with a 108.37 MHz
> > one learns nothing* — no longer describes either operand. The terrain pipe
> > is no longer the slow half, and after the 2026-09-18 timing work the
> > composed shell is the one with the open clock question.
> >
> > **This paragraph is left standing rather than deleted** because it is the
> > reason G8C was parked, and a blocker that quietly disappears leaves the next
> > reader unable to tell whether it was solved or forgotten. It was solved.
> > What remains for G8C is not a timing precondition at all: there is no G8C
> > TOP. `zhao_shell_top_v2`'s 97 sources contain no `terrain/` file, so nothing
> > in the tree instantiates both halves, and that module is the actual next
> > piece of work for Packet J.
> >
> > **Both operands exist and are named**, so the module is a composition rather
> > than a build — the same shape Packet H turned out to be:
> >
> > | half | module | how it is already fitted |
> > |---|---|---|
> > | render | `zhao_shell_top_v2` | `- top:` in `fit_targets.yml`, 97 sources |
> > | terrain | `zhao_terrain_pipe` | via `zhao_terrain_pipe_rpp3_matw18_fit_top`, 17 ports, which fixes `ROWS_PER_PASS(3)` and `MATW(18)` |
> >
> > So G8C is: one top instantiating `zhao_shell_top_v2` and `zhao_terrain_pipe`
> > **at G8B's ratified parameters** under one clock, a `- top:` entry whose
> > source list is the union of the two closures, and the gates Packet I got —
> > directed activity witness, synthesis-mode lint, generated freshness, and a
> > registration-static test. The parameters matter: fitting terrain at anything
> > other than RPP3/MATW18 would measure a configuration G8B never closed, and
> > the 102.19 MHz receipt would not describe it.
> >
> > **The open question is what "combined" means, and it decides what the number
> > is worth.** Two tops are possible and they are not the same measurement:
> >
> > * **CO-FITTED** — both instances in one module, ports promoted, nothing
> >   crossing between them. Cheap to write, and it measures the SUM plus
> >   whatever placement pressure they put on each other. It answers "do both
> >   fit on this device at once", which is a real budget question.
> > * **CONNECTED** — terrain's output actually feeding the render path. This is
> >   what R9 means by *"connected V3/terrain/geometry selection"*, and only this
> >   one can expose a cross-subsystem critical path, which is exactly the class
> >   of problem G8A and the composed shell both turned out to have.
> >
> > A co-fitted top that gets reported as "G8C closed" would be the
> > `status: ok` mistake one level up: a true number answering a smaller
> > question than the one asked. Whichever is built, the row's label should say
> > which it is.
> >
> > #### And the co-fitted ALM answer is already computable, which is the point
> >
> > Both halves now have clean `ok` rows on the same device:
> >
> > ```
> > zhao_shell_top_v2   @packet-h-m10k   29,044 ALM   (render path + shell)
> > zhao_terrain_pipe   @g8b-t11-pins-s2  8,295 ALM   (RPP3/MATW18, 102.19 MHz)
> >                                      ----------
> >                                      37,339 ALM
> > ```
> >
> > Against a **41,910-ALM device** that fits, with about 4,600 spare. Against
> > the owner's **30,000-ALM target** it is roughly **7,300 over** — and the
> > shell's 97 sources already contain command, geometry, video, audio and
> > debug, so shell + terrain is most of the machine rather than two fragments
> > of it.
> >
> > That number is **lower than the 40,591 the scoreboard carries**, and the
> > difference is not good news arriving — it is the scoreboard being a sum of
> > per-block rows, several of them stale, against two composed measurements.
> > Treat 37,339 as the better estimate and still an estimate: it is a sum of
> > two fits, which the island's own history says overstates by about 2.4% once
> > things are actually composed.
> >
> > **So the co-fitted G8C fit would mostly confirm arithmetic already available,
> > which is an argument for building the CONNECTED one** and spending the fit
> > on the question that cannot be computed: whether a path crosses between the
> > two subsystems.
> >
> > #### AND A WHOLE-MACHINE TOP ALREADY EXISTS. IT HAS NEVER MAPPED.
> >
> > Found 2026-09-18 while the shell re-fit ran, and it changes what Packet J is.
> > `fpga/rtl/prod/zhao_prod_top.sv` instantiates `zhao_shell_top` **and** seven
> > terrain blocks — `bake`, `island_dir`, `lod`, `mipgen`, `normals`, `patch`,
> > `project` — so the co-fitted composition is not something to invent. It is
> > in `design/fit_targets.yml` with a `- top:` entry and a 147-file closure.
> >
> > Its entire history in `zhao_block_fit.json` is **one row**:
> >
> > ```
> > zhao_prod_top   failed:quartus_map.exe   59.9 s   128 sources   clean tree
> > ```
> >
> > **The whole machine has never been synthesised, let alone fitted.** Every
> > budget number in this document — the 40,591 scoreboard, the 173 DSP, the
> > 37,339 above — is a SUM of per-block or per-subsystem measurements, because
> > the one target that would measure the machine as a machine dies in a minute.
> >
> > Sixty seconds is a fast, loud failure: by this repository's own note that is
> > a syntax or elaboration error, not a capacity wall. It therefore costs a
> > minute to reproduce rather than a day, and it is the cheapest unanswered
> > question on the closure path.
> >
> > **SUPERSEDED 2026-09-18, BOTH SENTENCES ABOVE.** The whole machine HAS been
> > synthesised: `@whole-machine-map-probe` maps in 1,180 s (was
> > `failed:quartus_map.exe` at 59.9 s) and reports ~92,700 ALM and 123 DSP
> > against a 41,910 / 112 device, with the per-group allocation below. The
> > paragraph is kept because the diagnosis in it was right — sixty seconds was
> > an elaboration error, not a capacity wall, and it cost a minute to fix.
> >
> > **AND THE SECOND HALF IS SUPERSEDED TOO, on the same day and more usefully.**
> > "Move it to the V2 shell" turns out to be only half of Packet J. The other
> > half is the shared projector: `zhao_terrain_pipe` → `zhao_proj_subsystem` →
> > `zhao_project_service` → ONE `zhao_project_core` is built, composed AND
> > fitted at 102.19 MHz on physical pins, and `zhao_prod_top` instantiates none
> > of it — it still carries `zhao_geom_project` and `zhao_terrain_project` with
> > a core each, 24,399 ALUT of one circuit built twice. See *THE PROJECTOR
> > CHEQUE IS NOT UNBUILT* at the end of this document, and
> > `uncashed_cheques.py` check 4, which was written to make this class
> > detectable instead of noticeable.
> >
> > So Packet J's real shape is probably not "write a G8C top" but **"make the
> > whole-machine top map, then move it to the V2 shell"** — `zhao_prod_top` is
> > generated by `tools/quartus/gen_prod_top.py`, which is the supported way to
> > change what it composes.
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
| **R0 — freeze accepted texture** | V3 contains RCP12, paired PERSPUV, owner and read-late combiner; Packet A seam types landed. **Packets B–G are landed and green, and Packet H's PREREQUISITES are — 199/199 across `packet-b` (88), `packet-c` (4), `packet-d` (13), `packet-e` (26), `packet-f` (14), `packet-g` (22), `packet-h` (45), measured 2026-09-16 — and G8A's timing gate is closed at 108.37 MHz with zero setup TNS (23/23 on `g8a`). **CORRECTED 2026-09-16: this sentence said "B–H are landed".** **SUPERSEDED 2026-09-17: the sibling shell now exists and its gate clauses are closed.** `fpga/rtl/common/zhao_shell_top_v2.sv` is 2,567 lines, 27 instances, lint-clean and Quartus-map-clean, in the `packet-h` label at 72/72 — with a structural differential against the protected V1 (`packet_h_sibling_diff`), a behavioural one under paired traffic (`shell_paired_diff_directed`, 16,408 cycles, 0 mismatches, its own committed mutant control), a sealed V3 programming channel, a six-term fault OR and a measured reset-barrier re-arm. The paragraph below is kept because it records how the packet was mis-reported, which is the part worth not repeating. What remains owed is the sequence-abort RELEASE control, which is **blocked on Packet J** — the only producer that could fail to emit a terminal is the V3 return path, tied off until then — and a post-adoption composed FIT, which is the R0 sentence still standing. The original text follows.

Packet H owns `fpga/rtl/common/zhao_shell_top_v2.sv`, and at the time this was written that file **did not exist anywhere in the tree** — no source, no generated wrapper, no entry in any manifest or source list. Its 45 green tests cover `zhao_engine1_raw_last_v2`, `zhao_renderer_lease_v2`, `zhao_video_ready_bridge_v2` and `zhao_video_terminal_adapter_v2`: the sibling shell's COMPONENTS, built and verified with their controls, which is real work and is not the packet. The tests never claimed otherwise — the last one is literally named `packet_h_shell_prereqs_registration_static` — so the label was accurate and the prose reading it was not. The R9 row below has been right about this all along (*"`zhao_shell_top_v2` ... [does] not"*), which is the tell: two rows of one table disagreeing, with the disagreement resolved in the direction that made R0 look a packet closer to done.** V3 is still a separate selected subsystem, not connected to the shell: there is no post-adoption composed fit and no current Checkpoint C. **Packet I/G8B is IN PROGRESS; J/G8C and K are not started.** Measured 2026-09-16: `packet-i` 5, `g8b` 4, and `packet-j` / `packet-k` / `g8c` still 0. This sentence previously read *"Packets I/G8B, J/G8C and K have no tests at all ... so they are not started rather than in progress"*, which was true when written and is not now. Packet I has its four gates — directed activity witness, synthesis-mode lint, generated freshness, and `packet_i_g8b_registration_static`, the fourth one the CMake comment had promised and nothing delivered — plus the RPP3/MATW18 pipe test. **It is NOT closed:** G8B's receipt gate needs a clean row at 100 MHz with ZERO virtual pins, and the campaign stands at 97.61 MHz on virtual pins with the physical-pin run still owed. J/G8C is blocked on `zhao_shell_top_v2`, which does not exist — see the Packet-H correction above. **BOTH SENTENCES ARE SUPERSEDED, 2026-09-17/18.** G8B's gate is MET: `@g8b-t11-pins-s2` is `status: ok` at **102.19 MHz, physical pins, zero virtual pins, zero total negative slack, clean tree** (commit `861816b0`); seeds 2 and 4 close, seed 1 reads 98.90 and the closing commit says so rather than quoting the best row. And `zhao_shell_top_v2` exists, is 72/72 on `packet-h`, and is being composed-fitted. **Packet I's only remaining blocker is Packet H's composed fit**, after which I promotes and J/G8C has a hierarchy. **THAT BLOCKER CLEARED 2026-09-18 AND PACKET I IS PROMOTED.** Packet H's composed fit exists twice, both `status: ok` on a clean tree with 97 declared sources: `@packet-h-m10k` (29,044 ALM, 54.12 MHz) and `@packet-h-timing` (28,959 ALM, **61.52 MHz**, TNS −19,984.6, commit `cebed2fe`). Packet I's own five gates are green — directed activity witness, synthesis-mode lint, generated freshness, the RPP3/MATW18 pipe test, and `packet_i_g8b_registration_static`, which carries the G8B RECEIPT LAW and therefore checks clean-tree, zero-virtual-pin and 100 MHz *together* against `@g8b-t11-pins-s2` (102.19 MHz, physical pins, zero TNS). **What Packet I does NOT close, said plainly so the promotion is not read as more than it is:** the composed shell is at 61.52 MHz against a ruled 100, its remaining binder is `attrgrad → attrdiv` at −6.254 with the divider holding more than half of it, and `uvw_m` still accounts for 84% of the negative paths with its fix measured only in the next fit. Packet I's gate was never the machine's clock — that is R9's — and promoting it on its own terms is not a claim about closure. | **I CLOSED 2026-09-18; J/K open** |
| **R1 — reusable ROM/hybrid arithmetic** | Embedded terrain quarter-square and dual-18 calibration exist. Reusable quarter-square primitive, 26+6 full-width hybrid, coefficient-table primitive, and the promised MapOnly bundle do not. Dual-18 proves narrow mapped ownership/routes only. | **Open; partial prerequisite work** |
| **R2 — arena/replay identity and trace** | Non-power-of-two address repair, dense seal, three-copy terrain replay, 106-bit `w` carriage, ModeVtx/ModeRef, 81-entry topology proof, and tested lifecycle exist. Exact 2x1089/3x17 gates, configuration/deformation epochs, selected RAM mapping, and real workload trace remain open. | **Most advanced stage; substantially built, not accepted** |
| **R3 — shared projection service** | One service/core is composed with the terrain candidate and functionally proven. Real geometry client, NORMALS, DEPTHQUANT/canonical depth, selected nine-DSP backend, current map/fit, and atomic adoption remain open. | **Candidate built, unfitted and unadopted** |
| **R4 — colour/fog memory** | Hard arithmetic baselines exist for material combine, bilerp, pixel fog, and vertex fog. The promised quarter-square/coefficient-memory replacements are absent. Fog is not connected end to end. | **Open** |
| **R5 — pose/skin/normals** | Pose advanced structurally from the stale 18-DSP map toward a four-DSP schedule; 4->3 temporal sharing is only analysis. Legal skin baseline is 9 DSP; one-lane is throughput-illegal and no two-lane parameter exists. ROM normal transform, one-DSP square farm, root bank, and shared palette owner are absent. | **Open; strong baselines, endpoint unbuilt** |
| **R6 — cull/attributes** | Two-lane cull is built and functionally proven at a structural six-DSP target, but unfitted and its workload row remains disputed. ATTRSETUP/ATTRINTERP/ATTRSTEP baselines exist; coefficient memories, final tie law, mapping, composition, and adoption do not. | **Open** |
| **R7 — terrain maintenance** | Major world/load organs, TESS replay modes, shared normals, current LOD, bake-v2 pieces, shade/detail leaves, and shared projection pipe exist with substantial functional evidence. Tagged TESS timing rebuild, NORMALS/DEPTHQUANT pipe integration, zero-DSP LOD, complete separable bake/DDA, world-to-draw composition, current fits, and adoption remain. | **Substantially built in pieces, not accepted** |
| **R8 — lighting and remaining functions** | Shared scalar shade core, fog leaves, some particle/Forge leaves, and FIELD providers exist. GEOM.LIGHT RGB/multi-light shell, emission carriage, particle STATE/UPDATE/COLLIDE/SPAWN, FORGE.SHADOW, selected FIELD executor, provider calendars, and physical prices remain absent. | **Open; several mandatory organs unbuilt/unpriced** |
| **R9 — selected-console closure** | Manifest/accounting infrastructure, the legacy shell, **and now `zhao_shell_top_v2` (2026-09-17, `packet-h` 72/72)** exist. G8A is closed at 108.37 MHz; G8B/G8C, connected V3/terrain/geometry selection, real board/framework, PLLs/pins/SDRAM integration, and a clean margin receipt do not. The sibling shell has no composed FIT yet, so its area and Fmax are unmeasured claims — every gate it has passed so far is a Verilator or source-level one. **THAT LAST SENTENCE IS SUPERSEDED 2026-09-18: the sibling shell is MEASURED, three times.** `@packet-h-m10k` (29,044 ALM, 54.12 MHz, TNS −29,689) → `@packet-h-timing` (28,959 ALM, 61.52 MHz, TNS −19,985) → **`@packet-h-uvw` (27,601 ALM, 63 DSP, 136 M10K, 66.03 MHz, TNS −8,851)**, all `status: ok` on clean trees. Across the day: **ALM −1,443, Fmax +22%, TNS −70%**, 2,399 inside the 30,000 budget, and **no uninferred array left anywhere in the composition**. Its area is inside budget and its clock is not — both now numbers rather than claims. G8B is closed at 102.19 MHz on physical pins, and Packet I is promoted. **What R9 still needs, restated against measurement instead of absence:** (1) the composed shell from **66.03 MHz** to 100 — binder is now `tile_pipe → attrgrad` at −5.144, a DSP multiply plus a soft partial-product carry chain, and every remaining move there needs `attrgrad`'s pipeline depth to change in BOTH differential implementations at once; (2) a WHOLE-MACHINE number — **this arrived 2026-09-18**: `zhao_prod_top` maps for the first time (1,180 s, was `failed:quartus_map.exe` at 59.9 s) and its own estimate, calibrated against the shell's +8.6% map-to-fit error, is **~92,700 ALM and 123 DSP against a 41,910 / 112 device**, i.e. **2.2× over on ALM and over the physical part on DSP**, with a per-group allocation showing every envelope breached and `zhao_terrain_project` + `zhao_geom_project` ≈ 15,600 ALM against a 5,000 envelope; (3) connected V3/terrain/geometry selection, i.e. G8C on a CONNECTED top; (4) board/framework, PLLs/pins/SDRAM; (5) a clean margin receipt. **The decisive change is that (2) is no longer absent — it is measured, and it says the selected machine is more than twice the device.** | **Open; shell measured, whole machine never synthesised** |

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

## OWNER DIRECTION 2026-09-16 — spend M10K to buy ALMs

Fabian, 2026-09-16: *"remember we have lot's of m10k memory, ALM's are over
budget 15 times over, so what you can you need to solve with memory"*.

Standing, and it governs every optimisation choice below rather than one
block. The direction is right and the scoreboard above is why: **40,591 ALM
against a 41,910 DEVICE** — 97% of the chip — while Texture, Complete FIELD
and 34 further blocks contribute **zero**. The machine as measured does not
fit at all. M10K sits at **94 / 464 allocated / 553 on the device**.

Three things have to be said with it, because each changes what the direction
means in practice. None of them softens it.

**1. Memory is bounded, not free — and the specific claim below is WITHDRAWN; see "The worst ALM domain is TWO PROJECTORS THAT NO LONGER EXIST" above. The 52 is two stale rows. The general point stands: the envelope is per-domain and finite, and no domain's current memory position can be read off the scoreboard until its stale rows are re-measured.** ~~and the worst ALM domain is already over its
memory.** The 464 is a REPLACEMENT allocation — the roadmap is explicit: *"Do
not add these 464 M10Ks to the historical 147."* And *Projection and result
arenas*, the worst ALM breach at 12,267 against 4,500, is **already over its
48-block M10K allocation at 52**. So the domain that most needs to spend
memory is the one with none of its own left. Re-allocating across domains is
available — Texture holds 96 unspent, FIELD 64, Geometry 64 — but it is a
plan change and belongs to the owner, not to an implementer who needs a few
blocks.

**2. The lever is NOT relocating state, and this is measured rather than
assumed.** `tools/design/check_array_storage.py` was widened on 2026-09-16 to
read expression-valued and package-sourced parameters (unresolvable
declarations 206 → 47), and with that sight it finds **no block carrying a
current fit row that holds 8 Kbit or more of declared array in flip-flops**.
At a 2 Kbit threshold exactly two appear and both are small. The 1.35x ALM
breach is therefore **combinational logic**, not misplaced registers.

What that leaves is the conversion of COMPUTATION to LOOKUP, and the roadmap
already names those primitives and already records them as absent:

* **R1** — *"Reusable quarter-square primitive, 26+6 full-width hybrid,
  coefficient-table primitive, and the promised MapOnly bundle do not [exist]."*
* **R4** — *"The promised quarter-square/coefficient-memory replacements are
  absent."*

A quarter-square multiply is `((a+b)^2 - (a-b)^2) / 4` against a table of
squares in M10K: it removes a DSP **and** the LUT-multiplier logic, and pays
in memory. It is simultaneously the owner's DSP audit lever and this
direction's ALM lever, and it is the single highest-value unbuilt thing in
R1–R9 under both.

**3. A ROM is not free on the timing side.** M10K clock-to-out is roughly
2 ns against a flip-flop's 0.3. On a path that is already the block's cap a
lookup can cost more than the logic it replaces — G8B T4 is a worked example
in the other direction, where the cheap fix was noticing the stride is always
a power of two and the multiply was never needed at all. Check the CONSUMED
side before converting, not just the generating side.

**The order this implies** is unchanged — finish R0 (Packets I/J/K) — but R1
is promoted in importance within itself: the quarter-square and
coefficient-table primitives stop being one bullet of a stage and become the
mechanism the owner has asked for, to be reused by R4, R5, R6 and FIELD
rather than re-derived in each.

### G8B MEETS ITS TIMING CRITERION — 2026-09-17

**`zhao_terrain_pipe_rpp3_matw18_fit_top@g8b-t11-pins-s2`: 102.19 MHz,
physical pins, zero total negative slack, `status: ok`.** The first accepting
row this target has produced, and the first time
`packet_i_g8b_registration_static`'s acceptance branch has run rather than
been skipped.

| row | seed | Fmax | slack | TNS | ALM | status |
|---|---:|---:|---:|---:|---:|---|
| `@g8b-t11-pins` | 1 | 98.90 | −0.111 | −0.175 | 8,293 | failed:structure |
| `@g8b-t11-pins-s2` | 2 | **102.19** | +0.214 | **0** | 8,295 | **ok** |
| `@g8b-t11-pins-s4` | 4 | **101.68** | +0.165 | **0** | 8,274 | **ok** |

Clean at HEAD, zero virtual pins, digest `fda08fed9972` over nine sources at
commit `9f262f71`, no rule violations.

**The seed dependence, stated rather than buried.** Two of three seeds close;
seed 1 misses by 111 picoseconds. The design sits AT its criterion and
placement decides which side a run lands on. That is not the same as closing
on one lucky seed — seeds 2 and 4 both reach zero TNS with positive margin —
and it is also not the *comfortable* margin the final console receipt will
want. It is what G8B's own gate asks for and no more.

**From 43.94 MHz**, and the two negative results are as much of the record as
the positive ones:

| package | changed | measured |
|---|---|---:|
| T3a | registered the window mask | +11.7 |
| T5+T6 | registered the lattice base; split the output cone | +9.7 |
| T7 | moved a constant add into a **DSP's** register | **−7.9**, reverted |
| T8 | **deleted** a register whose value was implied | +8.6 |
| T9 | strength-reduced a constant add | **0.0**, reverted |
| T10c | fourth blend stage, banked vertex captures | +2.0 |
| T11 | viewport mux off the DSP input | +0.5 |

T10c took five attempts; four failed identically and the one that passed but
stalled is what proved the diagnosis —
`reports/G8B-T10-BLEND-STAGE-ATTEMPT-20260917.md`. **No fit was spent on any
failure.**

**What it does not close, which the section below already said:** Packet I
still cannot be promoted, because Packet H precedes it and
`zhao_shell_top_v2.sv` does not exist. A clean G8B receipt was the EXPENSIVE
part of Packet I and never the whole of it.

> **2026-09-17:** the shell now exists and its clauses are closed (`packet-h`
> 72/72). Packet I's blocker is therefore no longer Packet H's *absence* — it
> is Packet H's remaining post-adoption composed FIT, plus G8B's own timing
> campaign, which is the larger of the two by a wide margin at 43.94 MHz.
>
> **CORRECTED the same day: G8B is CLOSED at 102.19 MHz** (`@g8b-t11-pins-s2`,
> physical pins, zero TNS, `status: ok`). The "43.94" above was this document's
> own first measurement quoted back as if it were current — written after
> reading `G8B-T10-BLEND-STAGE-ATTEMPT-20260917.md`, whose closing section said
> the next step was "specified and NOT built" and which had never been updated
> after that step was built, fitted and followed by T11. **The receipts in
> `reports/synthesis/zhao_block_fit.json` and `git log` were right throughout.**
> Prose describing work in progress goes stale in the direction of asking
> somebody to redo finished work; the receipts do not. Packet I's only
> remaining blocker is Packet H's composed fit.

### G8B CANNOT CLOSE PACKET I WHILE PACKET H IS MISSING

Noted 2026-09-16, and it changes what "finish R0" means next.

The packet order is fixed by the architecture:

```
A -> B -> C -> D -> E -> F(G8A) -> G -> H -> I(G8B) -> J(G8C) -> K
```

and the overlap rule is explicit about what development buys you:

> *"a later packet may be developed while an earlier **independent** fit runs,
> but it cannot be promoted past its gate or selected by a dependent packet
> until every predecessor is green."*

**Packet H is H, and it comes before I.** `zhao_shell_top_v2.sv` does not
exist, so H is not green, so **Packet I cannot be promoted past its gate no
matter what G8B measures.** A 100 MHz physical-pin receipt would satisfy the
part of I's gate that says *"one clean G8B receipt proving the parameters
actually elaborated"* and would still not close the packet.

**This does not make the G8B timing campaign premature.** The same rule
permits exactly this work — G8B is an independent fit, Packet I's RTL is
legitimately developed alongside it, and the terrain pipe had to reach 100 MHz
whenever it was done. What it settles is the ORDER OF WHAT COMES NEXT:

1. finish G8B's timing to a clean **physical-pin** 100 MHz receipt — the fit
   evidence Packet I needs, and the only part of it that is expensive;
2. **write `zhao_shell_top_v2.sv`** — Packet H, a composition of eight blocks
   that all exist and are tested (see the section below), and the single file
   standing between R0 and its last two packets;
3. then Packet I promotes, then J/G8C has a hierarchy to fit, then K.

Step 2 is not blocked on step 1 and does not touch its closure, so it is the
work to do while G8B fits.

### Packet H: `zhao_shell_top_v2.sv` EXISTS

*2026-09-17, later still. The sections below describe the road to it; this is
where it got to.*

**The file is written, lint-clean and registered.** 2,506 lines, 27 instances,
`-Wall` clean, `excluded:not-yet-adopted`, and `zhao_shell_top.sv` remains
byte-identical at its protected hash.

All **57 new inputs** have a driver or a stated reason. The composition is
driven by 72 directed checks in `tests/shell/zhao_shell_v2_lease_path.sv`:
one legal frame end to end, both writers contending on the shared response,
the frame-clear ordering measured against the bin pipe's own drain, a
structural fault suppressing publication and releasing the lease, the V3
programming channel run for real, and the swap echo through both CDC FIFOs.

| clause | state |
|---|---|
| one held-until-quiet clear per accepted lease, before admission | **measured** -- counted in RTL, and the counter seen to move |
| old-work drain ordering | **measured** -- refused in flight, accepted 1 cycle after drain |
| structural fault: no READY, no publication, lease released | **measured -- but read the last clause of that sentence with the correction two rows down.** No READY and no publication are consequences of the FAULT. The release is a consequence of the TERMINAL EVENT arriving on a lease already marked faulted, and the clause's wording invited the wrong reading for as long as the only test offered a terminal immediately afterwards |
| V3 programming channel | **SEALED.** BEGIN/ROW/END with a correct seal is accepted (status 0) and generation 2 activates; the unsealed attempt is kept ahead of it and still refused with CFG_BAD_CRC, because a test that only ever presents a correct seal cannot tell a channel that checks it from one that ignores it. Seal `0x12b4803e`, modelled in `tests/harness/zhao_binding_seal.hpp` -- extracted from the binding resolver's own test rather than folded a second time, and that test plus all 7 of its mutant controls still pass against the shared header. **The name in this document was wrong: the polynomial is `0xEDB88320`, reflected CRC-32 (zlib/IEEE), NOT CRC-32C** -- Castagnoli is `0x82F63B78`. Two details are load-bearing and neither is guessable from one row: the generation byte folds FIRST, and an absent selector contributes ten ZERO bytes, so a one-row page still folds 2,560 bytes |
| READY/swap CDC round trip | **measured** -- 10 cycles, through the real FIFOs |
| every new port connected | **audited** -- `packet_h_tieoff_audit`, 0 silent across 74 declared decisions. **The earlier "0 silent" was measured by an audit that could not see two whole classes.** It skipped EMPTY connections as "an unread output, named on purpose" -- an assumption wearing a check's clothes -- and its regex was anchored `^...$`, so a port map packing several connections onto one line was invisible. Widened to audit empty FAULT outputs (not telemetry; 89 reasons all saying "telemetry" is silence a tool manufactured for itself) and to read every connection on a line, the shell went from "19 declared, 0 silent" to 13 silent, ten of them literal tie-offs |
| the shell's fault OR names every structural fault | **6 terms, and two arrived by audit.** `attr_abort` sat in an empty port connection beside the `raster_abort` that WAS wired -- same class, same instance, dropped only because nobody looked. `cdc_gpu_protocol_fault` fires when a stalled producer mutates the 84-bit tuple or drops valid, and the shell discarded it. Its video-domain twin is declared and OWED a synchroniser: OR-ing a `vid_clk` level into a `gpu_clk` one is the CDC violation this packet already made once |
| the harness models the shell's attribution | **checked** -- `packet_h_fault_or_parity`. `bin_fault_w` is a hand-written copy of `v2_fault_level_c`, and divergence is invisible: no directed check reads the list, so a harness attributing four of six faults passes exactly as green. Fires on either side gaining a term, and on failing to FIND an expression -- two empty sets compare equal |
| nested V3 has no migration shadows | **witnessed** -- `packet_h_shadow_witness`, and the witness fires on two broken trees |
| old/new differential under paired traffic | **BOTH HALVES DONE.** Structural: `packet_h_sibling_diff` -- 93.4% of the protected V1 is line-identical in the sibling, every other region comment-only or declared, the 20 carried-over instances sealed; fires on drift, on a moved seed and on a dead rule. Behavioural: `shell_paired_diff_directed` -- both shells in one binary from one stimulus, **16,408 GPU cycles, 91 of 95 outputs compared, 0 mismatching cycles**, across quiet / audio+pads / HPS / scanout phases. Possible only because the sibling is a strict SUPERSET of the historical shell: 154 ports, none removed |
| ...and the differential is shown to FIRE | `shell_paired_diff_mutant_control` -- a committed mutant under `tests/mutants/`, one substantive line (`px_valid_o`'s V2 side negated), `WILL_FAIL`. It reports 16,400 mismatching cycles and names index 18 correctly. **A PASS here is the ABSENCE of a signal from 91 generated compare lines nobody reads**; one mistyped to compare v1 against v1 would agree forever and look identical to the green run |
| ...and it cannot pass by comparing nothing | An ACTIVITY WITNESS: one bit per compared output, set when V1's copy changes, floor asserted at 20. **27 of 91 toggle** under this stimulus. Two shells producing nothing agree perfectly, so without this the whole file passes on a stuck clock or an unreleased reset -- loudly, with green checks. Armed one cycle late: the delayed copies hold `x` until first loaded, and an unguarded compare marks ALL 91 toggled on the first edge |
| ...and the exemption list is minimal | 4 names, not 19. The first draft exempted every render counter by reasoning from the outside; reading the historical shell refuted 15 of them (`ring_wr_*` is the command scheduler's, `render_busy_o`/`render_pixels_o`/`render_fatal_o` the memory writer's, and the REPLACED slot manager drives no top-level output at all). Fifteen speculative exemptions would have been fifteen outputs never compared -- and it would have passed |
| sequence-abort RELEASE control | **not started, and now NAMED as a liveness hazard rather than a missing feature.** `zhao_video_slotmgr_v2` clears `lease_valid_q` in exactly ONE place outside reset -- `term_fire_c && term_match_c`. No timeout, no fault-driven release, no reclaim. `request_granted_c` requires `!lease_valid_q`. So a lease whose terminal never arrives is not a stalled frame, it is a **permanently wedged renderer**, with `faults_latched_o` reading 1 and every other counter looking healthy. `sequence_abort_o` is precisely the state in which no terminal will come -- it is the texture stage's drain mode, not a fault, which is why it is correctly absent from the fault OR and why the shell currently sinks it into a reduction-XOR with no functional consumer. **Unreachable today** (the only producer that could fail to emit a terminal is the V3 return path, tied off until J), and the directed test now asserts the dependency -- "no new lease is granted while a faulted one is held" -- so that when J wires that producer up, this control is not optional |
| structural faults through the reset barrier | **measured** -- reset clears the latch (1 -> 0), the barrier reopens in 16 cycles, the binner redoes a 561-cycle cold init, a second lease is granted, and a second fault latches EXACTLY ONCE. That last part is the clause: every term of the OR is a sticky level cleared only by reset, so without re-arming, "latched exactly once" is a claim about one fault in the whole life of the machine, and a machine that faults once and then reports nothing looks identical to one that never faults again |
| the six terms individually | **not separately reachable, and the honest answer is that they cannot be.** None is reachable from a quiescent bin pipe, and OR-ing six force bits into one aggregate would be six copies of one test -- the detector-with-operands-that-move-together shape. The property is carried in two halves instead: the aggregate path is proven re-armable (above), and `packet_h_fault_or_parity` proves the shell's OR names exactly the terms the harness's does |
| **THE ONE REMAINING GATE: a composed FIT of the sibling shell** | **Not started, and scoped here so it does not become an uncashed cheque.** Every gate Packet H has passed is Verilator or source-level; area and Fmax are unmeasured claims, and ALMs are the binding constraint. A physical-pin fit is impossible — 209 ports, far past the device's user I/O — which is why the V1 shell has `fpga/rtl/generated/zhao_shell_fit_top.sv`, a ten-pin characterization wrapper from `tools/quartus/gen_shell_fit_top.py`. **Three pieces of work, in order:** (1) make that generator module-agnostic — it hardcodes `zhao_shell_top`, `zhao_shell_fit_top` and `zhao_shell_fit_stimulus` in six places. **This was attempted on 2026-09-17 and REVERTED, and the reason is the useful part: the generator's sha256 is embedded in the committed wrapper AND bound into the committed fit-receipt fixtures, so any edit to it breaks nine `shell_fit_preflight_fixtures` tests until those fixtures are rebound.** That rebind is a provenance operation and belongs inside this work package, not ahead of it. The mechanical part is known-good — the refactor was verified to regenerate V1 byte-identically apart from its own hash — so redo it, then rebind, in one pass; (2) author `design/shell_fit_ports_v2.yml`, 209 entries against V1's 1,322-line 154-entry policy — **this is the part that is authored rather than mechanical, and the part that can silently lie.** A new port given a constant driver is folded away by the fitter, so the area comes back LOW, which is the flattering direction and the one nobody audits; (3) register the target in `design/fit_targets.yml` and spend one 1.5–4 h fit. The question that fit answers, named in advance per CLAUDE.md: *what does the composed sibling shell cost in ALMs, DSPs and RAM blocks, and at what Fmax?* |
| **CORRECTION: a fault does not release the lease** | The first fault case checked `lease_valid_o == 0` right after offering a publication, so the release read as a consequence of the FAULT. It is a consequence of the TERMINAL EVENT arriving on a lease already marked faulted. The reset-barrier case holds a faulted lease 400 cycles with no terminal and it stays live. The behaviour is right -- a fault must not abandon a slot the writer may still be mid-transfer into; the fault decides the frame is not PUBLISHED, the terminal decides the lease is DONE -- and only the reading was wrong |

**Six findings came out of composing rather than reading**, and each was a
thing the separate blocks could not show:

1. **Nothing acquired the blitter's lease.** The packet named four organs;
   there were five. Invisible from the port delta, because the manager's
   `blit_req_*` channel exists and looks connected.
2. **The 84-bit tuple layout was already defined** -- in the bridge's header
   and load-bearing at `zhao_video_ready_bridge_v2.sv:164` -- and the package
   written for it had the order REVERSED. Its own literal-based test passed,
   because the literals came from the same wrong premise.
3. **A CDC violation**: the bridge's `cdc_ready_*` is the video-domain output
   of the forward FIFO, not a GPU-domain source. Symptom: `ready_events_o`
   reads 1, `pending_q` stays 0, screen never updates, counters all right.
4. **The manager's fault port counts cycles, not events**, while the bin
   pipe's fault outputs are levels. Six-cycle assertion read six faults.
5. **A mutant shim was changing a production interface.**
   `zhao_raster_tile_pipe_v2_mutants.sv` defines `ZHAO_PACKET_D_TEST_HOOKS`
   from inside Packet D's shared source list, adding three ports to
   `zhao_geom_bin_pipe_v2`. 165 ports in production, 168 through that list.
6. **The `lseq` sequencer did two jobs** -- lease ordering AND payload
   capture. Replacing it with a lease leaf takes both, and losing the second
   is silent: right lease, wrong source address, plausible garbage.


### Packet H: what is BUILT, and what the remaining work actually is

*2026-09-17, later the same day. The section below correctly says Packet H is
a protocol job. This says how much of the protocol now exists.*

The section below names four organs. **There were five**, and the fifth was
missing rather than unfinished:

* `zhao_renderer_lease_v2` acquires writer 1's lease;
* `zhao_video_terminal_adapter_v2` already carried BOTH writers' terminal
  events back to the manager;
* **nothing acquired the BLITTER's lease.** The retained `zhao_debug_frameblit`
  still speaks the V1 `fb_lease_*` interface and had no way to be told which
  slot and generation it owns. The manager's `blit_req_*` channel exists and
  looks connected, which is why the gap was invisible from the port delta.

`fpga/rtl/video/zhao_video_blit_lease_v2.sv` is that organ. It carries forward
the historical shell's one-cycle law -- DEBUG.FRAMEBLIT latches
`fb_lease_generation_i` on the same edge it accepts a request, so the record is
frozen a state EARLIER and cannot be the pre-grant one -- structurally, and as a
fired control rather than as a comment. 45 directed checks, seven committed
mutants all DETECTED, `quartus_map` clean, 117 ALUTs / 123 registers / 0 DSP.

**The composition is driven, not just wired.**
`tests/shell/zhao_shell_v2_lease_path.sv` composes both leases, the manager and
the terminal adapter, and `shell_v2_lease_path_directed.cpp` drives one legal
frame end to end, both writers contending on the shared response, and a stalled
blitter that must not stop the renderer. 43 checks, counts asserted exactly.
`packet-h` is 59/59, up from 47.

**Two claims were withdrawn on measurement rather than carried forward**, both
about where a protection lives. The harness comment said its response demux was
what stopped either writer retiring the other's response; miswiring it as a
plain OR fired at that claim and nothing noticed, because each leaf qualifies
its own ready and asserts the invariant internally. And the starvation control
that the hand-driven stub version had is GONE -- with the real leaf that state
is unreachable from outside, so the deadlock is structurally absent rather than
untested, and the case was replaced by the head-of-line failure that is still
reachable.

**The 84-bit tuple is pinned.** `zhao_video_slotmgr_v2` takes the swap echo as
six fields summing to exactly 84, `zhao_fb_ready_cdc_v2` carries "one frozen
84-bit tuple" and never looks inside, and **the field order was defined nowhere**
-- both the pack and the unpack belonged to a file that does not exist, and a
rotated layout produces a plausible slot and a plausible generation that are not
the frame's, refused as stale with every counter balancing.
`fpga/rtl/video/zhao_fb_tuple_pkg.sv` defines it once, with an elaboration
tiling check and a directed test written against literals -- a round trip is
structurally blind here, since pack and unpack read the same constants.

**WHAT REMAINS, counted rather than estimated.**
`tools/design/packet_h_driver_contract.py` computes it and
`reports/PACKET-H-DRIVER-CONTRACT-20260917.md` is that output with judgement
applied. There are **57 new inputs** across the two swapped blocks. Forty-five have
organ drivers and are composed and tested today. The rest:

| group | inputs | what it needs |
|---|---:|---|
| V3 config / palette / page-generation programming | 20 | **WIRED AND SEALED** -- legal sequence runs, a correct seal is accepted and the page activates, an incorrect one is still refused. The seal is reflected CRC-32 (`0xEDB88320`), not CRC-32C as this table previously said |
| Packet-D attribute carriage (`tri_*`) | 7 | check `zhao_geom_wcache`'s payload width first |
| Packet-E ENGINE1 share (`fill_*`) | 4 | `fill_refused_i` is already ruled: typed recoverable path only |
| structural fault entry (`fault_*`) | 4 | **DONE** -- wired, edge-detected, tested |
| READY/swap CDC return | 7 | **DONE** -- both FIFOs and the bridge composed |
| remaining | 3 | the three `test_*` enables do not exist in a production build |

So the honest state is: **the lease path is finished and proven; the V3
programming, attribute, share and fault paths are specified and unwired; the
file is not started.** Sections 3.1 to 3.4 of the contract are design work, not
transcription, which is the section below's correction made specific.

### Packet H measured properly — it is a PROTOCOL job, not a wiring job

*2026-09-17, and this corrects the estimate in the section below.*

That section says Packet H is *"wiring an inventory that exists rather than
commissioning blocks"*, on the strength of all eight component blocks being
present and tested. The blocks are present. **The estimate was still too
optimistic, and the port delta says so:**

```
zhao_video_slotmgr -> zhao_video_slotmgr_v2     22 ports -> 73
    KEPT       8   clk, rst_n, swap_valid_i, swap_slot_i,
                   displayed_valid_o, displayed_slot_o,
                   leases_granted_o, stale_events_o
    REMOVED   14   the ENTIRE lease/publish/release interface --
                   lease_req_*, lease_grant_o, lease_refused_o,
                   fb_lease_*, publish_*, release_*, slot_ready_o
    ADDED     65   a writer-aware protocol: lease_{valid,slot,base,span,
                   writer,mode,generation,fault}_o, rsp_*, ready_*, term_*,
                   fault_*, blit_req_*, render_req_*, and eight counters
```

**Eight of twenty-two ports survive.** This is not a block whose name changed
— it is a different block implementing a different protocol, and the
lease/publish/release flow the historical shell wires directly between
`u_slotmgr`, `u_frameblit` and the renderer now has to be re-plumbed through
`zhao_renderer_lease_v2`, `zhao_video_terminal_adapter_v2` and
`zhao_video_ready_bridge_v2`. `zhao_geom_bin_pipe` tells the same story more
mildly: 63 → 165 ports, 114 added and 12 removed.

**Which also rules out generating the sibling from the original.** A generator
that renamed two instances and patched their port maps looked attractive — it
would be reproducible and could not go stale, which is how the other generated
tops in this tree earn their keep. It cannot work here: there is no
transformation from the V1 lease wiring to the V2 protocol, only a design.

**So the honest cost of Packet H** is: learn the writer-aware lease protocol
from the four organs' interfaces and section 13's gate clauses, compose it
through a ~2,000-line top, and satisfy a gate that includes the frame-fault
clear handshake, the sequence-abort RELEASE control and the reset-barrier
entry for five distinct structural faults. That is a packet-sized piece of
design work and it should be started with the protocol in front of you, not
bolted onto the end of a timing campaign.

It remains the next item, and it is the only thing between a closed G8B and
Packet I's promotion.

### What Packet H actually costs, now that the correction is in

Scoped 2026-09-16, after finding that `zhao_shell_top_v2.sv` does not exist.
**BUILT 2026-09-17** — the estimate below is kept for comparison against what
it actually took, which is the only way an estimate ever becomes calibration.
**Every component it composes DOES**, built, linted and tested with controls:

| file | bytes | packet-h tests |
|---|---:|---|
| `zhao_engine1_raw_last_v2.sv` | 19,521 | directed + 4 controls + 2 collisions + lint |
| `zhao_renderer_lease_v2.sv` | 15,155 | directed + 8 controls + 2 collisions + lint |
| `zhao_video_ready_bridge_v2.sv` | 13,696 | directed + 8 controls + 2 collisions + lint |
| `zhao_video_terminal_adapter_v2.sv` | 11,166 | directed + 7 controls + 2 collisions + lint |
| `zhao_video_slotmgr_v2.sv` | 19,490 | (Packet G) |
| `zhao_geom_bin_pipe_v2.sv` | 20,759 | (Packet G / byte-frozen) |
| `zhao_raster_tile_pipe_v2.sv` | 64,535 | (byte-frozen) |
| `zhao_geom_binner_v2.sv` | 46,692 | (byte-frozen) |

**So Packet H is a COMPOSITION, not a build.** The historical
`zhao_shell_top` instantiates 22 blocks; the sibling swaps `zhao_geom_bin_pipe`
and `zhao_video_slotmgr` for their V2s, adds the four Packet-H organs above,
and wires the V3 island's frame-fault clear handshake, the writer-aware lease
and the reset-barrier law its packet description sets out. That is real work —
the terminal law and the clear handshake are most of the packet's gate — but it
is wiring an inventory that exists rather than commissioning blocks.

**Why this belongs in the roadmap rather than a run log:** it changes the cost
of the critical path. G8C cannot run without this file, so after G8B closes,
`zhao_shell_top_v2.sv` is the single thing standing between R0 and its last two
packets — and knowing it is a composition of tested parts is the difference
between scheduling it and deferring it.

### The worst ALM domain is TWO PROJECTORS THAT NO LONGER EXIST

Measured 2026-09-16. *Projection and result arenas* carries **12,267 ALM
against a 4,500 allocation**, the largest breach in the scoreboard, and it is
exactly two rows:

```
zhao_geom_project      ALM  6,199   DSP 33   M10K 29   fitted at 83f050c5
zhao_terrain_project   ALM  6,068   DSP 33   M10K 23   fitted at 96c0394a
                       -----------------------------
sum                        12,267       66        52
domain row says            12,267       66        52
```

Three figures, exact on all three. **The domain IS those two rows**, and they
are the pair the DSP audit already identified as replaced: the shared G8B
projection group is physically fitted at 34 DSP against their 66, and is the
reason the audit puts 72–75 DSP of the 173 in the "already in the tree"
column.

**The roadmap anticipated this and said so in section 2.1**, which is quoted in
`tools/budget/domain_scoreboard.py`'s own header:

> *"A 5.8k projection candidate misses its 4.5k objective but might still be a
> large improvement over two engines."*

The allocation was written for ONE engine. The scoreboard is charging the
historical TWO-engine price against it, so "2.7x over" is a statement about an
arrangement the tree has already replaced, not about the current design.

**AND THIS IS NOT PERMISSION TO SUBTRACT.** The replacement is measured — G8B
is 8,268 ALM, 32 DSP, 45 M10K at `@g8b-t56` — but at a DIFFERENT SCOPE: that
row is the whole terrain pipe, including tessellation, the w-cache and the group
sequencer, which belong to *Terrain, forge and maintenance*, and it excludes the
geometry client that `zhao_geom_project` served. Restating the domain needs the
shared group fitted AT the domain's boundary, which is a MEASUREMENT and not
arithmetic. The audit is explicit that historical, candidate and adopted prices
must be kept in separate columns, and this is exactly the subtraction it
forbids.

**A CORRECTION THIS FORCES, made the same day it was written.** The M10K
direction section above says this domain is *"already over its 48-block M10K
allocation at 52"*, offered as the reason memory cannot simply be poured into
the worst ALM breach. **That 52 is 29 + 23 from these same two stale rows.**
The current shared arrangement uses 45 M10K for a wider scope. The caution was
built on the measurement it was warning about — *never compare a current file
to an old measurement* — and it is withdrawn as stated. What survives is the
weaker and still-true form: **the M10K envelope is per-domain and finite, and
no domain's current memory position can be read off this scoreboard until its
stale rows are re-measured.**

**What to do about it, in order:** the `uncashed_cheques.py` check-2 list
already flags both rows (`zhao_terrain_project` measured at a commit older than
its own source). Re-measuring them is not a new fit — it is the G8C
composition, which fits the connected hierarchy and prices the whole thing
once. Until then every domain total containing a stale row is evidence about
the past, and the scoreboard should say so per row rather than only in prose.

### The work list the direction implies — blocks that spend ALMs and no memory

> ## 2026-09-18 — THE COMPOSED SHELL ANSWERS THE DIRECTION, AND THE ANSWER IS ONE BLOCK
>
> The first composed measurement of `zhao_shell_top_v2` (97 sources, virtual
> pins, clean tree at `3e41ba61`) **did not fit the device**, and Analysis &
> Synthesis says by how much:
>
>     Estimate of Logic utilization (ALMs needed)   62,534      device: 41,910
>     Total registers                               80,173
>     Total DSP Blocks                                  63      budget:     85
>     Total block memory bits                      422,480      device: 5,662,720
>
> `quartus_fit` then failed, which is consistent with a design at 149% of the
> device. The row is stamped `incomplete:failed:quartus_fit.exe` and carries NO
> ALM or Fmax, because the fitter never produced any.
>
> **THE HIERARCHY NAMES ONE BLOCK.** The nodes nest, so read it as containment:
>
> | node | ALUT | registers |
> |---|---:|---:|
> | `zhao_shell_top_v2` | 65,696 | 80,173 |
> | ` └ zhao_geom_bin_pipe_v2` | 54,301 | 68,753 |
> | ` └ zhao_raster_tile_pipe_v2` | 52,910 | 66,687 |
> | ` └ zhao_raster_texture_stage_v3` | 42,165 | 58,943 |
> | ` └ zhao_texture_island_v3_top` | 42,075 | 58,705 |
> | ` └ **zhao_texture_binding_resolver_v2**` | **28,957** | **39,449** |
>
> **One leaf is 44% of the composed shell's logic and 49% of its registers**, and
> Quartus says why in its own words:
>
>     Info (276007): RAM logic "...page0_m" is uninferred due to asynchronous
>     read logic   zhao_texture_binding_resolver_v2.sv Line: 259
>     ...same for page1_m Line 260, and uvw_m in zhao_texture_island_v3_top Line 916
>
> The two binding page banks are `binding_row_t page0_m [0:255]` and `page1_m`,
> and `binding_row_t` is exactly 75 bits (valid + 8 + 2 + 32 + 32). So
> **2 x 256 x 75 = 38,400 bits of page table are sitting in flip-flops**, plus
> 512 valid bits — 38,912 against a measured 39,449 registers. The page banks
> ARE the block's register count; the remaining 537 is its FSM and CRC state.
>
> **THIS IS THE OWNER'S DIRECTION, EXACTLY.** *"We have lots of M10K memory,
> ALMs are over budget — what you can, you need to solve with memory."* 422 Kbit
> of a 5.66 Mbit device is in use. Moving 38 Kbit of page table out of fabric is
> the single largest lever in the machine and it is not an architecture change.
>
> **WHY THE EARLIER SWEEP MISSED IT, which matters more than the number.** On
> 2026-09-16 `check_array_storage.py` concluded *"no block with a current fit
> row holds 8 Kbit or more of declared array in flip-flops"*, and the work was
> redirected to converting computation to lookup on that basis. The conclusion
> was true as stated and misleading as used: **`zhao_texture_binding_resolver_v2`
> had no current fit row**, so the qualifier excluded the one block that breaks
> the rule by a factor of five. A filter keyed on "has a fit row" hides exactly
> the blocks nobody has measured, which are the blocks most likely to be wrong.
>
> **AND THE FIX IS AVAILABLE, by an invariant the design already maintains.**
>
> Each bank looks like it has three readers. It has two, and they never collide:
>
> * the two CRC-walk reads (`crc_selector_q` at the group start,
>   `crc_next_selector_c` at the advance) are mutually exclusive branches of one
>   FSM writing one destination, `crc_row_q` — that is ONE port with a muxed
>   address, not two;
> * the data plane reads `page{active_bank_q}_m[req_binding_selector_i]` into
>   `read_row_q`, and the CRC walk reads `page{staging_bank_q}_m`.
>
> And the bank selectors are complementary **by construction**:
>
>     441  active_bank_q  <= 1'b0;            reset: complementary
>     442  staging_bank_q <= 1'b1;
>     495  staging_bank_q <= ~active_bank_q;  maintained
>     579  active_bank_q  <= staging_bank_q;  the atomic activation edge
>
> So `page0_m` is read by the data plane only when `active_bank_q == 0`, and by
> the CRC walk only when it is 1. **Neither array ever has two readers in the
> same cycle.** Each therefore needs ONE read port with an address and
> destination muxed on `active_bank_q` — one write, one read, which is exactly
> the simple-dual-port idiom M10K infers.
>
> 256 x 75 bits per bank is 19,200 bits; at M10K's 40-bit simple-dual-port width
> that is ~2-3 blocks each, so **roughly 4-6 M10K against 553 available**, to
> retire ~28,957 ALUT and ~39,449 registers. If it lands anywhere near that, the
> composed shell goes from 62,534 ALMs — 149% of the device — to roughly 34,000,
> which fits with margin and is close to the 30,000 target.
>
> ### THE FITTER'S OWN RECEIPT: `zhao_shell_top_v2@packet-h-m10k`, status `ok`
>
> | | measured | budget |
> |---|---:|---:|
> | **ALMs** | **29,044** of 41,910 | 30,000 — **inside** |
> | DSP blocks | 63 of 112 | 85 — inside |
> | M10K | 134 of 553 | — |
> | registers | 40,773 | — |
> | **Fmax** | **54.12 MHz** | 100 — **fails** |
> | setup TNS | −29,688.8 ns | |
>
> Clean tree at `e05d409b`, 97 sources, digest `6927be23edea`, seed 1, virtual
> pins. **The first `ok` row this target has produced.**
>
> **READ `status: ok` NARROWLY.** It means the budget rules for this target
> accepted the row, and this target has no Fmax rule — unlike G8B's, which is
> why G8B sat at `failed:structure` for its whole campaign. The area question
> is answered and the CLOCK question is not: 54.12 MHz against a ruled 100.
>
> **AND THE PATHS START AT THE MEMORY — WHICH IS NOT THE SAME AS THE MEMORY
> BEING THE COST.** This heading read "AND THE M10K READ IS ON THE CRITICAL
> PATH" until the path detail was opened; it is left visible because the
> difference between "launches at X" and "is slow because of X" is the entire
> lesson of the three corrections below. The 2,000 summarised negative paths,
> grouped by the leaf at each END:
>
> | from → to | paths | total | worst |
> |---|---:|---:|---:|
> | **`altsyncram` → `zhao_texture_binding_resolver_v2`** | **515** | −2,617.8 | −8.273 |
> | `zhao_texture_v3own` → itself | 276 | −1,154.1 | −5.221 |
> | `zhao_raster_texture_stage_v3` → `zhao_texture_island_v3_top` | 207 | −839.0 | −4.317 |
> | `zhao_geom_binner_v2` → `zhao_texture_island_v3_top` | 206 | −773.6 | −4.475 |
> | `zhao_raster_attrgrad_v2` → `zhao_raster_attrdiv_v2` | 121 | −537.0 | **−8.477** |
>
> Data delay: worst **17.809 ns**, median **13.313 ns**, against a 10.000 ns
> period. The median bad path is a third over the clock, so this is not one
> chain — it is a broad shortfall with one dominant family.
>
> #### Two corrections, and both matter more than the table
>
> **1. The M10K read is NOT the cost. It is 0.192 ns.** I wrote in the previous
> revision of this section that the obvious move was to register the RAM output,
> because 515 paths launch inside the memory. Reading the *detail* rather than
> the summary kills that: on the −8.273 path the RAM contributes
> `portbdataout[20]` at **+0.192 ns**, and the remaining **16.0 ns is
> combinational logic after the data leaves the memory** —
>
> ```
>  8.219  +0.192  ...|page0_m_rtl_0|...|ram_block1a0|portbdataout[20]
>  9.597  +1.378  read_row_c.mode[13]~10|combout
> 11.107  +1.510  Add8~9|sumout
> 12.254  +1.147  Add10~9|sumout
> 13.665  +1.411  ShiftLeft2~29|combout
> 14.489  +0.824  ShiftLeft2~47|combout
> 15.893  +1.404  ShiftLeft2~60|combout
> 18.342  +2.449  Add13~125|sumout        <- 64-bit carry chain, ~30 cells
> 19.051  +0.709  max_byte_offset~54|combout
> 21.212  +2.161  Add14~45|sumout
> 21.964  +0.752  binding_fault_o~1|datad
> ```
>
> That is `binding_row_legal()` — the packed-chain address bound — evaluated
> combinationally on the freshly-read row. Registering the RAM output would buy
> **0.192 ns**. The caution about M10K read latency was real and is not what
> happened here; the honest reading is that the memory move cost essentially
> nothing in time and the arithmetic hanging off it costs everything.
>
> **2. The resolver does not set Fmax. `zhao_raster_attrgrad_v2` does.**
> `1 / (10.000 + 8.477) ns = 54.12 MHz` — exactly the reported figure, and
> −8.477 is the **attrgrad → attrdiv** path, not a resolver path. Deleting
> every one of the 515 RAM-sourced paths would leave the worst at −8.477 and
> **Fmax would not move at all.** The resolver family is the biggest
> *population* and dominates TNS; it is not the binder. The binder is a
> 17.809 ns chain of long adders (`Add5` → `Add6` → …) from `row_r[0]` into
> `attrdiv`'s `final_sat_r`, which is genuine arithmetic depth and needs
> pipelining, not deletion.
>
> So the resolver work below is a **TNS and area** win. Reaching 100 MHz needs
> every one of 2,000 paths under 10 ns, against a median bad-path delay of
> 13.313 ns. That is a campaign, not two fixes, and this file should stop
> implying otherwise.
>
> **Still NOT established: that the M10K move made the clock worse overall.**
> There is no before-picture — the pre-change fit FAILED and produced no timing,
> and map-only rows produce none by construction. What is established is where
> the time goes NOW, and that the memory itself is not where it goes.
>
> #### The prediction, written down BEFORE the fit that tests it
>
> This file has a chapter on diagnoses that arrive already believed. The defence
> is to state the expected number first, so the fit can refute it.
>
> Decomposing the −8.477 path (data path opens at 6.084 ns):
>
> | stretch | ns |
> |---|---:|
> | `Add5` → `Add8`, four 96-bit adds in `attrgrad` | 9.50 |
> | `u_div\|Add0` | 2.24 |
> | `u_div\|LessThan0~15..26`, a 96-bit compare chain | 4.45 |
> | `final_sat_r~3..4` | 1.62 |
>
> Two changes went in against this:
>
> 1. **`attrgrad`'s row offset summed as a tree, depth 5 → 3.** Removes one to
>    two adder levels from the stretch above, so ≈ **2.3–4.7 ns**. Bit-exact —
>    associativity of mod-2⁹⁶ addition — and held to that by the directed test,
>    the R4 variant, two mutants and the DSP3 differential against an untouched
>    second implementation.
> 2. **The resolver's legality verdict stored beside the row.** Removes the
>    16.0 ns cone — from **two paths**, not from 515. See below.
>
> ##### The 515 are not one family, and assuming they were was this section's
> ##### third flattering error in a row
>
> I wrote above that storing the verdict "removes the 16.0 ns cone from 515
> paths", because the two worst RAM-sourced paths run through
> `binding_row_legal`. Splitting all 516 by DESTINATION instead of by module:
>
> | destination | paths | worst | cone |
> |---|---:|---:|---|
> | `page0_valid_q` / `page1_valid_q` | **512** | −6.230 | **CRC scan** |
> | `binding_fault_o`, `disposition_refuse_q` | 2 | −8.273 | legality |
> | `read_row_present_q`, `cfg_rsp_op_q` | 2 | −4.764 | mixed |
>
> **512 of 516 are the CRC SCAN**, and the legality change does not touch them.
> Their cone is `portbdataout` → `Mux10~4/5/6` (the byte selector) → `crc~3`
> (`crc32_byte`) → `Equal35~5/~14` (the seal compare) → the valid vectors —
> read from the path detail, where 140 of the 200 detailed paths land there.
>
> So the legality change removes the two WORST RAM paths and about 16 ns of TNS,
> not 2,617. It is still right — it deletes a re-derivation and one of two
> silicon copies of a 64-bit arithmetic cone — but it is a small TNS win, not a
> large one.
>
> **The revised prediction: worst slack −8.477 → about −6.2, and Fmax 54.12 →
> roughly 61–62 MHz**, with the binder becoming the **CRC scan at −6.230**. The
> `attrgrad` tree does the work; the resolver change clears the two paths above
> it and leaves the CRC family exposed as the next target.
>
> **REVISED AGAIN, because the CRC change landed in the same batch** rather than
> waiting for a fit between them — CLAUDE.md's rule is to batch changes and
> spend one fit, and three changes in one fit is what that means here. With the
> 512-path family split at a register too, the remaining candidates for binder
> are `v3own → v3own` at −5.221 and `binner_v2 → uvw_m` at −4.475, neither
> touched. So: **worst ≈ −5.2 to −6.0, Fmax ≈ 62–66 MHz, binder becoming
> `c3t_v_q → clm_q[*]` in `v3own`.**
>
> Three predictions now sit above this line and the fit will judge all of them
> at once. That is the cost of batching, and it is the right trade when a fit is
> 23 minutes and there are three changes: what it buys is that a wrong
> prediction is still a measured number, while three separate fits would have
> bought one extra hour and the same three numbers.
>
> ### THE WHOLE MACHINE SYNTHESISES — FOR THE FIRST TIME
>
> 2026-09-18. `zhao_prod_top@whole-machine-map-probe`, `map_only`, **1,180 s**:
>
> ```
> registers     105,818
> memory bits 1,029,005   of 5,662,720   (18.2%)
> ALM / Fmax    not produced — map-only, by construction
> ```
>
> Its only previous row was `failed:quartus_map.exe` at **59.9 s**. Whatever
> killed Analysis & Synthesis on commit `0e8b1c9d` no longer does, and the
> complete console — legacy shell plus seven terrain blocks plus geometry,
> FIELD, particles, compositor and the rest of 147 sources — now elaborates and
> maps end to end. **No fit has ever been attempted on it**, so ALM and Fmax for
> the machine as a machine still do not exist.
>
> #### AND THE MAP REPORT ANSWERS THE AREA QUESTION WITHOUT A FIT
>
> I first estimated this from a register ratio — 1.41 registers per ALM on the
> shell, giving ~75,100 — and then read the report, which states it outright:
>
> ```
> Estimate of Logic utilization (ALMs needed)   100,709
> Combinational ALUT usage for logic           158,887
> Total registers                              105,818
> Total DSP Blocks                                 123
> Total block memory bits                    1,029,005
> ```
>
> **The register-ratio estimate was 25% LOW** — the flattering direction, for
> the fourth time in this section. It is left above rather than deleted because
> the pattern is the point.
>
> **The synthesis estimate is trustworthy here, and that is measured, not
> assumed.** The composed shell gives a live calibration on the same device,
> same flow, same day:
>
> | | map estimate | fitted | error |
> |---|---:|---:|---:|
> | shell ALM | 31,456 | 28,959 | +8.6% |
> | shell DSP | 63 | 63 | exact |
>
> Applying that 1.086: **~92,700 ALM fitted, against a 41,910 device — about
> 2.2× over.** DSP needs no scaling because the estimate was exact: **123 DSP
> against a 112-block DEVICE**, which is over the physical part and not merely
> over the 85 objective. Memory is the one comfortable number at 18%.
>
> *(The roadmap elsewhere records a map estimate of 62,534 against a fitted
> 29,044 — a 2.15× error — which is why the calibration was checked rather than
> reused. That case was the pre-M10K arrangement, where 38,400 bits sat in
> flip-flops and the fitter recovered enormously. The current design has no such
> slack left, which is exactly why its estimate is close.)*
>
> #### What this does and does not say
>
> **It says the machine as composed in `zhao_prod_top` is about 2.2× too big and
> is over the DSP device count.** That is the first whole-machine statement this
> project has ever had, and it did not need the multi-hour fit — a 1,180 s map
> and one calibration row produced it.
>
> **It does not say the SELECTED machine is 2.2× too big.** `zhao_prod_top`
> carries superseded variants alongside their replacements — the V1 texture
> island beside the V3 one, `material_combine_v1` beside its successor — and
> nothing in the tree marks which is selected. The leaves analysis below charges
> 93,310 ALM including exactly that duplication, which lands within 1% of the
> calibrated 92,700 and corroborates both numbers while sharing their one flaw.
>
> **So the next measurement is not a bigger fit. It is the selected-variant
> list**, after which the same 1,180 s map answers the real question. Spending
> four hours fitting a composition known to contain duplicates would measure a
> circuit we already know is wrong — which this repository has a rule about.
>
> #### AND THE MAP CARRIES THE PER-GROUP ALLOCATION ITEM (1) ASKED FOR
>
> The report's **Compilation Hierarchy Node** table attributes ALUTs to every
> instance in one consistent synthesis — no fit-row sums, no double counting, no
> graph reconstruction. `zhao_prod_top` has **66 direct children totalling
> 151,705 combinational ALUTs** of the design's 158,887.
>
> Converting with the map's own two totals (100,709 ALM ÷ 158,887 ALUT = 0.634)
> so the unit is consistent, against the golden path's envelopes:
>
> | group | ALUTs | ≈ALM | envelope | |
> |---|---:|---:|---:|---:|
> | Backend/platform | 49,241 | 31,200 | 14,000 | 2.2× |
> | **Shared projection and replay** | **29,684** | **18,800** | **5,000** | **3.8×** |
> | Terrain/Forge/surfaces | 25,874 | 16,400 | 4,500 | 3.6× |
> | Complete FIELD | 21,565 | 13,700 | 4,500 | 3.0× |
> | Geometry and lighting | 18,412 | 11,700 | 5,500 | 2.1× |
> | Post and 2D | 6,102 | 3,900 | 2,000 | 1.9× |
> | Complete particles | 827 | 500 | 2,000 | 0.26× |
> | **TOTAL** | **151,705** | **96,200** | **37,500** | **2.6×** |
>
> The eight largest single nodes:
>
> ```
> zhao_shell_top              17,917      zhao_raster_toon        5,289
> zhao_texture_island_v3_top  15,446      zhao_terrain_bake       5,272
> zhao_terrain_project        14,289      zhao_geom_wcache        5,132
> zhao_geom_project           10,263      zhao_forge_cliff        8,715
> ```
>
> **`zhao_terrain_project` + `zhao_geom_project` = 24,552 ALUTs ≈ 15,600 ALM**,
> against a 5,000 envelope for *exactly one* projection service. **That is the
> single largest overrun in the machine and it is the uncashed cheque CLAUDE.md
> documents**: the source was deduplicated into `zhao_project_core` and the
> silicon still carries it twice. It has been described for weeks as
> "~6,000 ALM"; measured in the whole machine it is about 15,600, because each
> instance drags its own arenas and caches — `zhao_geom_wcache` at 5,132 is a
> third row of the same story.
>
> **Particles at 0.26× is NOT a credit.** The golden path is explicit: *"an
> omitted function is an error, not a free saving."* 827 ALUTs is a domain that
> is essentially not built, and its 2,000 is owed, not banked.
>
> **IT IS A SELECTED-SCOPE STATEMENT AFTER ALL — I was wrong twice over.**
>
> I wrote above that "nothing in the tree says which variant is SELECTED", and
> then that this table "is not a selected-scope statement". Both are false, and
> checking took one command. `design/prod_manifest.yml` records selection
> explicitly — **63 adopted tops and 128 excluded**, the exclusions carrying
> reasons: 33 `not-yet-adopted`, **31 `superseded`**, 30 `probe`, 17 `frozen`,
> 13 `unused`, 4 `harness`. Cross-referencing it against the hierarchy:
>
> ```
> zhao_prod_top direct children          64 entities / 66 instances
>   of which marked superseded            0
> adopted tops NOT instantiated           0
> ```
>
> **`zhao_prod_top` is exactly the adopted set** — every adopted top appears,
> and no superseded module does. `zhao_texture_island_top` is not a child at all;
> it is marked `probe`, *"retained G1-D composition oracle; not a selected
> accounting root"*, which is why my fit-row pass kept charging it and the
> hierarchy never did.
>
> **So the table above IS the selected machine**, and the conclusion is stronger
> than it was hedged to be: **the selected console is about 2.6× its design-to-
> cost objective and 2.2× the device, with DSP over the physical part.** The
> two projectors are both adopted; the duplication is real, in the selected
> scope, and is the single largest line item.
>
> *(The error is worth keeping. The first claim was flattering — "the number is
> inflated by duplicates" makes the machine sound smaller than measured. The
> second was the opposite kind, refusing a conclusion the evidence supported.
> Both came from not running the one query that settles it, and this document
> had already named `prod_manifest.yml` as the selection record in another
> section.)*
>
> ### THE GOLDEN PATH SAYS WHAT CLOSURE IS, AND IT WAS UNINDEXED
>
> Found 2026-09-18. `reports/Zhaozhou_conditional_golden_path.md`, owner
> document of 2026-09-14, **pinned to this branch by name in its own header**,
> and indexed in neither `DOCKET.md` nor `OWNER-DOCUMENT-INDEX.md` — the latter
> having been regenerated past its date. Both are fixed now. It is the authority
> on what "closure" means and it outranks any local reading of the 30,000
> target:
>
> * charter-required **10% fabric reserve** → a **90% working limit of 37,719**;
> * an illustrative complete portfolio of **37,500** in seven non-overlapping
>   groups: Backend/platform 14,000 · Shared projection and replay 5,000 ·
>   Geometry and lighting 5,500 · Terrain/Forge/surfaces 4,500 · Complete FIELD
>   4,500 · Complete particles 2,000 · Post and 2D 2,000;
> * *"Do not call a 40.5k result closure merely because it is below 41,910."*
>
> #### It names a trap this document came one step from
>
> Shell + terrain measures **37,254** and the portfolio totals **37,500**. Those
> are nearly equal and **not comparable**. The 37,500 covers the complete
> selected scope — geometry front end, complete FIELD, particles, post and 2D —
> none of which is in the 37,254. The envelopes are per-group precisely so this
> cannot be fudged, and the group-by-group ownership mapping does not exist yet.
>
> #### And §7 lists a reject condition this campaign has to answer
>
> > *Reject or redesign the portfolio when … **ALM decreases while DSP, RAM
> > ports, timing or bandwidth violate their limits**.*
>
> That is a fair description of the M10K work in isolation: area fell while the
> composed shell sits at 61.52 MHz against a ruled 100. The honest position is
> that the timing work is not a separate nicety running beside the area work —
> it is the condition under which the area result counts at all, which is why
> three of today's four changes were timing changes rather than further area
> ones. It is not resolved, and quoting 28,959 ALM without it would be exactly
> what §7 forbids.
>
> #### What the golden path says to do next, in its own words
>
> §8 asks for **one bounded architecture packet**, alongside Packet D/E work,
> that: (1) produces a complete owner/function allocation including unbuilt
> requirements and the board wrapper; (2) derives mandatory per-frame and
> per-phase workloads; (3) qualifies the exact compositor fusion and particle
> schedules in simulation; (4) freezes selected memory geometries and provider
> calendars; (5) attaches each unpriced objective to an existing subsystem gate.
>
> #### Item (1) attempted, three times, and what actually blocks it
>
> Tried 2026-09-18 against the measured rows. Recorded because each failure
> names a real obstacle rather than a mistake in arithmetic:
>
> 1. **Sum every measured row by directory → 140,559 ALM.** A pure double count:
>    `zhao_shell_top_v2` (28,959) sits in the same directory as the leaves it
>    contains, so whole subtrees were added to themselves.
> 2. **Fold contained modules using `module_graph` → still 140,559.** It folded
>    ZERO, because `build()` returns `(module → file)` and `(FILE PATH →
>    instantiated modules)`, not `(module → children)`. *The number not moving
>    is the tell* — the same stale-measurement signature this repository has a
>    chapter on, caught only because the total was identical to the digit.
> 3. **Invert the map properly → 42 modules folded, 93,310 ALM charged.**
>
> | group | charged | envelope | Δ |
> |---|---:|---:|---:|
> | Backend/platform | 52,135 | 14,000 | +38,135 |
> | Shared projection and replay | 12,267 | 5,000 | +7,267 |
> | Complete FIELD | 12,364 | 4,500 | +7,864 |
> | Terrain/Forge/surfaces | 10,365 | 4,500 | +5,865 |
> | Geometry and lighting | 6,179 | 5,500 | +679 |
> | Complete particles | 0 | 2,000 | −2,000 |
> | Post and 2D | 0 | 2,000 | −2,000 |
>
> **93,310 is still not the answer, and the reason IS item (1).** The table
> charges `zhao_texture_island_top` (13,615, the V1 island) beside
> `zhao_shell_top_v2`, which contains the V3 one; it charges
> `zhao_texture_material_combine_v1` beside its successor. **Nothing in the tree
> says which variant is SELECTED**, so a mechanical pass cannot tell a
> replacement from a duplicate, and every such pair inflates the total.
>
> The two zeros are the other half of the same gap: particles and post/2D have
> **no measured rows at all**, and the golden path is explicit that *"an omitted
> function is an error, not a free saving"* — so those are not credits.
>
> **So item (1) is not a computation somebody failed to run. It is a decision
> nobody has recorded**, and the useful deliverable is a selected-variant list,
> after which this table computes itself. That is worth saying precisely,
> because "produce a complete owner/function allocation" reads like an analysis
> task and the blocking part of it is not.
>
> Item (1) is the missing mapping above. **And §4 is a concrete, already-proven
> DSP rescue sitting unclaimed:** POST.COMPOSITE's three generated curves and
> 3×3 Q2.14 matrix fuse into three precomputed unrounded product tables, so nine
> per-pixel multiplies become three table reads and an add — an exact
> distributive identity the owner verified over **1,048,576 input/configuration
> pairs with zero mismatches**, with a one-entry perturbation producing 2,048
> wrong colours as its positive control. Its own caution is that the ALM benefit
> is uncertain because addressing and adders cost logic, and that it is *"three
> simultaneous 72-bit read ports … six physical M10Ks initially"*, not one.
>
> ### THE RESULT: 54.12 → 61.52 MHz, and the prediction was optimistic
>
> `zhao_shell_top_v2@packet-h-timing`, `status: ok`, clean tree, commit
> `cebed2fe`, digest `c3ae9b76`, 1,717.7 s.
>
> | | `@packet-h-m10k` | `@packet-h-timing` | |
> |---|---:|---:|---|
> | Fmax | 54.12 MHz | **61.52 MHz** | +7.40 |
> | worst setup slack | −8.477 ns | **−6.254 ns** | +2.223 |
> | setup TNS | −29,688.8 ns | **−19,984.6 ns** | **−33%** |
> | ALM | 29,044 | 28,959 | −85 |
> | DSP / M10K / registers | 63 / 134 / 40,773 | 63 / 134 / 40,790 | — |
>
> **The prediction was 62–66 MHz and −5.2 to −6.0. It came in at 61.52 and
> −6.254 — just outside on both, optimistic in both directions.** Writing it
> down first is what makes that statement possible; without it this would read
> as an unqualified success instead of a slightly-short one.
>
> **What each change did, measured:**
>
> * **The resolver is GONE from the table entirely.** Not one `altsyncram →
>   binding_resolver` row survives. Both the 512-path CRC family and the two
>   legality paths are eliminated, and that is where the 9,704 ns of TNS went.
> * **The `attrgrad` tree worked and is STILL the binder.** −8.477 → −6.254, so
>   the depth 5 → 3 change bought 2.2 ns, and the path count in that family fell
>   from 121 to 26. But nothing overtook it, so `attrgrad → attrdiv` sets Fmax
>   again: `1/(10.000 + 6.254) ns = 61.52 MHz`, exactly the reported figure.
> * **`v3own → v3own` has dropped off the list**, which the prediction expected
>   to become the binder. It did not.
>
> #### The one number that looks like a regression and is not
>
> `zhao_geom_binner_v2 → zhao_texture_island_v3_top` went from **206 paths to
> 1,689** and from −773.6 to −5,493.5 total. Read carelessly that is a large
> regression in the family this section already identified as `uvw_m`.
>
> It is the opposite. Both reports summarise the **2,000 worst paths**, a
> fixed-size list — so when the top of it is repaired, the long tail becomes
> visible. Per path: **−3.76 ns before, −3.25 ns now.** The family did not get
> worse; it stopped being crowded out by the CRC scan.
>
> This is the same instrument error as grouping by module, in a new place: a
> count from a truncated list is a statement about the list, not about the
> design.
>
> #### The binder after the tree: the DIVIDER is now more than half of it
>
> `@packet-h-timing`'s worst path, −6.254, decomposed (data path opens at 5.988):
>
> | stretch | ns |
> |---|---:|
> | `attrgrad` — `Add8` → `Add5` → `Add6` | 6.73 |
> | `attrdiv` — `Add0` | 3.21 |
> | `attrdiv` — `LessThan0~15..26` | 3.87 |
> | `attrdiv` — `final_sat_r~3..4` | 1.60 |
> | | **15.67** |
>
> Two things to read off it. **The tree is exactly what was designed** — three
> adds where there were four-plus-one, and `attrgrad`'s own share fell from
> ~9.50 ns to 6.73. And **`zhao_raster_attrdiv_v2` now holds 8.68 of the
> 15.67 ns**, more than half, entirely untouched by this work.
>
> So the next lever in this family is the divider's saturation tail: a 96-bit
> `Add0` feeding a `LessThan0` comparator chain feeding `final_sat_r`. The
> obvious split is a register between the add and the compare, and the walk that
> feeds it is a per-row divide rather than a per-pixel one — but
> `zhao_raster_attrdiv_v2.sv` is in Packet E's `PROTECTED_HASHES`, so it is the
> third file in that set this campaign has needed. The precedent from the other
> two applies: the set asserts Packet E did not touch these, which stays true.
>
> **The cone, read out of the source rather than guessed.** `zhao_raster_attrdiv
> _v2.sv:73–82`:
>
> ```systemverilog
> rounded_num_c   = `ZHAO_ATTR_V2_ROUND_NUM(num_i, area_ext_c);  // the 97-bit Add0
> pos_sat_limit_c =  area_ext_c <<< 31;
> neg_sat_limit_c = -(area_ext_c <<< 31);
> sat_pos_c = (area_i != 47'd0) && (rounded_num_c >= pos_sat_limit_c);
> sat_neg_c = (area_i != 47'd0) && (rounded_num_c <  neg_sat_limit_c);
> ...
> final_sat_r <= sat_pos_c || sat_neg_c;                          // line 191
> ```
>
> `num_i` is `attrgrad`'s `row_num_c`, arriving combinationally from the adder
> tree — so one cycle carries the tree, a 97-bit round-and-add, and TWO 97-bit
> signed comparisons. That is the whole 8.68 ns, and it explains why the
> divider's share grew when the tree shrank: the two are one path, not two.
>
> **The promising fix costs no cycle, which is why it needs checking carefully.**
> `D_IDLE` already captures the rounded value into `dividend_r`, and `D_PREP`
> already exists as a state that reuses that bank. Computing the saturation test
> in `D_PREP` from the REGISTERED `dividend_r` would start the compare at a
> flip-flop with no `num_i` and no `Add0` ahead of it, using a cycle the divider
> already spends. If that holds it is the same shape as the CRC verdict fix.
>
> **What makes it delicate:** the file is in Packet E's `PROTECTED_HASHES`, it
> carries an exact-law negative-half mutant that selects `ZHAO_ATTR_V2_ROUND_NUM`
> specifically, and lines 84–104 contain an explicit *"Do not simplify this back
> into a materialized negate"* warning about a neighbouring identity. Any change
> here owes the full mutant gauntlet, and the saturation decision is externally
> visible through `q_saturated_o` and `saturations_o`.
>
> ##### THE CHANGE BELONGS IN THE DIVIDER, AND THAT IS NOT THE OBVIOUS PLACE
>
> The obvious fix is a pipeline stage in `attrgrad`, since that is where the
> tree is. **It would break the control that proved the tree correct.**
> `raster_attrgrad_dsp3_diff` drives `zhao_raster_attrgrad_v2` and
> `zhao_raster_attrgrad_dsp3` from one stimulus and compares them cycle by
> cycle; changing one side's depth invalidates the differential itself.
>
> `zhao_raster_attrgrad_dsp3.sv:134` instantiates **the same
> `zhao_raster_attrdiv_v2`**. So a change in the divider moves both sides
> identically and the differential survives. That is the argument that makes
> this safe, and it is not visible from the timing report.
>
> ##### And the split must go BEFORE the round-add, not after it
>
> Registering only the comparison leaves `attrgrad`'s tree (6.73) plus `Add0`
> (3.21) = **9.94 ns** in one cycle, which is the 10 ns period before skew or
> routing. The break has to be on `num_i` as it arrives.
>
> The divider already has the register to do it with — no new bank:
>
> ```
> D_IDLE   dividend_r <= sign-extended num_i          attrgrad tree -> reg   6.7
>          den_r <= area_i; final_err_r <= (area_i == 0)
> D_PREP   rounded = ROUND_NUM(dividend_r, den_ext)   reg -> add -> compare  7.1
>          dividend_r <= {rounded[96], rounded}
>          neg_r <= rounded[96]; final_sat_r <= sat_pos || sat_neg
> D_SAT    today's D_PREP body: err / sat -> D_DONE, else magnitude -> D_RUN
> ```
>
> One extra state, one extra cycle on a divide that already spends ~49 in
> `D_RUN`, and `dividend_r` is reused exactly as the existing comment describes
> it being reused ("this avoids a second 97-bit bank in all three lanes").
>
> **NOT IMPLEMENTED IN THIS PASS, and the reason is not caution about the running
> fits** — those snapshot their sources, as established twice today. It is that
> this restructures ratified arithmetic in a file carrying an exact-law
> negative-half mutant, an explicit *"do not simplify this back"* warning, and
> externally visible `q_saturated_o` / `saturations_o` behaviour. It deserves its
> own pass with the full mutant gauntlet rather than a hurried one at the end of
> a long session.
>
> ##### Its test impact, traced so the design is complete
>
> Unlike the `base_min_y0` split, this one DOES move an observable edge — it adds
> a state to every divide — and the tests already assert that exactly.
> `tests/raster/raster_attrgrad_v2_directed.cpp:134`:
>
> ```cpp
> const uint64_t expected_latency = exceptional ? 2u : kNormalVisibleLatency;
> if (cycles - accepted_cycle != expected_latency) fail("divider response-visible latency changed");
> if (dut->d_busy_clocks_o - busy_before != expected_latency) fail(...);
> ```
>
> So the change owes **both** constants: the exceptional path goes 2 → 3, and
> `kNormalVisibleLatency` goes +1. Two independent assertions check it — the
> observed cycle delta and the DUT's own `busy_clocks_o` counter — which is the
> counters-see-what-pictures-cannot discipline, and it means a mistake here
> cannot pass quietly.
>
> **And the DSP3 differential is the hard constraint, not the soft one.** That
> comparison is cycle-by-cycle and both implementations instantiate the SAME
> `zhao_raster_attrdiv_v2`, so the change moves both traces identically and the
> differential survives — the same argument that made today's tree change safe,
> verified today rather than assumed.
>
> The design above plus these two constants is a complete specification.
>
> ### `@packet-h-uvw` LANDED: 61.52 → 66.03 MHz, and area bought clock
>
> `status: ok`, clean tree, 1,536.4 s, single variable — only the `uvw_m` read
> register moved.
>
> | | predicted | actual | |
> |---|---|---:|---|
> | M10K | 134 → ~136 | **136** | exact |
> | ALM | −500 to −1,500 | **28,959 → 27,601** (−1,358) | in band |
> | setup TNS | −14,000 to −16,000 | **−8,851** | better than predicted |
> | Fmax | unchanged, ~61.5 | **66.03** | **wrong** |
> | `Info (276007)` for `uvw_m` | gone | **gone** | ✓ |
>
> **The binary assertion is conclusive.** `blockMemoryBits` 461,392 → 465,488 =
> **exactly +4,096**; registers 40,790 → 36,517 = **−4,273**; RAM blocks +2. The
> array moved into memory, and the composed shell now reports **no uninferred
> array at all** — `uvw_m` was the last one.
>
> #### The Fmax prediction was wrong, and I said in advance to look
>
> The note above reads: *"An Fmax jump would mean something other than the
> stated cause, and should be treated as a reason to look rather than to
> celebrate."* So: the binder moved from `attrgrad → attrdiv` at −6.254 to
> **`tile_pipe → attrgrad` at −5.144**, and `1/(10.000 + 5.144) = 66.04 MHz`,
> matching. The old binder improved below −5.144 **without being touched.**
>
> **The cause is congestion, not arithmetic.** Removing 4,273 registers and
> 1,358 ALMs from a design occupying two-thirds of the device gave the fitter
> room, and it placed the untouched paths better. The whole table shifted down:
> the worst family is now `aux_pipe → island` at −3.280 across 305 paths, and
> median bad-path delay fell 12.510 → 11.578 ns.
>
> **So the prediction was wrong because it assumed placement is independent of
> area.** At this occupancy it is not, and that is the generalisable lesson:
> *"solve it with memory"* buys clock as well as ALMs, by a second mechanism
> that has nothing to do with the path being fixed. The three-fit sequence shows
> it cleanly — 54.12 → 61.52 with three arithmetic changes, then 61.52 → 66.03
> with one change that touched no arithmetic at all.
>
> #### Where the composed shell stands, and what now blocks it
>
> Four changes, three fits, one day:
>
> ```
> @packet-h-m10k    29,044 ALM   54.12 MHz   TNS -29,689   134 M10K
> @packet-h-timing  28,959 ALM   61.52 MHz   TNS -19,985   134 M10K
> @packet-h-uvw     27,601 ALM   66.03 MHz   TNS  -8,851   136 M10K
> ```
>
> **ALM −1,443, Fmax +22%, TNS −70%**, and the shell is now 2,399 inside its
> 30,000 budget with no uninferred array anywhere in it.
>
> The remaining families, worst first by total:
>
> | from → to | paths | worst |
> |---|---:|---:|
> | `aux_pipe` → island | 305 | −3.280 |
> | `binding_resolver` → itself | 278 | −2.432 |
> | `v3own` → itself | 175 | −3.901 |
> | `rsp_dispatch` → island | 138 | −2.677 |
> | `frag_expand` → `video_slotmgr_v2` | 122 | −3.571 |
> | **`tile_pipe` → `attrgrad`** | **44** | **−5.144** |
>
> **The binder is now the MULTIPLIER**, not the adder tree or the divider:
> `base_min_y0_c = job_n0_i + dndx·job_min_x_i + dndy·job_tile_y_i + …` maps to a
> DSP macro (`Mult0~mult_h_mult_hlmac`) feeding a long soft carry chain for the
> partial-product sum — 14.24 ns, launched from the multiplier's own input
> register.
>
> **And every remaining move in that family needs `attrgrad`'s pipeline depth to
> change**, which is the constraint recorded above: `raster_attrgrad_dsp3_diff`
> compares the two implementations cycle by cycle, so one side moving alone
> invalidates the control that proved today's tree change correct.
>
> **That is workable and was checked rather than assumed.** Both
> `zhao_raster_attrgrad_v2` and `zhao_raster_attrgrad_dsp3` are
> `not-yet-adopted` in `prod_manifest.yml` — neither is in the selected machine
> yet — so **both can take the same stage in one change** and the differential
> stays cycle-aligned. It is a two-module change plus the exact-count test
> updates, and it is the next piece of RTL work rather than an obstacle.
>
> ##### And the stage is FREE, which changes the difficulty entirely
>
> Read out of the state machine rather than assumed. `base_min_y0_r` is loaded
> in `S_IDLE` on job acceptance:
>
> ```systemverilog
> S_IDLE: if (job_valid_i && job_ready_o) begin
>   base_min_y0_r <= base_min_y0_c;      // two 96-bit multiplies + four adds
>   st_r          <= S_GRAD_REQ;
> ```
>
> and **it is not read until `S_ROW_REQ`** — through `S_GRAD_REQ`, `S_GRAD_WAIT`
> and the gradient divide, which alone spends about fifty cycles in `attrdiv`'s
> `D_RUN`. The value has enormous schedule slack.
>
> So splitting it costs nothing:
>
> ```
> S_IDLE      mul_x_r <= dndx_in_c * job_min_x_i        multiply alone
>             mul_y_r <= dndy_in_c * job_tile_y_i
> S_GRAD_REQ  base_min_y0_r <= n0_r + mul_x_r + mul_y_r
>                            + (dndx_r >>> 1) + (dndy_r >>> 1)
> ```
>
> **No state is added, no cycle is spent, and no output edge moves** — the
> result is simply ready one cycle into a wait that already lasts fifty. Which
> also means **the differential is safe**: nothing observable changes, so the
> cycle-by-cycle comparison sees the same trace.
>
> Cost is two 96-bit registers per lane, 576 flops across the three lanes, which
> is noise against the 27,601 ALM the shell now measures — and the shell just
> demonstrated that removing flops from a congested design *buys* clock, so even
> that may not read as a cost.
>
> **This supersedes the paragraph below it.** The pipeline stage was recorded as
> needing both differential implementations to change together; that is true of
> a stage that moves an output edge, and this one does not. It is the cheapest
> remaining move on the binder and should be the next RTL change.
>
> ##### LANDED, and the prediction for `@packet-h-mulstage`
>
> The split is in: the two multiplies land in `S_IDLE`, their sum in
> `S_GRAD_REQ`. Three 96-bit registers per lane — `mul_x_r`, `mul_y_r`, `n0_r` —
> so 864 flops across the shell's three `g_attr` instances.
>
> **The differential proves the central claim rather than arguing it.**
> `raster_attrgrad_dsp3_diff` compares this module against the untouched
> `zhao_raster_attrgrad_dsp3` cycle by cycle and passes, which is exactly the
> evidence that no observable edge moved. 11/11 including all seven DSP3
> controls.
>
> **Prediction, written before the fit:**
>
> * **Fmax ≈ 70–72 MHz.** The binder was `tile_pipe → attrgrad` at −5.144 and
>   this removes its 14.24 ns cone; the next candidates already on the table are
>   `v3own → v3own` at −3.901 and `frag_expand → video_slotmgr_v2` at −3.571, so
>   the worst should land near −3.9 and `1/(10 + 3.9) = 71.9 MHz`.
> * **TNS down again**, though by less than last time: the attrgrad family is
>   only 44 paths of 2,000, so the gain is depth rather than population.
> * **ALM roughly flat, ±400.** 864 added flops is about 0.9 ALM-equivalents per
>   flop at this design's packing, but `@packet-h-uvw` just demonstrated that
>   flop count and placement quality interact, so the sign is genuinely
>   uncertain. **If ALM falls, that is congestion again and not this change
>   being free.**
>
> The honest risk: if Fmax lands near 66 again, the multiply was not the whole
> cone and the DSP macro's own latency dominates — in which case the next move
> is Quartus's DSP output register rather than another RTL stage.
>
> **And the fit is genuinely single-variable, checked rather than assumed.**
> Diffing the two snapshots' commits over `fpga/rtl/` shows four files moved, but
> three are the two instrument wrappers and a provenance comment — and the only
> generated file inside the 97-source closure is `zhao_abi_pkg.sv`, which is
> neither. So of the sources this fit actually compiles, **exactly one changed**:
> `zhao_raster_attrgrad_v2.sv`. Whatever the number does is attributable to the
> multiply split and to nothing else.
>
> ##### A clarification, and an unfixed twin
>
> The tree change could never have broken the differential, and it is worth
> being exact about why: replacing a serial accumulate with a balanced tree is
> **combinational restructuring inside one cycle**. It moves no edge, so the
> cycle-by-cycle comparison is blind to it by construction. A PIPELINE STAGE is
> a different kind of change and is what the differential would catch. The
> earlier note treated the two as one risk; only the second is real.
>
> **And `zhao_raster_attrgrad_dsp3.sv:100–102` still carries the serial chain:**
>
> ```systemverilog
> row_offset_c = 96'sd0;
> if (row_r[0]) row_offset_c = row_offset_c + dndy_r;
> ...
> ```
>
> Byte for byte the arrangement that cost 2.2 ns in the V2. It is not in the
> composed shell — the shell elaborates the `g_v2` branch — so it costs nothing
> measured today, and it is **exactly the shape this repository calls an
> uncashed cheque**: a known defect left in an unadopted sibling, which arrives
> as a surprise on the day the sibling is adopted.
>
> **FIXED the same day rather than left recorded.** The first version of this
> paragraph argued for deferring it, on the grounds that fixing an unmeasured
> path is how a bounded change becomes a campaign. That was the wrong reading of
> a good rule: the change is the same twelve lines as the V2's, it is
> combinational and therefore differential-blind, the file is already inside the
> shell's 97-source closure, and `dsp3` is `not-yet-adopted` rather than
> protected. Writing down "there is a known defect here" and leaving it is the
> failure mode, not the discipline. 11/11 with both implementations
> restructured, including all seven DSP3 mutant controls.
>
> #### The prediction for `@packet-h-uvw`, written before it starts
>
> Single variable: the `uvw_m` read register moved out of the asynchronously
> reset block. Nothing else changed between `c3ae9b76` and this fit.
>
> * **Fmax: roughly unchanged, about 61.5 MHz.** The binder is
>   `attrgrad → attrdiv` at −6.254 and this change does not touch it. An Fmax
>   jump would mean something other than the stated cause, and should be
>   treated as a reason to look rather than to celebrate.
> * **TNS: down substantially.** The family carries −5,493.5 of the −19,984.6
>   total, so if `uvw_m` infers, most of that should leave — call it **−14,000
>   to −16,000**.
> * **ALM: down, by roughly 500–1,500.** 4,096 bits leave the fabric along with
>   a 64-entry read mux. The resolver's 38,400-bit move is not a usable rate
>   here — it was pathological — so this is an estimate with a wide band.
> * **M10K: 134 → about 136.** 4,096 bits is under one block's worth, but the
>   64 × 64 shape will not pack into one.
> * **The map report's `Info (276007)` line for `uvw_m` should be GONE.** That
>   is the assertion this fit exists to test, and it is binary.
>
> If `uvw_m` still appears as uninferred, the change did not work and the ALM
> and TNS movements — whatever they are — are not evidence that it did.
>
> #### And it is now 84% of everything
>
> 1,689 of 2,000 negative paths end in `uvw_m`, whose read register is still
> reported uninferred by this fit's own map report (`Info (276007)`) — **exactly
> as recorded in advance**, because the fix landed after the snapshot. The next
> composed fit is therefore a clean test of a single change against a family
> that dominates the remaining list.
>
> **`uvw_m` IS NOT IN THIS FIT.** `@packet-h-timing` snapshotted at source digest
> `c3ae9b76`, and the `uvw_m` read was moved afterwards. So the row that lands
> measures the attrgrad tree, the stored legality verdict and the CRC verdict
> state — and **not** the island change. Reading its RAM summary and finding
> `uvw_m` still uninferred would therefore mean nothing about whether that fix
> works; it is simply not in the design that was compiled.
>
> That needs saying in advance because a number arriving after four changes will
> be attributed to all four by anyone who did not check the digest, and the
> digest is right there in the row.
>
> *(The pattern is worth naming, because it has now happened three times in this
> one section: group by module, find one expensive thing, attribute the whole
> group to it. Grouping by module said "the boundary dominates"; grouping by
> source leaf said "the resolver dominates"; grouping by source module again
> said "the legality cone is 515 paths". Only the DESTINATION SIGNAL told the
> truth each time, and it was in the same report throughout.)*
>
> ##### The CRC scan, decomposed — the next change, specified before it is made
>
> ```
> 8.208   portbdataout[24]                 the RAM, again ~0.2 ns
> 13.072  Mux10~4/5/6                      +4.86  byte select, 80:8 variable
> 13.631  crc~3                            +0.56  one crc32_byte round
> 17.121  Equal35~5 / ~14 / ~14_RESYN      +3.49  the 32-bit seal compare
> 21.438  page1_valid_q[64]~1 -> page0_valid_q[86]~78   +4.32  clear fanout
> 21.700  page0_valid_q[86]                       13.69 ns of data path
> ```
>
> Four stages, all in ONE cycle:
>
> ```systemverilog
> wire [7:0]  crc_byte_c  = crc_canonical_row_c[crc_byte_q*8 +: 8];
> wire [31:0] crc_step_c  = crc32_byte(crc_q, crc_byte_c);
> wire [31:0] crc_final_c = crc_step_c ^ 32'hFFFF_FFFF;
> ...
> if (crc_final_c == crc_expected_q) loader_state_q <= LOAD_SEAL_PENDING;
> else begin page0_valid_q <= '0; ... end
> ```
>
> **This walk is already byte-serial** — the comment above it records that a
> previous pass removed "the measured ten-byte combinational chain" — so it
> already spends 256 × 10 cycles at page-load time, entirely off the render
> path. **Another one or two cycles there cost nothing measurable.** The obvious
> split is a register between the byte select and the CRC step, which separates
> the 4.86 ns mux from the 8.4 ns of CRC + compare + clear; neither half is over
> the 10 ns period.
>
> It is a state-machine change rather than an expression change, so it is the
> riskiest of the three and wants the seal tests and the `stale_invalid_crc`
> mutant watching it. It is also the largest single lever left: **512 of the
> 2,000 worst paths.**
>
> **LANDED the same day.** `LOAD_CRC_CHECK` now holds the verdict: the last byte
> folds into `crc_q`, and `(crc_q ^ 32'hFFFF_FFFF) == crc_expected_q` is taken
> on the next edge, so the RAM-to-CRC half ends at a flip-flop and the
> compare-and-clear half starts at one. `binding_crc_busy_o` covers the new
> state, so a consumer waiting for busy to fall still sees it fall exactly once.
> The seal VALUE is unchanged — `crc_q` takes exactly the `crc_step_c` the old
> code compared — so the bytes folded, their order and the accept/refuse outcome
> are identical; only the edge the outcome is acted on moves.
>
> **The seal tests caught it by exactly one cycle, which is the point of
> them.** `expected 0xA01, got 0xA02` and `0x9FD → 0x9FE`. Both were updated
> with their NAMES rather than only their constants: *"BAD_CRC response lands on
> final serialized byte"* had become false, and the scan count is now spelled
> `1 + 256*10 + 1` so a re-collapsed fold still drops it by thousands and the
> check still fires. 8/8 including all seven mutant controls.
>
> **That is 61–62, not 100.** The remaining gap is the CRC scan (512 paths), the
> divider's own 96-bit compare chain (4.45 ns on the attrgrad path alone), and
> the two island families. Anyone reading a single improved number as "closure
> is near" is reading it wrong.
>
> #### And 29,044 ALM is NOT a whole-machine number
>
> The receipt reads *"29,044 of 41,910 — budget 30,000 — INSIDE"*, which invites
> the reading that area is solved with 956 ALM to spare. Counting the target's
> own 97 sources by directory:
>
> ```
> raster 25 · texture 20 · video 14 · geometry 13 · common 9 · memory 6
> debug 3 · command 2 · input 2 · generated 1 · field 1 · audio 1
> ```
>
> **There is no `terrain/` in the list.** The composed shell is the shell and the
> render path; terrain is not in it. Combining the two is precisely what G8C is
> for, so the 956 ALM of headroom is headroom against a machine that is not yet
> whole — and the largest single breach in the scoreboard, *projection and
> result arenas* at 12,267 ALM and 66 DSP, is two projector instances that the
> source already deduplicated into `zhao_project_core` and the silicon still
> carries twice.
>
> The closure target is the WHOLE console under 30,000 ALM and 85 DSP at
> 100 MHz. On this evidence the honest status is: **area is inside budget for
> the part that has been composed, the clock is not, and the part that has been
> composed is not the whole.** Quoting 29,044 without that third clause is the
> same error as quoting `status: ok` without reading what the target's rules
> actually check.
>
> #### THE AREA PROBLEM AND THE CLOCK PROBLEM ARE THE SAME PROBLEM
>
> This is the most useful thing the receipt says, and it only appears once the
> remaining families are resolved to their endpoint SIGNALS rather than their
> modules. Three of the four have one shape:
>
> | family | paths | from → to | worst |
> |---|---:|---|---:|
> | `binner_v2` → island | 206 | `d_meta_r[97]` → **`uvw_m~*`** | −4.475 |
> | `v3own` → itself | 276 | `c3t_v_q` → **`clm_q[0..47][2:0]`** | −5.221 |
> | resolver | many | RAM enable → **`page0_valid_q[*]`** | −6.230 |
>
> Every one is **one register driving an array of flip-flops**. All 206 of
> the binner family land on `uvw_m` — and `uvw_m` is the ONE array in the entire
> composition that Quartus still reports as uninferred, 4,096 bits in fabric at
> `zhao_texture_island_v3_top.sv:916`.
>
> **`clm_q` is NOT the same case, and the sentence that follows was written
> before that was checked.** It is `logic [3:0] clm_q [OWNERS]` — 64 × 4 bits =
> **256 bits**, a twentieth of `uvw_m` — and its source `c3t_v_q` reaches it
> through `fwd_t_hit_c`, an associative forwarding compare against every entry's
> slot and generation. A memory has no read port that answers "which entry
> matches", so relocating it is not the lever; the lever there is the compare
> depth. Grouping it with `uvw_m` on endpoint SHAPE was the same mistake as
> grouping timing families by module instead of by signal, one level down.
>
> **That changes what the `uvw_m` owner decision is about.** It has been docketed
> as an area question — 4,096 bits that could be M10K, blocked because the file
> sits in Packet D's `PROTECTED_HASHES`. It is also the destination of **all 206
> paths** in the third largest timing family. Moving it is worth ALM *and* ns.
> It is written once per accepted admission (`uvw_m[own_adm_owner_w[13:8]]`) and
> read from a registered owner slot, which is the single-registered-read shape
> that infers — so the question for the owner is narrow.
>
> This is the standing owner direction landing from an unexpected direction:
> *"we have lots of M10K, ALMs are over budget, what you can you need to solve
> with memory."* The instruction was given as a way to buy AREA. On this
> evidence it buys CLOCK as well, because a wide array in fabric is both a pile
> of ALMs and a high-fanout net into thousands of enables. The binding
> resolver's own history is the proof: its banks left fabric and its area fell
> from 28,957 ALUT to something that fits — and what was left on its critical
> path was never the memory, it was the arithmetic.
>
> **So the campaign after the current re-fit is: `uvw_m` first**, and then the
> forwarding compare in `v3own` — a different problem needing a different fix.
>
> ##### `uvw_m`: the change, read out of the source and ready to make
>
> Traced 2026-09-18 while the re-fit ran. The array is written once and read
> once, which is already the shape that infers:
>
> ```systemverilog
> logic [63:0] uvw_m [0:OWNERS-1];               // 916  64 x 64 = 4,096 bits
> always_ff @(posedge clk)                       // 917  write: no reset
>   if (own_adm_accept_w)
>     uvw_m[own_adm_owner_w[13:8]] <= {frag_u_over_w_i, frag_v_over_w_i};
>
> always_ff @(posedge clk or negedge rst_n) begin // 980  READ lives in here
>   if (!rst_n) begin
>     rcp_head_valid_q <= 1'b0;
>     persp_prep_valid_q <= 1'b0;                 // and NOT the read target
>   end else if (persp_prep_room_c) begin
>     if (rcp_head_valid_q)
>       {persp_prep_uow_q, persp_prep_vow_q} <= uvw_m[uvw_read_owner_c[13:8]];
> ```
>
> **The read register sits in an asynchronously-reset block**, and an M10K output
> register cannot have an asynchronous reset — which is what costs the
> inference, exactly as this section predicted before the source was opened.
>
> The preconditions for moving it were checked rather than assumed:
> `persp_prep_uow_q` and `persp_prep_vow_q` are declared at 961, assigned at
> **line 990 and nowhere else**, consumed at 1020, and **have no assignment in
> the reset branch** — so relocating them into their own `always_ff @(posedge
> clk)` with the same `persp_prep_room_c && rcp_head_valid_q` enable changes no
> behaviour, adds no latency, and loses no reset that exists today.
>
> **LANDED, and the reason I first gave for deferring it was wrong.** I wrote
> here that the change had to wait for the running fit, because the file is in
> that fit's 97-source closure and moving the tree underneath it "risks the
> receipt being stamped `rtlCleanAtHead: false`". Checked instead of asserted:
> `run_block_fit.ps1` captures `$head` at line 214, `$treeClean` at 229 and
> `$rtlClean` at 231 — all at script start, hundreds of lines before the fit
> loop that writes the row. The tree was clean when the fit began, so the row's
> provenance was fixed before the first edit and nothing done afterwards can
> spoil it. The sources are snapshotted besides.
>
> That is the caution the fit-guard hook exists to refuse, and its warning names
> the mechanism exactly: *an agent that believes the whole RTL tree is frozen
> for four hours will invent reasons to avoid the work it should be doing.* The
> invented reason was plausible, specific, and cost most of a fit's worth of
> working time before it was checked.
>
> ##### And do NOT build a source sweep for this: Quartus already answers it
>
> The obvious follow-up is to sweep the closure for other arrays read inside an
> asynchronously-reset `always_ff`, since that is the defect CLASS rather than
> one site. It was written, and it is the wrong instrument. It reports dozens of
> hits — `fifo_tag_q`, `v_q`, `c1_tag`, `rq_en` — and almost all are small
> pipeline-stage or lane arrays that belong in flops and should stay there, so
> the output needs a size filter it cannot compute and a judgement it cannot
> make.
>
> **The composed map report already names every uninferred array by name**, and
> on `@packet-h-m10k` it named exactly one: `uvw_m`. That is an authoritative
> census from the tool that decides the question, against a heuristic that
> guesses at it from syntax. The right habit after each composed fit is to
> re-read the RAM summary and the `Info (276007)` lines — which also means the
> next fit will say whether this change worked and whether anything new
> appeared, without anyone writing another scanner.
>
> ##### `v3own` is NOT the next cheap win, and that is worth saying in advance
>
> Its family is 276 paths at −5.221, `c3t_v_q → clm_q[0..47][2:0]`, and the shape
> looks inviting: one valid bit into an array. It is not a deep arithmetic chain
> — `fwd_t_hit_c` is a shallow compare on slot, generation and a ticket mask —
> so the cost is the compare plus the acceptance logic plus a 64-way write
> decode, and the obvious fix is to pipeline the forward decision.
>
> **That decision is the T2 lifetime contract.** The comment above it cites the
> owner's ruling directly — *"an owner's authority ends at its ordered external
> output transfer, and matching a slot's residual generation bits does not
> extend the owner's authority after retirement"* — and S8.2 requires C2 to
> check snapshot identity AND current full-ticket membership, with S8.1 giving
> a counterexample for each half. Adding a cycle to a forwarding path whose
> whole purpose is to be exact *within one cycle* is a change to ratified
> behaviour, not a restructuring.
>
> So the ordering is: `uvw_m` (mechanical, no behaviour change), then measure
> again, and only then decide whether `v3own` is worth opening — with the
> contract in hand and the owner's ruling read first.
>
> #### And the whole-machine numbers rest on rows that have gone stale
>
> Run 2026-09-18, `tools/budget/uncashed_cheques.py` check 2 — *the repair
> landed, the receipt did not*. A sample of what it lists:
>
> ```
> zhao_project_core     row asserts 33 DSP   file moved 23.9d AFTER the fit
> zhao_geom_pose_decode row asserts 18 DSP   file moved 17.3d after
> zhao_terrain_normals  row asserts 18 DSP   DIRTY TREE, moved 18.1d after
> zhao_geom_cull        row asserts 15 DSP   file moved 18.3d after
> zhao_geom_mat3x4_mul  row asserts  9 DSP   file moved 17.3d after
> ```
>
> `zhao_project_core` matters most: the *projection and result arenas* domain is
> the largest breach in the scoreboard at 12,267 ALM and 66 DSP, and it is two
> instances of this block — so **half that number comes from a row describing a
> file that changed three weeks later**, and several of these were taken from a
> dirty tree besides, where the digest describes nothing.
>
> This does not make the scoreboard useless and it is not an argument that the
> machine is secretly smaller. It means the 173-DSP and 40,591-ALM totals are
> **an estimate built partly on expired receipts**, and that the re-measurement
> is itself a piece of work with a cost — which belongs in the plan rather than
> being discovered when a closure claim is challenged. `tools/budget/
> dsp_census.py` says so in its own header (*"STALENESS NOT CHECKED"*); this
> section is here so the roadmap says it too.
>
> *(The first two attempts at this table were wrong, and both in the flattering
> direction. One reported every family ending at `gpu_clk`, which reads as "the
> virtual-pin boundary dominates" and excuses the design; the other concluded
> from a truncated header that the report had no destination column at all and
> grouped on sources alone. The report has eight `;`-separated fields. Split on
> the delimiter, do not pattern-match a line whose shape you guessed.)*
>
> ### BUILT AND MEASURED, same day. The trade is real.
>
> | | before | after | change |
> |---|---:|---:|---:|
> | **ALMs needed (A&S estimate)** | 62,534 | **31,589** | **−30,945** |
> | registers | 80,173 | 41,526 | −38,647 |
> | block memory bits | 422,480 | 460,880 | **+38,400** |
> | DSP blocks | 63 | 63 | — |
>
> The memory grew by **exactly 38,400 bits** — the page table, relocated, not
> re-estimated. Quartus reports `altsyncram:page0_m_rtl_0` and
> `page1_m_rtl_0`, so both banks inferred.
>
> **The composed shell went from 149% of the 41,910-ALM device to 75% of it**,
> and to within 5% of the whole-machine 30,000 target — from a design that
> could not be placed to one with room. One block, one structural change, no
> architecture change, and the owner's standing direction is what named it.
>
> `texture_binding_resolver_v2_directed` and all SEVEN committed mutant
> controls pass: late page generation, early activation while held, active bank
> write, stale invalid CRC, witness class route, same-cycle refusal, refusal
> without issue.
>
> **STILL AN ESTIMATE, AND STILL VIRTUAL-PIN.** 31,589 is Analysis & Synthesis
> before placement, not a fitter ALM count, and the row carries virtual pins
> which inflate. Both point the same way — the real number should be lower —
> but the fitter has not yet produced one, so this is not a receipt.
>
> **ONE ARRAY IS STILL IN FABRIC, and here is its size.** The same run reports
> `uvw_m` in `zhao_texture_island_v3_top.sv:916` uninferred for the same reason.
> It is `logic [63:0] uvw_m [0:OWNERS-1]` with `OWNERS = 64` — **4,096 bits**,
> a ninth of the binding banks, with one conditional write (line 919) and one
> registered read (line 990).
>
> One write and one read is already the inferrable shape, so the blocker is
> narrower than the binding banks' was: the read lands in an `always_ff` whose
> reset branch clears its destination registers, and a reset on a RAM output
> register is one of the things that costs the inference. That is a smaller
> change than the one made here — it does not need an address mux, only the
> reset moved off the read path.
>
> **It is worth doing and it is not editable work.** `zhao_texture_island_v3_top.sv`
> IS in Packet D's `PROTECTED_HASHES`. A 64-entry read mux over 64 bits is
> perhaps 1–2k ALUT plus 4,096 registers — small against what was just
> recovered, and NOT small against the 1,589 ALUT that currently separate this
> composition from the 30,000 target. **Owner decision, with the number
> attached rather than a hunch.**
>
> **AND IT IS THE ONLY ONE LEFT**, which is worth stating because it bounds the
> lever. Cross-referencing the composed map's own RAM-inference messages
> against every file the fit compiles: `uvw_m` is the single array Quartus
> still reports `uninferred` in the whole composition. Eleven files in the cone
> declare 8 Kbit or more — `zhao_audio_fifo` 65,536, `zhao_geom_binner_v2`
> 48,856, `zhao_cmd_dma` 33,280, `zhao_raster_tilestore` 32,768,
> `zhao_texture_palette_res_v2` 16,420 and the rest — **and every one of them
> infers.**
>
> So after this change the memory-for-ALM trade is spent for this composition
> apart from 4,096 bits behind an owner decision. Further ALM reduction has to
> come from LOGIC, which is what the candidate list below is for, and
> `zhao_raster_edgewalk` at 3,350 ALUT against 2 DSP is the right shape for it.
>
> ### And the distribution afterwards, which is where the next work goes
>
> The block itself went **28,957 → 2,770 ALUT** and **39,449 → 901 registers**.
> What is left has no dominant consumer, which is a different and healthier
> problem than the one before:
>
> | leaf | ALUT | registers |
> |---|---:|---:|
> | `zhao_texture_v3own` | 3,745 | 2,604 |
> | `zhao_raster_edgewalk` | 3,350 | 934 |
> | `zhao_texture_binding_resolver_v2` | 2,770 | 901 |
> | `zhao_cmd_dma` | 2,361 | 1,141 |
> | `zhao_raster_attrgrad_v2` x3 | ~1,530 each | 734 each |
> | `zhao_geom_binner_v2` | 1,406 | 2,008 |
>
> `zhao_raster_edgewalk` is now the largest single leaf, and the candidate list
> below already names it: *"the largest zero-memory block; its factored-row
> proposal is unimplemented."* It is also the right SHAPE for the owner's
> direction — 3,350 ALUT against 2 DSP — where the rows above it that carry
> nine to eighteen DSPs are not.
>
> **`zhao_geom_binner_v2` is worth a second look for the opposite reason.** It
> declares 48,856 array bits and shows 2,008 registers, so those arrays already
> infer — 2,008 flip-flops cannot hold 48,856 bits. It is on the no-fit-row
> list and is NOT a gap.
>
> **AND THERE IS A TRAP IN IT, which is this repository's most-cited defect
> wearing the opposite sign.** Both reads are CONDITIONAL today:
>
> * the CRC walk loads `crc_row_q` only `if (!crc_have_row_q)`, holding the row
>   while its ten bytes fold;
> * the data plane loads `read_row_q` only on an accepted, unrefused request,
>   and writes `'0` otherwise.
>
> The obvious way to make an array infer as RAM is to register its read
> unconditionally — and *"A detector wired to two operands that move together
> cannot fire"* in CLAUDE.md is the post-mortem of doing exactly that to a
> metadata bank in this same family: *"The bank registered its read
> UNCONDITIONALLY, so its output tracked whatever address was being OFFERED
> while the stage downstream held the previous response,"* producing response
> A's data with B's metadata and every counter balancing.
>
> So the restructure must keep the hold, and it can: M10K supports a read
> ENABLE, and `if (read_en) rd_q <= page_m[addr];` still infers. The two
> readers' enables are mutually exclusive by the same invariant, so one enable
> per array is exact.
>
> The `'0` write on refusal cannot stay — you cannot write a constant into a RAM
> read register without losing the RAM — and it does not need to: the refusal
> path already clears `read_row_present_q`, which is what consumers should be
> gating on. **Moving that zeroing is the part to review hardest**, because a
> consumer that reads the row without checking `present` would go from seeing
> zeros to seeing a stale row, which is the same defect again by a different
> route.
>
> `zhao_texture_binding_resolver_v2.sv` is **not** in Packet D's or Packet E's
> `PROTECTED_HASHES`, so this is editable work rather than an owner decision.

Generated 2026-09-16 from `reports/synthesis/zhao_block_fit.json`, unlabelled
fitted rows only. **51 fitted blocks report zero M10K**, and they carry
39,121 ALM between them.

**Read that total carefully; it is a sorting aid, not a budget.** It
double-counts seed variants of the same block (`zhao_raster_rcp24_svc`,
`…seed2`, `…seed3` are one design measured three times), it overlaps composed
roots that contain their own children, and several rows are stale — the two
33-DSP projector rows below are the pair the shared G8B group already replaced
at 34 DSP total. What the list is good for is pointing at *where the
conversion candidates are*, which is what the direction needs.

| block | ALM | DSP | why it is a candidate |
|---|---:|---:|---|
| `zhao_geom_skin` | 2,225 | 9 | matrix-vector products; R5. A quarter-square byte multiply removes the DSPs and the LUT multiplier both |
| `zhao_geom_cull` | 1,102 | 15 | the highest DSP density in the tree with no memory at all |
| `zhao_raster_rcp24_svc` | 1,041 | 6 | a reciprocal, which is the canonical table lookup |
| `zhao_texture_material_combine_v1` | 1,663 | 2 | R4 names this one explicitly: *"the promised quarter-square/coefficient-memory replacements are absent"* |
| `zhao_terrain_lod` / `zhao_geom_lod` | 1,759 / 1,183 | 3 / 6 | signed 33x32, structurally 6->3 per the implications below |
| `zhao_probe_dist_svc` | 1,745 | 0 | a distance service with no memory; whatever root it computes is a table |
| `zhao_raster_edgewalk` | 2,286 | 2 | the largest zero-memory block; its factored-row proposal is unimplemented |

**`zhao_qsq_bytemul` is the instrument for most of that column**, extracted
from `zhao_terrain_shade` on 2026-09-16: one M10K, no DSP, exact for all 65,536
byte pairs, with the sweep driven through the RTL rather than argued. Every row
above that has a DSP count can be asked the same question against it.

**Three cautions that belong with the list.** Not every block should hold
memory — a small table read on several ports in one cycle is correctly logic.
An M10K read is ~2 ns against a flip-flop's ~0.3, so a lookup on a block's
critical path can cost more than it saves. And the M10K allocations are
per-domain and one domain is already over; see the direction above.

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

---

## THE COMPOSED SHELL HAS NO GEOMETRY FRONT END, AND ITS SOURCE LIST SAYS IT DOES

Found 2026-09-18 while `@packet-h-satstage` ran, by asking a question nobody
had asked of the receipts: *which of the 97 declared sources actually
elaborate?*

`zhao_shell_top_v2`'s closure in `design/fit_targets.yml` declares 97 files.
Cross-checking them against the Compilation Hierarchy Node table of
`@packet-h-mulstage`'s own map report, **eight geometry blocks are declared and
instantiated nowhere in the synthesis**:

| declared in the closure | instances in the shell synthesis | ALUT in the whole machine |
|---|---:|---:|
| `zhao_geom_project` | 0 | 10,263 |
| `zhao_geom_clip` | 0 | 714 |
| `zhao_geom_assetfetch` | 0 | 687 |
| `zhao_geom_depthquant` | 0 | 539 |
| `zhao_geom_setup` | 0 | 463 |
| `zhao_geom_meshfetch` | 0 | 302 |
| `zhao_geom_assemble` | 0 | 233 |
| `zhao_geom_vdecode` | 0 | 128 |
| **total** | **0** | **13,329 ALUT ≈ 8,450 ALM** |

Positive control, same instrument, same report: `zhao_geom_bin_pipe_v2` 9,842
instance mentions, `zhao_texture_island_v3_top` 6,563, `zhao_raster_attrgrad_v2`
105. The instrument fires; the eight are absent.

**So 27,231 ALM is the render BACK end.** The shell starts at the binner. There
is no vertex decode, no mesh fetch, no assembly, no setup, no projection, no
clip and no depth quantisation inside that number — and `zhao_geom_project.sv`
being listed among its sources is exactly what would persuade a reader
otherwise. Every place this document compares 27,231 against a 30,000 budget is
comparing against a part, and the honest form of the sentence is *"the back end
is 2,769 inside budget with the front end still outside the measurement."*

`zhao_project_core.sv` is **not** in the closure at all, which is the structural
proof rather than an inference: `zhao_geom_project` instantiates it at line 142,
so if the block elaborated, the fit would have failed `MODMISSING`. It fit
clean, three times.

### The second consequence is a live-tree hazard for no benefit

`QUARTUS_GOTCHAS.md` §11 forbids editing a file inside a running fit's closure,
and the Stop hook enforces it. A DEAD closure entry therefore freezes a file the
fit does not measure. That is not hypothetical: the projector-sharing work —
**the single largest overrun in the machine at 3.8× its envelope** — was picked
up during this fit, found to need `zhao_geom_project.sv`, and correctly put down
again, for a file contributing nothing to the measurement in progress.

### What it does not say

This is about the V2 shell's closure, not about `zhao_prod_top`, which
instantiates all eight through `zhao_shell_top` (V1). The machine has a geometry
front end; the sibling shell's composition does not yet reach it. Whether that is
a deliberate staging (the V2 shell is being composed outward from the renderer)
or an oversight is a question for whoever wrote the closure — but either way the
source list should not assert a block that never elaborates.

### Owed

1. Either remove the eight from the `zhao_shell_top_v2` closure, or mark them
   `# declared, not yet composed` so the list stops reading as a measurement
   claim. **Do this when the running fit ends**, not during it.
2. Re-state the closure arithmetic wherever 27,231 is compared to 30,000.
3. Cash the projector cheque — `zhao_geom_project` and `zhao_terrain_project`
   hold one `zhao_project_core` each, 6,908 and 6,883 ALUTs of *own* logic
   apiece, nearly identical, which is the duplication measured rather than
   argued.


---

## THE PROJECTOR CHEQUE IS NOT UNBUILT. IT IS BUILT, COMPOSED AND MEASURED — AND PRODUCTION STILL CARRIES TWO

Found 2026-09-18, immediately after the closure finding above, by applying
CLAUDE.md's own rule before starting work: *grep the tree for the thing it
replaces.* The thing it replaces exists, and so does the thing after that.

```
zhao_terrain_pipe  ->  zhao_proj_subsystem  ->  zhao_project_service  ->  ONE zhao_project_core
      (G8B's fitted top, 102.19 MHz, physical pins, zero TNS)
```

Verified in G8B's own map report (`@g8b-t11-pins-s2`), instance counts:
`zhao_proj_subsystem` 4,177, `zhao_project_service` 3,971, `zhao_project_core`
3,964, **`zhao_terrain_project` 0**. The receipt this project already holds for
terrain is a receipt for the SHARED arrangement.

### The two arrangements, side by side, both measured

| | production today (`zhao_prod_top` map) | G8B (`@g8b-t11-pins-s2`) |
|---|---:|---:|
| `zhao_geom_project` → its own core | 10,220 ALUT (own 6,908) | — |
| `zhao_terrain_project` → its own core | 14,179 ALUT (own 6,883) | — |
| one shared `zhao_project_core` | — | 8,694 ALUT (own 7,932), 24 M9K, 0 DSP |
| **total projection silicon** | **24,399 ALUT ≈ 15,470 ALM** | **8,694 ALUT ≈ 5,512 ALM** |

The two production cores' OWN logic is 6,908 and 6,883 — within 0.4% of each
other. That is the duplication measured rather than argued: not two blocks that
happen to both project, but one circuit instantiated twice.

### What would make this number smaller than it looks, stated before anyone quotes it

**In G8B's fit top, client A is not driven.** `zhao_proj_subsystem`'s header is
explicit — *"client A raw — the geometry producer is not composed anywhere in
this tree"* — so Quartus may have folded away arbitration and buffering that a
two-client service has to carry. 8,694 ALUT is therefore a **floor**, not the
delivered cost, and the honest claim today is *"one core plus an unmeasured
arbitration increment"*, not a 9,957-ALM saving.

**`zhao_project_service` has a `- top:` entry in `fit_targets.yml` (line 527)
and has never been fitted** — `zhao_block_fit.json` holds no row for it. So the
one measurement that would settle the increment has been available to run, as a
cheap leaf fit, for as long as the entry has existed. **That is the next fit
after `@packet-h-satstage`,** and its question is stated in advance as this
document requires: *what does the shared service cost with BOTH clients driven,
against 24,399 ALUT for two unshared cores?*

### Why the cheque detector did not see this

`tools/budget/uncashed_cheques.py` reports 7 open rows and none of them is the
projector. It is not wrong; it is asking a different question. Check 1 scans
modules that are **measured or fit-targeted** and finds those instantiated by
nothing — and every link here has a root: `zhao_project_service` is
instantiated by `zhao_proj_subsystem`, which is instantiated by
`zhao_terrain_pipe`, which is fitted. The chain is rooted at every step and
**adopted at none**, because the root of the chain is not `zhao_prod_top`.

**So there is a third shape of uncashed cheque, and it is the one that hid the
largest saving in the machine: ADOPTED NOWHERE, rather than instantiated
nowhere.** A module can be built, composed into a subsystem, composed into a
pipe, fitted at 102.19 MHz on physical pins — and still not be what production
instantiates. The detector should ask reachability **from the production top**,
not reachability from anything. The manifest already carries the words:
`zhao_proj_subsystem: not-yet-adopted`, `zhao_terrain_pipe` likewise. Nothing
reads them back, which is this project's oldest lesson.

### What Packet J actually is, restated

Not "write a G8C top". **Move `zhao_prod_top` off `zhao_geom_project` +
`zhao_terrain_project` and onto the shared service**, with geometry on client A
— via `tools/quartus/gen_prod_top.py`, which is the supported way to change what
the production top composes. The terrain half of that wiring is already written
and fitted; the geometry half is the port `zhao_proj_subsystem` deliberately
left exposed.


### CORRECTION, one hour later: `zhao_prod_top` is a RESOURCE top, not a machine

The section above says production "still carries two cores" and casts Packet J
as moving `zhao_prod_top`'s wiring. Read `zhao_prod_top.sv`'s own header before
repeating that:

> *"This is a RESOURCE top, not the console. Blocks are not wired to each
> other, so no timing number here means anything. Each instance is fed by its
> own seeded LFSR — constants would let the fitter fold blocks away, and a
> shared source would let it merge logic across them."*

Checked against the file rather than assumed: `zhao_geom_project u29_i` takes
`v_valid_i(u29_src[28 +: 1])` from an LFSR and drives `u29_out_*` wires that go
nowhere. Every one of the 66 children is instantiated that way.

**What survives, and it is the substance.** The SELECTED set counts two
projectors. `design/prod_manifest.yml` selects `zhao_geom_project` and
`zhao_terrain_project`, and files `zhao_proj_subsystem`, `zhao_project_service`
and `zhao_terrain_pipe` as `not-yet-adopted` / `unused`. The 24,399 ALUT is a true measurement of what the selection costs when counted once, which is exactly
the question that top was built to answer.

**What does NOT survive, and I had it wrong.** "Adoption" is not a rewiring of
`zhao_prod_top` — there is no wiring there to change. It is a **selection**
change in `design/prod_manifest.yml`, after which `gen_prod_top.py` regenerates
the census against the new set. That makes Packet J's projector half the same
act as golden-path item (1), *the selected-variant list*, rather than a separate
RTL task — and item (1) is recorded in this document as an owner decision.

**And it names the real precondition.** A selection change is only honest once
the geometry client is actually composed onto `zhao_project_service`'s client A,
and `zhao_proj_subsystem`'s header says plainly that it is not: *"the geometry
producer (GEOM.SKIN/WARP into GEOM.WCACHE) is not composed anywhere in this
tree."* So the order is: compose geometry onto client A, then change the
selection, then re-census. Not the other way round.

The interfaces say that composition is small. `zhao_geom_project`'s ports map
one-to-one onto client A — `v_valid_i/v_ready_o` to `a_valid_i/a_ready_o`,
`vx/vy/vz/view` across, `src_id_i[15:0]` onto `a_payload_i` with
`PAYLOAD_A_W = 16` already matching, and the six result fields onto `a_x_o`,
`a_y_o`, `a_d_o`, `a_w_o`, `a_behind_o`, `a_payload_o`. **Exactly one port has
no counterpart: `out_ready_i`.** `zhao_geom_project` accepts backpressure on its
result; the service's client A port has no `a_ready_i` and pushes
unconditionally. Terrain gets away with that because its consumer accepts every
beat — the subsystem header's *"no adapter, no FIFO, no shadow state"*. So the
one open design question in the whole adoption is whether GEOM.CLIP can accept
unconditionally, or whether client A needs an elastic buffer. That is a
Verilator question, not a fit question, and it should be answered before any
selection changes.


#### The one open question in the adoption is ANSWERED, and the answer is "no adapter"

Stated above: the only `zhao_geom_project` port with no counterpart on the
service's client A is `out_ready_i`, because client A pushes results
unconditionally. Terrain tolerates that on a stated property —
`zhao_proj_subsystem` line 238 declares `wire fill_ready_unused;` with the
comment *"the primitive never stalls a fill"*.

Geometry has the same property, from the same primitive, and it is written down
as a constant rather than emerging from behaviour:

```systemverilog
// fpga/rtl/geometry/zhao_vertex_arena.sv:209
// Always accept: neither channel can stall, because the store answers in one
// clock and the metadata is combinational. Stated as constants so a future
// banking change has to change them deliberately rather than by accident.
assign fill_ready_o = 1'b1;
assign look_ready_o = 1'b1;
```

`zhao_geom_wcache` instantiates exactly one `zhao_vertex_arena` (line 154) and
wires `.fill_ready_o(fill_ready_o)` straight out, so its fill port is constant
ready. `zhao_terrain_wcache` ANDs three replicas' copies of the same constant.
**Both sides of the shared service therefore have a consumer that cannot stall a
fill, and the adoption needs no elastic buffer, no FIFO and no adapter.**

Worth stating the boundary, because this is the sentence that would otherwise
get over-quoted: the property belongs to `zhao_vertex_arena`, not to geometry in
general. It holds for the arrangement the adoption builds — client A into
`zhao_geom_wcache` — and says nothing about any other consumer someone might put
there later. The arena's own comment anticipates exactly that: a banking change
has to move those two constants deliberately.

**So the projector adoption now has no unanswered design question.** What
remains is the ordering already recorded: compose geometry onto client A, fit
`zhao_project_service` with both clients driven to price it, then change the
selection in `design/prod_manifest.yml` and re-census. None of that needed the
fit that is running, and none of it was visible from the leaf fit rows.


---

## `@packet-h-satstage`: THE RENDER CLOCK IS 77.80 MHz, AND THE ROW SAYS 72.44

```
ALM 27,583   DSP 63   M10K 136   registers 37,510   clean tree, 97 sources
gpu_clk 77.80 MHz   worst -2.853   TNS -2,276.4   hold +0.086
```

The saturation split delivered what was predicted for it and more. `final_sat_r`
(3 paths at -6.372), `dividend_r` (22, plus 7 duplicated) and `neg_r` are all
better than the new printed floor of -1.623. **gpu_clk went 61.08 → 77.80 MHz
and TNS fell 76%**, against a prediction of 71–72 MHz recorded before the fit.

The over-delivery is itself informative, and it is the case the prediction's
falsification clause named: the ~-3.9 band was **both** independent walls and
one shared placement region. Removing the attrdiv cone let the texture cone
below it improve from -3.949 to -2.853 without anyone touching it.

### The receipt's `fmaxMhz` field no longer means what a reader will assume

The row records `fmaxMhz: 72.44, fmaxClock: audio_clk`. **All 200 paths in the
printed window are `gpu_clk → gpu_clk` at a 10.000 ns relationship.** `audio_clk`
and `vid_clk` own zero negative paths between them.

`run_block_fit.ps1:1074` takes the FIRST row of Quartus's Fmax Summary, which is
sorted ascending — the slowest clock in the design *regardless of its
constraint*. Correct for a single-clock leaf. For a composed multi-clock top it
changes meaning silently the moment the ranking flips, and that is what just
happened: the render path overtook a domain nobody has been working on.

`audio_clk` is not a new wall. It reads 90.41 → 103.21 → 96.47 → 72.44 across
four fits in which no commit touches the audio domain — a 30 MHz swing that is
the fitter spending placement where the constraints are, which it can do freely
because that clock has no negative slack in any of the four.

**The gate hazard, stated because nothing is currently mis-gated and that will
not last:** `min_fmax_mhz` is checked against this field (`fit_rules.ps1:67`).
No rule binds `zhao_shell_top_v2` and labelled rows are never rule-checked, so
today it is inert. Eleven tops in `design/fit_targets.yml` carry
`min_fmax_mhz: 100`, and any of them that is multi-clock would be judged on
whichever clock is slowest in absolute terms. The sound metric is the
worst-slack clock's achieved Fmax, `1 / (relationship + setupSlackNs)` — for
this row `1 / (10.000 - 2.853) ns = 77.80 MHz`, agreeing with Quartus's own
`gpu_clk` line exactly.

### The campaign so far

```
@packet-h-m10k      29,044 ALM   54.12 MHz   TNS -29,689
@packet-h-timing    28,959       61.52       TNS -19,985
@packet-h-uvw       27,601       66.03       TNS  -8,851
@packet-h-mulstage  27,231       61.08       TNS  -9,514
@packet-h-satstage  27,583       77.80       TNS  -2,276
```

**ALM -1,461, gpu_clk +43.8%, TNS -92.3%.** 2,417 ALM inside the 30,000 budget,
63 of 112 DSP, 136 of 553 M10K. What remains to 100 MHz is 22.2 MHz and
-2,276 ns, and it is now entirely texture: `zhao_texture_cache_pipe_v2`
(`c2_tag` → `valid_r`) and `zhao_texture_v3own` (`cq_own_q` → `zhao_texture_v3rq`'s
`h_d_q` / `s_d_q` / `lcnt_q`), at 11.9–12.7 ns of data delay against 10.000.

Every number above is the render BACK end — see the closure finding above.

### And the closure audit is now a tool, run over all 60 receipts

`tools/budget/closure_liveness.py`. **48 of 60 receipts declare only what they
elaborate.** `zhao_shell_top_v2` declares 97 and elaborates 78; the 19 split
three ways and only the first group is a problem:

* **the geometry front end, genuinely absent (8)** — `setup`, `meshfetch`,
  `assemble`, `vdecode`, `assetfetch`, `project`, `clip`, `depthquant`;
* **V1 siblings kept beside their V2 (3)** — `geom_bin_pipe`, `geom_binner`,
  `raster_tile_pipe`, which the differential tests want in one source list;
* **unselected arithmetic variants (5)** — `attr_mul72x13_dsp3`, `dual18_mul`,
  `mul27_exact`, `raster_attrgrad_dsp3`, `texture_bilerp_lane_dsp2`;
* plus `raster_blend`, `raster_quant`, `raster_rcp24_svc`.

`zhao_prod_top`'s census declares 147 and has exactly one dead entry,
`zhao_raster_quant` — which `prod_manifest.yml` already declares a `probe` with
a paragraph explaining that RASTER.RESOLVE instantiates its two halves directly.
That is a closed question, correctly recorded, and the tool finding nothing else
in the production census is the reassuring half of this audit.

**The tool's first version was wrong, in the accusing direction.** It read the
Compilation Hierarchy Node table alone and called `zhao_texture_mod255` dead;
that module is elaborated inside `zhao_texture_mosaic_v2` and simply has no row
of its own, because Quartus folds small entities into the parent. It now reads
the `Info (12128): Elaborating entity` lines as authoritative and unions the
table in as corroboration, so the union can only ever call a module MORE alive.
The eight geometry blocks have **zero** such lines; `mod255` has one. The
finding survives its own instrument being corrected, which is the only reason it
is still in this document.


---

## THE REMAINING RENDER BAND IS TEXTURE, AND ITS WORST PATH IS MEASURED NODE BY NODE

At `@packet-h-satstage` the render path is `gpu_clk` at 77.80 MHz, worst
-2.853, and every one of the 200 printed paths is in the texture island. Two
cones own it.

### Cone 1: `zhao_texture_cache_pipe_v2`, `c2_tag[3][16]` → `valid_r[2][11]`

12.684 ns of data path against a 10.000 ns period. The receipt's own
Data Arrival Path, level by level:

```
  6.465   c2_tag[3][16]|q                  the launching register
  7.771   c3_hit_c[3]~30                   compare captured tag vs read tag
  9.286   m_any_c~0                        the LANES-deep priority chain
 10.571   m_tag_c[15]~3     ┐
 12.047   m_tag_c[6]~8      │  select the winning lane's tag
 13.055   m_tag_c[6]~9      ┘
 13.857   m_mask_c[2]~25    ┐
 14.286   m_mask_c[2]~26    │  ... then compare every lane against it
 15.861   m_mask_c[2]~27    ┘
 17.312   valid_r~19
 19.149   -> valid_r[2][11]
```

**From `m_any_c` to `m_mask_c` is 9.286 → 15.861 = 6.575 ns, 52% of the data
path**, and all of it is one ordering decision: the block selected a tag and
*then* compared four lanes against it, 28 bits wide (TAG_W 24 + IDX_W 4), in
series behind the priority chain.

The endpoint is what named the fault. Lane 3's TAG reaching lane 2's VALID can
only happen through the selected tag — grouping these paths by module would
have said "texture cache" and stopped.

**Fixed 2026-09-18 by inverting the order: compare first, select second.** Every
operand (`c2_tag`, `c2_idx`, `c2_en`, `c2_rval`, `c2_rtag`) is a register, so
the pairwise `(tag, idx)` equality table `same_c[j][k]` does not depend on the
selection and computes beside it. The priority chain narrows to one bit wide,
and the mask becomes a one-hot pick from a table that was already settled. The
`m_tag_c` levels leave the mask's cone entirely.

Bit-exact and unpipelined: the same lane wins (lowest index), the same lanes
join its mask, `m_tag_c` / `m_idx_c` carry the same values to the same
consumers, and `same_c[j][j]` is trivially true so the winner is in its own mask
exactly as before. **A combinational restructuring inside one cycle is invisible
to a cycle-by-cycle differential**, which is why no latency constant moves —
and is also why the differential cannot confirm it, so the directed and mutant
suites are the evidence that matters here.

This block's header already carried the warning: *"81.06 MHz worst internal path
`rq_rp[1] -> valid_r[1][2]` 12.159 ns"*, recorded at some earlier standalone
fit. The endpoint was the same register array then. Nobody read it back.

### Cone 2: `zhao_texture_v3own` `cq_own_q` → `zhao_texture_v3rq` `h_d_q`

12.277 ns, 1 ns behind cone 1, and now traced the same way — from the receipt,
not from the source:

```
  6.443   cq_own_q~22|q                    the launching register
  7.869   cq_own_q~56                      the 4-deep queue read mux
  9.630   u_texture_v3|Mux4~1     |
 11.209   u_texture_v3|Mux4~4     |   a 64:1 mux, AT ISLAND SCOPE
 12.136   u_texture_v3|Mux4~20    |
 12.799   u_own|cmb_fire_c~0
 14.672   u_own|cmb_pop_c~1
 15.874   u_own|u_rq_init|pop_taken_c
 16.350   u_own|u_rq_init|n_h_d_c[13]~0
 16.791   u_own|u_rq_init|h_d_q[13]~0
 18.720   -> u_own|u_rq_init|h_d_q[1]
```

**The three `Mux4~*` levels are 7.869 → 12.136 = 4.267 ns, 35% of the data
path, and they are one line of the island:**

```systemverilog
// zhao_texture_island_v3_top.sv:2356
wire owner_combine_validation_ready_c =
    !join_validation_pending_q[owner_combine_owner_w[13:8]];
```

`[13:8]` is six bits — a 64:1 mux — and its INDEX is `owner_combine_owner_w`,
which is `cq_own_q[cq_rp_q]`, the output of another array read. **Two chained
reads, the second 64 wide, and the answer travels back into `u_own` to decide
`cmb_fire_c`.** The owner leaves the block and its readiness returns.

Note what this is NOT, recorded because it is where an hour goes: the
suspicious-looking `win_gen_of_slot()` in `zhao_texture_v3own` is not a table
lookup at all, it is `(sl < tail_q) ? sh_alloc_gen_q : sh_alloc_gen_q - 1` —
two values and a compare, nowhere near this path. Reading the source names
candidates; the report names levels.

**Why this one is NOT fixed in the same pass as cone 1, stated rather than left
as silence.** The cache-pipe fix was safe because every operand was a register
and the reordering was bit-exact inside one cycle. Here the obvious analogue is
not: reordering the two reads — do the 64:1 mux from each of the four queue
entries, then select by `cq_rp_q` — removes only the 1.4 ns read mux and leaves
the 4.267 ns of 64-wide select in the cone. Removing *that* means carrying a
per-entry "my slot is pending" bit, which is a REGISTERED copy of a fence that
other logic clears, and a fence reacting one cycle late asserts ready one cycle
early. That is a protocol change, not a restructuring, and it needs its own
design pass with the admission/validation contract open beside it.

So cone 2 is **diagnosed and left**, deliberately, with the measurement and both
candidate shapes written down. The hint for whoever takes it:
`owner_combine_owner_w[13:8]` indexes six other island arrays on the lines
immediately below — `material_m`, `material_refused_m`,
`owner_required_mask_m`, `join_validation_generation_m` and more — and **none of
those are in this cone, because they are registered reads.** The fence is the
odd one out, and that is where the fix belongs.

### What this means for the campaign

The remaining band is dense: 200 paths between -2.853 and -1.623, across the
cache pipe, the owner block and the request queues, with no single dominant
endpoint. Getting from 77.80 to 100 MHz is not one more fix — it is a Timing4-
shaped campaign whose first two cones are now named and one of which is done
pending measurement.

