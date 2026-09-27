# FINDINGS - REDFIX, 2026-09-27

Branch `gz/redfix`, worktree `C:\programmieren\zencrifice\gz-redfix`, base
`72159626`. Three inherited reds with named fixes -- **and the named fix for the
first one was wrong.**

---

## 1. THE REGISTER, MEASURED BARE, BEFORE AND AFTER

```
python tools/budget/completion_register.py
  MANDATORY GAPS REMAINING : 2   (2 tie-offs + 0 disconnected + 0 unbuilt
                                  + 0 uncited + 0 unresolvable)
  I34  boundary        TERRAIN.PATCH's FIELD-HEIGHT LANE
  I55  unclassified    GEOM.PARAMBUF's WALK REQUEST and DECODED OUTPUT
```

**2 at base, 2 at my last commit.** Unmoved, which was the requirement. This
packet closed no gap and was never meant to.

---

## 2. RED 1 - BIT 410. THE PRESCRIBED REPAIR WOULD HAVE LEFT THE SIX TESTS RED.

`ceba0bfe` (NORMALMAP, 2026-09-26) grew `zhao_texture_v3_request_v2_t` by one bit
at its top for `detail_required` and moved the TEXREQ, EZPAY, EARLYZ_KEY and
CONTINUATION constants with it. It did **not** move the PRETEX field constants,
which describe the same layout seen from the whole 491-bit record. So
`PRETEX_EARLYZ_PAYLOAD_HI` said 409 while `PRETEX_EARLYZ_KEY_LO` said 411, and
**bit 410 belonged to no field.**

### The handover's "exact replacements, derived from the layout, not guessed" are INCOMPLETE

`FINDINGS-doorcost.md` 7.1 prescribes moving four fields plus `PAYLOAD_HI`. Those
five are right and they are not the repair. **The continuation tail sits inside
the payload, above the request, so it moved with the request too**, and
`detail_required` had no PRETEX constant at all. Applying 7.1 exactly as written
leaves `PRETEX_VERTEX_RGB_HI = 409` and `PRETEX_SOURCE_ID_LO = 411` -- **the same
orphaned bit 410, relocated by one field.**

It is not cosmetic. `check_candidate()` also reads the tail at **386, 378, 370
and 362**. With 7.1's five changes applied the six tests get past address, depth,
state and source and then fail on four more fields. The real repair is
**seventeen constants changed and two added**:

| field | was | is |
|---|---|---|
| `PRETEX_DETAIL_REQUIRED_LO/HI` | *absent* | 362 / 362 |
| `PRETEX_STENCIL_REFERENCE_LO/HI` | 362 / 369 | 363 / 370 |
| `PRETEX_EFFECT_TAG_LO/HI` | 370 / 377 | 371 / 378 |
| `PRETEX_VERTEX_ALPHA_LO/HI` | 378 / 385 | 379 / 386 |
| `PRETEX_VERTEX_RGB_LO/HI` | 386 / 409 | 387 / 410 |
| `PRETEX_SOURCE_ID_LO/HI` | 410 / 425 | 411 / 426 |
| `PRETEX_FRAGMENT_STATE_LO/HI` | 426 / 457 | 427 / 458 |
| `PRETEX_INVW24_LO/HI` | 458 / 481 | 459 / 482 |
| `PRETEX_IN_TILE_ADDR_LO/HI` | 482 / 489 | 483 / 490 |
| `PRETEX_EARLYZ_PAYLOAD_HI` | 409 | 410 |
| `PRETEX_TEXTURE_REQUEST_HI` | 361 | 362 |
| `EZPAY_TEXTURE_REQUEST_HI` | 361 | 362 |

Note the direction: the handover's number made the repair look **smaller** than
it is -- the flattering direction, the one nobody audits.

### The six tests are GREEN

```
geom_bin_pipe_v2_directed ................ Passed  0.32 s
geom_bin_pipe_v2_coordinate_mutant ....... Passed  0.13 s
geom_bin_pipe_v2_omit_v3_quiet_mutant .... Passed  0.15 s
geom_bin_pipe_v2_identity_cancel_control . Passed  0.12 s
geom_bin_pipe_v2_old_ready_mutant ........ Passed  0.13 s
geom_bin_pipe_v2_skip_cancel_mutant ...... Passed  0.26 s
```

