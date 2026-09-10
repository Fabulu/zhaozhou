# The owner/COMBINE resident-read seam (Commit4): what dies, what it costs, and the one fit it is owed

2026-09-10, RUN-20260910-0837-texture-readlate-commit4. Roadmap section 4.2
("T2: read-late COMBINE is the main topology change") and section 14 Commit4,
built on `zixxtrixx-v8-closeout` from a clean tree. No fit run. No commit.
Every number below says where it came from; numbers that came from reading
source are marked as such, numbers that came from the `@g2-prod` fit report
are quoted with their table, and predictions are labelled predictions.

---

## 0. The verdict, before the detail

1. **The seam is built and the island draws through it.** Tickets carry the
   14-bit owner handle only; the combiner's phase engine addresses the four
   result planes itself, one reader per plane, and the aux-or-s2 choice
   moved behind the boundary. Same recipes, same rounding sites, same outputs.
2. **What dies is mostly M10K, not registers.** The `@g2-prod` fit's RAM
   Summary shows the four payload queues `cq_s0..cq_ax_q` inferred as **four
   altsyncrams of two M10Ks each -- 8 of the island's 49 blocks** -- plus 160
   capture flops and a 28-bit owner shadow. The architect's report called
   those queues "640 MLAB bits"; the fitter disagrees, and the fitter is the
   receipt. The combiner's own payload copy narrows 110 -> 47 bits (3 M10Ks
   -> a predicted 2).
3. **Registers, measured from source: about -110 declared flops.** The
   brief's "honest net ~1,200-1,450 registers" is the architect's number for
   the WHOLE owner-residency campaign (scoreboard-to-planes ~1,000-1,150 plus
   read-late ~200-300). Applied to this seam alone it is roughly 10x too high.
   Section 4 has the split.
4. **The binding refusal is honoured, and one line of the brief is refused
   with it** (section 6): "build the epoch planes with the flag retained" is
   not a safe instruction. The retained flag guards the DUPLICATE ticket; the
   lost-ticket deadlock the architect described comes from `cmt` living in a
   RAM mirror, and it is there whether or not `rdy_q` stays. So the scoreboard
   is untouched in this commit and the epoch-plane move is named as the next
   increment with its prerequisites.
5. **"Near-equal thirds" is no longer the shape of the register breach**
   (section 5). Re-derived, not inherited.

---

## 1. What was built

One parameter on each of two blocks, one wiring change in the island, one
tie-off in the oracle island. The default (`READ_LATE=0`) elaborates the RTL
that was there before, so the two leaf suites -- v3own's 541-check adversarial
bench and the combiner's oracle differential -- run against an unchanged
netlist; the island instantiates `READ_LATE=1`.

| file | what changed | where |
|---|---|---|
| `fpga/rtl/texture/zhao_texture_v3own.sv` | `READ_LATE` parameter (:204); plane port `src_rd_valid_i/src_rd_slot_i/src_s0..aux_o` (:253); handle-only job queue (:1059); `g_legacy` / `g_readlate` generate (:1070 / :1122); tripwire `ev_src_unpub_o` (:288, :1150, :1726); three boundary assertions (:2225-2236) | header section at :139 |
| `fpga/rtl/texture/zhao_texture_material_combine_v2.sv` | `READ_LATE`, `SLOTW` (:133); `f_slot_i`, `f_has_aux_i`, `src_*_i`, `src_rd_valid_o/src_rd_slot_o` (:159-166); `PAYW` 110 or 47 (:262); `g_copy` / `g_readlate` (:427 / :453); slot file + `r_slot` (:482-491) | header section at :83 |
| `fpga/rtl/texture/zhao_texture_island_v3_top.sv` | `u_own` at `READ_LATE(1)` (:1482) with the plane port wired (:1505); `u_combine` at `READ_LATE(1)` (:3400) reading the planes (:3411-3412); the `mat_has_aux_c ? aux : s2` mux DELETED; `ev_src_unpub` is the seventh class in `cnt_fragrob_id_errors_o` and the sticky (:2984) | |
| `fpga/rtl/texture/zhao_texture_island_top.sv` (the ORACLE) | eight new combiner pins tied off at `READ_LATE=0` (:2068). **Wiring only; the netlist gate 3 compares against is unchanged.** Found by a configure-time `PINMISSING`, not by foresight. | |
| `tests/texture/tb_combine_readlate.sv` | the READ_LATE combiner behind four REAL `zhao_texture_v3bank` planes, wired as v3own wires them | new |
| `tests/texture/material_combine_readlate_diff.cpp` | V2's oracle workload through the planes + the seam's own laws | new |
| `tests/texture/texture_v3own_readlate_directed.cpp` | v3own at `READ_LATE=1`: data path, counter fired on every class, blind spot pinned | new |
| `tests/mutants/zhao_texture_material_combine_v2_slotswap_mutant.sv` | the committed break: slot file off by one | new |
| `tests/CMakeLists.txt` | three executables, one control, two `READ_LATE=1` lint tests | |

