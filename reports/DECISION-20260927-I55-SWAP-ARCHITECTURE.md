# DECISION — I55's raster swap is a TIME MULTIPLEX, not a second back end

SWAPBUILD, 2026-09-27. Branch `gz/swapbuild`, base `62d8b6a7`.
Standing authority: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0 and §4.
Decision-record format per §0: question; chosen option; reason and alternatives;
constraints/cost; code/tests/compatibility consequences.

---

## QUESTION

Entry `I55` has been refused five times. Every refusal since WALKSWAP has stated
the remaining work in the same words:

> the swap owes a **SECOND setup and attrpack back end** fed from SDRAM plus the
> vertex-fetch arm `zhao_geom_paramwalk` deliberately does not drive. **That is
> ADDITION, not substitution.**

The question this record answers is narrower than the entry's and is the one
nobody has asked: **must the second back end be a second INSTANCE?**

---

## THE FACT THAT MAKES THE QUESTION LIVE

**`zhao_geom_setup` and `zhao_geom_attrpack` are structurally idle for the whole
raster drain.** This is not a schedule observation; it is one assign and one
priority test in the binner's own FSM.

`fpga/rtl/geometry/zhao_geom_binner_v2.sv:818`

```systemverilog
  assign tri_ready_o = (state == S_IDLE) && !drain_req_r;
```

`:931`, unconditional, outside the case:

```systemverilog
        if (frame_end_i) drain_req_r <= 1'b1;
```

`:941`, and note the priority — `drain_req_r` is tested **before** `tri_valid_i`:

```systemverilog
          S_IDLE: begin
            if (drain_req_r) begin
              ...
              state   <= D_TILE;
            end else if (tri_valid_i) begin
```

The bin-phase states are 0..5 and the drain states are 6..11. **The ranges are
disjoint and one FSM owns both.** Once `frame_end_i` has pulsed, no further
triangle is accepted until `D_DONE` clears `drain_req_r`. The binner's own port
comment states the law at `:258-261`.

`zhao_geom_setup` and `zhao_geom_attrpack` exist only to feed that intake. So
for the entire window in which the raster is consuming `job_*`, both blocks have
provably no other work.

Two corroborating properties, both measured rather than assumed:

* `grep -c frame fpga/rtl/geometry/zhao_geom_setup.sv` → **0**. The block has no
  frame port and no frame-scoped register. Its only non-pipeline state is
  `cnt_sub`, a lifetime saturating counter behind `triangles_submitted_o`.
* `zhao_geom_attrpack` has three `frame` hits and **all three are comment text**
  about its throughput budget. Its state (`state_q`, `lane_q`, `va_q/vb_q/vc_q`,
  the `n0_q/dndx_q/dndy_q` accumulators) is strictly per-triangle and returns to
  the lane-0 idle state when a packet retires.

Neither block can be corrupted by being fed from a second source, because
neither carries anything across a triangle boundary.

---

## CHOSEN OPTION

**Time-multiplex the EXISTING `u_geom_setup` / `u_geom_attrpack` pair.** During
the drain window, feed them from the SDRAM walk instead of from GEOM.CLIP, and
take `job_*` from that path rather than from the binner's on-chip drain.

The multiplex is placed **upstream of the three-way fork**, never into one leg.
`zhao_console_core.sv` gates each of the three consumers' valid on the other
two's ready —

```systemverilog
    .tri_valid_i (cl_o_valid && ap_tri_ready_w && vid_tri_ready_w),   // u_geom_setup
    .tri_valid_i (cl_o_valid && st_tri_ready_w && vid_tri_ready_w),   // u_geom_attrpack
```

— so the accept edge at GEOM.CLIP's output is an accept at all three or at none.
A mux into one leg alone breaks that invariant. The mux therefore selects the
**source of the triangle record**, and the fork conjunction is left exactly as
RASTERSWAP repaired it.

---

## WHY, AND THE ALTERNATIVES REJECTED

### 1. A second INSTANCE is refused on measured DSP

`zhao_geom_attrpack` had **never been mapped or fitted — zero rows in either
database** — so the architecture five packets have demanded has never been
priced. SWAPBUILD mapped it. The row is
`zhao_geom_attrpack@swapbuild-secondback`:

| | value |
|---|---|
| status | `map_only` (analysis & synthesis, 30 s) |
| `rtlCleanAtHead` | **true** |
| `treeCleanAtHead` | **true** |
| `measuredDevice` | **5CSEBA6U23I7** (the shipping part) |
| `sourceCommit` | `62d8b6a7` — this packet's base |
| combinational ALUTs | **1,121** |
| estimate of ALMs needed | **2,354** |
| registers | **2,327** |
| block memory bits | 0 |
| **DSP blocks** | **36** |

