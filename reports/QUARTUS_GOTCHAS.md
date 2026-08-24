# What Quartus 17.0.2 Lite does that the simulators do not

> Everything here is **simulation, synthesis or fit**. No hardware has run any of
> it.

Every entry below cost this project real time, and every one has the same shape:
**Verilator and slang accept it, the differentials pass, the mutation sweep
scores clean — and only a Quartus run says otherwise.**

**§10 is the exception, and deliberately so.** It was not discovered by being
surprised; it was MEASURED, with a controlled grid of generated microbenches,
after three separate blocks had each paid for the same lesson individually. Nine
entries of hindsight bought one entry of foresight. They were scattered
across individual RTL headers, one per block, which is why each was rediscovered
rather than remembered.

This file is the list. Read it before writing arithmetic RTL, and add to it when
the tool surprises you.

---

## 1. An inline `genvar` in a for-header is rejected

```systemverilog
for (genvar N = 0; N <= 8; N++) begin : g   // Verilator: fine. slang: fine.
```
```
Error (10170): syntax error near text "genvar"
```

Declare it separately. Found in `zhao_crc32c_fold`, and only the composed fit
caught it — three frontends had already agreed.

## 2. `lpm_divide` numerators are capped at 64 bits

```systemverilog
logic signed [71:0] q;
assign q = (n + 72'sd8) / 72'sd9;          // division by a CONSTANT
```
```
Error (272006): In lpm_divide megafunction, LPM_WIDTHN must be
                less than or equals to 64
Error (12154): Can't elaborate inferred hierarchy "lpm_divide:Div0"
```

The block does not synthesise at all. Found in `zhao_geom_lod`, where the
division path was carrying 72 bits of "free" slack; the honest width was 40.
See §5 — the slack was not free in a second way either.

## 3. `(* multstyle = "logic" *)` is SILENTLY IGNORED

```systemverilog
(* multstyle = "logic" *) logic signed [24:0] p;
assign p = d * t;      // still infers a DSP block. No warning. No error.
```

Quartus 17.0.2 accepts the attribute and does nothing with it. There is no
diagnostic, so the only symptom is a DSP count that will not fall.

**The fix is to write the multiply as what it is.** In `zhao_field_sin` the
operand `t` is six bits, so `d * t` is a six-term shift-add, exact in 25 signed
bits. That took the Field engine's last inferred DSP away — the difference
between "one nonconstant `*` in the cone" and four blocks.

## 4. Indexing the result of a function call is rejected

Found in `zhao_geom_binner` (2026-08-19). Assign the call to a variable first.

## 5. Operand slack is NOT free — it changes DSP decomposition

Not an error; a cost. `zhao_geom_lod` carried 72-bit operands as "deliberate
slack" because a comparison does not care. Measured:

| | ALMs | DSPs |
| --- | ---: | ---: |
| 72-bit operands | 1,436 | **28** |
| 64-bit, proven sufficient | 1,303 | **18** |

A 72-bit operand asks for a 72×72 multiplier where the honest need is 32×32.
The external survey named this a textbook FPGA anti-pattern. **Prove the width,
then synthesise.**

## 6. `set_instance_assignment -name VIRTUAL_PIN ON -to *` matches EVERY node

A wildcard instance assignment is matched against every node name in the design,
not the top-level ports it was written for. The composed synthesis peak went
from an apparent **28.4 GB** requirement — which was believed to mean the design
could not be built on this machine, and led to a whole second-machine script —
to **6.2 GB** once the 101 ports were named explicitly (`d1a2b8a`).

Note the asymmetry: the per-block lane still *wants* the wildcard, because there
the leaf block's own ports are the ones being virtualised. Fixing one broke the
other (`bc66758`).

## 7. The per-block SDC must name the port the block actually has

`tools/quartus/run_block_fit.ps1` handed every leaf block the **shell's** SDC,
which constrains `gpu_clk` / `vid_clk` / `audio_clk`. Sixty-three of this
design's seventy-one clock ports are named `clk`.

```
Warning: Ignored create_clock at blockfit.sdc(4):
         Argument <targets> is an empty collection
```

