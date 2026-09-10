# R1 implemented: `MATW` on the projection core — 18-bit matrix operand, refusal law, and a marginal of −3 (not −10)

2026-09-10. Owner ruling R1 (`reports/OWNER-RULINGS-20260909-2300.md`,
Fabian, verbatim: *"minimum fov you propose is ok"*) accepts a vertical FOV
floor a shade above 53.13°, which caps the view-projection matrix's
perspective coefficients at ±2.0 and lets the projector's nine **product**
words be carried as signed 18-bit Q16.16. This packet builds that lever as a
parameter, proves the default unchanged, proves the narrowed shape invisible on
legal content, decides what the hardware does with illegal content, and
re-derives the DSP value — which comes out **much smaller than the brief and the
docket say.**

**Verdict up front.**

* **Built, default off.** `MATW` is a parameter of `zhao_project_core`
  (default 32), passed through `zhao_project_service`, `zhao_geom_project` and
  `zhao_terrain_project`. Legal values are **32 and 18 only**; elaboration
  `$fatal` otherwise, deliberately — the calibration measures 32×19 at **4 DSP,
  worse than 32×32**, so "one bit of headroom" is a trap the parameter refuses
  to let anyone fall into.
* **MATW=32 is bit- and cycle-identical to HEAD.** Every existing projector suite
  passes unchanged, including `terrain_project_random`, whose matrix words sweep
  the **full s32**; `proj_rowmux_directed` still shows RPP=1 byte-identical to
  RPP=3.
* **MATW=18 is invisible on legal content.** A new three-instance differential
  (`MATW=32` reference against `MATW=18` at `ROWS_PER_PASS=3` **and** `1`)
  shows byte-identical output streams over six matrices spanning the whole
  legal domain — every product word on the ±2.0 boundaries, the translation
  column on the s32 rails, vertices at full s32 — under independent stall
  patterns.
* **Illegal content is REFUSED, counted, never clamped.** A write of a product
  word that does not fit signed 18 leaves the register unchanged and moves
  `mat_refused_o`. The counter is **seen to fire** (54 refusals counted exactly);
  the checker is **seen to fail** against a committed mutant with the check
  removed (151 of 274 checks fail, all in the refusal section, none elsewhere).
* **The marginal DSP value is −3, not −10.** 66 → 33 → 15 → **12**, not 5. The
  brief's lattice needs the *vertices* narrowed too, which R1 explicitly does
  not do. Corrected inline in the rulings docket; details in §5.
* **No Quartus was run.** The DSP figures are STRUCTURAL and one map gate is
  named (§7) with its exact question.

---

## 1. What was built, and where

| file | change |
|---|---|
| `fpga/rtl/common/zhao_project_core.sv` | `MATW` parameter (32 default / 18); `mat_refused_o` counter port; the refusal law on the cfg write path; `mulm`/`extp`/`mw` replace `mul32`/`ext64` (identical at 32); `ROW_W = MATW + 36` (68 at 32, unchanged; 54 at 18); typed rescale constants; header rewritten with the ruling, the refusal law and the re-derived lattice |
| `fpga/rtl/common/zhao_project_service.sv` | `MATW` passed through; `mat_refused_o` exposed (one bank, one counter for both clients) |
| `fpga/rtl/geometry/zhao_geom_project.sv`, `fpga/rtl/terrain/zhao_terrain_project.sv` | `#(MATW)` parameter added (default 32); `mat_refused_o` exposed rather than dropped |
| `fpga/rtl/prod/zhao_prod_top.sv` | **regenerated** (`gen_prod_top.py`); diff is exactly the two projector instances `u29_i`/`u57_i` gaining `mat_refused_o` |
| `tests/geometry/tb_proj_matw.sv`, `proj_matw_directed.cpp` | the three-instance differential + refusal law + latency + positive control |
| `tests/mutants/zhao_project_core_mutant.sv` | committed mutant: the fits-check removed (one line) |
| `tests/geometry/tb_proj_matw_mutant.sv`, `proj_matw_mutant_control.cpp` | inverted-polarity control: passes when the counter is silent AND the differential fails |
| `tests/geometry/tb_proj_rowmux.sv`, `tb_proj_service_rowmux.sv`, `tests/shell/tb_zhao_shell.sv` | the new pin connected (drivers untouched) |
| `tests/CMakeLists.txt` | `proj_matw_directed`, `proj_matw_mutant_control` registered (`fast;nightly`) |
| `design/blocks.yml` | `mat_refused` added to `counter_catalog` and to GEOM.PROJECT / TERRAIN.PROJECT `counters:` — **minimal, localised**; `check_counters.py` resolves it by the default `<name>_o` |
| `design/contracts/GEOM.PROJECT.md`, `TERRAIN.PROJECT.md` | register-map and malformed-input sections gain the `MATW`/refusal paragraph; GEOM's "27-bit narrowing" follow-up corrected to 18 |
| `reports/OWNER-RULINGS-20260909-2300.md` | R1 marginal corrected inline (−10 → −3; 112 → 119), in the style R4's correction already used |

