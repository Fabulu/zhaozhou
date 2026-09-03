# Contract — TEXTURE.FRAGROB (Fragment transaction centre)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/texture/zhao_texture_fragrob.sv`
> Reference: `zhao_raster_texjoin_v2` is the BEHAVIOURAL oracle — see below

## Purpose and exclusions

FRAGROB owns a fragment's texture transaction from the moment it survives
Early-Z until it retires: slot allocation, generation, the sample descriptors,
the required/arrived bookkeeping, AUX identity, and **ordered retirement**.

**Written 2026-09-03 from `reports/islandrearchitecture5.md` §6.1**, which is
explicit about the method:

> Create a new production block beside v1 and v2: `zhao_texture_fragrob`. Do
> not mutate `zhao_raster_texjoin_v2` until it becomes impossible to tell which
> implementation its tests describe. Keep v2 as the oracle for token
> allocation, multi-sample completion, generation rejection and
> allocation-order retirement.

So this block exists because v2 is **wrong about storage and right about
behaviour**, and the two must not be changed at once.

**What v2 got wrong, measured:** 3,824 ALM and **7,151 registers** against a
budget of 900 / 1,200. Its entry table is 7,056 bits, so those registers *are*
the table, in flip-flops. Three independent causes, all structural:

1. the arrays are written from an **async-reset process**, and an M10K has no
   reset port;
2. the retire path reads `srgb_q[head_q][0]` **combinationally** through a
   dynamic index, which forces a 16:1 mux per bit whatever the writes do;
3. two dynamic write addresses into one array, which §5.3 forbids by name.

**Exclusions, each a specific refusal:**

* **No reciprocal, perspective, texture-address, palette, bilinear or
  material-combiner arithmetic.** §6.1 lists these as explicitly not owned.
* **No cache policy.**
* **No blending, depth or stencil** — that is `RASTER.FRAGMENT`'s.
* **No free-slot scan.** A priority encoder over the valid bits is exactly the
  16:1 mux shape that put muxes in front of everything in v2.

## The structure, and why it is the whole point

**Control state in flops, deliberately** (§6.3): valid, generation,
required/arrived masks, AUX flags. *"This is a few hundred registers total and
permits TMU and AUX return events to update independent fields without a RAM
read-modify-write loop."* Wide U/V, context, bindings and colours are
explicitly **not** allowed here.

**Payload in banks keyed by SAMPLE INDEX** (§6.4): `desc_u_m[3]`,
`desc_v_m[3]`, `desc_met_m[3]`, `res_rgb_m[3]`, `res_a_m[3]`, plus the context
and AUX banks. Banking by sample index is what lets one three-sample fragment
write all three descriptors on a single clock without a three-write-port
memory, and lets the combiner read all three in parallel.

Every bank is written **and** read only inside a clock-only process, through
registered addresses. That is the difference between an M10K and a flip-flop
array, and it is the entire reason this block exists.

**The price, stated plainly:** one pipeline stage on issue and one on retire. A
descriptor living in a bank cannot be picked combinationally. Latency for
structure is the trade the brief asks for.

## Input and output packet layouts

Identical to `zhao_raster_texjoin_v2`, deliberately — that is what makes the
differential possible and what would break loudly if either drifted. Fragment
in (`sample_count`, three `{u, v, binding, lod}`, recipe, context, aux flag,
uv-sat), TMU request/return carrying `{slot, sidx, generation}`, AUX
request/return carrying `{slot, generation}`, retired fragment out.

## Backpressure rules

Ready/valid throughout. `f_ready_o` is low while the free list is empty and
during the sixteen-cycle init sweep. TMU and AUX returns are always accepted —
a return is *data*, and refusing one would strand the sample it answers.

## Memory ownership

None outside its own banks. It reads nothing and writes nothing in SDRAM.

## Q formats and rounding

None of its own. It stores what it is given and changes no bit.

## Latency (fixed or variable)

`variable`. Issue is three clocks from work-queue pop to request present;
retire is two from head-ready to output valid. Neither is a number a consumer
may depend on — the ordered-retirement contract is what is promised.

## Overflow and malformed-input behaviour