Three times, in every run, for weeks. **Every per-block fit ever run had no
timing objective** — 47 rows in which the Fmax column is not a slow measurement
but not a measurement at all, and the area columns were taken with the fitter
under no timing pressure, which understates what a constrained fit needs.

Fixed by generating a per-block SDC. Verify a fit is constrained by looking for:

```
Info (332111):   Period   Clock Name
Info (332111):   10.000          clk
```

**And budget for it:** a constrained fit is far heavier. One small block spent
7m27s in placement preparation alone under Advanced Physical Optimization, where
the whole unconstrained fit used to take about ten minutes. Three concurrent
fits exhaust this 24 GB machine.

## 8. A module-scope `if` generate needs the `generate` keywords

```systemverilog
// Verilator: fine. slang: fine. IEEE 1800: fine.
if (!(MUL_LANES == 1 || MUL_LANES == 3 || MUL_LANES == 6)) begin : g_illegal
  ZHAO_GEOM_SKIN_MUL_LANES_MUST_BE_1_3_OR_6 u_static_assert ();
end
```
```
Error (10170): Verilog HDL syntax error at zhao_geom_skin.sv(223) near
               text: "if";  expecting "endmodule"
Error (10112): Ignored design unit "zhao_geom_skin" due to previous errors
```

Wrap it in `generate` / `endgenerate`. Found in `zhao_geom_skin` (2026-08-23),
which had linted clean under `-Wall` at all three of its `MUL_LANES` settings
and then died in analysis and synthesis at 44 seconds.

This is §1 again with a different keyword, and it is worth counting twice: both
are **generate-region syntax that three frontends accept**. The rule that
generalises them is that Quartus 17.0.2's parser predates SystemVerilog's
relaxations around generate blocks, so anything clever at module scope is worth
a cheap `quartus_map` before it is worth a fit.

The construct itself is a portable static assertion -- an unresolved module
reference inside a generate-if, which errors in every tool when the condition
holds and is never elaborated when it does not. `$error` was avoided precisely
because this tool's support for elaboration system tasks was unknown; the
irony is that the `if` was the part it could not parse.

## 9. Constraining the CLOCK does not constrain the BLOCK

§7 fixed `create_clock` resolving to an empty collection and concluded the Fmax
column was a real measurement at last. **It is real only for
register-to-register logic.** The generated per-block SDC has no
`set_input_delay` and no `set_output_delay`, and TimeQuest excludes every
pin-to-register and register-to-pin path when none is declared. For a leaf block
whose arithmetic sits **between** its ports — which is most of them — that is
most of the block.

Found 2026-08-23 on `zhao_texture_tmu`, by re-running `quartus_sta` on three
kept workspaces. No re-fit; the same databases the fits had already produced:

| workspace | reported Fmax | actual worst timed path |
| --- | ---: | --- |
| `@pre-rearch` | 199.72 MHz | `texture_samples_o[19] → texture_samples_o[27]`, 4.818 ns |
| `FILT_LANES=4` | 192.46 MHz | `texture_samples_o[3] → texture_samples_o[21]`, 4.634 ns |

**Both numbers are the 32-bit saturating sample counter's carry chain.** The
block's 32 multiplies, its five format decodes, its 48-bit address generator and
its three wrap folds appeared in **no timed path at all**.

The third workspace differs in one accidental way — at `FILT_LANES = 2` one
filter output lands in a real register — and there the worst path is
**20.462 ns**, through the very arithmetic the other two rows called fast.

Applying `set_input_delay`/`set_output_delay 0` to that same database moves the
worst path again, to **`req_mode_i[8] → q_addr_r[55]` at 37.004 ns** — the
address generator, which that block's own contract had named "the longest path
in the file" and which nothing had ever measured.

**Fixed** in `tools/quartus/run_block_fit.ps1`: the generated SDC now declares
`set_input_delay -clock clk 0` on every non-clock input and
`set_output_delay -clock clk 0` on every output, guarded by a `get_ports clk`
test because eight of this design's clock ports are not called `clk`. The model
is *same clock, no external budget* — optimistic about inter-block routing,
exact about the logic inside the block, which is what a per-block
characterisation is for.

