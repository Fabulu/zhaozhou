# Static scan for the Field engine's timing defect elsewhere in the RTL

> **This is a source scan, not a measurement.** It predicts where deep
> combinational logic is likely; only a constrained fit with STA can say whether
> a block closes. It exists because fits are expensive — one constrained leaf
> fit costs 10–30 minutes on this machine, and there are 91 modules — so it is
> worth knowing where to look first.

## Why

`zhao_field_seq` measured **8.59 MHz against a 100 MHz target**, and TimeQuest
named a single wire at **78 logic levels**: two 64-iteration combinational loops
in `zhao_field_normalize` that unroll into 128 serially dependent
compare-and-shift stages inside one clock.

Every one of the 47 block fits before 2026-08-23 ran with no timing objective,
so **no other block has evidence either way.** The question this scan answers is
narrow but useful: does that *specific shape* — a long serially dependent
combinational chain — occur anywhere else?

## Method

Every `for` loop in `fpga/rtl` with a trip count of 16 or more, then each one
classified by hand into: sequential (reset/registered), structural (`generate`),
independent (no loop-carried dependency), constant-folding, or **serially
dependent combinational** — which is the dangerous class. Separately, a scan for
non-constant `/` and `%`, which synthesise into large combinational dividers.

## Result: the Field defect is NOT widespread in this form

| site | trip | verdict |
| --- | ---: | --- |
| `field/zhao_field_normalize.sv:173,179` | 64 ×2 | **THE DEFECT.** Serially dependent, combinational. Being replaced with the log-depth leading-zero count `zhao_field_rcp` already uses. |
| `raster/zhao_raster_edgewalk.sv:324` | 16 | **CANDIDATE — the only other one.** See below. |
| `common/zhao_crc32c_fold.sv:100` | 32 (+64) | Safe. Every `foldn` argument is a compile-time constant (`32'd1 << i`, `64'd1 << j`, `N` a genvar), so all 96 calls constant-fold; what remains is a masked XOR reduction, and XOR trees balance to log depth. |
| `memory/zhao_mem_guard.sv:71` | 64 | Safe. `mask_of[b] = (b < len_b)` has no loop-carried dependency — 64 parallel comparators, depth ~1. |
| `video/zhao_scanout_linebuf.sv:144` | 256 | Safe. Inside `initial` under `` `ifdef FORMAL `` — never synthesised. |
| `field/zhao_field_seq.sv:370,382` | 64 | Safe. Register-file reset in `always_ff`. |
| `geometry/zhao_geom_cull.sv:439` | 16 | Safe. Reset loop in `always_ff`. |
| `geometry/zhao_geom_project.sv:211` | 16 | Safe. Reset loop in `always_ff`. |
| `terrain/zhao_terrain_project.sv:313` | 16 | Safe. Reset loop in `always_ff`. |
| `geometry/zhao_geom_skin.sv:394` | 24 | Safe. Reset loop in `always_ff`. |
| `raster/zhao_raster_edgewalk.sv:285,378` | 16 | Safe. `generate` (structural) and a reset loop. |

**No non-constant `/` or `%` anywhere in `fpga/rtl`.** The last combinational
divider was removed from `zhao_geom_lod` when its 72-bit operands were narrowed
to a proven-sufficient 40 bits.

## The one candidate

```systemverilog
// raster/zhao_raster_edgewalk.sv:322
logic [4:0] row_pc;
always_comb begin
  row_pc = 5'd0;
  for (int i = 0; i < 16; i++) row_pc = row_pc + {4'd0, row_cov[i]};
end
```

A popcount of 16 bits written as sixteen **serially dependent** 5-bit adds.
Worst case that is ~16 ripple-carry adders end to end; a balanced tree is depth
4. Quartus often reassociates adder chains, so this may already be fine — which
is exactly why it is listed as a candidate and not a defect.

**Do not rewrite it on suspicion.** That is the mistake this project has made
before: `(* multstyle = "logic" *)` was applied, silently ignored, and believed
for weeks. Fit the block, read the logic-level count, and only then decide. If
it does need changing, the shape is an adder tree, and it costs no clocks.

This block matters more than its size suggests: it is on the per-pixel raster
path, so its Fmax bounds the whole rasteriser.

## What this scan does NOT cover

Long combinational chains do not need a loop. The Field engine also had
`zhao_field_rcp` fully combinational, and a 24×31 multiply plus two more
multiplies and saturating rescales in a single cycle. **A block can be nowhere
in this table and still be 12× too slow.** The only way to know is a constrained
fit with the STA stage, and the honest position remains that 47 of the project's
measurements were taken against the wrong objective.

Order to re-measure in, by expected impact: the blocks already known to carry
heavy arithmetic — `zhao_geom_skin`, `zhao_terrain_project`,
`zhao_texture_tmu`, `zhao_surface_stamp` — then `zhao_raster_edgewalk` for the
candidate above.

---

## RESOLVED 2026-09-01: the one candidate was real, and it was fixed on evidence

`zhao_raster_edgewalk.sv:322`, the serial 16-bit popcount, **is now a balanced
tree** (16 -> 8 two-bit -> 4 three-bit -> 2 four-bit -> 1 five-bit), fed from a
registered row mask rather than from the fill tests in the same cycle.

The scan's own condition for acting was met before it was touched. It says *"Do
not rewrite it on suspicion... Fit the block, read the logic-level count, and
only then decide."* The fit at `b3bd69b` named the tail explicitly --
`sx0_r[7]~DUPLICATE -> pend_r[6]`, -1.679 ns, with EDGEWALK owning **69 of the
worst 100 paths** -- and the reduction was on that path. It was rewritten
because a measurement asked for it, not because the source looked wrong.

    round  9   85.62 MHz   before
    round 10   86.48 MHz   after the ROW-B/ROW-C split + balanced tree

And the scan was right about the stakes: *"its Fmax bounds the whole
rasteriser."* EDGEWALK has been the reported owner in four separate rounds
(4, 8, 10, 11), each time with a DIFFERENT tail. Fixing one shape exposes the
next.

### What the scan could not see, and the fits found anyway

The closing caveat -- *"a block can be nowhere in this table and still be 12x
too slow"* -- was correct, and every remaining offender this pass has been of a
kind no loop-trip-count scan can find:

* a **read-modify-write loop** in FRAGMENT (rounds 1, 3)
* a **RAM-launched** combinational path in RESOLVE (round 6), where an M10K's
  register sits ~2 ns deeper than a fabric flop before any logic runs
* a **256-input AND reduction** whose result fanned back to all 256 inputs'
  next-state muxes, in EARLY-Z (round 9)
* a **register bit steering a DSP's operands in the DSP's own cycle** --
  `cross_r[47]`, the sign of the triangle area, in EDGEWALK (round 11)

None is a `for` loop. The generalisable shape is not "long loop" but **anything
that reaches a slow resource, or fans out widely, inside the cycle that consumes
it.** A future scan should look for reductions wider than ~32 inputs, RAM
outputs feeding combinational logic, and register bits in the cone of a DSP
operand -- not trip counts.

### Still un-measured, and still the right order

The scan's re-measure list stands and none of it is done: `zhao_geom_skin`,
`zhao_terrain_project`, `zhao_texture_tmu`, `zhao_surface_stamp`. None has
appeared in any shell-fit worst-100, but the shell fit does not exercise all of
them, so that is **absence of evidence, not evidence of absence** -- the same
error that had BINNER and FBWRITE named as offenders for weeks without a single
measurement putting them on a list.
