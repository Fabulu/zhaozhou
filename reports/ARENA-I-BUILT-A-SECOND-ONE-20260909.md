# The archaeology's first find is my own commit, from one hour ago

2026-09-09. The owner asked for TODO archaeology after discovering that the
shared-projector move had been fully planned on 2026-08-24, its prerequisite
deliberately built, and the final consolidation simply never performed. The very
first thing that sweep turns up is a duplication **I created today**.

## What already existed

`fpga/rtl/geometry/zhao_vertex_arena.sv` -- 36,565 bytes, a reusable
direct-indexed arena primitive with **58 formal assertions** and a committed
SymbiYosys proof (`tests/formal/geom_wcache_arena_bounds.sby`, with separate
`prove` and `cover` tasks).

`fpga/rtl/geometry/zhao_geom_wcache.sv` -- the projected-vertex shell that
instantiates it, **already composed in `zhao_prod_top.sv:2257`**.

And the owner ruling both files quote, dated 2026-08-24:

> "Build a reusable parameterized arena primitive and a GEOM.WCACHE shell.
> Terrain may later instantiate the same primitive with its own depth/payload.
> This does not mean one physical cache shared between the two pipelines."

`zhao_geom_wcache.sv`'s header then says, in as many words:

> "the arena mechanism -- the direct-indexed store, the flop valid bitmap, the
> generation, the six refusals, the READ-OLD semantics and the formal shadow
> proof -- lives once, in `zhao_vertex_arena`. [...] **a second copy of that
> valid mechanism is exactly what the ruling forbids.**"

## What I did

I commissioned `fpga/rtl/common/zhao_proj_arena3.sv` and committed it in
`452fe49a` with a long message about how carefully it had been verified. It is a
second copy of that valid mechanism. I checked its lint, its Quartus-17 syntax,
its directed test and both of its positive controls -- and I did not check
**whether the thing already existed.**

That is the same failure the owner is hunting, at one day's remove instead of two
weeks'. Every gate passed and the question the gates cannot ask went unasked.

## The uncomfortable part: the new one is better, in one specific way

`zhao_vertex_arena`'s header enumerates three ways to answer "was this slot
written during THIS use of the arena", and picks the third:

1. clear the payload memory on open -- **forbidden**, breaks M10K inference;
2. a generation tag per slot inside the memory -- **wrong**, post-reset contents
   are undefined so a never-written slot can match by accident, and the contract
   demands a deterministic refusal rather than "almost certainly refuses";
3. **a valid bitmap in flops**, cleared per arena on open. `ARENAS*DEPTH`
   registers. "The cost is real and bounded: the shell's 2x1089 shape is **2,178
   flops**."

It also names the escape hatch it did not build: *"For a much deeper
instantiation the alternative is a clear WALK that holds `sealed` low until it
completes [...] Not built until something needs it."*

`zhao_proj_arena3` found a **fourth** option that the list does not contain.
Make the fill DENSE -- the write address IS the group's fill counter, so rows
0..DEPTH-1 are written exactly once in order -- and REFUSE to seal unless the
count is exactly DEPTH. Then "was this row written this lifetime" stops being a
per-row question at all; it is a property of the GROUP, carried in a handful of
control flops. No bitmap, no clear, no walk, no tag-in-RAM. An unfilled group can
never seal, so whatever the RAM holds is unreachable rather than hazardous.

It costs a real restriction -- the producer must fill in order and completely --
which the terrain tessellator satisfies by construction and which a
random-access consumer would not. So it is not free, and it is not universally
better. But where it applies it deletes the 2,178 flops outright.

## So what should happen

**Not "keep both".** Two arena primitives is precisely what the standing ruling
forbids, and the one I added is the one without the formal proof.

The right move is to **fold dense fill into `zhao_vertex_arena` as a named,
parameterised valid-mechanism mode** -- `VALID_MODE = BITMAP | DENSE_SEAL` --
and let `zhao_geom_wcache` and a terrain shell both instantiate it. That:

* satisfies the 2026-08-24 ruling instead of violating it;
* keeps the 58 assertions and the committed `.sby` proof, extending them to the
  new mode rather than abandoning them for a directed test;
* still captures the flop saving, and captures it for the GEOMETRY shell too --
  which `zhao_proj_arena3` as committed does not, because nothing instantiates
  it;
* leaves one place where this mechanism lives, which was the entire point.

`zhao_proj_arena3.sv` then becomes the **design study** it actually is: its
throughput argument, its 106-bit record derivation, its free-padding
observation and its two fired positive controls are all still good work. The
module should not ship as a second primitive.

## A correctness question the comparison exposes, which matters more than the area

The two payloads **disagree about a field**, and this is not a preference.

`zhao_geom_wcache.sv` stores **75 bits**:

    [20:0]   screen x, S 12.8, guard-band clamped
    [41:21]  screen y, S 12.8
    [73:42]  invw, Q16.16
    [74]     behind: clip.w <= 0

`zhao_proj_arena3` stores **106 bits** -- the same four fields plus **`w`, the
guarded clip w, 31 bits** -- and argues that dropping it is forbidden, because
GEOM.DEPTHQUANT consumes `w` itself and recovering it from the ROUNDED `1/w`
compounds a rounding that has already happened.

Exactly one of these is right.

* If the wcache is right, `zhao_proj_arena3`'s record is 31 bits per row too
  wide -- 324 rows x 3 replicas x 31 = **30,132 wasted bits** -- and its M10K
  arithmetic should be redone.
* If `zhao_proj_arena3` is right, the **already-composed** `zhao_geom_wcache`
  silently drops a value the pipeline needs, and every consumer that replays
  through it is recovering `w` from a rounded reciprocal.

This wants resolving before either is built on, and it is a question about the
existing production block, not about the new one. **Open, unresolved, not
guessed at here.**

## Measurement status, stated rather than assumed

`zhao_vertex_arena` and `zhao_geom_wcache` have **NO fit summary anywhere** in
`reports/synthesis/blockpaths/`. Their ALM, M10K and Fmax are UNKNOWN. The 2,178
flops is the arena's own stated figure for its bitmap at the shell's 2x1089
shape, not a measured row, and 2,178 flops is not 2,178 ALM. Nothing here should
be quoted as a measured saving.

## The lesson, phrased so it survives

The gates I ran on `zhao_proj_arena3` -- lint, the Quartus-17 syntax gate, a
directed test, two fired positive controls -- are the strongest evidence set I
have produced today, and **not one of them can ask whether the module needed to
exist.** Component checks passing is not architecture evidence, which is the art
law ("component checks passing is not likeness evidence") wearing an RTL costume.

**Before commissioning a new block, grep the tree for the thing it replaces.**
One `ls` would have found `zhao_geom_wcache.sv`, and its header would have
handed over the ruling, the three-option analysis and the 2,178-flop number in
the first forty lines.
