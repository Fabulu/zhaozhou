# FINDINGS — PVSCHEMA

Branch `gz/pvschema`. Base `e9c384d3` on `claude/ceiling-architecture-20260912`.
Worktree `C:\programmieren\zencrifice\gz-pvschema`. 2026-09-27.

Decision record: `reports/DECISION-20260927-PROJECTEDVERTEX-V2.md`.

---

## 1. THE REGISTER, MEASURED BARE, AND `I55` DID NOT CLOSE

`python tools/budget/completion_register.py`, bare (RC 1 is normal):

| | value |
|---|---|
| at base `e9c384d3` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

**`I55` did not close and this packet never claimed it would.** The brief's own
framing is the right one: *"the schema is the blocker; everything else is
downstream of it."* What this packet removed is the **blocker**, not the entry —
after v2 the six Packet-D planes are reconstructible from the arena record at
full precision, which they were not before, **by any back end**.

No gap was closed by removing, narrowing, stubbing or tying off anything. One
tie-off-shaped thing was *added* and it is declared in §7.

---

## 2. `ProjectedVertex` v2 — THE LAYOUT, AND IT IS DECLARED ONCE

Declared once in `fpga/rtl/common/zhao_pkg.sv` as `ZHAO_PV_*_LO` / `ZHAO_PV_*_W`,
with `ZHAO_PARAMBUF_PV_SCHEMA = 2` and `ZHAO_PV_END_BIT`.

| offset | width | field | signed | why that width |
|---|---|---|---|---|
| 0 | 21 | `screen_x` | s | the domain `fits_s21` already refuses outside of |
| 21 | 21 | `screen_y` | s | ditto |
| 42 | 24 | `invw24` | u | R7, unchanged — plane input, exact |
| 66 | 8 | `status` | u | R7's ratified byte; `[7:4]` stay reserved-zero |
| 74 | 32 | `u_over_w` | s | **plane input — stored EXACTLY** |
| 106 | 32 | `v_over_w` | s | plane input — exact |
| 138 | 32 | `gouraud_r` | s | plane input (R234 D1) — exact |
| 170 | 32 | `gouraud_g` | s | plane input — exact |
| 202 | 32 | `gouraud_b` | s | plane input — exact |
| 234 | **22** | `alpha` | s | **decided, §3** |
| | **256** | | | **32 bytes exactly** |

**Before v2 the layout was declared three times and nowhere authoritatively:**
`zhao_pkg` carried only the byte count, `zhao_geom_paramarena.sv:804` packed it
by hand and `zhao_geom_parambuf.sv:199` unpacked it by hand — with the encoder's
own comment promising *"the two must agree bit for bit."* They now derive from
the same constants, and an elaboration guard asserts
`ZHAO_PV_END_BIT == PV_B * 8`.

**It costs no address space.** `PV_STRIDE_B` was already 32 while the record was
24, so the eight bytes were allocated and skipped. `VERT_CAP_B` does not move,
the view's 3,407,872-of-4,194,304-byte footprint is unchanged to the byte, and
no region in `spec/memory_rules.md` §5c changes. 32 bytes is a **ceiling**: a
48-byte stride takes the footprint to 4,456,448 against a `VIEW_SPAN` of
4,194,304 and does not fit.

**The one real cost, declared rather than absorbed:** the vertex write goes from
three 8-byte beats to four, so SDRAM **write traffic on the vertex arm rises
33%** — 1,704 extra bytes per frame at the smoke fixture's 213 vertices, 512 KiB
per frame at R7's 65,536-vertex giant. Unavoidable: the bytes are the function.

### The round-trip test that fails against v1

`tests/geometry/geom_parambuf_directed.cpp` §2 — **24 checks, 0 failures**. It
stores two colours that v1's quantiser maps to **the same byte** and requires
them back **distinct**:

* `lo = 0x1180` (4,480) and `hi = 0x127F` (4,735). The bucket is **computed, not
  guessed**: `(v + 128) >> 8 == 18` holds for 4,480 ≤ v ≤ 4,735, so these are its
  two ends — 255 counts apart in fx16 and **one value** in a v1 record.
* The test asserts that premise **first**, so the control cannot quietly stop
  being a discriminating case. (It caught my own first attempt: see §8.)
* `{255,255,255}` is not used anywhere. Saturated white survives any quantiser.

`geom_vertid_directed` (99 checks) carries the producer half: the attribute slots
are published **unchanged**, including `0xFF80` — the Review C2 rail value that
v1 destroyed by construction.

---

## 3. ALPHA IS **s22**, DECIDED, WITH THE REASON IN THE RECORD

Decided by measuring **what reads the field and what writes it**, not by taking
what was left over. The reason sits beside the constant in `zhao_pkg`.

**Nothing downstream interpolates alpha, and the RTL says so by name.**
`zhao_geom_attrpack.sv:141-146` waives its own unread slot: *"the only slot this
block does not ask for is `alpha` (slot 6), whose value is governed by ruling
R48's named `ALPHA_C` constant and whose PER-PRIMITIVE producer is
`tri_continuation_tail_i`'s `vertex_alpha` (ruling R89)."* The six planes are
invw, u/w, v/w, r, g, b. `zhao_console_core.sv:10762`: *"`ALPHA_C` IS NOT ON THE
BLEND'S PATH AT ALL."*

**Alpha's entire declared domain is 17 bits**, measured at all four
attribute-packet producers rather than assumed:

| producer | slots 3/4/5 | slot 6 | max |
|---|---|---|---|
| `zhao_geom_vattr:534-537` | `{15'd0, lit[16:0]}` | `ALPHA_C = 32'h0001_0000` | `0x1_0000` |
| `zhao_terrain_clipfeed:760-783` | `lit_of_mod` → `{16'd0, s[15:0]}` | `TERR_ALPHA = 65536` | `0x1_0000` |
| `zhao_part_clipfeed:535-541` | `lit_of_byte` → `{16'd0, c, 8'd0}` | `PART_ALPHA = 65536` | `0x1_0000` |
| `zhao_forge_assemble:681-684` | `art_r/g/b_i` (32-bit ports) | `art_alpha_i` | `0x1_0000` |

`zhao_forge_assemble`'s four ports are fed from named composer constants;
`FORGE_LIT_B = 32'sd65536` is the largest colour value anywhere in the tree.

**So: every quantity the six planes are built from is stored at its FULL 32-bit
slot width and carries no domain claim that could be wrong, and alpha takes the
remainder — s22, its domain plus a sign bit plus five bits of overbright
headroom.**

**The tempting inverse was REFUSED.** Narrowing r/g/b to their measured 17-bit
domain and giving alpha 32 would make the record's one job — *the planes come out
bit-identical* — conditional on a claim about every present and future producer.
`art_r_i` is a 32-bit port driven by a **named, editable owner constant**, and
silently capping it is CLAUDE.md rule 6's *"removing the owner's control in the
name of fidelity"*. **No claim at all beats a claim with a large margin.**

**A per-record version nibble was considered and rejected.** The only place it
could go is `status[7:4]`, which `GEOM.VERTID.md` rules *"reserved, written 0.
Nonzero is a malformed record"* — spending it would repeal a live legality rule
to encode something no reader needs. The schema is versioned at elaboration
instead, and v1 and v2 records never coexist in one arena.

---

## 4. THE THREE ARENA MUTANTS — AND ONE HAD SILENTLY BECOME A NO-OP

**This is the most important finding in the packet.**

### 4.1 They were NOT stale when I started — the brief's premise was false

