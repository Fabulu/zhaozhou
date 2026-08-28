# What is actually blocking every remaining block

> ## OPEN DEFECT, 2026-08-28: OPEN-LOOP CLAIMANTS ON THE SHARED MULTIPLIER
>
> Found by the first composition test, and it is the reason composition tests
> exist. Two blocks, one cause.
>
> **The DOT sequencer cannot tolerate refusal.** `zhao_probe_v3_exec` issues
> a DOT's second and third products at S3 and S4 on a FIXED CLOCK SCHEDULE
> and never checks that either was granted. Behind an arbiter those requests
> can be refused, and the accumulator then sums a product that never arrived.
>
> MEASURED: with a rival claimant contending, 4-5 of 12 programs containing
> DOT give wrong answers. ALU-only programs pass every time, which is what
> localises it. Three fixes were tried and all failed -- freezing the whole
> pipe, upstream-only back-pressure, and a stall-with-bubble -- because the
> multiplier is a FIXED-LATENCY PIPE: holding the instruction desynchronises
> it from a product that arrives on schedule regardless.
>
> **The services have the identical defect.** Neither `zhao_probe_curve_svc`
> nor `zhao_probe_dist_svc` has a `mul_ready` input at all. A refused service
> advances as though its multiply happened.
>
> ### Why no sweep found either one
>
> Both blocks are correct in isolation and score full marks alone. The defect
> is at the SEAM, and a claimant that never gets refused has no refusal path
> to mutate. The engine sweep scored 4 of 11 on its first run for exactly
> this reason -- seven mutants were unreachable because the test never drove
> the rival.
>
> ### The fix, which is one decision for three blocks
>
> Every claimant on the bank must close the loop: hold state until its
> product is GRANTED, or reserve the bank for a whole multi-product sequence.
> That is one change of shape applied three times (executor, curve service,
> distance service), and two of those are already probed and fitted, so their
> numbers would need redoing.
>
> **Until then the composition is correct only when nothing contends**, which
> is exactly the condition that will not hold once the services are attached.
> The contention test is written and passing for ALU ops, with DOT programs
> skipped under a comment naming this entry.



> ## FIELD v3 STATE, 2026-08-28. The Field IR engine is no longer one block.
>
> The ledger treats `FIELD.SEQ.*` as sequencer blocks blocked on one another.
> That is no longer the shape of the work. Field v3 decomposed the engine into
> measured pieces, and what blocks the remaining ops is now a RESOURCE, not a
> sequencer:
>
> | piece | state |
> | --- | --- |
> | FIELD.PLAN (software planner) | swept 16/16 |
> | ready-context FIFO | fitted, 257 ALM, 97.8 MHz |
> | banked register file (probe) | fitted, 372 ALM, 93.14 MHz -- a FLOOR, see below |
> | two-bank distance service | fitted, 1,745 ALM, 90.6 MHz |
> | barrel curve service | II 13, swept 15/15, fit RUNNING |
> | four-bank patch accumulator | 297 updates in 297 clocks, swept 15/15, fit RUNNING |
> | Earth lattice walker | 297 clocks/patch, swept 18/18 |
> | v3 executor datapath | swept 31/31, ALU ops + sequenced DOT2/DOT3 |
> | functional register file | built; NOT the probe -- the probe implements no semantics |
> | four-wide multiplier bank + arbiter | built, 18,202 routed with none lost; sweep RUNNING |
>
> ### What actually blocks the nine remaining ops
>
> Not a sequencer. `zhao_field_exec_shared.sv` holds FIVE shared resources --
> one multiplier, one isqrt, one sine, one reciprocal, one rcp24 ROM -- and
> the ops take turns on them. That block exists because the first synthesis of
> the Field engine measured **79 DSPs against a device with 112**, with nine
> of ten units idle at any instant.
>
> So each remaining op is blocked on ARBITRATING the resources it borrows:
>
> | op | needs, beyond the multiplier |
> | --- | --- |
> | CURVE, DCURVE, SPLINE, NOISE2, RIDGE | nothing -- the bank arbiter unblocks these |
> | ROT2, ROT3 | the shared sine table |
> | RING | the shared reciprocal |
> | NORMALIZE2/3 | the shared isqrt and the rcp24 seed ROM |
>
> Full reasoning in `reports/FIELD_V3_SERVICE_ATTACH.md`, including the
> correction where I claimed ROT and RING needed only the bank and the RTL
> said otherwise.
>
> ### One measurement that is NOT what it looks like
>
> The banked register file's 372 ALM / 12 M10K / 93.14 MHz is a fit of
> `zhao_probe_banked_rf`, which addresses every bank with the SAME ROW and
> therefore cannot read a register group that crosses a multiple of four. Its
> own header says it implements no Field semantics. The functional file
> (`zhao_field_v3_rf.sv`) does per-bank address arithmetic the probe does not,
> so **that number is a floor, not a measurement of the shipped part.** It
> needs its own fit before it is quoted anywhere.
>
> ### Still owner-blocked, unchanged
>
> The `material` writer-selection and `nav_cost` reduction laws are declared
> in `design/contracts/FIELD.SEQ.EARTH.md` as CHOSEN, NOT FOUND. Nothing in
> the tree defined them; they are reducer semantics rather than game content,
> but they are still a choice the owner has to own.



> ## RULE-1 SURVEY, 2026-08-26. Read before planning any greenfield block.
>
> ### SIX OF THE NINE REMAINING NON-GAMEPLAY BLOCKS HAVE NO ORACLE
>
> The working rule is *check the reference resolves BEFORE writing any RTL*.
> `design/blocks.yml` names a `reference_model` for every block, and a name in
> the ledger is not a model in the tree. Grepped for both the CamelCase symbol
> and the snake_case spelling:
>
> | block | reference_model | in the tree? |
> | --- | --- | --- |
> | GEOM.WCACHE | `zref::geom::VertexArena` | **YES** -- `zref_geom_wcache.hpp` |
> | GEOM.PROJECT | `zref::render::project_vertex` | **YES** -- `zref_cull.hpp` |
> | GEOM.MESHFETCH | `zref::MeshFetch` | **NO** |
> | GEOM.VDECODE | `zref::VertexDecode` | **NO** |
> | GEOM.LOOM | `zref::TransformLoom` | **NO** |
> | GEOM.WARP | `zref::GeomWarp` | **NO** |
> | MEASURE.HISTOGRAM | `zref::MeasureHistogram` | **NO** |
> | FORGE.PRIM | `zref::ForgePrim` | **NO** |
> | POST.GATHER | `zref::PostGather` | **NO** |
>
> GEOM.MESHFETCH is not an oversight and says so in its own words:
> `zref_cull.hpp` states that the model there **is not** `zref::MeshFetch` and
> that `zref::MeshFetch` *stays unresolved*.
>
> ### What this means for "finish the hardware"
>
> Those six cannot be started under rule 1. The work in front of them is
> **writing the reference model**, which is a different activity from writing
> RTL and has to happen first -- otherwise a differential has nothing to differ
> against and the RTL becomes its own specification.
>
> It also means a plan that counts them as "RTL to write" is counting the
> smaller half of the job.
>
> ### The one block that IS startable, and its state
>
> **GEOM.WCACHE.** Oracle resolves (`zref::geom::VertexArena`), RTL exists
> (`zhao_vertex_arena.sv`), and `geom_wcache_directed` PASSES.
>
> Its FORMAL proof does not. `a_hit_implies_written` fails at k=4. The file
> records the diagnosis honestly and keeps the failure visible: a bulk async
> reset over an unpacked array is not expressible as a memory reset, so the
> solver could start cells at 1; `valid_q` became a PACKED vector and
> `p_array_implies_shadow` now returns UNSAT at k=3 in BOTH directions. That
> closed one real modelling gap and did **not** close the proof, and the CI lane
> stays unregistered rather than claiming otherwise.
>
> That is the next piece of real hardware work with an oracle behind it.
>
> ### GEOM.VDECODE IS BLOCKED ONE LEVEL DEEPER THAN 'NO REFERENCE MODEL'
>
> The survey above lists GEOM.VDECODE as missing `zref::VertexDecode`, which
> reads like 'write the model'. Checked what writing it would require, and it
> is not writable yet.
>
> `design/contracts/GEOM.VDECODE.md` is a generated stub -- every section still
> says TODO, including the packet layout and the Q formats -- and its one real
> line is the note:
>
> > Compression format owned by SW.TOOLS.ASSET (pack side) -- one spec, two ends.
>
> **There is no pack side.** `tools/pack/` holds one file, `mkcreaturepage.py`,
> which is a contact-sheet generator; grepping the whole reference tree for a
> vertex packing or unpacking format finds none. So the spec that VDECODE is
> supposed to be one END of does not exist at either end.
>
> That means the honest order for this block is three steps, not one:
>
> 1. define the compressed vertex format (positions / normals / UV, and the
> quantisation each gets),
> 2. write the packer, because a decode model with no encoder has nothing to
> round-trip against and becomes its own specification -- the exact failure
> rule 1 exists to prevent,
> 3. then `zref::VertexDecode`, then RTL.
>
> Step 1 is a format decision that binds the asset pipeline and the silicon
> together for good. It is recorded here rather than taken.
>
> The same question has NOT yet been asked of the other five oracle-less
> blocks; VDECODE was checked because it looked like the most mechanical of
> them, and it was not mechanical. Assume the others are at least as deep
> until each is checked the same way.
>
> ### CHECKED. ALL SIX ARE BLOCKED ON SPECIFICATION, NOT ON RTL
>
> Counted the generated-stub sections in each contract (a stub still carries
> its `gen-contracts` TODO line verbatim):
>
> | block | stub sections | what is actually missing |
> | --- | --- | --- |
> | GEOM.MESHFETCH | 14 of 17 | meshlet limits are unfrozen Phase-0 data |
> | GEOM.VDECODE | 14 of 17 | the compression format, at both ends |
> | GEOM.LOOM | 15 of 17 | packet layout, Q formats, latency |
> | GEOM.WARP | 15 of 17 | packet layout, Q formats, latency |
> | FORGE.PRIM | 15 of 17 | packet layout, Q formats, latency |
> | POST.GATHER | 15 of 17 | packet layout, Q formats, latency |
> | MEASURE.HISTOGRAM | **0 of 19** | see below -- it is the exception |
>
> MEASURE.HISTOGRAM's contract has no stubs left, which looks like the one
> startable block until you read it. Every section was filled in with the
> truth, and the truth is that it cannot be built: its Scalar reference
> function section says `zref::MeasureHistogram` **does not resolve and never
> has** -- the TENTH phantom in the ledger -- and that no oracle was written
> because writing one would mean making four inventions. Its packet layout
> section says the declared `fragment_error` input resolves to a one-bit
> protocol flag on RASTER.FRAGMENT, not to an error magnitude, so the block's
> stated input does not carry the quantity the block is for.
>
> So a complete contract here means completely documented as blocked, which is
> the discipline working, not a lane to pick up.
>
> **NET: zero of the six are startable, and none of them is short of RTL.**
> Five are short of a packet layout and a Q format; one is short of a file
> format with no owner; one is short of four product decisions. That is
> specification work and decision work, and the two that touch the compositor
> and 2D path (FORGE.PRIM, POST.GATHER) are exactly the ones Fabian has
> reserved. None of it is unblocked by writing more SystemVerilog.

> ## STATE AS OF 2026-08-26 (evening). This block supersedes everything below it.
> ## Read this first.
>
> ### The Field engine: ELEVEN of fourteen operations run on v2
>
> | | v2 |
> | --- | --- |
> | the twelve ALU ops | executing |
> | CURVE, DCURVE, SPLINE | executing |
> | LEN2, LEN3, DIST2 | executing |
> | RING, RIDGE, NOISE2 | executing |
> | ROT2, ROT3 | executing |
> | **NORMALIZE2, NORMALIZE3** | **the last two** |
>
> ALU-path throughput is unchanged throughout at **3.97 vertex-instructions per
> clock**, 27.8x v1.
>
> ### What NORMALIZE still needs, and it is one wire bundle
>
> Everything else exists: the second read pass, the multi-result reply, the mode
> field, the immediate, the ledger discipline, the multiplier priority chain.
>
> `zhao_field_normalize` takes the **shared integer square root**, and v2 wires
> `zhao_field_isqrt` STRAIGHT to `u_len` because the length family was its only
> consumer. NORMALIZE makes it two, so those wires become a mux on the captured
> unit id -- same shape as the multiplier, licensed by the same interlock, and
> carrying the same caveat: it is a mux, not an arbiter, and it becomes wrong
> the moment two long ops can run concurrently.
>
> It does NOT use the shared reciprocal; it carries its own rcp24 ROM.
>
> ### Then the sequencer, and what that means here
>
> v2 IS the sequencer for the operations it runs -- issue, scoreboard, interlock,
> retire. What remains under that heading is the FRONT of it: program fetch and
> the per-patch driving `zhao_field_seq` does in v1. v2 is driven by a testbench
> today.
>
> ### FIVE SHARED RESOURCES NOW, and the rule for adding the sixth
>
> multiplier, reciprocal, integer square root, curve table, sine table.
>
> The question to ask of a new unit is NOT "can two operations overlap" -- the
> interlock answers that -- but **"does this operation call anything that also
> needs a shared lane"**. RING was the first where the answer was yes INSIDE a
> single operation, and it turned the multiplier's mux into a priority chain.
>
> ### THE EVIDENCE, and the two things it still does not cover
>
> Every increment: differential against `zfield::interpret`, mutation sweep with
> forced regeneration and a generated-model hash check, `ctest -L fast`, ledger.
> The sweep now enforces its own law -- a survivor without a proof of equivalence
> FAILS the run, and a declared equivalent that is CAUGHT aborts.
>
> * **v2 HAS NO LEDGER ENTRY AND HAS NEVER BEEN FITTED.** Every number above is
>   simulation. `ledger:check` is green WITHOUT v2 in it.
> * **DEBUG.FRAMEBLIT is closed** -- RTL_VERIFIED on the composed path, swept
>   20/20, composed fit 7,442 ALM. It is not outstanding work.