Read `rtlCleanAtHead` first, always: it is true, and so is `treeCleanAtHead`.

⚠️ **The row carries `notTargetDevice: true` and that flag is WRONG here.**
`run_block_fit.ps1:882-887` stamps it on any row where `-Device` was passed,
without checking whether the value *is* the shipping part. It was
`5CSEBA6U23I7`. The row's own `sizingNote` contradicts itself in one sentence —
*"fitted on 5CSEBA6U23I7 ONLY to measure size; the target is 5CSEBA6U23I7"*.
The part is the target part; the flag is the script's stamping quirk and is
declared here rather than inherited silently.

Adding `zhao_geom_setup`'s existing clean map row on the same part (500
combinational ALUTs, 938 estimated ALMs, 1,340 registers, 4 DSP,
`rtlCleanAtHead: true` — **not** the dirty full-fit row, which reads 743 ALM and
disagrees in both directions), **a second setup+attrpack back end costs:**

> **+1,621 combinational ALUTs, +3,292 estimated ALMs, +3,667 registers, and
> +40 DSP**, on `5CSEBA6U23I7`, from a clean tree at this packet's base.

The shipping part has **112 DSP** in total. **+40 DSP is 35.7% of the entire
device's multiplier budget for one duplicated block**, on a console already
measured at ~335% of that budget.

LANESCOST refused I34's gathering front three days ago on **+11,979 ALUT and
+9 DSP**. **This is 4.4× that DSP bill.** It would be refused on identical
arithmetic, and this record declines to build it rather than build it and have
it refused.

### 2. A second instance is also a SECOND IMPLEMENTATION OF RATIFIED ARITHMETIC

This is the stronger objection and it is this codebase's own law.

`CLAUDE.md`, *"read the SIBLING contract"*: `GEOM.LIGHT`'s contract line 118
warns that re-deriving a ratified computation is *"the exact failure this
contract was written to prevent"*. `zhao_forge_assemble.sv:62-66` states the
admissible form of the exception:

> A second INSTANCE of one law is not a second law; a second EXPRESSION of it
> would be, and there is none here.

A second setup/attrpack back end that **recomputes** the six 240-bit plane
equations from the ProjectedVertex records would have to produce bit-identical
planes to the live path or the picture changes. That equality would be a
*claim requiring verification at every plane, every profile and every edge
case*. The time multiplex needs no such verification: **it is the same silicon,
so the planes are bit-identical by construction.**

This is the decisive reason. The DSP number decides affordability; this decides
correctness, and it does not depend on a budget.

### 3. A metadata SIDECAR in SDRAM was considered and rejected on capacity

