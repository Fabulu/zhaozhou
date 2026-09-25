# FINDINGS -- earthram

Packet **EARTHRAM**, 2026-09-25. Branch `gz/earthram`, based on `ead6f107` --
the live tip, **not** the `13b22987` the brief named (see premises).

**This is an OPTIMISATION packet. The completion register did not move and no
pixel moved.** Both are stated plainly because both are true and neither is a
disappointment: the deliverable is a resource number.

Owner authority: ruling R244 / D-EARTH-A, and
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` section 8 -- *"Preserve the
full 16-field Earth capability. Synchronous banks, replicated read-only RAMs,
exact narrower representations ... are authorized."*
**`MAX_FIELDS` was not touched and remains 16.**

---

## 1. The register: 12 -> 12

`completion_register.py` reads **12 MANDATORY GAPS REMAINING** at the base
commit and **12** at the pushed commit. Nothing closed, nothing opened.
`gate_sweep.py` says *"every gate matches the committed baseline (30 gate(s))"*
at both ends.

## 2. The two map numbers, side by side

Two `-MapOnly` leaf runs of `zhao_field_earth_adapter` via
`tools/quartus/run_block_fit.ps1`, same closure (`-ExtraSources` with the one
file -- the adapter instantiates nothing), same device, **both from a clean
tree** (`rtlCleanAtHead: true` on both rows), digests recorded.

| | before `ead6f107` | after `1397b754` | delta |
|---|---|---|---|
| combinational ALUTs | 2,419 | **888** | -1,531 |
| dedicated logic registers | 6,274 | **1,093** | -5,181 |
| **estimated** ALMs (A&S estimate) | 4,326 | **1,628** | **-2,698** |
| **placed** ALMs | n/a | n/a | **no fit ran** |
| block memory bits | **0** | **4,880** | +4,880 |
| DSP blocks | 0 | 0 | 0 |
| map seconds | 30.9 | 26.4 | |

The five are kept apart and **never summed**, per section 8: *"Separate ALUTs,
registers, estimated ALMs, placed ALMs and capacity bounds."* The capacity bound
(5CSEBA6U23I7: 41,910 ALM / 553 M10K / 112 DSP) is recorded in the receipt as a
**bound only** -- a leaf map's utilisation is not a closure claim.

**The packing, read off the tool that chose it:**

```
altsyncram:b_uni_rtl_0|...|ALTSYNCRAM ; M10K block ; Simple Dual Port ;
  Port A 16 deep x 305 wide ; Port B 16 deep x 305 wide ; 4,880 bits ; MIF None