> ## STATE AS OF 2026-08-26 (morning). Superseded by the block above it.
> ## Read this first.
>
> ### The Field engine's blocker was never the clock, and v2 is the answer
>
> Seven fitter-measured waves took `zhao_field_seq` from **8.59 to 58.99 MHz**
> and it did not matter, because the terrain cost model assumes one instruction
> per clock and v1 delivers one per SEVEN. The measured gap was throughput.
>
> `zhao_field_v2_core` is the replacement: LANES=4 vertices under one PC, WFS=8
> wavefronts resident, one instruction in flight per wavefront.
>
> | | v1 | v2 |
> | --- | ---: | ---: |
> | vertex-instructions per clock | 0.143 | **3.97** (27.8x) |
>
> **v1 is FROZEN, not deleted.** It says so in its own header, and it is the
> reference v2 is differentially checked against. It was not converted: its whole
> design rests on one instruction in flight, that assumption is baked into a
> dozen places, and converting it would have falsified them one at a time with
> every test still green.
>
> ### What v2 executes, and what it refuses
>
> | | status |
> | --- | --- |
> | MOV/LDC/ADD/SUB/MUL/MAD/MIN/MAX/ABS/CLAMP/SELECT/CMP | executes |
> | CURVE, DCURVE, SPLINE | executes (one unit, three modes) |
> | LEN2, LEN3, DIST2 | executes (one unit, three modes) |
> | RING, RIDGE, NOISE2, ROT2, ROT3, NORMALIZE2/3 | **REFUSED with a status**, never skipped |
>
> Refused rather than ignored is the law: an instruction quietly skipped
> produces a plausible field and a wrong world.
>
> ### THE DEFECT THE SWEEP FOUND, because it is the pattern to expect
>
> Three mutants survived the CURVE tests. All three were unreachable for one
> reason: every test section started ONE wavefront, so only one long operation
> was ever in the machine. The real workload is eight wavefronts on one program,
> drifting apart in pc, contending for the same unit.
>
> The test written to reach them **hung**: the dispatch slot is filled at stage
> 2, two cycles after issue, so the guard on `lq_valid` was two cycles late and a
> second long op overwrote the first's pending request. Not a wrong answer -- a
> stop.
>
> **The generalisation worth keeping:** v2's whole point is concurrency, and a
> test that exercises one wavefront tests the part of v2 that is still v1.
>
> ### What the Field engine still needs, in measured order
>
> 1. **RING** -- oracle resolves, v1 unit exists, three operands on the natural
>    ports so no steal cycle. Cost is that `zhao_field_ring` and
>    `zhao_field_rcp` both want the multiplier inside one operation, so the mux
>    becomes v1's priority chain.
> 2. **An IMMEDIATE PORT.** v2's instruction interface is `{op, dst, a, b, c}`.
>    RIDGE and NOISE2 both read `ins.imm` and there is no immediate at all. Its
>    own increment.
> 3. **NOISE2, RIDGE, ROT2, ROT3**, then a **second multiplier lane** -- the
>    model prices MUL_LANES 1/2/3/4 at 3/6/9/12 DSPs and finds width 4 needs at
>    least two.
> 4. **NORMALIZE2/3 last**, regardless of its position in the op list: no
>    committed Earth program calls it, verified against the three shipped program
>    hashes. It has the worst II in the engine and would have been a day spent
>    making something faster that this game never calls.
>
> ### TWO THINGS THAT ARE NOT DONE AND MUST NOT READ AS DONE
>
> * **v2 HAS NO LEDGER ENTRY.** It is not a registered block, because it has no
>   fit. `ledger:check` is green *without* it. Nothing in the ALM/DSP/M10K census
>   accounts for v2, and the census must not be read as covering it.
> * **v2 HAS NEVER BEEN FITTED.** Every number above is simulation. The area and
>   Fmax of a 4-lane, 8-wavefront barrel core with a banked register file are
>   unmeasured. The nearest evidence is `zhao_probe_banked_rf` -- 12 M10K, 375
>   ALM, 96.54 MHz -- which is the register file alone.
>
> ### DEBUG.FRAMEBLIT is closed
>
> `RTL_VERIFIED` on the composed path, mutation-swept 20/20 on the atomicity law
> itself, and step 8's composed Quartus fit landed: **7,442 ALM (17.8%), 13
> M10K, 0 DSP**. The CMD.DMA defect that blocked it -- a 156-byte serial CRC
> chain in one cycle -- was fixed; the block that could not fit is now smaller
> than most of the design.



> ## STATE AS OF 2026-08-24 (morning). This block supersedes everything below it.
> ## Read this first.
>
> ### NOTHING IS NOW TOO BIG TO BUILD. Yesterday two blocks were.
>
> | block | was | now |
> | --- | ---: | ---: |
> | `zhao_surface_sheet` | 95,947 ALM — **229% of the device** | **279 ALM — 0.7%** |
> | `zhao_forge_cliff` | 33,109 ALM — 79% | **7,664 ALM — 18.3%** |
>
> `surface_sheet` declared 131,072 bits and inferred **zero** — every bit a
> flip-flop, 131,258 registers. It now infers all of them: **170 registers, a
> 344x logic reduction, no behavioural change.** Both blocks report
> `ramConversionWarnings: 0`.
>
> **Neither had any DSPs**, which is why two days of multiplier work never saw
> them. A DSP census cannot find a block like this.
>
> ### THE STORAGE LAW, corrected — one killer, not three
>
> `QUARTUS_GOTCHAS` §10 was first written as three independent killers: async
> read, reset touching the array, byte enables. **That is too coarse and the
> first item is wrong.**
>
> Measured: of the three, **only byte enables applied** to `surface_sheet` — its
> read was already synchronous and its array deliberately unreset, exactly as its
> header claimed. And `forge_cliff`'s `edge_mem_r` now **infers while still being
> read asynchronously**.
>
> > **A byte enable cannot be survived. An asynchronous read sometimes can.**
>
> The fix in both cases: **split the byte-enabled word into whole-written
> arrays** — which is the shape `zref::surface::Sheet` has always had
> (`uint8_t tag[4096]; uint8_t strength[4096]`). The RTL had been carrying a
> complication the reference never needed. Read-old semantics survived with **no
> bypass network**, verified by a `ram_rdw` calibration family added for the
> purpose.
>
> ### THE 27-BIT CLIFF — the cheapest lever on the board, and it is large
>
> Measured over 102 calibration points: a product costs **1 DSP from 8 to 27
> bits, 3 from 28 to 33, 4 at 40–48, 9 at 64.** Signed and unsigned identical.
> It is a cliff, not a slope, and **most of this design's arithmetic is 32-bit**
> — one band past it, paying triple.
>
> The band model predicts mapped reality **exactly**: `geom_project` 11 muls x 3
> = 33 measured; `terrain_project` 33; `terrain_normals` 18; `mat3x4_mul` 9.
>
> Thirteen blocks sit above the cliff holding **168 mapped DSPs, which would be
> 58 if narrowed — a candidate saving of 110**, against a remaining gap of ~45.
>
> **They are candidates, not winnings.** Each needs a *proof* that the narrower
> width suffices (`QUARTUS_GOTCHAS` §5 has demanded this since it was written).
> `zhao_geom_lod`'s 64 bits is the division path and is **excluded**. Whether 27
> bits covers **world coordinates** is an owner question about map size and
> precision, not a hardware one.
>
> **Width narrowing costs no clocks, no state, no interface change and no
> rearchitecture** — unlike every rebuild so far. And it **composes** with
> sequencing rather than competing: `TERRAIN.NORMALS` is 18 -> 6 by width, then
> 6 -> ~3 by rate.
>
> ### The audit's deliverables, and the limits it stated on itself
>
> `reports/BUDGET_HEATMAP.md`, `reports/budget_manifest.json`,
> `reports/rtl_inventory.json`, `tools/budget/calibration.json` (102/102),
> `tools/budget/scan_rtl.py`, `tools/quartus/map_sweep.ps1`.
> Coverage went **41 of 94 -> 89 of 91**.
>
> Three limits, still true:
>
> 1. **No fit in the census describes HEAD** — zero of 41. Every trustworthy
>    resource number came from the **map** lane, and the heatmap says so per row.
> 2. **WNS/TNS/hold extraction is fixed but UNPROVEN.** No fit has run since the
>    rewrite. **Do not quote a slack number until a real `.sta.rpt` has been
>    read.**
> 3. **84 blocks still have no demand figure**, which is what separates
>    "expensive" from "wrong".
>
> ### AND THE AUDIT TOOL ITSELF HAD BLIND SPOTS
>
> `scan_rtl.py` had **no byte-enable detector at all**, and its `resetTouched`
> check walked the ELSE branch — Verilator folds `if (!rst_n)` by **swapping the
> arms**, so `thensp` is the working branch. **It had been reporting the
> repository's worst block as healthy.** Both fixed, both now carrying positive
> controls in **either** direction.
>
> The tool that checks things is a thing that needs checking. Treat a GREEN from
> the scanner as evidence only for detectors that have a control.
>
> ### Where the DSP campaign stands
>
> | | DSPs |
> | --- | ---: |
> | 2026-08-23 morning | 327 |
> | **now, measured** | **134** |
> | ceiling | **85–90** |
>
> Rebuilt and measured: `field_seq` 79 -> 3, `geom_skin` 72 -> 9 (and it **meets
> its 120,000-vertex frame budget** at 124,514), `surface_stamp` 28 -> 0,
> `texture_tmu` 28 -> 6.
>
> ### The queue
>
> | # | work | return | state |
> | --- | --- | ---: | --- |
> | 1 | `surface_sheet` + `forge_cliff` storage | 229%/79% -> 0.7%/18.3% | **DONE** |
> | 2 | projector merge, phase 1 | **~33 DSPs** | **in flight** |
> | 3 | width audit over the 13 cliff blocks | up to 110 DSPs | **recommended next** — cheapest, no rearchitecture |
> | 4 | Field memories rebuild, 6 waves | 7,958 -> ~4,000 ALM, 33.86 -> 100+ MHz | docketed |
> | 5 | TMU pipeline, 5 waves | II 6 -> 1, 36 -> >=100 MHz | docketed |
> | 6 | `TERRAIN.NORMALS` width + rate | 18 -> 6 -> ~3 | docketed |
> | 7 | demand figures for 84 blocks | separates expensive from wrong | docketed |
> | 8 | projected-vertex cache | 6,144 -> 1,089 projections/patch | docketed, `GEOM.WCACHE` |
>
> **`zhao_field_seq` is now the largest single block** — 7,958 ALMs with **zero
> memory bits**, a 64x32 register file and three ROMs built from logic while 502
> M10Ks sit idle. It is the clearest remaining application of the storage law,
> and the one block where the **valid-bitmap** pattern genuinely is the answer.
>
> ### Still blocked on the owner
>
> * the three earth-field WRITE ops (`FIELD.WRITE.MATERIAL/NAV/HAZARD`);
> * particle-simulation, compositor and 2D behaviour, reserved by standing
>   instruction;
> * `zref::rescale_s32`'s silent `__int128` -> `int64_t` narrowing in the shipped
>   skinning reference — three options docketed, none taken;
> * the scar-texture **pool size** — `SURFACE.STAMP` is pool-bound, not
>   rate-bound;
> * **whether 27 bits covers world coordinates** — this one now gates up to 110
>   DSPs and is the highest-value question outstanding;
> * the **three-bone skinning tail** (2.51% of vertices) and the
>   weight-normalisation precondition.


> ## STATE AS OF 2026-08-23 (night). This block supersedes everything below it,
> ## which is now history. Read this first.
>
> ### The DSP campaign: 327 -> 188, all measured
>
> | | DSPs |
> | --- | ---: |
> | morning | 327 against a 112-DSP device |
> | **now** | **188** |
> | policy ceiling | **85-90**, warning line >95 |
>
> Landed today, each measured under a constrained fit:
> `zhao_field_seq` **79 -> 3**, `zhao_geom_skin` **72 -> 9**.
> Earlier: `zhao_terrain_lod` 28 -> 3, `zhao_geom_lod` 18 -> 6.
>
> Remaining, largest first — **every one now has a demand figure, which was not
> true this morning**:
>
> | block | DSPs | demand basis | target |
> | --- | ---: | --- | ---: |
> | `zhao_terrain_project` | 33 | ~270 patches/frame, costed against 1.67 M clocks | cache-then-sequence |
> | `zhao_surface_stamp` | 28 | **20,000 texels/frame** (derived) | **0-2** |
> | ~~`zhao_texture_tmu`~~ | ~~28~~ **-> 6** | **850,000 samples/frame** (derived) | ~~6-9~~ **LANDED** |
> | `zhao_terrain_normals` | 18 | **2,000 normals/frame** (derived) | **1-2** |
> | `zhao_geom_cull` | 15 | one evaluation per five clocks | 4-6 |
> | `zhao_geom_binner` | 12 | per-triangle costs + arena caps | — |
>
> The three **derived** figures come from Sacrifice itself (survey of `sacengine`
> plus the retail install; full working in `docs/OWNER_DOCKET.md`). They are
> derived, not ruled — overturn on sight. If they land the census reaches ~124
> before the other three are touched.

