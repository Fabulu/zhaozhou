# Task Log: RUN-20260903-1958 - [Describe objective here]

**Created:** 2026-09-03 19:58 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-1958-island-recovery-and-known-defects/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 19:58 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-1958
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-09-03 20:0x — the known-wrong list, worked in the owner's order

### A1 — a fit can no longer pass on Fmax while violating its storage law
`3bc25633`. The island brief's tripwires existed only as prose; nothing checked
them, which is why a cache with 2 M10K where its predecessor had 4 reported
`ok` at 98.66 MHz. `design/fit_targets.yml` gains `rules:`, the runner enforces
them, and `tools/quartus/check_fit_rules.ps1` audits every recorded row with no
Quartus at all. Against today's numbers it fails 4 of 4, including the one that
passed this morning.

Three self-inflicted faults on the way, all worth remembering: a regex that
silently extracted nothing and printed PASS for the block that violates all
three of its rules; a markdown backtick inside a double-quoted PowerShell
string (backtick is the escape character); and an em-dash in a script PowerShell
5.1 reads as ANSI -- where the python that stripped it opened with 'w', so the
encode error truncated the file to zero bytes and the retry cleanly processed
nothing.

### C1 — the cache storage, moved to a clock-only process
`97d3b637` (with the tile pipe). The file's own header claimed the registered
READ was "the entire reason this file was rewritten". It is half the reason.
The WRITES at `:412`/`:416` sat in the async-reset process, and an M10K has no
reset port. `data_r` and `tag_r` now live in the clock-only process that
already did the reads; `valid_r` stays where it is because it needs the reset a
memory cannot give.

Behaviour identical -- both assignments were already non-blocking, so a read and
a write to one address on one edge still returns the old contents, which is the
read-during-write mode an M10K provides. 10 checks pass, sustained throughput
unchanged at 1.02 clocks per access.

**FIT RUNNING** to answer the only question that matters here: does it infer?
Brief S25: if M10K does not infer, stop.

