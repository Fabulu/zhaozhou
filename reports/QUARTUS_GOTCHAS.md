# What Quartus 17.0.2 Lite does that the simulators do not

> Everything here is **simulation, synthesis or fit**. No hardware has run any of
> it.

Every entry below cost this project real time, and every one has the same shape:
**Verilator and slang accept it, the differentials pass, the mutation sweep
scores clean — and only a Quartus run says otherwise.** They were scattered
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
