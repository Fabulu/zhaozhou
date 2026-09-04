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

## 4a. `foreach` is rejected outright

Measured 2026-09-03 with a probe module, while composing the production-only
resource top. `foreach` is a reserved keyword the pinned parser does not
implement:

    always_comb foreach (arr[k]) arr[k] = src[k];
    Error (10170): syntax error near text: "foreach"; expecting "@", or an
    identifier ("foreach" is a reserved keyword)

Verilator accepts it. Write the indices out, or drive the array from a
generate loop. A GENERATOR that emits array drives must emit explicit indices
-- `tools/quartus/gen_prod_top.py` does, for this reason.

## 4b. A unary minus applied to a size cast is rejected

Measured the same day, and this one is easy to write by accident because the
cast itself is fine:

    dndx_c = (-72'(cy_by)) * 72'(va_i);   // Error (10170) near "'"
    dndx_c = ((-(72'(cy_by)))) * 72'(va_i);   // accepted

The parser reads `-72` as the start of a sized literal and then chokes on the
quote. **Parenthesise the cast, then negate.** Nine instances, in
`zhao_post_gather`, `zhao_field_v3_spline`, `zhao_geom_attrsetup` and
`zhao_raster_attrstep`.

What IS accepted, so nobody "fixes" these: sized signed literals (`17'sd128`),
size casts of arbitrary expressions (`17'(lim)`), functions with `output`
arguments, and `'{default: ...}` assignment patterns. All four were suspected
and all four are fine -- which is the argument for the probe rather than for
theorising: two of my guesses were wrong.

## THE META-LESSON, 2026-09-03

Gotchas 1, 4 and 8 in this very file are three of the five faults that stopped
the first production-only fit. **They were already written down here, and I
rediscovered them with a synthesis probe anyway.** That is the CLAUDE.md rule
about instructions not being delivered until they are read, costing an hour.

**Read this file before touching RTL that has never been through the fitter.**
The blocks that break are always the ones only Verilator has ever seen.

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

## 10. Storage inference is a LAW with three independent killers, and the penalty is superlinear

Every entry above was found by being surprised by one block. This one was
**measured on purpose**, with a controlled grid of generated microbenches
(`tools/budget/gen_calib.py`, mapped by `tools/quartus/run_calib.ps1`), because
`zhao_field_seq` reporting **zero M10Ks while spending 8,901 ALMs** deserved an
explanation better than "Quartus did not feel like it".

Thirty-four RAM templates — four shapes x sync/async read x reset/no-reset x
one and two read ports, plus two byte-enable variants — on `5CSEBA6U23I7` under
this project's own settings. All 102 benches in the grid completed:

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

> **Synchronous read, no reset touching the array, and no byte enables — it
> infers, and costs tens of ALMs. An ASYNCHRONOUS READ, a RESET on the array,
> or BYTE ENABLES — ZERO memory bits. The three conditions kill inference
> INDEPENDENTLY; any one alone is sufficient, and having several is no worse
> than having one.**

Read the `sync + RESET` and `ASYNC + no reset` columns against each other: they
are the same number to within a rounding of the estimator. Neither is a
degradation of inference. **Both are its complete absence.**

### The third killer: byte enables

Added after the grid finished, because it was not expected and it is the one
most likely to be written by accident:

| template | memory bits | est. ALM |
| --- | ---: | ---: |
| `1024x32`, sync, no reset, 1 port | **32,768** | **44** |
| `1024x32`, sync, no reset, 1 port, **byte enables** | **0** | **22,583** |
| `2048x32`, sync, no reset, 1 port, **byte enables** | **0** | **45,134** |

Same read style, same reset behaviour, same depth and width. The only
difference is writing

```systemverilog
if (be_i[0]) mem[waddr_i][ 7: 0] <= wdata_i[ 7: 0];
if (be_i[1]) mem[waddr_i][15: 8] <= wdata_i[15: 8];
...
```

instead of `mem[waddr_i] <= wdata_i;` — and the memory disappears. **45,134
ALMs for a 65,536-bit buffer is more than the entire device.**

M10Ks have byte-enable support in hardware; Quartus 17.0.2 Lite does not infer
it from this template. **This matters because a byte-enabled write is exactly
how a blit buffer or a framebuffer wants to be written**, and
`design/budgets/dsp.md`'s own history records `zhao_cmd_dma`'s `blit_buf`
blocking the composed fit on Error 276003. If a byte-enabled memory is needed,
instantiate `altsyncram` explicitly rather than hoping — and then check the
memory-bit count, because hoping is what this whole entry is about.

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
  **FIXED 2026-08-24 (RUN-20260824-0317).** Exactly ONE of the three killers
  applied and it was the byte enables — the read was already synchronous and the
  array already deliberately unreset, and the block's own header said so. The
  16-bit word was split into the two 8-bit planes the oracle already uses
  (`zref::surface::Sheet` is `uint8_t tag[4096]; uint8_t strength[4096]`), so
  each array is written WHOLE under its own enable and no part-select remains.
  Map-only, same tool, same device, `rtlCleanAtHead`:

  | | before | after |
  | --- | ---: | ---: |
  | `blockMemoryBits` | 0 | **131,072** |
  | `inferredMemoryCount` | 0 | **2** |
  | `registers` | 131,258 | **170** |
  | `estimatedAlms` | 95,947 | **279** |
  | map seconds | 1,095.8 | **32.5** |

  **344x**, which sits on the penalty curve above rather than beside it. Note
  the SYNTHESIS TIME fell 34x too: the missing 1,063 seconds were Quartus
  building 131,072 flip-flops and the mux trees behind them.

### The one place the rule bends, and it does not help

`zhao_forge_cliff`'s `prio_mem_r` (2048x32) and `run_mem_r` (1024x17) **DID**
convert to Simple Dual Port RAM despite async reads in the source — 82,944 bits
of the expected 119,808. So Quartus sometimes rescues an async read by inserting
a read-address register. **It is not a guarantee and it cannot be planned
against**: same file, same tool, same run, one of the three refused. Write the
synchronous read; do not hope for the rescue.

**And 2026-08-24 (RUN-20260824-0317) found WHY the third one refused, which
makes this less mysterious and more useful.** The three tables differ in exactly
one structural way:

| table | shape | write sites | inferred |
| --- | --- | --- | --- |
| `prio_mem_r` | 2048x32 | 1, whole word | **yes** |
| `run_mem_r` | 1024x17 | 2, both whole word | **yes** |
| `edge_mem_r` | 2048x18 | 2, **one PARTIAL** | **no** |

```systemverilog
edge_mem_r[mhead_r][5:0] <= mtake_r;   // the only difference
```

**The rescue survives an async read. It does not survive a byte enable.** So the
two killers are not merely independent, they are of different strength: the
async-read killer is the one Quartus sometimes forgives, and the byte-enable one
is not forgiven at all. That is a rule with a mechanism behind it, and it means
the async read should still never be written — but when you are looking at a
block with several arrays and only some of them infer, **look for the
part-select first**.

### The grid did not cover the template real blocks actually use

Added 2026-08-24, because the 34-template grid above turned out not to answer
the question `zhao_surface_sheet` was about to ask. **Every `sync` point in it
writes in one `always_ff` and reads in ANOTHER, with no read enable:**

```systemverilog
always_ff @(posedge clk) if (we_i) mem[waddr_i] <= wdata_i;
always_ff @(posedge clk) rdata_o <= mem[raddr_i];
```

Real blocks do not look like that. They share ONE process — which is what makes
read-during-write return the OLD word, a semantic several contracts here state
rather than leave to the synthesiser — and they gate the read with an enable so
a stalled response does not lose its word. Neither variation had been measured,
and the repo's two working exemplars both dodge the question (`zhao_dc_sdp_ram`
splits the processes; `zhao_raster_tilestore` carries an explicit same-address
bypass, i.e. it does not rely on inferred read-during-write at all).

Four points at 8192x8 — one byte plane of SURFACE.SHEET at `Slots = 2`:

| template | memory bits | est. ALM | registers |
| --- | ---: | ---: | ---: |
| `calib_ram_8192x8_shared_re` (shared process, read enable) | **65,536** | 23 | 0 |
| `calib_ram_8192x8_shared_nore` | **65,536** | 22 | 0 |
| `calib_ram_8192x8_split_re` | **65,536** | 23 | 0 |
| `calib_ram_8192x8_split_nore` | **65,536** | 22 | 0 |

All four infer, as `ALTSYNCRAM [AUTO Simple Dual Port 8192x8]`. **Neither the
shared read/write process nor the read enable is a fourth killer, and neither
costs anything.** So a block that needs read-during-write to return the pre-write
word can have it for free, in one `always_ff`, with **no bypass network** — worth
knowing, because building the bypass "to be safe" costs a mux on the read path
and a comparator on the address, and this measurement says it buys nothing.

Generated by `tools/budget/gen_calib.py` (family `ram_rdw`); re-runnable with
`tools/quartus/run_calib.ps1 -Family ram_rdw`.

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
3. Do **not** write it with per-byte enables in RTL — and that includes ANY
   part-select on the left of a write to an array element, which is the same
   thing wearing different syntax. If the fields are written independently,
   **split the array into one per independently-written field**; that is free,
   needs no vendor primitive, and is what SURFACE.SHEET and FORGE.CLIFF both
   did. Instantiate the megafunction only if a genuine sub-field write of one
   logical word is unavoidable.
4. Then **measure that it took.** `Total block memory bits` in the map summary,
   or `Total RAM Blocks` in the fit summary. Zero is a failure.

> **`tools/budget/scan_rtl.py` could not check point 3 at all until
> 2026-08-24**, and was wrong about point 2. It had no byte-enable detector in
> 1,577 lines, so it reported the repository's worst block as healthy; and its
> `resetTouched` walked the whole `IF` including the ELSE, so every array
> written in the operating logic of any `always_ff` with an async reset read as
> reset-touched. Both are fixed and both now carry positive controls in
> `runs/CLAUDE-RUNS/RUN-20260824-0317-.../validate_scan_rtl_fixes.py`. The
> second fix was WRONG on the first attempt, instructively: taking `thensp`
> looks obviously right, and Verilator's elaborated AST folds `if (!rst_n)` by
> SWAPPING the arms, so `thensp` is the WORKING branch. The controls caught it.
> Read the tree, not the source, when the tool reads the tree.

> Evidence: `tools/budget/calibration.json`, and the tool's own console output
> in `runs/CLAUDE-RUNS/RUN-20260823-2226-budget-audit-wave1/calib_map.log`.
> Generated by `tools/budget/gen_calib.py`; re-runnable with
> `tools/quartus/run_calib.ps1 -Family ram`.

---

## 11. The fit reads the LIVE working tree, so editing during a fit rewrites what was measured

`run_block_fit.ps1` names every source in the generated QSF by **absolute path
into the working tree**. Nothing is copied into the workspace. So a file edited
while a fit is running is the file the fit elaborates — and the row it writes
still carries `sourceCommit = <HEAD>` as though it described that commit.

**MEASURED 2026-08-24.** An edit to `zhao_field_normalize.sv` landed at 10:33:06
during a 90-minute `zhao_field_seq` fit whose map report was written at 10:34:47
— **101 seconds later**. Afterwards there was no way to establish which version
had been elaborated, so the fit was killed and discarded. It may well have been
perfectly good; it could not be *shown* to be. That is the expensive way to
learn this, and it is the same failure the ticks-suffixed workspace fixed for
logs: not a wrong measurement, an unprovable one.

**Now enforced.** The flow hashes every source named in the QSF at the moment it
writes the QSF, again the instant `quartus_map` finishes, and once more at the
end. Any difference sets `status = contaminated:source-changed-during-fit`,
suppresses the resources, and — because the check runs *before* the summary is
parsed — such a row can never reach `status = 'ok'` nor be merged into the
census. The previous good measurement is kept rather than erased.

**Both detection arms carry a positive control** (`zhao_audio_fifo`, 2026-08-24):

| arm | control | result |
| --- | --- | --- |
| start-vs-end | `zhao_debug_counters.sv` edited after the QSF, left edited | **CONTAMINATED**, ALM suppressed |
| during-map | `zhao_input_rumble.sv` edited after the QSF, **reverted after the map** so the tree ends byte-identical | **CONTAMINATED** |
| negative | three clean runs | `ok`, `sourcesHashed: 27` |

The during-map arm is the one worth having: at the end of that run the file was
byte-identical to its committed form, so a start-vs-end check alone would have
called it clean.

**Three attempts at the control tested nothing, each silently.** The first
edited at a fixed `t=8s`, before the QSF existed. The second polled for the QSF
and matched a **stale workspace** — 36 orphaned `zhao-block-fit-*` directories
had accumulated in `%TEMP%` — so it fired at `t=0.5s`, again before the run had
hashed anything. The third baselined the workspaces correctly but edited
**terrain files, which are not in the 27-file shell cone at all**; the guard
ignored them because it is supposed to. `sourcesHashed: 27` in the row is what
made that legible, and it matches `grep -c SYSTEMVERILOG_FILE` on the shell QSF
exactly.

> **A control that reports "not detected" is not evidence the detector is
> broken.** Three times running it was evidence the control was.

**Still not airtight, stated rather than hidden:** an edit made *and undone*
entirely inside the elaboration window is invisible. Only copying the sources
into the workspace would close that, and the one `` `include `` in the tree
(`sdram_params.svh`) means a copy must preserve directory structure. Not free,
and not yet done.

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

---

## 12. A pair/leaf fit in a mostly-empty device measures ROUTING, not the design

**MEASURED 2026-08-24, and it invalidated a ranking I had already acted on.**

Four renderer pair wrappers were fitted to rank them for pipelining:

| pair | Fmax |
| --- | ---: |
| TESS+NORMALS | 31.10 |
| TMU+CACHE | 37.25 |
| FRAGMENT+TILESTORE | 55.52 |
| SETUP+BINNER | 88.79 |

I read the spread as a property of the blocks — specifically that the slow ones
carry control state while the fast one is pure arithmetic. Then the critical
path report arrived:

    Data Delay              30.000 ns
    Number of Logic Levels  10

**Three nanoseconds per logic level.** Cyclone V logic is ~0.3–0.5 ns/level. The
clock path spent **66% of its delay in interconnect**. That is not a deep
datapath; it is a **1,523-ALM design placed in a 41,910-ALM device** with virtual
pins on every port. Nothing pressures the fitter to pack it, so it sprawls and
the router takes long routes between the pieces.

**So the number characterises the placement, not the logic.**

### What this does and does not invalidate

* **Relative ranking between pairs is still weak evidence** — all four were fitted
  the same way, so all four are inflated by the same mechanism. But they are not
  inflated by the same AMOUNT: sprawl depends on size and shape, and the fastest
  pair (SETUP+BINNER, 1,405 ALM) is nearly the same size as the slowest
  (TESS+NORMALS, 1,523 ALM), which is exactly what makes the comparison
  untrustworthy rather than merely imprecise.
* **The composed shell fit is NOT affected.** It is 7,442 ALMs of genuinely
  connected logic with a real top level, and its critical paths run between named
  blocks rather than across empty fabric.
* **DSP and memory counts are unaffected** — those are inference results, not
  placement results.

### The rule

> **A leaf or pair Fmax is an upper bound on nothing and a lower bound on the
> design's real speed.** Treat it as a screening number that can only say "this
> block is not obviously catastrophic". To compare two blocks, either compose
> them into something that fills a meaningful fraction of the device, or accept
> that the comparison is between two placements rather than two designs.

The audit asked for pair fits because leaf fits carry ~1,000 fictional virtual
pins. That was right and the pairs did fix it — TESS+NORMALS went 1,045 pins to
67. **It fixed the boundary problem and left the sprawl problem**, and I did not
notice until I read a path.

## A single "no quartus process" sample is a FALSE NEGATIVE

2026-09-01, and it cost a fit.

`tasklist | grep -ci quartus` returned **0** while `quartus_fit.exe` was alive
and 30 minutes into its work. The run was declared dead and relaunched, giving
**two concurrent fitters writing the same project directory**:

    PID 17780  started 22:38:15  57.0 CPU-min   <- first launch, never died
    PID  6616  started 22:49:40  43.4 CPU-min   <- the relaunch

Both were killed and the round restarted clean, because a mixed `output_files/`
is worse than no result: the reports would have been a blend of two placements
with no way to tell which number came from which.

**The window is real.** `run_shell_fit.ps1` runs several executables in
sequence -- `quartus_map`, then `quartus_fit`, then `quartus_sta` -- and between
any two there is a genuine interval with no `quartus*` process at all. Sampling
inside that gap reports zero for a perfectly healthy run.

### The rules

* **Never act on one sample.** Require several consecutive misses. The automated
  monitor already debounced 6; the MANUAL check did not, and that is what
  failed. The discipline has to apply to both.
* **Check identity, not just count.** `Get-Process quartus*` with `Id` and
  `StartTime` distinguishes "dead" from "between stages" from "there are two of
  them", which a count never can.
* **Kill the WRAPPER too, not only the tool.** The `run_shell_fit.ps1` shell
  outlives `quartus_fit` and launches the next stage the moment it exits.
  Killing the fitter alone leaves a process that will start `quartus_sta` on a
  half-finished database. This is CLAUDE.md's "stopping an agent does not stop
  its background work", in the build lane.
* **A `Get-CimInstance ... CommandLine -like "*run_shell_fit*"` query MATCHES
  ITSELF**, because the pattern is in its own command line. It will kill its own
  shell, return exit 255, and report a new PID every time it is run, which looks
  exactly like a process respawning. Exclude `$PID`.

### Recovering afterwards

Restore the tracked reports before relaunching, or the previous round's
committed evidence stays overwritten by a contaminated partial run:

    git checkout -- reports/characterization reports/synthesis reports/timing
    rm -rf fpga/quartus/shell_fit/output_files fpga/quartus/shell_fit/db

## The ~40 minutes before elaboration is the HEAD snapshot, not a hang

Measured 2026-09-02, after twice suspecting a stalled fit.

`run_shell_fit.ps1` guarantees the fit builds from committed HEAD rather than
the working tree, by `git archive --format=zip HEAD` into a temp workspace and
extracting it. That is the right guarantee -- a fit from a dirty tree produces a
number belonging to sources no commit records -- but it is not free:

    ~250 MB, ~3300 files extracted per run
    repo pack is only 81.6 MiB, so the ZIP is not the cost
    file-by-file creation on Windows (AV scanning each) is

The script already replaced `Expand-Archive` with `System.IO.Compression.ZipFile`
for this reason; its own comment says the cmdlet "takes tens of minutes on a tree
this size". The remaining cost is the file writes themselves.

**So a fit showing only the two parity PASS lines, with a live wrapper and no
`quartus*` child, is NORMAL for the first ~40 minutes.** Check the workspace is
growing before concluding anything:

    Get-ChildItem $env:TEMP -Filter "zhao-shell-fit-*" -Directory |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1

A recent `LastWriteTime` and a rising file count is a healthy extraction.

### Stale workspaces accumulate when a fit is killed

The cleanup is in a `finally`, so killing the wrapper skips it. Thirteen dead
workspaces had accumulated -- 1.43 GB. Sweep them when a run is killed, keeping
only the newest if one is live.

### Not changing this mid-sequence, deliberately

The obvious optimisation is to archive only the paths the fit reads (`fpga/`,
`tests/CMakeLists.txt`, `tools/quartus/`) instead of the whole tree, which would
cut the snapshot to a few MB.

It is NOT being done during an active comparison sequence. Rounds 1-14 all used
the same apparatus, and swapping the snapshot mechanism between rounds would put
a methodology change inside a series whose whole value is that its numbers are
comparable. A broken or subtly different fit also costs an hour to discover,
against the ~40 minutes it saves. Do it between passes, then re-fit one
unchanged commit to confirm the number is identical before trusting the series
across the change.

## `run_block_fit -Module` alone cannot fit a NEW block

2026-09-02, after a 139-second fit that reported `failed:quartus_map` and looked
like a synthesis error in the design.

It was not. The generated QSF is built from the **shell** project's source list
with `../../rtl/` rewritten to an absolute path:

    $qsf = $qsf -replace '\.\./\.\./rtl/', "$rtlAbs/"
    $qsf = $qsf -replace '^set_global_assignment -name TOP_LEVEL_ENTITY.*', "... $mod"

So `-Module zhao_texture_tmu_plan` set the top-level entity to a module that
**no source file in the list defines**, and quartus_map failed with the block
looking guilty. Any block outside the shell cone -- every new one -- needs its
sources passed:

    run_block_fit.ps1 -Module zhao_texture_tmu_plan `
      -ExtraSources fpga/rtl/texture/zhao_texture_tmu_plan.sv

and every sub-module it instantiates, in the same list. The parameter's own
comment says this ("blocks that are NOT in the shell cone ... can be
characterized with the same flow"); it just does not fail in a way that says so.

**The tell:** a `failed:quartus_map` with no error text in the log, and no
per-block workspace directory created under the temp workspace. A genuine
synthesis error leaves both.

---

## 13. The live-tree rule covers Quartus SOURCES and says nothing about the fit lane's own CONFIG

§11 is about `.sv` files: a running fit reads the working tree, so editing a
source rewrites what was measured. **That rule has a blind spot, and it cost
three block fits on 2026-09-04.**

`run_block_fit.ps1` re-reads `design/fit_targets.yml` **once per block**, at each
preflight. So the config is a live-tree file too — and it is one that gets
edited far more casually than RTL, because editing a YAML "does not touch the
design".

### What happened

The island campaign was launched with nine modules. `design/fit_targets.yml` was
being edited in place during the run (rules rewritten, a new target added). At
11:49 a preflight threw:

    preflight: no source names `module zhao_texture_tmu_pipe`.

`zhao_texture_tmu_pipe` **is** a declared target and its `.sv` **does** declare
that module — before the run and after it. The complaint was true for the
instant the reader saw the file.

**`io.open(path, 'w')` truncates before it writes.** Any reader opening the file
in that window gets an empty or partial one. The hand-rolled parser then finds
no sources for the target, and the preflight refuses — correctly, on what it
was shown.

### And the blast radius was the whole campaign

`run_block_fit.ps1:297` wraps the entire module loop in ONE `try`, so a throw in
block seven ended blocks seven, eight and nine. The report then carried six rows
where nine were asked for. **Silence and absence look identical in that file**:
nothing says "three were never attempted".

### The rules

1. **Never rewrite a config in place while a process polls it.** Write a
   temporary file and `rename` — rename is atomic, truncate-then-write is not.
2. **Treat `design/fit_targets.yml` as inside the running fit's closure**, the
   same as the `.sv` files it names. It is read later and more often than they
   are.
3. **Read the `.err` file.** The stdout log ended cleanly after the previous
   block and looked like a queue that had finished its list. The cause was in
   the stderr file, unread for an hour, and the first written diagnosis blamed
   an unrelated `failed:structure` in the stdout log.
4. **A per-block fit is independent by construction.** Chaining blocks inside
   one process gives that up for nothing; invoking the script once per module
   makes one block's failure cost exactly one block.

---

## 14. What actually decides whether an array becomes an M10K

Measured 2026-09-04, after **two** plausible explanations for D19m turned out to
be wrong. Both had a counterexample sitting in this repository, and finding each
one took a single grep.

### The two wrong answers, recorded because they are the obvious ones

**"It is multidimensional."** `zhao_texture_fragrob` declares
`desc_u_m [3][DEPTH]` and friends and measured **13 M10K, 6,464 memory bits**.
Multidimensional arrays infer here. Quartus's *"cannot regroup multidimensional
array"* message is real but is not this.

**"The read is asynchronous."** `zhao_audio_fifo` reads its 65,536-bit `mem`
through a bare `assign rd_word = mem[rd_ptr];` and measured **7 M10K with 292
registers**. Asynchronously-read arrays infer here too.

### What separates them

An M10K has an **output register**, and inference works by absorbing the
consumer's flop into it. That absorption needs the read to reach a register with
nothing but wiring in between:

    zhao_raster_tilestore   ram0_q  <= ram0[b0_raddr];        -> M10K
    zhao_audio_fifo         pcm_l_o <= rd_word[15:0];         -> 7 M10K
                                       (assign rd_word = mem[addr])

    zhao_texture_tmu_pipe   dec_clut565_c =
                              decode16(pal_dat_r[way][idx], FMT_RGB565);
                              ^^^^^^^^ a function, then a mux, THEN a flop
                                                              -> 65,536 FLOPS

**Combinational logic between the array read and the first register blocks the
absorption**, and the array falls back to flip-flops. That is the discriminator
that survives all four blocks above.

`zhao_raster_tilestore.sv`'s own comment names the working idiom in passing --
*"`ram0_q <= ram0[addr]` puts…"* -- which is what the pattern looks like when
somebody got it right on purpose.

### Why it matters more than the folklore

The cost of believing either wrong answer is a redesign aimed at the wrong
property: reshaping an array that was never the problem, or adding a pipeline
stage where a register move would do. In `tmu_pipe` the fix is to **register the
raw word out of the array and decode on the next cycle** — the register being
added is the one the M10K supplies for free.

**Stated as a limit, not a law.** Four blocks agree with it; that is consistent,
not proven. Quartus's inference has more conditions than this (write-port count,
reset style, initial values), and the honest use of the rule is *"check what sits
between the read and the flop FIRST, because it is cheap and it explained every
case here"* — not *"this is the only thing that can matter."*
