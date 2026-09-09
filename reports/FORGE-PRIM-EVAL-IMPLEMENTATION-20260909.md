# FORGE.PRIM.EVAL — the lightning position evaluator, built

2026-09-09 · run `RUN-20260909-2216-forge-prim-eval` · branch `zixxtrixx-v8-closeout`

The owner's ask (`reports/ADDLIGHTNING.md`, 2026-09-04): *"The missing work is
chiefly the procedural position evaluator and end-to-end Forge integration,
not some huge new lightning ASIC organ."* This packet is that evaluator:
`zhao_forge_prim_eval` + `zhao_forge_jitter_rom`, lint-clean, Quartus-17-gate
clean, unit-verified bit-for-bit against a new scalar oracle, with every
counter seen to fire and one committed mutant for the guard legal stimulus
cannot reach.

**No FORGE.PRIM.EVAL contract existed** — all 118 files in `design/contracts/`
were checked. The brief therefore stood; `design/contracts/FORGE.PRIM.EVAL.md`
now exists and makes the owner's law exact.

## What was built (all uncommitted, in the working tree)

| file | what |
|---|---|
| `fpga/rtl/forge/zhao_forge_prim_eval.sv` | the evaluator |
| `fpga/rtl/forge/zhao_forge_jitter_rom.sv` | JITTER_Q16, generated, one M10K |
| `tools/forge/gen_jitter_rom.py` | the table formula — lives ONCE, emits both consumers |
| `reference/include/zref/zref_forge_eval.hpp` | the scalar law (`zref::forge::eval_job`) |
| `reference/include/zref/generated/zref_forge_jitter_table.hpp` | the oracle's table copy |
| `tests/forge/forge_prim_eval_directed.cpp` | 968 checks (see Verification) |
| `tests/forge/forge_jitter_rom_directed.cpp` | 512 checks, every entry, both ports |
| `tests/forge/forge_prim_eval_overrun_control.cpp` | inverse-polarity mutant driver |
| `tests/mutants/zhao_forge_prim_eval_mutant.sv` | the committed break for `walk_overrun_o` |
| `design/contracts/FORGE.PRIM.EVAL.md` | the contract |
| registrations | `design/blocks.yml` (+ counter catalog + `counter_ports`), `design/fit_targets.yml` (gate **F-EVAL1**), `design/prod_manifest.yml` (`excluded: not-yet-adopted`), `tests/CMakeLists.txt` (4 test targets) |

`check_prod_manifest.py` passes (215 modules, every one counted once or
declared absent). `check_quartus17_syntax.py` passes. `check_counters.py`
resolves all seven counters to ports. `dsp_census.py`'s SPECIFIED-BUT-NOT-
BUILT list correctly dropped from 6 to 5.

## The arithmetic

The owner's law, made exact (every choice named; oracle and RTL match bit for
bit):

* **Lerp, exact rational.** `off_c = rhu(D_c·i/N)` computed as
  `floor((2·D_c·i + N)/(2N))`. The numerator is maintained by exact integer
  accumulation (`acc += 2D` per point, s41); the division is a bit-serial
  restoring divider, 40 iterations, **zero DSP** (divisor `2N ≤ 48` fits six
  bits). No accumulated error exists to argue about: each point's offset is
  independently the true rounded rational, so `P0 = start` and `PN = end` are
  arithmetic facts (endpoints are additionally emitted by assignment with
  jitter masked).
* **Jitter, from a resident table.** Two xorshift32 streams — seeded from
  `(seed, tick_phase)` and from the owner's literal `seed²` (low 32 bits of
  `seed*seed`, salted with named constant `SEED2_SALT` so seeds 0/1 still
  decorrelate). Low 8 bits index JITTER_Q16. Branch streams re-seed with
  `BR_SALT[b]` so a branch is not a phase-shifted main bolt.
* **Scale and sum, qformats-lawful.** `jA = fx_mul(amp, T)` (one rescale);
  `disp_c = rescale(perp1_c·jA + perp2_c·jB, 16)` — FUSED, exact 66-bit sum,
  ONE rounding (the qformats §3 single-rounding law);
  `P_c = sat(S_c + off_c + disp_c)`, one saturation, counted. Ribbon pair
  `sat(P ∓ wvec)`, `wvec = fx_mul(half_width, waxis)`.
* **One multiplier.** Every product in the block — `seed²`, three `wvec`
  components per polyline, `jA/jB`, six displacement products per point —
  goes through ONE operand-muxed 33×33 signed multiply with a registered
  product, the `zhao_terrain_normals` mseq pattern the brief named. There is
  no second multiplier site.
