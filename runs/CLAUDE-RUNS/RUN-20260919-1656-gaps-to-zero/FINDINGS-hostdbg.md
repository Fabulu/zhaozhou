# FINDINGS — HOST/DEBUG lane (gz/hostdbg)

Register **33 → 31**. Two entries CLOSED AND DELETED, two refused with evidence,
one owner decision, three false claims corrected (one of them in my own brief),
five instrument defects.

---

## Gaps closed

### I19 — MEASURE.HISTOGRAM's host read window, and I45 — DEBUG.TRACE's arming and readout

Closed together, in one pass, because I45's own text asked for exactly that:
*"the same sentence I19 writes about MEASURE.HISTOGRAM applies unchanged …
Closing one closes both, and they should be closed together rather than twice."*

**The premise was TRUE and it was a missing CARRIER, not a missing block.**
"The host is the HPS and no register path from HPS to this block exists" was
correct. `zhao_hps_bridge` is a 64-byte **burst** engine on the h2f DATA bridge
(`spec/memory_rules.md` section 3) and a 24-bit histogram bin is not in DRAM for
it to fetch.

**What I searched before building** (the instruction that matters most):

* `fpga/rtl` for `csr`, `regwin`, `lwh2f`, `h2f_lw`, `lightweight bridge` —
  zero hits outside ruling R51's own text;
* every `.sv` whose *name* contains hps, csr, bridge, reg, host, dbg or debug —
  nine files: `zhao_hps_bridge`, `zhao_hps_arbiter`, `zhao_part_hps`,
  `zhao_field_host`, `zhao_video_ready_bridge_v2` and the four
  `fpga/rtl/debug/` blocks;
* `spec/memory_rules.md`'s twenty-six section headings.

Nothing in the tree was a register aperture. **`zhao_debug_counters` is the
nearest thing and is not one** — it *streams* `(counter_id, u64)` pairs in
ascending catalog order at vblank (`spec/counters.md`), a different protocol
serving a different law. Bending it into an address-mapped window would have
been a second opinion about what a host read is.

**The carrier (R51).** `fpga/rtl/debug/zhao_host_regwin.sv`, on the Cyclone V's
HPS **lightweight** bridge — the part's second host port, the one whose purpose
is register access. It is a physical edge in the same class as `hps_req_*` and
`pad_buttons_i`, and in Verilator the harness is the HPS exactly as it is for
those. Map frozen in `spec/memory_rules.md` **section 8**.

**No-escape is STRUCTURAL, not a bound check.** tenant = `addr[15:12]`, word
offset = `addr[11:2]`, and the offset wire handed to a tenant is **ten bits
wide** — there is no wire on which one tenant could be given another's address.
MEM.GUARD compares against bounds because *its* regions are runtime
configuration; these are frozen at elaboration, so the stronger form is
available and is taken. The escape is not refused, it is unrepresentable.