### 1.1 The seam, edge by edge

Under `READ_LATE=1` the combiner's Q stage selects a context; at the Q->R edge
it registers that context's owner slot from an 8 x 6 slot file (`r_slot`);
during R it presents `src_rd_slot_o = r_slot` with `src_rd_valid_o = r_v`; at
the R->D edge each `zhao_texture_v3bank` in v3own loads `rd_data_o <=
mem[src_rd_slot_i]` -- the same edge that loads the combiner's own `pay_rd`
from its (now 47-bit) payload row. The D->O selection therefore reads the
plane outputs exactly where it used to read the copied samples. **No pipeline
stage was added and no phase costs an extra clock**; the only latency change
in the island is that a popped ticket reaches `cmb_valid_o` two edges after
the pop instead of four, because the k1/k2 stages that existed to read and
capture the planes no longer exist.

The canonicalisation the copy path applied at admission -- count==0 puts BASE
in s0, a missing s1 or s2 falls back to s0, and (in the island) aux stands in
for s2 when the descriptor says so -- is applied at D on the plane data from
the row's `count` and `has_aux` bits (`combine_v2.sv:472-475`). Those bits were
always what decided it; they merely stop being applied to a copy.

Port pressure does not grow: `g_sres[0..2]` and `u_ares` keep exactly one
reader each. The reader relocated from v3own's prefetch pipeline into the
consumer, which is what architecture report section 2.4 predicted and what
`plane_rd_addr_c` (`v3own.sv:972`) makes literal -- a parameter select, so no
mux exists in either build.

### 1.2 Why the join is safe, stated rather than hoped

CLAUDE.md's metadata-bank law: a detector wired to two operands that move
together cannot fire, and an unconditionally registered read tracks whatever
address is offered while a stalled consumer holds the previous response. Here
the plane output register IS loaded unconditionally every edge -- and so are
`d_ctx`/`d_ph`. Both are driven from registers (`r_slot`, `r_ctx`) loaded on
the same edge from the same selected ticket, and the combiner's Q->R->D->O
pipe has no ready and never holds. There is no stall to desynchronise them.
That argument covers the timing class; the DATA-identity class (right timing,
wrong slot) is exactly what the differential and the mutant exist for
(section 7).

---

## 2. What actually dies -- measured from source, reconciled to the fit

The `@g2-prod` fit (`reports/synthesis/blockpaths/zhao_texture_island_v3_top@g2-prod.fit.rpt`,
commit 82a4f317, `rtlCleanAtHead: true`, 16,285 registers / 10,837 ALM / 49
M10K) is the receipt this is measured against. Its RAM Summary and entity table
say what the copy chain was physically.

### 2.1 The copy chain as the fitter saw it