The brief says to refresh them because *"they are copies, and a copy of a record
that no longer exists is a positive control for a block that does not exist."*
**At base `e9c384d3` all three were in sync**, committed with production in the
same commit `41c7eb12`, and `tools/budget/mutant_copy_drift.py` returned **RC 0,
"every committed mutant copy is at least as new as the module it copies"**, 78
copies checked. The refresh was needed **because of my change**, not as inherited
debt — CLAUDE.md's *"before calling a red inherited, read the two commit dates"*,
applied in the other direction.

### 4.2 The refresh is a COMMAND, not a three-way merge

The brief says *"three-way merge them; do not transplant."* There is a better
instrument and the mutants' own headers name it:
**`tools/rtl/gen_paramarena_mutants.py`** — *"GENERATED … so a refresh is a
command rather than an act of transcription."* One invocation regenerated all
three from current production, each carrying its own mutation and nothing else.

### 4.3 AND THEN THE ALIGN MUTANT WAS A NO-OP, WITH EVERY INSTRUMENT GREEN

The align mutation is two localparams:

    localparam int unsigned PV_SLOT_B      = PV_B;   // was PV_STRIDE_B
    localparam int unsigned LAYOUT_ALIGN_B = 1;      // was BURST_ALIGN_B

Its mechanism, in its own header: *"TRI_OFF_B is then 65,535 × 24 = 1,572,840,
which is 8 mod 16, so every TriangleDescriptor request starts at column 4 of an
aligned eight-column block."* **That worked because `PV_B` was 24.**

**Schema v2 makes `PV_B` 32, which IS a multiple of `BURST_ALIGN_B` = 16.**

| | `PV_SLOT_B` | `TRI_OFF_B` | mod 16 | vertex-1 offset | mod 16 |
|---|---|---|---|---|---|
| v1 | 24 | 1,572,840 | **8** | 24 | **8** |
| v2 | 32 | 2,097,120 | **0** | 32 | **0** |

So the mutation installs a **perfectly aligned layout** and `burst_unaligned_o`
**cannot move**. The mutation survived the refresh intact and **the fault it
exists to create did not.**

**Note what every instrument would have said.** The copy's provenance is perfect
— regenerated from production in the same commit — so `mutant_copy_drift.py` is
green and *correct* to be. The one substantive line is present and unchanged.
Only the arithmetic moved, one file away, and **nothing was watching the
arithmetic.** This is *"a copy goes stale in the flattering direction"* arriving
from a direction nobody named: **not a stale copy, but a FRESH copy whose
mutation has quietly become a no-op.**

**Repaired two ways**, both in the generator so the refresh stays a command:

1. the value is written **literally** (`PV_SLOT_B = 24`) rather than derived from
   a name whose meaning changed — deriving it from `PV_B` is exactly how it
   broke;
2. **the mutant now asserts its own faultiness** —
   `if ((PV_SLOT_B % BURST_ALIGN_B) == 0) $fatal(... "this positive control is a
   NO-OP")`, inside `initial begin` for Quartus 17.

**AND THAT GUARD WAS FIRED, not asserted.** The generator was temporarily put
back to `PV_SLOT_B = PV_B`, the mutant regenerated and rebuilt, and the run
aborted with

    %Fatal: zhao_geom_paramarena_align_mutant.sv:729: Assertion failed in
    TOP.tb_zhao_geom_paramarena.u_arena: align_mutant: PV_SLOT_B is
    burst-aligned, so this positive control is a NO-OP

which proves both halves at once: the guard works, and the old mutation really
was inert under v2. The generator was then restored and the tree **verified by
content** — `git status --porcelain` empty against the pushed commit — rather
than by trusting a copy, which is the `Copy-Item`-timestamp trap's lesson.

**All three mutants fire and all three negative controls stay silent:**
`geom_paramarena_{drainmut,alignmut,reservemut}_{fires,silent}` — 6/6.

### 4.4 A TOOLCHAIN FACT THE FIRE TEST TAUGHT, worth the line