**Every refusal is ANSWERED and COUNTED; it never hangs** (R20's law):
misaligned, unmapped region, the tenant's own refusal, and a tenant that does
not answer within 255 cycles.

**The two tenants.** `zhao_host_reg_hist` (tenant 0) drives the histogram's read
port — **the address IS the bin**, so there is no select register to race
against a live accumulator; a select/data pair is the metadata-swap defect of
CLAUDE.md's own chapter rebuilt on purpose. `zhao_host_reg_trace` (tenant 1)
answers the ring, word-addressed as `{event, word}`, plus armed/count/dropped.

**The arming (R52) deliberately did NOT go to the aperture.** `DebugTraceArm`
0xF003 is ratified in the reserved debug range and lowered by CMD.EXEC, because
R18's principle — one authority per level — forbids a ring with two writers and
no ordering between them. The aperture READS `armed` and is refused a write, so
a capture is always attributable to the packet that asked for it.

* Applied at the record's **last byte**, during the walk, *not* after the
  packet's verdict like every other command CMD.EXEC executes. The trace
  surface is already speculative by CMD.DECODER's own contract (it reports "a
  record that may still be rejected"), and a ring that could not observe a
  packet that FAILS would be blind to the one packet anybody is debugging.
* **The ordering is exact**: CMD.DECODER offers a record to the ring at the
  record's byte 15, so the arming record itself is not traced and every record
  after it in the packet is. The next header cannot complete for sixteen more
  byte-cycles.
* **REFUSE, NEVER MASK**: a reserved bit in `stage_mask` or `flags` refuses the
  record whole and is counted. Masking it off would arm a different set of
  stages than the host asked for.
* `zref::trace::apply_arm_command` is the ratified lowering, so the RTL has a
  reference to be differenced against rather than a convention to re-derive.

**The evidence, measured.** `run_console_core_smoke.ps1` PASSES with
`raster pixels=2560` and `frames_admitted=1` unchanged:

```
SMOKE: trace     armed=0000001 stored=9 dropped=0 against decoder records=11
                 arms=1 arm_refused=0
SMOKE: hostreg   reads=9 writes=1 unmapped=1 misaligned=1 tenant=2 timeout=0
                 stall=31 snapshots=3
```

The bench reads a trace ring **word** and a histogram register back **through
the aperture** — not off a core output port, which would prove the block and
not the carrier — and fires the unmapped, misaligned, no-such-register and
read-only-write refusals in the running console.

`stored = records - 2` because BeginFrame *and* the DebugTraceArm record are
both offered to the ring before the arm lands. `TRACE_SKIP_C` is derived from
where the record sits, not fitted to the answer. It was minus ONE on first
writing, which forgot BeginFrame, and the check caught it — which is the check
working.

`tests/debug/host_regwin_directed.cpp` (**57 checks**) is the aperture alone
against tenant *models*, for the three things a live console hides: no-escape
MEASURED on the wire rather than inferred from a plausible answer;
`refused_timeout_o` **fired** on both its paths (a tenant that never
acknowledges, and one that acknowledges then goes quiet); and reads/writes
counted as ASKED rather than as succeeded.

**No mutant is owed for the timeout.** `t_ack_i` is an INPUT, so holding it low
is perfectly legal stimulus at the block's port. This is the `wq_overflow_o`
shape without the `wq_overflow_o` problem, and the distinction is worth keeping:
a mutant is for a state no *legal input* can reach, not for one no *composed
system* happens to produce.

New smoke control **`-BadTraceArm`**, DIRECT polarity: the record sets an
unassigned `stage_mask` bit, is refused whole, and nothing is armed.

---

## Gaps refused, with the exact blocker

### I18 — MEASURE.HISTOGRAM's event ingress. REFUSED, and its stated cause is WRONG.

**The entry's reason does not describe this block.** I18 reads: *"the block
measures a difference against a reference and the console has no reference."*
`zhao_measure_histogram`'s event port is `ev_err_i`, `LANES*EW` bits — **one
unsigned magnitude per lane**. It takes no expected, no actual and no
difference. The block's own header is explicit: *"It takes an unsigned
magnitude of EW bits and says nothing about what it measures."*

That sentence is DEBUG.TRACE's (`ev_expected_fx_i` beside `ev_actual_fx_i`), and
I18 is the entry that *records* the trace refusal having reasoned from the wrong
port — so the reasoning it preserved as a cautionary tale is the reasoning it is
now using itself. **My own brief repeated it**, which is how it kept travelling.

**A real producer of the right shape EXISTS, is built, and is tested.**
`fpga/rtl/terrain/zhao_terrain_loddev.sv` emits `dev1_o`/`dev2_o`/`dev3_o` —
three **24-bit unsigned deviation magnitudes** per record, with
`dev_valid_o`/`dev_ready_i` and a `dev_src_id_o`. That is `hist_ev_*`'s shape
field for field. Its wrapper `zhao_terrain_lodfeed` is tested by
`terrain_lodpath_directed` (286 checks, four counters fired).

**And its input producer is ALREADY COMPOSED.** `zhao_terrain_lodfeed`'s inputs
name TERRAIN.MIPFEED's ports in its own comments (`mg_start_o`, `mg_job_slot_o`,
`mg_fine_valid && mg_fine_ready`, `mg_fine_h`), and MIPFEED is composed in
`zhao_console_core` — the stream is live at `tmg_fine_valid/ready/h`. lodfeed
**observes** it, so it steals nothing, needs no arbitration and adds no fork
hazard.