| structure | declared (source at HEAD) | fitter (`@g2-prod`) | now (`READ_LATE=1`) |
|---|---|---|---|
| `cq_s0_q`, `cq_s1_q`, `cq_s2_q` (v3own) | 3 x 4 x 40 = 480 bits | **altsyncram, 4 x 32, 2 M10Ks EACH** (status lane swept) | do not exist |
| `cq_ax_q` (v3own) | 4 x 40 = 160 bits | **altsyncram, 4 x 24, 2 M10Ks** | does not exist |
| `sres_cap_q[3]`, `ares_cap_q` (v3own) | 160 flops | dedicated logic registers | do not exist |
| `k1_v_q`, `k2_v_q`, `k1_owner_q`, `k2_owner_q`, `cmb_rd_addr_q` | 2 + 28 + 6 = 36 flops | registers | do not exist |
| `cmb_s0_o..cmb_aux_o` bus, island `own_cmb_*` | 160 wires | routing + the 4:1 read selects on the queues | tied to zero (swept) |
| island `mat_has_aux_c ? aux : s2` | 32-bit 2:1 mux | ALUTs | deleted (the choice is 1 bit in the combiner's row, applied at D) |
| combiner `payload_m` | 8 x 110 | **altsyncram 8 x 108, 3 M10Ks** | 8 x 47 |

What is ADDED, from source: the combiner's slot file `slot_m` (8 x 6 = 48
flops, fabric by design and declared so), `r_slot` (6), `src_rd_valid_o` (a
wire off `r_v`), the 32-bit `ev_src_unpub_o` counter and its three 64:1
scoreboard selects (`live`, `cbi`, `fcl` at the read slot), and a 14-bit
`cq_push_owner_c` select that is a constant in either build.

### 2.2 The M10K arithmetic

| | `@g2-prod` | after the seam | basis |
|---|---:|---:|---|
| island total | 49 | **~40** | prediction |
| `cq_s0/s1/s2/ax_q` | 8 | 0 | the arrays no longer exist; MEASURED absence |
| combiner `payload_m` | 3 | 2 (or MLAB) | 8 x 47 needs 40 + 7 bits in the shallow 256 x 40 mode; Quartus may also choose MLAB at that depth -- PREDICTION |
| `g_sres` x3, `u_ares`, `u_fres`, `u_ctx`, `v3rq` x3 | 9 | 9 | unchanged; the planes are the residency, they were already there |
| everything else | 29 | 29 | untouched |

**No new M10K is added.** The roadmap's texture allocation is 96 M10K
(scoreboard) with 49 spent at `@g2-prod`; this seam hands back ~9 of them to
the epoch-plane move that comes next.

The fitter had already trimmed the planes' unread status lanes (`g_sres`
64 x 32, `u_ares` 64 x 24, `u_ctx` 64 x 16 -- only `ctx[15:0]` is read
downstream). Each is still one physical block; the trimming changes nothing
here but is worth knowing before anyone quotes "64 x 40".

---

## 3. Recipes, outputs and pins: preserved, and how that was checked

* The arithmetic in the combiner -- the O-stage selection, both product
  sites, F and W -- is untouched in both modes; everything downstream of the
  six D-stage names `p_s0/p_s1/p_s2/p_w/p_rc/p_rf` is mode-independent
  (`combine_v2.sv:418-425`).
* **Latency pins: none exist and none were moved.** The composed suite and the
  fault suite were searched for cycle-pinned expectations on the combine path
  and reach into no `u_own`/`u_combine` internals (they probe `u_metajoin` and
  `u_expand` only); gate 3 compares retired records positionally, not by
  cycle. The two-edge-earlier `cmb_valid_o` after a pop is therefore invisible
  to every existing check, and the paired run confirms it.
* The island's boundary ports are unchanged, so `zhao_prod_top.sv` needed no
  regeneration; `tools/quartus/check_prod_manifest.py` passes.

---

## 4. The register saving, split honestly

### MEASURED (declared bits, from the diff of the three RTL files)

| block | removed | added | net |
|---|---:|---:|---:|
| v3own | 160 (`sres_cap/ares_cap`) + 36 (k1/k2 stage, `cmb_rd_addr_q`) = 196 | 32 (`ev_src_unpub_o`) | **-164** |
| combiner | 0 | 48 (`slot_m`) + 6 (`r_slot`) = 54 | **+54** |
| island | 0 (wires and a mux) | 0 | 0 |
| **total** | | | **about -110 flops** |

### STRUCTURAL PREDICTION (what the fitter will do with that)

* The 160 capture flops and 36 stage flops were "Dedicated Logic Registers"
  in the entity table (they are not memories), so the -196 should appear
  nearly one-for-one in the island's register column; `slot_m` may be kept as
  48 flops or packed into an MLAB -- either way small.