```

No `uninferred` and no `cannot regroup` note in **either** report.

### Reading the silence the right way round

The brief warned about this and it was the actual situation. Quartus prints
*"uninferred due to ..."* when it **considered** an array and refused. At
`ead6f107` it printed **nothing at all** about `b_age`/`b_phase`/`b_par` -- the
array never presented as a RAM candidate -- so an absence of complaints was the
worse signal, not success. **The number that settles it is
`Total block memory bits`, and it was 0.**

### 305, not 320, and it is not a loss

The payload word is `{params[255:0], phase[31:0], age[31:0]}` = 320 bits and
Quartus packed **305**. The missing 15 are the phase field's bits 31:17, which
are **structurally zero in both arms of its assignment**: `PHASE_ONE` is
`32'h0001_0000`, and the other arm is `{15'd0, dv_q}` with `PhaseW = 17`. The
module's own header already proves phase <= 65,536; Quartus found the same proof
and constant-trimmed. `320 - 15 = 305`. **Checked by hand against the two
assignment arms, not inferred from the arithmetic happening to work out.** The
directed test's `in[3] == u.phase` check passes on the full 32-bit value, so the
trim is lossless.

### The owner's ~8 M10K is neither confirmed nor refuted, and that is the honest answer

**A map reports no block count -- that is a fitter result, and no fit ran.** What
it reports is the packing above. At the M10K's widest legal mode (x40) a 305-bit
port implies `ceil(305/40) = 8` blocks, consistent with the owner's estimate.
**That 8 is an implication from packing, not a placement**, and section 8 is
explicit: *"exact M10K counts require legal width/depth/port packing, not payload
bits divided by 10,240"* and *"Do not declare usable RAM headroom from
payload-bit percentage alone."* It is not quoted here as a measured figure.

### The receipt, because the report is not in history

`reports/synthesis/blockpaths/*.map.rpt` is **gitignored by a size rule**, and
the committed `.map.summary` beside it carries `Logic utilization (in ALMs):
N/A`, registers and block memory bits -- but **neither the ALUT count nor the
estimated ALMs**, which are the two numbers an area argument is made of. The
attribution report an entire ALM lever list was derived from was found gone from
disk on 2026-09-25 for exactly this reason.

`tools/quartus/extract_map_receipt.py` ->
`reports/synthesis/receipts/earthram_field_earth_adapter_map_pair.json` is that
missing half. "Commit the probe" was already the rule; the gap it left was the
**receipt**.

## 3. The cycle cost: ZERO, measured on both revisions

The owner's instruction allowed *"one more state on an ~80-100-cycle field run"*.
**None was needed.**

`tests/field/field_earth_adapter_cycle_census.cpp` is a committed probe, and the
**same source was built twice** -- once against the flop-bank revision, once
against the RAM revision, nothing else changed:

| | before (flop bank) | after (RAM) |
|---|---|---|
| ACCEPT-TO-OFFER | **1 clock** | **1 clock** |
| ACCEPT-TO-ANSWER (3-clock engine model) | **6 clocks** | **6 clocks** |

Why it is free: `req_valid_o` is `(state == E_REQ)` and `state` leaves `E_IDLE`
on the very edge that loads `b_uni_q`, so the payload lands **exactly as the
request is first offered**. The synchronous read reused the `E_IDLE` decision
cycle that already existed. The census also asserts the payload is **valid on
that clock** (`age == 100`, the vertex x, `req_slot_o == 2`) -- a latency of zero
is worthless if the words are not there yet.

**Identical before/after numbers are the one reading CLAUDE.md says to distrust
hardest**, so the rebuild was proven rather than assumed: tree content asserted
in both directions (`b_par [0:MAX_FIELDS-1]` present for BEFORE;
`ramstyle = "M10K"` present and `b_par` absent after the restore), timestamps
forced, and the build log shows **10 steps of re-verilation and a relink**, not
`no work to do`.

### And the "~80-100 clocks" figure belongs to the engine, not the adapter

Worth stating precisely, because the brief's framing invites the confusion. The
adapter header's *"order 80-100 clocks PER COVERED VERTEX PER FIELD"* is
`zhao_field_host`'s **program run length** (REGS + IN_LANES + the program's own
length). The adapter's own contribution is the 3 clocks the census measures
around it. So the state this change could have added would have been 1 clock on
~3, not 1 on ~90 -- a larger proportional risk than the brief's phrasing
suggests. It is zero.

## 4. What changed in the RTL

* `b_age` / `b_phase` / `b_par` -> **one 320-bit `b_uni` array** with a named
  layout (`UniAgeLo`/`UniPhaseLo`/`UniParLo`/`UniW`),
  `(* ramstyle = "M10K" *)`, read synchronously into `b_uni_q` in a
  **reset-free** `always_ff`.
* The **per-element reset loop is removed from the payload only.** That loop was
  the second dynamic write address the island brief's S5.3 forbids by name.
  **Both repairs were needed** -- either alone leaves the array in flops.
* `b_begun` / `b_obj` / `b_res` (**80 bits**) deliberately **stay in flops with
  their reset**: all three are read on the *decision* cycle, which is what
  chooses whether to read the payload at all, so a synchronous read could not
  have answered in time. Moving them would cost a state to delete a 16-to-1 mux
  five bits wide.
* **`held_in` (480 flops) is deleted.** `b_uni_q` is the directive-15.1 capture
  latch itself, not an extra stage in front of one.
* `cap_wx`/`cap_wz`/`cap_slot`/`cap_noprog`/`cap_a` are loaded by **one enable
  (`cap_en_c`) in one process**, so the whole request is an atomic capture.
* **No `no_rw_check`.** The intake writes `wr_a` from one process and the lane
  stream reads `lane_a` from another, and nothing in this module interlocks them,
  so a same-cycle same-address collision is not provably impossible. The
  nonblocking read yields the **old** word -- byte-identical to the
  combinational flop read it replaces, which **is** the equivalence argument.
  Declaring `no_rw_check` would buy area by asserting an interlock this module
  does not own.
* **No port was added** (see section 7), so `zhao_prod_top` needed no
  regeneration; `gen_prod_top.py --check` confirms fresh anyway.

## 5. The counter added, and the evidence it fires

**One**: a third arm of the existing `lane_desync_o`, the *bank pairing* check.

A registered read means data arrives a cycle after its address -- the exact shape
of CLAUDE.md's metadata-swap defect ("A's data, A's token, B's metadata", every
other counter balancing). Arm (c) differences `cap_a` (the address the payload
was **actually read at**, loaded by `cap_en_c` in the reset-free RAM process)
against `lane_a`/`cur_lane` (loaded **in the main process** by the consumer's
`ans_ready_i` handshake and by `vtx_fire_i`). **Two processes, two separately
authored enables, neither loading both** -- so it can see a *timing* fault and
not only a value one, which is the first question that chapter says to ask.

**Fired by legal stimulus -- no committed mutant is owed.** Directed test
**case 9d**: a control pass first shows the arm **silent** while a request sits
in flight, then the consumer accepts a vertex under the live request and the arm
**moves**. Arms (a) and (b) are held silent across that stimulus on purpose
(`vtx_live` and `ans_ready_i` both high; replayed count equals `lanes_i`) so the
increment is **attributable**. The test asserts the correct behaviour, never the
bug.

**Its first version did not fire, and that is the most useful thing this packet
found about its own instrument.** It moved the vertex while `cur_lane` was still
0, so `cap_a` and `lane_a` both read 0 and there was nothing to disagree about --
a detector that looked incapable of firing when the stimulus simply never
reached the state. Lane 0 is now served in full first, and the reason is written
into the test beside it.

`tests/CMakeLists.txt`'s claim that *"both arms of the lane shadow guard"* are
fired was updated to **three**, and the directed test's own header census with
it. A stale claim about what a test covers is the same defect one level up.

## 6. Premises in the brief that were false

1. **"Integration branch ... at `13b22987`."** Origin was at **`ead6f107`**, two
   owner commits newer (`460296f9`, `ead6f107`), adding the 601-line
   `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`. I branched from the live
   tip. **That file is standing delegated authority and section 8 governs this
   packet directly**, including the correction in premise 3.
2. **"the ugly estimate ... about 2,600 of the ~3,350 estimated ALM."** The
   measured estimate for the whole block is **4,326** estimated ALMs, not ~3,350
   -- **the estimate was low by about 29%**, the unusual direction for this repo.
   The *saving* lands almost exactly on the owner's figure for the bank + mux
   specifically: **-2,698** against "about 2,600".
3. **"spend roughly 8 M10K."** Corrected by the coordinator mid-packet from
   section 8, and the correction is right: a map reports **no block count at
   all**. See section 2 -- 8 is an implication from the 305-bit packing, not a
   measurement, and is not quoted as one.
4. **"5,200-bit bank."** 5,200 is the total. The part that **moved** is the
   **5,120-bit payload**; **80 bits of control stayed in flops on purpose**, and
   the RAM is **4,880 bits** after Quartus' lawful 15-bit constant trim. Three
   different numbers, all correct, none interchangeable.
5. **"add one state to an ~80-100-cycle field run."** Zero states were added, and
   the ~80-100 is the **engine's** program run, not the adapter's overhead
   (section 3).
6. **The adapter's own header** carried the now measured-false *"order 2,000
   ALM"* and ended by recommending **cutting `MAX_FIELDS` to 4** to "recover
   three quarters of it". That is now forbidden by R244 and section 8. The
   paragraph is rewritten and the sentence **explicitly marked superseded** in
   the file, per CLAUDE.md: *"when you fix a thing this file names as broken, fix
   this file in the same commit."*

## 7. What I did NOT do, and why

* **No fit.** Two leaf maps only, as authorised and as section 8 sanctions
  (*"local, bounded synthesis/map experiments ... without waiting for an
  impossible full fit"*). **Therefore there is no placed-ALM and no Fmax number
  for this change.** The -2,698 is an Analysis & Synthesis **estimate**. This
  block has never been through `quartus_fit`, and a -2,698 estimated-ALM result
  is not a -2,698 placed-ALM result.
* **Did not touch `MAX_FIELDS`.** Not mine, and now a hard capability boundary.
* **Did not add an output port.** A new `bank_pair_mismatch_o` would have cost
  `zhao_console_core` (**shared**), `zhao_console_board`, the generated
  `zhao_prod_top`, `tb_zhao_console_core_smoke` and **two committed
  `zhao_console_core` mutants** -- six files including a shared one and two
  drift-prone copies -- for one counter. Folded into `lane_desync_o`, whose
  documented job already is "this module's belief about which lane it is on
  versus the truth" and which was already multi-arm. The third arm is documented
  as such in the header.
* **Did not move `b_begun`/`b_obj`/`b_res` to RAM** (section 4).
* **Did not declare `no_rw_check`** (section 4).
* **Did not write a committed mutant.** None is owed: every counter in this
  block, including the new arm, is reached from the block's own boundary. The
  file's standing claim to that effect is preserved and still true.

## 8. Instrument defects found this packet

* **My own `extract_map_receipt.py` reported `"; Legal Notice ;"` as the RAM
  packing.** It anchored on the string `"RAM Summary"` and matched the table of
  **contents** twenty lines into the file. Caught only because the value was
  printed and read. It now anchors on the column header only the real table has
  **and raises if a present table names no memory primitive** -- a tool that
  emits a plausible wrong row is the broken-instrument law wearing a receipt's
  clothes.
* **My own case 9d did not fire** on first writing (section 5).
* **`tests/CMakeLists.txt` is LF while the RTL and the directed test are CRLF.**
  The edit script refused rather than converting 17,641 lines of a **shared**
  file's endings, which would have collided with every other packet. Endings are
  now detected per file and restored, never assumed. This is CLAUDE.md's
  merge-conflict chapter arriving from the write side rather than the merge side.
* **`run_block_fit.ps1 -RowLabel` requires a separator prefix** (`@...` or
  `-...`) and refuses loudly in preflight, costing seconds rather than a run.
  Working as designed; recorded so the next packet does not rediscover it.

## 9. Evidence, all at the pushed commit

| check | result |
|---|---|
| `field_earth_adapter_directed` | **132/132 checks passed** (was 128; +4 for case 9d) |
| `field_earth_adapter_cycle_census` | **PASSED**, 0 failures, both revisions |
| `test_cmd_exec_directed` (R60) | **977 checks passed** |
| `verilator --lint-only -Wall` | **0 diagnostics** |
| `check_quartus17_syntax.py` | RC 0 (self-test 13 fire / 22 no-fire PASSED) |
| `gen_prod_top.py --check` | fresh |
| `check_prod_manifest.py` | RC 0 |
| `check_console_inventory.py` | RC 0 |
| `gen_console_board.py --check` | fresh |
| `check_case_labels.py` | RC 0 |
| `gen_shell_paired_diff.py --check` | fresh |
| `gate_sweep.py` | 30 gates, nothing moved vs baseline |
| `completion_register.py` | 12 gaps (unchanged) |
| `run_console_core_smoke.ps1` + 4 controls | **all five PASS** -- see section 11 |
| `mutant_copy_drift.py` | run **after** the commit (R121) |

**A lint-clean block is not a synthesizable one, and this one is now both** -- it
went through `quartus_map` twice and elaborated cleanly, which is evidence
`--lint-only` cannot give (it does not run `initial` blocks, and the new
`BankAw`-covers-`MAX_FIELDS` elaboration guard lives in one).

## 10. Owner decisions found

**None requiring the owner.** Every choice above was made under the standing
delegation and recorded in the commit messages in the directive's format
(question; option; reason and alternatives; cost; consequences).

The one item worth surfacing to the coordinator is the **`no_rw_check` refusal**
(section 4). There may be a real interlock between the uniform intake and the
patch lane walk, owned by `zhao_terrain_fieldlist`'s `patch_stall_o`. If somebody
establishes it, `no_rw_check` would remove Quartus' pass-through logic for a
further small ALUT saving. **I did not assert it because I did not verify it**,
and asserting an interlock you have not read is how this repo's worst numbers get
made.

## 11. The console smoke suite and its four committed controls

The adapter **is** composed in `zhao_console_core`, so this is the integration
evidence rather than a formality. All five runs exit 0 and all five print their
own PASS verdict -- and the wording differs per switch, which matters: two of
them print no `SMOKE: PASS` line at all, so **an RC of 0 is not the verdict** and
a summariser keyed on the plain PASS string reports a false blank for those two.
The verdict lines, quoted:

| run | verdict |
|---|---|
| plain | `SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.` -- with **`raster pixels=2560`** over 160 bursts, **1 admitted frame**, 14 triangles, 1,190 fragments carrying a texel |
| `-Mutant` | `SMOKE: MUTANT PASS -- terr_pl_slot_overflow_o fired 1 time(s). The detector works; production's zero is a measurement.` |
| `-BadVertex` | `SMOKE: BAD_VERTEX PASS -- one refused record dropped its batch (holes=1, groups_poisoned=2, replay_poisoned=8) and the frame completed` |
| `-NoEchoArm` | `SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.` |
| `-BadTraceArm` | `SMOKE: PASS/BAD_TRACE_ARM -- the reserved bit was refused whole and nothing was armed.` |

`raster pixels=2560` and `frames_admitted=1` are the two numbers the gate list
names, and both hold.

**This packet changed no port**, so none of these benches was touched and none
needed a new build-directory TAG. They are reported because the block is inside
the composed core and a change to its internal storage could in principle have
altered its handshake timing -- and the measured answer is that it did not.