**The recorded blocker belongs to the CONSUMER, not the producer.**
`design/console_inventory.yml` says lodfeed and devstore are *"Blocked with the
store above on the same missing camera-position producer."* That is TRUE for
TERRAIN.LOD, which selects a level in **screen** space and needs the eye (D1 /
ruling R63). It is NOT true of the deviation: a **world-space** deviation needs
no camera, and **neither `zhao_terrain_loddev` nor `zhao_terrain_lodfeed` has a
camera or eye PORT** — grepped for `cam`, `eye`, `view` across both files; every
hit is prose. `zhao_terrain_lod` does (`cam0_x_i`, `cam0_y_i`, …), and
`zhao_terrain_lodfeed.sv:124` says so itself: *"`zhao_terrain_lod` measures its
camera"*. The consumer's blocker was applied wholesale to the producer — the
same shape as the fourteen false absences, one level over, and the producer's
own comment names the right owner.

**So why is this still REFUSED?** Because the gap is not "nothing produces an
error magnitude". It is **"the metric is unratified"**, and that is an owner
decision — see below. Wiring a world-space terrain page deviation into the organ
the ARM reads as *pixel error per camera* would close the register entry and put
the wrong quantity in it, and **nothing would catch it**, because the block is
metric-agnostic by design. That is the histogram contract's own named failure
mode ("two blocks disagreeing about one policy"), manufactured deliberately.

**A gap closed by a producer that is real but WRONG is worse than an open gap.**

*Not verified*: whether the smoke's current stimulus drives MIPFEED's fine
stream at all (its three pages fault on CRC and the smoke prints no mip
counter). Anyone acting on the recommendation must check that before quoting a
traverse.

### I20 — zhao_shell_top_v2's remaining provisional ports. REFUSED; my carrier makes none of it real.

Three OPEN ports remain: `tri_flat_request_i` (298b), `tri_continuation_tail_i`
(48b), `tri_fragment_state_i` (32b). **Exact blocker: MATERIAL.RESOLVE's request
issue point and its response join — entry I49, the texture lane's.** The block
is built and composed; what is missing is a request issued per meshlet and its
answer joined back to the triangles that asked. The host register window has
nothing to do with any of it.