> ### TEXTURE.TMU CLOSED, and it left TWO OPEN PROBLEMS BEHIND IT (2026-08-23, RUN-20260823-1736)
>
> **28 -> 6 DSPs**, constrained fit, samples bit-identical to `zref::Tmu`. The
> filter's four-weight law was FACTORED — `A = (t00<<8) + (t10-t00)*fu`,
> `B` likewise, `S = (A<<8) + (B-A)*fv`, one rescale — which is 3 products a
> channel instead of 8, and then multiplexed 2 channels at a time. 32 products
> became 6. `zhao_texture_bilerp`'s ports did not change, so
> `tests/formal/texture_bilerp.sby` needed no edit at all -- but its bmc task
> stopped closing (3,300 s, no answer), so P1-P4 are UNPROVED on the new form.
> A green harness is not a green theorem. What stands in its place is a TOTAL
> two-part equivalence check over all 2^48 inputs (no lane truncates, proved at
> the 16 texel corners x 65,536 fraction pairs; and the pre-rounding sum is
> exactly linear in the texels, so four basis vectors per fraction pair settle
> the whole map). Recorded in the contract as an open provability regression.
>
> **PROBLEM 1 — the block runs at 0.33x its derived demand, and nothing had ever
> measured that.** The suite asserted accept-to-retire latency and byte
> stability; the rate lived in prose. Measured now, and asserted exactly:
> **6 clocks per CLUT sample = 277,778/frame against 850,000.** Terrain is
> CLUT8, so that is the demand-critical figure. The fix is designed and written
> into `design/contracts/TEXTURE.TMU.md` (II = 2 needs a 2-entry in-flight
> record, an issue arbiter over the single cache port, and in-order completion;
> II = 2 is the port's own floor because a CLUT sample needs two serial
> accesses). **Not built.** It is orthogonal to the DSP work: every multiplier
> was in the filter and the CLUT path never touches the filter.
>
> **PROBLEM 2 — the per-block Fmax column has been measuring whatever
> register-to-register path happened to exist, which for this block was its
> sample counter.** See `reports/QUARTUS_GOTCHAS.md` §9. The per-block SDC
> constrained the clock and nothing else: no `set_input_delay`, no
> `set_output_delay`, so every pin-to-register and register-to-pin path was
> excluded. This block reported 199.72 MHz before and 192.46 MHz after; **both
> are `texture_samples_o`'s carry chain.** With I/O delays applied the worst
> path is `req_mode_i[8] -> q_addr_r[55]` at **37.004 ns** — the address
> generator, which this block's contract has named "the longest path in the
> file" since the day it was written.
>
> `tools/quartus/run_block_fit.ps1` now emits the I/O constraints. **Every row
> in `reports/synthesis/zhao_block_fit.json` measured before 2026-08-23 evening
> carries the old meaning**, exactly as 47 rows carried the pre-§7 meaning. A
> row's Fmax is trustworthy only where that block's critical logic happens to be
> register-to-register. Re-measuring the census under the corrected SDC is a
> campaign-sized job and is **not** RUN-20260823-1736's; it is docketed here.
>
> ### THE NEW AXIS: timing. Every block before 2026-08-23 was fitted with no
> ### timing objective at all.
>
> The per-block SDC named `gpu_clk`/`vid_clk`/`audio_clk` while 63 of 71 clock
> ports are called `clk`, so `create_clock` resolved to an empty collection in
> **every fit this project had ever run**. Fixed. Consequences:
>
> * DSP counts **stand** (inferred before the SDC is read);
> * **47 rows' ALM figures are the OPTIMISTIC end** — with no timing to meet, the
>   fitter optimised for area alone. The column was measured against the wrong
>   objective, not merely missing one;
> * Fmax figures in those rows were never measurements.
>
> ### THE SDC DEFECT HAD A SECOND HALF, found 2026-08-23 night
>
> Fixing `create_clock` was necessary and **not sufficient**. The generated SDC
> declared **no `set_input_delay` and no `set_output_delay`** — and TimeQuest
> **silently excludes every pin-to-register and register-to-pin path when none
> is declared.**
>
> Demonstrated on identical RTL, same tool, same device:
>
> | `zhao_texture_tmu@pre-rearch` | clock only | clock + I/O |
> | --- | ---: | ---: |
> | Fmax | **199.72 MHz** | **36.92 MHz** |
>
> A factor of **5.4**. That block's arithmetic runs from `req_*` pins to `smp_*`
> pins, so almost nothing in it is register-to-register except a counter —
> **199.72 MHz was the counter's speed**, and the arithmetic had never been timed
> at all. Constrained properly the worst path is 37.0 ns through the address
> generator, exactly where the block's own contract said it would be.
>
> **Fixed** in `tools/quartus/run_block_fit.ps1`: the generated SDC now declares
> `set_input_delay -clock clk 0` on every non-clock input and
> `set_output_delay -clock clk 0` on every output, guarded by a `get_ports clk`
> test. Validated against a real database before being trusted.
>
> **CONSEQUENCE: every Fmax measured before that fix is SUSPECT.** How wrong
> depends on how much of a block sits between registers rather than at its edges
> — a deeply pipelined block barely moves, a mostly-combinational one moves 5×.
> **Re-measure before quoting any of them.** Rows measured with I/O delays carry
> an `-io` suffix or postdate the fix.
>
> **Not affected: every DSP count.** Those are inferred at Analysis & Synthesis,
> which never reads the SDC. The census is sound.
>
> **Only these blocks have a speed number at all, and most are suspect:**
>
> | block | Fmax | meets its own budget? |
> | --- | ---: | --- |
> | `zhao_geom_skin` | **89.65 MHz** | **yes** — 124,514 vertices/frame vs 120,000 |
> | `zhao_geom_skin@MUL_LANES=6` | 84.61 MHz | yes, but costs 9 more DSPs for 13% |
> | `zhao_field_seq` | **33.86 MHz** | **no** — 3x short of 100 MHz |
>
> **Thirty-eight other blocks have no evidence either way.** Expect surprises.
>
> Two blocks are now known to have been holding `gpu_clk` far below its
> constraint before rearchitecture — `zhao_surface_stamp` at **32%** (32.33 MHz)
> and `zhao_texture_tmu` at **37%** (36.92 MHz). Both were reported as meeting
> their throughput target, because those targets were counted in cycles. **The
> emerging pattern is that pre-rearchitecture blocks are slow as a rule**, and
> that the DSP campaign and the timing campaign are the same campaign.
>
> `zhao_field_seq` went 8.59 -> 33.86 MHz on one rewrite (two 64-iteration
> combinational loops -> a six-stage leading-zero count, bit-identical). **That
> is a campaign, not a one-off** — a second path is unnamed. But WNS improved
> 5.4x while TNS improved **47x**, which means a *population* of paths collapsed
> rather than one outlier, so the remaining work is likely ordinary.
>
> **`zhao_geom_skin` at 89.65 MHz would cap the shared `gpu_clk` below 100.**
> Margin 3.8%. The remaining 1.155 ns is `Mult -> acc`; registering it pushes II
> to 13, which needs 93.6 MHz and would **fail** at 89.65. Only worth taking if
> it buys >4 MHz, and that must be measured.
>
> A static scan for the same long-combinational-chain shape
> (`reports/TIMING_HAZARD_SCAN.md`) found **one** other candidate:
> `zhao_raster_edgewalk.sv:324`, a 16-step serial popcount on the per-pixel
> raster path. Deliberately not rewritten on suspicion — fit it, read the logic
> levels, then decide.
>
> ### AND SUSPECT EVERY "THROUGHPUT MET" CLAIM IN EVERY CONTRACT
>
> `SURFACE.STAMP.md` recorded its throughput target as **"met, measured"**. That
> was **true about cycles and false about time**: the block closed at
> **32.33 MHz**, holding the shared `gpu_clk` at a third of its constraint, and
> its 28 DSPs had bought a *cycle count* on a block that could not run fast
> enough to spend it. Nobody knew, because the original fit had no timing
> objective.
>
> Every contract in `design/contracts/` predates the SDC fix. **Assume every
> "met" is cycles-per-item until someone has measured seconds-per-item.** The
> cheap check is a `variantOf` `@pre-rearch` re-fit of the unmodified RTL under
> the corrected SDC *before* changing anything — it costs one fit, and it is the
> only way a before/after is like-for-like rather than a constrained fit
> measured against an unconstrained one.
>
> ### The acceptance test is a RATE, never a clock
>
> `Fmax / initiation interval`, not Fmax. On GEOM.SKIN, +2 pipeline stages bought
> +31 MHz and turned a failing block into a passing one; a fix that buys MHz by
> adding stages can equally come out behind. **Report both numbers.** And note
> `gpu_clk` is shared, so meeting a block's own budget is necessary and not
> sufficient.
>
> **Two frame budgets exist and differ 6.6x.** `frame_gpu_cycles` (Z60 251,520)
> is exactly `2 x h_total x v_total` — the raster period and the scheduler's
> deadline. The **compute** budget is **1,666,667 clocks/frame** at the 100 MHz
> placeholder. Both are called "gpu cycles". See `design/budgets/latency.md`.
>
> ### Test-infrastructure defect, fixed, blast radius zero
>
> In a fresh checkout, CRLF made the mutation preflight's regex match nothing;
> it printed `linted 0 mutants, 0 do not build` and **exited 0**. A clean pass
> over an empty set. Fixed twice over (`.gitattributes` pins `*.sh`/`tools/*.py`
> to LF; the preflight refuses fewer than two parsed mutants). **Audited: every
> recorded sweep score parsed a real set, so nothing is withdrawn.** It was found
> on the first run under the recent worktree ruling, before it had a history to
> poison.
>
> ### Correctness findings that outrank the sizing
>
> * **Sacrifice skins to THREE bones; `zhao_geom_skin` does two.** Measured over
>   317,234 ring-vertices: 65.07% one bone, 32.41% two, **2.51% three**. Ours is
>   exact for 97.49% and clips the seam vertices (shoulders, hips, neck) where
>   error shows most. A rare second pass is probably cheapest. **A decision, not
>   a defect — but it must be taken deliberately.**
> * **Sacrifice's bone weights do NOT sum to a constant** (raw `ubyte`/64, all
>   256 values occur), so the blend is affine rather than convex — while our
>   `(pb<<6) + w0*(pa-pb)` identity is valid only when `w0 + w1 = 64`. Fine if
>   the asset pipeline normalises on import; that is now a stated precondition.
> * **Creature textures are 256 wide with arbitrary height up to 799**; only
>   12.7% are power-of-two in both axes. A TMU assuming square power-of-two
>   breaks on most character art.
>
> ### Blockers REMOVED today
>
> * **Widescreen is RULED** (not scheduled): `VIDEO_WIDE` = 384x216 displayed
>   from a 384x224 tiled canvas, exact 5x to 1080p; `WIDE_DUO` = 2 x 192x144.
>   Prerequisite recorded: "enum value 3 is free" is **false in practice** —
>   three `else-is-DUO` ternaries, a three-entry `ZHAO_TIMING` table, and a
>   `default:` arm that would make a fourth mode silently fetch nothing.
> * A latent out-of-bounds read in the reference oracle
>   (`zref_video.cpp` returned `kTable[mode & 3u]` from a three-entry table)
>   is **fixed**, provably golden-neutral.
>
> ### Still genuinely blocked on the owner
>
> * the three earth-field WRITE ops (`FIELD.WRITE.MATERIAL/NAV/HAZARD`), whose
>   law is unspecified — `TERRAIN.PATCH` sits downstream;
> * particle-simulation, compositor and 2D behaviour, reserved by standing
>   instruction;
> * `zref::rescale_s32` silently narrows `__int128` to `int64_t` in the shipped
>   skinning reference. Not a regression, unreachable with a real bone matrix
>   (0 of 24,000 pose-range coordinates). Three options in the docket; none
>   taken.
> * the scar-texture **pool size** — Sacrifice's `GetFreeScarTexture` /
>   `ReleaseScarTexture` prove a finite copy-on-write pool exists, but its
>   capacity is not recoverable. **`SURFACE.STAMP` is pool-bound, not
>   rate-bound**, so this is the number that matters for it.