* **A return whose `{slot, generation}` does not match a live slot is REFUSED
  and counted** (`id_errors_o`, and a single-cycle `id_error_o`). It is never
  applied to whatever now occupies that slot. This is what `GENW = 8` protects:
  ruling X5 found that a 2-bit generation wraps after four reuses, so a return
  delayed longer than that matches the **wrong** fragment and is silently
  accepted — worse than the stale return it exists to catch.
* **Allocation blocks when the free list is empty**, counted as
  `full_clocks_o`. It does not overwrite a live slot.
* **The work queue asserts `wq_overflow_o`** rather than wrapping silently.

## Scalar reference function

**None, and that is deliberate.** This block has no arithmetic — it is
transaction bookkeeping, and a scalar model of it would be a second
implementation rather than a law. Its oracle is `zhao_raster_texjoin_v2`,
whose behaviour is pinned by `tests/raster/raster_texjoin_v2_directed.cpp`.

## Directed tests

**`tests/texture/fragrob_differential.cpp` — WRITTEN.** Both blocks driven
from one stimulus through one shared model of the outside world, whose answers
depend on what was asked and never on when. Five scenarios: no stalls with
immediate returns, late returns, a stalling consumer, both, and more fragments
than slots so the free list must recycle. **280 retirements compared, zero
divergence.**

The comparison is the **sequence** — same fragments, same order, same values —
and explicitly *not* cycle-by-cycle, because the pipeline depths differ by
design and a cycle-exact check would be testing the wrong thing.

A coverage guard refuses a run that compared fewer than 200 retirements, so a
differential that silently compared two empty lists cannot report success. It
fired on every failing round during bring-up.

**Three RTL bugs it found, in a block that was already lint-clean and
committed:**

1. **allocation order is not slot order** — slots come from a free list, so
   after the first recycle the hand-out order has nothing to do with the index.
   Fixed with an explicit allocation-order ring.
2. **the lost-update fault** — `free_cnt_q` and `live_cnt_q` each written from
   two separate `if` blocks in one `always_ff`, so a coincident accept and
   retire produced ±1 instead of net zero and the free list eventually reported
   empty while holding slots.
3. **a single AUX pending register dropped the second request**, so the
   fragment that needed it could never complete — a deadlock, not a lost pixel.

**The refusal paths are also WRITTEN**, in the same file, because a
differential cannot reach them: two blocks with the same blind spot would agree
about being wrong.

* the sixteen-cycle free-list sweep holds `f_ready_o` low, then raises it — a
  block that accepted a fragment before its free list existed would allocate
  slot garbage;
* exactly `DEPTH` fragments are accepted while nothing retires, so allocation
  **blocks** rather than overwriting a live slot, and `full_clocks_o` counts the
  stall so a starved raster is visible rather than merely slow;
* **a return whose generation matches no live slot is refused and counted** —
  the property `GENW = 8` exists for;
* a **zero-sample** fragment retires with no TMU request at all. It is excluded
  from the random mix on purpose (it would be lost among fragments that do
  request samples) and it is the one case with no TMU handshake to wait on, so
  it is the one that can hang waiting for a sample it never asked for.

**Still planned and not written:** `wq_overflow_o`, which needs a stimulus that
offers more samples than the 64-entry work queue can hold.

## Randomized differential tests

The differential above is randomized over sample count, recipe, AUX presence
and the U/V payload. **Not yet run at nightly depth.**

## Integration capture cases

None on hardware. No board, no programmed device.

## Synthesis / resource ceiling

Registered in `design/fit_targets.yml` with §3.4's tripwires:

    min_m10k: 6        max_registers: 2500        max_dsp: 0

The **minimum** is the point. v2 has 4 M10K against 7,151 registers, so a pass
here *requires* the banks to be memory. §3.3's budget for the block is 900 ALM
/ 1,200 reg / 14–20 M10K / 0 DSP.

**Not yet fitted.** The rule checker reports it as *unmeasured*, which is
deliberately a different thing from *failed*.

## Notes

The material combiner is a separate block with its own budget line (§3.3), so
every recipe currently retires sample 0 — which is v2's behaviour, and is what
`combiner_unfrozen_o` exists to announce. **A gate that ignores that signal is
testing a placeholder.** The recipe is nevertheless stored per slot, so the
descriptor is genuinely captured rather than dropped until the combiner exists.

v2 is **not** retired from `design/prod_manifest.yml` by this block's
existence. It is superseded only once FRAGROB has been fitted against its
tripwires and the composed island has been re-measured.