* ALM: the four 4:1 x 40-bit read selects behind the queue altsyncrams, the
  160-bit bus routing and the island's 32-bit mux leave; the three 64:1
  scoreboard selects of the counter (about 30-40 ALUTs) and the relocated
  canonicalisation muxes arrive. Expect a modest net ALM decrease in `u_own`
  and roughly neutral `u_combine`; no number is claimed.
* Registers per ALM: the brief's ~1.9 is the machine average; the island's is
  16,285 / 10,837 = 1.50 at `@g2-prod`. Do not convert this seam's flops to
  ALMs with either ratio; it is not that kind of saving.

### UNKNOWN (only a fit can say)

* `payload_m`'s new geometry (2 M10K vs MLAB) and whether the four freed
  M10Ks per queue pair are counted as freed or re-used by the fitter for
  something it previously kept in fabric.
* Fmax. The new address path is `contq/newq head -> slot_m 8:1 -> r_slot`
  (registered) and then a flop into the bank -- shorter than the k-pipeline's
  `sel_data_c -> cmb_rd_addr_q`; the counter's selects sit off the read port
  and end in a counter. Nothing here touches the palette or RCP timing
  families the roadmap names (4.3).

### What this saving is NOT

It is not the 1,200-1,450 registers the brief quotes. That is the architect's
campaign total (report section 5: scoreboard ~1,000-1,150 net PLUS read-late
~200-300), and even the read-late slice of it counted "640 MLAB queue bits"
that the fit shows were M10Ks. **The register criterion (16,285 vs 9,000) is
untouched by construction**; the register lever is the 1,472-flop scoreboard,
which is the NEXT increment and is gated by the hazard in section 6.

---

## 5. The thirds, re-derived -- and they are not thirds any more

From the `@g2-prod` entity table directly (`Dedicated Logic Registers`
column, not inherited from the architecture report):

| entity | registers | share of 16,285 |
|---|---:|---:|
| `zhao_raster_perspuv_svc:u_persp` | 3,240 | 19.9% |
| `zhao_texture_v3own:u_own` | 3,018 (2,841 own + `v3rq` pointers) | 18.5% |
| `zhao_texture_cache_pipe:u_cache` | 2,945 | 18.1% |

So the architect's three numbers were right for that receipt. But **the
pairpipe swap landed at 8f61e083 (2026-09-09 14:32), nine hours AFTER the
`@g2-prod` pin (82a4f317, 05:13)**, and `zhao_block_fit.json` holds no
composed island row since. The composed receipt with the pairpipe does not
exist; the leaf receipts do (`perspuv_pairpipe@regfit`: 820 registers, 794
ALM, `rtlCleanAtHead: true`).

Structural prediction, leaf-for-leaf: 16,285 - 3,240 + 820 = **~13,865**
registers, of which v3own 21.8%, cache_pipe 21.2%, pairpipe 5.9%. Two blocks
now carry ~43% between them and the third is a sixth of what it was. Quote
that shape, not "56.5% in three near-equal thirds". And note what the
prediction is not: the pairpipe's leaf row was fitted with 300 virtual pins;
its composed cost will differ by some amount nobody has measured.

Three more things found while verifying, each of which would have been
inherited wrongly:

* The unlabelled `zhao_texture_island_v3_top` fit row reads **20,561**
  registers. It is a 2026-09-08 build (commit bcdfadea) with
  `MIGRATION_SHADOWS=1` -- the `g1-lab` vs `g1-prod` map rows put the
  shadows at ~4,400 registers -- and it predates `@g2-prod`. `@g2-prod`
  (16,285) is the shipping receipt; the roadmap quotes it correctly.
* The scoreboard's **"Texture 12,500 ALM" could not be reconciled to rows**:
  the tool prints no per-row bill, and 10,837 (island `@g2-prod`) + 1,633
  (`zhao_texture_cache_pipe` standalone, which is INSIDE the island) = 12,470,
  suspiciously close. If that is the composition, the domain double-counts
  the cache. UNVERIFIED -- flagged, not claimed. Worth ten minutes with
  `dsp_census.build_bill` before the 12,500 is quoted again.
