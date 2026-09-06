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

### D1. The 100 MHz timing surgery — **CLOSED 2026-09-04**
`reports/composed/wumen-5d5b1b16-20260904T061710Z/RESULT.md`.

    gpu_clk            100.00 MHz (target met)
    worst setup        +0.057 ns
    failing endpoints  0
    hold               +0.245 ns, 0 failing
    timingPassed       TRUE

Eleven rounds from 53.48 MHz, **+87%**, the entire original violation closed,
**+844 ALMs and not one extra DSP.**

**The closing change was one line's worth of indexing** in
`zhao_raster_tilestore.sv` — the present bit read by PORT address rather than by
BANK address, cutting a path that was **unreachable in the design's own
semantics**. It closed all twelve remaining violations, not just its own: the
other three structures (`mem_guard` x6, `binner` x4, `cmd_dma` x1) went away on
placement once the last structural offender did. Round 11 concluded the opposite
— *"closing the last 0.198 ns needs all four touched"* — so **a flat tail cuts
both ways**, and the costed binner rework was never needed.

**THE MARGIN IS 0.057 ns, 0.57% of the period.** A pass, not comfort, on a
provisional device with virtual I/O. And it is still the shell **without** the
geometry front end (D22) — wiring nineteen blocks into this will move it.

**What it unblocks:** D1's own rule was *"D1 still comes first, because adding
nineteen blocks to a shell that is 14 MHz short would make attribution
impossible."* That constraint is gone, and both of D22's blockers were cleared
the same morning. **D22 is now the active P0.**

### D1 (superseded status, kept for the record)  ·  `reports/MHZArchitected`
**Measured, and this line was three days stale:** `gpu_clk` is **85.62 MHz**
against 100 (`reports/composed/renderer-b3bd69b-20260901T090000Z/RESULT.md`,
round 9). It opened at 53.48 MHz with TNS −6,566 ns.

Owner's diagnosis: first-composition timing debt, not existential. Five textbook
18 ns structures named. Execution order is **his**, and it is deliberate —
measure, then cheap broad fixes, then the deep ones, **fitting each step rather
than batching**:

1. ~~registered fit top; export the worst 100 setup paths~~ — **done**
2. ~~Early-Z full detection + Edgewalk registered steps / balanced popcount~~ — **done (r4, r8, r9)**
3. ~~streamed Edgewalk row + cross pipelines~~ — **WRITTEN, NOT MEASURED**
   (`7f95e592`, ROW-B/ROW-C split + explicit balanced popcount)
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

### MEASURED 2026-09-04, TWICE: **98.06 MHz, 12 failing endpoints**

`reports/composed/wumen-3546bfa2-20260904T041156Z/RESULT.md`. The Early-Z skid
was restored and **it paid**: 97.28 → 98.06 MHz, 18 → 12 failing endpoints, and
**Early-Z appears in none of the violated paths**.

| | r9 | r10 | **r11** |
|---|---|---|---|
| `gpu_clk` | 85.62 | 97.28 | **98.06 MHz** |
| worst setup | −1.679 | −0.280 | **−0.198 ns** |
| failing endpoints | 430 | 18 | **12** |

The same skid COST 2 MHz at round 2 and was removed at round 4 on a correct
measurement. Nothing regressed to bring it back — everything else got faster, so
a chain that was slack at 79 MHz became the longest at 97. It costs the same
+270 ALMs; what changed is that it now buys something.

**The offender list is new and nothing predicted it:** `zhao_mem_guard` 6,
`zhao_geom_binner` 4, `zhao_raster_tilestore` 1, `zhao_cmd_dma` 1. Neither of
the leaders is on `MHZArchitected`'s list. That is the fourth time this effort
the report has named a structure the note did not. **Read the paths.**

Worst path is now `tile_pipe | rs_state.RS_CLEAR` → `tilestore | res_pres_q`.

### The previous round: **97.28 MHz, 18 failing endpoints**

`reports/composed/wumen-18054414-20260904T020546Z/RESULT.md`.

| | r0 | r9 | **now** |
|---|---|---|---|
| **`gpu_clk`** | 53.48 | 85.62 | **97.28 MHz** |
| worst setup | −8.697 | −1.679 | **−0.280 ns** |
| failing endpoints | — | 430 | **18** |
| ALMs | 12,569 | 12,707 | 13,031 |
| DSPs | 16 | 16 | **16** |

**+82% over the effort, 97% of the original violation closed, +462 ALMs, zero
extra DSPs across ten rounds.** Hold passes with 0.244 ns of margin.

**The fit was the whole remaining action and it cost nothing but running it.**
The four EDGEWALK commits had been written for three days and never measured;
this docket was describing them as pending.

**The last 0.28 ns is one seam, not a sweep:**

    from  zhao_raster_fragment | s3_addr_r[3]~DUPLICATE
    to    zhao_raster_earlyz   | acc_mask_r[115]        −0.280 ns

17 of the 18 violations end in `zhao_raster_earlyz` and the eighteenth is in
`zhao_cmd_dma`.

**It is a READY path, not a data path** — traced through the RTL, not inferred
from the endpoints:

    fragment.s3_addr_r -> hazard comparator -> fragment.frag_ready_o
      -> earlyz.cand_ready_i -> out_free -> frag_acc -> hiz_qualify
      -> acc_mask_r write enable

`s3_addr_r` reaches Early-Z by travelling **backwards through the ready
signals**. Pipelining the address would do nothing, because the address is not
what arrives late.

So the fix is a skid on `earlyz.cand_ready_i`, or the hazard comparator taken
off `fragment.frag_ready_o`. **Fit it alone:** round 2 added an Early-Z
ready-path skid and `gpu_clk` fell 62.89 → 60.92, kept anyway as prepaid work.
This class has cost 2 MHz before it paid.

**Still not the finished console's number:** this is the shell without the
geometry front end (D22), on a provisional device, with virtual I/O.

### The history: the plan was RTL-complete and the fit was owed

**Corrected again 2026-09-04, one layer deeper than the refresh above.** The
section below was written from `b3bd69b` (2026-09-01 17:33) and named step 3 as
the last one outstanding. Step 3 landed **the next day**, in `7f95e592`, along
with three more EDGEWALK commits:

    7f95e592  ROW-B/ROW-C split + explicit balanced popcount   (step 3)
    05cf5e8d  the area's sign bit no longer picks the multiplier's operands
    8918a8f2  the tile-start pixel centre gets registers
    15828e71  the twelve w-operands are job-invariant, so they get registers

`zhao_raster_edgewalk.sv` now says in its own comments that *"nothing
downstream of a fill test is in the same clock as the fill test"*.

**`renderer-b3bd69b-20260901T090000Z` is still the newest composed fit in the
tree, and more than twelve commits have touched `fpga/rtl/raster` and
`fpga/rtl/geometry` since it ran** — the four above, the coverage-cursor
registration (`97d3b637`), and 2026-09-04's `RASTER.TILESTORE` port move.

**So the next D1 action is not a surgery. It is a composed fit.** 85.62 MHz is
the last measured number and it predates every change listed here. Nobody knows
what the console runs at right now, and the docket has been describing written
work as pending for three days because nothing re-measured.

### The path b3bd69b named, which step 3 was written to answer

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

## D22 — **THE GEOMETRY FRONT END IS NOT WIRED INTO THE CONSOLE**

**Found 2026-09-04 while tracing why `GEOM.DEPTHQUANT` has no consumer.** The
answer is not that one block was forgotten. It is that **nineteen of the twenty
geometry blocks are not in the shell at all.**

`zhao_shell_top.sv` instantiates exactly one of them:

    WIRED:      zhao_geom_bin_pipe

    NOT WIRED:  arena  assemble  attrsetup  binner  clip  cull  depthquant
                lod  mat3x4_mul  parambuf  pose_cache  pose_decode  project
                quat2mat  setup  skin  vdecode  wcache  vertex_arena

`zhao_geom_project` appears in only two files in the whole tree:
`zhao_geom_cull.sv` (a mention) and `zhao_prod_top.sv` — and the production top
is a **resource** top where no block is connected to any other. The shell's
`tri_ax_i` comes from `render_ax_i`, a shell **input**.

**So the console today renders from screen-space triangles handed to it from
outside.** command → (external triangles) → binner → tile pipeline → raster →
framebuffer → video. Everything from a vertex to a triangle is a set of verified
blocks sitting beside the machine rather than in it.

### Why this matters more than any single block

* It is why `GEOM.ASSEMBLE` (audit R1) and `GEOM.DEPTHQUANT` (audit R6) have no
  consumers. They are not two loose ends — they are two of nineteen, and
  wiring either one alone would connect it to nothing.
* **D1's 85.62 MHz is a measurement of the back end only.** Every composed fit
  so far has fitted a shell with no geometry front end in it. The number is
  honest for what it measured and it is not the console's number.
* The audit's finding that "`tri_ax_i` is driven only from a harness" was
  recorded per-block. Stated once, at the top level, it is a different and
  larger fact.

### What this entry does NOT claim

It does not say the blocks are wrong — most are UNIT_VERIFIED against oracles.
It does not say the composition is hard; the seams are contracted. And it does
not set a priority: **D1 still comes first**, because adding nineteen blocks to
a shell that is 14 MHz short of target would make attribution impossible, and
D3's fit-top split exists precisely so that composition can be measured in
controlled steps rather than as one before-after mystery.

**It is here so that "finish the console" has a name for the largest thing still
missing**, instead of that fact being rediscovered one unwired block at a time.

### The first block is BUILT, and the next blocker is a PHASE, not a bug

**2026-09-04.** `zhao_geom_meshfetch.sv` exists — oracle
(`zref::MeshFetch`), 19 + 5 oracle checks, 7 RTL differential checks. Position 1
of the order below is no longer empty.

**It cannot read yet, and the reason is specified rather than broken.**
`zhao_mem_guard.sv` admits SCANOUT reads inside the framebuffer slots, two
writers inside the granted slot, and `default: pass_ok = 1'b0`. Every region it
knows is a FRAMEBUFFER region, and a meshlet descriptor is in asset memory.
`spec/memory_rules.md` §5 says that is deliberate:

> *Phase 2 allocates exactly [the two FB regions] ... Later phases extend the
> map (texture/terrain/particle pools per the charter allocator); Phase 2 ships
> ONLY the two FB regions — everything else is a violation by construction.*

**So the next item on the console's critical path is the Phase-3 region map.**
It is a charter-allocator decision carrying a formal proof
(`tests/formal/mem_guard_no_escape.sby`), which is why no RTL written this
session touched it. Until it exists, every geometry descriptor read is denied
by design — and `zhao_geom_meshfetch.sv`'s `guard_denied_o` is the counter that
says whether that is still true.

### BOTH BLOCKERS ARE CLEARED (2026-09-04) — what is left is wiring

The two entries below named the front end's blockers precisely, and both are
now closed. **They were the hard part; what remains is glue.**

**1. The Phase-3 region map — DONE.** `spec/memory_rules.md` §5f:

    0x06A0_0000 .. 0x07FF_FFFF   GEOM.ASSET_POOL   22 MiB   ENGINE1   READ-ONLY

Bank 3, beside `GEOM.PARAMBUF`, and that placement is the W2.7 lesson applied
rather than re-learned: banks come from address bits `[26:25]`, sharing one cost
~82 of 192 Duo lines to row thrash, and mesh assets are the same `ENGINE1`
render-geometry traffic as PARAMBUF — same domain, same phase, one local
arbiter — so they serialise against each other instead of against scanout.

The no-escape proof **widened with the region rather than around it**: `a1_map`
now reads `slot0 || slot1 || asset` instead of exempting ENGINE1 from a two-slot
assertion, which is the shape that keeps a proof green by shrinking its scope.
`a1_asset_ro` and the `c_forward_asset` non-vacuity cover were added; bmc and
cover pass, and deleting `!req.write` fails two assertions at step 4.

**2. The asset fetcher — DONE, `UNIT_VERIFIED`.**
`fpga/rtl/geometry/zhao_geom_assetfetch.sv`, oracle `zref::assetfetch::plan`,
48 differential checks plus 25 oracle-edge checks.

It **buffers rather than caches**, and the reason is the consumer rather than
speed: `zhao_geom_assemble.sv`'s index port is `ix_req_o` / `ix_valid_i` with
**no ready** — it cannot say "wait" — and that block's own comment says it
deliberately does not buffer. A cache behind a port that cannot stall is not an
optimisation, it is a protocol violation waiting for a miss.

It is **single buffered on purpose**. The contract describes double buffering;
that is an optimisation whose value is unmeasured, and `prefetch_stall_cycles`
is the counter that will decide it. Building it first and measuring afterwards
is how a wrong number becomes an unadjustable wrong number.

**One ruling came out of building it.** `vertex_offset % 32 == 0` and
`index_offset % 8 == 0` are now refusals — which is `GEOM.VDECODE`'s own
contract sentence ("32 bytes per vertex, **naturally aligned**") enforced rather
than hoped for. Unaligned, a record spans five 64-bit words and needs a
320-to-256 funnel shifter **per vertex**; aligned, it is four consecutive words
and the shifter does not exist. The asset builder pays nothing for it.

**So D22's remaining work is what this entry originally said it was not:
wiring.** `zhao_shell_top.sv` still instantiates one of the twenty geometry
blocks, and D1 still comes first — adding the front end to a shell that is
0.198 ns short would make attribution impossible, which is exactly what D3's
fit-top split exists to avoid.

### The blocker is ONE asset-memory path, not eighteen wiring jobs

**Checked 2026-09-04 by reading every port in `fpga/rtl/geometry/`.** D22 reads
as eighteen separate pieces of work. It is not.

**`zhao_geom_meshfetch.sv` is the only `zhao_guard_req_t` client in the whole
subsystem.** Every other block that consumes asset bytes takes them through a
generic caller-fed port:

| block | how it gets its bytes |
|---|---|
| `GEOM.MESHFETCH` | `guard_req_o` / `beat_data_i` — **a real memory client** |
| `GEOM.VDECODE` | `v_bytes_i[255:0]` — 32 bytes, caller-fed |
| `GEOM.ASSEMBLE` | `ix_req_o` → `ix_valid_i/ix_a_i/…` — caller-answered |

So composing the front end needs **one asset fetcher serving three consumers**
— descriptors, the `u8` index stream, and vertex records — and that fetcher
needs a region `MEM.GUARD` does not yet have. Everything downstream of it is
ready-made ports waiting to be joined.

**That is why the honest next item is the Phase-3 region map and not "wire
eighteen blocks".** The wiring is glue over one memory path; the memory path is
a charter-allocator decision carrying a formal proof. Getting this the wrong way
round would mean eighteen people's worth of integration work queued behind a
question nobody had asked yet.

> **SUPERSEDED 2026-09-05 — THE REGION MAP WAS RULED THE NEXT DAY.**
> `spec/memory_rules.md` §5f allocates `GEOM.ASSET_POOL` at
> `0x06A0_0000..0x07FF_FFFF`, 22 MiB, `ENGINE1`, **read-only**, in bank 3
> alongside `GEOM.PARAMBUF` — with `formal_mem_guard.sv` widened *with* the
> region (`a1_map` reads `slot0 || slot1 || asset`) rather than around it, plus
> `a1_asset_ro` and a non-vacuity cover.
>
> **So D22's blocker is cleared and the paragraph above is stale.** It is
> corrected rather than deleted because it nearly cost a second session: read on
> 2026-09-05 while the composed-island fit ran, it reads as an open P0 and sent
> me looking for a region map that already existed. This is the docket's own
> version of *"never compare a current file to an old measurement"* — an entry
> that was true when written and describes a world that moved.
>
> The remaining D22 work is now genuinely the wiring, in
> `tools/design/compose_order.py`'s order.

### The order is already decided, and it is the ledger's own

`tools/design/compose_order.py` (committed) topologically sorts the geometry
blocks over the `upstream`/`downstream` edges each row already declares. It does
not invent an order; it reads back the one the contracts agreed to:

| | block | maturity |
|---|---|---|
| 1 | **`GEOM.MESHFETCH`** | **SPECIFIED — no RTL exists** |
| 2 | `GEOM.ASSEMBLE` | UNIT_VERIFIED |
| 3 | `GEOM.POSE` | REFERENCE_COMPLETE |
| 4 | `GEOM.VDECODE` | SPECIFIED (RTL exists) |
| 5 | `GEOM.SKIN` | UNIT_VERIFIED |
| 6 | `GEOM.LIGHT` | SPECIFIED |
| 7 | `GEOM.LOOM` | SPECIFIED |
| 8 | `GEOM.WARP` | SPECIFIED |
| 9 | `GEOM.WCACHE` | UNIT_VERIFIED |
| 10 | `GEOM.PROJECT` | UNIT_VERIFIED |
| 11 | `GEOM.CLIP` | UNIT_VERIFIED |
| 12 | `GEOM.DEPTHQUANT` | UNIT_VERIFIED |
| 13 | `GEOM.SETUP` | UNIT_VERIFIED |

### The probe now finds this class mechanically

`compose_order.py` reported *"every declared seam agrees"* for an edge that was
wrong, because **symmetry is not agreement**: both rows named each other, and no
signal travels across. It now also checks that every edge is justified by a
shared signal name, and that every declared input has a declared producer.

Run on geometry it finds **14 inputs with no upstream producer**. Some are
legitimately external (`camera_pair` from the command stream, `dispatch` from
the caller) — the check is a warning, not a verdict, because the vocabularies
are prose and two rows may name one thing differently. What it buys is that such
a pair gets looked at once instead of being derived into an order and believed.

**A second real edge error, found immediately.** `GEOM.PARAMBUF` takes
`triangle_descriptors` and nothing in the ledger emits it — while
`GEOM.ASSEMBLE` emits `triangle_descriptor` and its contract says that output is
*"Exactly `GEOM.PARAMBUF`'s 16-byte layout, so nothing is invented here"*.
So `ASSEMBLE -> PARAMBUF` is a real edge that is missing, and the declared
`ASSEMBLE -> CLIP` is suspect: ASSEMBLE emits triangle descriptors, CLIP takes
`view_vertices`.