**Six of FIFTEEN, not six of thirteen** -- see section 6.

---

## 3. A SEVENTH INHERITED RED NOBODY HAD RECORDED, IN A REGISTERED CTEST

**`render_texture_packet_a` fails at BASE, at VERILATION**, measured in my own
worktree at `72159626` with a clean `git status --porcelain` **before I edited
anything**:

```
%Warning-WIDTHEXPAND: zhao_render_texture_layout_top.sv:256: Operator NEQCASE
    expects 363 bits on the RHS, but RHS's REPLICATE generates 362 bits.
%Warning-WIDTHEXPAND: zhao_render_texture_layout_top.sv:269: Operator NEQCASE
    expects 411 bits on the LHS, but LHS's SEL generates 410 bits.
%Warning-UNUSEDPARAM: TEXREQ_DETAIL_REQUIRED_LO / _HI not used
%Error: Exiting due to 4 warning(s)
```

`ceba0bfe` left **three** files inconsistent: the package's PRETEX set, the
fixture's independently-written layout vector (which never learned
`detail_required` exists), and two constants it declared and pinned nowhere.
All repaired; the test now passes **5 of 5**.

### And the findings' instrument claim is too strong

`FINDINGS-doorcost` 7.1: *"the package's own self-check cannot catch it."* True of
`PRETEX_OFFSET_CONTRACT_OK`, **false of the package.** `expect_span` sets each
struct field BY NAME and compares the lit bits against the constants' span. It
*was* firing -- `render-texture layout span earlyz_payload.texture_request
expected [361:0]` -- for a week, in a registered ctest. **A working instrument was
shouting and the finding recorded that none existed.**

---

## 4. THE NEW SELF-CHECK, AND IT IS SEEN TO FIRE

`PRETEX_OFFSET_CONTRACT_OK` compares every constant **to its own literal**. It
asserts `410 == 410`: CLAUDE.md's *detector wired to two operands that move
together*, with the two operands being **one operand written twice**. Kept (the
literal pins are deliberate and catch a single unreviewed edit), and joined by:

* **`PRETEX_FIELDS_TILE_OK`** -- every bit of the record belongs to exactly one
  field: no hole, no overlap, nothing past the end, reaching `RASTER_PRETEX_W`
  exactly. One function over a 23-entry table, used at elaboration AND at
  runtime, so there is one implementation and no copy to go stale.
* **`PRETEX_CROSS_RECORD_OK`** -- the layout seen from `TEXREQ_*`, `EZPAY_*` and
  the Early-Z key lifted to `PRETEX_EARLYZ_KEY_LO` agrees with the whole record,
  and every group span brackets exactly the fields it claims at the declared
  width. Those sets are maintained by hand and independently, which is what makes
  them two operands capable of moving apart.
* **`tools/rtl/check_pretex_offsets.py`** -- the desync's victim was not
  SystemVerilog at all. It was a C++ test holding **fourteen bare literals**,
  which no SV check can reach. Registered as `pretex_offset_agreement` and
  `pretex_offset_agreement_control`.

### And PRETEX was the ONLY record that was broken

The same tiling law checked against every other record the package declares
offsets for -- EARLYZ key, continuation tail, continuation, AUX, TEXREQ, RESULT
and RETIRE -- reports **0 breaches**. That is not luck. **PRETEX is the only
record assembled from two independently-maintained sub-records**, so it is the
only one whose constants mirror another set that can move without it. The defect
class is precisely "a hand-maintained mirror of a packed struct": no production
RTL slices this record with literals (checked: zero literal slices in the 360-490
range outside the package), which is why NORMALMAP's RTL side was correct
throughout and only the constants, the C++ literals and the checker pins rotted.

### Shown firing FIVE ways, the last being the one that matters

1. **46 refusals in the plain run, every run.** The guard displaces each of the
   23 fields by -1 and +1 and requires refusal all 46 times. It asserts the
   CORRECT behaviour -- a displaced field is not a tiling -- so it survives the
   repair instead of expiring with the bug.