* `tools/rtl/check_v3_banks.py` reports `mat_m` as "payload-shaped state in
  fabric" while the fit shows it as 2 M10Ks. Pre-existing (identical output on
  HEAD), and a known limit of a declaration-shape heuristic; recorded so
  nobody reads that line as a finding of this commit.

---

## 6. The binding refusal, and the fourteenth claim

The architect's verdict (report 4.4) is binding: the ready-claimed table is
retained. `rdy_q`, `crs_q`, `cbi_q`, the three single-writer ready queues, the
arbiter, the CMBQD reservation and every scoreboard bit are as they were.

**The brief's instruction "build the epoch planes with the flag RETAINED" is
refused, and this is the fourteenth claim.** The architect's section 4.2
hazard is not about the flag. Move `cmt` into a committed-epoch RAM mirror
and the C4 publication decision reads a row that is L cycles stale; two
publications for one owner within L each see a pre-peer state, each decides
"incomplete", and **no ticket is created -- the owner never combines and the
island deadlocks in allocation-order retirement.** `rdy_q` cannot help: it
guards a SECOND ticket, and the failure is a FIRST ticket that never comes.
The cure the architect names -- a cross-pipe, full-handle, same-owner-within-L
commit-forwarding structure feeding eligibility, plus the re-derived
`FWD_WINDOW` -- is needed for the mirror whether the flag stays or goes.
Building the planes "with the flag retained" would therefore ship the lost
ticket while looking cautious.

So this commit is the seam the roadmap's Commit4 text describes ("owner-only
acceptance and phase reads; publication and release assertions; preserve
every recipe/output") and nothing of section 3 of the architecture report.
The next increment, named with its prerequisites in the architect's own
order:

1. the cycle table snapshot -> validation -> claim -> physical write ->
   publication -> ready, for each retimed read, with `FWD_WINDOW` re-derived;
2. the cross-pipe commit-forwarding record over the last L publications per
   lane, full-handle matched, and the same-address read/write reconstruction
   rule (roadmap 8.4 premise 4);
3. an adversarial same-owner dual-publication kernel sweeping offsets 0..L in
   both orders, plus duplicate-terminal exclusion and zero-work admission
   re-proven composed;
4. a committed mutant demonstrating the double-ticket detector fires
   (`a_ticket_once_*` and a synthesizable counter beside it);
5. only then the `rdy_q`/`crs_q` deletion, worth 128 flops and four 64:1
   selects -- "architecturally pleasing and materially small".

The M10Ks this seam frees (~9) are the budget that move spends.

---

## 7. Assertions, the counter, its blind spot, and the checker seen to fail

### Added at the boundary (`v3own.sv:2225-2236`, simulation, `--assert`)

* `a_src_read_published` -- publication-before-read: a source read names an
  owner that is live, ticketed, reserved, ACCEPTED by COMBINE, and whose
  committed mask covers its required mask.
* `a_src_read_before_final` -- release-after-last-reader, first half: no read
  of an owner whose final has been claimed.
* `a_release_not_under_reader` -- second half: no read of an owner on the
  edge that frees it.

### The shipped tripwire and what it cannot see

`ev_src_unpub_o` counts reads of an owner that is not live, not
combine-accepted, or already final-claimed (`live && cbi && !fcl` at the read
slot: three 64:1 selects). It reaches the island's `cnt_fragrob_id_errors_o`
as a seventh mutually exclusive class and the sticky `err_fragrob_id_error_o`.

**Fired, by stimulus, on every class it names**
(`texture_v3own_readlate_directed`): a slot not live; a live owner before
acceptance; an owner after its final claim; an owner after its output
release. The three assertions fire on the same events (made non-fatal for
the injections, required to have fired -- independent corroboration). No
mutant is needed at THIS boundary: the combiner is outside v3own, so an
illegal read is legal stimulus here.

**Blind spot, pinned as a property, not hidden:** a read of the WRONG owner
that is itself live, accepted and unfinished moves the counter by exactly
zero -- every bit it inspects says "legal". The directed test asserts that
zero, so a future widening of the counter announces itself by failing that
line. The metadata-bank law again: a detector wired to fields the fault
leaves intact cannot fire. For that class the instrument is the differential.

