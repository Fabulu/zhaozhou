# SPEC v1: SURFACE.STAMP — take the multiplier farm out of a block that stamps once per 83 clocks

**Run ID:** RUN-20260823-1415
**Created:** 2026-08-23 14:15 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`zhao_surface_stamp` fits at **28 DSPs** — measured, `96c0394`,
`rtlCleanAtHead: false`, 950 ALMs / 493 registers / **no Fmax recorded**. Bring
it to **0–2 DSPs** without changing a single output bit against the shipped
oracle, and leave behind a **parameterised frontier** rather than one point.

Success:

- 0 DSPs, argued from a **derived frame demand** (20,000 stamp texels/frame)
  rather than from the ledger's one-clock placeholder;
- a `SQ_RADIX` parameter with **measured constrained fits at more than one
  setting**, so the frontier is data;
- a differential against `zref::render::stamp_surface` through the proven view
  that is bit-identical on every case the old one covered;
- a mutation sweep with forced regeneration and a binary-hash check, run in a
  worktree, with a **non-zero verified mutant count** in the preflight;
- `ctest -L fast` green, ledger no worse than the one-error baseline;
- **Fmax and texels/frame reported for every point**, including a re-measured
  baseline, because the shipped row has no Fmax at all.

---

## Step 1 (ledger rule V17): the oracle resolves — CHECKED FIRST

Run before any RTL was written, 2026-08-23 14:5x:

    npm run ledger:check
    => CHECK FAILED — 1 error(s) against 92 blocks / 40 ops
       V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal
            "tests/formal/field_seq_bound.sby" is recorded as "pending"

**That is the Field agent's gate and not mine**, and it is the same single
error the GEOM.SKIN run baselined against. **V17 is green for this block**, and
I checked the symbols by hand rather than trusting the aggregate:

| cited symbol | resolves to |
| --- | --- |
| `zref::surface::blend_apply` | `reference/include/zref/zref_surface.hpp:215` |
| `zref::surface::stamp_apply` | `reference/include/zref/zref_surface.hpp:392` |
| `zref::surface::stamp_apply_field` | `reference/include/zref/zref_surface.hpp:443` |
| `zref::render::stamp_surface` | `reference/src/zrender/terrain.cpp` (the executed law) |

The oracle resolves. **Step 1 is clear.** The V16 error is this run's ledger
baseline; a second error after my commits is mine.

---

## Where the 28 DSPs actually are — six multiplies, all of them 64-bit

Read out of `fpga/rtl/surface/zhao_surface_stamp.sv` directly. There is no
multiply in the material conversion, none in the blend, and none in the
age/decay path — `zhao_surface_blend` is 82 lines of adders, a comparator and a
barrel shifter and it fits at **64 ALMs / 0 DSPs** on its own. Every DSP in this
block is in the **coverage geometry**:

| # | expression | line | operands | rate |
| ---: | --- | --- | --- | --- |
| 1 | `r_outer2_c = rad_ext * rad_ext` | `rad_ext * rad_ext` | signed 64×64 | once per stamp |
| 2 | `r_inner2_c = rin_ext * rin_ext` | `rin_ext * rin_ext` | signed 64×64 | once per stamp |
| 3 | `numx = 41'(st_spanx) * 41'(two_i_1)` | — | signed 41×41 | **every clock** |
| 4 | `numz = 41'(st_spanz) * 41'(two_j_1)` | — | signed 41×41 | **every clock** |
| 5,6 | `d2 = dxw * dxw + dzw * dzw` | — | two signed 64×64 | **every clock** |

The contract's own synthesis note predicted exactly this shape and called the
per-cycle squares "honestly oversized". It is worse than oversized: **two of the
six are per-stamp constants that Quartus still gave silicon to**, because they
are written as combinational expressions off the command port.

`reports/QUARTUS_GOTCHAS.md` §5 is the whole story of the width: 72-bit operands
bought 28 DSPs where a proven 40-bit width bought 18. This block reaches 28 from
the other direction — six multiplies at 41 and 64 bits.