**A Verilated model that hits `$fatal` in an `initial` block does NOT exit on
this toolchain — it prints, says `Aborting...`, and STAYS ALIVE.** Three copies
of `test_geom_paramarena_alignmut_fires.exe` were left running by the fire test
above, and they held a lock on their own `.exe`, so the next build failed with

    ld.exe: cannot open output file tests	est_geom_paramarena_alignmut_fires.exe:
    Permission denied

Read cold, that is a mystifying build error with no connection to the thing that
caused it, and it looks like the documented killed-`g++` mystery. It is not: it
is a **live process holding its own output file**, and the cure is to kill the
wedged run. Per ruling R81 the three were classified by **full command line**
first — all three were
`C:\programmieren\zencrifice\gz-pvschemauild	ests\...`, my own worktree and
my own binary — and only then killed by PID. No other lane's process was
touched.

**So: after deliberately firing an elaboration guard, check for the survivor
before rebuilding.**

---

## 5. THE SDRAM SHARE — NO SLOT EXISTS, AND THIS PACKET NEEDS NONE

Confronted before designing the feed, as the brief required, and **verified
independently rather than inherited**:

* **`zhao_geom_mem_adapter` has TEN requesters `a..j` and all ten are driven** at
  `zhao_console_core` (one `.<letter>_req_i` connection each, counted). Its own
  header line 1 still says *"NINE logical ENGINE1 readers"* and is stale by one —
  SWAPBUILD found this and it is confirmed.
* **A REFINEMENT ON SWAPBUILD'S NUMBER, in the unflattering direction.** It is
  described as `N = 10`. **There is no `N`.** The block is not parameterised on
  its requester count at all — the letters are hardcoded ports. Widening it is
  therefore **not a parameter bump**: it is a new port pair plus arbitration plus
  the round-robin re-proof. The cost is higher than "N → N+1" suggests.
* **`u_geom_wshare` is `zhao_mem_share_wr #(.N(3), .CLIENT_ID(3))`** at
  `zhao_console_core.sv:30228` — three, all driven.

**Neither share has a free slot.** This packet needs none: v2 rides the arena's
existing ENGINE1 write socket and changes only the burst length of a request
already being issued — three beats to four.

### `spec/memory_rules.md` does contradict itself about 22 MiB, and the GUARD is the tiebreaker

Read with that in mind rather than resolved by picking the convenient half:

* **§5c, line ~711** lists `0x06A0_0000 .. 0x07FF_FFFF` as *"reserved / unmapped,
  pending evidence"*;
* **line ~902**, same file, lists the identical range as `RENDER.ASSET_POOL`,
  22 MiB, `ENGINE1`, **read-only**, under owner ruling R32.

**The second is the true one, and the deciding evidence is RTL rather than
prose:** `zhao_pkg.sv:329-330` defines `ZHAO_RENDER_ASSET_BASE = 32'h06A0_0000`
and `ZHAO_RENDER_ASSET_SPAN = 32'h0160_0000`, and `zhao_mem_guard`'s
`render_asset_ok` admits ENGINE1 reads over exactly `[0x06A0_0000, 0x0800_0000)`.
**A region with a live guard window is not unmapped.** The first reading is what
an architect hunting room to widen a record would find first, and it looks like
22 MiB of free space. It is not — and v2 needed none of it, because the slack it
spent was already inside the stride.

---

## 6. CLAIMS IN THE BRIEF OR THE RECORDS FOUND FALSE

### 6.1 The brief's own v2 layout would have SILENTLY TRUNCATED THE RECORD

The brief proposes *"242 bits = 30.25 bytes … fitting the allocated stride with
14 bits spare."*

`zhao_geom_paramarena.sv:1195` sets the record's burst length as

    m_beats_q <= 4'(PV_B / 8);