> ## STATE AS OF 2026-08-22 (late). This block supersedes the two survey
> ## sections below it, which are now history.
>
> ### The composed shell after the saturating-counter rewrite
>
> | | previous run (`de2794d`) | now (`6d23c84`) |
> | --- | ---: | ---: |
> | setup worst | -0.729 ns | **-0.475 ns** |
> | failing endpoints | 97 | **56** |
> | ALMs | 7,667 | **7,415** |
> | hold | 0 failing | **0 failing**, +0.253 ns |
>
> Worst path 10.475 ns against a 10 ns target: the machine is presently good
> for about **95.5 MHz**. Five hand-written saturating counters became one
> proven `zhao_pkg::zhao_sat_add{64,32}`, which removed five wide borrow chains
> — including the `input_snapshot|seq -> gaps` family that had been the largest
> remaining one at 38 paths.
>
> ### DO NOT chase the remaining setup paths yet — owner ruling
>
> The ruled order (see `docs/OWNER_DOCKET.md`) is: **fix the GPU/video CDC seam
> first**, structurally, by moving the displayed CRC into `vid_clk` rather than
> crossing per-pixel state. Only then re-measure BALANCED against HIGH
> PERFORMANCE. Moving that logic changes placement enough that today's 56
> endpoints may not be tomorrow's 56, so tuning before it is measuring the
> wrong design. BALANCED remains the authoritative fitter configuration.
>
> ### Clock targets are now on record
>
> 120 MHz GPU fabric and a 150 MHz DDR memory interface — the GeForce 256 DDR
> part, the card Sacrifice shipped against. 120 MHz is 20.4% beyond today, on a
> design that is placement-bound rather than logic-bound, so it is architecture
> work rather than cleanup. The memory number cannot be costed at all until the
> board is frozen: the composed fit has zero package pins and all harness I/O is
> virtual.
>
> ### Blockers REMOVED by the 2026-08-22 rulings
>
> * **The five `FIELD.SEQ.*` profiles are not blocked and never were blocks.**
>   Ruled one engine, five profiles; they are `kind: profile` with
>   `implemented_by: FIELD.SEQ.CORE` under new rule V21. What remains for them
>   is lane binding, which is software and shell, not RTL.
> * **`GEOM.MESHFETCH`'s cull law is now defined.** "Visibility sectors" is
>   deleted — the phrase appeared only in the block's own purpose line. The law
>   is conservative per-camera frustum rejection of a BOUNDING SPHERE before
>   vertex decode, rejecting only when outside every active camera. Its LOD
>   third is already built (`zhao_geom_lod.sv`).
>
> ### Still genuinely blocked on the owner
>
> * the three earth-field WRITE ops (`FIELD.WRITE.MATERIAL/NAV/HAZARD`), whose
>   law is unspecified — and `TERRAIN.PATCH` sits downstream of them;
> * particle-simulation, compositor and 2D behaviour, reserved by standing
>   instruction.


> ## STATE AS OF 2026-08-22 (evening). Read this first; the survey below is
> ## from 2026-08-21 and parts of it are now history.
>
> ### The composed shell FITS and does NOT close timing
>
> | | measured | device |
> | --- | ---: | ---: |
> | ALMs | **7,648** | 41,910 (18%) |
> | registers | 9,753 | |
> | block memory bits | 114,688 (13 M10K) | 553 blocks |
> | DSPs | **0** | 112 |
> | setup worst | **-0.639 ns** | 10 ns target |
> | failing endpoints | **125** | 0 for closure |
> | hold | +0.250 ns, 0 failing | |
>
> Worst path 10.64 ns against 10 ns: **6.4% over**. At session start it was
> 65 ns and 3,746 endpoints. `reports/composed/wumen-e6b5fef-*` is the run.
>
> **The composed fit is deterministic** -- identical RTL reproduces identical
> numbers -- so every A/B in the log is signal, not noise.
>
> ### What is left, from a full negative-slack census (not a top-100 report)
>
> **245 negative-slack paths, down from 13,651.** 97 failing endpoints.
>
>     38 paths  input_snapshot|seq   -> input_snapshot|gaps        -0.351
>     28        scanout|fetch        -> mem_guard|fwd_req.addr     -0.143
>     21        cmd_dma|hdr_win      -> cmd_dma|crc_pay_r          -0.342
>     20        cmd_scheduler rq_rp  -> cmd_sched|dead_lim         -0.059
>     11        hps_arbiter|state    -> cmd_dma|crc_pay_r          -0.729  (worst)
>     11        hps_arbiter|state    -> cmd_dma|crc_hdr_r          -0.577
>      9        cmd_dma|m.M_SEED     -> cmd_dma|crc_hdr_r          -0.219
>      8        cmd_dma|pkt_len_r    -> recq                       -0.258
>
> **No family is over 0.75 ns and they are spread across five blocks.** This is
> fine-tuning territory, not a structural problem, and each attempt costs a
> ~25-minute fit to evaluate.
>
> ### Why I stopped here rather than continuing
>
> The design is **placement-bound below ~1.5 ns** -- proven by the determinism
> check plus three principled changes that each did what they were designed to
> do and moved the headline the wrong way. Two were reverted, one was kept for a
> stated reason. Further sub-nanosecond RTL changes are as likely to lose as
> win, and the next one on the list -- `input_snapshot|seq -> gaps` -- needs
> semantic reasoning about which cycle's state a once-per-frame counter samples,
> against a block carrying a documented "no gaps by construction" proof. That is
> not a change to make quickly.
>
> **The productive next moves are different in kind:** the CDC seam (docket), the
> fitter-effort basis (docket), or SDC work. Not more depth removal.
>
> ### Blockers that are GONE, and should not be re-derived
>
> * **CMD.DMA could not be fitted.** It needed 83,977 ALMs against 41,910. Its
>   staging buffer is real M10K now: 3,607 ALMs, and the block is 8.6% of the
>   device. The `blit_buf` async-read defect that `reports/composed/README.md`
>   names as THE composed-fit blocker no longer exists.
> * **The composed fit needed a bigger machine.** It was briefed at 42:33 and a
>   6.2 GB peak. Measured: ~4 minutes at 5.0 GB on this one. Both halves of
>   that brief are superseded.
> * **The bit-serial CRC.** `zhao_crc32c_step` chained 8 deep per beat and 28
>   deep for the payload seed -- 224 XOR levels -- and owned the two worst
>   timing families. Replaced by `zhao_crc32c_fold`, bit-exact against the
>   shipped CRC, with a `no_serial_crc` gate so it cannot return.
> * **The audio Gray-decode family.** Measured -14.9 ns once; absent from all
>   13,651 paths of the full census. Confirmed gone, not merely unreported.
>
> ### What is genuinely blocked, and on whom
>
> **On Fabian, in `docs/OWNER_DOCKET.md`:**
> 1. fitter effort -- 125 failing endpoints clean on hold, or 17 with two hold
>    violations. Both measured. One line either way.
> 2. the GPU/video crossing. `DEBUG.CRC.md` says the displayed lane is
>    video-domain; `design/blocks.yml` says GPU; the implementation followed
>    blocks.yml and built a direct per-pixel crossing. The documents disagree
>    and the code picked one. This is architecture, not SDC: a false path only
>    stops the tool reporting a real crossing, and `set_max_delay` addresses
>    setup rather than the hold failures seen.
> 3. the rasteriser, particle simulation, compositor and 2D blocks -- recorded
>    as needing game-behaviour decisions and deliberately not invented.
>
> **On nobody -- available work:**
> * ~~the guard range check~~ **DONE 2026-08-22.** The violation counter now
>   follows the registered pulse instead of the verdict, taking the range-check
>   chain off a 32-bit register bank's enable: 1,383 paths -> 28, and 125
>   failing endpoints -> 97. The no-escape proof still passes, which is what
>   made it defensible to touch at all.
> * ~~three Field IR pieces still unswept~~ **DONE 2026-08-22.** Reciprocal
>   23/23, sine/cosine 20/20, length/distance 21/21, no survivors. Every piece
>   of the Field IR engine now carries a mutation score.
> * ~~`TERRAIN.LOD.md` is wrong about its own block~~ **DONE 2026-08-22.**
>   Corrected, and the block MEASURED rather than counted: **2,086 ALMs and
>   28 DSPs -- a quarter of the device's 112**, confirming the DSP audit's
>   estimate for it. The old text said four ladder comparators and no ladder
>   multipliers; it is 12 and 24, because the count stopped at the `ladder()`
>   function instead of its four call sites. A reduction lever is recorded in
>   the contract: the strict ladder's `h` is the constant 256, so six of the
>   24 multiplies are shifts being spent as DSPs.
>
> ### A caveat that limits every timing number above
>
>     Unconstrained Input Ports       609
>     Unconstrained Input Port Paths  13,920
>
> Paths that START at an input port are not analysed. **-0.639 ns is a lower
> bound on the problem, not a measurement of all of it.**


**Date:** 2026-08-21
**Method:** each of the 24 remaining `SPECIFIED`, non-deferred, non-hardware-
blocked RTL blocks was trial-advanced to `REFERENCE_COMPLETE` in a scratch copy
of `design/blocks.yml`, the ledger check run, the errors recorded, and the file
restored. Dashboard-staleness errors are filtered out as noise.

## 2026-08-25 — WHAT IS ACTUALLY UNBUILT, derived from the ledger and the tree

The prose below counts **16 greenfield blocks**. That count is stale, and the
list naming `GEOM.PROJECT` as the cheapest of them is stale twice over:
`GEOM.PROJECT` is `UNIT_VERIFIED` with `zhao_geom_project.sv` on disk and a
669-line differential. This section is derived rather than remembered.

### The derivation, and its two failure modes, because neither method is trustworthy alone

**Matching block IDs to filenames over-reports gaps.** It called 23 blocks
unbuilt, including `MEM.HPS.ARBITER` (`zhao_hps_arbiter.sv` exists),
`GEOM.POSE` (`zhao_geom_pose_cache.sv`, `zhao_geom_pose_decode.sv`) and
`MEM.SDRAM` (`zhao_sdram_ctrl.sv`). The guess `zhao_mem_hps_arbiter` is simply
not what the file is called.

**Searching the RTL for each block ID under-reports them.** It called only 12
unbuilt, because an ID appearing in a file may be an upstream/downstream mention
in someone else's header rather than an implementation.

Two methods, opposite error directions, so the pair gives BOUNDS and neither
gives a count. The candidates that mattered were then checked individually --
and for every one below, the ID appears ONLY in other blocks' files:

    GEOM.MESHFETCH -> zhao_geom_cull.sv, zhao_geom_lod.sv, zhao_measure_governor.sv
    GEOM.VDECODE   -> zhao_geom_skin.sv
    GEOM.LOOM      -> zhao_geom_skin.sv
    GEOM.WARP      -> zhao_geom_skin.sv
    GEOM.WCACHE    -> zhao_project_core.sv, zhao_vertex_arena.sv, zhao_terrain_project.sv
    INPUT.SNAC     -> nothing at all

### The result

| | count | which |
| --- | ---: | --- |
| RTL_VERIFIED | 16 | done |
| have RTL, need EVIDENCE not code | ~32 | the bulk of the tree |
| **unbuilt and buildable now** | **6** | `GEOM.MESHFETCH`, `GEOM.VDECODE`, `GEOM.LOOM`, `GEOM.WARP`, `GEOM.WCACHE`, `INPUT.SNAC` |
| unbuilt, owner-blocked | 9 | 5x `PART.*`, 2x `TWOD.*`, 3x `POST.*` (minus overlap) |
| unbuilt, hardware-blocked | 4 | `SYS.PLL`, `SYS.RESET`, `SYS.CDC`, `MEM.SDRAM` |
| unbuilt, deliberately late | 1 | `MEASURE.HISTOGRAM`, charter §12 Version 2 |

**So "sixteen blocks to write from scratch" is wrong in both directions.** The
real code frontier is SIX blocks, five of them in geometry. What the tree mostly
needs is not more RTL — it is evidence promoting ~32 `UNIT_VERIFIED` blocks that
already have RTL, which is the slower and less glamorous half.

**`FIELD.SEQ.EARTH`'s exception is also closed.** The docket entry says its
`FIELD.WRITE.MATERIAL/NAV/HAZARD` ops have "no reference function, no RTL, and
no law pinned". All three now exist (`zhao_field_sinks.sv`, `compose_*`,
11/11 mutants). EARTH now sits exactly where the other four profiles do:
blocked on DECIDING the profile I/O contract, which is specification and is
Fabian's.

---

## The headline

**There are no cheap advances left.** `TERRAIN.BAKE` was the last block that was
finished but merely unrecorded — it advanced today with no work beyond
regenerating a diagram. `SURFACE.SHEET` and `TERRAIN.VELOCITY` each needed one
real test written. Everything after them needs the block itself built.

Today's advances went: `GEOM.SKIN`, `DEBUG.TRACE`, `SURFACE.SHEET`,
`TERRAIN.BAKE`, `TERRAIN.VELOCITY`, plus both halves of `GEOM.POSE`. That
exhausted the backlog of *built-but-unrecorded* work.

## The three shapes, and how many of each

### A. Greenfield — 16 blocks

`FIELD.PROGCACHE`, `GEOM.MESHFETCH`, `GEOM.PROJECT`, `GEOM.VDECODE`,
`GEOM.WCACHE`, `GEOM.LOOM`, `GEOM.WARP`, `MEASURE.HISTOGRAM`, all seven `PART.*`,
`FORGE.PRIM`, `TWOD.PLANE`, `TWOD.SPRITE`, `POST.GATHER`, `POST.COMPOSITE`.

Identical error shape every time:

- **V6** — both declared test paths do not exist;
- **V17** — the `reference_model` is a phantom, *and* the contract names no
  `zref::` symbol under "Scalar reference function".

So each one needs, in order: a reference (or a forward to the real law), the
contract's reference section, RTL, a differential, a random lane, a mutation
sweep. That is the full DEBUG.TRACE treatment, sixteen times.

**One of them is much cheaper than the rest.** `GEOM.PROJECT` cites
`zref::GeomProject`, which is a phantom — but `zref::render::project_vertex` is
real, is what `TERRAIN.PROJECT` already uses, and `TERRAIN.PROJECT` is already
`UNIT_VERIFIED` with working RTL. The ledger even records why the two are
separate: *"Kept separate from GEOM.PROJECT by architect ruling (1.D): merging
later is a trivial edit."* This is a kind-1 phantom with a proven neighbour.