**None of this is behaviour.** The five stamp modes, the material conversion,
the age/decay shift and the capture-exact ordering are all downstream of the
coverage bit and are not touched by anything in this spec.

---

## The rate derivation, which is the whole argument

Full working in `docs/OWNER_DOCKET.md`, "THE THREE DEMAND NUMBERS", derived from
Sacrifice's own SCAR system.

| quantity | value | source |
| --- | ---: | --- |
| `gpu_clk` period | 10.000 ns (100 MHz) | `fpga/quartus/shell_fit/zhao_shell_fit.sdc:4` and the per-block SDC |
| clocks per 60 Hz frame | **1,666,667** | 100e6 / 60 |
| god-scar size | 128×128 texels | Sacrifice's five elemental scars |
| land tile | 64×64 texels | `terrain_rules` §1.3 / charter §12 |
| tiles dirtied per impact | ≤ 3×3 = 9 → **36,864 texels** | 128×128 scar over 64×64 tiles |
| heavy barrage | 10 impacts/s → **6,144 texels/frame** | owner |
| **DERIVED DEMAND** | **20,000 texels/frame** (3× heavy) | owner |
| **clocks available per texel** | **83.3** | 1,666,667 / 20,000 |

The shipped block issues **six 64-bit multiplies to place one texel per clock**.
The demand is one texel per 83 clocks. **Over-provisioned by ~83×** on the
per-cycle path, and infinitely on the two per-stamp constants, which did not
need a rate at all.

Note the units carefully: a `SurfaceStamp` command **visits all 4,096 texels of
its patch** whether they are covered or not (chosen law S2 — the field brush
consumes one record per *visited* texel). So "36,864 texels per impact" is the
*visited* count for the 9 dirtied tiles, and the 20,000 figure is a visited-texel
budget. That is the number the cursor loop must sustain: **~4.9 stamps/frame**.

**Beware the other "gpu cycles".** `design/budgets/latency.md`'s
`frame_gpu_cycles` = 251,520 is the **raster period**, not the compute budget.
The compute budget is 1,666,667. They differ by 6.6×, and using the wrong one
here would make a 37-cycle texel look like a failure.

---

## The architecture

### 1. The two per-cycle multiplies for the texel centre become an ACCUMULATOR — exactly, not approximately

`numx = spanx * (2i + 1)`. As `i` advances by one, `numx` advances by `2*spanx`.
So the multiply is a first-order recurrence:

    at i == 0:  numx <= spanx
    on i++   :  numx <= numx + (spanx << 1)

This is **bit-identical to the multiply for every input, inside the domain and
out of it**, not merely inside: the shipped form truncates the product to 41
bits, and an accumulator that adds `2*spanx` into a 41-bit register produces the
same residue mod 2⁴¹. `numz` is the same recurrence on `j`, stepping once per
row. **Two multiplies, gone, with no domain argument required.**

### 2. The two per-cycle squares and the two per-stamp squares share ONE SEQUENTIAL SQUARER

Four squarings remain: `r*r`, `r_inner*r_inner`, `dx*dx`, `dz*dz`. All four are
*squares*, and a square is `|v|²`, so one unsigned shift-add engine serves all
four. **One shared unit, local to this block** — the standing ruling is share
within a subsystem only, smallest local farm, and this is the smallest one there
is.

    m      = |v|                       (36 bits: dx/dz are signed 36 today)
    acc    = 0
    repeat: if m[k] then acc += m << k          (accumulated mod 2^64)

`acc` is 64 bits, which is exactly the width the shipped `dxw*dxw` product is
truncated to, so **the sequential result is the low 64 bits of the true square —
bit-identical to the shipped multiply for every input**, including `v = -2³⁵`
where both give 0. No domain narrowing is required and none is done. *(Recorded
deliberately: the contract flags narrowing to the ±4,096 m domain as "a real
optimisation and a real risk". It is not taken. The DSPs come out without it,
so paying that risk would buy nothing.)*

Implementation detail that matters for the DSP count: the addend is a **left-
shifting register**, not a barrel shift by a counter, so there is no wide
shifter and — critically — **no `*` operator survives anywhere in the file**.
`reports/QUARTUS_GOTCHAS.md` §3: `(* multstyle = "logic" *)` is accepted and
silently ignored, so the only reliable way to not get a DSP is to not write a
multiply.