### The differential, seen to fail

`material_combine_readlate_diff` drives V2's workload (corner-heavy samples,
all eight recipes, count 0..3, has_aux in both planes, back-pressure,
overtaking, output stall) through four real `v3bank` planes with the bench
playing the TMU/AUX commit, every plane a fragment may not read POISONED, and
watches `src_rd_*` so that every read names an in-flight slot.

`tests/mutants/zhao_texture_material_combine_v2_slotswap_mutant.sv` changes
one line -- the slot file stores `f_slot_i + 1` -- and
`material_combine_readlate_mutant_control` (same driver, same wrapper,
inverse polarity) **passes when the oracle differential reports mismatches
AND the seam watch reports reads of unowned slots.** Every counter the
mutant exposes balances -- phases, jobs by recipe, fragments retired -- which
is the whole reason a differential rather than a counter is the witness.

---

## 8. What was run, and what it said

Built through `cmake --preset windows-native` (configure RC 0) and
`cmake --build` (RC 0, binaries stamped 08:42-08:44 -- checked, per the
stale-binary law, before any of them was run). The five pre-existing suites'
SOURCES are untouched (`git status`), so their check counts are HEAD's by
construction; they were run against the modified RTL.

| test | source state | result |
|---|---|---|
| `island_v3_composed_directed` (gate 2, `MIGRATION_SHADOWS=1`) | unchanged | **133/133 passed** |
| `island_v3_prod_composed_directed` (`MIGRATION_SHADOWS=0`) | unchanged | **127/127 passed** |
| `island_v3_fault_directed` | unchanged | **31/31 passed** (incl. `cnt_fragrob_id_errors_o == 0`, now seven classes) |
| `island_v3_paired` (gate 3, `tools/texture/gate3_paired.py`) | unchanged | **392 retired records byte-identical in rgb/a/tag/refused AND order** against the oracle island |
| `material_combine_v2_diff` (the copy path, `READ_LATE=0`) | unchanged | **27/27 passed** |
| `texture_v3own_adversarial` (`READ_LATE=0`, `--assert`) | unchanged | **541/541 passed** |
| `material_combine_readlate_diff` (NEW; the design test) | new | **33/33 passed** -- 2,800 / 492 / 560 / 210 plane reads across the four batches, **0 seam violations, 0 reuse faults**; phase batch read the planes exactly once per phase (560 = 560) |
| `material_combine_readlate_mutant_control` (NEW; inverse polarity) | new | **passes: 1,598 of 1,600 fragments mismatched the oracle, 2,600 of 2,800 reads named an unowned slot** -- with every fragment retired and every count balanced. The checker is seen to fail. |
| `texture_v3own_readlate_directed` (NEW; `-GREAD_LATE=1 --assert`) | new | **30/30 passed** -- committed result40 read back bit-exact through the plane port; legacy lanes zero; `ev_src_unpub_o` 0 -> 4 across the four injected classes; `a_src_read_published` and `a_src_read_before_final` each fired twice on those injections; the blind spot read moved it by zero |
| `lint_texture_v3own_readlate`, `lint_texture_material_combine_v2_readlate` (NEW, ctest) | new | passed |

Registered in ctest as #94, #95, #234, #236, #237.

Gates that are not tests:

| gate | result |
|---|---|
| `verilator --lint-only -Wall` v3own, `READ_LATE=0` and `=1` | 0 diagnostics each |
| `verilator --lint-only -Wall` combine_v2, `READ_LATE=0` and `=1` | 0 diagnostics each |
| `verilator --lint-only` island_v3_top (24-file closure) and island_top (oracle) | 0 diagnostics each; `-Wall` on the V3 island adds nothing seam-related beyond the unread status bytes, waived with a reason |
| `tools/quartus/check_quartus17_syntax.py` | 223 files, no Quartus-17-rejected forms (explicit `generate`/`endgenerate` used throughout) |
| `tools/quartus/check_prod_manifest.py` | OK -- no island port changed, so `zhao_prod_top.sv` is not stale |
| `tools/rtl/check_v3_banks.py` | identical findings on HEAD and on the working tree (four pre-existing `V3-FABRIC` on island arrays, three pre-existing marker warnings); **nothing new** |

