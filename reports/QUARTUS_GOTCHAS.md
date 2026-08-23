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

Six of the seven were printed by the tool, in plain text, in runs nobody read.
The seventh (§5) was invisible until the number was compared against a target.

**Run the fit. Read what it says.** A green differential and a clean mutation
sweep say the logic is right; they say nothing at all about whether the thing
can be built.