### 3. The cursor loop becomes multi-cycle; nothing else moves

The two-stage sheet pipeline (`s1` read-in-flight, `s2` blend/write/result),
the ACQUIRE-before-any-write structure (S4), the ready/valid discipline on all
three channels, the scan order, the counters and the `stamp_results` stream are
**unchanged**. The only change is that `advance` now additionally requires the
geometry engine to have produced `d2` for the current cursor.

Sub-sequence per cursor position:

| step | cycles | when |
| --- | ---: | --- |
| square `dz` for the row | `CYC` | only at `i == 0` |
| square `dx` for the texel | `CYC` | every texel |
| evaluate `d2`, compare, advance | 1 | every texel |

so the initiation interval is `CYC + 1 + CYC/64`.

### 4. The parameter and its legal settings

`SQ_RADIX` = bits retired per cycle, implemented as a chain of `SQ_RADIX`
conditional 64-bit adds:

| `SQ_RADIX` | `CYC` = ceil(36/R) | adders in the cone | predicted II/texel |
| ---: | ---: | ---: | ---: |
| 1 | 36 | 1 | **37.6** |
| 2 | 18 | 2 | **19.3** |
| 4 | 9 | 4 | **10.1** |

Only powers of two are legal (the shift register steps by `SQ_RADIX`); the
elaboration refuses anything else.

### 5. The frontier this predicts — to be replaced by measurement

texels/frame = Fmax / 60 / II. At the 100 MHz placeholder:

| `SQ_RADIX` | II | texels/frame @100 MHz | vs. 20,000 demand |
| ---: | ---: | ---: | --- |
| 1 | 37.6 | 44,326 | ✓ 2.22× |
| 2 | 19.3 | 86,356 | ✓ 4.32× |
| 4 | 10.1 | 165,017 | ✓ 8.25× |
| *shipped* | *1.0* | *1,666,667* | *83×, at 28 DSPs* |

**All three settings pass**, which is a weaker frontier than GEOM.SKIN's (whose
`MUL_LANES=1` point failed and thereby showed the wall). The wall here is on the
**Fmax** axis instead: `SQ_RADIX = 4` chains four 64-bit adds combinationally
and is expected to be the slowest clock. `gpu_clk` is shared, so a point that
meets its own rate and drags the console's clock down is a losing point. **That
is the trade this frontier exists to measure**, and it is why both numbers get
reported for all three.

### 6. Rejected: the 64-entry dx² table

`dx²` depends only on `i`, so there are only 64 distinct values per stamp and a
table would restore one texel per clock at 0 DSPs. **Rejected:** 64 × 62 bits is
~4,000 flops plus a 64:1 mux of 62 bits, an estimated 2,000+ ALMs, to buy 37×
the throughput of a block that already meets its demand 2.2× over. That is the
same over-provisioning by a different resource, and the contract's "Memory
ownership: **None**" would have to be given up for it. Recorded, not done.

---

## Widths, stated before synthesis (gotcha §5)

Nothing here is narrowed, so the obligation is to show the sequential form is
not *wider* than the parallel one it replaces:

| quantity | shipped | new | note |
| --- | ---: | ---: | --- |
| `numx` / `numz` | 41 (product) | 41 (accumulator) | same residue mod 2⁴¹ |
| squarer magnitude | — | 36 unsigned | covers dx/dz (36 signed), r (32), r_inner (33) |
| square accumulator | 64 (truncated product) | 64 | same truncation |
| `d2` | 64 signed | 64 signed | unchanged |
| `r_outer2` / `r_inner2` | 64 signed | 64 signed | unchanged, now registered |

---

## Scope

**In Scope:**

- Rearchitect `fpga/rtl/surface/zhao_surface_stamp.sv`: accumulator texel
  centres, one shared sequential squarer, `SQ_RADIX` frontier.
- A new leaf `fpga/rtl/surface/zhao_surface_sq.sv` if the squarer is cleaner as
  its own module (decide during implementation; a leaf is also a formal target).
