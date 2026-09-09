# tmu_pipe's palette moves to M10K, and the feared throughput trade does not exist

2026-09-09. Follow-up to `reports/TMU-PIPE-PALETTE-IN-FLOPS-20260904.md` (D19m),
whose fit finished five days ago and whose repair was never applied — the
`max_registers: 12000` tripwire landed in `design/fit_targets.yml` and the
storage it watches for stayed in flip-flops. This pass applies the repair.

**What changed:** `pal_dat_r` in `fpga/rtl/texture/zhao_texture_tmu_pipe.sv` is
now ONE FLAT synchronous-read RAM, `(* ramstyle = "M10K" *) logic [15:0]
pal_dat_r [4096]` indexed by `{way, index}`, written at one site, read through
the M10K's own registered address, with `decode16` moved to the cycle after the
response. `pal_tag_r`, `pal_ten_r` and `pal_val_r` (4,608 bits) stay in flops
**deliberately** — they are the associative half of the lookup, compared or
cleared on all sixteen ways in one cycle, which no memory can do. That split is
the 09-04 report's design, kept.

**The headline:** the 09-04 report ended on *"whether 65,536 registers or CLUT
throughput matters more is an owner call."* It is not an owner call. The added
cycle is LATENCY, absorbed by the ROB, and never initiation interval — measured
in clocks, before and after, on a cache model with the real TEXTURE.CACHE's
one-access-per-clock shape. The resident CLUT path holds **II = 1** with the
registered read in place. Details in "The cycle absorption" below.

---

## Why it did not infer, argued from the RTL (not from the ranker)

The brief was right to demand this: `check_ram_inference.py`'s own output calls
one of its rules *"WEAK SIGNAL, measured false positives"*, and its 09-04
CORRECTION found two flagged blocks already inferring. So, from the read/write
shape itself.

To emulate the old combinational read, Quartus needs somewhere to put the
M10K's **mandatory registered read address**. There are exactly two shapes it
can absorb, and this tree contains a measured example of each:

1. **The read address is itself a register** with nothing combinational in
   front of the array — `zhao_audio_fifo`'s `assign rd_word = mem[rd_ptr]`,
   `rd_ptr` a flop. The M10K's internal address register becomes a copy of
   `rd_ptr`. Measured: 65,536 bits, **7 M10K, 292 registers**.
2. **The read data feeds a register with only wiring between** — the
   `q <= mem[addr_comb]` template, `zhao_texture_v3bank` verbatim. The
   consumer flop becomes the RAM's synchronous read.