The obvious alternative to recomputing is to not recompute: write the 1,877-bit
metadata the existing pair already produces into SDRAM keyed by arena triangle
id, and read it back on the walk. Directive §4 explicitly pre-authorises the
schema amendment (*"a versioned extension or immutable sidecar keyed by the same
identity"*), so this was a live option and not a decision refusal.

It is rejected on **measured capacity**, not on taste:

* the full `job_*` record is 1,877 metadata bits + 126 corner bits + 16 src-id
  bits + 2 profile bits = **2,021 bits, 253 bytes**, so a 256-byte stride;
* `zhao_geom_paramarena.sv:599-621` computes `VIEW_USED_B = 3,407,872` against a
  `ZHAO_PARAMBUF_VIEW_SPAN` of `0x0040_0000` = 4,194,304 — **786,432 bytes free
  in the view**, which is 3,072 records against `MAX_TRIS = 16,384`;
* the adjacent region `0x06A0_0000..0x07FF_FFFF` is **not** free: `memory_rules.md`
  line 903 assigns it to `RENDER.ASSET_POOL`, 22 MiB, ENGINE1 read-only.

So the sidecar does not fit at declared capacity, and §0 forbids reaching zero
by shrinking a declared maximum. It is also the wrong shape twice over: it
stores the DERIVATIVE (253 B per triangle, unshared) where the SOURCE is already
resident and **shared between triangles** (3 × 24 B ProjectedVertex, 213 vertices
for 75 triangles on the smoke fixture); and at R7's 32,768-reference giant it
would read 253 bytes per tile reference against an SDRAM already measured at
**88% occupancy** (`sdram_busy` 645,846 of ~730,000 clocks).

### 4. What the time multiplex is, in directive §4's own words

> The architect chooses a bounded implementation and **may move backing state to
> SDRAM rather than growing a frame-sized FPGA register file.**

The frame-sized state this moves is `zhao_geom_binner_v2`'s `meta_ram`. At the
console's ratified `METAW = 1877` and `TRI_CAP = 128` that is
**47 slices × 40 bits × 128 = 240,640 bits**, and it is real block memory — the
binner's map row reads 2,109 registers against 191,296 memory bits, so it
inferred. The swap retires it **by removal**, together with the tile lists the
walk replaces.

That is substitution. The entry's standing claim that the swap is *"ADDITION,
not substitution"* is true of the second-instance implementation and **false of
this one**.

---

## CONSTRAINTS AND COST

**No new SDRAM share slot is needed, and this is why the vertex arm goes INSIDE
`zhao_geom_paramwalk`.** Both shares are full: `zhao_geom_mem_adapter` is
`N=10` with all ten requesters driven (its own header line 1 says "NINE" and is
stale by one), and `u_geom_wshare` — the share `paramwalk` actually sits on — is
`N=3` with all three driven. A new read client would cost an `N`→`N+1` widening
plus a re-proof of `zhao_mem_share_n`'s round-robin bound at the new width.
Issuing the vertex reads from the walker's existing ENGINE1 socket costs none of
that.

**The decoder already exists and is tied off.** `zhao_geom_paramwalk`
instantiates `zhao_geom_parambuf u_rec` with the full 24-byte ProjectedVertex
arm wired to `'0` (`:306-307`), under a comment calling it *"an uncashed cheque
authored on purpose"*. Driving it costs no new decode logic. What must be added
is a `w_vert_base_q` latch — `pub_vert_base_i` is a port today but is used at
exactly one site (`:358`, the directory round-trip compare) and is never turned
into an address.

**The throughput cost is the open risk and it is the thing to measure.**
`zhao_geom_attrpack` is a 3-state sequencer at **13 gpu clocks per triangle**
(six lanes at two clocks plus the accept), and in this architecture it runs once
per **tile reference** rather than once per triangle — 101 references against 75
triangles on the smoke fixture. Against the on-chip drain's measured
**4.12 clocks/ref** that is the number that could refuse this design, and it
will be measured rather than argued. The one encouraging prior is the binner's
own header (`:204-206`): behind the raster, *"the drain is idle ~80% of the
time"*, so the drain is not the frame's critical path today.

**R7's giant is the case that may not close.** At 32,768 references the walk's
measured 29.58 clocks/tri plus three vertex fetches per triangle is a large
number against a frame. Directive §7 says exactly what to do if it does not
close: *"retain the correct complete oracle, use only the already-permitted
admission/fallback behavior, and report the deadline miss."* That is a finding,
not permission to shrink the giant.

---

## CONSEQUENCES — CODE, TESTS, COMPATIBILITY

**Code.** `zhao_geom_paramwalk` gains a vertex-fetch arm and the decoded vertex
outputs. That is a port change on a leaf, so `zhao_prod_top.sv` and
`zhao_console_board.sv` must be regenerated and every bench that instantiates
the block must be connected — *"a port on a leaf costs its WHOLE instantiation
chain plus every bench."* A triangle-source mux is added upstream of the
console's three-way fork. `zhao_geom_binner_v2`'s `meta_ram` and tile lists
retire by REMOVAL, never by tie-off, at the commit that makes the walk the sole
`job_*` source.

**Tests.** The existing `tb_zhao_geom_paramarena.sv` already carries the arena,
the real guard, arbiter, controller, SDRAM model and `zhao_geom_paramwalk`, so
the vertex arm is testable in a fixture that exists. Every counter added is
shown to fire; a guard unreachable with legal stimulus gets a committed mutant
under `tests/mutants/` with inverted polarity.

**Compatibility.** No ABI change and no record-schema change — this is the
option that needs neither, which is a point in its favour that the sidecar could
not claim. `spec/commands.zidl` is untouched, so `npm run abi:check` is not
implicated.

**The id repair is load-bearing and must not regress.** `geom_tidq_directed` is
83 checks with 21 failures against the base RTL, and the walk reads descriptors
**by arena id** — the whole design assumes RASTERSWAP's repair holds.

---

## WHAT THIS RECORD DOES NOT CLAIM

It does not claim the swap is built, that `I55` is closed, or that any pixel yet
depends on bytes that went through SDRAM. It claims one thing: **the second back
end the entry has demanded for five packets does not have to be a second
instance, and the instance form is refusable on this tree's own measured
numbers.** Whether the multiplex form closes on throughput is measured in
SWAPBUILD's FINDINGS, not here.