- Adapt the three test files' **timing** expectations only. Not one behavioural
  check may be weakened.
- Mutation sweep in a worktree, forced regeneration, binary-hash check, lint
  preflight with a non-zero verified count.
- Constrained per-block fits: **the pristine baseline re-measured for Fmax**,
  plus at least two `SQ_RADIX` settings.
- Update `design/contracts/SURFACE.STAMP.md` and `design/blocks.yml` — both
  `latency` and `target_throughput` become wrong the moment the RTL changes.

**Out of Scope:**

- Any change to the five stamp modes, the ABI mapping, the material conversion,
  the age/decay law, the scan order, the ordering or the counters.
- Narrowing the datapath to the ±4,096 m domain. Not needed; not taken.
- `texture_tmu` and `terrain_normals`. Queued behind this, serially.
- Particle-simulation, compositor and 2D behaviour — owner docket.
- Any claim about physical hardware. This is simulation, synthesis and fit.

---

## Constraints

- **Machine at capacity.** All design, RTL, differential and sweep work first;
  no fit until `quartus_map`/`quartus_fit`/`quartus_sta` are confirmed absent.
  (Confirmed absent at run start, 14:5x — recorded, and re-checked before each
  fit.)
- Build via presets only, env dot-sourced in the **same** shell:
  `. .\tools\env\zhao-env.ps1; cmake --preset windows-native; cmake --build build`
- Verify every fit was constrained: `Info (332111):   10.000          clk`.
  Capture the line as evidence before the harness deletes the workspace.
- Never hand-edit `reports/synthesis/zhao_block_fit.json`. Variants carry
  `variantOf: "zhao_surface_stamp"` so the V23 census excludes them.
- Ledger baseline is **one** error (V16 FIELD.SEQ.CORE). A second is mine.
- Commit the run folder as I go; push logical commits during the run.

---

## Don't Retry

*Failed approaches, so they are not re-learned after compaction.*

- **Do not trust `(* multstyle = "logic" *)`.** Quartus 17.0.2 accepts it and
  does nothing. Write shift-add, or better, write no `*` at all.
- **Do not assume a mutation sweep that reports a score actually ran.** Every
  sweep in this repo scored an empty set in fresh checkouts until 2026-08-23:
  CRLF made the preflight print "linted 0 mutants, 0 do not build" and exit 0.
  **Confirm a non-zero parsed mutant count** before believing any score.
- **Do not score a mutant that failed to COMPILE as caught.** The executable
  lives outside the target directory, so a failed build re-runs the previous
  binary. That inflated a real 22/23 into a reported 21/22.
- **Do not hash `V<top>.cpp`** to decide whether a model re-elaborated — it is
  byte-identical between pristine and mutant. Hash the whole model directory.
- **Do not run `cmake --preset windows-native` without dot-sourcing
  `tools/env/zhao-env.ps1`** in the same shell. "Could not use disabled preset"
  is a lie about the preset and the truth about the PATH.
- **Do not use `frame_gpu_cycles` (251,520) as the compute budget.** It is the
  raster period. The compute budget is 1,666,667.
- **Do not fit in the working tree while editing it.** The baseline Fmax fit
  runs from a pristine worktree at HEAD for exactly this reason.

---

## Open Questions

- **The shipped row has no `fmaxMhz` at all** and `rtlCleanAtHead: false`. There
  is therefore *no* honest "before" Fmax in the tree. Re-fitting pristine costs
  ~18 minutes and is the only way to report a real before/after. Doing it.
- How are the 28 DSPs split across the six multiplies? Not measurable without
  six fits, and not worth six fits: the only attribution that matters is that
  the count goes to 0. **Not claiming a split.**
- Does `SQ_RADIX = 4`'s four-deep adder chain cost more Fmax than it buys in II?
  Only the fit answers it. If it does, `SQ_RADIX = 1` or `2` is the shipped
  default and 4 is a recorded failing point — which is still coverage.
- Does the sequential squarer need its own formal target? `zhao_surface_blend`
  has one and this is arithmetic of the same kind. Deciding after the sweep;
  it is not in the success criteria above.