**an integer division with no elaboration guard**, in a block whose elaboration
section refuses **six** other alignment breaches by name. At `PV_B` = 30 or 31
that yields **3 beats = 24 bytes**: the record is allocated 32, declared 30 in
`m_len_q`, and **24 are written**. The trailing fields decode as whatever SDRAM
held, **with no diagnostic anywhere** — a short write completes and every counter
balances.

Flattering direction, again. v2 is 32 bytes so it does not trip it; **the guard
was added anyway** (plus the same for `TD_B`, `CK_B`, and
`ZHAO_PV_END_BIT == PV_B * 8`), because the block's own words two lines up are
*"a guard that holds by luck is one that should say so out loud."*

### 6.2 The three mutants were not stale, and the refresh is a command, not a merge

§4.1 and §4.2.

### 6.3 The align mutant's mutation became a no-op under the brief's own schema

§4.3. The brief's instruction to *"refresh the three mutants"* would have been
satisfied — provenance-green, mutation intact — while one of them proved nothing.

### 6.4 `zhao_console_core.sv:10757-10775` is stale in production RTL, in two ways

Both were true when written and are now false, and each sends a reader wrong:

* it says `ALPHA_C` is *"written into per-vertex attribute **SLOT 3**"*. It is
  **slot 6**. `zhao_geom_vattr.sv:537` writes `rep_data_o[191:160]`, the sixth
  32-bit field, and every `SLOT_ALPHA` parameter in the tree reads 6. Slot 3 is
  `SLOT_R` — so the sentence points at the red Gouraud channel while discussing
  alpha.
* it says *"`zhao_geom_attrpack` emits exactly THREE planes … so slots 3..6 — lit
  r/g/b AND alpha — have no interpolator and no carriage."* **Owner ruling R234
  D1 (2026-09-21) made it SIX planes**, indexing `SLOT_R`/`SLOT_G`/`SLOT_B`, and
  widened `METAW` 1157 → 1877 to carry them.

The second is load-bearing: a reader taking it at face value concludes the
Gouraud channels have no consumer and that the v1 record was adequate — **which
is precisely the false premise this packet exists to clear.** Corrected in place
with a dated amendment rather than by rewriting the historical paragraph.

### 6.5 `zhao_geom_mem_adapter` is not parameterised on its requester count

§5.

---

## 7. WHAT I REFUSED

* **The second setup/attrpack back end, and the multiplex itself.** Not built.
  SWAPBUILD decided the architecture and priced the instance form at **+40 DSP,
  35.7% of the shipping device**; this packet's job was the blocker beneath it.
  Building the back end here would have stacked a subsystem on a schema change
  that is neither gated nor fitted yet.
* **Driving `walk_valid_i` / `t_ready_i`.** Untouched. `paramwalk` remains
  `dirs=0 chunks=0 tris=0`. Fifth consecutive refusal; nothing in v2 changes the
  reason.
* **ORing the walk into the live stream.** Forbidden and not done.
* **Exporting `pv_narrow_o` to `zhao_console_core`'s port list.** This is the
  tie-off-shaped thing I added, and it is declared rather than silent. The
  counter refuses a screen coordinate outside s21 — but the composed producer is
  `zhao_geom_vertid`, whose `cx_q`/`cy_q` are `signed [20:0]`, so **no console
  stimulus can reach the fault.** Exporting it would add a counter to the
  console's evidence surface that cannot move there: *"a gate that cannot reach
  the state is not evidence about the state."* It is exported from
  `tb_zhao_geom_paramarena` instead, where the bench drives the 32-bit port
  directly and the fault IS reachable. The unconnected pin carries a named
  comment giving this reason.
* **Any Quartus run.** No fit, no map, no `-MapOnly`. This packet makes **no
  synthesis claim whatsoever.** v2 adds no arithmetic and no DSP; it is a layout
  plus four register-width changes, and its area effect is a question for the
  coordinator's fit, not a claim here.
* **Widening either SDRAM share.** Not needed; §5.

---