**The deliberate review, for geometry, done.** All fourteen orphans triaged
against the contracts and RTL rather than left as a list:

| input | verdict |
|---|---|
| `ASSEMBLE.meshlet_descriptor` | **vocabulary** — MESHFETCH emits `meshlet_stream` |
| `ASSEMBLE.index_stream` | **external** — asset memory, caller-answered `ix` port |
| `DEPTHQUANT.projected_w` | **vocabulary** — PROJECT emits `view_vertices`, and `out_w_o` was added to it on 2026-09-04 |
| `DEPTHQUANT.depth_profile` | **external** — the two-bit profile in SetView |
| `LIGHT.world_normal` | **real gap** — `SKIN.NORM` is a prerequisite that does not exist |
| `LIGHT.environment` | **external** — rig state |
| `LOOM.transform_graph` | **external** |
| `MESHFETCH.dispatch` | **external** — the caller's instance walk |
| `PARAMBUF.triangle_descriptors` | **REAL MISSING EDGE** — ASSEMBLE emits it |
| `PARAMBUF.tile_references` | **vocabulary** — BINNER emits `tile_lists` |
| `POSE.pose_requests` | **vocabulary** — MESHFETCH's instance walk, per POSE's own contract |
| `POSE.clip_pages` | **external** — asset memory |
| `PROJECT.camera_pair` | **external** — SetView |
| `SKIN.bone_palette` | **vocabulary** — POSE emits `bone_matrices` |

**Six of fourteen are one thing named twice.** That is the finding worth more
than any individual edge: the ledger's signal vocabulary is not normalised, so
the graph cannot be checked mechanically without a human deciding each time
whether `bone_matrices` and `bone_palette` are the same wire. They are.

So the choice is explicit and belongs to the owner: **normalise the vocabulary
and the graph becomes machine-checkable**, or leave it prose and accept that the
probe's edge checks stay advisory forever. What must not happen is the third
thing, which is what happened tonight — a graph nobody can check being derived
into an order and believed.

### The whole ledger, measured

`compose_order.py --all` over all 105 blocks:

    asymmetric seams              0
    edges carrying no named signal   81
    inputs with no declared producer  102

**The graph is symmetric everywhere and semantically unchecked everywhere.**
Nobody has ever mistyped an edge; nobody has ever verified one carries anything.

**`tools/design/propose_signal_vocab.py`** (committed) scores each unproduced
input against what its declared upstreams actually emit. **17 name pairs
explain 28 of the 102**, and one pair explains twelve:

| producer emits | consumer takes | rows fixed |
|---|---|---|
| `engine_dispatch` | `dispatch` | **12** |
| `bone_matrices` | `bone_palette` | 1 |
| `tile_lists` | `tile_references` | 1 |
| `aux_request` | `aux_requests` | 1 |
| `meshlet_stream` | `meshlet_descriptor` | 1 |
| …12 more | | |

**It is a shortlist, not an oracle, and it is wrong in places** — it offers
`meshlet_stream == index_stream` (no: the index stream is asset memory) and
`tile_write == tile_read` (no: opposite directions). That is the intended
failure mode. A tool that decided this would be deciding architecture from
string similarity; a tool that shortlists it turns a 102-line problem into
seventeen judgements, and `aux_request`/`aux_requests` is a judgement that
takes one second.

**Renaming `engine_dispatch` to `dispatch`, or the reverse, would fix 12% of
the orphans in one edit.** That single decision is the cheapest available
improvement to whether this graph can ever be machine-checked.

**Not bulk-edited.** Two wrong edges found in one evening from one subsystem is
a reason to review the whole graph deliberately, not to patch the two that
happen to have been noticed.

**THE ORDER ABOVE IS WRONG AT POSITION 2, and the ledger is where it is wrong.**
Corrected 2026-09-04, hours after publishing it — recorded rather than quietly
edited, because I derived it mechanically and presented it as authoritative.

`GEOM.ASSEMBLE` declares `upstream: [GEOM.MESHFETCH]` and nothing else, which is
what puts it at position 2. But its own RTL header says what it actually
consumes:

> *"`vertex_offset` is PER VIEW, so the same local index resolves to a different
> **projected vertex** in view 0 and view 1."*

and its port is `m_vertex_offset_i [VIDW-1:0]` — **16 bits, a per-view
projected-vertex id base**. `GEOM.MESHFETCH` emits `vertex_offset [31:0]`, a
**byte offset into the mesh's vertex stream**. Different quantity, different
width, different address space.

**A projected-vertex id does not exist until after `GEOM.PROJECT`.** So
`ASSEMBLE` cannot run at position 2, before `VDECODE`, `SKIN` and `PROJECT` —
its input would not have been created yet. What it takes from MESHFETCH is
`index_offset`; the vertex base comes from whatever allocates arena slots after
projection.

**Which block assigns the id — answered by reading, not guessed.** Neither
does. `zhao_geom_wcache`'s fill port takes `fill_index_i [INDEX_W-1:0]` from its
**caller**; the arena stores a vertex where it is told to, it does not allocate.
(`zhao_vertex_arena` has the same port shape and is the superseded twin — the
ledger already rules "zhao_geom_wcache.sv over zhao_vertex_arena.sv", and the
manifest counts only the former.)

**So `m_vertex_offset_i` is assigned by the COMPOSITION, not by any leaf
block** — whatever decides where a meshlet's projected vertices land in each
view's arena. That is why no upstream edge was added: the producer is the
pipeline that does not exist yet, and writing `upstream: [GEOM.PROJECT]` would
have named a block that does not in fact hand out the number.

The constraint that IS certain: **ASSEMBLE runs after projection, not before
decode**, and the pipeline owes it a per-view base.

This is the whole value of deriving the order mechanically and then checking it
against the RTL: the probe faithfully reported what the ledger said, and the
ledger was wrong. A composition built to that order would have wired a byte
offset into a vertex-id port — same signal name, same-looking wire, silently
wrong geometry.

**Every declared seam agrees.** The probe also checks for edges where A names B
downstream while B does not name A upstream, and there are **none** across all
fifteen geometry rows. The contracts are consistent with each other; what is
missing is wire, not agreement.

**The blocker is position 1.** `GEOM.MESHFETCH` has **no RTL** —
`fpga/rtl/geometry/` has twenty files and `zhao_geom_meshfetch.sv` is not one of
them. It is the entry point: nothing downstream of it can be fed by anything but
a harness until it exists. That is the single concrete thing standing between
the console and a composed front end, and it was previously visible only as
"MESHFETCH is SPECIFIED" in a row nobody reads next to the shell.

**`GEOM.BINNER` and `GEOM.PARAMBUF` declare a mutual edge** and are therefore
not orderable. That is not an error — the binner writes triangle records into
the parameter buffer and reads them back per tile, so the cycle is the real
dataflow. It is recorded because the composition order cannot be derived
mechanically across that pair and someone will otherwise try.

**Two maturities may understate what is built**, and the evidence is a test run
away: `GEOM.VDECODE` (SPECIFIED) and `GEOM.PARAMBUF` (REFERENCE_COMPLETE) both
have RTL *and* directed tests that are registered in CMake. If those pass, the
rows are stale. Not advanced here — a maturity moves on evidence, and the test
binaries were not built at the time of writing.

---

## SWEEP 2026-09-04 — decision-bearing documents that were not indexed

A sweep of `reports/` against this file found **70 of 98 files unindexed**. Most
are creature-lane working notes and belong to that lane, not here. **Three carry
decisions**, and one of those is still open — which is the failure this index
exists to prevent, since an unindexed decision is an undelivered one.

### D19. `min_m10k: 8` on TEXTURE.CACHE — **CLOSED 2026-09-04: the gate was on the wrong block**
`reports/TEXTURE-ISLAND-STORAGE-GAP-20260904.md`.

This entry asked for a ruling on whether the tripwire was wrong or the cache was
half its intended size. **It was neither. The gate belongs to a different
block, and the "8" was never a bit budget.**

`reports/islandrearchitecture5.md` §10.1:

> **Replace, do not patch.** Create `zhao_texture_cache_v2`. Keep
> `zhao_texture_cache` and `zhao_texture_cache_pipe` as **behavioral oracles**.

And §3.3's budget row is named **"synchronous texture cache v2"**, under a
heading that says *"These are architecture budgets, not predicted fit results."*
So `zhao_texture_cache_pipe` is the ORACLE, and §10.11's gate — 900 ALM,
900 registers, 8-10 M10K, 125 MHz — is `zhao_texture_cache_v2`'s. That block
has not been written.

**And the count is of MEMORIES, not bits.** §C3: *"CACHE V2 STORAGE: four static
data banks + four static tag banks."* Four plus four is eight. An M10K holding
1,024 bits is still an M10K, so `min_memory_bits: 8192` — added to interrogate
this failure — was answering a question the brief never asked.

**Resolved without a ruling**, because the disagreement was between this
docket and the brief rather than between the brief and the RTL. All four
resource rules removed from `cache_pipe` in `design/fit_targets.yml`; they move
to `zhao_texture_cache_v2` when it is written. The fit stays — what the oracle
costs is worth knowing.

**A caveat recorded earlier the same day is REVERSED by this.** The run log said
*"fragrob's `min_m10k: 6` failure will NOT be an RTL defect"*, on the same
count-as-capacity reasoning. §6.13 hard-rejects *"any sample/context payload
array **in flops** above the explicit control bits"* — the brief REQUIRES those
arrays to be RAM and expects fifteen-odd small ones. So a low count from
`fragrob` is evidence of exactly the defect §6.13 names, with a known cause:
`desc_u_m[3][DEPTH]` is multidimensional and Quartus reports *"cannot regroup
multidimensional array"*. **`min_m10k: 6` stays, and a failure means reshape the
arrays — not lower the number.**

### D19b. **Nine FAULT OUTPUTS that no test ever names** — found 2026-09-04
`tools/design/check_port_coverage.py`, first run.

    zhao_raster_fbwrite      fatal_error_o
    zhao_surface_sheet       res_overflow_o
    zhao_raster_texjoin      o_uv_sat_o
    zhao_raster_texjoin_v2   o_uv_sat_o
    zhao_texture_fragrob     o_uv_sat_o
    zhao_field_exec_shared   exec_unsupported_o exec_sat_add_o
                             exec_sat_mul_o exec_sat_rescale_o

**Verified, not inferred**, for the headline at least: `fatal_error_o` is
asserted on two distinct fault paths in `zhao_raster_fbwrite.sv` and the string
`fatal_error` does not occur anywhere under `tests/`.

**Why this is its own docket entry rather than a lint note.** These are not
unchecked data outputs. `spec/counters.md`'s argument about counters applies
exactly: the whole purpose of the signal is to be non-zero when something has
gone wrong, so **a fault output nothing reads is a fault that has never once
been observed**. Three of them are saturation flags on the texture path, which
is the block family currently being fitted.

Twenty-two further outputs are unmentioned without being fault reporters —
including two from blocks built this session (`geom_depthquant d_behind_o`,
`geom_skin_norm sq_rready_o`), so the tool indicts its author first.

**The tool reports and does not gate**, deliberately: an absence is not always a
defect, and its second tier (317 ports named but not obviously compared) is a
weak heuristic summarised rather than listed. It exists because `out_w_o` went
into `GEOM.PROJECT` the same day with no differential and **no test could fail**
— the missing expectation and the missing check cancelled out.

**RESOLVED 2026-09-04, same day. Of the nine, THREE were real and six were
the tool's own blindness.** Each disposition was checked individually, because
the first attempt to explain the six named the wrong mechanism.

**Real, and fixed:**

* `zhao_raster_fbwrite.fatal_error_o` — closed by the W_VERD fix, which the
  shell suite now exercises (`fatal=0` is asserted on a run that writes 3,328
  pixels).
* `zhao_surface_sheet.res_overflow_o` — three checks added, in BOTH directions
  (pulses once on refusal, silent on allocate, silent on hit). Mutation-proven:
  forcing the assert low fails the first, forcing the default clear high fails
  all three.
* the three `o_uv_sat_o` ports — **and these were worse than filed.** Not merely
  unread: their cause `f_uv_sat_i` is set in five places across the whole test
  tree and *all five write 0*. The flag had never been high in any simulation.
  `raster_texjoin_v2_directed` case 3b now drives it, with an irregular pattern
  and backwards returns so a desynchronised flag is caught, plus a non-vacuity
  check that it was seen high at all. Mutation-proven with `sat_q[head_q ^ 1]`.

**Not real — covered by COMPOSITION:** the four `zhao_field_exec_shared`
reporters. No testbench instantiates that module. `zhao_field_seq.sv` consumes
them and accumulates across a whole program exactly as the reference's single
`SatLedger` does (`sat_add_o <= sat_add_o || exec_sat_add;`), and
`field_curve_svc_directed` compares the resulting `rsp_sat_add_o` per lane
against that reference. **The behaviour is checked while the name appears
nowhere.**

*(An earlier disposition said these were compared through a testbench wrapper
that renames ports, citing `zhao_field_alu_tb.sv`. That was wrong — those are
`zhao_field_alu`'s own ports, a different module's signals sharing a stem. The
verdict held; the reason did not, and the reason is what the next reader uses.)*

**And the tool that found them was itself wrong five ways**, all fixed and all
recorded in its docstring: it could not read width brackets, scoped types, final
ports without a comma, or unpacked arrays, and it never opened a `.sby` — so a
formally proven module counted as untested. Its numbers moved 29 → 107
unmentioned and 313 → 987 read-only. **Every one of those defects made the
answer look better than it was**, which is the property that keeps a broken
instrument in service.

**The one row still standing** is `zhao_raster_texjoin.uv_sat_fragments_o`,
invisible until the width-bracket fix — filed separately as D19k, because texjoin
v1 is instantiated nowhere and the production path has no equivalent counter.

### D19c. The ledger's `counters:` field is not checkable — **needs a convention**
Measured 2026-09-04 over the 62 blocks whose module name is derivable from their id.

`design/blocks.yml` declares a `counters:` list per block, `spec/counters.md`
governs their behaviour, and V12 checks each name is in `counter_catalog`.
**Nothing checks that the RTL implements one.** Measured:

    counters matching their port as <counter>_o :   0
    present in the module text some other way   :  54   (comments included)
    absent from the module entirely             :  54

**CORRECTED 2026-09-04, later the same day. That zero was a measurement
artefact, and the real figure is 23 of 108.** The port-scanning regex shared by
these tools had no provision for a WIDTH BRACKET, so `output var logic [31:0]
meshlets_fetched_o` was silently skipped while `output var logic
exec_sat_add_o` matched. Every counter port in the tree is width-bearing, which
is why the count came out at exactly zero rather than merely low -- **a zero
that precise should have been read as a broken measurement, not a finding.**

`tools/design/check_counters.py` now measures it properly:

    108 declared on blocks with a module
     40 resolve by the default <name>_o
     10 by an explicit counter_ports mapping
     58 UNRESOLVED
     30 further blocks declare counters but have no module file yet

**The number was corrected THREE times before it settled: 0 -> 23 -> 40.** Four
separate parser defects, each of which silently dropped ports and so made the
figure smaller and more alarming:

    width brackets      output var logic [31:0] x_o,
    scoped types        output zhao_pkg::zhao_counter_snap_t x_o,
    the final port      output logic x_o          <- no trailing comma
    unpacked arrays     output var logic [31:0] refused_o [7]

The tool now REPORTS how many `output` lines it failed to read, so a fifth form
shows up as a number instead of as a smaller answer. **That self-check was
itself broken on its first version** -- written with a `` that a shell heredoc
turned into a literal backspace (0x08), so it matched nothing and printed "no
silent drops" while 17 ports were being dropped. It is now asserted at import.

**CLOSED 2026-09-04. The residue is genuinely unimplemented, and that is now
measured rather than assumed.** `check_counters.py --suggest` lists, for each
unresolved counter, the `[31:0]` output ports its block actually has — the shape
a counter takes here. The result:

    54 of 54 remaining unresolved counters are on blocks with
    NO 32-bit output port at all.

So there is no candidate to map any of them to. The mapping convention has
extracted everything a naming difference can explain; what is left is **not a
vocabulary problem, it is missing work**. Final:

    40  resolve by the default <counter>_o
    13  by an explicit counter_ports mapping
     1  on a snap-channel block, wants a mapping
    54  no port, no snap channel, no candidate -- unimplemented
    30  further blocks declare counters but have no module file yet

One counter is deliberately left unmapped and is the useful example:
`VIDEO.SLOTMGR.slot_leases_granted`. The nearest port is `lease_grant_o`, whose
own comment calls it *"one pulse"* — an EVENT, not a count. Mapping it would
make the row read implemented while nothing accumulates, which is the original
complaint wearing the checker's approval. **Map a counter to a port that COUNTS;
a pulse needs an accumulator written first, and its absence is the finding.**

**FINAL TALLY, with the two piles separated.** The checker now asks whether an
unresolved counter's block has a snap channel at all, because the answer changes
what the row means:

    40  resolve by the default <counter>_o
    10  by an explicit counter_ports mapping
     1  unresolved on a block that HAS a snap channel -> wants a mapping
    57  unresolved with NO snap channel and no port -> no visible
        presentation path at all
    30  further blocks declare counters but have no module file yet

Only **four** blocks in the tree carry a `zhao_counter_snap_t` (AUDIO.FIFO,
CMD.DMA, CMD.SCHEDULER, DEBUG.COUNTERS), so the snap-channel explanation covers
almost nothing. **The real finding is the 57**: counters declared in the ledger
that the RTL presents by neither mechanism the spec defines. That is a much
smaller and much sharper claim than "the field is not checkable", and it is the
one worth acting on.