> **UPDATE 2026-08-22.** This section's diagnosis is superseded. The three V10
> op blockers it names (`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB`) are gone — every
> op has a differential now — and the sequencer RTL it says does not exist is
> `zhao_field_seq.sv`, RTL_VERIFIED as of today on a formal proof of its
> anti-hang law.
>
> **Trial-advancing each profile now gives a precise, uniform answer:**
>
> | block | what actually blocks it |
> | --- | --- |
> | `FIELD.SEQ.WARP` | V6 only: `field_seq_warp_directed.cpp` / `_random.cpp` |
> | `FIELD.SEQ.FLOW` | V6 only |
> | `FIELD.SEQ.FORMATION` | V6 only |
> | `FIELD.SEQ.STAMP` | V6 only |
> | `FIELD.SEQ.EARTH` | V6 **plus** three ops whose law is unspecified — docket |
>
> V17 blocked all five this morning: their contracts carried a generated TODO
> where the oracle belongs. Filled today with `zref::fieldir::interpret`, which
> resolves — it forwards to `zfield::interpret`, and `zref_fieldir.hpp` names
> WARP as one of its users. Four of the five now need **only their tests**.
>
> **What a profile test has to pin** is in each contract now: a profile is not a
> different machine, it is the core sequencer wearing different I/O LANE
> BINDINGS. The op semantics are the interpreter's. So the test is not a second
> Field IR differential — it is a check that this profile binds the right
> registers to the right lanes.
>
> ### CORRECTION, same day: those four tests are NOT writable either
>
> I wrote above that four profiles "need only their tests" and said elsewhere
> that those tests are writable because a lane binding is a structural property.
> **Both halves are wrong, and checking is what showed it.**
>
> `in_lanes` and `out_lanes` are fields of `zfield::Decoded`, filled by the
> DECODER from the program image (`zfield_decode.cpp` ~line 331). The lane
> binding is a property of **the program**, not of the block. `zhao_field_seq`
> has no profile input and no profile-specific port; `field_seq_directed.cpp`
> supplies `in_regs`/`out_regs` per program, exactly as the reference does.
>
> So there is nothing in the sequencer that distinguishes W from F from M from
> S. Whatever separates the five profiles lives in the SHELL — which programs
> get loaded and what their lanes are wired to — and:
>
> * `ops.yml` defines each profile by name, description and sequencer, with **no
>   lane bindings**;
> * every profile contract's "## Input and output packet layouts" is still the
>   generated TODO stub.
>
> Writing `field_seq_warp_directed.cpp` therefore means DECIDING what the W
> profile's I/O contract is. That is specification, not testing, and for FLOW it
> is particle behaviour explicitly reserved.
>
> ### The question this raises, which is Fabian's
>
> If a profile is a program set plus shell wiring rather than a hardware
> variant, are these five blocks — or one block used five ways, with the
> profiles belonging to the shell that instantiates it? The ledger currently
> models five. Nothing in the RTL distinguishes them. Recorded on the docket
> rather than answered here, because collapsing or keeping five ledger entries
> is an architecture call.
>
> `FIELD.SEQ.EARTH` is the exception and it is NOT a test-writing job: see
> `docs/OWNER_DOCKET.md`. Its `FIELD.WRITE.MATERIAL/NAV/HAZARD` ops have no
> reference function, no RTL, and no law pinned beyond charter §11.2 naming the
> layers.

### B. Blocked on the Field IR sequencers — 5 blocks

`FIELD.SEQ.EARTH`, `FIELD.SEQ.FLOW`, `FIELD.SEQ.FORMATION`, `FIELD.SEQ.STAMP`,
and `TERRAIN.PATCH` downstream of them.

All four sequencers report the **same three** V10 blockers:
`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB` — the Field IR's base arithmetic ops, none
of which has a differential test. They are shared, so writing those three unlocks
the op layer for all four at once.

But the op tests are differentials, and a differential needs RTL to differ
against. No `FIELD.SEQ.*` block has any. **So the three op tests are not the
blocker; the sequencer RTL is**, and the op tests come with the first sequencer
that exists.

> **STALE AS OF 2026-08-25 — both halves of that paragraph have been overtaken,
> and it is corrected here rather than deleted so the reasoning stays readable.**
>
> * The three op blockers are **closed**. `tests/differential/field_alu_ops.cpp`
>   exists and `ops.yml` cites it for all three.
> * `FIELD.SEQ.CORE` **has RTL** — `zhao_field_seq.sv`, fitted at 36.84 MHz on
>   2026-08-25 — and the four profile sequencers carry
>   `implemented_by: FIELD.SEQ.CORE`. They are PROFILES of a block that exists,
>   not four unbuilt blocks, so "write the first sequencer" is already done and
>   what remains is per-profile evidence.
> * The three Earth sinks now have RTL and a differential
>   (`zhao_field_sinks.sv`, `field_sinks_directed.cpp`, 11/11 mutants caught),
>   which was the last op-layer gap specific to `FIELD.SEQ.EARTH`.
>
> `FIELD.SEQ.EARTH` is deliberately **NOT advanced past SPECIFIED** on the
> strength of this. Its ops are covered and its implementing block is
> RTL_VERIFIED, but there is still no end-to-end Earth8 PROGRAM differential,
> and op coverage plus a shared implementation is not the same evidence as the
> profile having been run.

`TERRAIN.PATCH` is a different case, documented in
`reports/PHANTOM_REFERENCES.md`: its three remaining op blockers are sinks that
belong to `FIELD.SEQ.EARTH`, and it is blocked on that block being built rather
than on anything of its own.

### C. Registered-but-intentionally-late — 1 block

`MEASURE.HISTOGRAM`. The ledger's own note says it: *"Charter §12 calls this
Version 2: registered now, built late."* Its blockers are real but so is the
decision not to build it yet.

## What this means for order

The wave order does not change, but the *cost* per block just became uniform and
much higher, and it is worth saying plainly: from here, "get through the waves"
means writing sixteen blocks, not clearing a backlog.

The cheapest next steps, in order:

1. **`GEOM.PROJECT`** — kind-1 phantom, real oracle, and a working sibling to
   pattern-match against.
2. **`FIELD.PROGCACHE`** — a cache, and the pose cache built today is a close
   structural analogue (tags, residency, counters, delegated storage).
3. **The first `FIELD.SEQ.*` sequencer** — expensive, but it unblocks four
   blocks plus `TERRAIN.PATCH`, and brings the three shared op tests with it.

The `PART.*` family (seven blocks) and the compositor family (four) are each a
coherent chunk that would be better done together than interleaved, since they
share references.

---

## Addendum: the Field IR sequencers are constrained, not merely unbuilt

`spec/form/field-ir.md` §1 carries a grep-audit law (charter §29-6) that changes
what "build FIELD.SEQ.*" is allowed to mean:

> Field IR *op semantics* exist in exactly two places — the C++ generic
> interpreter (`zfield::interpret`) and the TS interpreter … There is and shall
> be no third implementation: no hand-written per-program evaluator, no "faster"
> fused C++ variant, **no RTL-side re-derivation ahead of the profile engine
> (which will consume the same serialized bytes)**. A reviewer greps for the
> op-name switch outside those two files and must find none.

Read carefully, this is a design constraint rather than a prohibition. RTL is
foreseen — the parenthesis names "the profile engine" and says it consumes the
same serialized bytes. What is forbidden is a sequencer that re-derives op
semantics: a per-program hardwired evaluator, or an RTL opcode switch written
from the spec by hand.

So the five sequencer blocks are not just expensive, they have a required shape:
**a byte-code engine that executes `.zprog` images**, differentially verified
against `zfield::interpret` on the committed `.zvec` corpus. Anything that reads
like a second implementation of the op table will fail the grep audit by
construction, however well it tests.

Two consequences worth planning around:

1. The three shared op blockers (`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB`) come with
   that engine and are differentials against the same interpreter — they are not
   separate work.
2. The engine is one block's worth of effort that unblocks five. That makes it
   better value than its size suggests, and it is the reason the "first
   `FIELD.SEQ.*` sequencer" sits third on the cheapest-next list above rather
   than last.

`FIELD.PROGCACHE` is clear of this constraint: it caches and validates programs
and never evaluates one. Its validation half is already law —
`zfield::decode` with thirteen named error classes — so only its cache policy
needs deciding.

---

## Addendum 2: the cheap blocks are gone, and what that leaves

Since the map above was written, five greenfield blocks were built and verified —
`GEOM.PROJECT`, `FIELD.PROGCACHE`, `PART.EXPAND`, `PART.SOFT`, plus both halves of
`GEOM.POSE`. Four of the five were **kind-1** phantoms: the law already existed
under another name and only had to be found, cited and pinned.

**A systematic scan says there are no more of those.** Every remaining
`reference_model` was checked against the reference tree for a law shipped under a
different name. The results:

| Block | Is the law already shipped? |
| --- | --- |
| `GEOM.VDECODE` | **No.** Meshlets hold plain `SkinVertex` — there is no compressed form anywhere, and the ledger says the format belongs to `SW.TOOLS.ASSET`: *"one spec, two ends"*. |
| `POST.GATHER`, `POST.COMPOSITE` | **No.** `zref_aux.hpp` says of the distortion map that *"the offset arithmetic belongs to whoever"* — it is explicitly unassigned. Bloom, flash and grading exist only in the star/sky path, which is a different block's law. |
| `TWOD.SPRITE`, `TWOD.PLANE` | **No.** `blit_pattern_8x8` is a form-marker blit, not a HUD sprite pipeline with descriptors, affine and CLUT paths. |
| `GEOM.LOOM`, `GEOM.WARP` | **No.** Transform-graph evaluation and Warp8 deformation are unimplemented in software as well. |
| `PART.SPAWN/STATE/UPDATE/COLLIDE` | **No.** `zref::render::Particle` is a draw-time snapshot the renderer is HANDED. Nothing simulates particles. |
| `PART.LADDER` | Partly. The seven rungs are charter §9 and the counter lanes are `zref::measure`, but the ledger says the thresholds are *"provisional until Phase-10 evidence"* — the numbers are explicitly not ratified. |

## So the remaining 37 split three ways, and only one is mine to do alone

**1. Needs a spec another block owns.** `GEOM.VDECODE` is the clear case: the
vertex compression format has two ends and the pack side is `SW.TOOLS.ASSET`'s.
Inventing one end unilaterally would create exactly the kind of unratified law
this project keeps catching. `PART.LADDER`'s thresholds are the same shape —
recorded as provisional pending evidence that does not exist yet.

**2. Needs the Field IR engine.** Five blocks, one engine, required shape already
documented in addendum 1. This is large but it is unambiguous work: a byte-code
engine over `.zprog`, differentially verified against `zfield::interpret` on the
committed `.zvec` corpus. **It is the single highest-value remaining item** and
nothing about it needs a decision from anyone.

**3. Needs behaviour decided.** The four particle-simulation blocks, the two
compositor blocks and the two 2D blocks have no law in software, no ratified spec
section, and no donor behaviour to extract. Each one means choosing how the game
behaves — how a particle spawns, ages and collides; what bloom looks like — and
then writing that choice down as the reference before any RTL. That is design
work, and the choices belong to the person whose game it is.

## The honest statement of scope

"Finish the full hardware" is not one more sitting's work. Group 2 is the next
substantial thing I can do without input. Group 3 is roughly a dozen blocks whose
*behaviour* has never been decided, and doing them well means deciding it
deliberately rather than having me invent it and record the invention as law.

---

## 2026-08-21 (RESOLVED, same day) — CMD.DMA now synthesises