2. **Anti-vacuity in the same run.** 46 refusals is also what a function
   returning constant zero gives, so the declared layout must be ACCEPTED by the
   same function. The guard prints `field_spans=46 tiling_refusals=46`.
3. **Elaboration CONTROL 18 is not a boolean override.** It runs the PRODUCTION
   tiling function against a layout with `source_id` displaced by one bit -- the
   fault `ceba0bfe` created -- and the `$fatal` fires with its own label.
   Overriding the boolean would have been the same vacuity one level up.
4. **`check_pretex_offsets.py --selftest`: 9 of 9 mutations detected**, including
   **a field dropped from the tool's own table** (a tool blind to its own blind
   spot fails that one).
5. **IT FIRES ON THE REAL HISTORICAL FAULT.** The shipped `check()` function, run
   unmodified against the BASE COMMIT's own constants, reports **18 breaches**
   and names every one of the nine moved fields -- including the four that
   `FINDINGS-doorcost` 7.1's prescribed repair would have left behind:

```
   FIELD_ORDER names ['DETAIL_REQUIRED'] which the package does not declare
   the PRETEX fields end at bit 489 but the record is 491 bits wide,
       so bit(s) 490..490 belong to no field
   PRETEX_STENCIL_REFERENCE_LO = 362 but EZPAY_STENCIL_REFERENCE_LO = 363
   PRETEX_EFFECT_TAG_LO = 370 but EZPAY_EFFECT_TAG_LO = 371
   PRETEX_VERTEX_ALPHA_LO = 378 but EZPAY_VERTEX_ALPHA_LO = 379
   PRETEX_VERTEX_RGB_HI = 409 but EZPAY_VERTEX_RGB_HI = 410
   PRETEX_SOURCE_ID_LO = 410 but PRETEX_EARLYZ_KEY_LO + EARLYZ_SOURCE_ID_LO = 411
   ... 12 more
```

   This is not a synthetic mutation. **It would have failed `ceba0bfe` the day it
   landed.**

---

## 4a. RED 2 - `-UntexMutant` IS GREEN, AND THE CLAUSE IS STILL A REAL PROPERTY

The fatal clause read the GLOBAL four-client counter:

```systemverilog
if (geom_clip_submitted_o != 0 || geom_setup_triangles_submitted_o != 0)
  $fatal(1, "MUTANT FAILED: %0d triangle(s) entered GEOM.CLIP ...");
```

`cl_in_untex_c` is a declaration about ONE door client; `geom_clip_submitted_o`
counts arrivals from ALL FOUR. TERRAIN.CLIPFEED became the fourth at `1b6f8895`
(CARRIAGE, 2026-09-26), **five days after the assertion was written** at
`addc0be7` (2026-09-21), when GEOM.REPLAY was the door's only producer.

**NOT relaxed to a bound.** The property is re-expressed against the arm it was
always about -- every arrival at the door must be one TERRAIN.CLIPFEED emitted,
which says the replay arm contributed none. That is an **equality between two
independently measured counters**, so a single leaked triangle makes it 129
against 128 and it fires. And it is preceded by a demand that the terrain arm be
alive and delivering its reference count, because `128 == 128` is also satisfied
by `0 == 0` and a dead terrain arm would have made this the fifteenth vacuous
control of the week.

**MEASURED, RC 0:**

```
SMOKE: MUTANT zhao_console_core_untex_decl_mutant -- geom_untex_refused_o=16
  (want 16) clip_submitted=128 terr_cf_emitted=128 (want 128)
  setup_submitted=61 (want 61) raster_pixels=512 matwin[unpub/underflow]=[0 0]
SMOKE: MUTANT PASS -- ... nothing from the REPLAY arm entered GEOM.CLIP
  (128 arrivals, all 128 of them TERRAIN.CLIPFEED's)
```

### The scoped counter still catches the fault the global one existed for

Shown with numbers from the PLAIN smoke, which is a gate that runs on every
merge, so no new mutant was needed. The plain run is exactly the un-refused case
-- the fault the clause exists to detect:

```
SMOKE: clip submitted=144 clipped=69 culled=0 setup_submitted=75
       (reference mesh: 16 / 2 / 0 / 14, terrain: 128 / 67 / 0 / 61)
SMOKE: terrcf triangles=128 emitted=128
```

`geom_clip_submitted_o = 144` against `terr_cf_emitted_o = 128`. The scoped
equality **fires**, and the difference it reports is `144 - 128 = 16` -- exactly
`SGF_EXP_REPLAYED`, the mesh count. The SETUP clause fires too: 75 against
terrain's reference 61, surplus 14, exactly the mesh's accepted count. So the
scoped form detects a leak of the whole mesh, and by equality a leak of one.

No new port and no GEOM edit: `terr_cf_emitted_o` is already a core port, and the
plain run already differences it against the door's arrivals for the same reason.

And the plain form itself: **PASS, `raster pixels=2816`, `frames_admitted=1`.**

---

## 5. RED 3 - THE BOARD LINT, CLASSIFIED RATHER THAN RE-BASELINED

Measured at base `72159626` AND at my tree, diffed on warning **IDENTITIES**
(class + file + message, line numbers stripped), because equal counts can be
different sets:

```
parsed warning rows: base=290  mine=290
NEW warnings in my tree: 0
GONE from my tree:       0
```

| count | class | directory |
|---|---|---|
| **209** | UNUSEDPARAM | `fpga/rtl/field` |
| 62 | PINCONNECTEMPTY | `fpga/rtl/prod` |
| 13 | UNUSEDSIGNAL | `fpga/rtl/prod` |
| 2 | SIMILARNAME | `fpga/rtl/prod` |
| 2 | WIDTHEXPAND | `fpga/rtl/prod` |
| 1 | DECLFILENAME | `fpga/rtl/common` |
| 1 | UNUSEDSIGNAL | `fpga/rtl/field` |

**All 290 inherited. Zero mine.** RC 1, `%Error: Exiting due to 290 warning(s)`.

### The protocol's baseline row is wrong in three ways

1. **The count is 290**, not the protocol's 262 and not the brief's 291.
2. **The classes are not what it says.** The row names *"DECLFILENAME and
   PINCONNECTEMPTY"*. DECLFILENAME is **1 of 290**. The dominant class is
   **UNUSEDPARAM at 209 -- 72% of the red -- and the row does not mention it.**
3. **209 of 290 are ONE FILE**:
   `fpga/rtl/field/generated/zhao_field_host_image_pkg.sv`, a *generated*
   package. That makes the red a single tractable repair for whoever owns FIELD's
   generator rather than a diffuse 290 -- and nobody could have known it from the
   row, because the row misnames the class.

### The brief's "known asymmetry" is FALSE in both halves

The brief warns `check_console_closure_lint.py` *"deduplicates its source list"*
while the `.ps1` does not. **Neither deduplicates.** The Python uses
`srcs: list[str]` with `srcs.append(...)` (`:104-121`); the PowerShell uses
`$srcRel += $Matches[1]` (`:69`). And it is moot: `zhao_console_core`'s list has
**296 sources, 296 unique, zero duplicates.**

The real asymmetry is larger: **the two tools measure different things.** The
Python lints top `zhao_console_core` with `-Wno-fatal`, **no `-Wall` and no
`.vlt`**; the PowerShell lints top `zhao_console_board` with **`-Wall` and the
waiver file**. That is why one says OK while the other reports 290.

---

## 6. EVERY CLAIM I FOUND FALSE

1. **"`fpga/rtl/texture/`'s Packet-D package"** (brief, twice). It is
   `fpga/rtl/common/zhao_render_texture_pkg.sv`. Nothing named PRETEX lives under
   `fpga/rtl/texture/`.
2. **"the thirteen required Packet-D tests"** (brief and `FINDINGS-doorcost`
   7.1). The list holds **15** and the CMake guard checks `EQUAL 15`. Only the
   `FATAL_ERROR` **message** said 13 -- and that message is the sentence a reader
   reaches for, so it propagated into two documents. It is **six of fifteen**.
   Corrected in `tests/CMakeLists.txt` and in the checker's pinned copy.
