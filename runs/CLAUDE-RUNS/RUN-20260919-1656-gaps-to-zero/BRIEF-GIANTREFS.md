# GIANTREFS — R7's giant is ALREADY being truncated. Raise the wall, derive the count, and make the breach visible.

**Branch `gz/giantrefs`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is not a reservation packet. Two packets have now proved a reservation
cannot be built yet, and REFPUSH proved why: the wall being protected is not the
binding one.** Read entry `I56` in `fpga/rtl/prod/zhao_console_core.sv`
(`grep -n '^// I56\.'`, never a line number) — GIANTQUOTA's and REFPUSH's
sections are measurements, not opinions. **Do not re-derive them.**

## THE FINDING YOU ARE ACTING ON

**The composed binner holds 1,024 tile references per frame.**
`zhao_shell_top_v2:1274` instantiates `zhao_geom_bin_pipe_v2` overriding exactly
one parameter (`ARENA_ID_W`), so `CHUNKS = 256`, `CHUNK_REFS = 4` and
`ref_ram [0:(CHUNKS*CHUNK_REFS)-1]` is **1,024**. `zhao_geom_arena` is a bump
allocator handed back whole at `frame_begin_i`, so nothing recycles inside a
frame.

**What the ruled workloads actually need**, re-measured by REFPUSH at its own
commit with `tools/render/count_bin_load.cpp` against the shipped `zref::Binner`
— *the same binning law the hardware implements*:

| workload | references | vs the 1,024 arena |
|---|---:|---:|
| **giant near camera, 126 tris** | **25,704** | **25.1×** |
| 256 creatures, no LOD | 30,609 | 29.9× |
| creature army, 200 × 96 | 23,912 | 23.4× |
| one terrain patch 32×32 | 4,080 | 4.0× |
| sky backdrop, 2 triangles | 396 | 0.4× |

**So R7's *"the giant is never silently truncated"* is ALREADY BREACHED, today,
in the shipped composition** — a frame holding a near-camera giant loses ~96% of
its tile references to the binner's overflow wall before the arena's quota is
consulted at all. **This is not a feature waiting to be built. It is a ruled
requirement the machine currently violates.**

**And the breach is UNOBSERVABLE.** `zhao_shell_top_v2:1360-1365` discards five
of the six binner instruments into `rp_*_unused`; only `binner_overflow_o`
survives as `render_overflow_o`, which `zhao_console_core` declares at `:10959`,
connects at `:20197` and **reads nowhere**, and no test reads it either. The
counters are proven to fire **at the leaf** (`geom_binner_directed.cpp:437`
asserts `triangles_culled == 12` on a real overflow) — which says nothing
whatever about the composed machine.

## THE JOB, in this order

1. **DERIVE `CNT_W`. It is a latent corruption and it must be fixed FIRST.**
   REFPUSH found it hardcoded `11`, sized to `CHUNKS*CHUNK_REFS` but **not
   derived from it**. Raise `CHUNKS` with this unfixed and every tile's count
   **silently wraps at 2,048** — a corruption, not an overflow, and **the
   safe-overflow wall would not fire.** Derive it (`$clog2`), and prove the
   derivation with a parameterisation that would have wrapped.
2. **RAISE THE REFERENCE CAPACITY to cover R7's 32,768.** REFPUSH priced it:
   raising `CHUNKS` alone measures **523,712 bits, +33 M10K, 9.3% of device
   block RAM — affordable.** Two committed `-MapOnly` rows on the shipping part
   back this, `@refpush-shipped` (191,296 bits) and `@refpush-giantrefs32k`
   (523,712 bits), same sources digest `14d58d825e87`, both `rtlCleanAtHead`.
   **The +33 M10K is a bits-equivalent FLOOR** — real packing granularity makes
   it somewhat higher, so measure what you actually build.
3. **RESTORE THE INSTRUMENTS**, which is cheap and is what makes any of this
   checkable. Reconnect the five discarded binner counters through the shell and
   **make something READ them** — a console-level assertion, not another
   pass-through. `render_overflow_o` reaching nobody is the exact shape this
   campaign refuses.

**If (1) and (3) land and (2) does not, that is still a good packet** — a
derived count and a visible breach are worth more than a silent bigger number.

