# Task Log: RUN-20260822-2136 - the saturating-counter law, the creature LOD ladder, and the CDC seam

**Created:** 2026-08-22 21:36 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260822-2136-saturating-law-creature-lod-and-cdc/

> Backfilled at 21:36 to cover a session that started around 04:00. The run was
> not initialized at the start; that omission is the reason runs are now a
> permanent fixture of the process.

---

## Objective

Push the hardware forward under the standing method — oracle first, differential
against the shipped oracle, mutation sweep with forced regeneration, gaps closed,
gates green — and get the owner's outstanding architecture decisions answered and
implemented. Everything is simulation, synthesis or fit.

---

## Progress Timeline

### ~04:00-17:40 - inherited state

`main` at `e59f34a`. The composed shell fitted and missed timing at -0.729 ns
with 97 failing endpoints. Field IR ops all built and swept; `FIELD.SEQ.CORE`
`RTL_VERIFIED` on a formal proof of the anti-hang law.

### 18:43 - `6d23c84` five copies of a law no test could reach

`INPUT.SNAPSHOT`'s saturation test spelled `MAX - x` as a wide subtract. All-ones
minus x is exactly `~x` — an identity, no borrow — and the pattern appeared FIVE
times across four blocks in three different spellings.

The rewrite was trivial. The useful part was checking whether anything would
notice if it were wrong: mutating `~b` to plain `b`, which removes saturation
entirely, PASSES `cmd_dma_directed` (48 checks) and its 5,000-packet random lane
(19,015 checks). More stimulus cannot fix that — these are u64 counters and the
rail is ~2^64 events away, so the saturating arm is unreachable in simulation.

So the five copies became one `zhao_pkg::zhao_sat_add{64,32}` and the evidence
became a proof: `tests/formal/sat_add.sby`, checking the result against the
(W+1)-bit sum, which cannot overflow and is therefore the true arithmetic answer.
Five covers reached with witness traces. SCOPE-TOTAL rather than an `a_scope_*`
guard, because the harness is purely combinational with every input free.

Sweep: 7 attempted, 7 accounted, 6 caught, 1 equivalent (`>` widened to `>=`
differs only at `a == ~b`, exactly where `a + b == MAX` and both return MAX).

Rule V20 caught my own comment claiming the guard was right "by construction"
with no enforcer named. Correct refusal; the fix was to build the enforcer.

### 18:56 - `f0107f6` composed shell fit, and the measurement owed

Against the immediately prior run `de2794d`, not a doc summary:

| | before | after |
| --- | ---: | ---: |
| setup worst | -0.729 ns | **-0.475 ns** |
| failing endpoints | 97 | **56** |
| ALMs | 7,667 | **7,415** |
| hold | 0 failing | 0 failing |

The caveat carried in — placement-bound below ~1.5 ns, may not move the headline
— did not hold, because five real borrow chains went away.

### 20:29 - `aec3c4c` creature LOD, four rulings, and a bug my own test caught

**Four owner rulings** were requested and acted on (see Decisions Made).

**`zhao_geom_lod.sv`**, the creature representation ladder, with NO DIVIDER. The
law divides twice per evaluation, but every quotient feeds a comparison and
`floor(N/e) <= T  <=>  N < (T+1)*e` exactly, so both divides become multiplies
and the hysteresis reduces to division by the constants 9 and 11.

**A defect in the SHIPPED reference**, found by the random lane: `boundary_q8`
computed `thresh * R` in `__int128` and narrowed the quotient to `int32_t`.
2,422,670,754 wraps to -1,872,296,542, and a negative boundary makes the
eager-coarsen test false for every projected radius — the ladder refuses to
coarsen and a creature walking into the distance stays at full detail forever.

**A defect in my own RTL**: the three rung error terms were an unpacked array
indexed by a genvar in a generate loop, and all three legality bits came out
equal, so the block could only ever answer rung 3 or rung 0. It agreed with the
oracle on **27,618 of 29,459 checks** anyway. The corner sweep caught it; a
magnitude bisect showed it reproduced at R = 65,536, so it was never about
magnitude.

Sweep: 22 attempted, 22 accounted, 21 caught, 1 equivalent. Both survivors of the
first scored run were REAL holes, closed by constructed boundary cases.

### 20:41 - `8d48608` (subagent) carry-over commit

The CDC subagent found my in-progress `geom_lod` work in the shared tree and
committed it separately rather than folding it into its own change. Correct call;
the collision was my fault for editing the tree while it built.

### 21:0x - `54ec158` the LOD block measured in Quartus

The block had never seen Quartus. It **did not synthesise**:

```
Error (272006): In lpm_divide megafunction, LPM_WIDTHN must be <= 64
```

Verilator, slang and 629,510 differential checks were all happy with the 72-bit
constant divisions. Same shape as the inline `genvar` only the composed fit
caught. Then the measurement showed the slack was expensive:

| | ALMs | DSPs |
| --- | ---: | ---: |
| first synthesis (72-bit) | 1,436 | **28** |
| after narrowing | 1,303 | **18** |