**Every row measured before 2026-08-23 evening carries the old meaning**, in
exactly the way 47 rows carried §7's. A row's Fmax is trustworthy only if that
block's critical logic happens to run register-to-register.

The rule that generalises §7 and §9 together: **a constraint file is not a
timing objective until you have read which path it actually reported.** Both
entries were found the same way — by disbelieving an implausible Fmax and asking
the tool which path produced it.

---

## 10. Storage inference is a LAW with two independent killers, and the penalty is superlinear

Every entry above was found by being surprised by one block. This one was
**measured on purpose**, with a controlled grid of generated microbenches
(`tools/budget/gen_calib.py`, mapped by `tools/quartus/run_calib.ps1`), because
`zhao_field_seq` reporting **zero M10Ks while spending 8,901 ALMs** deserved an
explanation better than "Quartus did not feel like it".

Thirty-two RAM templates, four shapes x sync/async read x reset/no-reset x one
and two read ports, on `5CSEBA6U23I7` under this project's own settings:

| shape | bits | sync + no reset | sync + RESET | ASYNC + no reset | ASYNC + reset |
| --- | ---: | --- | --- | --- | --- |
| 64x32 p1 | 2,048 | **2,048 bits, 40 ALM** | 0, 1,411 | 0, 1,427 | 0, 1,411 |
| 64x32 p2 | 2,048 | **4,096 bits, 59 ALM** | 0, 1,769 | 0, 1,801 | 0, 1,769 |
| 256x16 p1 | 4,096 | **4,096 bits, 26 ALM** | 0, 2,801 | 0, 2,818 | 0, 2,801 |
| 256x16 p2 | 4,096 | **8,192 bits, 38 ALM** | 0, 3,501 | 0, 3,534 | 0, 3,501 |
| 1024x32 p1 | 32,768 | **32,768 bits, 44 ALM** | 0, 22,071 | 0, 22,071 | 0, 22,071 |
| 1024x32 p2 | 32,768 | **65,536 bits, 65 ALM** | 0, 27,566 | 0, 27,566 | 0, 27,566 |
| 2048x18 p1 | 36,864 | **36,864 bits, 31 ALM** | 0, 24,985 | 0, 25,039 | — |
| 2048x18 p2 | 36,864 | **73,728 bits, 45 ALM** | 0, 31,145 | — | — |

> **Synchronous read AND no reset touching the array — it infers, and costs tens
> of ALMs. EITHER an asynchronous read OR a reset on the array — ZERO memory
> bits. The two conditions kill inference INDEPENDENTLY; either one alone is
> sufficient, and having both is no worse than having one.**

Read the `sync + RESET` and `ASYNC + no reset` columns against each other: they
are the same number to within a rounding of the estimator. Neither is a
degradation of inference. **Both are its complete absence.**

### The penalty is superlinear in the array

| bits | inferred | not inferred | penalty |
| ---: | ---: | ---: | ---: |
| 2,048 | 40 ALM | 1,411 | **35x** |
| 4,096 | 26 ALM | 2,801 | **108x** |
| 32,768 | 44 ALM | 22,071 | **502x** |
| 36,864 | 31 ALM | 24,985 | **806x** |

An inferred memory is nearly free and barely grows — 2,048 bits and 36,864 bits
both cost about thirty ALMs, because the cost is the address and control logic,
not the storage. A memory that did not infer costs **every bit as a flip-flop
behind a mux tree**, so it grows with the array *and* with the mux depth.

**The rule matters most exactly where the array is biggest and the temptation to
"just make it registers" is strongest.**

### The detection signal is `memory bits = 0`

There is no warning. Quartus does not say "I declined to infer your memory". The
only symptom is a number that is zero, in a block that obviously contains
storage — which is precisely §3's shape, one more time.

> **A fit or map reporting zero memory bits for a block that clearly contains
> storage is a FAILED IMPLEMENTATION, whatever the tests say.**