**And a second reading error, worth more than the count.** Many unresolved rows
are the D9 SNAP-CHANNEL form (`spec/counters.md` §3): the block owns the counter
locally and presents it as a `zhao_counter_snap_t`, so there is no `<counter>_o`
port and **there is not meant to be one**. CMD.SCHEDULER, CMD.DMA and AUDIO.FIFO
are all correctly implemented this way and were all reported as gaps. The
original framing -- "documentation that reads like a claim" -- was too harsh for
those rows; a mapping makes the snap form legible without pretending it is
missing.

**The ruling below is unchanged, and the corrected number strengthens it.**
Twenty-three blocks already follow `<counter>_o`, so that is the established
default rather than a new imposition -- and a block already conforming needs no
mapping entry at all, so adopting the rename later simply deletes entries rather
than rewriting them.

**Original (wrong) reading follows.** Zero. The ledger's counter names and the RTL's port names are two separate
vocabularies with no mechanical link, so `counters: [meshlets_fetched,
triangles_culled]` on `GEOM.MESHFETCH` — whose ports are
`meshlets_considered_o` and `descriptors_fetched_o` — is documentation that
reads like a claim.

**This is NOT a report of 54 missing counters**, and the distinction matters:
most are probably implemented under another name, and the "present some other
way" figure is soft because a match can be a comment. What is established is
that **the field cannot currently be verified**, which is the same class as the
signal-vocabulary problem `compose_order.py` surfaced for `upstream`/`downstream`
and which was worth fixing there.

**It wants a ruling, not a sweep.** Either the ledger names become the port
names (`<counter>_o`, mechanical but touching 60+ blocks), or each row carries
an explicit port mapping. Renaming ports across the tree during an active fit
campaign is the wrong move; choosing the convention is cheap and can be applied
as blocks are next touched.

**One of the 54 is this session's own**: `GEOM.ASSETFETCH` declares
`prefetch_stall_cycles` and its port is `prefetch_stall_o`. Left as-is
deliberately — fixing one block to a convention that does not exist yet would
make the tree less consistent, not more.

### D19k. The production texture path can FLAG a railed UV but cannot COUNT one
Found 2026-09-04, by the port scan once its width-bracket blindness was fixed —
`uv_sat_fragments_o` is `[31:0]` and had been invisible to every earlier run.

`zhao_raster_texjoin` (v1) carries **`uv_sat_fragments_o`**, a running count of
fragments on which PERSPUV railed. Three facts about it:

* **v1 is instantiated nowhere.** Not in `zhao_prod_top.sv`, not in any other
  RTL file. It is a leaf nothing builds on.
* **v2, the behavioural oracle, has the per-fragment flag `o_uv_sat_o` and no
  counter.**
* **`zhao_texture_fragrob`, the production block, has neither.** Its four
  counter ports are `fragments_o`, `samples_o`, `full_clocks_o`, `id_errors_o`.

So the shipped path can say *this fragment railed* on a one-cycle flag and
cannot answer *how many railed this frame*. UV saturation means a texture
coordinate left the representable range, which is a visible-artefact signal, and
the thing you want from it is a per-frame number you can watch move.

**Not filed as a bug in fragrob** — S6.1 built it beside v2 deliberately and its
counter set is its own. Filed because the capability existed in v1, is absent
from the production path, and nothing recorded the trade. Whether to restore it
is an owner call; the cost is one 32-bit accumulator on a flag fragrob already
has to compute.

**Also observed while checking**: fragrob's RTL has FOUR counter ports and its
ledger row declares THREE. `full_clocks_o` is implemented and undeclared, which
is the D19c vocabulary problem running in the opposite direction — the ledger
under-claiming rather than over-claiming.

### D19l. Eight fit rules have NEVER been evaluated — and three blocks breach one
Found 2026-09-04 while sweeping for D19m's defect.
`tools/quartus/check_rule_freshness.py`.

Three blocks in `reports/synthesis/zhao_block_fit.json` read `status: ok` while
measuring far past a ceiling their own `fit_targets.yml` entry declares:

    zhao_raster_texjoin_v2    registers 7151 > 2500
    zhao_raster_perspuv_svc   registers 3293 >  700 ; alms 2204 > 900
    zhao_raster_rcp24_svc     registers 1101 >  600 ; alms 1041 > 650 ; dsp 6 > 4

**The evaluator is not broken.** `zhao_texture_fragrob` carries the same
`max_registers: 2500` and failed it correctly at 2,631 today. The cause is
duller: **the rules were written AFTER those blocks were last fitted.**

    perspuv_svc   row's sources 2026-09-03 11:47   its rule 2026-09-04 07:36
    texjoin_v2    row's sources 2026-09-03 04:31   its rule 2026-09-03 17:29

All eight resource rules on the four blocks not fitted today postdate the
measurement that governs them.

**REFINED 2026-09-04, after `cache_pipe` showed what a stale row is worth.** Of
the three blocks reported as breaching, only ONE sits on a row that is current
with its own source:

    zhao_raster_perspuv_svc   row 2026-09-03, rtl changed LATER same day  -> STALE
    zhao_raster_texjoin_v2    row 2026-09-03, rtl changed LATER same day  -> STALE
    zhao_raster_rcp24_svc     row 2026-09-03, rtl last changed 2026-09-02 -> CURRENT

So **`zhao_raster_rcp24_svc` is the only confirmed breach**: 1,101 registers
against 600, 1,041 ALMs against 650, and **6 DSPs against 4** — measured on the
source that is still checked out. The DSP overrun matters beyond its own rule:
the census already totals 196 DSPs against the part's 112.

**And the one confirmed breach is a WRONG RULE, not a wrong design.**
`zhao_raster_rcp24_svc` contains exactly one multiplier:

    m1_p_q <= 64'(mul_a_c) * mul_b_c;      mul_a_c [31:0], mul_b_c [63:0]

A **32 x 64** multiply. On Cyclone V that is four-plus 18x18 partial products
whose weight falls below bit 64, which is where the measured **6 DSPs** comes
from; `max_dsp: 4` was an estimate of a narrower multiply than the block has.

**The width is not free to change.** The file's own header fixes it:
`w = (m * x) >> 24` with a 64-bit product, and `t = 2^31 - w` *"WRAPS at 64
bits, matching the reference's uint64"*. Narrowing the operand changes results
and breaks parity with `zref`, so the arithmetic is doing what the reference
requires and the rule is describing something else.

So the correct edit is **the rule to 6**, with the 32x64 shape named in the
comment beside it so the next reader does not re-estimate 4. Not made in this
pass: `design/fit_targets.yml` is polled by the running fit queue, and a
non-atomic rewrite of that file has already broken a campaign once.

**It still costs 6 DSPs against a part with 112 and a census already totalling
196**, so "the rule was wrong" removes a false alarm without removing the
pressure. That is the DSP budget's problem, not this rule's.

The other two are **questions, not findings.** `cache_pipe` read 11,328
registers on a stale row and measured 3,097 when re-fitted; a breach on a stale
row carries exactly that much weight. `texjoin_v2`'s figure is separately
documented in the ledger ("v2 is wrong about storage, 7,151 registers holding a
7,056-bit table"), so it is at least a deliberate known state rather than a
surprise — but the number itself is still older than the file. A rule declared after the last fit **has never
run**, and nothing in either file distinguishes it from one that was evaluated
and passed.

**CREDIT WHERE IT IS DUE, AND A CORRECTION TO THIS ENTRY'S CLAIM.**
`tools/quartus/check_fit_rules.ps1` already existed and already reports every
breach listed above — it reads the same two files, runs no Quartus, and exits 1
as a gate. **The breaches were not hidden; nobody had run the tool.** What
`check_rule_freshness.py` adds is only the TIME dimension: a rule newer than the
fit it governs, and a row older than its own RTL. I should have looked for an
existing tool before writing half of one.

**And running it found something worse than any breach.** With a fresh
`max_registers: 12000` added for `zhao_texture_tmu_pipe` (D19m), the gate
reported:

    PASS  zhao_texture_tmu_pipe

on the one block in the tree holding a 65,536-bit palette cache in flip-flops.
Its row exists but every resource field is null — `status:
failed:quartus_fit.exe` — and every rule in `Test-FitRules` is guarded by
`$null -ne $x`, so all of them passed vacuously. The summary said *"0
unmeasured"*, because its notion of unmeasured was *no row* rather than *a row
with no numbers*.

**A gate absent is not a gate passing; neither is a gate with no data.** Fixed:
a row carrying no resource numbers is now counted as unmeasured and printed with
its status. 4 pass / 5 FAIL / 0 unmeasured became 3 / 5 / 1.

**This is the third instance of one failure mode**, and the reason it is now a
tool rather than a third anecdote: the Stop hook that bypassed itself and fired
exactly once (`CLAUDE.md`), and a port-coverage self-check written with a ``
that a heredoc turned into a literal backspace, so it matched nothing and
printed *"no silent drops"* while 17 ports were dropped. **A check that has
never fired is worse than no check, because it reassures.**

Also reported: the `status` / `lastAttemptStatus` split. `status` holds the last
GOOD run, so a block whose rules now fail still reads `ok` — `cache_pipe` is
exactly that today.

*(A first draft of this entry named `cache_pipe` as the worst breach at 11,328
registers against `max_registers: 2000`. **Wrong.** Those gates were removed
earlier the same day and survive only as quoted text inside the comment
explaining the removal — a loose regex read the retraction as the rule. The
tool now matches whole lines only.)*

### D19m. tmu_pipe holds a 64 Kbit palette cache in flip-flops — 72,824 registers
Found 2026-09-04 by reading the LIVE fit's synthesis stage, which finished 71
minutes before the fitter did. Full write-up:
`reports/TMU-PIPE-PALETTE-IN-FLOPS-20260904.md`.

    Total registers : 72824      Total block memory bits : 256

**CORRECTED:** 72,824 registers need **18,206 ALMs at 4 per ALM, ~38,300 at the
~1.9/ALM this tree achieves — 43% to 91% of the device for one block**, while
using 256 bits of the 553 available M10Ks. (An earlier line said "87% of every
register", from a wrong 2-flops-per-ALM figure; Cyclone V ALMs carry four, and
the meaningful unit is ALMs rather than registers.) The cause is two declarations:

    logic [255:0] pal_val_r [PAL_SLOTS];        //  16 x 256      =  4,096 bits
    logic [15:0]  pal_dat_r [PAL_SLOTS][256];   //  16 x 256 x 16 = 65,536 bits

As memory that is **7 M10K out of 553**. `pal_dat_r` is multidimensional, the
shape Quartus 17.0.2 declines to infer.

**Nothing caught it because nothing was watching**: `tmu_pipe` has a `sources:`
list and no rules at all — see D19l. A gate absent is not a gate passing.

This also refines a prediction I got wrong this morning. I expected `fragrob` to
fail on this very mechanism; it did not (13 M10K inferred), and I recorded that
the blocker "is real for some shapes and was not fragrob's". **This is one of
the shapes.** Multidimensional alone is not sufficient — multidimensional AND
large AND indexed on both axes is what breaks inference.

**The fix needs more than flattening, and it should be built with the II = 2
work.** An M10K cannot be read asynchronously, and the palette read is
`always_comb` — so flattening alone leaves all 65,536 bits in flops. The read
must be REGISTERED, which adds a cycle to the CLUT path that the block's own
suite already measures at **0.65x of demand**. But `REMAINING_BLOCKERS.md`
records an II = 2 redesign for exactly that path — a 2-entry in-flight record,
an issue arbiter, in-order completion — and a pipeline with in-flight records is
what absorbs a registered read for free. **Doing D19m first and separately is
the one ordering that makes both problems worse.**

No fix in this pass: the file is inside the running fit's closure (live-tree
trap, `QUARTUS_GOTCHAS.md` §11).

### D19d. `PART.EXPAND -> GEOM.SETUP` narrows six SIGNED coordinates by a bit
`tools/design/check_seam_widths.py`, first run. **Verified at source:**

    zhao_part_expand.sv:118   output logic signed [21:0] t_ax_o
    zhao_geom_setup.sv:139    input  logic signed [20:0] tri_ax_i

and the same for `ay bx by cx cy` — **six coordinate lanes, 22 bits into 21.**

**A signed narrowing does not clip, it wraps.** A particle quad vertex beyond
`2^20` would arrive at `GEOM.SETUP` with the opposite sign — a triangle folded
across the origin rather than one pushed to the edge of the screen.

### CORRECTION — the producer is RIGHT, and had already argued the case

**The paragraphs below were written before reading `zhao_part_expand.sv`'s own
WIDTHS section, and they got the fix backwards.** That section says, in the
file, before any of this was noticed:

> *"A vertex is therefore at most 21 bits plus a twelve-bit offset: 22 bits
> signed covers it with room, and **the output is widened to 22 rather than
> silently wrapping a 21-bit port**."*
>
> *"**The expanded fan is NOT re-clamped to the guard band.** A large particle
> near the edge can put a vertex outside ±2048 px, and that is correct: the
> software does exactly the same and lets the rasteriser's scan box scissor it.
> Clamping here would deform the triangle instead of clipping it, which moves
> the particle rather than cropping it."*

So **22 bits is deliberate, reasoned and documented**, and narrowing the
expander is explicitly the wrong answer — it would move particles instead of
cropping them. The `±2048 guard` question I posed below is already answered: the
guard is `GEOM.PROJECT`'s, expanded particle vertices legitimately exceed it,
and that is the design.

**The seam is still a real problem, and now a sharper one.** It is not "a
producer with a stray guard bit"; it is **two blocks that disagree about the
coordinate range**, where the producer has written down why it needs the wider
one. Wiring `PART.EXPAND -> GEOM.SETUP` therefore needs a decision — widen
`GEOM.SETUP` and its downstream to 22, or scissor between them — and **must not
narrow the expander.**

**The lesson, and it is the session's recurring one:** the answer was in the
file, in a section headed WIDTHS, and the entry below was written from the port
declarations alone. *Read the block before diagnosing the block.*

### What the entry below got wrong (kept, because the reasoning is the lesson)

    zhao_part_expand.sv:87    // Screen coordinates are S 12.8 in 21 bits,
                              //   already inside the +/-2048 px guard
    zhao_part_expand.sv:106   input  logic signed [20:0] p_x_i,   // S 12.8
    zhao_part_expand.sv:118   output logic signed [21:0] t_ax_o

**It takes 21 bits in, states in its own comment that screen coordinates ARE 21
bits, and emits 22.** `GEOM.SETUP`'s side is not arbitrary either — its input
comment says *"S 12.8 screen subpixels"*, and S12.8 is exactly 1 + 12 + 8 = 21
bits. So the whole pipeline agrees on the format and this block's output ports
are the one place that does not.

`±2048 px` in S12.8 is `±524,288`, which needs 20 bits plus a sign — **21**. So
the 22nd bit looks like a computation guard on `centre ± half_side` that reached
the port instead of being dropped after the range was established.

**That makes the fix cheap and the risk real:** cheap because the values
provably fit 21 bits if the ±2048 guard holds; real because if the guard does
NOT hold, narrowing silently wraps and widening the rest of the pipeline is the
answer instead. **The guard must be checked before either.** What must not
happen is the seam being wired as-is, which truncates without anyone choosing
to.

**The seam is DECLARED but not yet wired** — neither block is in
`zhao_shell_top` — which is precisely when this is cheap. It wants one of: the
setup inputs widened to 22, the expander's outputs proven to fit 21, or an
explicit saturating narrow at the seam. **Not a truncation nobody chose.**

### The other eight were triaged, and **every one is noise**

The tool's first run said "nine width differences", which was a raw output and
not a finding list. Every one was read at source, and **exactly one is a
defect** — the other eight are stem collisions or harmless widenings:

| seam | verdict |
|---|---|
| `PART.EXPAND -> GEOM.SETUP` (x6 lanes) | **REAL** — same signal, 22 into 21, signed |
| `MEASURE.GOVERNOR min_hold_o [7:0] -> PART.LADDER p_hold_i [3:0]` | noise, **checked** — a THRESHOLD constant against a running COUNTER, and benign either way: `MIN_HOLD = 6`, `DEG_HOLD = 12`, both inside 4 bits |
| `CMD.SCHEDULER fence_status_o [7:0] -> SURFACE.STAMP pg_status_i [1:0]` | noise — a FRAME FENCE status against a PAGE status (`StOverflow`) |
| `CMD.SCHEDULER fetch_slot_o [1:0] -> MEM.GUARD blit_slot` | noise — a command-ring slot against an FB slot |
| `CMD.SCHEDULER fetch_slot_o [1:0] -> TWOD.PLANE d_slot_i` | noise — same collision |
| `GEOM.PROJECT out_w_o [30:0] -> GEOM.CLIP vp_w_i [11:0]` | noise — `vp_w_i` is a **VIEWPORT** width |
| `GEOM.PROJECT out_behind_o -> GEOM.CLIP tri_behind_i [2:0]` | noise — one per-vertex flag against three vertices' worth |
| `SURFACE.SHEET pg_strength_o [7:0] -> cmd_strength_i [15:0]` | widening, cannot lose data |
| `VIDEO.FRAMECTL swap_slot -> CMD.SCHEDULER dma_slot_i [1:0]` | widening, cannot lose data |

**Seven of the nine are STEM COLLISIONS** — `status`, `slot` and `w` are generic
enough that two unrelated signals share one. The tool now separates narrowing
from widening and prints the collision warning above its own list, because a
tool whose output must be triaged by hand and does not say so is a tool that
will be believed once and ignored after.

**82 seams fit exactly.** That is the number that matters for D22: the wiring is
mostly a matter of connecting ports that already agree.

### D19e. The shell's render path — **NOW SIMULATED, same day**
`tests/shell/shell_draw_directed.cpp`, 9 checks passing.

The render port, the seven job words and **ten render observables the wrapper
had been discarding with `()`** are now brought out of `tb_zhao_shell`, and a
triangle offered at the console's edge is driven through the shell for the first
time. What it measured:

    [shell_draw] pixels=0 bursts=0 issued=0 retired=0 drained=1 FATAL=1