3. **"Exact replacements are in FINDINGS-doorcost 7.1 ... derived from the
   layout, not guessed."** Five of nineteen edits, leaving bit 410 orphaned one
   field away and four more fields misread. Section 2.
4. **"the package's own self-check cannot catch it."** True of one check, false
   of the package. Section 3.
5. **The protocol's 262 baseline and its class attribution.** Section 5.
6. **The dedup asymmetry.** Neither tool dedupes; there are no duplicates.
7. **`PAYLOAD_W(410)`** pinned in `test_render_texture_packet_d.py` while
   `zhao_raster_tile_pipe_v2.sv:869` reads `PAYLOAD_W(411)` -- **sixth site** of
   the same desync, red at base. And behind it **`zhao_skid2 #(.W(490))`** while
   `:956` reads `.W(491)` -- **seventh site**. The RTL is right in both; the
   CHECKER was stale, which is the dangerous direction: anyone trusting the pin
   over the source would have edited working RTL down to the old width.
   `test_d3_rtl_shape_and_selector_controls` is GREEN as a result.

One claim that was true and is now discharged rather than false:
`FINDINGS-doorcost` 7.2's stale `gen_console_board`. It reads **FRESH at 1616
core ports** here; the regeneration is already in the base commit.

---

## 7. WHAT I GOT WRONG AND CAUGHT MYSELF, AND WHAT I REFUSED

### Got wrong, caught by measuring

**I re-enabled the door test's candidate/fragment probes and had to put them
back.** DOORCOST disabled them because *"asserting the corrected ones would rest
this door's evidence on an unreviewed repair"*. That reason **is** discharged, so
I turned them on. `geom_bin_pipe_v2_door` then failed:

```
packet-d directed FAIL cycle=1370: Packet-C admitted a candidate absent
from the zref plane scoreboard
```

The door arm **never pushes `expected_candidates` for the jobs it issues** -- it
drives the DUT directly and leans on the framebuffer oracle -- so
`check_candidate` ran against an empty deque. Enabling them needs a candidate
oracle for the door path, which is door/I55 work.

The error is mine and it is instructive: **I treated "the stated reason is
discharged" as "the way is clear."** CLAUDE.md's *a refusal is only as good as its
scope*, run backwards -- the quoted blocker was gone and a second, unwritten one
was not. Reverted **with the measured failure recorded in the file**, because the
next reader will reach exactly the conclusion I did and the second blocker is
written nowhere else.

**I also put the named offset block inside a class**, where a `namespace` is
illegal, and killed eight build targets. Moved to file scope.

### Refused

* **Widening `cd_o_owner` and `geom_clipdoor_granted_o`** -- section 8,
  SWAPCLOSE's fence.
* **The 209 UNUSEDPARAM** in FIELD's generated image package: a generator change
  in another lane. Classifying it is what was asked; named so it is somebody's job.
* **`test_current_shell_is_fail_closed_not_a_connected_owner_claim`.** Red at
  base. It asserts `assertNotIn("zhao_shell_top_v2", tops)` and
  `excluded[...] == "not-yet-adopted"` while `design/prod_manifest.yml:55` lists
  `zhao_shell_top_v2` **as a top**, under **owner ruling R86** of 2026-09-20 and
  *"YOU ONLY GET TO FIT THE LATEST VERSION"*. I touched no file that test reads,
  which settles attribution without a re-run. **This is a BUILD, not a DECISION**
  -- R86 decided it seven days ago and the test was never updated -- so it must
  not be inherited as blocked. It belongs to whoever owns `prod_manifest`.
* **`test_packet_b_source_registrations_are_exact_ordered_and_unique`** and
  **`test_probe_is_excluded_and_production_selection_stays_unchanged`**, both red
  at base with the identical message *"non-source token inside exact Packet-B
  CMake list: `'  '`"* -- a two-space line inside
  `set(ZHAO_TEXTURE_V3_PACKET_B_SOURCES`. The parser reads only that region, so
  not mine. Packet-B's to fix.