That was already the acceptance rule written into the Field ruling. These
measurements turn it from a policy into a threshold: expected bits from the
elaborated AST (`tools/budget/scan_rtl.py`), inferred bits from the map row, and
`EXPECTED_RAM_NOT_INFERRED` in `reports/BUDGET_HEATMAP.md` when the first is
large and the second is zero.

### What it retro-explains, with no new fit required

* **`zhao_field_seq`: 0 M10Ks, 8,901 ALMs.** Its register file is
  `logic signed [31:0] rf [0:63]` — **64x32, read asynchronously, written from
  a reset branch.** `calib_ram_64x32_async_rst_p1` is a byte-for-byte model of
  it and costs **1,411 ALMs against 40**. Both killers, and the block also holds
  three constant tables built as `always_comb` case trees, which are ROMs by any
  other name.
* **`zhao_forge_cliff`: fit timed out at 5,000+ s.** Three `assign x =
  mem_r[idx]` async reads over ~120 kbit. Two of the three did convert anyway
  (see below); `edge_mem_r` is **2048x18** — exactly the shape in the last row
  of the grid — and it stayed in logic. Estimated **33,109 ALMs, 79% of the
  device.** At the 806x curve that is not a mystery, and **nobody needs to fit
  it to know.** The source shape is the diagnosis.
* **`zhao_surface_sheet`: 131,072 bits, ZERO inferred, 95,947 estimated ALMs —
  229% of the whole device**, in a block with no DSPs that had never appeared in
  any census because the DSP column is what everyone was reading.

### The one place the rule bends, and it does not help

`zhao_forge_cliff`'s `prio_mem_r` (2048x32) and `run_mem_r` (1024x17) **DID**
convert to Simple Dual Port RAM despite async reads in the source — 82,944 bits
of the expected 119,808. So Quartus sometimes rescues an async read by inserting
a read-address register. **It is not a guarantee and it cannot be planned
against**: same file, same tool, same run, one of the three refused. Write the
synchronous read; do not hope for the rescue.

### Two-port templates REPLICATE

Every `p2` row infers exactly **twice** the bits of its `p1` sibling. Two
independent read addresses plus a write is three ports, an M10K has two, so
Quartus builds two copies. That is the right answer and it is worth budgeting
for: a second read port doubles the M10K count, it does not come free out of
true-dual-port.

### The check, before writing the RTL

1. Read it **synchronously** — `always_ff @(posedge clk) q <= mem[addr];`
2. Do **not** touch the array from a reset branch. M10K contents are undefined
   after reset anyway; use a `valid` bitmap, which is one register per entry
   and clears in a cycle.
3. Then **measure that it took.** `Total block memory bits` in the map summary,
   or `Total RAM Blocks` in the fit summary. Zero is a failure.

> Evidence: `tools/budget/calibration.json`, and the tool's own console output
> in `runs/CLAUDE-RUNS/RUN-20260823-2226-budget-audit-wave1/calib_map.log`.
> Generated by `tools/budget/gen_calib.py`; re-runnable with
> `tools/quartus/run_calib.ps1 -Family ram`.

---

---

## Addendum: the same trap exists outside Quartus

Entry 3 above — an attribute accepted and silently ignored — is not a Quartus
quirk. It is a class, and it appeared twice in one night with two different
tools.

**yosys `cutpoint` without `opt_clean` is a silent no-op.** Cutting the shared
multiplier's product wire for the Field engine's bounded proof produced a
**byte-identical model** — the cut selects the wire, but the now-dead multiplier
stays in the design until `opt_clean` removes it. No warning. The only symptom
was a proof that did not get faster. Verified by counting cells: **4 `$mul` → 1**
only once both passes ran.

**So the rule for any directive that is supposed to change the hardware:**
measure that it did. A DSP count that will not fall, a model that is
byte-identical, a proof that does not speed up — these are the only symptoms you
get, because the tool will not tell you it ignored you.

## The pattern, stated once

Seven of the eight were printed by the tool, in plain text, in runs nobody read.
§5 was invisible until the number was compared against a target.

**Run the fit. Read what it says.** A green differential and a clean mutation
sweep say the logic is right; they say nothing at all about whether the thing
can be built.