`render_fatal_o` is `zhao_raster_fbwrite`'s `fatal_error_o`, and fbwrite.sv:301
says exactly when it latches — *"The guard REFUSED: the write is outside the
leased region and nothing was written."* `MEM.GUARD`'s `blit_ok` requires
`map_valid`, which only `CMD.SCHEDULER` grants at frame start, and this test
sends no command. **The refusal is correct.**

**And the refusal is the proof of reach.** A broken wiring produces no burst, no
guard decision and no latch. The triangle travelled the whole chain and was
turned away by the rule that exists to turn it away.

It also caught **`render_fatal_o` firing — one of the nine outputs D19b reports
that no test names.** The first test to watch one found it doing its job.

**CLOSED the same day.** The command path is driven alongside the render port,
a blit packet grants the lease, and the composed shell is shown to render:

| claim | evidence |
|---|---|
| the right **tiles** | 13, matching `zref::Binner` |
| to the right **addresses** | `0x0000..0x1FA0`, advancing by the row pitch |
| in the right **colours** | two clusters, each channel within one dither LSB |
| with the right **silhouette** | **1,586 covered pixels — `zref::EdgeWalk`, exact** |
| and the counters honest | oracle 3328 = counter 3328 = VRAM 3328 |

**20 checks.** Per-pixel colour is NOT re-checked here and that is not a gap:
`render_pipe_directed` already compares `fb_rgb565_o` against
`zref::TileResolve`'s `rgb565[]` and a per-tile CRC-32C, dither phase included.
**A composition test that re-proves its blocks' laws is a slower copy of them.**

<details>
<summary>The original entry</summary>