Nothing in `design/fit_targets.yml` or `design/prod_manifest.yml` changed.

## 2. The decision: deterministic refusal, counted — and exactly what it covers

The owner ruled the FOV floor, so **legal content never produces a coefficient
outside ±1.99998**. The question left to the hardware is what happens to
*illegal* content, and the repo's law is deterministic refusal, counted, never
a silent clamp. Three shapes were on the table:

| option | what a `cot(fov/2)` one LSB over the cap does | verdict |
|---|---|---|
| **saturate** to ±1.99998 | projects a plausible, slightly wrong picture; nothing says so | rejected — the silent-drift failure |
| **wrap** (store the low 18 bits) | +2.0 becomes **−2.0**: a mirrored, in-range, entirely plausible picture; nothing says so | rejected — this is what the mutant does, and it is the worst of the three |
| **refuse and count** | the previous coefficient stays in force (a *stale* picture rather than a *wrong* one) and `mat_refused_o` names the fault | **chosen** |
| elaboration-time contract only | nothing at runtime | rejected — the coefficient is written at runtime by software; elaboration cannot see it |

**What is checked: the nine product words only** — columns 0..2 of rows 0, 1
and 3 (addresses 0,1,2,4,5,6,12,13,14). These are the only words that enter a
multiplier. **What is not checked, deliberately:**

* **The translation column (words 3, 7, 15)** is a *shift* (`<<< 16`), never a
  multiply. It keeps the full 32 bits at every `MATW` — free, and necessary:
  `m15` is the camera distance and `m3`/`m7` carry world-sized offsets, none of
  which fit ±2.0. This is the "different range" the brief flagged, and it is
  harmless precisely because it never touches a DSP.
* **Row 2 (words 8..11)** is inert by construction and stays writable and
  unchecked, so the register map remains a plain sixteen-word block.

The check is the exact one — bits `[31:MATW-1]` all equal to the sign bit —
not a magnitude compare, so the boundary is asymmetric by one LSB exactly as
two's complement is: **−131072 (−2.0) fits, +131072 (+2.0) does not.** The
floor is therefore a hair above 53.13° for a positive `m11` and exactly 53.13°
for a Y-flipped (negative) one. Both boundaries are driven in the test.

At `MATW=32` the check is the constant 1 (a generate branch, not a comparison)
and the counter is **structurally zero**. It is not "a counter that cannot
fire": the same RTL at `MATW=18` fires it 56 times in the directed test. The
counter saturates at `0xFFFF_FFFF`; saturation is not reached in simulation and
is stated as such in §9.

## 3. Evidence — MATW=32 is bit-identical to HEAD

All existing projector suites, **drivers untouched**, built in a lane-local
tree (`C:\programmieren\zencrifice\build-matw`, configured with the
`windows-native` preset) and run with one `ctest` invocation plus the
nightly-depth modes directly:

| test | what it pins | result |
|---|---|---|
| `geom_project_directed` | default core vs `zref::render::project_vertex` (directed) | 900 checks passed |
| `geom_project_random --random 100` / **`--random 2000` (nightly)** | same, random | 8,218 / **151,106** checks passed |
| `terrain_project_directed` | terrain wrapper vs oracle, directed rails | 2,313 checks passed; 128 tris in 423 cycles (unchanged from the row-multiplex report) |
| `terrain_project_random` / **`--nightly`** | matrix words at **full s32** (25% of entries), 2,736 row saturations, 242 divider rails | 2,089 / **20,809** checks passed |
| `terrain_project_chain` | wrapper in its chain | 28 checks passed |
| `proj_rowmux_directed` | **RPP=1 byte-identical to RPP=3** at MATW=32, four stall patterns, mid-sequence cfg write, positive control | 38 checks passed; L(3)=37, L(1)=40, II=3; skew control 5/64 |
| `proj_service_rowmux_smoke` | the shared service's arbiter at RPP=1 | 413 checks passed |
| `lint_zhao_geom_project`, `lint_terrain_project` | `verilator --lint-only -Wall` | passed |