**One candidate closure found and deliberately NOT taken: `fb_writer_i`.** The
entry says the value it will become is VIDEO.SLOTMGR's `lease_writer_o` and that
"making that substitution is CMD.SCHEDULER's act, not this composer's". I
checked whether that reason still holds. `lease_writer_q` is loaded from
`rsp_writer_q` — whoever was **granted** the lease — so it is the manager's own
record rather than a policy invention, and `zhao_shell_top_v2` already consumes
it for the render guard's window (`rmap_valid_q = v2_lease_valid &&
v2_lease_writer`). So the substitution is more defensible than the entry admits.

I did not make it, for a reason that decides it on cost rather than on taste:
**it would not close I20** — the three MATERIAL.RESOLVE ports keep the entry a
BOUNDARY regardless — while removing a **shell** port, which forces the
paired-diff harness *and* its mutant to be regenerated and re-proves a
framebuffer **safety** guard. That is a large blast radius for zero register
movement. Recommendation: it belongs in the packet that closes I49, where the
triangle port is being changed anyway.

---

## OWNER DECISION

**What is the v1 MEASURE.HISTOGRAM event?**

*Evidence.* The charter section 9 Version 1 says *"ARM predicts a **pixel-error**
threshold **per camera** from prior counters"* and that "FPGA builds a small
histogram of candidate error buckets". `design/contracts/MEASURE.HISTOGRAM.md`
is a 186-line **refusal** whose first named invention is "what number goes in a
bucket"; the RTL deliberately parameterises it away rather than naming it. So
the metric has never been ratified, and the block cannot ratify it.

*Option A — the terrain page-load LOD deviation.* A real, built, tested producer
(`zhao_terrain_loddev`), observing an already-composed live stream
(TERRAIN.MIPFEED), needing no camera and no new block. Three lanes of four carry
`dev1/2/3` zero-extended 24 to 32; the fourth `lane_valid` stays low; `src_id`
comes from `f_src_id_i`. The log2 bucketing makes the zero-extension a constant
shift of the bin index and no change of shape at all, so the width adaptation is
not an invention. **Cost is small and it is the TERRAIN lane's work, not the
debug lane's** — it is their stream and their block, and it should not be bolted
on from here.

*Option B — a screen-space pixel error per camera.* Matches the charter's
sentence exactly. Needs R63's eye (TERRAIN5 is landing it) plus a projector-side
residual that nothing computes. That is a GEOM.LOD-class build — ruling R68's
packet — not a wiring job.

*My recommendation.* **B is what the charter says and A is what exists.** Ratify
**A as the v1 metric**, explicitly and in `spec/`, with the interval's `src_id`
recording which source an interval came from, and leave B as the v2 refinement
the governor will eventually want. Rationale: the ARM's Version-1 job is to
predict a refinement threshold from prior counters, and terrain page deviation
is a genuine *candidate error bucket* for exactly that decision; and the
alternative is an organ that stays empty through v1 while its producer sits
built and tested on disk. If the owner prefers B, **say so in the ledger**, so
that I18 stops reading as "no producer exists" — because one does, and that
sentence will send the next person to build a second one.

*Either way, correct the entry.* Its present reason is not true of this block.

---

## False claims found

1. **I18's own stated cause.** "The block measures a difference against a
   reference" is DEBUG.TRACE's port, not MEASURE.HISTOGRAM's. `ev_err_i` is one
   magnitude per lane. The entry that records a refusal reasoning from the wrong
   port is doing it.
2. **My brief repeated it** — "`expected_fx` against `actual_fx` is a
   differential against a reference the console does not have" — which is how a
   wrong sentence keeps travelling. The confident one-line summary is where the
   error lives.
3. **`design/console_inventory.yml`'s blocker on `zhao_terrain_lodfeed`** ("the
   same missing camera-position producer") is its *consumer's* blocker. Neither
   lodfeed nor loddev has a camera port.

## Instrument defects found

1. **`git show HEAD:<binary> > out` corrupts the file**, and it corrupts it in a
   way that *looks like a real diff*. Comparing a regenerated golden vector
   against it reported 26 differing bytes in a 32-byte file, and a length of 41.
   `cmd /c "git show … > out"` gives 32 bytes and **one** differing byte. The
   handover warns about the BOM; what it does not say is that the corrupted read
   produces a plausible, alarming, entirely fictional byte diff.
2. **Inserting a command mid-file in `spec/commands.zidl` churns four unrelated
   golden vectors.** The sample generator's synthetic `source_id` is the
   command's declaration INDEX, so every command after the insertion point moves
   by one byte. Appending at the end of the file (the precedent PublishResource
   and SetPost already set) churns none. Worth knowing when several packets share
   the zidl: a mid-file insertion manufactures conflicts in binary files.
3. **Verilator's `$finish` does not stop the CURRENT process.** It raises a flag
   honoured when the eval returns, so a `$finish` at the end of the
   `-BadTraceArm` control printed its verdict and then fell straight through to
   the run's own PASS line — **two verdicts for one run**, the second of which
   describes checks that were compiled out. Caught by reading the log rather
   than the exit code, which was 0 either way. The `$finish` is removed and the
   control relies on its assertions, with the reason written beside it. A stop
   that does not stop is worse than no stop.
4. **A new smoke switch with no build-directory TAG silently shares the plain
   run''s object directory.** `run_console_core_smoke.ps1` keys its build dir on
   a per-variant tag; my `-BadTraceArm` fell through the `if/elseif` chain to
   the plain tag. The file''s own comment three lines below describes exactly
   this collision (two checkouts verilating into one directory, failing each
   other''s link with `undefined reference to ...::ctor`, "which reads exactly
   like a partition bug in your own change") — and adding a switch reintroduces
   it silently, because the script deletes `*.o` before compiling, so the
   variants merely rebuild each other rather than failing. Found by reading the
   running process''s `Path`, not by any failure. Fixed, with "EVERY NEW SWITCH
   NEEDS A TAG HERE" beside it; verified by parsing the script (0 errors) and
   evaluating the chain for all seven switches, which now yield seven distinct
   directories.
5. **`npm run abi:gen` cannot run in a fresh worktree**: `tools/abi-gen/dist/`
   is git-ignored and `tsc` is not on PATH, so the build step fails before the
   generator runs. I ran the generator from a `dist` copied out of the
   coordinator's checkout after verifying all 16 `tools/abi-gen/src` files are
   byte-identical between the two trees. **This is a local gate that does not
   match CI** and it is the documented failure shape: an ABI change is easy to
   make and hard to regenerate, so the generated files drift. Recommend pinning
   `typescript` as a devDependency, or committing `dist`.

## Gates

| gate | result |
|---|---|
| `completion_register.py` | **31** (was 33) |
| `check_console_inventory.py` | OK — 202 modules elaborated by the core, 206 fit sources |
| `check_prod_manifest.py` | OK — 335 modules, 68 tops |
| `gen_prod_top.py --check` | fresh (68 instances) |
| `gen_console_board.py --check` | fresh (1,195 core ports re-exported) |
| `mutant_copy_drift.py` | OK — 48 copies |
| `check_quartus17_syntax.py` | RC 0 — 325 files |
| console-board lint | SILENT RC 0 over 223 modules |
| smoke, plain | PASS, `raster pixels=2560`, `frames_admitted=1` |
| smoke `-Mutant` | PASS — `terr_pl_slot_overflow_o` fired once |
| smoke `-BadVertex` | PASS |
| smoke `-NoEchoArm` | PASS |
| smoke `-BadTraceArm` | PASS (new) — `armed=0000000 stored=0 arms=0 arm_refused=1` |
| `host_regwin_directed` | 57 checks passed |

Registering a block in the ledger, the manifest and the source list is **three
different acts**, and `check_prod_manifest` caught the third: all three new
modules are COUNTED census slots (`top:`), not `not-yet-adopted`, because they
are composed and cost ALM in the shipping machine.

**Cost.** Not fitted (the coordinator fits at completion). Shape arithmetic
only, and it is small: the aperture is a four-state FSM, seven saturating
counters and a 2:1 32-bit response mux; the two tenants are a small FSM each
plus a 16-way status mux. **No new memory** — the histogram's bins and the trace
ring are already priced at their own blocks and these only address them. The ALM
figure is an unmeasured claim until a fit sees it.

## ABI note for the coordinator

`DebugTraceArm` 0xF003 is an **additive** command appended at the END of
`spec/commands.zidl`, so it moves no existing record's layout and churns no
existing golden vector but `zcap_minimal.zcap` (which carries the command
table). `abi version` stays 3, on the PublishResource/TerrainEpoch precedent.
TERRAIN5's `eye[3]` addition to SetView touches the same file and the same
generated artifacts; the two zidl hunks do not overlap, and one regeneration of
`npm run abi:gen` after the merge resolves both.

## The elaboration guards were FIRED, not assumed

`zhao_host_regwin` carries four parameter guards in an `initial` block (Quartus
17.0 rejects a module-scope `if`). CLAUDE.md records that **`--lint-only` does
not run `initial` blocks**, so a clean lint says nothing whatever about them —
and a guard nobody has seen fire is a claim.

Fired on purpose, against a scratch wrapper instantiating the aperture with
`NTENANT = 17` (17 regions cannot be named by `addr[15:12]`):

```
LINT of the bad parameterisation : silent about the guard
ELABORATE and run                : %Fatal: zhao_host_regwin.sv:158:
    zhao_host_regwin: NTENANT (17) exceeds the 16 map regions