## 8. WHAT I GOT WRONG AND CAUGHT MYSELF

* **My own "fails against v1" colour pair did not actually collide.** I chose
  `0x1200` / `0x12FF` by eye; they quantise to `0x12` and `0x13`. Because the
  test's **first assertion is the premise itself**, it failed loudly — *"expected
  0x12, got 0x13"* — instead of passing as a discriminating case that
  discriminated nothing. That is the entire reason to assert a control's premise.
  Replaced with `0x1180` / `0x127F`, the two ends of the computed bucket.
* **My s21 round-trip stimulus asked for a truncation and then complained about
  it.** I drove `y = -x` over a list containing `-1,048,576`; negating that gives
  `+1,048,576`, which is **outside s21**, so the 21-bit field returned
  `-1,048,576` and the check failed. **s21 is asymmetric and the RTL was right.**
  Replaced with a separate rotated `ys[]` list — better anyway, since x and y now
  carry different values in every case, so a decoder reading one field's bits for
  the other fails rather than agreeing with itself.
* **I twice wrote a comment whose first word after `//` was the linter's own
  name**, turning it into a pragma (`BADVLTPRAGMA`). A documented trap, hit
  anyway.
* **I nearly created a dead wire.** The first plan kept `pv_rgba_o` on
  `zhao_geom_vertid` and simply stopped routing it at the console. `UNUSEDSIGNAL`
  is waived across whole directories by `tests/shell/v3_closure_inherited.vlt`,
  so it would have raised nothing at all. The port was removed and the colour law
  moved to the decoder instead, where it has a real consumer.

---

## 9. GATES AT THE PUSHED COMMIT

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — no higher than the 2 I started at |
| `check_console_inventory.py` | **OK** |
| `check_prod_manifest.py` | **OK** — 406 modules, 88 tops |
| `gen_prod_top.py --check` | **fresh** (regenerated; vertid and arena both changed ports) |
| `gen_console_board.py --check` | **FRESH** (1610 core ports) |
| `gen_shell_paired_diff.py --check` | **fresh**, harness and mutant |
| `check_quartus17_syntax.py` | **RC 0**, 664 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK** |
| `mutant_copy_drift.py` | **RC 0, run AFTER the commit** (ruling R121) |
| `npm run abi:check` | not implicated — `spec/commands.zidl` untouched |
| Verilator `-Wall` on the three changed blocks | **RC 0** each |

Directed tests, built and run (ruling R60), at the pushed commit:

| test | result |
|---|---|
| `geom_parambuf_directed` | **24 checks, 0 failures** (4 illegal vertices FIRED) |
| `geom_vertid_directed` | **99 checks, 0 failures** (all twelve counters fired) |
| `geom_paramarena_directed` | **351 checks, 0 failures** |
| `geom_arenabin_directed` | **317 checks, 0 failures** |
| `geom_arenabin_price` | **Passed** |
| `cmd_exec_directed` | **977 checks, 0 failures** |
| `ctest -R "paramarena\|parambuf\|vertid\|arenabin"` | **13/13, 100%** |

Console smoke and the console-board lint were **not** run: this packet changed
`zhao_console_core` port connections but no core port, and the coordinator gates
the merged result. That is stated rather than implied.

---

## 10. BRANCH AND COMMITS

**Branch `gz/pvschema`**, pushed to origin. **Never force-pushed. Not merged to
the integration branch.**

* `2de100c9` — `decide(PVSCHEMA)`: the v2 layout and alpha's width, with the
  measurements behind both.
* `1e516cd6` — `feat(PVSCHEMA)`: schema v2 implemented across
  `zhao_pkg` / `zhao_geom_parambuf` / `zhao_geom_paramarena` / `zhao_geom_vertid`
  / `zhao_geom_paramwalk` / `zhao_console_core` / `zhao_prod_top`, both contracts,
  the three regenerated mutants and their generator, and eight tests. 22 files.
