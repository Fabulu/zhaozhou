# Brief: rearchitect the TERRAIN domain to spend memory and stop spending ALM/DSP

2026-09-09, rescue phase. Written after four recons, all committed under
`reports/terrain-recon/`. Read them first -- they are the evidence base and they
contain corrections to things I previously asserted.

## The owner's instruction, verbatim

> "If it all depends on terrain, have a fable agent architect terrain stuff after
> you sent out recons to send him the relevant info, and then build it. And
> architect it so it saves on the stuff we need to save and uses memory instead
> whenever possible."

And the standing goal:

> "finish all the briefs and roadmaps and start architecting optimizations until
> we crack the ceiling we need to crack and come in much below the budget
> required. Use fable architects for big rearchitectures. Don't do quartus fits
> unless you really need to, they cost a lot of time."

## The device is grotesquely asymmetric, and that IS the architecture

| resource | have | counted so far | verdict |
|---|---:|---:|---|
| ALM | 41,910 | 58,359 | **139% -- desperately scarce** |
| DSP | 112 | 192 | **171% -- scarce** |
| M10K | 553 | 147 | **27% -- abundant, ~400 free** |
| registers | (packed in ALM) | 81,925 | the third problem |

The thesis the owner set: ~80k ALM is not "this console needs 80k". It is
evidence that **architecture-as-a-collection-of-independent-engines is wrong for
this FPGA.** Target shape: *small arithmetic factories + lots of RAM +
scheduling*, not *every pipeline gets its own arithmetic + enormous distributed
in-flight state*. Spend the M10Ks like Monopoly money.

## What terrain costs today, honestly

**DSP -- terrain is 62 of 192 (32%):**

    zhao_terrain_project  33     zhao_terrain_bake  17
    zhao_terrain_tess      6     zhao_terrain_lod    3     zhao_terrain_normals 3

**ALM -- and this is the uncomfortable part: MOST OF TERRAIN IS UNMEASURED.**

| block | ALM | reg | M10K | timing |
|---|---:|---:|---:|---|
| `zhao_terrain_residency_v2` | 2,234 | 1,226 | 16 | **FAILS, -6.597 ns slack** |
| `zhao_terrain_pagestream` | 1,649 | 2,043 | 0 | 97.11 MHz (short) |
| `zhao_terrain_cmd` | 1,069 | 869 | 0 | 90.87 MHz (short) |
| `zhao_pair_tess_normals` | 1,574 | 1,577 | 1 | **31-32 MHz, see recon 3** |
| `zhao_terrain_bake` | **UNKNOWN** | | | MAPPED-ONLY: DSP known, ALM never zero |
| `zhao_terrain_velocity` | **UNKNOWN** | | | MAPPED-ONLY |
| `zhao_terrain_patch` | **UNKNOWN** | | | no row anywhere in the repo |
| `zhao_terrain_project` | **UNKNOWN** | | | 33 DSP known |
| `zhao_terrain_shade` | -- | | | **RTL NOT BUILT.** Contract REFERENCE_COMPLETE |

Do not invent numbers for the UNKNOWN rows. If a plan needs one, say which fit
would produce it. I put "1,584 ALM" for `terrain_patch` in an earlier brief and
it is **not corroborated by anything on disk** -- I made it up without noticing.

## What is ALREADY architected -- do not redo it

Two pieces landed today and are yours to build ON, not to revisit:

1. **`fpga/rtl/common/zhao_project_service.sv`** -- one shared projection core
   for both clients instead of two. -33 DSP. Its header carries a correction:
   **it must not be adopted until the arena exists**, because terrain's
   per-corner projection demand alone exceeds the frame.
2. **`fpga/rtl/common/zhao_proj_arena3.sv`** + `reports/PROJ-ARENA3-ARCHITECTURE-20260909.md`
   -- the projected-vertex arena. 4 groups x 81 rows x 3 SDP read replicas,
   106-bit record, 18 M10K, dense fill, sealed lifetime, generation-checked
   handles. Lint clean, Quartus-17 gate clean, directed test green with two
   positive controls fired. It cuts terrain projection **5.64x** (6,144 corner
   references against 1,089 unique lattice vertices per 33x33 patch).

**So `zhao_terrain_project`'s 33 DSP and its 94.4%-of-frame saturation are
SPOKEN FOR.** Take them as solved-in-design and architect everything else.

## The in-repo model you should copy

Recon 3's most useful finding: **two terrain blocks have already built exactly
what the rescue roadmap asks for.**

* `zhao_terrain_normals.sv:203` -- ONE nonconstant multiply, 33x33, **shared
  across six operand-mux cases** by an `mseq`/`m_busy` sequencer at `:239-330`.
* `zhao_terrain_lod.sv:273` -- ONE shared multiplier across square and eval.
* `zhao_terrain_tess.sv:317-333` -- a header recording that `span_mask`
  replaced a **128-multiply-site double loop** with two sites.

This is the existence proof that operand-muxed shared arithmetic works in this
codebase and in this toolchain. Prefer extending that pattern over inventing one.

## The targets, in the order I think they matter

**1. `zhao_terrain_bake` -- 17 DSP and ZERO memory bits.** Recon 1: seven
multiplier sites, and a **1,089-bit `meets_row` kept as flops BY DESIGN** per its
own header, read two rows per cycle. That header is an argument, not a law --
test it. This is the single densest arithmetic-with-no-RAM block in terrain and
the most direct instance of the owner's thesis. Recon 1 refused to split the
remaining 13 DSP without a resource map; produce that map.

