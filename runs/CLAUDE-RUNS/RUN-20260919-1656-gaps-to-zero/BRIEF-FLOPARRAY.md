# FLOPARRAY — two arrays in flip-flops, and NEITHER is free

**Branch `gz/floparray`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Phase 3, second and third targets.** PALRAM took the biggest one; these are the
next two, and the honest difference is in the title.

## The baselines exist — diff against them, not against prose

Both blocks had **no fit target at all** until 2026-09-26, so neither had ever
been measured on its own. Both now have one and both are measured:

| row | registers | mem bits | map time | digest |
|---|---:|---:|---:|---|
| `zhao_forge_assemble@flop-census-20260926` | **39,167** | 2,198 | 124.3 s | `46e4cae54a15` |
| `zhao_geom_lodstate@flop-census-20260926` | **10,826** | 0 | 41.2 s | `0dfe1327a272` |

Both track the composed entity table (39,005 and 10,801 subtree in
`reports/synthesis/console_entity_attrib.md`), so **a register saved in the leaf
is a register saved in the console.**

```
tools/quartus/run_block_fit.ps1 -Module zhao_forge_assemble -MapOnly -RowLabel '@<label>'
```
(`-RowLabel` must start with `@` or `-`.)

## Target 1 — `zhao_forge_assemble`, 34,840 bits

```systemverilog
logic [POSW-1:0] pos_q [MAX_VERTS];   // POSW = 1+21+21 = 43, MAX_VERTS = 520
logic [23:0]     inv_q [MAX_VERTS];
```

520 × 43 + 520 × 24 = **34,840 bits**, roughly 8,710 ALM, about **21% of the
shipping part.**

**Two blockers, both of which `check_ram_inference.py` already encodes**, so
unlike `pal_q` you are not starting from silence:

* **an ASYNCHRONOUS RESET LOOP over the array** — `:710-713`,
  `for (k = 0; k < MAX_VERTS; k++) begin pos_q[k] <= '0; inv_q[k] <= 24'd0; end`,
  inside the `!rst_n` branch of an `always_ff @(posedge clk or negedge rst_n)`.
  This is precisely the case `zhao_geom_drawjob`'s own comment warns about and
  deliberately avoids: *"The array itself has NO reset, which is what lets it
  infer M10K."*
* **a COMBINATIONAL READ** — `:579-580`,
  `wire [POSW-1:0] pos_rd_c = pos_q[rd_a_q];` and the same for `inv_rd_c`. A
  continuous assignment through a dynamic index forces a per-bit mux the width of
  the array.

## Target 2 — `zhao_geom_lodstate`, 9,216 bits

`st_q`, 9,216 bits, **9,985 own registers and ZERO memory bits in the console.**
Same two shapes: a combinational read through `slot_c`, and a reset loop. Smaller
(~2,496 ALM, ~6% of the part), and worth doing in the same packet **only because
the diagnosis is shared** — if it turns out not to be, say so and leave it.

## WHY THIS IS NOT PALRAM, AND WHY THE TITLE SAYS SO

`pal_q` cost nothing: its read already landed in a flop one cycle later, so the
M10K conversion was free. **These are not.**

* **Removing a combinational read costs a PIPELINE STAGE** on that path. That is
  a real timing and throughput change and it must be priced, not waved through.
  `zhao_forge_assemble` is on the geometry path and the console is already 3.5×
  over on logic — **an area win that costs throughput is a trade, and a trade has
  to be stated as one.**
* **A reset loop cannot simply be deleted.** It is doing something: the array is
  readable after reset. Replacing it needs an explicit **validity discipline** —
  a valid bit, a generation, or a proof that no read can precede a write. Note
  `zhao_geom_drawjob` keeps `pal_v_q` as separate flops *for exactly this
  reason*, and its comment says why: *"the valid bits are separate flops because
  they must be CLEARED by reset and an asynchronous clear on the array would
  destroy that inference."* **That is your template.**
* **If a conversion costs a cycle anywhere, say so in numbers and justify it.**
  Do not let it pass as free.