> **`Quartus Prime Analysis & Synthesis was successful. 0 errors.`** 21:06
> elapsed. This block had never once been successfully processed: the census
> row is `failed:quartus_map` (16.2 GB elaboration) and at HEAD it was
> `timeout` at 4,838 s.
>
> **THE FIX WAS NOT THE REDESIGN THIS SECTION CALLED FOR.** The analysis below
> was right about the cause and wrong about the remedy, and the correction is
> worth more than the original entry.
>
> The loop is bounded at **192**. The reachable maximum is **64**:
> `fetched` is zeroed when the fetch is accepted, `M_HDR_REQ` issues exactly
> ONE burst, `burst_len` caps at 64 bytes, and `M_HDR_WAIT` adds 8 per beat and
> leaves on `last`. So iterations 64..191 had their `k < seed_end` guard false
> in **every reachable state** — 128 steps of unreachable logic that synthesis
> had to build a ~1,248-stage dependent chain for before discarding.
>
> Bounding the loop at 64 is **exactly equivalent**, and the diff is one
> number. No incremental CRC state machine was needed. The bound is now
> asserted in the formal cone rather than argued in prose, because it is the
> reason the loop is safe.
>
> Evidence: 43 directed + 139,113 random checks (1,000 frames of packets);
> mutation sweep 11 / 10 caught / 1 recorded equivalent / 0 discarded.
>
> **The lesson is about the diagnosis, not the bug.** "156 dependent CRC steps"
> was measured and true. "Therefore it needs an incremental CRC redesign" was
> inferred and false — nobody had asked how many of those steps could actually
> execute. A cone that large is worth a bound check before it is worth a
> rewrite.
>
> **SYNTHESIS IS FIXED; PLACEMENT IS NOT.** The same run then failed the
> FITTER at 2,839 s — a real failure, not a timeout (the limit is 3,000 s and
> the exit was non-zero). So this block now reaches two stages further than it
> ever has, and `failed:quartus_fit` is the new wall.
>
> | attempt | result |
> | --- | --- |
> | census (`96c0394`, with `blit_buf`) | `failed:quartus_map`, 16.2 GB |
> | HEAD after step 6 | `timeout`, 4,838 s |
> | HEAD + bounded CRC loop | **synthesis 0 errors**, then `failed:quartus_fit`, 2,839 s |
>
> **The cause is NOT yet established and is deliberately not recorded here as
> if it were.** The workspace is auto-deleted on success paths, so the fitter's
> own error was not captured; the next run keeps it.
>
> A PREDICTION, held as a prediction: `slot_buf` still has `blit_buf`'s defect
> in miniature — initialiser, async-reset write, combinational read — so it
> does not infer as RAM at 4,096 entries, and
> `assign pkt_byte_o = slot_buf[rd_off]` is a **4,096:1 byte mux**, on the order
> of 32,760 LUTs against a 41,910-ALM device.
>
> That is exactly the shape of reasoning that was wrong about the CRC loop an
> hour earlier: a true measurement, an inferred remedy, written down as
> required work. The fitter's error decides it, not this paragraph.
>
> ### THE FITTER'S ERROR, CAPTURED
>
> ```
> Error (170011): Design contains 95328 blocks of type combinational node.
>                 However, the device contains only 83820 blocks.
> Error (11802): Can't fit design in device.
> ```
>
> **This one block needs 114% of the whole device's combinational capacity.**
>
> The prediction above was directionally right and badly undersized: it named
> the read mux at roughly 32,760 LUTs, and the measurement is nearly three
> times that. The half it did not name is probably the larger one —
> `slot_buf[wr_off + 32'(i)] <= ...` writes eight bytes at a **variable index**
> into a 4,096-entry array, which is a 4,096-way write decoder on top of the
> 4,096:1 read mux. So: predicted the cause, missed the dominant term. Recorded
> that way rather than as a hit.
>
> ### The fix is already written down in the RTL, and was deferred
>
> `zhao_cmd_dma.sv` says of this array:
>
> > "Still NOT an M10K, and the contract says why: the write lives in an
> > async-reset process and the read is combinational, and an M10K has no reset
> > port and a registered read. Fixing that is a protocol change (the beat
> > stream needs a one-cycle read lead) and is deliberately not done here."
>
> That is the same defect `blit_buf` had, the same one `zhao_scanout_linebuf`
> was cured of by moving to `zhao_dc_sdp_ram` with a registered read, and the
> same one Quartus Error 276003 named on the composed shell. **Three memories,
> one defect, and this is the last of them.**
>
> **What it blocks:** the composed fit contains `CMD.DMA`, so Step 8 remains
> gated. Synthesis is no longer the obstacle; placement is, and the remedy is
> the known protocol change rather than anything new.
>
> **What is now known that was not:** the block synthesises, so the CRC cone
> was a real and separate problem, and the remaining cost is entirely
> `slot_buf`'s shape. That is a much smaller and better-specified piece of work
> than "CMD.DMA cannot be fitted".
>
> ### ATTEMPT 1: re-describe as 512 x 64 words. MEASURED WORSE. REVERTED.
>
> | shape | combinational nodes (device has 83,820) |
> | --- | ---: |
> | `logic [7:0] slot_buf [0:4095]` (shipped) | **95,328** |
> | `logic [63:0] slot_buf [0:511]` + byte accessor | **109,350** |
>
> The reasoning was: both sides move aligned 8-byte groups, so a write becomes
> ONE word and the 4,096-way write decoder disappears, while constant-offset
> reads fold to constant slices. That reasoning predicted a large reduction. It
> was wrong by 14,022 nodes IN THE WRONG DIRECTION.
>
> The change itself was sound and bit-identical — lint clean, 43 directed and
> 139,113 random checks, and `cmd_random`'s transcript hash unchanged at
> `0xb95b5f70a413bdbd` across 1,000 frames. It was reverted because the
> measurement rejected it, not because it was incorrect.
>
> **Why it grew is NOT established, and this entry does not guess.** Two
> inferences about this block have already been wrong tonight — "the CRC cone
> needs an incremental redesign" (it needed a bound check) and "words will
> shrink it" (they enlarged it). A third guess written down as fact would be
> the pattern, not the exception.
>
> ### What the measurements DO establish
>
> **A re-description does not fix this. Only a real memory does.** Both shapes
> are register arrays with a combinational read, and both overflow the device
> by themselves. The remedy has been written in the RTL from the start:
>
> > "the write lives in an async-reset process and the read is combinational,
> > and an M10K has no reset port and a registered read. Fixing that is a
> > protocol change (the beat stream needs a one-cycle read lead)."
>
> That is the work: a registered read, a one-cycle lead in the beat stream, and
> the initialiser dropped so the array can infer as RAM — the same cure
> `zhao_scanout_linebuf` received via `zhao_dc_sdp_ram`. It is a protocol
> change touching `CMD.DECODER`'s byte stream, which is why it was deferred
> originally and why it is not a same-session edit.
>
> **Step 8 remains gated.**
>
> ### ATTEMPT 2: split the readers so `slot_buf` can be a memory. FAILED, and
> ### it corrects the recorded remedy.
>
> The remedy recorded above — and in the RTL since the block was written — is
> "a registered read and a one-cycle lead in the beat stream". **That is
> incomplete, and the reason is why this block resists becoming a memory at
> all.**
>
> A RAM has one or two ports. `slot_buf` has **three independent
> arbitrary-offset readers** plus the write:
>
> | reader | offset | line |
> | --- | --- | --- |
> | the streamed byte | `rd_off`, walks the packet | `pkt_byte_o` |
> | header fields, header CRC, payload-CRC seed | below 64 | `hget*`, `crc_final` |
> | **the payload CRC compare** | `36 + command_bytes` — anywhere | `hget32(36 + cb)` |
> | **the RECORD WALK** | `36 + walk_off` — anywhere | `hget16(36 + walk_off)` |
>
> I attempted the obvious split: a 64-byte register window shadowing the
> header, leaving `slot_buf` with the stream as its only reader. The premise
> was that every non-streaming reader lives below byte 64. **It does not.** The
> record walk random-accesses record headers throughout the payload, and the
> payload CRC compare reads at an offset that depends on the packet's length.
>
> Measured: `cmd_dma_directed` 8 of 43 checks failed. Note that `cmd_random`
> PASSED with an identical transcript hash, so the directed lane is what caught
> it — the random lane never built a packet whose walk reached past the window.
>
> Reverted.
>
> ### What the remedy actually is
>
> Not one registered read but **three**, each needing a cycle of lead in the
> state machine that uses it:
>
> 1. the streamed byte (`M_STREAM` pre-issues the next address);
> 2. the record walk (`M_WALK` presents an address and waits a cycle before
>    reading `op`/`rb`);
> 3. the payload CRC compare.
>
> Plus the write moved into a process with no async reset and the initialiser
> dropped, or the array cannot infer as RAM regardless of the reads.
>
> **The alternative worth weighing first** is to stop random-accessing the
> buffer at all: have the record walk consume the streamed bytes rather than
> re-read them, which is how a decoder normally works and would leave one
> reader by construction. That is a larger change to `CMD.DMA` and possibly to
> the `CMD.DECODER` seam, and it is a design decision rather than a repair.
>
> **This is the third theory about this block to be corrected by evidence** —
> after "the CRC cone needs an incremental redesign" (it needed a bound check)
> and "words will shrink it" (they grew it). The pattern is consistent: the
> measurements are reliable, the inferences from them are not, and each one
> only fell over when something ran.
>
> ### ATTEMPT 3: share the variable-offset readers. NO EFFECT. Fourth theory
> ### falsified.
>
> | shape | combinational nodes (device has 83,820) |
> | --- | ---: |
> | baseline: byte array, payload CRC inside `M_WALK` | 95,328 |
> | `slot_buf` as 512 x 64 words | 109,350 |
> | payload CRC lifted into its own state `M_PCRC` | **95,306** |
>
> **Twenty-two nodes.** The reasoning was that nine variable-offset byte muxes
> could become four: `hget32(36 + cb)` is four, the two `hget16(36 + walk_off)`
> calls are two each, the stream is one, and the CRC compare shared a state
> with the walk so both were live at once. Splitting it is structurally right
> and behaviourally identical, and it is **not where the cost is**.
>
> ### What is now known, and what is not
>
> **Known:** the read muxes do not dominate. Three separate attempts to reduce
> read cost moved the total by 22 nodes, 0, and -14,022 respectively.
>
> **NOT known, and deliberately not asserted:** what does dominate. The
> obvious remaining candidate is the WRITE — eight bytes at a variable offset
> into a 4,096-entry array is a 4,096-way decoder — but the word
> re-description should then have helped, and it made things 14,022 nodes
> worse. **That contradiction is unexplained, and a fifth guess written down as
> fact would be the pattern rather than the exception.**
>
> ### The recommendation
>
> **Stop reshaping the RTL and change the architecture, or measure what Quartus
> is actually building.** Four attempts have moved the number by 0.02%,
> -14.7%, and 0.02%. The block needs `slot_buf` to become a real memory, and it
> cannot while three independent readers random-access it.
>
> The architectural fix is to stop random-accessing it: have the record walk
> CONSUME the streamed bytes rather than re-read them, which is how a decoder
> normally works and leaves one reader by construction. That changes `CMD.DMA`
> and possibly the `CMD.DECODER` seam, so it is Fabian's design call and not a
> repair to be made unilaterally.
>
> The cheaper diagnostic first step, if the call is deferred: get a resource
> breakdown out of Quartus rather than inferring one from the source. Four
> theories have now been wrong, and every one of them was an inference about
> what the fitter was building.
>
> ### THE MEASUREMENT, TAKEN. It settles the question.
>
> `blockfit.map.rpt`, from the run that synthesised cleanly:
>
> ```
> Estimate of Logic utilization (ALMs needed) : 83,977      device has 41,910
> Combinational ALUT usage for logic          : 94,698
>     -- 6 input functions                    : 47,694
>     -- <=3 input functions                  : 39,525
> Dedicated logic registers                   : 33,680
> Total block memory bits                     : 0
> ```
>
> **Two lines settle it.**
>
> `Total block memory bits: 0` — **no RAM was inferred at all**, which is the
> defect stated as a measurement rather than a suspicion.
>
> `Dedicated logic registers: 33,680` — and `slot_buf` is 4,096 x 8 =
> **32,768** of them. The block is a 4,096-entry REGISTER FILE with a variable
> read address and a variable write address, and that shape costs ~95k ALUTs on
> a device holding ~42k ALMs. It needs **twice the device**.
>
> ### Why every mux attempt failed, now explainable
>
> The cost is not one shareable mux. It is the register file itself: 32,768
> registers each needing hold/load selection (the 39,525 <=3-input functions
> are about that size) plus the address decode and read trees (the 47,694
> 6-input functions). Sharing readers moves a few thousand ALUTs around a
> ~95,000 ALUT structure, which is exactly the 0.02% the two split attempts
> measured.
>
> **No reshaping of the reads can fix this. `slot_buf` must become BLOCK
> MEMORY**, and the target is unambiguous: `Total block memory bits` must go
> from 0 to 32,768.
>
> ### What that requires, from the M10K rules already recorded in this repo
>
> 1. no initialiser on the array (`= '{default: 8'h00}` must go);
> 2. the write in a process with **no async reset** — M10K has no reset port;
> 3. a **registered** read, one per port, and at most two ports.
>
> Point 3 is the hard one and the reason this is a design change rather than a
> repair: three readers random-access the array today. They are now in three
> DIFFERENT states after the `M_PCRC` split, so they CAN share one port with a
> state-muxed address — that split was not wasted, it is the precondition —
> but each consumer must then wait a cycle for its answer, and `hget16`/`hget32`
> need two to four CONSECUTIVE bytes, so a byte-wide port turns each into
> several sequential reads and more states.
>
> **The estimate is worth stating: moving 32,768 bits into M10K removes on the
> order of 95,000 ALUTs from a block that currently needs 2x the device.** That
> is the whole gap, not a contribution to it.
>
> ### THE DESIGN, NOW FULLY SPECIFIED. A 512 x 64 RAM serves every access in
> ### ONE read.
>
> The objection to a single port was that `hget16`/`hget32` want two to four
> CONSECUTIVE bytes, so a byte-wide port would turn each into several
> sequential reads. **Checked, and it does not arise:** every multi-byte access
> in this block is contained in one 64-bit word.
>
> | access | offset | lands in |
> | --- | --- | --- |
> | header fields | 0, 4, 6, 8, 12, 16, 20, 24, 28, 32 | one word each |
> | `hget32(36 + cb)` | `cb` is a multiple of 16 (records are 16-byte multiples, enforced by `rb_v & 0xF`), so the offset is 4 mod 8 | one word |
> | `hget16(36+walk_off)` **and** `hget16(36+walk_off+2)` | same reasoning | **the SAME word** — one read serves both |
> | the write | `wr_off` advances by 8 from zero | exactly one word |
> | the stream | any byte | one word + byte select |
>
> A 32-bit field at an offset that is 0 or 4 mod 8 fits entirely inside one
> 64-bit word; a 16-bit field at 0, 2, 4 or 6 likewise. **No access straddles a
> word boundary**, so no access needs two reads.
>
> ### Why the earlier 512 x 64 attempt still failed
>
> It had the right SHAPE and none of the RAM properties. It kept the
> initialiser, kept the write inside the async-reset process, and kept the read
> combinational, so nothing inferred and the array stayed 32,768 registers with
> the mux cost merely rearranged. **Width was never the missing piece;
> inference was.**
>
> ### The remaining work, in full
>
> 1. `logic [63:0] slot_ram [0:511]`, **no initialiser**;
> 2. the write in an `always_ff @(posedge clk)` with **no reset** — one word
>    per bridge beat, which is what a beat already is;
> 3. **one registered read port**, address muxed by state. The readers are
>    already in four distinct states after the `M_PCRC` split, so they cannot
>    collide;
> 4. one cycle of lead in each consumer.
>
> ### CORRECTION, and it makes the change much smaller
>
> Point 4 above said the two CRC walks each become a four-step read loop. **That
> is wrong, and reading the block properly is what showed it.** Both walks, and
> every header field, live inside the first 64 bytes:
>
> * `crc_final()` covers bytes [0,32);
> * the payload-CRC seed covers [36,64) — bounded at 64, proven reachable-max;
> * every header field sits below offset 40;
> * and the payload CRC for the REST of the packet is already computed from the
>   BUS DATA as beats land (`M_PAY_WAIT`), never re-read from the array.
>
> So the entire header path reads only bytes 0..63. **Keep those 64 bytes in
> registers as a header window — 512 registers, nothing — and `M_HDR_CHK` does
> not change at all.** No read loop, no extra states, no re-timing of the check
> ladder.
>
> Only THREE consumers then need the RAM, and each wants exactly one word:
>
> | state | read | address |
> | --- | --- | --- |
> | `M_PCRC` | `hget32(36 + cb)` | `(36+cb) >> 3` |
> | `M_WALK` | both `hget16`s | `(36+walk_off) >> 3` — one read serves both |
> | `M_STREAM` | `pkt_byte_o` | `rd_off >> 3`, a new word only every 8 bytes |
>
> `M_STREAM` gets seven cycles of lead on each next word, so only its first
> read needs a stall. That is roughly 80 lines, not a rewrite of the packet
> path, and the earlier estimate should not have been written before the block
> had been read end to end.
>
> **Everything needed to write this is now known, and none of it is a design
> decision** — the alignment argument above removed the one question that was.
>
> ### DONE, AND MEASURED. 2026-08-22.
>
> Built as described. `quartus_map`, same flow and same device as the
> measurement that opened this section:
>
> | | before | after | device |
> | --- | --- | --- | --- |
> | Estimate of Logic utilization (ALMs needed) | 83,977 | **3,519** | 41,910 |
> | Combinational ALUT usage for logic | 94,698 | **4,551** | |
> | Dedicated logic registers | 33,680 | **1,521** | |
> | **Total block memory bits** | **0** | **32,768** | |
>
> The target named above — block memory bits from 0 to 32,768 — was hit
> exactly, and it took the logic with it: **24x fewer ALMs**, from 2.0x the
> device to 8.4% of it. `Total MLAB memory bits: 0`, so the 32,768 bits are in
> real M10K, not distributed memory pretending.
>
> A second confirmation that the array is genuinely gone: `quartus_map`'s log
> for this block was 9.8-11 MB on every prior run and is **571 KB** now.
>
> ### AND IT FITTED. Not an estimate — the fitter placed and routed it.
>
> `quartus_fit`, 1,038 s, recorded in `reports/synthesis/zhao_block_fit.json`:
>
> ```
> zhao_cmd_dma   ok   ALM 3607 / 41910      (8.6% of the device)
>                     registers        1571
>                     blockMemoryBits 32768
>                     ramBlocks           4 / 553      M10K, not MLAB
>                     dspBlocks           0
> ```
>
> "Fitter placement was successful" is a line this block had never produced.
> Router estimated **average interconnect usage 2%**, peak 28% in one region.
> The block that could not fit is now one of the smallest in the design.
>
> The shape built: a 64-byte header window in registers serving the whole
> header ladder unchanged, plus `slot_ram` 512 x 64b with one registered read
> port, its address muxed across the four states that read it — `M_PCRC_RD`,
> `M_WALK_RD`, `M_STREAM_RD` and the stream's own one-word lookahead. Reads
> cost one cycle of lead each; the walk takes two cycles per record instead of
> one, and the stream fetches a word every eight bytes with seven cycles of
> slack.
>
> **This unblocks the composed fit and FRAMEBLIT step 8.** CMD.DMA was the
> block that could not fit; it is now smaller than most of the design.