* **Branches grow from the jittered bolt.** Branch start = the main
  polyline's centre point at the attach index, captured in flight — the
  caller cannot know a jittered position, so it is never asked to.

**A rounding discrepancy, for the owner to settle:** FORGE.PRIM.md says
positions round *half away from zero*; `spec/qformats.md` §4 defines the one
rounding primitive as `(x + 2^15) >>> 16`, *round-half-up* (ties toward +∞),
and that is what `zhao_terrain_normals` and every SIN_Q16 consumer already
do. **qformats was followed.** If half-away-from-zero is ratified instead,
`eval_rescale16`/`rs16sat` change in both oracle and RTL together and the
directed test re-pins everything.

## Determinism — how it is guaranteed, not just tested

1. Jitter stream state advances **once per point inside the walk FSM**, never
   per clock — backpressure structurally cannot reach it.
2. The lerp is an exact rational with no history.
3. Emission values are combinational from registers that are stable for the
   whole stall; a waiting consumer sees one unchanging vertex.
4. Everything else is a frozen table, a named constant, or a latched param.

Tested as: byte-identical streams under always-ready / period-3 /
pseudo-random-50% / ready-1-in-10 consumers, and across reruns (T2), plus the
oracle equality everywhere else.

## The M10K arithmetic, shown

256 entries × 18 bits = **4,608 bits**. One Cyclone V M10K in ×18 mode holds
10,240 bits → one block, 45 % occupied. The console owns 553 M10K and uses
~147; this spends **1** of ~406 free. The ROM is the proven inference shape
(one array, synchronous dual read, no reset near the array — the
`zhao_field_sin_rom` idiom per QUARTUS_GOTCHAS S10). Computing jitter instead
would put a hash-to-fx16 path through multipliers the console does not have —
the table trades an abundant resource for a scarce one, which is the whole
rescue posture.

## Rate against the frame budget — MEASURED

The directed test measures the walk (T1, always-ready consumer):

* **worst legal bolt (24 seg + 2×8-seg branches): 5,451 clocks = 0.327 % of
  `computeClocksPerFrame` = 1,666,666** (printed by the test every run, with
  a 12,000-clock regression tripwire).
* 16 simultaneous worst-case bolts ≈ 87 k clocks ≈ **5.2 %** of a frame.
* A typical storm (a few mid-LOD bolts) is well under 1 %.

So the brief's expectation holds with room to spare: a sequenced evaluator is
trivially affordable at 24 segments, and parallel arithmetic would buy
nothing but DSPs. The dominant term is the 40-cycle serial divide × 3
components × interior points; if rate ever mattered (it does not), the
divmod-recurrence form (one divide per polyline) is the named lever.

## The bill

**MEASURED (Verilator, this packet):**
* bit-exactness against `zref::forge::eval_job`: 968 directed checks
  including a 150-job randomized full-domain sweep (anchors anywhere in s32,
  `start == end` included, extreme-saturation params included);
* determinism under four stall patterns and reruns;
* caps refused at their boundaries (0/25 segments, 3 branches, 0/9 branch
  segments, attach past end) with **nothing emitted**, legal boundaries
  (24, 8, attach == N) accepted;
* view-skip vs refuse separated; seed corners 0/1/0xFFFFFFFF;
* all seven counters equal to oracle counts cumulatively; `sat_events` fired
  by legal extremes; `walk_overrun` fired by the committed mutant (counter
  read 1, inverse-polarity driver passes);
* the comparator itself SEEN TO FAIL on a planted one-bit corruption (T10);
* every JITTER_Q16 entry equal to the generated oracle copy, both ports;
* worst-bolt walk = 5,451 clocks.

**STRUCTURAL PREDICTION (fit gate F-EVAL1 measures):**
* DSP: one 33×33 multiplier site → 2–4 DSP blocks, output register in the
  DSP;
* M10K: exactly 1, by the proven inference idiom;
* ALM: ~1,000–1,800 (≈900 flops of latched params/walk/divider plus control);
  the FORGE.PRIM contract ceiling (prim + eval ≤ 2,800 ALM / 10 DSP /
  2 M10K) is the budget it must land inside;
* Fmax: no path longer than (operand mux + 33×33 multiply) or the 43-bit
  saturating adds — both shorter than the terrain-normals shape that measured
  31 MHz *before* its product register and this block registers the product
  from birth. Prediction, not measurement.

**UNKNOWN:**
* actual fit numbers (F-EVAL1 has not run — deliberately; see below);
* the FX.LIGHTNING dispatch seam's cost (not designed);
* how the bolt LOOKS. The uniform-table kink distribution is a first guess;
  the art law applies — render it, look at it, retune `amp`/table via the
  generator. No picture exists yet because no composition exists yet.