## What the last two packets learned, and you should not re-learn

* **PALRAM: the cause was a CONJUNCTION** — a part-select element write *and* an
  over-wide index, either one harmless alone. Its first single-cause story was
  wrong, and **only the controls it had already predicted caught it.** Run your
  controls even when you think you have the answer.
* **ATTRSETUP: the obvious hypothesis was simply false.** Narrowing every
  declared multiply width cost exactly zero; the cause was a minus sign taken
  inside a cast. **The coordinator's hypothesis in a brief is a hypothesis.**
  This one is better supported — both shapes here are rules the checker already
  encodes from measured failures — but **measure before you believe it.**
* **Both COMMITTED their probe** (`tests/probes/zhao_palram_probe.sv`,
  `tests/probes/zhao_attrsetup_mul_probe.sv`) with a `@probe-vN` / `@probe-mN`
  row per arm. Do the same: a probe written once and thrown away leaves
  unreproducible numbers.
* **One change at a time, map after each.** "I changed three things and it
  inferred" teaches nobody anything and will not survive the next edit.

## The fences

* **`MAX_VERTS = 520` IS A CAPACITY LAW, NOT A KNOB.** The block's own overflow
  path says so: *"MORE VERTICES THAN THE STORE HOLDS. The evaluators' own bounds
  make this unreachable with a legal job"*, and `vtx_overflow_o` counts it.
  Shrinking the store to save registers is the owner's prohibited move —
  *"NOT authority to delete a feature … or call reduced work equivalent merely
  to reach zero or fit a device."* Same for `POSW`: it is `{behind, y, x}` at
  1+21+21, and narrowing it narrows the canvas.
* **Behaviour must be identical** — same refusal semantics, same counters, same
  handshake. If the read gains a cycle, that is a declared change with a number
  beside it, not a silent one.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a single block is
  41–124 s and is yours; a console fit is 01:54:49 and is the coordinator's.

## Evidence bar

* **Before/after map rows for each block**, recorded in
  `reports/synthesis/zhao_block_fit.json`, with the RAM Summary showing the
  blocks now inferred. Baselines are the two rows above.
* **A causal answer**, from one-change-at-a-time maps, with a committed probe.
* **Equivalence proved, not asserted.** The existing benches must pass unchanged,
  and add a differential across the full `MAX_VERTS` range including the overflow
  boundary. Assert the CORRECT behaviour; keep any positive control separate;
  **do not write a test that asserts the bug.**
* **The pipeline cost, in clocks**, measured on the bench rather than reasoned
  about — and what it does to the block's throughput criterion.
* If a guard becomes unreachable with legal stimulus it needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  driver polarity inverted so it passes when the counter FIRES.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* does not set `$LASTEXITCODE` at all, so a loop can carry the
  previous command's zero. Both traps have been hit in this campaign this week.
* **`gate_sweep` does not run the console smoke controls.** Run them yourself if
  you touch anything the console composes, with proper switch binding
  (`& .\tests\prod\run_console_core_smoke.ps1 -Mutant`, or `@splat` in a loop —
  `& script $sw` binds the switch as a positional path and silently does
  nothing).
* **Check a file's committed mutant-copy count before your first edit.**
  `mutant_copy_drift` keys on COMMIT TIME, editing even a comment stales every
  copy, and reverting does not clear it.
* **Regenerate `zhao_prod_top.sv` after ANY port change** and re-run
  `tools/quartus/check_prod_manifest.py`.
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree**; after killing one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*` before restarting.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **Whether each array converted**, with before/after registers, memory bits and
   ALUTs — measured, not divided.
2. **The causal answer** for each, from one change at a time, with the probe rows.
3. **THE PRICE.** Clocks added, where, and what it does to throughput. If it is
   zero, prove it is zero.
4. **The validity discipline** that replaced each reset loop, and why it is
   sound.
5. **What you refused**, especially anything that would have moved `MAX_VERTS`,
   `POSW` or a capacity law.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/floparray` only.** Never rebase or push
   the shared branch, and never `--force`.