### D19e. **The shell's render path has never been simulated** — found 2026-09-04
Grepped tree-wide: `render_tri_valid_i` is driven to `1'b0` and nothing else.

    tests/shell/tb_zhao_shell.sv:190   .render_tri_valid_i(1'b0)

and that is the **only** testbench that instantiates `zhao_shell_top`. The bench
says so itself at line 185:

> *"This bench drives the shell CMD/MEM/VIDEO path and **does not draw**. The
> render port is held quiet rather than left dangling, so a future bench that
> DOES draw has to name every pin it uses instead of inheriting an X."*

The other references are the port declaration, the instantiation, a QSF virtual
pin, and `zhao_prod_top` — a resource top where **nothing is wired to
anything**.

**So `zhao_shell_top`'s composition of geometry and raster is elaborated,
fitted and timed, but has never been RUN.** The blocks inside it are well
tested — `render_pipe_directed` drives `zhao_geom_bin_pipe` and the whole raster
chain against `zref` — but that test instantiates the bin pipe **directly**. It
does not go through the shell, so the shell's own wiring of it is unexercised.

</details>

### What this does and does not mean for D1

**It does not invalidate the 100 MHz result.** Timing is structural: the fitter
analysed the real elaborated netlist, and a path's delay does not depend on
whether a testbench ever toggled it.

**It does mean the machine that closed timing has never drawn a triangle
through the port it closed timing on.** That is worth saying plainly next to
the number.

### And it is the true cost of D22 stage 1

`reports/D22-WIRING-PLAN-20260904.md` already carries the correction that
"the picture does not change" is unavailable as evidence. This is why. Stage 1
introduces exactly one new risk — **wiring** — because `geom_setup_directed`
already proves `zhao_geom_setup` equals `zref` bit for bit, so a side-by-side
composed test would pass trivially and prove nothing about the connection.

**A wiring error can only be caught in the wired shell.** So stage 1's real
scope includes **giving `tb_zhao_shell` a drawing path**, which is a
pre-existing gap it inherits rather than creates. Adding nineteen blocks to a
composition no simulation drives would compound the problem rather than find it.

### D19f. **The renderer writes under the BLIT'S lease and the BLIT'S span**
Traced 2026-09-04 while making the shell draw. `zhao_shell_top.sv:1234`:

    assign map_valid_q = fb_lease_valid;   // from VIDEO.SLOTMGR
    assign map_slot_q  = fb_lease_slot;
    assign map_span_q  = r_blit_len;       // latched from dpy_blit_len

and those three feed **both** `zhao_mem_guard:u_guard_blit` **and**
`u_guard_render` (lines 676-678 and 837-839). The shell's own comment states the
intent — *"THE GUARD WINDOW IS THE LEASE... while a lease is live the guard
admits writes to that slot, and when it ends the window shuts with it"* — and
that is a good rule. The consequence is the part nobody has written down:

**`RASTER.FBWRITE` (ENGINE0) can only write while a DISPLAY BLIT lease is live,
into a window whose length is that blit's `len`.**

This is why `shell_draw_directed` sees `fatal=1`: a render-only frame has no
lease, so `blit_ok` is false, so the guard refuses and `fbwrite` latches
`fatal_error_o`. **Correct behaviour, and the first observation of it.**

### The question it raises for D22

For Phase 2 the coupling is harmless — the blit's `len` is `canvas_bytes(mode)`,
which is exactly the canvas the renderer fills, so the two windows coincide.

**But the geometry front end is what makes the console draw its own frames.**
When geometry produces the picture, is a display blit still the thing that opens
the guard window? If a frame is rendered and never blitted — or blitted with a
different length — the renderer has either no window or the wrong one.

**Not a defect today**, and deliberately not filed as one: nothing renders in
Phase 2, so nothing has ever needed the window without a blit. It is filed
because **D22 is the change that makes it matter**, and because the answer
("the renderer gets its own lease" vs "a frame always carries a blit") is an
architecture decision rather than a wiring one.

**ENFORCED-BY:** `tests/shell/shell_draw_directed.cpp` asserts the refusal, so
the day this coupling changes, that test says so.

### D19g. **FIXED — `RASTER.FBWRITE` read the guard's verdict a cycle early**
**The console could not write a single framebuffer row.** Found and fixed
2026-09-04 by `tests/shell/shell_draw_directed.cpp`, the first test ever to
drive the shell's render path.

    zhao_mem_guard.sv:185   rsp.ready = !fwd_active;   // a LEVEL
    zhao_mem_guard.sv:186   rsp.ok    = rsp_ok_q;      // "verdict 1 cycle
                                                       //  after accept"
    zhao_mem_guard.sv:202   rsp_ok_q <= 1'b0;          // default every cycle
    zhao_mem_guard.sv:231   if (req.valid && !fwd_active)
                              if (pass_ok) rsp_ok_q <= 1'b1;

`ready` is high before any request; `ok` is a **one-cycle pulse the cycle
after**. `zhao_raster_fbwrite`'s `W_REQ` sampled both together, so its first
burst read `ready=1, ok=0` — the reset value — took the "guard REFUSED" branch,
latched `fatal_error_o` and dropped the row. **Every frame unpublishable,
nothing ever written, against a guard that had refused nothing.**

`zhao_debug_frameblit` — the only other guard client — has always been correct:
`B_GUARD_REQUEST` waits for `ready`, and a separate `B_GUARD_VERDICT` reads the
answer one cycle later. **fbwrite now has that shape (`W_VERD`).**

### Why nothing caught it, and this is the part worth keeping

`tests/render/render_fb_directed.cpp` played a guard that answered `ready` and
`ok` in the **same cycle**:

    rsp = (uint8_t)(4u | (ok ? 2u : 1u));   // ready | ok, together

**The model and the DUT shared one wrong assumption.** The block passed its own
differential — 24 checks, thousands of pixels — and could not write a byte
through the real guard. The model now delivers the true one-cycle pulse, which
is what makes it able to fail.

**A played interface that is easier than the real one is not a simplification,
it is a different interface.** This is the same family as the terrain shade
oracle that was verified against its own duplicate, and as `min_m10k` measured
against a brief that counted something else.

### The result

    before   pixels=0     bursts=0    issued=0     fatal=1
    after    pixels=3328  bursts=208  issued=3328  retired=3328  fatal=0

Issued and retired balance, the frame drains, the framebuffer slot changes.
**A triangle offered at the console's edge is rendered into memory** — the first
time that has happened in simulation.

`shell_draw_directed` 16 checks, `render_fb_directed` 24 checks.

**NOT claimed:** that the pixels are the right ones. That is
`render_pipe_directed`'s job and this is not a picture test.

**Needs a composed fit**: `zhao_raster_fbwrite.sv` is a shell source and D1
closed at +0.057 ns, so the extra state must be re-measured.

<details>
<summary>The original OPEN entry</summary>

### D19g. `fbwrite` latches `fatal_error_o` with NO guard violation — **OPEN**
`tests/shell/shell_draw_directed.cpp`, measured 2026-09-04.

Driving the shell's render port with a granted framebuffer lease:

    lease opens=1  slot=0  span=245760      <- exactly ZHAO_FB_SLOT_SPAN
    render guard violations=0               <- the guard refused NOTHING
    latched stream_err=0  (whole drain)
    fatal=1, first at drain step 234
    pixels=0 bursts=0 issued=0 retired=3328

`zhao_raster_fbwrite` has **two** documented setters for `fatal_error_o`:
`stream_error_o` (fbwrite.sv:254) and the guard-refused branch (:307). **Neither
is evidenced here.** The render guard's own violation counter reads zero, and
`stream_error_o` was sampled on every cycle of the drain and never seen.

**So either a third path sets it, or one of those two fires without the counter
this test reads.** `retired=3328` against `issued=0` is the other half of the
oddity: words retiring that were never issued, on the same block's counters.

### What this is NOT

**Not a claim that the console is broken.** Nothing renders in Phase 2, this is
the first time anything has driven the port, and the stimulus may still be
incomplete (the render's target address and the canvas geometry come from a
frame setup this test does not perform). **A first-ever exercise finding an
unexplained latch is the expected outcome, not an alarming one.**

It is filed because the alternative is a comment in a test saying "probably the
guard", which is what the first draft of that file said before the counter was
read. **An explanation that was not measured is worse than an open question.**

### The next step

Read the fatal's third path, or instrument fbwrite's two known ones directly.
`shell_draw_directed` asserts the latch as an OBSERVATION, so whatever the cause
turns out to be, the test moves when it is fixed.

</details>

### D19h. **CLOSED — it was the test's unconfigured frame, not the hardware**
2026-09-04, same day. Final state:

    oracle   zref::Binner  13 tiles x 16 x 16 = 3328
    counter  render_pixels_o                  = 3328
    memory   halfwords changed in the slot    = 3328
    fbwrite asked: byte addr 00000000..00001FA0

**All three agree.** `zhao_shell_top:810` takes `render_fb_base_i` and
`render_fb_stride_i` as shell inputs, and `tb_zhao_shell` tied both to zero. With
no row pitch the resolve's address generator put every tile row on the same
bytes — 208 bursts at four addresses, 128 bytes touched, and a counter that was
telling the truth about a frame nobody had configured.

Both are now brought out of the bench and driven (`stride = 4 tiles x 16 px x 2
bytes = 128`).

**A test that does not configure the frame measures an unconfigured frame** —
and it looks exactly like a hardware fault while doing it.

### How it was found, which is the transferable part

The three candidates were: fbwrite over-counts, the writes land outside the peek
range, or they coincidentally match the blit's fill. **All three were wrong**,
and the answer came from probing the guard REQUEST rather than reasoning about
the result: four addresses spanning `0x00..0x60` is 128 bytes, which is exactly
the 64 halfwords that changed — so memory and the requests had agreed all along
and the question was never "where did the writes go" but "why is the address
constant".

**Measure the thing that decides, not the thing that disappoints.**

<details>
<summary>The narrowing step and the original entry</summary>

### D19h. **NARROWED: fbwrite's write ADDRESS does not advance** — still open
Measured 2026-09-04 by probing the render guard's request from the testbench.

    fbwrite asked: 4826 cycles, byte addr 00000000..00000060, len 32
    memory:        64 halfwords changed
    counter:       render_pixels_o = 3328   over 208 bursts

**Memory and the requests agree exactly.** Four distinct 32-byte requests span
`0x00..0x60`, which is 128 bytes = **64 halfwords** — precisely what changed.
The peek range was never the problem: the SDRAM model indexes
`mem[{bank,row,col}]` and the controller decodes `bank=waddr[25:24]`,
`row=waddr[23:11]`, `col=waddr[10:0]`, so that concatenation **is** the linear
word address.

**So one of the three candidates is confirmed and two are dead.** The writes are
not landing elsewhere, and they are not coincidentally equal to the blit's fill.
`fbwrite` issued 208 bursts at four addresses: **the address does not advance.**

### The question that replaces it

Is that a defect, or a frame this test never configured?

`shell_draw_directed` sets the job words and a tile grid, but **nothing tells
the render path where the canvas is or how wide a row is.** The resolve's
address generator needs a base and a stride; with neither supplied, an address
that stays near zero is what an unconfigured frame would produce. That is the
likelier reading and it is **not yet evidenced either**.

**Next:** find what supplies the render frame's base/stride in `zhao_shell_top`
and drive it, then re-measure. If the address still does not advance with a
configured frame, it is a defect in the resolve's address generator.

**What this does NOT change:** the extent claim (13 tiles x 256 = 3,328) rests
on `render_pixels_o`, which is now known to over-count relative to what landed.
**Until the address advances, "the console renders the right tiles" remains a
statement about fbwrite's bookkeeping.**

<details>
<summary>The original entry, before the address was measured</summary>

### D19h. `render_pixels_o` counts 3,328; memory shows 64 — **OPEN**
`tests/shell/shell_draw_directed.cpp`, measured 2026-09-04 after the fbwrite fix.

    counter   render_pixels_o = 3328     fbwrite's own tally
    oracle    zref::Binner    = 3328     13 tiles x 16 x 16
    memory    64 halfwords changed       snapshot before/after, whole slot
    blits completed = 1, lease still live = 1

**The counter and the oracle agree exactly.** What disagrees is the SDRAM
model: a before/after snapshot of the entire 245,760-byte slot finds only 64
halfwords different.

### The obvious explanation is measured FALSE

The natural guess is that the display blit overwrote the render — D19f says the
render's guard window IS the blit's lease, so the two are concurrent by
construction. **It is not that:** `blits completed = 1` before the snapshot was
taken, so the blit had already finished.

### What is NOT established

Three candidates, none of them evidenced:

* fbwrite counts words it did not land;
* the writes go somewhere the peek range does not cover (the model's word
  address space may not be the guard's byte space / 2);
* most writes stored a value equal to what the blit had left, so a
  value-compare cannot see them. The blit wrote `0x11` bytes and the render's
  fill is `0xA5A5…`, which makes this the least likely of the three.

**The test REPORTS this rather than asserting it**, and asserts only what is
certain: memory changed, so the picture reached it. An unmeasured explanation
committed as a comment is the failure this session has already made twice —
once about a guard refusal that never happened, once about a counter read as
coverage.

### Why it matters

`shell_draw_directed` is the only thing that has ever checked the composed
render path, and it is now the only thing that would notice this. The extent
claim (13 tiles) rests on the counter; **the memory-side confirmation of that
claim is what D19h is.** Until it closes, "the console renders the right tiles"
is a statement about fbwrite's bookkeeping and not yet about VRAM.

</details>

### D19i. The island queue died on a preflight throw — **and I probably caused it**
Found 2026-09-04, and the first version of this entry blamed the wrong thing.

`../.tmp/texisland.err` has the actual cause, which the stdout log does not:

    preflight: no source names `module zhao_texture_tmu_pipe`.
    Add it to design/fit_targets.yml or pass -ExtraSources.

`run_block_fit.ps1:297` wraps the whole module loop in ONE `try`, so that throw
ended the run: `tmu_pipe`, `fragrob` and `cache_pipe` never started. Six
preflights out of nine.

### The first version of this entry was wrong

It said the queue "stopped at the first `failed:structure`", i.e. that mosaic's
designed rule failure ended it. **That is not what happened** — a
`failed:structure` is a normal return and the loop survives it. The evidence was
in a file I had not read, and the story was assembled from the stdout log alone.

### And the throw was almost certainly self-inflicted

`zhao_texture_tmu_pipe` **is** a declared target and
`fpga/rtl/texture/zhao_texture_tmu_pipe.sv` **does** declare that module, so the
preflight's complaint was true only momentarily. What was happening at 11:49:
**I was rewriting `design/fit_targets.yml` in place**, repeatedly, while the
queue re-read it once per block.

`io.open(path, 'w')` TRUNCATES before it writes. A reader that opens the file in
that window sees an empty or partial one — and this reader's parser simply finds
no sources for the target and throws.

**Not proven**, and it is written as inference: no timestamp ties a specific
write to the read. But it is the only explanation consistent with a target that
is present before and after, and I had already noted the risk in this run
("editing fit_targets.yml while the queue runs … Risky") and done it anyway.

### Two things to change, both real

1. **`run_block_fit.ps1`'s outer `try` is too broad.** One block's throw should
   skip that block, not the rest of the campaign. The report then shows six rows
   where nine were asked for, and **silence and absence look identical in it.**
2. **Never rewrite a config in place while a process polls it.** Write a
   temporary file and rename — rename is atomic, truncate-then-write is not.
   This session's own live-tree discipline covers Quartus SOURCES and said
   nothing about the fit lane's own CONFIG.

The remaining four blocks were relaunched as **separate invocations**, which
makes point 1 moot for this campaign: a per-block fit is independent by
construction and chaining them gave that up for nothing.

### D19j. D1's top offender needs a CONTRACT change — **owner call**
Traced 2026-09-04 from r13's node list.

The path that reopens D1 at −0.066 ns runs:

    job_first_r -> ts_clear -> u_fragment|s1_retire -> rd_addr_o[3]
                -> tilestore Mux0 (256:1 present) -> rd_pres_q

`ts_clear` enters Fragment's retire chain through one line:

    zhao_raster_tilestore.sv:150   assign wr_ready_o = !clear_valid_i;

Removing it takes 2.7 ns out of the path and is the named next step. **But it
cannot be done as an implementation fix**, because `RASTER.TILESTORE.md` makes
it normative:

> *In-cycle ordering (normative) … 2. then **write** (front bank only) — a write
> is NOT accepted in a cycle where a clear is accepted*
>
> *The single cross-port rule is `wr_ready_o = !clear_valid_i` — a clear locks
> the write port for that cycle, **which is what makes ordering rule 2 sound**.*

Rule 2 has its own directed case, and rule 3 ("a read returns NEW data for a
same-cycle write") makes the difference **observable**: with the lock removed a
read in a clear cycle would see that cycle's write, and today it cannot.

### The alternative, and why it is not obviously wrong

The lock is not needed to keep the DATA right. Clear-then-write in one
`always_ff` is well defined — `present <= '0` followed by
`present[wr_addr] <= 1'b1` leaves exactly the written bit set — so a write
accepted during a clear lands correctly. What changes is the HANDSHAKE and the
same-cycle read, which is precisely what rules 2 and 3 pin.

So the options are:

1. **Amend rules 2 and 3** to allow a write in a clear cycle. Buys 2.7 ns on
   D1's worst path and removes the only cross-port ready dependency in the
   block. Costs a normative change with two directed cases behind it.
2. **Register the clear inside tilestore.** Keeps the rules but blocks the write
   one cycle LATE, which is a different violation of rule 2, not a fix.
3. **Break the chain at Fragment** so `rd_addr_o` does not depend on
   `wr_ready_i`. **CLOSED — checked, and it exists because of the line in
   option 1.** `zhao_raster_fragment.sv:533`:

   > *"The fix is to make the stall re-issue the read it is waiting on. While
   > stage 1 cannot retire (**the store refuses the write — its
   > `wr_ready_o = !clear_valid_i`**), the read port is pointed back at stage
   > 1's own address instead of stage 0's … that hack is unchanged and is still
   > needed for exactly the reason above."*
   > `ENFORCED-BY: raster_fragment_directed.cpp:test_write_stall`

   `rd_addr_o = s1_hold ? s1_addr_r : s0_addr_r` is the fix for a read lost
   during a write stall — and the write stall is caused by the very line option
   1 would remove. Breaking the dependency reintroduces that bug. The block's
   own hygiene note even sanctions it: *"`frag_ready_o` and `rd_valid_o` DO
   depend on `wr_ready_i` … the permitted direction."*

### So there is no local fix, and that is the finding

Three interlocking, tested laws hem this path in:

| law | what it forbids |
|---|---|
| TILESTORE ordering rule 2 | accepting a write in a clear cycle |
| TILESTORE ordering rule 3 | a same-cycle read not seeing that write — so the present lookup stays combinational, and the 256:1 mux stays on the path |
| FRAGMENT's stall re-issue | `rd_addr_o` ignoring `s1_hold`, which exists *because of* rule 2 |

**Every one has a directed case behind it.** The 5.2 ns mux and the 2.7 ns ready
chain are both load-bearing, and option 2 (registering the clear) blocks the
write a cycle LATE, which breaks rule 2 in the other direction.

**So the real choice is two-way, not three:**

* **amend TILESTORE rules 2 and 3** — the one change that pays, and a normative
  edit with two directed cases behind it; or
* **accept 99.34 MHz** on a provisional device with virtual I/O. 100 was chosen,
  not derived.

Both are owner calls. What an implementer can honestly report is that the path
is not a defect and not an oversight — **it is three correct rules meeting.**

### And the contract carries a claim that composition falsifies

> *"Nothing in the block reads a downstream ready, so there is no combinational
> valid←ready path anywhere."*

True **within** the block, and false in the machine: `wr_ready_o` depends on
`clear_valid_i`, and both channels come from the same upstream state machine, so
in `zhao_shell_top` it is a cross-block combinational path. r11 wrote this up in
its own words — *"a valid→ready dependency inside one block becomes a
cross-block combinational path once two of its channels share an upstream"* —
and the contract's sentence has not caught up.

**Filed rather than decided.** Option 1 is the one that pays, and amending a
normative ordering rule to buy timing is an owner's call, not an implementer's.

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

### D19n. `zhao_forge_cliff` cannot fit the part, which is why it always times out
Found 2026-09-04 by applying `QUARTUS_GOTCHAS.md` §14 to the one block in the
census whose row reads `status: timeout` with no numbers at all.

It declares, with `MaxEdges = 2048` and `MaxRuns = 1024`:

    prio_mem_r  [0:MaxEdges-1] x 32b  =  65,536 bits
    edge_key_r  [0:MaxEdges-1] x 12b  =  24,576
    run_mem_r   [0:MaxRuns-1]  x 17b  =  17,408
    edge_span_r [0:MaxEdges-1] x  6b  =  12,288
    alive_r                    2048b  =   2,048
                                        --------
                                        121,856 bits

**And §14 says none of it will infer.** Every array is read by an `assign` into
a combinational signal, and every one of those signals then passes through logic
before reaching a register:

    prio_rd_c  ->  assign prio_key_c = prio_rd_c ^ 32'h8000_0000;   an XOR
    edge_rd_c  ->  c_cj_c = edge_rd_c[17:13];                       into always_comb
    run_rd_c   ->  if (run_rd_c[5:0] == mlen_r)                     a comparison

That is the `tmu_pipe` shape, not the `audio_fifo` shape, on all three.

**As flip-flops, 121,856 registers need about 30,500 ALMs at best-case packing
(4 per ALM) and roughly 64,000 at the ~1.9 registers/ALM this tree actually
achieves — against 41,910 available.** The upper estimate is 153% of the device.

So the `timeout` is very likely **not a tooling flake**: the fitter is being
asked to place a design that may not fit at all, and a fitter that cannot
converge runs until something stops it. That reading is consistent with
`tmu_pipe`, which is smaller, placed successfully, and then spent over two hours
in routing.

**Not proven** — nobody has seen a resource line for this block, because the
harness blanks a timed-out row (D19l's sibling defect,
`reports/FIT-TIMEOUT-CANNOT-FIRE-20260904.md`). The cheap test is to fit it with
the timeout raised and **keep whatever numbers synthesis produces**, which is
exactly the harness change already recommended there.

### D19o. 34 of 85 census rows describe RTL that has changed since
Found 2026-09-04, immediately after `cache_pipe` proved the case concretely.
`tools/quartus/check_rule_freshness.py` now reports it.

Comparing each row's `sourceCommit` against the last commit touching that
block's own `.sv` file: **34 of 85 rows were measured before their source
changed** — 40% of the census.

**`cache_pipe` is the proof, not the hypothesis.** Its row read

    registers 11328   alms 5903   blockMemoryBits 128   status ok

which made it the **third-worst ALM figure in the whole census**. Re-fitted
today: **3,097 registers and 8,320 memory bits.** The number everyone could see
described code that no longer existed, and the only trace of that was a
`lastAttemptStatus` field nobody reads (D19l).

Others carrying a wide gap between measurement and source:

    rtl 2026-09-04  row 2026-08-20  zhao_terrain_project     regs 6685  alms 6068
    rtl 2026-09-04  row 2026-08-20  zhao_mem_guard           regs  177  alms  302
    rtl 2026-09-02  row 2026-08-20  zhao_raster_edgewalk     regs  579  alms 2286
    rtl 2026-09-01  row 2026-08-20  zhao_raster_resolve      regs  248  alms  344

`zhao_terrain_project` matters most of those: at 6,068 ALMs it is the **worst
single entry in the ALM census**, and its numbers are fifteen days older than
its source.

**This is why the census total wants reading carefully.** `npm run
ledger:check` prints *"ALM census 76,672 across 59 measured blocks against
41,910 (183%)"*, and already labels itself an UPPER BOUND because per-block fits
do not share. That caveat is about double-counting. **This is a second, separate
caveat: some of the rows are simply out of date**, and `cache_pipe` alone moved
by 2,806 ALMs' worth of registers.

Neither caveat means the budget is fine. They mean the number is not evidence
either way until the stale rows are re-measured, and that re-measuring is a
campaign-sized job already docketed under the Fmax re-measurement entry.

**Not a defect in the harness** — it records `sourceCommit` faithfully, which is
exactly what made this checkable. The defect is that nothing compared it to
anything until now.

### D19p. `terrain_residency_v2` fails a memory floor, and the message misdiagnoses it
Found 2026-09-04 running `check_fit_rules.ps1` after fixing its blanked-row bug.

    FAIL  zhao_terrain_residency_v2
            block memory 150528 bits < required 167936 -- the storage did not
            infer as memory

**The row is current** — measured 2026-09-04 04:00 against source last changed
02:54 — so unlike two of D19l's three, this failure is real and not a staleness
artefact.

**But the attached diagnosis does not survive the row's own numbers.** The block
measured **1,243 registers**. The shortfall is **17,408 bits**. 1,243 flip-flops
cannot hold 17,408 bits, so *"the storage did not infer as memory"* is false in
the direction it implies: the missing storage is **not in flip-flops**. It is
either in MLAB (LUT-based memory, which counts in neither `blockMemoryBits` nor
`ramBlocks`), or the required figure is an over-estimate of what the design
declares.

That is the same failure as `fragrob`'s gate message, which fired correctly and
explained itself wrongly while 13 M10Ks held the payload — and it is why
`CLAUDE.md` now says **when a tool explains itself, the explanation is a claim
too**.

**What settles it** is the fit's `Analysis & Synthesis RAM Summary`, which names
every array that inferred and its depth and width. That is one file in the fit
workspace, and reading it is how D19m was pinned. It was not read here because
that workspace is from an earlier run and is not the one still on disk.

**Two candidate actions, both cheap, and the choice needs the RAM summary
first:** widen the rule's floor if 167,936 over-counts, or name the specific
array that went to MLAB. Guessing between them from the numbers alone is exactly
what this docket entry is complaining about.

---

## D19q — the II=1 material combiner is refuted by its own tripwire (G1-C)

**2026-09-05. Closed by the measurement; no owner decision needed, recorded so
the shape change is traceable.**

`zhao_texture_combine` fit at commit `28db7708`: **494 ALM, 524 registers,
8 DSP, 100.12 MHz**, status `failed:structure` on two rules —
`DSP 8 > allowed 2` and `registers 524 > allowed 500`.

The DSP rule came from `islandrearchitecture5.md` §3.4, not from this pass, and
`design/fit_targets.yml` recorded the response to a firing **before** the fit
ran: *"If this fires, that IS the evidence for moving to the two-lane scheduled
form."* So the rule stays at 2. Raising it to 8 now would be the "rule written
after the fit it governs" failure `CLAUDE.md` names.

§15.5 closes with the exact instruction the block violates: **"Do not write six
independent `*` operators and assume they pack."** The block has eight inferred
`unit_mul` sites and the fitter gave each its own DSP block.

**The first reading of the result was wrong and is kept in the report.** The
island totals 7,913 ALM against a 7,500 redline but only 14 of 112 DSP, which
made "keep the DSPs, ALMs are the scarce resource" look reasonable. It is not a
trade: §15.5's preferred variant LOGIC2 is **zero DSP at ≤800 ALM** — cheaper on
both axes, because two shared multipliers over three cycles is less silicon than
eight always-live ones. The trade only appears if the parallel structure is
already assumed.

**A separate gap found while reading §15 to answer this:** the architecture
names **eight** recipes; both the RTL and the reference model
(`zref_material.hpp`, `kRecipeCount = 6`) stop at six and refuse recipe 6 as
illegal. The two missing are `TERRAIN_DETAIL_LIGHT` and `TERRAIN_DETAIL_MASK` —
the three-sample terrain recipes, and DETAIL_LIGHT is the worst case the entire
§15.4 two-lane capacity argument rests on. 50 passing checks across two test
files cover six of eight recipes and report coverage of the six they know about.

Full evidence: `reports/G1C-COMBINER-II1-REFUTED-20260905.md`.

**Next:** extend the reference to all eight recipes, then build
`zhao_texture_material_combine_v1` to §15.3/§15.5-A with the per-recipe product
job counters §15.4 requires, then re-fit against the same three unchanged rules.
The refuted block stays in the tree until its replacement measures better.

---

## D19r — the composed island: four wiring defects, and one that no handshake test could see

**2026-09-05. Closed by the composed test; recorded because of what the fourth
one implies about test design.**

`zhao_texture_island_top` wires the eleven approved components to each other.
`island_composed_directed` drives fragments in and requires **every block's
counter to have moved** on the way out. Final: **64 in, 64 out, 0 ID errors,
11/11 checks.**

Getting there took four fixes, each hidden behind the last:

1. **Harness.** The fill model served one halfword where `zhao_texture_cache_pipe`
   counts eight beats per line (`HW_PL = LINE_BYTES/2`). Reported as
   `cache miss 1, dispatch 0`.
2. **AUX token width.** FRAGROB validates a response against the slot *and*
   generation it issued — `$clog2(DEPTH)+GENW` = 12 bits — while `AUX_PIPE`'s
   `TOKW` defaults to 8, so the identity could not round-trip.
3. **`sheet_tok_o` left unconnected.** AUX_PIPE matches a sheet response to its
   request by that token, so the responder had nothing to echo. **This stalled
   the entire island**, because FRAGROB retires in allocation order and one aux
   fragment at the head blocks everything behind it. The visible state was
   `plan 48 | cache 48 | dispatch 48 | bilerp 48 | retired 0` — a perfectly
   healthy sample path and zero output.
4. **The recipe was not travelling with the fragment.** The combiner's
   recipe/weight/sample_count came from the island's *input ports*; with a
   reorder buffer in between, the fragment arriving and the fragment retiring
   are different fragments by construction.

**Number 4 is the one worth keeping.** It would have passed every handshake test
ever written — counters move, fragments retire, throughput is nominal. A wrong
recipe is a wrong picture, not a stall. Only a composed test that cared about
the *result* could see it, which is precisely why the roadmap demands a test
that draws through the added hardware rather than one that checks the added
hardware is present.

**Separately, found while writing the top:** FRAGROB banks all three sample
results (`res_rgb_m [3][DEPTH]`) and its retire read was `res_rgb_m[0]` with no
reader for banks 1 and 2 — the *"returns sample 0 for every recipe"* fault
`MATERIAL.RESOLVE.md` attributes to the surviving TEXJOIN, living one block
earlier in the chain. Fixed by exposing `o_s_rgb_o[3]`/`o_s_a_o[3]`.

**And the near miss.** The first draft of the top held two past retirements in a
shift register and called it a sample bank, because FRAGROB appeared to expose
one colour. That version would have synthesised, fitted, reported an ALM count,
and blended every fragment with its two predecessors while calling it
three-sample material.

Three seams remain, named at their sites: texel-to-channel extraction does one
channel of three; per-sample coordinates are not varied; and the response class
rides the top two bits of the source id because `CACHE_PIPE` has no class lane —
a real constraint on the id space that was nowhere written down.

Full evidence: `reports/G1D-COMPOSED-ISLAND-20260905.md`.

---

## D19s — D22 step 2 has a prerequisite the staircase does not name

**2026-09-05, found while the composed-island fit ran. Analysis only; no code
changed.**

D22's staircase moves the shell's input boundary backwards one geometry block at
a time. Step 1 (GEOM.SETUP) landed on 2026-09-05. The declared order from
`tools/design/compose_order.py` puts **GEOM.DEPTHQUANT** immediately before
SETUP, so step 2 is depth.

**It cannot be a boundary move yet, and the reason is one missing port.**

`zhao_geom_depthquant` turns `v_w_i` into a canonical 24-bit `d_invw24_o`. For
that to be a boundary move, the shell must already CONSUME a depth value that
the bench currently supplies — exactly as step 1 replaced supplied edge
coefficients with computed ones. Enumerating the shell's render port:

```
render_{ax,ay,bx,by,cx,cy}_i   render_{kx,ky,kc}{0,1,2}_i   render_tl_i
render_{min,max}_{x,y}_i       render_src_{a,id}_i          render_state_i
render_texel_{rgb,a,idx}_i     render_{fill,clear}_word_i    ...
```

**There is no `render_depth_i` and no `render_invw_i`** — and for about ten
minutes I concluded from that there was no way for a depth value to enter. Wrong
again. `zhao_raster_tile_pipe.sv:446`:

```systemverilog
  assign frag_depth = fill_r[31:8];
```

**The depth arrives packed inside `render_fill_word_i`.** That 64-bit word is
not a colour; it is a flat-fragment record — `[63:40]` vertex RGB, `[39:32]` the
effect tag, `[31:8]` the 24-bit `invw24` depth, `[7:0]` the stencil reference —
and `render_clear_word_i` carries the tile's clear depth the same way at
`clear_r[31:8]`.

So **step 2 IS a clean boundary move after all**, and the same shape as step 1:
the bench today supplies a precomputed `invw24` in those bits, and DEPTHQUANT
would compute it inside the design from `w`. Nothing new has to be added to the
render port. What DOES have to happen is that the bench stop hand-packing that
field and hand over `w` instead.

### My first reading of this was wrong, and the correction is the point

I first grepped `zhao_shell_top.sv` and `tb_zhao_shell.sv` for `depth|invw`, got
**zero matches in both**, and concluded "the shell has no depth path at all".

That was wrong. The path exists three levels down:

```
zhao_shell_top -> zhao_geom_bin_pipe -> zhao_raster_tile_pipe
                                          -> zhao_raster_fragment   (36 refs)
                                          -> zhao_raster_tilestore
```

and `zhao_raster_tile_pipe`'s own header says fragments are *"rasterized,
shaded, depth/stencil/blend-tested and resolved"*, with depth **flat across the
triangle**. A grep of two files answered a question about a hierarchy — the same
shape as measuring a projection instead of the thing.

The corrected finding is narrower and more useful than the wrong one: the depth
machinery is composed and working; only the **value** has no way in.

### Consequence for sequencing

Step 2 is a straight repeat of step 1's pattern: add `depth_mode_i` to the shell
bench, feed `zhao_geom_depthquant` the `w` and profile, drive its `d_invw24_o`
into `fill_word[31:8]` in place of the bench's precomputed value, and require
the two framebuffers to be **identical**. DEPTHQUANT needs an RCP24 service,
which the composed island already proves works — `zhao_raster_rcp24_svc` is in
it and completed 64 reciprocals in the composed test.

### The real lesson here is about me, not the shell

Three readings of the same question, each confidently wrong before the next:

1. *"The shell has no depth path"* — from grepping two files about a hierarchy
   four levels deep.
2. *"The path exists but the value has no way in"* — from enumerating the port
   list without reading what the ports carry.
3. The truth: the value comes in **packed inside a word named for something
   else**, which no port-name search can find.

Each wrong answer was **simpler than the truth**, which is the asymmetry
`CLAUDE.md` records — and each one would have produced real work aimed at the
wrong place. The only reason this entry is right is that I kept opening the next
file down instead of writing up the first plausible story.

---

## D22 — status after tread 10, stated plainly

**2026-09-06.** The staircase is **finished at ten treads**. Every input the
bench once supplied to the shell's render port has been moved backwards into a
composed production block, ending with memory itself. What each tread removed,
in order: edge coefficients, depth, setup, projection, clipping, triangle
assembly, vertex decode, the asset fetch, the u8 index stream, and the memory
those fetches read.

**What is NOT closed, and why it is not being closed now.** The geometry blocks
are composed into ``tb_zhao_shell.sv``, not into ``zhao_shell_top``. Tread 10
reaches the shell's real memory through exposed guard and beat ports, which this
docket already judged "a test-harness shape [that] would not survive into
production" -- and that judgement stands. The production act is moving
GEOM.MESHFETCH, GEOM.ASSETFETCH, GEOM.VDECODE and GEOM.ASSEMBLE inside the
shell top.

This entry's own sequencing defers that: *"Moving them into the top is a
separate act and belongs after D1."* D1 is the fmax campaign and is still
running. Composing four more blocks into a top whose timing is actively being
fought would make every D1 measurement a measurement of a different design, and
the reason for the ordering has not changed. So the staircase closes here and
the composition waits on D1 rather than being started because the staircase ran
out of treads.

**What tread 10 bought for that future step, concretely.** The memory side is
now known rather than assumed: one geometry client (ENGINE1, slot 3, positional
in the arbiter), one guard, a read-return route that tags by captured owner, and
a burst tripwire that admits it. And the guard's actual handshake -- which both
fetchers had wrong until this tread and no played bench could reveal.

## D22 — the staircase is complete in the bench; the asset fetcher is not

**2026-09-05. Status change, not a closure.**

All six treads of D22's staircase now have composed evidence in
`tests/shell/tb_zhao_shell.sv`. The shell's input boundary has moved from
PRECOMPUTED EDGE EQUATIONS to A MESHLET DESCRIPTOR IN MEMORY:

| step | block | what stopped being supplied | checks |
|---|---|---|---|
| 1 | GEOM.SETUP | edge coefficients | 5 |
| 2 | GEOM.DEPTHQUANT | the `invw24` depth | 7 |
| 3 | GEOM.CLIP | 2A and the scan box | 10 |
| 4 | GEOM.PROJECT | the screen vertices | 9 |
| 5 | GEOM.ASSEMBLE | which three vertices | 8 |
| 6 | GEOM.MESHFETCH | the meshlet itself | 9 |
| 7 | GEOM.VDECODE | the decoded coordinates | 12 |
| 8 | GEOM.ASSETFETCH | the records themselves | 16 |
| 9 | GEOM.ASSETFETCH | the u8 index stream | 17 |
| 10 | (the memory itself) | grants, beats and latency | 12 |

**The uncontended stall baseline, measured 2026-09-05, and what real memory
did to it.** `prefetch_stall_o` was tied off when ASSETFETCH was composed and is
now connected. Against a bench memory that grants immediately and answers in one
cycle:

```
meshlets 1, beats 24, denied 0, refused 0, STALLS 27
```

**27 became 30 on 2026-09-06** when both fetchers were taught the guard's real
two-cycle handshake -- one extra cycle per 64-byte line, three lines, exactly
the change's own prediction and a small corroboration that the repair does what
it says.

**And 30 became 360 through real memory** (tread 10). Twelve times the
uncontended figure, against an identical fetch of 24 beats and a byte-identical
frame. That ratio is the whole reason this baseline was recorded before the
tread rather than after it: a stall count with nothing to compare against is a
number, not a measurement.

Twenty-seven cycles of a consumer waiting on a buffer still filling, against 24
beats of fetch. The consumers begin asking almost as soon as the fetch starts,
so single buffering already costs about a full fetch per meshlet in the BEST
case this machine can have. That is the number the block header says should
decide whether double buffering earns its ~2.4 KB -- and it is non-zero before
contention exists at all.

**Tread 10 must be read against this.** It introduces a real arbiter, and
everything in treads 6 through 9 assumed a memory that never says no. Measuring
the baseline afterwards would give two variables and one number.

Every step draws the SAME triangle both ways and requires a byte-identical
framebuffer, and every step from 3 onward also MEASURES that its own comparison
is capable of failing — a habit step 2 forced, where the framebuffer check
turned out to be blind to depth until depth testing was switched on.

**Tread 7, added 2026-09-05.** The bench stops supplying decoded coordinates in
`asm_vtx_{x,y,z}_i` and supplies the four 32-byte format-0 RECORDS they were
written into; `zhao_geom_vdecode` decodes them inside the composed shell.
Measured: 4 records decoded, 0 refused, 0 format_bad, a BYTE-IDENTICAL
framebuffer, and sensitivity of 1279 words.

The bench's vertex table is driven to **poison** throughout the decode pass, so
a shell that quietly kept reading it draws a different picture rather than
accidentally agreeing — without that, a dead decode path and a working one are
indistinguishable.

**All twelve checks pass**, confirmed after the observation flag was separated
from the gate: `vd_have_r` must clear when the triangle goes away or the next
one would draw from a stale table, and the test read it after the frame
drained, so a working decode reported itself as a failure. The reported flag is
now a sticky "did this table ever fill", cleared only by reset.

It does **not** prove the transform (a record holds MODEL space, the table held
CLIP space; with no transform block in this shell the records carry the
clip-space values and the transform is identity by construction — the device
step 6 used answering the cull with a constant VISIBLE), and it does **not**
advance the GEOM.VDECODE ledger entry, which stays SPECIFIED because
`zhao_geom_vdecode`'s own header is explicit that it is the record leaf and not
the batch engine.

**Tread 10, added 2026-09-06.** REAL MEMORY -- the last thing the bench was
still playing, and the one every earlier tread was measured against.

``af_pool_i`` is a register file the C++ writes and the SystemVerilog indexes.
It grants immediately, answers in one cycle, has no refresh, no CAS latency and
no other client. Treads 6 through 9 all ran on it. In the real-memory pass
GEOM.ASSETFETCH's guard request now goes to ``zhao_shell_top``'s own MEM.GUARD,
the guard forwards to MEM.VRAM.ARBITER on client slot 3, the arbiter offers to
MEM.SDRAM.CTRL, and the beats come back out of ``zhao_sdram_model`` -- past
refresh cycles that close every open row, behind a scanout client reading the
framebuffer for the whole frame.

**The played pool is POISON in that pass**, filled with the decoy vertices, so
a shell that quietly kept reading it draws a different triangle. Without that
device an unconnected memory path and a working one produce the same frame and
"byte-identical" means nothing.

Measured: **14 checks**, ``REAL: 64-bit beats out of the DRAM > 0`` against
zero in the played pass, ``guard_violations 0``, ``route_err 0``, the same
beat count both ways, and a byte-identical framebuffer.

### The three things this tread FOUND

**BOTH GEOMETRY FETCHERS HAD THE GUARD PROTOCOL WRONG, and no bench could see
it.** They required `guard_rsp_i.ready && guard_rsp_i.ok` in ONE cycle.
`zhao_mem_guard` can never do that: `rsp.ready = !fwd_active` is a LEVEL and
`rsp_ok_q` is a PULSE the cycle AFTER the accept -- and the accept is what
raises `fwd_active`. A passing request therefore looked like A DENIAL WITH NO
VIOLATION FLAG, silently, with `guard_denied_o` staying at zero.

`zhao_raster_fbwrite` already waited in `W_VERD` and
`zhao_debug_frameblit` in `B_GUARD_VERDICT`, and fbwrite's own header quotes
the guard line. **Four clients, two protocols, and the two that were wrong are
exactly the two whose memory was played.** Both now wait in S_VERD. Three unit
benches went red on the change and every one was red for the right reason --
they were the models the blocks had been written to match:

    assetfetch_rtl_directed      16 of 28 FAILED  ->  56 checks passed
    assetfetch_random             4 of  9 FAILED  ->   9 checks passed
    geom_meshfetch_rtl_directed   6 of  7 FAILED  ->   7 checks passed

Note that first row. It reported TWENTY-EIGHT checks while failing sixteen and
reports FIFTY-SIX now: a fetch that never completed took the run down with it
and the checks past that point never ran. A failing test that also stops
counting is the broken-instrument law in its most ordinary form.

### The two things this tread FOUND, both of which change what comes next

**Only ONE geometry memory client exists, and it is positional.**
``zhao_vram_arbiter`` builds the controller's tag by casting the SLOT INDEX --
``ctrl_req.client = zhao_client_e'(offer_client)`` -- so slot 3 IS ENGINE1 and
slot 4 IS DEBUG, not by configuration but by construction. And
``zhao_mem_guard`` grants the asset-pool window to ENGINE1 alone; DEBUG falls to
``default: pass_ok = 1'b0`` and "still owns nothing".

So the earlier scoping in this entry -- "a guard per fetcher into slots 3 and 4"
-- is wrong a third time, and for a reason that is not about where modules live.
**A second geometry fetcher has no client identity that the guard admits and the
arbiter carries.** GEOM.MESHFETCH sharing ENGINE1 with GEOM.ASSETFETCH through
one guard, or ``spec/memory_rules.md`` allocating a second geometry client, is a
memory-law decision. It is recorded rather than fudged by putting a fetcher on a
slot that gets relabelled DEBUG one level down and refused.

**The read return had exactly one destination.** The shell's read packer was
wired straight to scanout, because until this tread the shell had one reader.
The burst-owner tripwire below it said reads must be SCANOUT'S -- the same shape
as the write-side bug this file already records, where the guard was taught a
new legal client and the tripwire under it was not, so every legal burst raised
the shell's own corruption alarm. Both are now owner-routed: the owner is
captured AT GRANT (the returning word carries no tag, and one burst is in flight
at a time), and the tripwire admits SCANOUT and ENGINE1 and nothing else.

### What tread 10 does NOT close, stated plainly

**The fetchers are still outside ``zhao_shell_top``.** This entry's own scoping
above named two options and called the port-exposure one "a test-harness shape
[that] would not survive into production". That judgement stands and this is
that shape: ``geom_guard_req_i`` / ``geom_beat_valid_o`` are ports a bench
reaches, not the production composition.

What it buys is not nothing, and it is worth being exact about which half is
real. The MEMORY PATH is now genuinely proven -- guard, arbiter, controller,
DRAM, contention with scanout, refresh -- and the guard's region and client
rules are exercised for the first time, because a played responder never reads
those fields at all. The COMPOSITION is not: moving the geometry chain inside
the shell is still the production step, and it is now known to be cheaper than
feared on the memory side (one client, one guard) and blocked on a memory-law
ruling for the second fetcher.

**Tread 9, added 2026-09-05.** The u8 INDEX STREAM. The cheapest tread in the
staircase, because the work was already being done: of ASSETFETCH's 24 beats,
EIGHT were the index run -- fetched, then discarded, because its index port was
tied off and GEOM.ASSEMBLE still read the bench's flat stream. This connects
the port that was already being fed.

Measured: 17 checks, beats still 24 (the fetch is unchanged; only the consumer
moved), and the decisive one -- **ASSEMBLE named (2, 0, 3), the triplet the
FETCHED run carried, while the bench's stream was poisoned with (1, 0, 3)**.
A shell still reading the bench draws the decoy triangle instead.

Checked before building rather than after: ASSETFETCH answers only from
S_SERVE, so it cannot hand back a half-filled buffer, and ASSEMBLE holds
ix_req_o with no deadline -- its "no ready" means the RESPONDER cannot stall,
not that ASSEMBLE cannot wait.

**Tread 8, added 2026-09-05.** The bench stops SYNTHESISING vertex records and
supplies raw POOL BYTES. GEOM.ASSETFETCH reads the meshlet's footprint out of
them as aligned 64-byte lines and streams each 32-byte record to GEOM.VDECODE,
so the record port stops being a bench input and becomes an internal seam. The
footprint is MESHFETCH's own descriptor answer, so this closes
**MESHFETCH → ASSETFETCH → VDECODE** rather than only replacing one producer.

Measured: `meshlets 1, beats 24, denied 0, refusedfp 0`, four records decoded,
byte-identical framebuffer, sensitivity 1279 words. **24 beats is exactly three
64-byte lines** — the index run of one followed by the vertex run of two —
which is the phase structure the block implements and the reason a single-grant
beat player could never have worked: the index phase would have swallowed the
whole pool and the vertex phase never started. The bench's records are driven
to POISON during the fetch pass.

### What this does NOT close

**THE MEMORY.** Scoped WRONG twice, both times by reasoning about the
composition without checking where the modules actually live. Third attempt,
from the source:

* First I wrote that it needed `zhao_vram_arbiter` put in front. Wrong: the
  shell already instantiates the arbiter and `zhao_mem_guard` twice, with
  client slots `[3]` and `[4]` tied to `'0`.
* Then I wrote that the shape was "a guard per fetcher into slots 3 and 4".
  Also wrong, and for a bigger reason.

**The fetchers are not inside the shell.** `tb_zhao_shell.sv` instantiates
`zhao_geom_meshfetch`, `zhao_geom_assetfetch`, `zhao_geom_vdecode` and
`zhao_geom_assemble` **alongside** `zhao_shell_top u_shell`, not within it.
The guards and the arbiter are INSIDE `u_shell`. There is no path between the
two, and no amount of wiring in the bench creates one.

So tread 10 is not a wiring step at all. It is one of:

* **Move the geometry fetch chain into `zhao_shell_top`**, where the memory
  already is. That is the production shape, and it is what the staircase has
  been walking towards — every tread so far has moved work from the bench into
  composed production blocks, and this moves the composition itself.
* Or expose spare guard-client ports on the shell so the bench can reach them,
  which is a test-harness shape and would not survive into production.

The first is the real answer and it is an architectural step, not glue. What
survives from the earlier scoping is the part that was checked: the arbiter has
two free client slots, so the memory side has room once the fetchers are on the
right side of the boundary.

**And the uncontended baseline is now measured** (27 stalls, above), which is
what any of this has to be read against.

**The asset fetcher.** `zhao_geom_meshfetch` is the only `zhao_guard_req_t`
client in the subsystem, and the bench plays THREE interfaces for it: the
memory guard, the beat stream carrying the descriptor bytes, and the cull
service. Step 6 proves the descriptor path inside the composed shell; it does
not prove the fetcher, and the cull answer is a constant "visible" precisely so
a cull failure cannot pass as a descriptor success.

~~The remaining work is what this entry has said since 2026-09-04: **one asset
fetcher over `GEOM.ASSET_POOL` serving three consumers** — descriptors, the u8
index stream, and vertex records.~~ **Tread 8 serves the VERTEX RECORDS through
it.** The u8 index stream is still the bench's: `ix_req_i` is tied off and
GEOM.ASSEMBLE still reads `asm_index_stream_i`, because a tread moves one
thing. That is the next tread, and it is small — the port already exists on
the fetcher. `spec/memory_rules.md` §5f ruled the region
(22 MiB, `ENGINE1`, read-only, bank 3). The wiring is glue over that one path.

~~**GEOM.VDECODE.** The bench holds the vertex TABLE and looks up the IDs
ASSEMBLE names. Turning 32-byte records into coordinates is VDECODE's job and
step 5 moved the SELECTION, not the DECODE.~~ **Closed by tread 7 above, for
the record leaf only.** The batch engine — `vertex_count`, addressing across
burst boundaries, the all-or-nothing batch rule — is still the bench's, and the
ledger entry stays SPECIFIED.

**`zhao_shell_top` itself.** Every one of these blocks is composed into the
BENCH, not into the shell top, which still instantiates one geometry block.
Moving them into the top is a separate act and belongs after D1.

### Four faults worth keeping, all in the bench, all mine

* **A wait loop that broke at the first interesting signal** ended the frame
  before the triangle reached the raster — step 1's recorded lesson, repeated
  verbatim in step 4.
* **A cull verdict driven from the tick** was high while the block was asking
  and low while it was listening, so it parked in S_WAIT before any sampling
  window opened.
* **A one-shot whose set condition was broader than the event it recorded**
  fired while ASSEMBLE was merely READY and gated the handshake off forever.
* **A level held for the whole offer window** re-submitted the same meshlet
  every cycle: `triangles = 15` for a one-triangle meshlet, with every re-run
  producing the identical picture. Only the counter disagreed.

The last one is the same shape as COMBINE.V1's double-issue earlier the same
day: **correct results from a machine doing many times the work, visible only
in a count.**

---

## D19t — G1-D is measured: 7,720 ALM at 69.05 MHz, and three owner questions

> **SUPERSEDED BY D19v: these numbers no longer describe the tree.** They were
> measured before the ingress-capture repair, which added a 64-entry attribute
> table with combinational LUT-RAM reads on PERSPUV input path, a typed token
> through FRAGROB, per-slot class and palette-binding tables, four output ports
> and LODW 4 to 8. The RELATIONSHIPS below - composed against nominal, redline
> and the standalone sum - are what the next measurement must be read against;
> the absolute figures are history. No re-fit has been run.

**2026-09-05. The composed texture island has a number for the first time.**

```
alms 7720 of 41910   registers 11790   ramBlocks 18   dspBlocks 17   fmax 69.05 MHz
```

Third fit attempt; 9,238 s. Full evidence in
`reports/G1D-COMPOSED-ISLAND-20260905.md`, decision material in
`reports/G1-CLOSEOUT-DECISION-BRIEF-20260905.md`.

**Settled.** The island is 18.4% of the device and 20.5% of the 10% reserve —
capacity is not the constraint. And `zhao_texture_material_combine_v1`
synthesises at **2 DSP** against §3.4's "reject DSP > 2", so §15.5's variant A
LOGIC2 is measured rather than asserted.

**Reopened.**

*The survivors decision*, with a real number: 7,720 against a 7,500 redline is
**+2.9%** — a choice, not a forced cut. The two blocks already over their own
§3.3 lines carry 2,480 ALMs of overrun between them (`fragrob` 1,676/900,
`perspuv` 2,204/900), eleven times the island's excess.

*The composed acceptance floor*: **69.05 MHz against 105 — 34% short.**

**The cause is located and it is not the flattering one.** All twelve worst
paths start at a virtual pin, which reads as a measurement artefact. Splitting
all 2,000 summarised paths by origin refutes it: 405 start at a pin (worst
−4.482), **1,595 start inside the design (worst −3.63, about 73.4 MHz)**.
Removing the boundary entirely buys ~4 MHz.

The limit is `zhao_raster_rcp24_svc`. `pick_i` is a combinational round-robin
scan over all `NCTX` contexts testing `c_val && c_pend`, and its result indexes
`c_m`/`c_x`/`c_w`/`c_ph` — so a valid bit walks eight levels of priority logic
into a memory's read-address port. `~DUPLICATE` shows the fitter already
replicated the register trying to help.

**Deliberately not done:** restructuring RCP24. Registering `pick_i` changes
arbitration timing in a block ruling R7 constrains to ≥ 1.64 products/clock,
currently measured at 1.99. The right shape is §15.5's own — both variants
behind one interface, the fitter decides — not an edit and a hope.

**Also refuted, and recorded rather than re-scoped:** the claim that standalone
sums overstate the island. For ALMs the correction is 2.4% and changes no
decision. It survives only where it is dramatic: the census totals 342 DSP
against a 112-DSP device and the composed island uses 17.

### Three questions for the owner

1. **The redline** — amend, recover from fragrob/perspuv, or cut?
2. **The floor** — is 105 MHz the requirement for the ISLAND, or for the
   composed console? The island is measured alone with 889 virtual pins the
   machine will not have.
3. **RCP24** — is a registered-pick variant worth building, given it spends
   throughput margin R7 constrains?

Nothing is blocked on these. COMBINE.V1's fit is running for its ALM and fmax;
perspuv's is chained behind it to confirm or refute the per-axis split.

---

## D19y — the pre-fit addendum, verified line by line, and the repairs it forced

**2026-09-06.** The owner's second brief --
``reports/TEXTURE-ISLAND-PREFIT-ADDENDUM-20260906.txt`` -- reviewed the island
before the next composed fit and reviewed the COMBINE V2 draft that landed
mid-review. Five agents verified its findings against the source in parallel.
The response is
``reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt``.

**Every claim checked holds.** Three are worse than stated, two defects are new,
and one of my own conclusions was wrong.

### The release gate, and where it stands

| | item | state |
|---|---|---|
| W1 | COMBINE v2's five blockers | **done** ``4dadc0d8`` |
| W2 | PERSPUV second write address | **done** |
| W3 | cache response reservation | **done** ``7d71235a`` |
| W4 | FRAGROB AUX read/valid stage | done, verifying |
| W5 | FRAGROB acceptance predicate | done, verifying |
| W6 | AUX held offer + credit + bounded queue | in progress |
| W7 | COMBINE v2 differential | **done** ``84c70bde`` |
| W9 | typed sample completions | in progress |
| W11 | PERSPUV registered read + e_mant split | **done** ``7d71235a`` |

### The three that are worse than the addendum states

**AUX context is not a timing window.** ``aux_ready_i`` is tied high through
the island, so the mismatched tuple is the ONLY cycle every request gets: every
AUX sheet lookup used the PREVIOUS fragment's world X/Z under the current
fragment's identity. The differential recorded the slot and generation and never
compared the context -- the one observable that would have failed.

**The cache's lost result was live.** Its header said *"Nothing instantiates
this yet"*; it is in the island and the production top with real backpressure
from the dispatcher's FIFO-full. The owner's sequence reproduced from source:
issued 100..107, returned 100 101 102 103 106 107. It does not deadlock -- it
returns correct-looking data under a mismatched tag.

**The bilinear test's other three taps are never fetched.** With
``acc_en = 0001`` lanes 1-3 are never needed, never filled and never written,
yet the response word copies all four unconditionally.
``disp_bil_data[63:16]`` is uninitialised RAM that only ``fu=fv=0``
discards, and the test's own comment claiming the four texels are identical is
FALSE.

### The severity multiplier

FRAGROB retires in ALLOCATION ORDER with no timeout, so any ONE lost sample
stalls the entire island rather than one fragment. That is why typed sample
completions became a fit blocker rather than a tidy-up: every hole in the sample
paths is a dead island, not a wrong pixel.

### Two of my own errors, both caught by evidence rather than by review

**The RAM diagnosis over-generalised.** ``rob_m[seq_head_r]``'s three output
slices read the SAME word, so "read from several places" was wrong about several
arrays. What survives is the inventory fact, not the mechanism I attached to it.

**And removing PERSPUV's allocation-time zero broke depth-zero.** My
justification was that ``e_have[i][0]`` is set only where ``e_q_u[i]`` is
written; that is false at accept, which sets ``e_have`` to ``2'b11``
directly for a depth-zero fragment, so the output read a slot nothing wrote.
**Both perspuv tests passed anyway** -- Verilator zero-fills arrays and the
depth-zero fragment is stimulus index 0, landing in a never-written slot. A
second depth-zero fragment reusing a slot would have emitted the previous
fragment's coordinates in simulation too. Repaired at the OUTPUT with a bypass
to zero, which is what the serial reference forces, and which keeps the single
write address that made the array inferrable.

### The law this run has now learned three times

Do not initialise a per-entry store at allocation: it creates a second write
address, the one thing an M10K simple-dual-port cannot have. COMBINE's brief
states it for scratch; PERSPUV had it in ``e_q_u``; the V2 draft reintroduced
the FLAG half of it. And where the initialisation IS needed -- a sticky flag
like ``e_sat`` -- keep it, keep the array in flops deliberately, and say which
of the two applies at every such site.

## D19x — OWNER BRIEF: the COMBINE / ASSETFETCH recovery architecture

**Received 2026-09-06, filed at
``reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt``.** Indexed here, per the
brief's own instruction, as **the COMBINE and asset-fetch SPECIALIZATION of the
existing texture recovery v2** -- not as a replacement for it, and not as
anything about the terrain or Field contracts.

Examined revision ``2f98562a``. It is a source-grounded implementation brief,
not fitted replacement RTL: no Quartus, Verilator or board run was performed for
it, and it labels every claim MEASURED / SOURCE / DEDUCED / PROPOSED.

**The two headline decisions.** COMBINE needs a different execution
ORGANIZATION, not different material math -- keep all eight recipes, replace
record-by-job priority scans with two explicit product lanes, a fixed
paired-phase schedule, small ready queues, synchronous operand storage and one
whole-context writeback per phase. ASSETFETCH needs OVERLAP AND OWNERSHIP, not
two complete fetch engines -- keep the checked footprint planner and the
repaired guard handshake, add two payload banks, one fill engine, independent
consumers and descriptor lookahead, and share the one permitted ENGINE1 client
through a logical-owner adapter.

### The three prerequisites, and where they stand

**1. The island's reorder buffer has no admission credit. OPEN -- and it
refutes a proof I wrote the same day.** My comment claims the ROB cannot
overflow because FRAGROB admits at most FCTXN fragments. The brief's
counterexample is exact and the claim is false:

> hold the sink not-ready; admit and complete sequences 0..63; entry 0 still
> holds sequence 0; **internal contexts have been released and admit more
> work**; sequence 64 completes and overwrites entry 0.

FRAGROB releases a context at ITS retirement, which is upstream of COMBINE and
of the ROB, so admissions are not bounded by what is parked at the output. A
64-fragment test cannot reach the wrap. The fix is an end-to-end credit --
reserved at ADMISSION, returned ONLY at final external output acceptance --
gating both ``frag_ready_o`` and RCP's valid, so RCP cannot take a job the
caller was told did not handshake.

Not applied yet: ``zhao_texture_island_top.sv`` is inside the composed-island
fit running now (QUARTUS_GOTCHAS 11).

**2. Both geometry fetchers read ingress pins live after acceptance. CLOSED**
(``ec3d86f4``). Verified in source, captured at acceptance, and both files put
under ``check_ingress_capture`` so the two-bank rework cannot reintroduce it
while adding the overlap that makes it bite. Mutating it back exposed that the
gate's port regex could not see ANY port declared with an unqualified typedef --
fixed in ``85865480``.

**3. The committed COMBINE timing exports were the wrong experiment. CLOSED**
(``6ff9d0fa``). They described the 29.74 MHz run; the 36.28 MHz database
survived under ``%TEMP%`` and ``quartus_sta`` re-exported matching reports in
32 seconds rather than a second 13,627-second fit. Setup **−17.561 ns**, hold
**−5.284 ns**, and the worst cone is now named: **recipe state into the
``jobs_by_recipe`` counter** -- the same family ``zhao_mem_guard`` was
repaired against, a decision chain landing on a counter's enable, not the
``Decoder5~9 -> Add46~41`` the superseded report pointed at.

### Corrections the brief makes to this docket's own claims

* **"R6 fully closed" was too broad.** The order test proves its 64-fragment
  workload, not sustained backpressure safety -- see prerequisite 1.
* **The 30-to-360 stall increase does not promise a 12x buffering win.** It
  exposes costs the fake memory omitted; overlap-only ideal speedup for
  two-bank ASSETFETCH is ``(F+C)/max(F,C)``, between 1 and 2.
* **Do not add 2.08 MHz to 36.28** because an unrelated shell comparison found
  that effort difference. "Probably 38 MHz" is not a measurement.
* **The recipe NAMES do not imply their math.** LERP interpolates alpha;
  MODULATE2X doubles alpha; DETAIL_MASK multiplies s0/s1 RGB and s0/s2 alpha.
  It is not a third-sample-weighted colour lerp.

### The first ASSETFETCH reader-side prerequisite is closed

**2026-09-06, ``458418a1``.** ``GEOM.ASSEMBLE`` holds ``ix_req`` throughout its
fetch state, and ASSETFETCH interpreted that level as a fresh request every
cycle. The single-bank implementation happened to overwrite the same one-cycle
answer; the two-bank reader described by the brief would enqueue duplicate
triangles. ASSETFETCH now captures one triplet index per request episode,
performs one synchronous read, returns one ``ix_valid`` pulse, and rearms only
after ``ix_req`` deasserts.

The directed gate holds the request for 12 cycles, poisons the live index after
acceptance and then rearms a second request: **66 checks pass**. Compiling the
same gate against the pre-fix RTL makes exactly the two new checks fail (repeated
valid and the poisoned triplet), so the detector was shown to fire. The 240-job
random differential remains green: 217 admitted, 23 refused, 11,160 beats,
**9 checks**.

**2026-09-06, ``c1d066b8``.** The shared ENGINE1 adapter now retains the recorded
logical owner when the expected useful-word count arrives without downstream
``LAST``. It reports LONG once, discards surplus words in an explicit drain
state, blocks both requesters through physical ``LAST``, and only then admits the
next logical request. Previously it returned to IDLE at the useful count, so the
remaining owned beats were misclassified UNOWNED and a queued request crossed
the adapter boundary before the physical response had ended -- contrary to the
one-logical-request-in-flight proof required by §12.4.

The strengthened four/eight-word directed gate passes **35 checks**. Its overlong
64-byte case queues a 32-byte descriptor during the surplus, proves that request
remains held through physical ``LAST``, then proves its four returned words are
uncontaminated. Compiling the same gate against the pre-fix RTL fails exactly the
two ownership discriminators: the queued request is accepted early and four
surplus beats are counted UNOWNED.

**WP6 compact single-bank checkpoint, 2026-09-06, ``41baae63``.** ASSETFETCH
still fetches the exact oracle-planned sequence of aligned 64-byte lines, but now
discards every whole prefix/suffix word during fill and writes each useful stream
from local RAM offset zero. It retains the exact index-byte count for the final
partial word, uses 48 meaningful words in a 64-word index allocation per copy,
and reduces the vertex payload from the old 264 raw-line words to exactly 256.
Reader latency, one fill engine and the held-request episode protocol are
unchanged; overlap is not yet claimed.

The strengthened RTL/oracle gate sweeps all **8 index prefixes × 2 vertex
prefixes**, uses triangle counts 17..24 to cover all eight final-word byte
remainders, retains zero-stream cases, and adds the real worst footprint: seven
index plus 33 vertex lines, **40 lines** total. Fixed RTL passes **152 checks**.
A separately built mutation which writes retained words at the old raw-line
offset while keeping compact readers fails **30 of 152** exact-byte checks, so
the compaction detector is nonvacuous.

## D19w — the guard-verdict mistake was in THREE clients, and the third fails the other way

**2026-09-06.** D22 tread 10 found both geometry fetchers testing
`BTguard_rsp_i.ready && guard_rsp_i.okBT` in one cycle, which
`BTzhao_mem_guardBT` cannot produce. Fixing two instances leaves the RULE
unenforced, so `BTtools/rtl/check_guard_verdict.pyBT` now enforces it, wired into
`BTnpm run design:reportBT`.

**It found a third, in MEM.SCANOUT's fetcher, and the polarity is inverted.**

```systemverilog
  F_REQ: if (guard_rsp.ready) begin
           if (guard_rsp.violation) ... // retry the line
           else                     ... // -> F_BEATS
         end
```

The fetchers tested `ready && ok`, so they read every PASS as a denial and
stalled. This tests `ready && violation`, so it reads every DENIAL as a pass and
drops into F_BEATS to wait for beats a refused request will never send. Same
misreading, opposite failure.

**The retry path and `BTviolation_nowBT` were both structurally dead**, and the
comment on the arm -- *"denied (impossible in Phase 2)"* -- is true of the region
rules and was NOT the reason the arm never ran. That is the shape this docket
keeps recording: an explanation that is correct about the observation and wrong
about the mechanism, which is how a dead safety path keeps its cover.

A dead retry on the DISPLAY fetch is not cosmetic. It is the difference between
a mis-programmed scanout base retrying its line and the display pipe stalling on
beats that are not coming. F_VERD added; the arm is reachable.

### The gate, and the two things that were done to it before trusting it

**It reported three offences that were all PROSE** on its first run -- including
two inside the very comments documenting the repair, and one in
`BTzhao_raster_fbwriteBT`'s header where it QUOTES the guard line to explain why
that block gets it right. A gate that flags the documentation of a fix as the
absence of the fix trains the reader to ignore it. Comments are stripped first.

**And it self-tests at import**, on a known-bad and a known-good arm, refusing to
run if either verdict is wrong -- the rule CLAUDE.md states after a detector was
written with an escape a shell heredoc ate, so it matched nothing and printed
reassurance for its whole life.

Its client list is WRITTEN OUT rather than discovered, for the same reason: a
scanner that finds its own inputs reports "0 problems" just as happily when its
pattern stops matching.

## D19v — COMBINE.V1 uses 2 DSP, claims ZERO, and the gate is set at exactly 2

**2026-09-06. Measured, from the committed fit row; no new fit needed to find
it.**

    zhao_texture_material_combine_v1   alms 1475   registers 893   dspBlocks 2
    rule violations: ALM 1475 > 800, registers 893 > 500, fmax 36.28 < 125

**DSP is not in that list, and that is the problem.** `design/fit_targets.yml`
gates the block at `max_dsp: 2` -- S3.4's "reject DSP > 2" tripwire, inherited
unchanged from the refuted II=1 block. The measurement is exactly 2. No rule
fires.

But this block's own header says it should be **zero**:

> "`multstyle = \"logic\"` is the whole point of this block. Without it these
> two `*` operators become two DSP blocks and the exercise has bought nothing;
> with it they are exact multipliers in ALM logic and the block uses ZERO DSP,
> **which is variant A's stated advantage over B**."

Two call sites, two DSP blocks, one each. The attribute is doing nothing.

**The fit target predicted this and could not catch it.** Its comment says: *"If
this one also breaches DSP, the LOGIC2 attribute is not doing what S15.5 variant
A says it does and that is the finding."* It did not BREACH. It landed exactly
on the ceiling, and a ceiling inherited from a design that was allowed 2 cannot
gate a design that claims 0. The gap between what the block says and what the
gate allows is precisely two, and the measurement sat in it.

### Why the attribute does nothing, and the tree already knew

`multstyle` is attached to a **function-local automatic variable**:

```systemverilog
  function automatic logic [7:0] unit_mul_logic(...);
    (* multstyle = "logic" *) logic [16:0] p;
```

`p` is not a persistent net; the function is inlined at each call site. And
`fpga/quartus/shell_fit/zhao_shell_fit.qsf` already carries the general lesson,
in the paragraph about verifying that a directive is echoed back:

> "An assignment Quartus silently ignores looks exactly like one that did not
> help, **which is how `multstyle` was believed for weeks**."

So the same attribute has already been believed and found inert once in this
tree, and the block that most depends on it still asserts the old claim in its
header.

### What this changes

* **Variant A's stated advantage over B is unmeasured, not established.** A is
  "LOGIC2, zero DSP"; the measurement is 2 DSP. Until the multiplies are
  genuinely in ALM logic, the A/B comparison has not been run.
* **This is the third time the comfortable reading arrived first here.** The
  refuted block's 8 DSP looked like Quartus ignoring `multstyle` and was
  actually fourteen multiplier sites -- the tool was fine and the RTL was
  wrong. This is the opposite and it took the *same* evidence to separate them:
  count the sites, then compare with the DSP count. Two sites, two DSP.
* **The lever is the QSF, not the source.** `set_global_assignment -name
  MULTIPLIER_STYLE LOGIC`, with the fitter's own settings table read back to
  confirm it was honoured -- which is the rule that same QSF states about
  `OPTIMIZATION_MODE` and does not yet apply to this.
* **And `max_dsp` for this target should be 0**, not the inherited 2. A gate
  set to what the previous design was allowed cannot test what this one claims.

Not applied yet: `zhao_texture_material_combine_v1.sv` and the block-fit runner
are inside the composed-island fit running now (QUARTUS_GOTCHAS 11, the
live-tree trap). Queued for when that closure clears.

## D19u — variant A LOGIC2 is measured and fails its own acceptance condition

**2026-09-05. G1-C's replacement combiner now has a full fit.**

> **These figures PREDATE the counter narrowing (2c858dbf).** The critical
> path they report is the S15.4 counters -- a decoder feeding a wide adder on
> all 1,820 summarised paths -- and that is exactly what was then narrowed from
> 32-bit to 2-bit per-cycle increments. Whether it moved 29.74 MHz is the
> question a re-fit answers and no prediction is offered here.
>
> That re-fit has been attempted three times: twice it died at
> run_block_fit.ps1's 3,000 s default timeout and was misread as a fitter
> failure, once it was killed deliberately. A fourth is running with a larger
> budget. The block takes longer than fifty minutes -- its placement
> preparation alone exceeds half an hour, and it carries 596 virtual pins
> against 744 registers, so its placement problem is far larger than 1,994
> ALMs suggests.

```
alms 1994 | registers 1383 | dspBlocks 2 | fmax 29.74 MHz | 9,417 s
```

§15.5 does not merely describe variant A, it states when it is preferred:
*"two exact 9x9/9x8 multipliers in ALM logic; zero DSP; **preferred if <= 800
ALMs and >= 125 MHz**."*

| | measured | bar | |
|---|---|---|---|
| DSP | 2 | ≤ 2 | **met** |
| ALMs | 1,994 | ≤ 800 | 2.5× over |
| fmax | 29.74 MHz | ≥ 125 MHz | 4.2× short |

**The DSP rule is met and nothing else is.**

Against the block it replaced — II=1 at 494 ALM / 524 reg / 8 DSP / 100.12 MHz
— six DSP blocks were bought for **1,500 ALMs and 70 MHz**, on a device with 112
DSPs of which the whole composed island uses 17.

**The tripwire was still right to fire.** The II=1 block wrote eight independent
`*` operators and assumed they would pack; they did not. What §3.4 could not say
is that the replacement should be measured against its own bar before adoption.

**Where the ALMs actually are:** not in the multipliers. Two 9×9 multipliers in
logic are ~130 ALMs. The rest is the §15.3 **microjob scheduler** — a record
file holding three samples, intermediates and three masks, plus a two-lane issue
loop scanning every record and job every cycle. Correct, and expensive:
`material_combine_v1_diff` passes 16 checks including exact per-recipe job
counts, out-of-order retirement and back-pressure.

**No cause is claimed for the 29.74 MHz.** The critical path has not been read —
one file, and the next action. On the island the obvious explanation turned out
to be worth 4 MHz of 36.

### The decision

§15.5's own procedure now points at variant B (`DSP2_PACKED_OR_EXPLICIT`,
capped at the same 2 DSP, *"accepted only if the fitter proves the count and
composition improves"*). Three options, none picked here:

1. **Build B and compare** — the cap makes it a fair test on ALMs and fmax alone.
2. **Attack the scheduler, not the multipliers** — that is where the 1,994 ALMs
   are, and it is orthogonal to which variant supplies the arithmetic.
3. **Reconsider whether 2 DSP is worth 1,500 ALMs** on a device using 17 of 112
   — a re-reading of §3.4 in the light of a measurement it did not have, and an
   owner call rather than a quiet amendment.

**The refuted `zhao_texture_combine` stays**, with its manifest row and
`texture_combine_diff.cpp`, because the replacement has been measured and has
not earned the deletion.

Evidence: `reports/G1C-COMBINE-V1-VARIANT-A-MEASURED-20260905.md`.

## D19v — the island never captured its fragments, and its counters could not say so

**Status** repaired and gated. **Evidence**
`reports/G1D-INGRESS-CAPTURE-REPAIR-20260905.md`, 25 checks in
`island_composed_directed`.

The combiner job-distribution anomaly opened in D19t was not in the combiner.
The island read **every per-fragment attribute off its own input pins at the
point each was consumed** — ten signals, including PERSPUV's `u/w` and `v/w`
numerators. A fragment spends ~12 clocks in RCP24 and PERSPUV, so each tap
sampled a different fragment, and once submission stopped the pins held the
last one.

**What the counters could not say.** 64 fragments in, 64 out, every block's
counter moving, totals plausible — while only **25 distinct fragments existed**,
39 were lost and one was delivered 24 times. Three hypotheses were written down
confidently and all three were refuted (the OR-ed field, neighbour
mis-association, double issue). Recording per-fragment **identity** settled it
in one run, using a port that was already exposed.

Counters aggregate away the field a transport bug destroys. `CLAUDE.md` says
"counters see what pictures cannot"; the converse is now also earned —
**counters cannot see identity**. When a count is wrong, measure the identity
of the things being counted before theorising about the count.

**What moved**, each independently predicted by the test's drive pattern:

| | before | after |
|---|---|---|
| combine jobs by recipe | `0 0 0 4 0 0 24 112` | `0 32 32 32 0 0 48 32` |
| bilerp / palette | 131 / 61 | 96 / 96 |
| aux accepted | 37 | 22 |
| distinct fragments out | 25 of 64 | 64 of 64 |

**FRAGROB was innocent throughout** — its head slot walked `0..15` exactly four
times. It was handed the wrong data.

### Two defects it had been masking

* **The CLUT path was black.** The test never followed the palette load
  protocol, *and* the island asked the palette with two **overlapping** slices
  of the response routing token — the "slot" was the low two bits of the
  "generation", which was FRAGROB's residency counter. 96 lookups / 96 stale /
  0 cold → 96 / 0 / 0. Owner v2, Appendix B: *"A provenance source ID is not an
  internal transaction-routing ID."* Staleness and coldness are counters now.
* **Order is not preserved** — 10 fragments out of place, displacement 8, drain
  phase only. The permutation is present at FRAGROB's *input*, so this is the
  misplaced ordering boundary of owner priority 6. Recorded with a regression
  guard on the measured bound, not patched.

### Gates, each mutation-verified

`tools/rtl/check_ingress_capture.py` enforces the rule rather than the ten
instances, wired into `npm run design:report`. **Its first version failed its
own mutation test** — it caught a direct pin tap but passed the capture word
laundered through one wire, which is exactly the bug it was written for. A
sticky flag now guards the completion merger's undocumented cross-module
assumption (the palette cannot be back-pressured, so it must win, which is safe
only while FRAGROB's `tmu_rready_o` is tied high).

Under the historical-bug mutation **the old aggregate checks still pass**;
`moved >= 2` sees four counters moved and is satisfied. That is the argument for
per-input identity over population statistics, demonstrated rather than
asserted.

### What this invalidates

**D19t's 7,720 ALM / 69.05 MHz is stale.** The capture table adds combinational
LUT-RAM reads on PERSPUV's input path. No prediction is offered — the island's
last obvious explanation was worth 4 MHz of 36. Per owner priority 5 the
corrected **combiner** leaf fit is the next bounded attribution experiment, not
a full island refit.

## D23 — terrain mipmapping: three defects VERIFIED IN SOURCE before any of it can work

**2026-09-05. Owner ruling landed, architecture delivered, and two blockers
confirmed by inspection — not by a fit and not by simulation.**

The owner ruled OUT full trilinear as a requirement and ruled IN the cheap
option: *"Sample one texel from level L / Sample one texel from level L+1 /
Decode both palette entries / Blend the two resulting colours using fractional
LOD. Blend colours, never palette indices."* Architecture:
`reports/TERRAIN-MIP-TWO-LEVEL-BLEND-ARCHITECTURE-20260905.md`.

Two defects stand in front of it. **Both were read directly out of the current
source and are stated as source deductions, not measurements** — no simulation
or fit was run for either.

### 1. The mip level cannot be non-zero at all -- FIXED

```
island:   .req_lod_i({{(8-LODW){1'b0}}, fr_tmu_lod})   // LODW=4 -> lands in [3:0]
planner:  lvl_req = m_mip_en ? t0_lod[7:4] : 4'd0;     // reads [7:4] -- always zero
```

The field is Q4.4: integer level in `[7:4]`, fraction in `[3:0]`. The island
zero-extends a 4-bit LOD into the **fraction** nibble, so the planner's integer
level is always zero and enabling `MIP_EN` changes nothing. Everything the
planner already implements — clamping, selected-level UV scaling, packed
mip-chain offsets — is unreachable through this wiring.

**FIXED in a4596338.** LODW is now 8 and the connection is direct.
The oracle already declares this ABI — `zref_texture.hpp` line 151,
`uint8_t lod = 0;  // U 4.4` — and the planner reads [7:4] as the integer
level, so the oracle and the planner agreed and the ISLAND was the outlier.
That settles which side was wrong rather than leaving it a judgement call
between two defensible widths. Inert at
that commit and verified so: the composed test drives bind_mode_i = 0, so
MIP_EN is low and lvl_req is 0 either way; every measured number is unchanged
and 25 checks pass. Fixed separately from the two gated repairs because it
changes nothing currently reachable, whereas the byte-select and expansion
fixes change star and sky bytes.

**The defect and the feature share one fix**: those misplaced fractional bits
*are* the blend weight the two-level blend needs.

### 2. Every CLUT lookup reads the same byte, whichever texel was addressed

```
.lu_idx_i(disp_clut_data[$clog2(PAL_ENTRIES)-1:0])   // = [7:0], always
```

`disp_clut_data` is `DATAW` = 64 bits (4 lanes x 16). A CLUT8 halfword holds
**two** texels, and which one is wanted depends on the addressed u — carried as
`plan_acc_fu`, which reaches the bilerp lane and **nothing else**. Grepping the
island for any other byte selection off that word returns zero hits. So odd
texels decode the wrong byte, always.

**This qualifies D19v.** That entry reports making the CLUT path return colour
instead of black, and it did. It did not make the path correct: the palette is
now resident and answering, and roughly half the indices it is asked for are
still the wrong texel. The path is LIVE, not RIGHT, and the directed test
asserts residency and non-blackness rather than index correctness — which is
exactly the gap a colour check cannot see.

### The design's load-bearing claim, checked against the RTL

The architecture puts both texels in ONE planner request across two cache lanes
rather than issuing two independent requests. That is the decision the whole
cost argument rests on, so it was checked rather than accepted:

* `zhao_texture_tmu_plan` already emits `acc_en_o` as a four-bit LANE MASK
  — `4'b1111` when filtering, `4'b0001` for nearest. A two-lane mask is the
  same port, not a new one.
* `zhao_texture_cache_pipe` takes `acc_en_i[LANES-1:0]` with LANES = 4 and
  `acc_addr_i[LANES*32-1:0]` — a SEPARATE 32-bit address per lane, so two
  lanes can address two different mip levels at different base offsets.
* Its all-hit path accepts and retires ONE ACCESS PER CLOCK, which is what
  makes the pair atomic without new pairing state.

So the shape the design needs is already supported, and today's nearest path
simply uses one lane of four. This is a source deduction about interfaces, not
a simulation or a fit: nothing has been built or measured.

**And the throughput premise holds too, which is why it is fragile.**
`zhao_texture_fragrob` issues TMU requests through
`I_IDLE -> I_READ -> I_HOLD` and back — one request every THREE clocks. So a
two-clock pair genuinely does hide behind it at today's rates, exactly as the
architecture argues. That safety is borrowed, not earned: it comes from FRAGROB
being slow. When the v2 recovery's WP3 makes issue one-per-clock, the pair
becomes the limit and the blended class caps at half a sample per clock. The
feature would then be paying for a bottleneck the machine does not have yet.

### The proposed new sample CLASS is not a free encoding

The architecture fences the blended path with a new class `CLS_CLUT_MIP = 3`,
alongside `CLS_CLUT=0`, `CLS_NEAR=1`, `CLS_BIL=2`. The 2-bit field has the
spare value, but the dispatcher does not:

* `zhao_texture_rsp_dispatch` holds THREE class queues — `cq_wp[3]` — and its
  routing loop is `for (int i = 0; i < 3; i++)` with
  `cpsh = dispatch_fire && (head_cls == 2'(i))`. Encoding 3 matches no `i`,
  so nothing is pushed.
* And the entry is still consumed, because `head_room` ends:

```systemverilog
default:  head_room = 1'b1;   // an unknown class is dropped, not stalled
```

So a class-3 response is popped from the raw FIFO and pushed to no queue. The
fragment waiting on that sample never completes, and FRAGROB retires in
allocation order, so one stuck head blocks the island — the signature the
aux-token bug produced.

**This is a correction to the architecture's own description of it.** It called
this a *silent* unknown-class drop; it is not silent, it is commented on the
line that does it, and dropping was chosen deliberately over stalling because
stalling deadlocks. The behaviour is intended. What follows from it is the part
that matters: **taking encoding 3 requires a fourth class queue**, with its own
depth, its own occupancy counter and its own head-of-line consequences, not
merely a spare number. Any cost estimate that treats the new class as free is
short by that queue.

### 3. The palette's 565 to 888 expansion ZERO-FILLS where the ABI replicates

`zref_texture.hpp` line 15 names the expansion outright — *"the RGB565 -> RGB888
expansion is zref::sky::rgb565::to_rgb888"* — and that function replicates the
high bits:

```cpp
r = (r5 << 3) | (r5 >> 2);   //  31 -> 255
g = (g6 << 2) | (g6 >> 4);
b = (b5 << 3) | (b5 >> 2);
```

The island appends zeros instead:

```systemverilog
{pal_lu_rgb565[15:11], 3'b000, pal_lu_rgb565[10:5], 2'b00, pal_lu_rgb565[4:0], 3'b000}
```

So 31 becomes 248 rather than 255, and full white returns as 248, 252, 248 — a
systematic darkening that is worst at the top of the range and exactly zero at
the bottom, which is why it survives a "did anything paint" check.

This is not a difference of opinion: the ABI is written down and the island
disagrees with it. It compounds with defect 2 — the wrong byte is selected, and
then the colour that byte names is expanded wrongly.

### A LATENT instance of the carriage defect, in the binding

Found by turning the same lens on the island's non-`frag_` inputs rather than
assuming the repair had covered everything.

`bind_base_i` and `bind_mode_i` are consumed at the TMU planner
instantiation — downstream of admission by the entire RCP24 and PERSPUV
latency, which is exactly where the ten repaired taps were reading from.

**It is not a bug today.** The island carries ONE global binding, so the value
is constant across every fragment in flight and a late read returns the same
thing. That is the same reason the shell may legitimately read `fb_base_i` at
pixel-write time: frame-scoped configuration, not transaction-scoped data. The
distinction the ingress gate has to make is scope, not timing.

**It becomes a bug the moment bindings vary per fragment** — and the owner's
two-level mip blend is the change that does it. That design needs a per-fragment
palette slot and generation, which is why `frag_pal_slot_i` and
`frag_pal_gen_i` were added to the ingress packet and are captured at
admission. A per-fragment `bind_base` has to go the same way, and if it is
added as a plain input consumed where `bind_base_i` is consumed now, it will
reproduce the exact defect that lost 39 of 64 fragments.

The gate does not currently watch these, because its island contract is scoped
to the `frag_` prefix and these are genuinely frame-scoped at this commit.
Recorded here rather than papered over with an exemption that would claim more
than it checks.

### Not fixed here, deliberately

The architecture gates both as WP-M1 behind a reviewed before/after delta,
because they change star and sky bytes from wrong-against-the-oracle to
right-against-it. The owner's constraint is that the terrain path must not
silently change materials that depend on raw palette-index semantics; folding
these in quietly would break that, and skipping them would let the blend path
inherit wrong indices. So they are a gated step with a visible delta, not a
drive-by fix.

### Three conflicts that need an owner ruling

1. The mipmapping architecture's own §3 states "one selected mip, one nearest
   texel" and "certainly does not require sampling two mip levels". The new
   ruling supersedes that sentence for terrain albedo, and its `ceil` LOD policy
   conflicts with the floor-plus-fraction a blend requires.
2. **The reference oracle's `lerp8` knowingly departs from the ratified
   rounding law, and a blend makes that matter.** This was checked rather than
   taken on report, and the first statement of it was wrong in a way worth
   recording: it was framed as `islandrearchitecture5.md` §15.1's `(x+128)>>8`
   against the oracle. That is not the conflict. §15.1 gives
   `lerp8(a,b,w) = sat_u8(a + rescale_s((b-a)*w, 8))` and defers to a SIGNED
   rescale; `(a*b+128)>>8` is `unit_mul8`, a different, unsigned operator.

   The real disagreement is with `spec/qformats.md`, which ratifies
   **round-half-up** for `unit8` and defines it at line 53 as
   `floor((n + floor(d/2))/d)`. For a product of −128 with d = 256 that is
   `floor(0) = 0`. The oracle instead rounds the MAGNITUDE and reapplies the
   sign, giving −1, and its own comment says why: *"the sign is reapplied
   afterwards so the rounding is symmetric about zero rather than biased toward
   +inf on darkening lerps."*

   So the oracle is deliberately not doing what the spec says, on exactly the
   negative half-LSB products a two-level blend generates whenever the higher
   mip is darker. Hardware built to the spec and hardware built to the oracle
   would differ by one LSB, and both could claim to be correct. **One of the two
   documents has to move**; this is an owner ruling, not an implementation
   choice, and it must be settled before any blend RTL is written rather than
   discovered as a differential failure afterwards.
3. `TEXTURE.TMU`'s text forbids this as written. A two-level nearest blend is
   not intra-level filtering, but the contract needs an explicit carve-out
   rather than an implementation that quietly disagrees with it.