## Why this is mine to authorise, stated so you can check it

The owner's directive **forbids shrinking the guaranteed giant** and fixes it at
**32,768 references**. The machine currently delivers ~1,024 of them. **Raising
the wall is not a feature choice; it is the minimum that makes a ruled
requirement true.** The resource direction is also already settled: **ALMs are
the binding constraint at ~97% of the device and memory is the slack**, so
spending M10K to honour a ruled capacity is the trade this project has
repeatedly chosen. **Do not spend ALMs to save M10K here.**

**If your measurement contradicts the +33 M10K, STOP AND REPORT IT.** A measured
engineering impossibility is a finding, not permission to invent a pass — and it
would be the owner's call, not yours or mine.

## The fences

* **32,768 references is the number. Do not trim, rescope or redefine the
  giant**, and do not "size to the workload we happen to test".
* **Do not raise a capacity without deriving every width that depends on it.**
  That is item 1 and it is the whole reason it is first.
* **Do not build the reservation.** Two packets refused it with measurements and
  a third attempt is not wanted: the reservation is meaningless until the wall
  it guards is the binding one. **This packet makes that possible; it does not
  do it.**
* **`zhao_geom_binner_v2.sv`, `zhao_shell_top_v2.sv` and `zhao_console_core.sv`
  are SHARED HOT FILES.** Stage your **hunk**, never `git add <file>`; never
  `git checkout --` on one; check `git diff --cached --name-only` before every
  commit.
* **`-MapOnly` on a block is yours. Do NOT start a console or full-device fit**,
  and **every map you quote must name its `-Device`** — the default is the
  shipping part, and rows on different devices must never be differenced.

## A stale document, so you do not inherit it

**`reports/BINNER_CAPACITY_FOR_8KM_MAPS.md` prices a triangle at 142 bits and
concludes "a bigger constant is exactly what will do".** REFPUSH measured that it
**predates the metadata bank** (report 2026-08-30; Packet-D 2026-09-14) and that
a triangle now costs **1,302 bits — 9.2× the figure every later pass has
quoted**, stale in the flattering direction. Its *reference* counts reproduce
exactly at today's oracle; its *bit* costs do not. **Use it for workload
references only, and fix or mark the document in your pass.**

## Evidence bar

* **The wrap demonstrated and then made impossible**: a parameterisation that
  wraps `CNT_W` before your fix and cannot after it.
* **The capacity measured, not asserted** — your own `-MapOnly` row, device
  named, `rtlCleanAtHead` true, against `@refpush-shipped` as the like-for-like.
* **A giant that survives**: the reference count for the near-camera giant
  reaching the arena without truncation, with the overflow counter **reading
  zero because nothing overflowed** — and a separate positive control proving
  that counter still fires when it should.
* **The instruments READ in the composed console**, not merely reconnected.
* **Prove every counter you quote.** A guard unreachable with legal stimulus
  needs a **committed mutant** under `tests/mutants/`, renamed so no source list
  elaborates it, polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit code, not
  a pipeline's** — `| tail` reports `tail`'s status (REFPUSH misread the
  register this way today), and a PowerShell *exception* leaves `$LASTEXITCODE`
  carrying the previous command's value.
* **NEW GATE 31: `tools/quartus/check_console_closure_lint.py`.** Run it after
  any port change — 32 s, and it refuses IMPLICIT/MODMISSING/PINMISSING on the
  real fit closure. It exists because a dead clock shipped this morning.
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`.
* **The fit ledger reserialises.** `reports/synthesis/zhao_block_fit.json` was
  fully re-sorted today; expect a noisy conflict and **verify no row is lost**
  (count before and after, and name the rows you added).
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE.**
2. **`CNT_W` derived**, with the wrap shown before and prevented after.
3. **The capacity**, what you set it to, and the measured cost with the device
   named. **If it is not affordable, say so with the number and stop.**
4. **The instruments reconnected AND read**, with the assertion that reads them.
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.** REFPUSH nearly shipped a
   confident engineering impossibility by scaling the army's parameter to answer
   the giant's question, and caught it. That is the standard.
8. Branch and commit hash. **Push `gz/giantrefs` only.** Never `--force`.