### B — Save the Renderer, commit one
`97d3b637`. TilePipe's 16-way priority encoder ran combinationally on
`pend_mask_r` and its output became `frag_addr` in the same period, entering
Early-Z's 256-bit presence lookup: ~2.6 ns of `column encode -> address ->
256:1 selection`. The cursor is now registered, using a staging opportunity
already in the protocol -- a coverage row is accepted only while the mask is
empty and a fragment only while it is not, so the accept is always one clock
ahead of the first fragment. No bubble, no delayed Early-Z decision.
74 + 12 + 16 checks pass including the pixel-CRC path.

`e814b772`. `run_shell_fit.ps1 -GpuPeriodNs` so the 105 MHz experiment can run
at 9.52381 ns instead of deriving an Fmax from a 10 ns placement. It throws
rather than fitting if the SDC substitution matched nothing.

### Next, in order
1. read the cache fit: **M10K >= 8 or stop**
2. the 105 MHz untouched-netlist experiment, five seeds
3. C2 TEXJOIN -- needs the C6-C8 banking design, not just a write move: its
   entry table is read COMBINATIONALLY through `head_q`, so no write change
   alone can make it memory
4. the production resource count, last

## IN PROGRESS at the moment the cache fit was due back

**`zhao_texture_fragrob` is written and lints clean; its TEST IS NOT WRITTEN.**

Per the brief S6.1 it is a NEW block beside v2, not an edit of it, and its port
list is deliberately identical so **v2 is the behavioural oracle**. Structure
per S6.3/S6.4: control state (valid, generation, required/arrived masks, aux
flags) in flops on purpose; payload in banks keyed by SAMPLE INDEX
(`desc_u_m[3]`, `desc_v_m[3]`, `desc_met_m[3]`, `res_rgb_m[3]`, `res_a_m[3]`,
plus context and aux), every one written AND read only inside a clock-only
process through registered addresses. Free-slot FIFO with a 16-cycle init
sweep, no scan. Issue and retire are two-stage state machines because a banked
descriptor cannot be picked combinationally -- that is the trade: latency for
structure.

Registered in `design/fit_targets.yml` with the S3.4 tripwires
(`min_m10k: 6`, `max_registers: 2500`, `max_dsp: 0`). NOT yet in
`design/blocks.yml` and it has no contract file.

### NEXT STEPS, in order
1. **the differential test** -- `tests/texture/fragrob_differential.cpp`:
   drive fragrob and `zhao_raster_texjoin_v2` from identical stimulus with a
   shared TMU/AUX responder, and compare the SEQUENCE of retired fragments.
   NOT cycle-by-cycle: fragrob has extra pipeline stages by design, so the
   contract being checked is same fragments, same order, same values.
   Directed cases to add: generation rejection of a stale return, ordered
   retire under out-of-order completion, allocation blocking when full.
2. `design/contracts/TEXTURE.FRAGROB.md` + a `blocks.yml` entry.
3. fit it against its tripwires; the acceptance question is **M10K, not Fmax**.
4. only then consider retiring v2 from the production manifest.

## 2026-09-03 21:2x — state while the cache fit finishes (75 min in, ~85 expected)

### Done since the last entry, none of it touching the fit's closure

| | |
|---|---|
| `5f4fec80` | FRAGROB differential vs v2: 280 retirements, 0 divergence, **3 real RTL bugs found** |
| `62a892d8` | `TEXTURE.FRAGROB.md` + ledger entry |
| `eca81e6f` | the terrain light law in ZRef, 3 oracle faults fixed, 12 checks |
| `05ce5529` | `MEM.UPLOAD` acceptance law pinned, 11 checks, -> REFERENCE_COMPLETE |
| `ebf1921c` | **the Stop hook**, because the CLAUDE.md rule was violated twice |
| `bb8a9f35` | `TERRAIN.SHADE.md` + ledger entry |
| `ca2fc653` | oracle split into shade/normalmap, matching the block split |
| `224f4736` | `TERRAIN.NORMALMAP` registered -- it had a contract and NO ledger entry |

Ledger 99 blocks, green.

### The three FRAGROB bugs, because they are the useful part

The block was lint-clean and COMMITTED before the differential existed, and it
was broken three ways. A clean lint proved nothing.

1. **allocation order is not slot order** -- slots come from a free list, so
   after the first recycle the hand-out order has nothing to do with the index,
   and the retire path used `head_q` directly as the slot.
2. **the lost-update fault, for the fifth time in this repository** --
   `free_cnt_q` and `live_cnt_q` each written from two `if` blocks in one
   `always_ff`, so a coincident accept and retire gave +/-1 instead of net zero.
3. **a single AUX pending register dropped the second request**, so the
   fragment that needed it could never complete. A deadlock, not a lost pixel.

### NEXT, in order

1. **read the cache fit.** The acceptance question is **M10K >= 8, not Fmax**.
   `tools/quartus/check_fit_rules.ps1` answers it in a second. Brief S25: if
   M10K does not infer, STOP.
2. **fit FRAGROB** against its tripwires (`min_m10k: 6`, `max_registers: 2500`,
   `max_dsp: 0`). Its RTL and test are done; only the measurement is missing.
3. **the 105 MHz experiment** -- `run_shell_fit.ps1 -GpuPeriodNs 9.52381`,
   five seeds, judged on zero WNS and zero TNS. The tooling is committed and
   the SDC substitution is verified; it needs the toolchain free.
4. **the look-gate**, which is the art law and blocks TERRAIN.SHADE's RTL: the
   amended oracle into the ZRef renderer, island under a MOVING sun at 240p,
   owner looks. A still frame will not do -- the detail has no Y component, so
   at the zenith the relief fades out by construction.
5. then TERRAIN.SHADE at **II=1 and II=3**, two rows, per bro's Pareto
   challenge -- the producer delivers ~1 triangle per 3 clocks and the block is
   specified at 1 per clock.

### Still true and still blocking
`REJECT: adding terrain/Field RTL faster than the texture fit can be closed.`
That is why no new terrain RTL has been written this pass despite the contracts
now existing for two terrain blocks.

## 21:40 — INTERIM: the storage inferred at synthesis

The fit is still routing (placement succeeded at 7:58; routing has been running
~40 min), but **synthesis has already answered the acceptance question**, and
that is the number the brief gates on:

    Info (21064): Implemented 32 RAM segments

Against the same block before the fix, whose completed fit reported
`ramBlocks: 2` and `blockMemoryBits: 128` — 9,728 bits of array in flip-flops.

So the clock-only process did what `zhao_texture_cache.sv:495-523` said it
would, and **the static checker's prediction holds so far**:
`check_ram_inference.py` said `data_r` and `tag_r` were structurally clean
after the fix, and synthesis agrees.

**Not yet proven, and not to be claimed:** the final `ramBlocks` count after
fitting, which is what `min_m10k: 8` actually tests. Synthesis says the arrays
became memory; only the fitter says how many M10Ks that packs into. Also
unresolved: 15,961 logic cells at synthesis is still large, so the ALM tripwire
(`max_alms: 1500`) may well still fail even with the storage fixed. That would
be a DIFFERENT problem from the one just fixed and should not be confused with
it.

Reading the finished fit with `tools/quartus/check_fit_rules.ps1` is the next
action; it applies all three tripwires at once.

## 22:2x — the per-lane re-fit, synthesis evidence (fit still placing)

| | previous fit (clock-only only) | this fit (per-lane flat arrays) |
|---|---|---|
| logic cells at synthesis | 15,961 | **4,438** |
| RAM segments | 32 | **96** |
| `cannot regroup multidimensional array` | **data_r AND tag_r** | **absent** |
| fitter preparation | 4:34 | **2:14** |

The "cannot regroup" message is **gone**, which was the actual blocker named in
the last fit's synthesis log. Logic cells fell 3.6x, which is what an array
leaving flip-flops looks like.

**NOT a claim of success.** Last time synthesis also looked good -- 32 RAM
segments -- and the fitter still returned 2 M10K and 128 memory bits. Synthesis
saying an array became memory is not the fitter saying it stayed memory. The
acceptance question is still `min_m10k: 8` on the finished row, answered by
`tools/quartus/check_fit_rules.ps1`.

The one remaining uninferred array is `rq_en`, "uninferred due to inappropriate
RAM size" -- a small enable vector that is too small to be a memory, which is
correct and expected.

### Done while this fit ran: audit items 1-7 complete
`e1bb06ff`. Five contracts (GEOM.ASSEMBLE, MATERIAL.RESOLVE, GEOM.LIGHT,
GEOM.DEPTHQUANT, FORGE.SHADOW), two amendments (MEM.UPLOAD's coherence law;
TERRAIN.SHADE's corrected citation), and eight owner decisions written to
`reports/FUNDAMENTALS-DECISIONS-NEEDED.md` and `reports/decisions.html`.
Ledger green at 104 blocks.

### Next while it finishes
`TEXTURE.COMBINE` -- the pair of MATERIAL.RESOLVE. Its own contract says
shipping the resolver alone makes the machine fetch samples nothing combines,
so the two are one piece of work in two contracts.

---

## 2026-09-04, while the relaunched cache fit runs

The fit relaunched at 23:52 from a **snapshot** copied into the workspace, so a
live edit can no longer reach a running fit. `provenance guard: 1 declared
source(s)` confirmed the scoping is right. At 02:00 it had 66 minutes of CPU,
1.1 GB resident, and had not touched disk since 23:55 — normal for the
placement phase, and the verilator builds below were competing for cores.

### GEOM.DEPTHQUANT built and UNIT_VERIFIED — `39f68c04`, `5a984ed0`
The block from audit R6. Checked against `zref::depth_of_raw`, the ratified law
the golden captures already pin, and **not** against a second implementation of
it — the mistake made and caught yesterday on the flat-shade law, where twelve
checks passed because a duplicate was compared with itself.

Three faults it found, all of which lint had passed:

* the normalisation loop **descended**, so the lowest set bit above 23 won
  instead of the highest. Depth at `wmax` came out exactly 2^5 too large;
* a shift of **zero was refused** as out of range — and zero *is* the near pin.
  The generator solves each profile's `SCALE` so the shift lands on exactly zero
  at `wmin` and the depth IS the raw reciprocal, which is how `0xFFFFFF` is hit
  exactly. Refusing it returned depth 0 — the value meaning "far" — for the
  closest geometry in the scene;
* the reciprocal handshake never consulted `rcp_ready_i`.

7/7 checks, 0 refused. Ledger green at 105 blocks.

### GEOM.PROJECT exposes clip.w — `475d0578`
DEPTHQUANT had no producer without it. Nearly free: the restoring divider
carries the divisor through every step, so `dstep_d[DIV_STEPS]` was already
aligned with the quotients — one register at s5, one at the output.

Verified against a **known** value rather than a re-derivation: the divider-rail
case already builds a matrix whose `clip.w` is exactly `wraw`. Mutating the RTL
to `s5_w + 1` fails exactly the four new checks. 756/756 restored.

### The resource count the owner asked for — `19db1e2d`
`check_ram_inference.py --rank`, filtered through `prod_manifest.yml`.

**280,784 bits of declared array in the production top will not infer as
memory.** At Cyclone V's best case of 4 reg/ALM that is 70,196 ALM; the device
has 41,910. **1.7x over, on the generous bound.**

The ranking was **wrong twice before it was right**, and both failures are the
house error. Unranked, 898 findings was true and unusable. Ranked, it printed a
confident "12 arrays worth looking at" while all five of the largest sat in a
331-entry UNKNOWN pile — a heredoc had eaten `\b` into a literal `0x08` so the
parameter matcher matched nothing. **The tell was 331 UNKNOWN against 12
ranked**, a ratio a working sizer does not produce.

### 280,784 -> 63,696 bits, a 77% reduction — `70f05f31`, `9304d668`
All three mechanical items, each baselined before the edit and each proved to
bite by a mutation:

| | before | after |
|---|---|---|
| `zhao_terrain_residency_v2` | 168,876 | 940 |
| `zhao_raster_tilestore` | 32,768 | 0 |
| `zhao_texture_palette_res` | 16,384 | 0 |

Behaviour byte-identical: 37 + 6 residency checks, 31 + 6 tilestore, 18
palette, all with the same printed counters as the baseline.

Two things learned that are worth more than the bits:

* **The read counts as much as the write.** `ram0_q <= ram0[addr]` puts an
  array inside an async-reset process just as surely as a write does. Both of
  these blocks had their reads there, and `palette_res` had a comment claiming
  the array was deliberately unreset — true, and the half that does not bite.
* **One defect was producing two findings.** The report called residency_v2's
  three write addresses a design question. They are mutually exclusive *per
  way*; the `[WAYS][SETS]` shape was hiding the per-way view. Fixing the shape
  dissolved the third finding entirely.

`hazard_c` stopped being belt-and-braces in the process: in flip-flops a
non-blocking read is always the old value, but in an M10K mixed-port
read-during-write is device behaviour, so that guard is now what makes the
conversion sound. Recorded in the RTL.

### Next
1. Read the cache fit. Acceptance is `min_m10k: 8`, **not** Fmax.
2. Fit `zhao_terrain_residency_v2` — the only thing that closes tonight's
   claim. Expect roughly 22 M10K; a fit reporting 0 has failed however good its
   Fmax.
3. `zhao_forge_cliff` (12,288) is now the largest remaining single-reason item.

---

## 2026-09-04 01:40 — the 23:52 fit died, and what it left behind

Both background watchers were stopped and `quartus_fit` went with them, 2h30m
in, with **no fitter summary written**. The parent script had been killed hours
earlier, so nothing was left to parse a result even if it had finished.

### Synthesis DID complete, and it is worth reading
`quartus_map` finished at 23:52:18 and its report survives:

* **"cannot regroup" is GONE.** That message was the actual blocker named in
  the previous fit's synthesis log, and the per-lane generate removed it.
* **All four lanes' `data_r` became `altsyncram`** — `g_lane[0..3].data_r`,
  Simple Dual Port, 128 x 16, 2,048 bits each.
* One uninferred array, `rq_en`, "inappropriate RAM size" — a small enable
  vector, correct and expected.
* Total block memory bits **8,320**, against **128** on the last fit.

### But `tag_r` did not infer, and min_m10k: 8 cannot be met
`tag_r` is `[TAG_W-1:0] tag_r [LINES]` = **16 deep x 24 wide = 384 bits per
lane**. That is far below the size Quartus will put in an M10K — it is the same
"inappropriate RAM size" verdict `rq_en` gets, and 384 bits in flip-flops is
the *correct* answer for it.

So the block has **four** arrays worth being memory, not eight. `min_m10k: 8`
from island brief S3.4 requires `tag_r` to be block RAM, which it structurally
cannot be. **The tripwire will fail a correct design.**

**Not changing it.** Lowering a gate so the thing passes is the failure this
whole gate exists to prevent, and S3.4 is the owner's number. Recorded here for
a decision. The defensible re-derivation by the same capacity-floor method used
for tonight's other three blocks is **min_m10k: 4** — four `data_r` banks of
2,048 bits, each of which needs its own block — with the note that 8 was
presumably written expecting the tag array to be memory too.

### My own snapshot change had broken every block fit
Relaunching failed at `quartus_map` in 1.9 seconds:

    Error (125048): Error reading Quartus Prime Settings File ... line 243

The snapshot puts sources under `[IO.Path]::GetTempPath()`, which here sits
below a profile whose name contains a **space**, and the QSF line was unquoted.
The live-tree paths it replaced were all under `C:/programmieren/`, so the flow
had never had to care.

**Why it went unnoticed is the lesson.** The snapshot was committed at 23:50:35
and the fit it was committed *for* started at 23:52 — but that run's synthesis
messages name `C:/programmieren/` paths, so it **compiled the live tree and
never exercised the new code at all**. A change made to protect a running fit
was "validated" by watching that fit run, and the fit was not using it.
Committing a change and observing the process it was meant to affect is not
observing the change.

Fixed by quoting; `quartus_map` now passes and the fitter is running again,
launched **detached** so a task-stop cannot take it with it.

### Still open
1. The relaunched cache fit. Acceptance `min_m10k`, and see the tripwire note
   above before reading a failure as an RTL failure.
2. `zhao_terrain_residency_v2`, `zhao_raster_tilestore`, `zhao_texture_palette_res`
   all now have capacity-floor tripwires and none has been fit.
3. ~~The local CMake tree is broken~~ — **wrong, corrected 02:20.** The tree is
   fine. `CMAKE_CACHEFILE_DIR` is `/c/programmieren/...`, and every failure came
   from invoking `cmake --build` after a `Set-Location C:\programmieren\...`
   that cmake resolved as `C:/Programmieren/...` — a CASING mismatch in my own
   invocation, not a broken tree. Built from the lowercase path it regenerates
   and compiles normally. **No reconfigure needed.**

   Recording the correction rather than quietly deleting the claim: "the build
   tree is broken, needs a full reconfigure" is exactly the kind of confident
   wrong diagnosis that sends the next pass at the wrong thing, and I had
   already written it down and worked around it all night with a scratchpad
   compile script.


## 02:10 — docket sweep while the fit runs

Nothing here touches `fpga/rtl/texture/zhao_texture_cache_pipe.sv`, the running
fit's entire declared closure.

**D17(e), new and it explains D17(c).** `tests/lint/cppcheck_check.cmake.in`
printed STATUS and returned when cppcheck was absent, and CTest read that as a
SKIP — so local was green and CI red. That is the standing memory *"local gates
must match CI"* exactly. Absence now fails loudly with the install line and an
explicit `ZHAO_ALLOW_MISSING_CPPCHECK=1` opt-out that still prints. All three
paths exercised against generated template instances.

There is **no npm package to pin cppcheck with** the way clang-format is
pinned, so the version is CHECKED instead — and the drift is live: **CI pins
2.19.0, this machine has 2.20.0, and D17(c)'s finding does not reproduce on
2.20.0 at all.** Same command, same file, different answer.

**D17(c) was already fixed** in `d93bf0b3`; the docket entry was stale.

**D17(d) is not ours.** `zhao-reel --check` fails on any sequence-CRC drift and
`tools/reel/` is mid-flight on a retime skeleton — Stage 0 `22d61057` through
Stage 4 `ada4c105`. The CRCs drift by design and re-baselining belongs to
whoever is authoring the motion. The entry said "reproducing"; there is nothing
to reproduce.

**A possible free win on D1, logged as unmeasured.** The RMW critical path ends
at `RASTER.TILESTORE`'s RAM write, and that block's read registers left the
reset list tonight so its banks could infer as M10K. A reset read register
cannot be the M10K's own output register, so a fabric flop at the end of that
path may now be inside the block. Done for AREA; the timing effect is **not
measured** and is in the docket so the next composed fit is read with it in
mind, not as a claim.


## 02:40 — the build tree, diagnosed properly on the third try

I got this wrong twice before getting it right, and both wrong answers were
written down, so both are corrected here rather than deleted.

**Wrong answer 1: "the tree is broken, needs a full reconfigure."** Based on a
`CMAKE_CACHEFILE_DIR` casing mismatch in the error text.

**Wrong answer 2: "the tree is fine, it was my invocation casing."** Based on a
build that appeared to succeed — `BUILD_RC=0`. That zero came from `tail`, not
from cmake. **The same masking trap as the stale-binary note in CLAUDE.md**, in
a new costume: a pipeline's exit status is the last command's, and I read it as
the build's.

**The actual fault.** `build.ninja` was stale AND self-inconsistent. The rule
for `Vtb_perspuv_pair` declares `Vtb_perspuv_pair.cmake` among its outputs, but
the command it runs passes **`--make json`**, which produces the `.json` that is
in fact there. So a `copy_if_different` of a file nothing ever writes failed on
every build — and because that rule is part of `build.ninja`'s own regeneration,
**ninja could not rebuild the graph that would have fixed it.** A deadlock:
255 of 256 verilate directories have their `.cmake`; the one that does not is
the one blocking regeneration.

**The fix is to regenerate through cmake rather than through ninja** — and to do
it the documented way, which I had not been. `CMakePresets.json` gates
`windows-base` on `${hostSystemName} equals Windows`, so invoking `cmake` from
Git Bash picks up the MSYS cmake, reports a non-Windows host, and disables the
preset — *"Could not use disabled preset"*. The preset's own `ZHAO_NOTE` says
to source `tools/env/zhao-env.ps1` first and warns about exactly this
("the broken devkitPro msys2 cmake"). From PowerShell with the env sourced:

    Configuring done (19.8s) / Generating done (7.8s) / Build files written

**All of tonight's scratchpad compiling was a workaround for a documented setup
step I skipped.** The direct-compile script was not wasted — it is what let the
three RAM fixes be baselined and mutation-tested while the graph was jammed —
but the tree never needed a reconfigure for the reason I first gave, and it was
never "fine" for the reason I gave second.

## 03:05 — tonight's work re-verified through the REAL gate

With `build.ninja` regenerated, everything was rebuilt and run through CTest
rather than the scratchpad harness:

    render_pipe_directed .............   Passed
    geom_project_directed ............   Passed
    raster_tilestore_directed ........   Passed
    terrain_residency_v2_directed ....   Passed
    texture_palette_res_directed .....   Passed
    geom_depthquant_directed .........   Passed
    100% tests passed, 0 failed out of 6

`geom_depthquant_directed` is test #482 — it only became visible to CTest once
the configure actually re-ran, which is worth noting: for most of the night it
existed, passed, and **was not in the suite**.

### Two registration gaps found and one closed mechanically

**`zhao_prod_top` was stale, and not only from last night.** Regenerating it
picked up `out_w_o` on `zhao_geom_project` — a PINMISSING that would have failed
the next production fit — *and* `acq_sub_i`/`acq_gen_i` on
`zhao_geom_pose_cache`, from the generation-tagged coherence work EARLIER in the
session. So the top was already stale before last night touched it. A generated
file that nobody regenerates is a stale file with a reassuring provenance line
at the top.

**A new block has to be registered in more than one place**, and only the first
was mechanical. `check_prod_manifest` caught `zhao_geom_assemble`,
`zhao_geom_depthquant` and `zhao_texture_fragrob` as UNACCOUNTED; after adding
them and regenerating, the production fit would **still** have died at
elaboration, because two of them were absent from `zhao_prod_top`'s source list
in `fit_targets.yml`. Two gates one step apart, and the second one was me
noticing by hand.

`check_prod_manifest` now verifies every module the generated top instantiates
resolves to a listed source. Mutation tested: deleting one source line fails
with a message naming the file; restoring it passes.

**FRAGROB is deliberately NOT counted** in the manifest. It was built beside
`zhao_raster_texjoin_v2` with v2 kept as the behavioural oracle, and v2 stays
the counted implementation until FRAGROB is fit against its own tripwires.
Counting both would break the one-implementation rule the file exists to
enforce; counting FRAGROB instead would claim an adoption with no fit behind it.

## 03:20 — the fit timed out, and the runner warned about this in its own header

`zhao_texture_cache_pipe` ended `timeout` at **3543.8 s** against the runner's
3000 s default. `run_block_fit.ps1`'s own parameter comment says exactly what
that means:

> *A "timeout" row in the report reads as "this block does not fit", when all
> it meant was "we did not wait". Ten rows in the committed report carry that
> status and every one of them is suspect for the same reason.*

**This also explains the 2h30m run.** The timeout is enforced by the parent
script polling — and that parent had been killed hours earlier, so nothing was
enforcing anything and quartus_fit ran unbounded until the task stop took it.
The long run was not evidence that the block needs 2.5 hours; it was evidence
that nobody was holding the clock.

Relaunched detached with `-TimeoutSeconds 10800`. Whatever it returns will be a
real number rather than a budget.

**`reports/synthesis/zhao_block_fit.json` is deliberately left uncommitted**
while this runs: it currently records `failed:quartus_map.exe` from my own QSF
quoting bug, and before that `contaminated:source-changed-during-fit` from the
destroyed fit. The running fit overwrites it, so committing either intermediate
would put a self-inflicted status into the record as though it were a
measurement.