## 2026-08-21 — CMD.DMA still cannot be fitted, and the cause is a design defect (SUPERSEDED, kept for the reasoning)

**Measured, not inferred.** After step 6 removed the 1.97 Mbit `blit_buf`,
`zhao_cmd_dma` was re-fitted at HEAD. It did **not** succeed:

| | result |
| --- | --- |
| census (96c0394, with `blit_buf`) | `failed:quartus_map` — 16.2 GB elaboration |
| HEAD (no `blit_buf`) | **`timeout` — 4,838 s without finishing** |

Removing the buffer moved this block from *immediate failure* to *does not
finish*. That is progress on the memory axis and **not a fix**.

**A CORRECTION.** The step 6 commit called that removal "the composed-fit
unblock". It is not, on two counts: the QSF source-list drift was a second
blocker (fixed, and now gated by `tests/lint/source_list_parity`), and this
block still cannot be characterised at all.

### The cause

`zhao_cmd_dma.sv`, in the state machine's `always_ff`:

```systemverilog
for (int unsigned k = 36; k < 192; k++) begin
  if (k < seed_end) begin
    cseed = zhao_abi_pkg::zhao_crc32c_step(cseed, slot_buf[k]);
  end
end
```

**156 dependent CRC-32C byte steps unrolled into one clock cycle** — about
1,248 chained XOR/shift stages in a single combinational cone, each with a
guarded read of a 4,096-entry register array. Synthesis is not running out of
memory; it is trying to flatten a 156-deep serial dependency chain.

The `crc_final()` helper immediately above it carries a note showing the
authors knew about this class of problem: *"the loop bound is the constant 32;
a parameter-bounded loop would put SLOT_BUF_BYTES muxed CRC steps in the formal
cone for nothing."* The 32-step loop was bounded deliberately. The 156-step one
was not.

**This is not only a synthesis-time problem.** A 156-byte serial CRC chain in
one cycle will not close timing on any device. The block is unsynthesisable as
written, and the per-block census never said so because this block has never
been successfully fitted.

### What it blocks

The composed fit contains `CMD.DMA`, so **the composed fit cannot be expected
to complete until this is fixed.** Step 8 of the FRAMEBLIT integration is
gated on it.

### RESOLVED 2026-08-28. Verified against the RTL and the fit, not assumed.

**This entry described the fix as "NOT yet done, and it is a real redesign".
It has since been done, and leaving the entry standing would send the next
reader to redo finished work.** Checking before starting it is the only
reason that duplication was avoided.

Both halves are in the tree:

* **The payload CRC accumulates incrementally.** `zhao_cmd_dma.sv`'s fold mux
  has a `default` arm taking `fold_c = crc_pay_r`, `fold_d = hps_rsp_i.data`,
  `fold_is4 = beat_tail` -- one bridge beat per cycle -- and a dedicated
  `M_SEED` state seeds it ahead of the ladder rather than behind it
  (commits 59de7ca, 1bfca73).
* **The staging buffer infers as a RAM.** It is `slot_ram`, 512 x 64b, and
  carries the shape explicitly: *"M10K: no initialiser on the array, no reset
  in this process, registered read. One word per bridge beat, which is
  exactly what a beat is."*

The measurement that settles it, from `reports/synthesis/zhao_block_fit.json`:

| block | status | ALM |
| --- | --- | --- |
| `zhao_cmd_dma` | **ok** | 3,607 |
| `zhao_debug_frameblit` | **ok** | 962 |

The block that "has never been successfully fitted" now fits. **FRAMEBLIT
step 8 is no longer gated on this.**

One caution kept rather than dropped: those rows carry no Fmax, so this says
the block PLACES, not that it closes timing. The composed fit and the timing
claim remain separate questions.

### The shape of the fix, as it was (kept for the record)

The payload CRC seed must be computed **incrementally across cycles**, the way
the fetch path already accumulates `crc_pay_r` one bridge beat at a time,
rather than re-walked over the staging buffer in one cycle. `slot_buf` also
still has `blit_buf`'s original defect in miniature — an initialiser, an
async-reset write process and a combinational read — so it will not infer as a
RAM either, at 4,096 entries instead of 30,720.

Both wanted the same treatment and only the large one got it.

---

## 2026-08-22 (night) — WHAT IS ACTUALLY LEFT, counted rather than estimated

Twenty-three RTL blocks are still `SPECIFIED`. The instinct is to read that as
twenty-three units of implementation work. **It is not.** Measured against the
reference tree and the contract stubs:

| | count |
| --- | ---: |
| RTL blocks still SPECIFIED | **23** |
| ...whose declared oracle RESOLVES | **2** |
| ...whose contract is fully filled | **3** |
| ...with neither (all 15 sections still generated stubs) | **20** |

Broken down by what each is actually waiting on:

| what it needs | blocks | count |
| --- | --- | ---: |
| **an owner decision on game behaviour** (reserved by standing instruction) | `PART.STATE/UPDATE/COLLIDE/SPAWN/LADDER`, `TWOD.PLANE/SPRITE`, `POST.GATHER/COMPOSITE/ECHO`, `FORGE.PRIM` | **11** |
| **a different KIND of evidence than the ledger can express** | `SYS.PLL`, `SYS.RESET`, `SYS.CDC` | **3** |
| **a format or policy decision, then ordinary work** | `GEOM.VDECODE`, `GEOM.LOOM`, `GEOM.WARP`, `GEOM.WCACHE` | **4** |
| **another block first** | `TERRAIN.PATCH` (the earth-field WRITE law), `MEM.SDRAM` (`blocked_on: hardware`) | **2** |
| **a ratified law — the charter gives only a sketch** | `MEASURE.HISTOGRAM` | **1** |
| **in flight** | `GEOM.MESHFETCH` (cull) | **1** |

**So roughly half the remaining hardware is waiting on decisions that are the
owner's by standing instruction, and almost none of it is waiting on typing.**

### MEASURE.HISTOGRAM is Version 2, and Version 1 is already built

The charter (§ error-bucket loop) gives it three bullets:

> - FPGA builds a small histogram of candidate error buckets;
> - a cutoff bucket is selected;
> - eligible refinements above the cutoff are emitted.

That is an ingredient list. It does not say how many buckets, how an error maps
to a bucket, or how the cutoff is chosen — and each of those is a determinism-
and budget-affecting choice. The same section explicitly labels this **Version 2**
and says *"Do not begin with a global FPGA priority heap"*; **Version 1**
(`MEASURE.GOVERNOR` + `MEASURE.TOKENS`) is implemented and `UNIT_VERIFIED`. So
this block is neither blocked nor urgent — it is a deliberate future upgrade, and
starting it would mean inventing the three numbers the charter left open.

### The SYS.* three are a LEDGER defect, not a work item

`reports/PHANTOM_REFERENCES.md` already called this out under kind 3: a PLL has
no scalar model, and a reset sequencer's correctness is a timing and sequencing
property, not a function from inputs to outputs. Naming a C++ function for them
was a category error at specification time.

The report's own recommendation was to fix `tools/ledger` **"before those blocks
come up, not while they are being built."** They are phase 0 — they came up long
ago, and all three still carry a fictional `reference_model` because the schema
offers one evidence field and one ladder. That is the next piece of ledger work,
and unlike the eleven above it needs no decision from anyone.

---

## 2026-08-23 — MEASURED: the DSP demand is 213 against 112, and it is growing

`zhao_geom_cull` fitted clean (1,102 ALMs, **15 DSPs**, 1,434 registers). Adding
it to the per-block report makes the running total impossible to ignore:

| block | DSPs |
| --- | ---: |
| `zhao_terrain_project` | 33 |
| `zhao_surface_stamp` | 28 |
| `zhao_terrain_lod` | 28 |
| `zhao_texture_tmu` | 28 |
| `zhao_geom_lod` | 18 |
| `zhao_terrain_normals` | 18 |
| `zhao_geom_cull` | **15** |
| `zhao_geom_binner` | 12 |
| `zhao_raster_fragment` | 10 |
| `zhao_texture_bilerp` | 7 |
| ...five more | 16 |
| **total, 15 blocks measured** | **213** |
| **device 5CSEBA6U23I7 has** | **112** |