11 of 11 ctest entries passed (4.7 s wall). No driver, oracle or vector changed;
only the three benches gained the new pin.

The proof that matters most here is `terrain_project_random`: its random
section draws a quarter of all sixteen matrix words from the **full s32**
(`terrain_project_random.cpp:214`), so the claim "the parameterised functions
degenerate to the old ones at 32" is exercised over the whole operand, not over
the 18-bit band the geom suite uses. `proj_rowmux_directed` passing unchanged is
the second half: RPP=1 still equals RPP=3 byte for byte, so the two levers
compose.

## 4. Evidence — MATW=18 is invisible on legal content, and the refusal law works

`proj_matw_directed` — **242 checks passed.** Three instances on separate
cfg buses and enables: `r` = MATW=32 RPP=3 (reference), `d` = MATW=18 RPP=3,
`s` = MATW=18 RPP=1.

| section | what it asserts | result |
|---|---|---|
| 1 stream equality | six matrices × 360 vertices (full-s32 extremes, tiny fractions, near-plane pressure, random spread, both views) × two narrowed instances under rotating stall patterns; refusal counter zero on all three throughout | **0 mismatching records** in all 12 comparisons |
| 1 corpus honesty | live / behind / guard-rail per config | pose 200/160/93 · edge 296/64/123 · rnd18 a–d 177/183/177 · 182/178/180 · 352/8/352 · 162/198/162 |
| 2a refusal sweep | 6 illegal values × 9 product words, view 0, narrowed instances only | counter +1 per write on both, **54 total**; reference 0; afterwards the never-written reference still equals both narrowed streams (72 records each) — the register held |
| 2b boundary | +131071 / −131072 accepted on all three (streams move, so the writes mattered); +131072 refused; −131073 on **view 1** refused | 2 more refusals, **56 total**; streams equal |
| 2c untouched words | 3/7/15 and 8..11 at `0x12345678`, `0x80000000`, `0x7FFFFFFF`, `0xFFFE0000` on all three | no refusal; streams equal; row-2 writes move nothing |
| 3 latency | fixed on all three; `d` = `r`; `s` = `r` + 3; II(`s`) = 3 | L = 37 / 37 / 40 en-cycles, II = 3 |
| 4 positive control | +1 raw on `d`'s m[5] and `s`'s m[1] (in range) | 6 of 72 records differ on each — the comparator fires |

The six matrices are: the row-multiplex suite's pose-plausible one (every
product word already `>> 14`, i.e. exactly the 18-bit band); an **edge**
matrix with the nine product words on `{+131071, −131072, 0, +1, −1, ±65536}`
in two different orders per view, the translation column on `INT32_MAX` /
`INT32_MIN`, row 2 at junk; and four **random** matrices with product words
over the full 18-bit range and every other word over the full s32.

`proj_matw_mutant_control` — **14 checks passed, inverted polarity**: on
legal content the mutant matches the real core on 120/120 records (the weak
vector — a broken guard is invisible there); after writing `+2.0` to m[5] its
counter reads **0** and the differential shows **20 of 120** records differing
(75 live); after `INT32_MIN` to m[0] (wraps to 0) the counter still reads 0
and **25 of 120** differ.

### 4a. The checker seen to fail: the directed driver against the mutant

A scratch copy of `tb_proj_matw.sv` with **both narrowed instances replaced by
the committed mutant** (reference still the real core) was built standalone and
driven by the unmodified `proj_matw_directed.cpp`:

```
  pose       live=200 behind=160 guard-rail=93 1/w-rail=0      <- section 1: all six configs PASS
  edge       live=296 behind=64  guard-rail=123 1/w-rail=0        (legal content: the mutant hides)
  rnd18-a..d ...                                                   (weak vector, as designed)
  FAIL ...:551: refusal total: expected 54, got RPP=3 0 RPP=1 0   <- section 2: the counter is silent
  FAIL ...:463: after-refusal/RPP=3 (register kept its value): 36 mismatching records
  FAIL ...:580: +131072 (2.0) was not refused
  FAIL ...:583: -131073 was not refused (view 1 does not count)
  ... (boundary 63/63, wide-translation 58/58, row2 58/58 mismatching)
  measured: L(ref)=37  L(MATW=18,RPP=3)=37  L(MATW=18,RPP=1)=40  II=3   <- section 3 PASS
  positive control: skewed words -> 6 / 6 of 72 records differ         <- section 4 PASS
proj_matw_directed: 151 of 274 checks FAILED
```