## Implementation order from here, fit gate named

1. **(done, this packet)** evaluator + oracle + tests + registrations.
2. **FX.LIGHTNING dispatch seam** (the "end-to-end Forge integration" half of
   the owner's sentence): a composition that turns one FX.LIGHTNING command
   into one eval job + one prim ribbon job per polyline and merges the
   position stream with prim's indices into GEOM.SETUP. Contract sketch is in
   ADDLIGHTNING.md; needs ratification as an effects-level contract, NOT a
   seventh Forge family.
3. **Fit gate F-EVAL1** — the ONE fit, batched with the next forge-subsystem
   fit (2026-09-08 batching law). Its exact question: *does the shared
   multiplier land in ≤ 4 DSP with its output register used, does the 256×18
   table infer one M10K rather than melting into ALMs, and is prim + eval
   inside the 2,800-ALM / 10-DSP / 2-M10K contract ceiling?* Nothing else in
   this packet needs Quartus — correctness, determinism, caps and rate are
   Verilator questions and are answered above.
4. **RASTER additive material + POST.GATHER glow tag wiring** for the bolt's
   material (both blocks exist; the bolt needs a material id and the emissive
   tag).
5. **PART.SOFT far-LOD rungs** (streak/glint) — already-specified particle
   work, per the owner's ladder.
6. **Look pass**: render, look, retune `amp`, `half_width`, the table
   distribution (regenerate via `tools/forge/gen_jitter_rom.py`), per the art
   law — measurement never trumps looking.

## Not verified, instrument by instrument

* **Quartus synthesizability**: NOT verified by a real `quartus_map` run —
  instrument would be the F-EVAL1 fit. `check_quartus17_syntax.py` (which
  self-tests 3 fire / 6 no-fire) passes, and the two known Quartus-17 traps
  (elab checks inside `initial`, no implicit generate) are respected, but
  "lint 0" settles one tool's opinion and a block that has never been through
  quartus_map has not been shown synthesizable.
* **DSP/M10K/ALM/Fmax numbers**: structural prediction only — instrument is
  F-EVAL1.
* **M10K inference specifically**: the idiom is copied from a block where it
  is PROVEN to infer (`zhao_field_sin_rom`, post-Wave-7), but inference is a
  Quartus behaviour, not a source property — instrument is the fit report's
  RAM summary.
* **The real block's overrun `$fatal` assertion**: the mutant disables it (by
  law, mutant-only, reason beside it), so the assertion itself has not been
  watched to fire — the synthesizable counter has. Instrument would be a
  second mutant variant keeping the assertion; judged not worth a fourth test
  binary.
* **Behaviour composed with zhao_forge_prim in one stream**: the seam is
  verified by construction (vertex order == ring-major `vidx`, checked
  against `zref::forge::prim_triangle` spans in T1), not by a composed
  simulation — instrument is the FX.LIGHTNING composition test when the seam
  exists.
* **The look of the lightning**: no instrument exists yet but eyes; nothing
  rendered.

## Where the brief was wrong (asked for plainly)

1. **Nothing contradicted the brief materially.** No FORGE.PRIM.EVAL contract
   existed to override it (verified against all 118 contract files).
2. Small correction: the census's SPECIFIED-BUT-NOT-BUILT line is read out of
   `design/prod_manifest.yml`'s `unpriced_requirements` block, not computed —
   so "making the census right" meant editing the manifest, which is done
   (the row left the list because the list means "no RTL file"; the deferral
   now lives in `excluded:` where `uncashed_cheques.py` watches it).
3. The census line said "ribbon/tube" positions; **tube is deliberately NOT
   in this v1** — lightning needs only the ribbon law (the owner's ladder
   uses PART.* for everything below mid-LOD), and the contract's exclusions
   section says how the other five families' laws get added by ratification
   rather than by drift.
4. FORGE.PRIM.md's rounding sentence conflicts with qformats §4 (detailed
   above); the brief pointed at qformats, which was followed.

## Outstanding, NOT this lane

The owner's commit subject also asks for **lightning versions of the creature
in the render reel** — creature art in the reel/Upheaval lane, needs its own
run, untouched here. It should not wait on hardware: ADDLIGHTNING.md itself
says the reel path is the currently executable route.

Also noted while working: this repo has at least one other live lane (files
changed on disk mid-session in `design/blocks.yml`, `tests/CMakeLists.txt`,
`design/prod_manifest.yml`); all edits here applied cleanly and re-ran their
checkers afterwards, and this packet touched only the forge/reference/tests/
design seams the brief assigned.