```

Same split, on new code, exactly as the rule predicts. The wrapper is scratch
and is deliberately NOT committed: it is a parameterisation, not a mutation of
production RTL, so it is reproducible from this paragraph in one command and
leaves no stale copy to rot.

## Every new counter was SEEN TO FIRE

Nine counters were added. None is asserted zero without a case that moves it.

| counter | fired by |
|---|---|
| `hostreg_reads_o` | smoke (9), directed |
| `hostreg_writes_o` | smoke (1), directed — including a REFUSED write, counted as a write |
| `hostreg_refused_unmapped_o` | smoke (1), directed (regions 2, 7 and 15) |
| `hostreg_refused_misaligned_o` | smoke (1), directed (byte offsets 1, 2, 3) |
| `hostreg_refused_tenant_o` | smoke (2), directed (a tenant's own err) |
| `hostreg_refused_timeout_o` | directed, BOTH paths: never-ack, and ack-then-silence |
| `hostreg_stall_cycles_o` | smoke (31), directed (a 7+5-cycle tenant) |
| `cmd_exec_trace_arms_o` | smoke (1) |
| `cmd_exec_trace_arm_refused_o` | smoke `-BadTraceArm` (1) |

`refused_timeout_o` is the interesting one: the composed console asserts it
**zero**, and that zero is now a measurement rather than a claim, because the
directed test showed the counter can move. No mutant is owed — `t_ack_i` is an
input, so the state is reachable with legal stimulus at the block's port. A
mutant is for a state no legal INPUT can reach, not for one no composed SYSTEM
happens to produce, and keeping that distinction is what stops the mutant
directory filling with copies that go stale.