**Nearly twice the device, and only fifteen blocks have been measured.** The
2026-08-20 session already found this ("DSP is the binding constraint: 171
against 112, and it had no budget"); it is now 213, because `zhao_geom_lod` (18)
and `zhao_geom_cull` (15) have been built since.

### What this number is and is not

**It is an UPPER BOUND, honestly.** `tools/quartus/run_composed_fit.ps1` says so
about ALMs and the same applies here: per-block fits give each block its own
multipliers with no cross-block sharing and no resource inference across a
composed cone. The composed shell currently reports **0 DSPs**, because none of
these geometry, terrain or texture blocks is integrated into it yet.

**But it is not comfortable.** The blocks driving it are not alternatives —
project, LOD, normals, the TMU, bilerp and the binner are all core rendering, all
present at once in any frame that draws terrain. A 1.9x overshoot is not a
rounding error that composition absorbs.

### The lever, and it is known to work

Every one of these numbers is *parallel multipliers where the rate does not
require them*. `zhao_geom_lod` went from **28 DSPs to 18** simply by narrowing
operands from a slack 72 bits to a proven-sufficient 64 and sharing two products
that were being computed twice. Its remaining 18 come from five 32x32 products
evaluated in parallel for a block that runs **once per instance per frame** —
sequencing them through one multiplier over three clocks should reach roughly 8.

`zhao_geom_cull` is the same shape: four multipliers on the instance path, which
is 10 cycles for a block whose rate is per-instance, not per-pixel.

So the honest reading is that the 213 is mostly slack rather than arithmetic, and
the reduction work is known and mechanical — but it is real work across a dozen
blocks, and **nobody has done it because nothing forced the question until the
per-block numbers were summed.**

### What this changes

It moves DSP reduction from "a thing to do eventually" to **a gate on the design
fitting at all**, alongside timing closure.

### Is the 213 stale? Checked: no.

Eleven of the fifteen rows were measured at `96c0394a`, **189 commits ago** —
the 2026-08-20 session that first found DSP was the binding constraint. That is
the obvious objection to the total, so it was tested rather than argued:

```
git rev-list --count 96c0394a..HEAD -- <the block's .sv>
```

returns **0 for every one of them** — `terrain_project`, `surface_stamp`,
`texture_tmu`, `terrain_normals`, `geom_binner`, `raster_fragment`,
`texture_bilerp`. Not one of those RTL files has been touched since it was
measured. The measurements are old in wall-clock time and **current in the only
sense that matters**.

The other four are recent by construction: `terrain_lod` (33 commits),
`geom_lod` (17), `geom_cull` (4).

So the 213 is not an artifact of stale data. It is what the design asks for
today, subject only to the per-block upper-bound caveat above.

### And the ledger is completely blind to it — checked, not assumed

| | count |
| --- | ---: |
| blocks declaring `resource_budget.dsp_percent` | **0** |
| blocks recording `resource_actual.dsp` | **0** |
| ledger rules that read a DSP figure | **0** — V5 sums ALM percentages only |

Both fields exist in the schema and in `tools/ledger/src/types.ts`. Nothing
writes them and no rule reads them. **That is exactly how a design reaches 1.9x
the device's DSP capacity with every gate green**: the numbers were measured,
written into `reports/synthesis/zhao_block_fit.json`, and never carried back
into the ledger that is supposed to be the schematic.

The ALM side does not have this hole — V5 sums `alm_percent` per budget group
and enforces a total ceiling, which is why ALM growth has never surprised
anyone here. DSP has no equivalent.

---

## 2026-08-23 — MEASURED: the sequencing lever works, and it is bigger than the
## estimate. 213 is now 201.

The section above named the lever and guessed at its size: "sequencing them
through one multiplier over three clocks should reach roughly 8." It was piloted
on `zhao_geom_lod` and measured on both sides, same tool, same provisional
device, clean worktree at each commit:

| | ALMs | DSPs | registers | latency |
| --- | ---: | ---: | ---: | ---: |
| before (`d8278bd`) | 1,303 | **18** | 21 | 1 clock |
| after (`09bbe05`) | 1,183 | **6** | 271 | 5 clocks |
| delta | **-120** | **-12 (-67%)** | +250 | +4 |

**Six, not eight.** The estimate assumed only the three legality products would
be shared. All FIVE went through one multiplier -- thresh*R and the boundary
product too -- because once a sequencer exists the other two are free to join
it. The block fell from **16% of the device's DSPs to 5.4%**.

### The ALMs went DOWN, which was not expected

+250 registers and -120 ALMs at the same time. Cyclone V ALMs carry registers
that were sitting unused, so the flops packed into logic that already existed;
and the five parallel 64-bit product/compare datapaths that the ALMs were
actually spent on collapsed into one. **Sequencing did not trade area for
DSPs here. It returned both.** That removes the objection that would otherwise
have to be argued block by block.

### What it cost, stated plainly

The block answers in **5 clocks instead of 1**, and grew a real `ready_o`. Its
rate is one evaluation per instance per frame; at 50 MHz that is 10 M
evaluations/s against a demand of about 600 k/s for ten thousand live creatures.
The parallelism was never bought by the rate.

### Where this generalises, surveyed rather than assumed

The decisive question for each remaining offender is whether its RATE consumes
the parallelism, not whether the products are independent:

| block | DSPs | stated rate | rate met? | slack |
| --- | ---: | --- | --- | --- |
| `zhao_terrain_lod` | 28 | 1 decision per patch per **frame** | yes, ~560 cyc/patch | **enormous** — 30 products live permanently, used 1 cycle in 34 |
| ~~`zhao_texture_tmu`~~ | ~~28~~ → **6** | ~~1 sample/clock~~ → 850,000 samples/frame | **CLOSED 2026-08-23** for DSPs; the RATE is still 0.33× and is now measured rather than assumed | — |
| ~~`zhao_surface_stamp`~~ | ~~28~~ → **0** | ~~1 texel/clock~~ → 20,000 texels/frame | **CLOSED 2026-08-23** | see below |
| `zhao_terrain_project` | 33 | 1 vertex/clock | **yes, and consumed** | **none** — 6,144 clocks/patch already gives ~270 patches against a 256-patch budget |

So three of the four are open on the same argument that carried here, and
`zhao_terrain_project` is the one where the rate is genuinely spent — sequencing
it would need the contract re-argued first, not a restructuring.

**`zhao_surface_stamp` CLOSED, 2026-08-23** (RUN-20260823-1415). The "slack:
partial" verdict above was **too cautious in one direction and wrong in the
other**, and both halves are worth correcting because the same reasoning is
about to be applied to `zhao_texture_tmu` and `zhao_terrain_normals`:

- **Too cautious.** The four per-texel products were judged "on the rate". They
  were not. The stated rate was a **placeholder**, and the demand derived from
  Sacrifice's SCAR system is 20,000 texels/frame — one texel per 83 clocks, not
  one per clock. The block was over-provisioned ~83x on that path.
- **Wrong.** The two radius squares were called "per-stamp constants and free".
  They were not free. They were written as combinational expressions off the
  command port, and Quartus gave them silicon like any other multiply. *Nothing
  in the RTL said they were rare*, so nothing in the fitter treated them as
  rare. **A rate that lives only in a comment is not a rate.**

Two of the six multiplies became first-order accumulators (exact mod 2⁴¹, no
domain argument owed); the four squares now share one sequential shift-add
squarer. **Measured, constrained: 28 → 0 DSP blocks, and Fmax 32.33 → 87.54 MHz**
— because the old block did not meet `gpu_clk` either, which the "rate met?
yes" column above could not see. Throughput 538,045 → 37,784 texels/frame,
still 1.89x the demand.

Applying only the measured result, the running total moves from **213 to 201**.
That is still 1.8x the device. The lever works; it has to be pulled about ten
more times.

### 2026-08-23, second pull: `zhao_terrain_lod` 28 -> 3, and the area fell again

The block the table above called the best target was sequenced next, and it beat
the pilot's ratio.

| | ALMs | DSPs | registers | fit s | commit |
| --- | ---: | ---: | ---: | ---: | --- |
| before | 2,086 | **28** | 1,257 | 434.3 | `47d607c` |
| after | 1,759 | **3** | 1,634 | 673.2 | `9f2928f` |
| delta | **-327 (-15.7%)** | **-25 (-89%)** | +377 | | |

Both sides measured on this machine at a clean worktree; the BEFORE was re-run
rather than quoted and reproduced the committed row to the digit. Device share
**25% -> 2.7%**.

**The area fell for the second time out of two.** That is now a measured pattern
rather than a single result, and the objection that sequencing trades area for
multipliers should not be raised again without evidence. The reason is the same
both times: the parallel form's cost was never mostly the multipliers, it was the
*wide datapaths standing beside them* — here six 66-bit squarers, two three-term
66-bit adder trees and **twelve** 49-bit comparators, collapsed to one
multiplier, one accumulator and **two** comparators.

Three things made 28 into 3 rather than into the contract's predicted 22:

1. `h` is the constant 256 in the strict ladder, so six of the twenty-four
   ladder multiplies were **shifts** — the lever the contract had already named,
   worth 6.
2. `s0`/`r0` and `s1`/`r1` share their left-hand sides exactly, so twelve of the
   twenty-four were **duplicates** — only six distinct products exist.
3. `|c - e|` fits in 32 UNSIGNED bits (both operands are s32, so the difference
   spans `[-(2^32-1), 2^32-1]`) and `d^2 = |d|^2`, so the ONE shared multiplier
   is 32x32 unsigned — **narrower** than the signed 33x33 the squares needed.

Cost: 34 clocks per descriptor became 48, so a patch is ~784 clocks rather than
~560 and the margin over the ledger's rate falls from ~11x to ~8x. Verification
did not weaken: directed 211 -> 219 checks (a real hole found by the block's new
mutation sweep and closed), random and nightly lanes identical to the digit,
LOD->TESS composition still crack-free, `ctest -L fast` 262/262.

**Running total: 201 -> 176.** Still 1.6x the device, and the two blocks pulled
so far were worth 37 DSPs between them.

---

## 2026-08-23 — the Field IR engine had NEVER been synthesised, and it is 79 DSPs

Half the design has never met Quartus. Counted:

| | |
| --- | ---: |
| RTL modules on disk | 91 |
| appearing in the block-fit report | 45 |
| **never fitted at all** | **46** |

Fifteen of those forty-six are `fpga/rtl/field/` — **the entire Field IR
engine**, which this project has repeatedly described as complete. It is
complete in *simulation*: every op has a differential and a mutation score, and
`FIELD.SEQ.CORE` is `RTL_VERIFIED` on a formal proof. None of it had ever been
through synthesis.

So it was fitted, as one unit — `zhao_field_seq` with its fourteen dependencies,
which is how the engine is actually used:

| | measured |
| --- | ---: |
| ALMs | **10,623** |
| **DSPs** | **79** |
| registers | 4,510 |
| M10K | 0 |
| `rtlCleanAtHead` | true |

**Seventy-nine DSPs is 71% of the device's 112, for one subsystem.** It is
larger than `terrain_project` (33) and `surface_stamp` (28) put together.

### What that does to the running totals

| | measured | device |
| --- | ---: | ---: |
| DSPs | **280** | 112 |
| ALMs | 47,003 | 41,910 |

**2.5x over on DSPs**, up from 1.9x an hour ago, and that is still only 39 of 91
modules. Treat the ALM row with more caution than the DSP row: per-block fits
give every block its own I/O and no sharing, and ALMs inflate badly under that
(the composed shell is 7,442 ALMs for blocks whose per-block sum is far higher).
DSP inference is much less sensitive to that, because it follows the arithmetic
rather than the boundary.

### But it is the same pattern, at the largest scale in the design

`zhao_field_seq` instantiates **every op unit in parallel** — ALU, reciprocal,
sine, isqrt, length, normalise, curve, noise, ring, rotation — each with its own
multipliers. And the sequencer **executes one instruction at a time**, six clocks
per instruction.

That is exactly the "parallel multipliers where the RATE does not require them"
shape the `zhao_geom_lod` pilot just took from 18 DSPs to 6 — except here there
are ten units idling instead of five products. A machine that retires one
instruction every six clocks does not need ten multiplier sets live at once.

So the worst measurement in the design is also the largest single opportunity in
it. **This should be sequenced before any of the 28-DSP blocks**, because it is
worth more than all of them combined.

### The honest caveat

Nobody has established that the engine's ten units *can* share without breaking
the six-clock instruction cadence or the formal anti-hang bound
(`tests/formal/field_seq_bound.sby`, proven at depth 140 under a shrunk
instruction count). That proof would have to be re-established against a shared
datapath. It is a bigger change than the LOD pilot, and it is the first
sequencing target where the answer is genuinely unknown rather than merely
unmeasured.

---

## 2026-08-23 — `zhao_geom_skin` is 72 DSPs, and the running total is 327

Second never-fitted block measured, chosen because it applies a bone matrix per
vertex and was therefore the likeliest remaining consumer:

| | measured |
| --- | ---: |
| ALMs | 1,801 |
| **DSPs** | **72** |
| registers | 145 |
| `rtlCleanAtHead` | true |

**64% of the device for one block**, second only to the Field IR engine's 79.
Running measured total: **327 DSPs against 112** — very nearly three times the
device, and still only 41 of 91 modules measured.

### But this one may be RATE-JUSTIFIED, unlike the others

The rule the `geom_lod` pilot produced is that the question is not whether the
products are independent but **whether the block's rate consumes the**
**parallelism**. Every target so far failed that test obviously:
`zhao_geom_lod` runs once per instance per frame; `zhao_terrain_lod` makes one
decision per patch per frame; the Field IR sequencer retires one instruction
every six clocks.

**Skinning is different.** It transforms a vertex by a 3x4 matrix, and vertices
are the highest-rate object in the geometry pipeline — thousands per frame per
creature. A block that genuinely needs one vertex per clock has a real claim on
its multipliers, and sequencing it would cost throughput the design actually
spends. That is the same verdict `zhao_terrain_project` got.

**So this is NOT queued for sequencing.** It is recorded as measured, with the
rate question open, because answering it needs a vertex-rate budget that nobody
has stated — how many skinned vertices per frame the machine must sustain. That
is an owner-shaped number, not something to infer from the RTL.

### Where the total actually stands

| block | DSPs | sequencing verdict |
| --- | ---: | --- |
| `zhao_field_seq` (whole Field IR engine) | 79 | **biggest opportunity** — ten units, one instruction per six clocks |
| `zhao_geom_skin` | 72 | rate may justify it — needs a vertex budget |
| `zhao_terrain_project` | 33 | do NOT — its rate is already spent |
| `zhao_terrain_lod` | 28 | in progress |
| `zhao_surface_stamp` | 28 | partial |
| `zhao_texture_tmu` | 28 | strong candidate |
| `zhao_geom_lod` | 6 | **done: was 18** |

Two of the three largest consumers may be legitimate. That materially changes
the shape of the problem: it is not simply "sequence everything", and the
remaining slack may not be enough on its own.