---

## 9. The ONE fit gate, its question, and its limits

**Gate:** `zhao_texture_island_v3_top@readlate` -- the composed island,
`MIGRATION_SHADOWS=0`, the SAME rules as `@g2-prod`, from a clean tree after
review and commit. Not before: the tree is dirty by design until the owner
commits, and a dirty receipt "describes nothing" (CLAUDE.md, the
`rtlCleanAtHead` law).

**The question it answers:** does the composed island lose the eight
`cq_*` M10Ks and at least one of `payload_m`'s three (49 -> <=40), lose on the
order of 110-200 registers with `u_own` and `u_combine` moving as section 4
predicts, and hold Fmax at or above 82.05 MHz -- read from the ENTITY TABLE
and the RAM SUMMARY, not from the totals?

**What it can settle:** M10K count and geometry, the register and ALM delta
of the seam, whether `slot_m`/`payload_m` inferred as predicted, Fmax.

**What it cannot settle, and must not be read as settling:**

* correctness -- Verilator settled that in seconds (section 8), and the fit
  would report an ALM count for a wrong netlist just as happily;
* the register criterion -- 16,285 vs 9,000 stays breached; this seam was
  never going to move it;
* the seam's delta IN ISOLATION -- the receipt will also carry the pairpipe
  swap and the rcp24_v3 swap, neither of which has a composed receipt yet.
  Attribute per entity, never by differencing totals against `@g2-prod`;
* the epoch-plane move -- a different commit, its own gate.

---

## 10. Not verified, with the instrument named per item

| claim | status | what would verify it |
|---|---|---|
| island M10K 49 -> ~40 | PREDICTION from the RAM Summary's own inference of the deleted arrays | the fit gate above, RAM Summary |
| `payload_m` 3 -> 2 M10K | PREDICTION (8 x 47 geometry) | the fit gate, RAM Summary |
| ~-110 fitted registers, `u_own` -164 / `u_combine` +54 | MEASURED as declared bits; fitted mapping is a PREDICTION | the fit gate, entity table |
| ALM direction (modest decrease in `u_own`) | PREDICTION, no number | the fit gate, entity table |
| Fmax not worse | ARGUMENT from path shape only | the fit gate, `sta.rpt` |
| composed register shares 21.8 / 21.2 / 5.9% | PREDICTION from leaf rows | a composed fit with the pairpipe (the same gate) |
| "Texture 12,500" double-counts cache_pipe | SUSPECTED, not checked | `dsp_census.build_bill` row listing |
| Quartus-17 synthesizability of the new generate blocks | LINT + the syntax checker only -- "a block that has never been through `quartus_map` has not been shown to be synthesizable" | `quartus_map` (33 s, loud) as the first step of the gate |
| the seam under the full-frame workload | the composed suites' workloads only | `tools/texture/gate3_paired.py` is the composed instrument; nothing longer exists |

---

## 11. Process notes for the reviewer

* The tree is left uncommitted as instructed. Five RTL/CMake files modified,
  five test files added; `git diff --stat` in the run's TASK_LOG.
* The oracle island (`zhao_texture_island_top.sv`) had to be touched -- eight
  tie-offs -- because it shares the combiner. Its netlist is unchanged
  (`READ_LATE=0`) and gate 3 is the proof; but it is the golden, so it is
  listed first here rather than buried.
* `zhao_texture_material_combine_v2`'s -Wall lint has a waiver block around
  the two mode-exclusive port groups (`:150-165`) with the reason beside it;
  exactly one group is unused in any elaboration.
* Both new mutant-facing tests are inverse-polarity controls; neither is a
  test of the design. `material_combine_readlate_diff` (normal polarity) is
  the design test.
* Instruments found wanting this session, for the record: a heredoc through
  the shell wrapper turns `\r\n` escapes into literal line breaks (a patch
  script was silently mangled twice before it was noticed); and the combiner
  file is CRLF while its two neighbours are LF, which made an exact-match
  patch report "anchor not found" on text that was visibly there.
