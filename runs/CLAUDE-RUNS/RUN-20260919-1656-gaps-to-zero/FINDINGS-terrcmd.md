# FINDINGS — TERRCMD (entries I27 / I32, the terrain command producer)

**2026-09-21. Branch `gz/terrcmd` at `3d6fe4ba`, merged. Register 22 → 22.
Diff 100% comment.**

Transcribed by the coordinator; the harness refused the lane's own write (the
eleventh lane to hit it).

> **Sent to find one wall, it found the interesting thing hiding behind it: a
> LIVE DEFECT in work merged the night before, against a contract that had
> rejected the approach by name.**

---

## 1. What `cmd_*` actually needs — and it was never ONE absence

**Not a missing ZIDL record, and not a missing executor arm.** The brief named
both and **both already exist**: `SurfaceStamp 0x0210` is `implemented` and
carries the patch handle32, the transform translation, radius, strength, tag and
operation; `zhao_cmd_exec`'s `EX_STAMP` state already drives **nine** `stamp_*`
ports off a CRC-validated packet.

> ***"`cmd_*` has no producer" has been forwarded by THREE LANES as one
> absence. It is THIRTEEN FIELDS, and NINE are already live on the core's
> wires.***

Live already: `cx`/`cz` from `cmd_exec_stamp_tx_w`/`_ty_w`, `radius`, `src_id`,
the four `env_*` **port-for-port** from `u_surface_dispatch` (R45), `dual` from
`tps_v_flags[TERR_FLAG_DUAL_BIT]`, `patch_id` from the dispatch key.

**Four are absent — `depth_from`, `depth_to`, `cells`, `depth_sheet` — and each
is a DECISION, not a wire.** *That is exactly why no amount of composition work
ever reached it.*

**And the coordinator's C4 boundary was satisfiable all along:** `job_handle_i`
need **not** be synthesised from `cmd_patch_id_i`, because **`stamp_patch_o` IS
the ABI's `handle32[patch]`**, already composed as `cmd_exec_stamp_patch_w`. The
value is present and validated. **C4 is satisfied by CARRYING it, not deriving
it.**

## 2. THE DEFECT — two ratified contracts disagree, and the built path took the rejected branch

**`SURFACE.STAMP.md` S3:** `stamp_results` carries
`{texel, tag, strength_after, strength_before}`; `TERRAIN.BAKE` *"needs the
DELTA, not just the new value"*; sending `before` *"saves BAKE a second read
port onto the sheet"*. It carries a **committed mutant (mutation 8)** and
**`surf_res_before_o` — the port entry I32 is named after — exists for this
alone.**

**The same clause rejects the alternative BY NAME:**

> *"**Rejected:** emitting only the new value and letting BAKE re-read — a
> second reader on a store whose whole rate budget is one texel per clock."*

**What was built instead:** `zref::terrain::stamp_depth_at_vertex` returns an
**ABSOLUTE** depth from one plane; `zhao_terrain_sheetseam` **IS** that rejected
second reader; and bake does `scar += delta16`.

> **Scar ACCUMULATES, depth is ABSOLUTE.** A stamp re-issued at the same place
> **digs the full depth again**; two stamps overlapping in one frame both dig
> the accumulated sheet. *Under the delta law the same re-issue correctly digs
> NOTHING, because op 0 replaces and `before == after`.*

**AND NO COUNTER IN THE SEAM CAN SEE IT.** `sheet_vertices_dug_o`,
`fallbacks_o`, `prefetch_beats_o` **all describe a healthy read of a sheet that
is telling the truth** — *the fault is in **which question is asked**, and every
instrument measures the answer.*