The old `dec_clut565_c = decode16(pal_dat_r[pal_way_c][rsp_idx], FMT_RGB565)`
had **neither**. Its address `{pal_way_c, rsp_idx}` is combinational — a 16-way
tag compare and a byte extracted from `cac_data_i`, both born in the response
cycle — so no register exists for shape 1; and the read fed `decode16`, a mux
and only then a flop, so shape 2 is blocked too. The 09-04 report's
self-correction named only the second half ("the DECODE between the array and
the flop"); the address half is equally blocking, and it is the half that
decides where the fix must go: the address cannot be produced earlier (see
absorption below), so the read must take the response edge and the decode must
move, exactly as that report proposed.

The multidimensional shape (`[PAL_SLOTS][256]`, the ranker's headline reason)
is real but was not sufficient on its own — fragrob's multidimensional arrays
inferred 13 M10K. Flattening is still done, because large + multidimensional +
indexed-on-both-axes is the combination Quartus reports "cannot regroup" on.

The 09-04 report also worried about the two write sites
(`[pal_way_c][...]` / `[pal_vic_r][...]`). They were mutually exclusive
branches of one `if`; they are now literally one site with a muxed address
(`pal_waddr_c`), which also retires the ranker's "two or more distinct write
addresses" finding.

**After the change** `check_ram_inference.py --rank` no longer lists
`zhao_texture_tmu_pipe.pal_dat_r` at all (the remaining `pal_dat_r` row is the
SERIAL block's 8,192-bit two-slot copy, which ships at 36 MHz and is not this
increment's). Per the standing law, that is a static checker's opinion and NOT
proof of inference — the proof is the fit gate named at the end.

## The new shape

    localparam int unsigned PAL_PAGE_ENTRIES = 256;            // 8-bit CLUT index
    localparam int unsigned PAL_ENTRIES = PAL_PAGE_ENTRIES << PW;  // 4,096 at 16 slots
    (* ramstyle = "M10K" *) logic [15:0] pal_dat_r [PAL_ENTRIES];

    always_comb begin
      pal_raddr_c = {pal_way_c, rsp_idx};
      pal_we_c    = rsp_take_c && rsp_is_pal;
      pal_waddr_c = {pal_hit_c ? pal_way_c : pal_vic_r, rb_idx[rsp_rec]};
    end
    always_ff @(posedge clk) begin                 // CLOCK-ONLY: an array touched
      if (pal_we_c) pal_dat_r[pal_waddr_c] <= cac_data_i[15:0];   // by an async-reset
      pal_rd_q <= pal_dat_r[pal_raddr_c];                          // process cannot
    end                                                            // become an M10K

* Depth is `1 << PW` pages rather than `PAL_SLOTS`, so `{way, index}` is a
  plain concatenation for every legal `PAL_SLOTS`; a non-power-of-two count
  pads unreachable rows rather than aliasing reachable ones (no elaboration
  guard needed, hence no mutant needed for one).
* The RAM was never reset before (only `ten`/`tag`/`val` were) and is not now;
  `pal_ent_c` (the per-entry valid bit, in flops) guards every read of a
  never-written row, unchanged.
* Read-during-write: OLD DATA, and it is never used. One response per cycle
  means a fill (the only write) and a resident hit (the only meaningful read)
  cannot be the same cycle, so a colliding read's value is dead — `pq_v` is 0
  after every fill cycle. Same declared law as `zhao_texture_v3bank` §6.4.
* The completion stage (`pq_v`, `pq_rec`, `pal_rd_q`) loads UNCONDITIONALLY,
  which is the 2026-09-08 metadata-swap trap's shape — safe here because the
  stage has no backpressure (a ROB write cannot stall), so all three advance in
  lockstep and no held consumer can watch a moving address. The comment in the
  RTL says so, and says what to do if a stall is ever added.
* One cycle later, `dec_clut565_c = decode16(pal_rd_q, FMT_RGB565)` completes
  the record. `decode16` itself is UNTOUCHED — same function, same input word,
  bit-identical output, and the shared oracle suite is what says so.

## The M10K arithmetic, shown

    65,536 bits needed;  one M10K = 10,240 bits  ->  ceil = 7 blocks minimum

But block COUNT depends on the configuration Quartus picks for a 4,096 x 16
logical RAM. M10K native depth tops out at 4K x 2:

    width-sliced:  16 bits / 2 per block at 4K deep  =  8 M10K, no soft logic
    depth-decoded: 4 x (1K x 10) + address decode + output mux = 7 M10K + LEs

`zhao_audio_fifo`'s 2,048 x 32 measured 7 (= ceil(32/5) at 2K x 5), i.e.
Quartus width-sliced it at native depth. The honest prediction here is
therefore **8 M10K of 553 (1.4%), with 7 possible** — the 09-04 report's flat
"7" was the bit-count division, not a configuration. Either answer is noise
against 553, and either answer removes ~65,536 flip-flops.

    registers   72,824  ->  ~7,300 expected     (STRUCTURAL PREDICTION)
    M10K             2  ->  ~9-10 total          (7-8 palette + existing 2)
    ALMs        18,206-38,300 for this block -> a normal block's share

## The cycle absorption — why this is a free win, not a trade

The 09-04 report deferred the fix as an owner call: *"The CLUT path is the
palette path ... already at 0.65x of demand. Adding a cycle to it without
absorbing that cycle elsewhere makes a known shortfall worse."* Three things
about that sentence turned out wrong on inspection, and they are the
centrepiece of this pass.

**1. The read is not on any feedback loop, so registering it cannot change the
initiation interval — only the latency.** An II is set by loops: request
acceptance (ROB occupancy), the cold-fetch round trip, the filter lane's
port-hold. Walk the cone of `pal_rd_q`: it feeds `decode16` -> `rb_rgb`/
`rb_done` -> retirement. Nothing on the palette read path feeds `req_ready_o`,
`cac_ready_o`, or the issue register within the same iteration — the only
feedback is through ROB occupancy, where +1 cycle of occupancy per CLUT record
is absorbed by `ROB_N = 16` against a steady-state occupancy of ~7. The
hit/miss DECISION (which does feed `pf_v`, a loop) reads only the flop planes
(`pal_ten_r`/`pal_tag_r`/`pal_val_r`) and is untouched.

**2. The 0.65x figure was never measuring this read.** It is
`1,666,667 / 3` from `test_throughput_against_the_derived_demand`, whose cache
model is SINGLE-OUTSTANDING: one access accepted, one response, then ready
again. That floors any measured interval at the round trip — the number
measures the harness's cache, not the TMU's palette. Proof: the assertion is an
exact equality, and it reads **3 before and 3 after** this change. The palette
stage is invisible in it in both directions.

**3. The absorption the report hoped to get from "the II = 2 work" already
exists — this block IS that restructure.** The II = 2 design in
`REMAINING_BLOCKERS.md` (in-flight records, issue arbiter, in-order completion)
was written for the serial FSM. `zhao_texture_tmu_pipe` is the built version of
that idea with 16 records, and against a cache of the real TEXTURE.CACHE's
shape it does not need a donated slot — there is no shortfall to donate to.

**The measurement.** `texture_tmu_dev.hpp` grew a PIPELINED cache model
(`cac_pipe`): one access accepted per clock, several outstanding, responses in
acceptance order — the real `zhao_texture_cache` port shape (`acc_ready_o`
accepts one per clock through a 1-deep response pipeline). The new directed
case `test_resident_clut_ii_on_a_pipelined_cache` streams 64 resident CLUT8
samples:

                                  before (flops)   after (M10K read)
    64 resident CLUT samples     68 cycles         69 cycles
      = II                       1                 1        (drain 4 -> 5)
    warm pass (64 cold fills)    134 cycles        134 cycles (fills complete
                                                    from the response itself)
    single-outstanding CLUT II   3                 3        (existing equality)
    single-outstanding direct II 4                 4        (existing equality)
    worst accept-to-retire       18                18       (bound 24; the +1
                                                    hides under patterns where
                                                    retire waits on smp_ready)
    shared oracle suite          79 + 3 new checks: 82/82 pass

The one cycle appears in the DRAIN, once per batch, and nowhere per sample.
That pair of numbers — 68 and 69 — is the absorption argument in its entirety.

**Against the contract's declared demand:** terrain is CLUT8 and the docket's
demand is 850,000 samples in 1,666,667 compute clocks = 1.96 clocks/sample.
Resident CLUT at II = 1 is 1,666,667 samples/frame of capacity, **1.96x the
demand**, with the registered palette read in the pipeline. Cold fills are
bounded by 4,096 entries per full invalidate (16 pages x 256), each a
handful of clocks — under 1% of the frame in the worst palette-thrash case.
(These capacity figures assume the composed cache sustains its port rate; that
composition is measured in TEXTURE.CACHE's own lanes, not here.)

**The options the brief asked to be priced, priced:**

* *Pipeline the tag search / know the way earlier.* The way IS the early
  operand (`rb_pal[rsp_rec]` is known before the response; `rsp_rec` is the
  tag-FIFO head). It does not help: the RAM address also needs `rsp_idx`, the
  responding texel byte itself. The late operand is the INDEX, and no
  restructuring of the way search moves it earlier. (Precomputing the way
  would also have to re-check residency at response time — a fill can
  reallocate a way in between.)
* *Speculative read of all ways, way selects the RESULT.* Dead for the same
  reason: speculation across ways doesn't touch the index, which is what's
  late. It would also cost 16 separate 256 x 16 RAMs (one M10K each = 16
  M10K vs 7-8) to buy nothing.
* *Donate a slot from the FILT_LANES frontier or the direct path.* Moot — a
  palette is never filtered (`filter_eff = m_filter && !is_clut`), so
  FILT_LANES cannot touch CLUT in either direction, and no slot is needed.

## The instrument was seen to fire

* The new II equality was first committed with a deliberately wrong constant
  and FAILED (expected 72, measured 68) before being pinned to the measured
  value — the equality bites.
* A ONE-BIT corruption of the RAM read address (`rsp_idx ^ 8'h01`) was built
  and run: **18 of 82 checks fail**, loudly, across formats, CLUT-index and
  resident-palette cases. Restored: 82/82. (No new counter was added, so no
  committed mutant is owed; the corrupted build is the positive control for
  the bit-exactness checker, per the break-it-on-purpose law.)
* During the restore, `Copy-Item` preserved the scratch copy's OLD mtime, ninja
  declared the verilated model current, and the "restored" run still failed
  18/82 — the stale-binary trap, caught by the number that should have moved
  and didn't. Touched the file, rebuilt, 82/82.

## Regression sweep

    test_texture_tmu_pipe        82/82   (was 79; +3 new pipelined-cache checks)
    test_texture_tmu_directed    79/79   (serial block, shared harness edited)
    test_texture_tmu_lanes4      79/79
    test_texture_tmu_lanes1      79/79
    test_texture_tmu_random       8/8
    test_texture_tmu_plan_directed 8/8   (elaborates the edited tmu_pipe.sv)
    verilator -Wall              warning set BYTE-IDENTICAL to HEAD (6 inherited
                                 UNUSEDSIGNAL warnings, 0 new)
    check_quartus17_syntax.py    clean (217 files; self-test 3 fire / 6 no-fire)
    no_control_bytes.py          clean on all three edited files
    dsp_census                   no tmu_pipe change (no multiplier touched)

## Found while doing this, including where the brief was wrong

1. **"NOT INSTANTIATED ANYWHERE" was stale twice over.** The brief inherited
   the file's own banner; in fact `zhao_prod_top.sv` instantiates the block as
   `u63_i` (the generated LFSR-fed RESOURCE top), so the file IS inside the
   production fit's source closure. No fit was running (verified by process
   list), so editing was safe — but the next person should know the closure
   membership. The banner is corrected, as is its "INCOMPLETE" headline: the
   five "WHAT IS MISSING" items all landed by 2026-09-02 (`a02808c0`, 79/79)
   and the header was never updated.
2. **The 09-04 report's "owner call" framing conflated latency with initiation
   interval**, and its 0.65x evidence measures the harness's single-outstanding
   cache model, not the palette path. The trade it deferred does not exist.
3. **A latent wedge in the cold-fallback path, PRE-EXISTING and untouched by
   this change:** `pf_v` is a single fallback record, and a second MISS
   response consumed while the first fallback has not yet issued overwrites
   it — the first record then never completes and the ROB head waits forever.
   Reaching it needs two outstanding texel misses plus an issue-port stall in
   the same window: impossible against the single-outstanding harness model
   (why 79/79 never saw it), possible against the real TEXTURE.CACHE, whose
   response pipeline lets responses arrive while accepts stall. The obvious
   fix — hold the response while `pf_v` is pending — DEADLOCKS against the
   real cache, whose `acc_ready_o` refuses accepts while its response is held,
   so the fallback could never issue. The right fix is a small fallback queue
   (worst case ROB_N entries), and it is NOT built here: it is invisible to
   every current test, orthogonal to the palette storage question, and
   deserves its own directed reproduction first. Recorded as a KNOWN LIMIT in
   `texture_tmu_dev.hpp`'s `cac_pipe` doc; until it is fixed, drive `cac_pipe`
   with `cac_stall = 0` or a warm palette.
4. `design/fit_targets.yml`'s D19m comment says `decode16()` "is what blocks
   inference today, not the array's shape" — half right; the unregistered
   ADDRESS was equally blocking (section above). The rule itself
   (`max_registers: 12000`) is correct and should now pass.

## MEASURED / STRUCTURAL PREDICTION / UNKNOWN

**MEASURED** (Verilator, this pass): every number in the absorption table;
82/82 + full regression sweep; lint parity; the 18/82 fire; ranker no longer
flagging the array.

**STRUCTURAL PREDICTION** (argued, not yet fitted): pal_dat_r infers as block
RAM (canonical v3bank template, clock-only, single write site, sync read,
`ramstyle` attribute with a fitted precedent); registers 72,824 -> ~7,300;
7-8 M10K for the palette; response-cycle timing no worse (the old path
CAM -> 4,096:1 mux -> decode16 -> mux -> flop became CAM/byte-extract -> M10K
address register); placement/routing time falls toward cache_pipe's (the 09-04
report measured 7x placement cost tracking the register count).

**UNKNOWN** until the fit gate: the actual register/M10K/ALM/Fmax numbers; and
whether Quartus width-slices (8) or depth-decodes (7) the RAM.

## Implementation order, with the ONE fit gate

1. DONE this pass: RTL reshape + registered read; harness pipelined-cache
   model; directed II case with the equality pinned to the measured value;
   positive control fired; full regression green. Files:
   `fpga/rtl/texture/zhao_texture_tmu_pipe.sv`,
   `tests/texture/texture_tmu_dev.hpp`,
   `tests/texture/texture_tmu_directed.cpp`. No ports changed, so
   `zhao_prod_top.sv` needs no regeneration.
2. Owner review + commit (per the brief, nothing committed by this pass).
3. **THE FIT GATE — one per-block fit of `zhao_texture_tmu_pipe`** (closure per
   `design/fit_targets.yml`: this file + `zhao_texture_bilerp.sv`), batched
   with the next texture-subsystem fit rather than run alone. The question it
   answers, stated in advance: *"Did the palette leave the flops — Total
   registers <= 12,000 (the D19m rule), block memory bits >= 65,536, M10K in
   the 7-10 range — and what Fmax does the shortened response path achieve?"*
   The A&S numbers land in ~10 minutes and answer the storage half before
   placement finishes; the existing `max_registers: 12000` rule turns the
   answer into a standing gate. If registers come back near 70k, the
   `ramstyle` attribute is the first suspect and `quartus_map`'s inference
   report (not the ranker) is the instrument.
4. Separately, as its own increment: reproduce and fix the `pf_v` overwrite
   wedge (finding 3).

## Not verified, and by which instrument each would be

* **RAM inference actually happening** — `quartus_map` / the fit gate. A clean
  ranker, a clean lint and a canonical template are three opinions, not a
  netlist; the standing law says a block never through `quartus_map` has not
  been shown synthesizable. (Not run: owner's no-fit instruction; the gate is
  named above.)
* **Register/ALM/M10K/Fmax deltas** — the same fit's `.fit.summary` +
  `worst_path_index.json` for the gating path.
* **The wedge reproduction** (finding 3) — a directed case driving `cac_pipe`
  with a stall seed on a cold CLUT batch; expected to hang the harness at its
  cycle limit. Not run, because until the fix exists it would be a test that
  asserts the bug (forbidden by the 09-08 law); write it WITH the fix.
* **Behaviour against the real composed TEXTURE.CACHE** — no composition block
  exists (ledger registers four TEXTURE blocks, none is "the composition");
  the pipelined model matches the cache's documented port shape but is still a
  model.
* **The 1.96x capacity claim at the console level** — depends on the composed
  cache sustaining its port rate under real address streams; that is
  TEXTURE.CACHE's contract's number, not established here.