Every one of the 151 failures is in the refusal section; every other section
passes. That is exactly the partition a correct checker must produce against
this mutant: it cannot see the fault on legal content (nobody can — the mutant
is byte-identical there) and it sees nothing but the fault on illegal content.

### 4b. What the corpus reaches, honestly

Per configuration the golden stream holds both live and behind-the-eye records
and drives the guard-band rails (93–352 rail hits per 360 records). **The `1/w`
saturation rail (`out_d_o = INT32_MAX`, needs `w == 1` raw) is NOT reached by
this corpus** at either width; at MATW=32 it is covered by
`terrain_project_directed`'s directed rails, at MATW=18 it is not covered by any
test. Listed in §9.

## 5. The marginal DSP value, re-derived — and the eleventh claim

The brief asked me to re-derive the marginal and check its numbers. **The
brief's marginal is wrong**, and so is the docket line it came from.

The instrument is `tools/budget/calibration.json` (Quartus 17.0.2, 5CSEBA6U23I7,
`ioreg`, this tool and device), asymmetric points:

| a × b | DSP | decomposition |
|---|---:|---|
| 32 × 32, 32 × 27 … 32 × 20 | **3** | 2 × independent 18×18 + 1 × sum-of-two |
| 32 × 19 | **4** | 4 × independent 18×18 — worse than 32×32 |
| **32 × 18** | **2** | 2 × independent 18×18 |
| 27 × 27 and every pair ≤ 27 | **1** | |

The core has, per instance, **nine row products** (matrix word × vertex
coordinate) and **two `fx_mad` products** (`ndc` × viewport half-extent,
32×27). R1 narrows the *matrix* side of the row products only. The vertex stays
s32, and the `fx_mad` sites never touch the matrix.

| configuration | sites × DSP | total | status |
|---|---|---:|---|
| two instances, RPP=3, MATW=32 | 2 × (9×3 + 2×3) | **66** | MEASURED (fit, twice: 33 + 33) |
| shared service, RPP=3, MATW=32 | 9×3 + 2×3 | **33** | MEASURED (one core = 33) |
| shared, RPP=1, MATW=32 | 3×3 + 2×3 | **15** | STRUCTURAL |
| **shared, RPP=1, MATW=18** | **3×2** + 2×3 | **12** | STRUCTURAL |
| two instances, RPP=3, MATW=18 (the lever alone) | 2 × (9×2 + 2×3) | 48 | STRUCTURAL |
| shared, RPP=3, MATW=18 | 9×2 + 2×3 | 24 | STRUCTURAL |