**Recommendation: the delta** — the only idempotent option, what
`surf_res_before_o` was mutation-tested to deliver, and it restores §9.2's
deferral identity (**D-TERRCMD-C**: that identity is written in `from`/`to`
*"and in nothing else"*, so sheet-mode records have no state-exactness argument
under `BAKE_PATCH_BUDGET`'s carry-over FIFO). **Cost if taken: one more
1,089-byte M10K half in the seam — recorded BEFORE the decision.**

*(Coordinator: ruled **R231**. Not an owner decision — S3 is ratified and
decided it already, including rejecting the built branch by name. Commissioned
as packet DELTALAW.)*

## 3. Re-measurement — two rots found, one sought and honestly NOT found

**TWO ROTS IN I32, BOTH LIVE.** The entry calls `TERRAIN.PAGEIO` **"NOT BUILT"**
with **"NO `design/blocks.yml` row"** — *in two places*. It is **63,729 bytes at
`blocks.yml:2377`**, and it consumes `sc_*` and serves layer D, which I32 says
have no consumer and no reader. **Three of I32's four listed holds are SPENT**,
and **two lanes had already inherited "PAGEIO does not exist" from those
sentences.**

**ONE ROT SOUGHT AND NOT FOUND — R229 working as intended.** I27's `terr_chk_*`
waits on `zhao_terrain_devstore` composing. The file exists and **greps eight
times in the core** — *"reads exactly like a composition."* **All eight are
comments.** The blocker stands.

> **"I expected to file an expiry and did not."**

**I27 AND I32 DO NOT NEED THE SAME THING.** `terr_dm_*` shares I32's root;
`terr_chk_*` waits on devstore (entry I21's subsystem), and **a `cmd_*` producer
would close I27 NEVER.** *The coordinator's "two entries behind one wall"
premise was false.*

## 4. Refused, and why nothing was built

Building the producer needs answers to **A, B and C**; picking any inside a
packet **invents a law** (PACKET-PROTOCOL rule 4). **No register row added** —
the producer has a contract and no silicon, which is POSEPAGE's *wrong half of
R214*. **No opcode taken; `spec/commands.zidl` untouched**, so nothing collided
with POSECMD's 0x0305 or W04's 0x0304.

## 5. Gates — scaled to the change (R227), and the zero was JUSTIFIED not assumed

Diff is **100% comment** (`grep '^+' | grep -v '^+//'` returns nothing). All
twelve always-gates RC 0 (`completion_register` RC 1, normal); tie-off audit
**0 SILENT**; `-LintOnly` RC 0 with **0 `%Fatal` and 0 `%Error` grepped from the
log**; `mutant_copy_drift` re-run **after** the commit per R121. **Phantom-gap
trap checked explicitly** — no added line matches `^//\s*I\d+\.`.

**On the `%Fatal` nuance, it justified its own zero rather than asserting it:**

> *"`-LintOnly` is not an inverted control — it elaborates and never runs the
> console, so it has no verdict arm to die in. Zero `%Fatal` is the correct
> expectation there, not an assumed one."*

**And it gave three independent confirmations the flag had BOUND**, none of them
an exit code: the verdict line is `-LintOnly`-specific; the plain run's markers
(`raster pixels=2560`, `frames_admitted=1`) are **absent**; and RC was 0 rather
than the **2** the repaired script now returns for an unknown argument.

## 6. THE BRIEF ERROR IT CAUGHT — the smoke list is short by two

`run_console_core_smoke.ps1`'s `param()` declares **ten** switches;
`-SkipVerilate` is a modifier, so the forms are **nine plus plain = TEN**:

```
plain  -Mutant  -UntexMutant  -NoTableLoad  -BadDescriptor  -BadVertex
-NoEchoArm  -BadTraceArm  -GlowTag  -LintOnly
```

**Every coordinator brief said EIGHT**, omitting **`-NoTableLoad`** and
**`-BadDescriptor`** — *and `-BadDescriptor` is the inverted control whose
`%Fatal` IS the evidence.*

> **A lane changing RTL behaviour and dutifully running "all eight" leaves two
> controls unrun WHILE REPORTING A COMPLETE SWEEP.**

**Same shape as the flag that used to be accepted silently: the LIST and the
SCRIPT disagreed, and only the script is authoritative.** The instruction is now
a **derivation**, not an enumeration.