* **The rest of `packet_d_registration_static`.** Red at base for four
  independent reasons; I fixed the two in my defect class. The others -- a
  `KeyError: 'zhao_raster_attrdiv_v2'` in the prod-manifest exclusions, *"source
  rows are not the exact ordered inventory"*, the *"exactly 59 SV paths"* marker,
  and six protected-oracle SHA pins -- are inherited and outside a layout packet.
  **Updating the SHA pins would be baselining a red**, which this packet is
  forbidden to do.

### A trap worth the next packet's time

**A base-comparison worktree under `%TEMP%` breaks this repo's environment
resolution.** My throwaway at `C:\Users\Fabs\AppData\Local\Temp\gz-redfix-base`
made an ownership census report **`{}`**, with the real cause buried in the
message: *"Verilator loader environment invalid: configured winlibs bin directory
is missing: `C:\Users\Fabs\AppData\Local\dsstuff\mingw64\bin`"* -- a path resolved
relative to the worktree's own location. An empty census is the broken-instrument
tell, and it nearly made me read a base-vs-mine difference as a regression of
mine. **Two existing base worktrees live under `%TEMP%`** (`gz-baseline`,
`gz-giantrefs-base`). Put them beside the repo.

---

## 8. FOR `SWAPCLOSE` - A LIVE TRUNCATION ON THE CLIPDOOR'S FOURTH CLIENT. NOT TOUCHED.

Two of the board lint's 290 are WIDTHEXPAND on `zhao_console_core.sv`, and they
are not noise:

```
:19497: Output port connection 'o_owner_o' expects 4 bits on the pin connection,
        but pin connection's VARREF 'cd_o_owner' generates 3 bits.
:19499: Output port connection 'granted_o' expects 128 bits on the pin
        connection, but pin connection's VARREF 'geom_clipdoor_granted_o'
        generates 96 bits.
```

`zhao_geom_clipdoor` declares `o_owner_o [NCLIENT-1:0]` and
`granted_o [NCLIENT*32-1:0]`. `NCLIENT` went **3 -> 4** when TERRAIN.CLIPFEED was
composed as the fourth client (`1b6f8895`, CARRIAGE, 2026-09-26). The wires did
not follow: `zhao_console_core.sv:19242` still declares `wire [2:0] cd_o_owner`
and `:13694` still declares `output logic [95:0] geom_clipdoor_granted_o`.
**The fourth client's ownership bit and its entire 32-bit grant count are
truncated away.**

Both are observability, not the pixel datapath -- so it is not a wrong picture.
It is worse in one specific sense: **any counter-based check on terrain's use of
the door reads a number that cannot include terrain.** That is the
broken-instrument class exactly, and it sits directly beside the RED 2 assertion
this packet just rescoped.

**And it was written down in advance.** `zhao_console_core.sv:3286`:

> *"It is `NCLIENT` 3 -> 4 plus one flattened slice per port, **with
> `cd_o_owner` and `geom_clipdoor_granted_o` widening with it**"*

The comment names both wires. The widening never happened. An uncashed cheque,
authored, in the file, with a lint warning pointing straight at it -- **inside an
accepted red nobody had read.** That is the entire argument for classifying a
baseline instead of accepting it.

Not touched: `zhao_geom_clipdoor` and the console's geometry plumbing are
SWAPCLOSE's fence.

---

## 9. BRANCH AND COMMITS

Branch **`gz/redfix`**, pushed. Never `--force`.

| gate | result |
|---|---|
| `completion_register.py` | **2** (I34, I55) -- unmoved |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `check_quartus17_syntax.py` | RC 0 |
| `check_case_labels.py` | RC 0 |
| `gen_prod_top.py --check` | fresh, 88 instances |
| `gen_console_board.py --check` | fresh, 1616 core ports |
| `gen_shell_paired_diff.py --check` | fresh, harness and mutant |
| `mutant_copy_drift.py` | OK, **run after the commit** (R121) |
| `pretex_offset_agreement` | Passed |
| `pretex_offset_agreement_control` | Passed, 9/9 |
| `render_texture_packet_a` layout tests | 5/5 OK |
| the six Packet-D tests | Passed |
| `test_d3_rtl_shape_and_selector_controls` | **was red at base, now green** |
| console board lint | RC 1, 290 -- inherited, 0 new, identity-diffed |