28 DSPs is a quarter of the device for one LOD evaluator. Three provably
equivalent fixes: proven-sufficient 64-bit widths, `thresh*R` computed once
instead of twice, and one boundary multiplier instead of two mutually exclusive
ones. The named next lever (sequencing the three legality products) is
deliberately NOT pulled: the consumer does not exist yet, and its throughput is
what decides whether sequencing is free.

### 21:1x - GEOM.MESHFETCH cull derived and validated, before any RTL

`grep frustum` returns nothing across the whole reference, but the frustum does
not need inventing: the five planes are row combinations of the `mat4fx`
view-projection the renderer already projects with. Three conventions would each
be easy to get wrong — row-major so rows not columns, **+Y NDC maps downward**
(no Y flip), and **no z clip at all**, so there are FIVE planes, not six.

The sphere test needs `|normal|`, and the ratified `isqrt_u64` is an exact FLOOR
sqrt — the wrong direction here. Flooring makes rejection easier to satisfy, so
the block would delete geometry that is visible. The safe form takes a ceiling.

Validated header-only outside the repo (`cull_check.cpp`), three camera matrices:

```
plane equivalence : 600000 checked, 0 mismatches
conservatism      : 60000 spheres, 26796 rejected, 0 WRONGLY rejected
looseness         : 344 kept although empty
```

Conservatism is proven by the direction of the bound; the sample corroborates.

### 21:36 - runs made a permanent fixture

Owner: *"you're meant to use the initialize run script and fill templates and
have agents fill them"* / *"Make runs a permanent fixture in our process.
Should've been from the beginning, serious blunder."* This run folder created,
memory written, past runs being reconstructed.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 2026-08-22 ~20:30 | a69cac37667f114f8 | Fix the GPU/video CDC seam: move the displayed CRC into `vid_clk` rather than crossing per-pixel state (owner ruling) | In Progress | pending — agent to write `FINDINGS.md` |

Notes on this spawn:

- Given the full method, the environment traps, and the instruction not to run a
  composed fit itself.
- Mid-run correction sent: its `ctest -R shell_duo` regex matched three tests
  including a 90-minute gate variant and an **8-hour soak** (`TIMEOUT 28800`).
- It committed `8d48608` to isolate my in-progress files rather than absorbing
  them, which was the right call.

---

## Files Created

- `fpga/rtl/geometry/zhao_geom_lod.sv` — the creature representation ladder.
- `tests/differential/geom_lod_directed.cpp` — differential against the shipped
  `zref::creature::lod_raw` / `lod_update`.
- `tools/sweep_geom_lod.sh` — mutation sweep; its header documents four distinct
  ways this build system will score a run that never happened.
- `tests/formal/sat_add.sby`, `tests/formal/sat_add_harness.sv` — the saturating
  counter law, proven over all inputs.
- `reports/PHANTOM_REFERENCES.md` — two addenda (MESHFETCH classification; the
  "all 25 still SPECIFIED" correction).

---

## Decisions Made

**Owner rulings, 2026-08-22** (full text in `docs/OWNER_DOCKET.md`):

1. **One engine, five profiles.** The five `FIELD.SEQ.*` entries were `kind: rtl`,
   demanding five reference models and ten test files under V4 and booking FIVE
   ENGINES' worth of ALM budget for one engine under V5. Implemented as a new
   ledger kind `profile` + `implemented_by`, schema updated, and **rule V21** to
   charge for the exemptions. Six tests; ledger suite 46/46.
2. **"Camera visibility sectors" deleted.** The phrase existed only in the
   block's own purpose line. Replaced by conservative per-camera frustum
   rejection of a bounding sphere before vertex decode, rejecting only when
   outside EVERY active camera.
3. **The LOD boundary overflow: fix the law, never bake the wrap into silicon.**
   Owner's amendment was the half I had missed — widening to `int64_t` is not a
   fix either, because the next line multiplies by 9 or 11.
4. **BALANCED stays authoritative; the CDC seam comes before any more setup
   tuning.**

**Owner target recorded:** 120 MHz GPU fabric + 150 MHz DDR memory interface (the
GeForce 256 DDR part). Costed honestly on the docket: 20.4% beyond today's
~95.5 MHz on a placement-bound design, and the memory half cannot be costed at
all until the board is frozen.

**My own call:** do not pull the DSP-reduction lever on `zhao_geom_lod` yet —
restructuring against a guessed throughput and then measuring is the wrong order.

---

## Next Steps

1. When the CDC subagent lands and the tree is free: the 23-mutant sweep on
   `zhao_geom_lod` (anchors updated and dry-checked, 23/23 resolve uniquely), and
   a full `ctest -L fast` plus ledger check.
2. Dispatch the next subagent for `GEOM.MESHFETCH`'s cull, using the derivation
   and validation already done in this run.
3. Re-run the composed fit after the CDC seam changes placement, then re-measure
   BALANCED against HIGH PERFORMANCE as ruled.