**2. `zhao_terrain_residency_v2` -- 2,234 ALM, 1,226 reg, and FAILING TIMING.**
It already has 16 M10K, so the easy memory move is done. Two live problems:
the -6.597 ns slack, and an **open defect** where `statram` is short 17 bits per
entry (Quartus infers `WIDTH_A=40` against a declaration of 57, so 150,528 of
167,936 bits land in RAM and the rest is unaccounted -- not flops, not MLAB).
See `reports/RESIDENCY-V2-MISSING-BITS-20260907.md`. A directory that fails
timing is not a solved block.

**3. `zhao_terrain_pagestream` -- 2,043 registers against 1,649 ALM.** More
registers than logic, and **zero memory bits**. But recon 2 found its `buf_q[3]`
staging is genuinely *impossible* to move to M10K -- read combinationally through
a byte-lane mux the same cycle as emit, and `max_m10k:0` is a deliberate gate.
So the register count has some other home. Find it before proposing anything.

**4. The 31 MHz on the tess/normals pair -- and it is NOT the arithmetic.**
Recon 3's headline. The pair's fit receipt carries a `sources.sha256` matching
the current tree exactly, so it already describes the repaired design, and the
worst path (-20.21 ns) launches from the **bench wrapper's** `lat_mem`
write-enable register into `zhao_terrain_tess`'s internal `vy[]` array; second
worst is `vy[] -> o_cy[]`. Anyone optimising the geomorph or normals multiply
chain would be optimising the wrong thing. Note the wrapper is a characterization
harness, so part of that path may not exist in the console at all -- establish
which part does.

**5. The geomorph blend at `zhao_terrain_tess.sv:592-608`** is
multiply-then-add-then-saturate across a single cycle with no register between,
the same shape flagged in the projection core.

**6. `zhao_terrain_patch` and `zhao_terrain_velocity` are production blocks with
no measured cost.** Architecting them blind is not possible; say what you'd need.

## Invariants. These are never traded, for any saving.

From recon 4, with citations -- go read them, do not take my summary:

* `design/contracts/TERRAIN.PATCH.md:294` -- the **bit-exact `live_top` law**.
* `design/contracts/TERRAIN.NORMALMAP.md:79` -- ready/valid **in input order**,
  fixed latency, **II=1**.
* `design/contracts/TERRAIN.WRITEBACK.md` -- all **1,024 journal words
  byte-identical**.
* `shade_flat_tri_dir_unclamped` is **THE LAW** for shading.
* Full legal numerical domains. **Guarded W. Behind-eye records. All views.**
  Every required recipe. Exact rounding and saturation at every existing
  semantic boundary.
* **Every shape, constant and timing value stays a named, editable parameter.**
  "It is derived, so it is not a knob" is forbidden (CLAUDE.md art law rule 6).

Latency may grow. Throughput contracts (II, in-order, fixed-latency) may not,
unless you show the frame budget still closes with the number computed.

## How to work

**Instruments, not recipes.** Use the committed tools by name --
`tools/budget/dsp_census.py` (with its own `build_bill` selector, never a
reimplementation), `tools/quartus/check_quartus17_syntax.py`,
`tools/quartus/check_prod_manifest.py`, `tools/maintenance/no_control_bytes.py`.
Do not inline a format-reading recipe.

**No Quartus fits.** Owner's explicit instruction. Verilator answers
correctness, throughput in clocks, handshakes, atomicity, parameter sensitivity
-- in seconds. Area, Fmax, RAM inference and DSP count need a fit; **name the one
fit gate and the exact question it answers**, and stop there.

**Quartus 17.0 rejects things Verilator accepts.** Module-scope elaboration `if`
outside `initial begin`; implicit generates without `generate`/`endgenerate`;
inline `for (genvar i...)`; struct fields used before declaration. Run the
syntax gate. `--lint-only` does **not** run `initial` blocks, so a clean lint
says nothing about an elaboration `$fatal`.

**Every counter you add must be SEEN TO FIRE.** A detector reading zero is a
claim. If legal stimulus cannot reach it, write a **committed mutant** under
`tests/mutants/` with inverted polarity -- `tests/mutants/zhao_texture_frag_expand_mutant.sv`
is the pattern. And never write a test that asserts the bug.

**Ask what clocks both sides of any comparison.** If one register enable drives
both operands, the checker is structurally blind to every timing fault that
enable participates in. That exact defect shipped here on 2026-09-08.

**The first explanation that absolves the design is the one to check hardest**,
and **a broken instrument lies in the direction that looks better**. A number
that is exactly zero is a broken instrument until proven otherwise.

## Deliverables

1. A report at `reports/TERRAIN-REARCHITECTURE-20260909.md`: what moves to
   memory, what shared-arithmetic factories replace what private engines, the
   M10K arithmetic **shown not asserted**, the throughput argument per changed
   block against `computeClocksPerFrame = 1,666,666` in
   `design/budgets/workloads.yml`, and the ALM/DSP/register saving **separated
   into measured, structural-prediction, and unknown**.
2. Concrete RTL for whatever is ready to build, lint-clean and Quartus-17-gate
   clean, with a directed test whose counters are all fired and whose checker has
   been **seen to fail** on a deliberate break.
3. An implementation order with the fit gates named in advance and the question
   each answers.
4. An honest "not verified" section naming the instrument for each item.

**Do not commit anything.** Leave files in the working tree; I review and commit.
Do not run a Quartus fit. Do not touch anything outside terrain plus the two
already-landed projection files (which you should read but not modify).
