# The dense-seal mechanism lives in the sanctioned primitive now, and the proof went with it

2026-09-09, run RUN-20260909-2120-vertex-arena-dense-seal. Consolidation of the
`zhao_proj_arena3` design study's one genuinely new idea into
`zhao_vertex_arena`, per the 2026-08-24 owner ruling both files quote and per
`reports/ARENA-I-BUILT-A-SECOND-ONE-20260909.md`. Nothing committed; everything
sits in the working tree for review. **No Quartus fit was run** — the one gate
and its question are named in §8.

Every number below is labelled MEASURED (an instrument ran this session),
STRUCTURAL (follows from the RTL's shape; the named instrument verifies it), or
UNKNOWN (with the instrument named). §10 is the not-verified ledger; §11 is
where the brief itself was wrong.

---

## 0. What is in the working tree

| file | status | what |
|---|---|---|
| `fpga/rtl/geometry/zhao_vertex_arena.sv` | MODIFIED | `VALID_MODE` parameter (0 = bitmap, default; 1 = dense seal); linear-addressing repair; `arena_seal_short_o` sticky output; formal extended |
| `fpga/rtl/geometry/zhao_geom_wcache.sv` | MODIFIED | `PAYLOAD_W` 75 → 106 (the `w` repair); explicit `VALID_MODE(0)`; ties off the dense-only sticky bit |
| `fpga/rtl/prod/zhao_prod_top.sv` | REGENERATED | `gen_prod_top.py`; exactly two width lines moved (75 → 106) |
| `tests/formal/geom_wcache_arena_bounds.sby` | MODIFIED | 3 tasks → 8: the original three untouched, plus `prove_np2`/`cover_np2` (non-power-of-two bitmap) and `bmc_dense`/`cover_dense`/`prove_dense` |
| `reference/include/zref/zref_geom_wcache.hpp` | MODIFIED | oracle grew a dense mode (default-off; existing callers unchanged) |
| `tests/geometry/vertex_arena_dense_directed.cpp` | NEW | dense differential at terrain's 4×81 shape, 394 checks |
| `tests/geometry/vertex_arena_dense_seal_control.cpp` | NEW | inverted-polarity positive control for `arena_misses_o`, drives the mutant |
| `tests/mutants/zhao_vertex_arena_dense_mutant.sv` | NEW | committed seal-guard mutant (`==` → `<=`), one substantive line |
| `tests/CMakeLists.txt` | MODIFIED | registrations for the two new lanes |
| `design/contracts/GEOM.WCACHE.md` | MODIFIED | dated corrections: 106-bit payload, linear addressing, VALID_MODE |
| `design/prod_manifest.yml` | MODIFIED | `zhao_proj_arena3` note records the consolidation as DONE, and corrects the "58 formal assertions" claim (§11) |

`zhao_proj_arena3.sv` is untouched, as instructed — it stays the registered
design study.

## 1. The mechanism, folded in as a MODE rather than forked

`VALID_MODE = 0` (named `VALID_BITMAP` in the module) is bit-for-bit today's
behaviour and the default; `VALID_MODE = 1` (`VALID_DENSE_SEAL`) is the design
study's fourth option: an accepted fill's **index must equal the arena's fill
count** (in order, exactly once), and a **seal is refused unless the count is
exactly DEPTH** — refused seals sticky on the new `arena_seal_short_o`, refused
fills folded into the existing sticky `arena_overflow_o` drop class. "Written
this lifetime" becomes `index < count`: ARENAS × $clog2(DEPTH+1) registers
instead of ARENAS × DEPTH.

Two deliberate deviations from `zhao_proj_arena3`'s shape, both improvements:

* **The fill still presents its index.** The study deleted the index port (the
  counter IS the address); the primitive keeps the port and REFUSES a
  mismatch. Same dense property, but a desynchronised producer is *observable*
  (drop + sticky) instead of silently landing rows one off, and the port
  contract is unchanged across modes — one shell can switch modes without
  rewiring.
* **The study's key/ref-count/release machinery did not come along.** Group
  provenance keys and consumer reference counting are SHELL business (the
  generation already answers staleness in the primitive); folding them in
  would have made the primitive a second copy of the study instead of the
  study a mode of the primitive.

Exactly four wires cross the mode boundary (`fill_gate_c`, `seal_gate_c`,
`seal_short_now_c`, `slot_written_c`); the store, refusal order, reply
register and counters are mode-blind. Explicit `generate`/`endgenerate`, and
the elaboration guard sits inside `initial begin` — both Quartus-17 laws.

## 2. The addressing repair the study also paid for (not in the brief — §11)

`zhao_proj_arena3`'s header contains a SECOND real finding the brief did not
mention: *"ADDRESSING IS PADDED, NEVER MIXED"*. The sanctioned primitive
addressed fill/lookup by the concatenation `{arena[AW-1:0], index[IW-1:0]}` —
a padded 2^IW stride — into an UNPADDED `ARENAS*DEPTH` array, while its own
open-clear loop and the zref oracle both index linearly (`arena*DEPTH + i`).
At any non-power-of-two DEPTH those disagree, and at the shell's shipped
2×1089 shape **arena 1's rows 130..1088 (88% of it) addressed past the end of
the array** — fills lost, lookups missing, the replay premise silently void
for one of the two views. MEASURED reachable by inspection; never observed
because every instrument ran powers of two (directed 16, formal 4).

Repaired to linear `arena*DEPTH + index` (constant-multiply shift-add fabric,
no DSP; padding was rejected because it buys no M10K aspect here and costs
2^(AW+IW) − ARENAS·DEPTH rows). At power-of-two depths linear and concat are
arithmetically identical bit for bit, which is why the default-mode
bit-identity claim (§4) survives the repair. The `prove_np2` task now holds
the bounds law at DEPTH=6 permanently, and the dense directed suite runs at
DEPTH=81.

## 3. The formal extension — the central requirement

All **eight tasks PASS** (sby 0.68, abc pdr / btor btormc, this session;
whole sweep ≈ 5.5 min wall with sby's task parallelism, against the lane
wrapper's 1800 s):

| task | config | engine | result | time |
|---|---|---|---|---|
| `bmc` | mode 0, DEPTH=4, depth 16 | btormc | PASS | 251 s |
| `cover` | mode 0, DEPTH=4 | btormc | PASS, 6/6 covers reached | 252 s |
| `prove` | mode 0, DEPTH=4 | abc pdr | PASS (inductive) | 261 s |
| `prove_np2` | mode 0, **DEPTH=6** | abc pdr | PASS (inductive) | 261 s |
| `cover_np2` | mode 0, DEPTH=6 | btormc | PASS | 261 s |
| `bmc_dense` | mode 1, DEPTH=4, depth 14 | btormc | PASS | 299 s |
| `cover_dense` | mode 1, DEPTH=4 | btormc | PASS, **7/7 covers reached with traces** | 14 s |
| `prove_dense` | mode 1, DEPTH=4 | abc pdr | **PASS (inductive)** | 10 s |

(The ~250 s figures are wall time under parallel contention; `prove` alone
measured 4 s before this work started.)

**Assertion disposition, as the brief demanded:**

* **Hold UNCHANGED, proven in both modes** (module scope, no guard):
  `a_probe_key_bounded`, `a_probe_key_constant` (harness self-checks),
  `a_hit_is_the_watched_value`, `a_hit_implies_written` (the watched-slot law
  — the reason the file is the sanctioned one), `a_refuse_is_not_a_hit`,
  `a_reply_present`, `a_overflow_sticky`. The shadow keys off `fill_ok`,
  which each mode gates through `fill_gate_c`, so the same seven properties
  are re-proven under the dense elaboration without edits.
* **Mode-guarded by placement** (moved into `g_bitmap`, no expression
  change): `p_array_implies_shadow` — it differences the BITMAP against the
  shadow, and dense mode has no bitmap. Its three-day diagnostic saga stays
  in the shared block, unedited. `c_miss` moved with it: dense mode PROVES a
  miss unreachable, and an unreachable cover reds the lane forever (the
  `c_fill_addr_alias` reasoning).
* **NEW, dense-only** (inside `g_dense`): `p_cnt_implies_shadow` (the count
  analogue of `p_array_implies_shadow`, and pdr's induction helper);
  `a_dense_fill_in_order` (fill is in-order — accepted index equals count);
  per-arena loop invariants `cnt <= DEPTH` and `sealed ⇒ cnt == DEPTH` (seal
  implies complete, over EVERY arena); `a_dense_no_miss` (a reply is a hit or
  a refusal, never a miss — read implies sealed implies complete implies
  written). New covers `c_dense_sealed`, `c_dense_seal_short`.

**The proof earned its keep on the first run.** `prove_dense` FAILED at step
7 before any directed test existed: a seal racing an open of the same arena
won the `sealed` bit (bitmap's historical resolution, harmless there because
open also clears the bits) while open zeroed the count — an arena SEALED and
EMPTY. The dense seal now loses that race, refused and sticky. That is a
semantic bug in my first dense implementation that 394 directed checks were
then written around; the solver found it in ten seconds from the invariant
alone.

Extending the proof was **practical** — no impediment was hit, and the
directed suite is corroboration, not a substitute.

## 4. Default behaviour: the evidence it did not move

* `bmc`/`cover`/`prove` run the EXACT pre-consolidation configuration
  (VALID_MODE=0 explicit, same DEPTH=4/GEN_W=3/PAYLOAD_W=8, same depths,
  same engines) and PASS — including `prove`, the inductive result.
* The existing differential suites, rebuilt against the modified RTL at their
  registered 2×16×64 shape: `geom_wcache_directed` **73/73**,
  `geom_wcache_random` **7/7**. MEASURED this session.
* The bitmap branch's structure is the old code moved, not rewritten: the
  valid-bitmap register, its reset, the open-clear/fill-set ordering and the
  gate wires (`fill_gate_c = 1`, `seal_gate_c = 1`) reduce every mode-shared
  expression to its previous text. At power-of-two depths the address change
  is the identity (§2).
* What this does NOT claim: gate-level equivalence (no LEC tool in the tree
  — §10), and at NON-power-of-two depths default behaviour deliberately
  changed, from out-of-bounds to correct. That is the §2 repair, stated, not
  smuggled.

## 5. The saving, per shape — REGISTER counts, not ALM

Neither `zhao_vertex_arena` nor `zhao_geom_wcache` has any fit row anywhere in
`reports/synthesis/`; their ALM is **UNKNOWN**, and none of the numbers below
is one. These are flop counts of the valid mechanism alone, STRUCTURAL from
the declarations:

| shape | bitmap (VALID_MODE=0) | dense (VALID_MODE=1) | saving |
|---|---:|---:|---:|
| 2×1089 (GEOM.WCACHE, shipped) | 2,178 | 2×11 + 1 = 23 | 2,155 registers — **hypothetical only, see below** |
| 4×81 (terrain shell, proposed) | 324 | 4×7 + 1 = 29 | **295 registers** |
| 2×4 (formal shape) | 8 | 2×3 + 1 = 7 | 1 register |

The 2×1089 row is labelled hypothetical because **GEOM.WCACHE cannot take
dense mode**: its producer fills on lookup misses, in triangle order — sparse
by nature — so the shell stays `VALID_MODE(0)` and its 2,178 bitmap flops are
the honest price of sparse fill. The dense customer is the terrain shell,
whose tessellator projects a whole subpatch before replay and is dense by
construction. The saving the consolidation actually banks today is the
terrain row, plus the deletion of a forbidden second primitive.

## 6. The payload widening, and the header's claim verified

`PAYLOAD_W` moved 75 → 106; the field map gains `w[104:74]` (guarded clip w,
fx16 raw, the core's own 31 bits) and `behind` moves 74 → 105. x/y/invw stay
put. Grounds: `GEOM.DEPTHQUANT.md`'s 2026-09-03 correction and
`reports/WCACHE-DROPS-W-20260909.md`; the 31-vs-40-bit reconciliation is
already settled with citations in `reports/TERRAIN-REARCHITECTURE-20260909.md`
§4 (DEPTHQUANT's 40-bit port is clamped headroom; zero-extending the guarded
31-bit w is lossless).

**"When it is settled, this parameter moves and nothing else does" — verified,
with one amendment.** A tree-wide sweep found no other stored copy of the
75-bit layout: the primitive, the oracle, the directed suites and the formal
proof all carry `PAYLOAD_W` symbolically, and no document outside the shell's
own header spells the field map. Two things did have to move, both mechanical:
the generated `zhao_prod_top.sv` (regenerated; exactly two width lines — its
harness wires are sized from the module's defaults) and the shell header's own
field map. So the claim is true of every HAND-MAINTAINED artefact and false
only of the generated one, which is precisely what `gen_prod_top.py` exists
for. `check_prod_manifest.py`: OK after regeneration (212 modules, 66 tops).

Memory arithmetic at the new width, GEOMETRY calculation, not a fitted
receipt: 2×1089×106 = 230,868 bits linear (the padded alternative would be
434,176 — the other reason §2 chose linear). Whether it INFERS is the fit
gate's question.

## 7. What the terrain shell needs (the ruling's anticipated second instance)

Per `reports/TERRAIN-REARCHITECTURE-20260909.md` §4, now concretely
satisfiable against the primitive in this tree:

* **Instantiation:** `ARENAS=4` (2 views × 2 working generations),
  `DEPTH=81` (9×9 subpatch), `PAYLOAD_W=106`, `GEN_W=8`, `VALID_MODE=1` —
  every one an owner-editable parameter at the instantiation site. The dense
  directed suite already runs THIS shape.
* **Producer discipline:** fill rows 0..80 in order, completely, then seal —
  the tessellator's natural walk. Misorder and short seals are refused,
  counted, sticky (`arena_overflow_o`, `arena_seal_short_o`): wire both to
  the shell's observation ports.
* **`w` at the source:** `zhao_terrain_project` exposes NO `w` today
  (WCACHE-DROPS-W, consequence 3) — the shell cannot fill a 106-bit record
  until the terrain projector (or the shared service, which already carries
  `out_w_o`) feeds it. Adopting `zhao_project_service` as the fill source
  resolves this without touching the legacy wrapper.
* **Replay width is the open shell question:** the primitive has ONE lookup
  port; `zhao_proj_arena3` argued three corner reads per clock. The shell
  answers it the way the ruling's own sentence permits ("does not mean one
  physical cache"): three primitive instances with a broadcast fill — same
  fill enable, same data, so a copy stays a copy — at 3× memory, which is
  the study's own 18-M10K arithmetic. A `REPLICAS` parameter inside the
  primitive is a possible later refactor; it was NOT added now because no
  consumer exists to say whether 3 is the number, and a knob nobody consumes
  is a guess baked into silicon.
* **Not primitive business:** the study's provenance keys and reference
  counting, if the replay consumers need them, live in the shell.

## 8. Can `zhao_project_service` be adopted? — plainly

**The stated blocker is cleared at the primitive level; adoption is now a
composition-and-measure task, not a design task.** The service header's
condition — *"do not instantiate this service in a composed top until the
vertex arena is in place"* — was about replay making one core's throughput
sufficient (23.9% of frame with dedup, vs 101.6% without; its own arithmetic).
The primitive now provides both replay stores: GEOM's shell carries the full
packet including `w`, and the terrain instance's mechanism is built, proven
and tested at 4×81×106. What remains before the service goes into a composed
top, in order:

1. the terrain shell RTL (§7) with its directed suite — Verilator only;
2. replay consumers wired (corner reads from the arenas; the service's
   client B as the fill source) — Verilator only;
3. **THE ONE FIT — the projection-subsystem gate.** Composition:
   `zhao_project_service` + `zhao_geom_wcache` (2×1089×106, bitmap) + the
   terrain shell (dense). Its question, stated in advance: *"Do the arena
   stores infer as block RAM at the real shapes (blockMemoryBits > 0 per
   instance, per the contract's pass/fail line), what ALM/DSP/Fmax does the
   subsystem measure, and does retiring the second projector land the −33
   DSP?"* Nothing else in this packet needs Quartus: correctness, refusal
   semantics, throughput-in-clocks and mode behaviour were all answered by
   sby and Verilator this session, in seconds to minutes.

The DSP ladder the brief cites holds unchanged: 66 (two cores) → 33 (one
shared core, `ROWS_PER_PASS=3`) → 15 (`ROWS_PER_PASS=1`, committed today,
one-vertex-per-three-clocks — legal only because replay divides demand by
5.64×; that arithmetic is the service header's own, corrected 2026-09-09).

## 9. Evidence ledger (all run this session, commands reproducible)

* **Lint** (`verilator_bin --lint-only -Wall`): arena mode 0, mode 1, mode 1
  @ 4×81, mode 0 @ 2×1089, wcache @ 106, mutant — all 0 diagnostics.
* **Quartus-17 syntax gate**: clean, 217 files, self-test 3-fire/6-no-fire
  first. (What this does and does not establish: §10.)
* **Formal**: the eight-task table in §3, including the three untouched
  default-mode tasks. `cover_dense` reached 7/7 with real traces — checked in
  the log, not inferred from the exit code (a cover PASS with zero traces is
  the broken-instrument shape).
* **Dense differential** `vertex_arena_dense_directed` @ 4×81: **394/394**.
  Within it, both sticky detectors SEEN TO FIRE on legal stimulus: a short
  seal fires `arena_seal_short_o`, a misordered fill fires
  `arena_overflow_o`.
* **Checker seen to fail:** the same 394-check suite compiled UNCHANGED
  against the committed mutant: **FAILS, 149/394**, first failure the
  seal-short family — the deliberate break is loud.
* **Unreachable counter demonstrated:** `arena_misses_o` cannot move in a
  correct dense arena (proved: `a_dense_no_miss`). The committed mutant
  (`seal_gate_c` `==` → `<=`) plus the inverted-polarity control
  `vertex_arena_dense_seal_control`: **PASSES with the counter at exactly
  1**, and it also asserts the mutation is live (`arena_seal_short_o == 0`
  where the real block sets it), so a regenerated-but-unbroken copy cannot
  pass vacuously.
* **Default-mode regression**: `geom_wcache_directed` 73/73,
  `geom_wcache_random` 7/7, rebuilt against the modified RTL.
* **Elaboration guard SEEN TO FIRE**: `--binary` build at `VALID_MODE=2`
  dies at time 0 with the named `$fatal` message, rc ≠ 0 — checked because
  `--lint-only` does not run initial blocks.
* **Hygiene**: `no_control_bytes.py` clean on all ten touched files;
  `check_prod_manifest.py` OK post-regeneration.
* Toolchain traps re-met and dodged, for the next reader: `cmd | tail`
  reports tail's RC (an exe's 127 masqueraded as 0); a Verilated exe run
  against msys64's libstdc++ dies 0xC0000139 (use the winlibs dir from
  `zhao-env.ps1`); standalone verilate links need `-std=gnu++17` even in
  `--binary` mode.

## 10. Not verified, with the instrument for each

| claim | instrument that would verify it |
|---|---|
| the 106-bit / 2×1089 and 106-bit / 4×81 stores INFER as block RAM | the §8 fit's map RAM summary — Verilator is structurally silent on inference, and `blockMemoryBits > 0` is the contract's own pass/fail line |
| ALM / DSP / Fmax of the arena, either shell, or the subsystem | the same fit; no fit row exists today, so there is no baseline either |
| the −33 DSP from single-core adoption | the same fit, after steps 1–2 of §8 |
| Quartus 17.0 ELABORATES the new generate/initial forms | `quartus_map` (a 33 s smoke per the 2026-09-08 note). The syntax gate passed and lint is clean, but "a block that has never been through quartus_map has not been shown synthesizable" — this RTL has not been, since the change |
| the CMake registrations build and the lanes pass under ctest | the next `cmake --preset` configure + `ctest`; this session ran the identical stimulus through standalone verilate builds instead, to stay out of the shared build tree's graph |
| the 8-task formal lane fits the CI wrapper budget | the nightly lane itself; measured ~5.5 min locally against the 1800 s wrapper, so the margin is wide but the CI machine is not this machine |
| bit-identity of the default mode at the GATE level | no LEC tool in the tree; the claim rests on the unchanged inductive proof + unchanged suites + the power-of-two address identity (§4), which is behavioural evidence, not netlist evidence |
| dense-mode benefit at the wcache's 2×1089 shape | nothing — the row is hypothetical by design (§5); the wcache producer is sparse and keeps the bitmap |

## 11. Where the brief was wrong (three agents found brief errors today; add these)

1. **"58 formal assertions" is not a real count.** The pre-consolidation
   file holds **8 labelled immediate assertions + 6 covers** (plus 6 harness
   assumes). 58 can only be reached by keyword-grepping a file whose
   comments narrate assertions extensively — a count of the word, not the
   property. The number also lives in `prod_manifest.yml` (now corrected in
   the working tree) and in `ARENA-I-BUILT-A-SECOND-ONE-20260909.md` (left
   as written; it is a dated record). Post-consolidation: 11 labelled
   assertions + 2×ARENAS unlabelled loop asserts + 8 covers across the two
   modes. The file's VALUE was never the count; it is that the assertions
   are proven inductively and non-vacuously.
2. **The brief credits `zhao_proj_arena3` with ONE real finding; it made
   two.** The second — mixed padded/linear addressing — turned out to be a
   live latent defect in the sanctioned primitive at its shipped 2×1089
   shape (§2), arguably worth more than the flop saving, and it was sitting
   in the study's header under "ADDRESSING IS PADDED, NEVER MIXED".
3. **"Instantiated by `zhao_geom_wcache.sv`, which is in `zhao_prod_top`"**
   is literally true and worth qualifying the way WCACHE-DROPS-W already
   did: that top is the generated area/PINMISSING harness and drives the
   shell from an LFSR. No live datapath carried the dropped `w`, the OOB
   addressing, or anything else here — these were traps laid for whoever
   composes it, now sprung on the bench instead.
4. Minor: the brief's "the write address IS the group's fill counter" was
   adjusted in the consolidation — the index port stays and a mismatch is
   REFUSED (§1). Same dense property, better observability, no port churn.

## 12. Implementation order (restated compactly)

1. **This packet** — review and commit (owner's call; nothing here is
   committed).
2. Terrain shell + its suite; terrain `w` source via the service (Verilator).
3. Replay consumer wiring (Verilator).
4. **The one fit** — §8's projection-subsystem gate, question pre-stated.
5. Adopt `zhao_project_service` in the composed top; retire the second
   physical projector; re-run the DSP census against the receipt.