So MATW=18 is worth **−18 taken alone** on two spatial cores (66 → 48 — the
brief's "alone" figure is right), **−9** on the shared spatial service, and
**−3 marginal** after `ROWS_PER_PASS=1` (15 → 12).

**Why "15 → 5" cannot happen.** It requires every one of the five sites to fall
from 3 DSP to 1, and the calibration grants 1 DSP only when *both* operands are
≤ 27 bits. For the three row sites that means narrowing the **vertex** to ≤ 27
— the world-size question R1 explicitly does not touch ("VERTICES ARE
UNTOUCHED"). For the two `fx_mad` sites it means narrowing the post-division
`ndc` operand, which the matrix width cannot influence at all. The number
appears to have been formed as "18 bits ⇒ 1 DSP per site" — true for a symmetric
18×18, false for the asymmetric 32×18 this core actually has. It went into the
docket as an inherited summary and was never re-derived from the sites.

Corrected inline in `reports/OWNER-RULINGS-20260909-2300.md` (R1 correction
subsection; the docket table row now reads −3 and the total **119 against 94**,
leaving 25 rather than 18). Still carrying the old figures and **not** edited
here, because they are outside this lane: `docs/OWNER_DOCKET.md:486` ("66 → 33
→ 11", which assumed 27 bits — 27 buys nothing) and the "Not verified" section
of `reports/OWNER-QUESTION-FOV-FLOOR-20260909.md` (−10). Both should be read
against this report.

**A follow-on that could recover some of it, unmeasured:** a hand-split of each
32×18 into `a[31:16]×b` and `a[15:0]×b` (two 18×18 partials, the 16-bit
shift-add in ALMs) might pack into ONE DSP block in "two independent 18×18"
mode, taking 12 → 9. That is a MapOnly experiment, a few minutes, and it belongs
behind the gate in §7 — stated here so nobody reads 12 as the floor.

### The other checks on the brief

* **"Nine matrix words be carried as signed 18-bit."** Nine *product* words.
  The register map still has sixteen; three of them (the translation column)
  stay 32-bit and must, because they carry world-sized values. The brief's
  phrasing is right only if "nine" is read as "the nine that multiply".
* **"±1.99998 is the only constraint the nine coefficients face."** It is the
  only *representational* constraint, and it is sufficient **given two
  assumptions the ruling does not state**: the view transform is **rigid**
  (rotation + translation, no scale, no shear — a world-unit scale folded into
  the matrix multiplies every coefficient regardless of FOV) and the frustum is
  **symmetric** (an off-axis projection puts `(r+l)/(r−l)` terms into rows 0/1
  via the view's z-row; at 60° vertical, 1.732, there is headroom; at the floor
  there is none). Both hold for the cameras described in the question. The
  refusal law is what turns a violation of either into a counted fault rather
  than a picture. Recorded in the core's header.
* **"The existing random test sweeps matrix entries only to 19 bits."** It is
  **exactly 18** (`>> 14` spans `[−131072, 131071]`; the source report's own
  addendum already corrected 19 → 18), and the statement is true of
  `geom_project_directed` only: **`terrain_project_random.cpp:214` sweeps
  matrix words over the full s32** for a quarter of the entries. So the
  MATW=32 identity proof does have full-domain matrix coverage — from the
  terrain suite, not the geom one.
* **"Vertices stay ±32,768 world units."** Untouched; the differential sweeps
  them to `INT32_MIN`/`INT32_MAX` at both widths and the records agree.

## 6. The bill, split honestly

**MEASURED (by an instrument that ran today)**

* MATW=32 bit/cycle identity to HEAD — the existing suites (§3).
* MATW=18 byte-identity to MATW=32 on legal content, at RPP=3 and RPP=1, under
  stall — `proj_matw_directed` (§4).
* The refusal law: counter fires exactly once per illegal write, register
  holds, boundary values behave as two's complement — `proj_matw_directed` §2.
* The checker fails against the committed mutant, only in the refusal section —
  §4a; the mutant control passes with inverted polarity.
* Latency: MATW=18 RPP=3 = reference; RPP=1 = reference + 3, II = 3.
* Lint clean (`-Wall`) at every legal parameterisation of core, service, both
  wrappers and every bench; `check_quartus17_syntax.py` clean;
  `gen_prod_top.py --check` fresh; `check_prod_manifest.py` OK;
  `check_counters.py` resolves `mat_refused` → `mat_refused_o`.

**STRUCTURAL PREDICTION (arithmetic on measured calibration rows; no map or fit
of this RTL at MATW=18 exists)**

* 2 DSP per row product at MATW=18 (calibration row `32x18`), hence the table in
  §5: 12 at (shared, RPP=1, MATW=18), 48 alone.
* 252 fewer registers at MATW=18 (9 words × 2 views × 14 unread bits removed by
  synthesis).
* ROW_W 68 → 54 shrinks the three row adders.
* The **Quartus inference of `mulm`** as an 18×32 product. The function
  sign-extends both operands to `MATW+32` bits before multiplying — the same
  shape `mul32` had, which Quartus mapped to exactly 3 DSP — so the same
  trimming is expected, but expected is not measured.

**UNKNOWN**

* Fmax at MATW=18. The row cone loses nothing and the adders narrow, so it
  should not worsen; the standing worst path (73.62 MHz, `mat → view mux →
  Mult0 → row adder → s1`) is unmeasured at this width.
* ALM at MATW=18 (calibration says ~33 ALM less per multiply for 32×18 vs 32×32,
  ~300 per instance at RPP=3; never measured on this core).
* Whether the hand-split follow-on reaches 1 DSP per row product.

## 7. The ONE fit gate, and its exact question

**`quartus_map` (MapOnly) of `zhao_project_service` at `ROWS_PER_PASS=1,
MATW=18`.** Question: *does the DSP count read exactly 12 — three row products
at 2 (each decomposed as "Two Independent 18x18") plus two `fx_mad` products at
3 — and does the register count fall by ~252 against the same map at
`MATW=32`?* A reading of 15 means Quartus did not narrow the operand (check
that the product's `a` operand is 18 bits in the map's multiplier report); a
reading above 15 means the cone grew and the change must not ship.

This is a map, minutes, not a fit; it answers area only. Fmax and the full ALM
figure ride the projection-subsystem fit that `ROWS_PER_PASS` and the shared
service are already waiting on — it should not run before the arena lands (the
service's own header says one core is infeasible without it), and it should not
run twice.

## 8. Things found on the way that are not about MATW

* **The generated prod top was restored to HEAD mid-session by another lane.**
  After my regeneration `gen_prod_top.py --check` said fresh; minutes later it
  said STALE, the working copy was 4,770 lines with CRLF endings — HEAD's content
  as `autocrlf=true` would write it — and the only other modified RTL
  (`zhao_geom_cull.sv`) had changed a *parameter*, not a port. I regenerated
  again; the diff against HEAD is exactly the two projector hunks. **Whoever
  commits first should run `gen_prod_top.py --check` at commit time**, because
  two lanes are touching the working tree and this file belongs to neither.
* **`npm run ledger:check` fails on HEAD, not on this change**: block 38
  `TERRAIN.SHADE` has an extra `tests.oracle` key and block 92 `FORGE.PRIM.EVAL`
  has `commit: pending` twice. Both from 2026-09-09 sessions. My `blocks.yml`
  hunks are the catalogue entry and two `counters:` lists.
* **Verilator 5 exposes top-level ports as reference members**, so a
  pointer-to-member abstraction over a multi-instance bench does not compile;
  capture-less lambdas as function pointers do. Cost one build.
* **A lane-local CMake tree under the scratchpad path exceeds Windows `MAX_PATH`**
  once Verilator's `CMakeFiles/<target>.dir/V<top>.dir/V<top>__Syms__Slow.cpp`
  is appended (`Can't write file`). A short sibling path
  (`C:\programmieren\zencrifice\build-matw`) works. Configure re-verilates
  every one of ~330 targets (`--make json`) and takes ~15 minutes.

## 9. Not verified — per item, naming the instrument

| claim | status | what would verify it |
|---|---|---|
| 2 DSP per row product at MATW=18 | STRUCTURAL | the §7 map |
| 252 fewer registers, ROW_W adders narrower | STRUCTURAL | the §7 map's register/ALM lines |
| Fmax unchanged or better at MATW=18 | UNKNOWN | the projection-subsystem fit |
| `mat_refused_o` saturates at `0xFFFF_FFFF` | not simulated (2³² writes) | a forced-value simulation; low value |
| the `1/w` rail (`out_d_o = INT32_MAX`) at MATW=18 | not reached by the differential corpus | a directed `w == 1` vertex added to `make_corpus`; covered at MATW=32 by `terrain_project_directed` |
| the elaboration `$fatal` on MATW=19 | **DEMONSTRATED** — `--lint-only` is silent at `-GMATW=19` (it does not run `initial`), so a standalone model was built and run: `%Fatal: zhao_project_core.sv:530: ... MATW (19) must be 32 or 18`, rc 1; the same driver at `-GMATW=18` runs through, rc 0 | done today; not a committed test — a negative-elaboration shape does not exist in the CMake lane yet |
| Quartus 17 synthesizes the new forms (typed `localparam logic signed [ROW_W-1:0]`, `{(33 - MATW){...}}` replication, generate-guarded `assign`) | `check_quartus17_syntax.py` clean, which checks the three known-bad forms only | the §7 map is the first `quartus_map` this file will see since the change |
| `zhao_prod_top.sv` elaborates with the new pins | `gen_prod_top.py --check` fresh; not verilated | the production fit |
| `tb_zhao_shell` still builds with the new pin connected | lint of the edit not run (the shell bench pulls the whole console) | `shell_project_path_directed` in the main tree |

## 10. What I verified versus inherited

**Verified today:** every row in §6 MEASURED; the calibration rows quoted in §5
(read from `calibration.json`, not from a report); the prod-top diff scope; the
coverage claims about both random suites (read the code, not the report).

**Inherited, and stated as inherited:** that `terrain_project_directed` drives
the `1/w` rail (its header says so; I did not re-read the vector); the 33-DSP
fit rows for the two wrappers (`fit_targets.yml` comments and the service
header); the 73.62 MHz worst-path figure.
