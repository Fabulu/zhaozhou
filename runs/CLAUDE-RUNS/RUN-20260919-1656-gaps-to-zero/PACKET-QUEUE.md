# Packet queue — what fills the next free slot

Ceiling is THREE concurrent packets. When one lands, the next one down starts.
This file is the coordinator's, and it is a WORK LIST: delete a line when the
gap it names is closed, never when it is merely attempted.

**Rewritten 2026-09-21, early hours.** The previous version was from 21:53 the
night before and had gone stale in every section — it claimed register 21 and
listed lanes that had finished hours earlier. **A stale queue is worse than
none, because the next free slot gets filled from fiction.** Verify the running
list against `git ls-remote` before trusting it; that is not paranoia, it is
the documented failure of this exact file.

**Register: 22** = 9 tie-offs + 13 disconnected. **Measure it yourself** —
`python tools/budget/completion_register.py`, exit 1 while gaps remain, which
is normal. This line goes stale.

---

## THE SITUATION CHANGED TWICE TODAY, AND BOTH CHANGES BIND

**1. The owner spent the budget (R234, `(owner, explicit)`).** Ship Gouraud
(+24 DSP). Reach true zero, **not** a floor — which **reversed R199** and put
FORGE back in scope. Grant the deviation store's 185 M10K.

**2. "FIT AT COMPLETION ONLY" STANDS. I misread it and ran a fit; it is killed.**

The owner's words were *"A full fit, no caveats. So we can assess the damage"*,
and I read them as *fit immediately*. **They mean the opposite of what I did.**
Corrected in his own words:

> *"We're not fitting now. We're going zero gaps. We want a full composed
> console. Fit now is useless. We are picking all the expensive options to see
> how big damage is."*

**The expensive options exist so that the EVENTUAL fit measures the whole
machine.** A fit today measures a console missing Gouraud, the deviation store,
the HUD band, the delta repair, FORGE.SHADOW and `zhao_geom_clipread` — **a
floor, which is exactly the caveated number he was ruling out.** *"No caveats"
is a property of the DESIGN being complete, not of the fit command's flags.*

**So: zero gaps first, full composition, then one honest fit.** D2 stands
unchanged. **Do not run Quartus.** The machine belongs to the packets.

---

## RUNNING NOW — verify before trusting

**Updated 2026-09-21 after DELTALAW, SHADOWSUB and GOURAUDBUILD all landed.**

| packet | target | branch |
|---|---|---|
| **BANDBUILD** | the HUD band (R233/R235); bears on **I17** | `gz/bandbuild` |
| **CLIPDOOR** | the `GEOM_CLIP_ATTRS = 7` input door — the binding wall | `gz/clipdoor` |
| **JOBISSUE** | I21 — TERRAIN's subpatch job issuer (item 0) | `gz/jobissue` |

**FORMIDX landed** (`gz/formidx`, merged): `form_idx_q` is real and correctly
keyed, **but it is the REQUESTER's half and POSEREAD's blocker is the BANK's**.
The question stays a law and **got bigger** — see **R239**. Register 22 → 22.

**Landed today:** DELTALAW (R231's depth law), SHADOWSUB (the tap arbiter; it
**withdrew its own blocker** — R237), GOURAUDBUILD (D1 built — R238).

---

## NEXT SLOTS, best first

**0. TERRAIN.GROUP_SEQ's SUBPATCH JOB ISSUER — entry I21. THE LARGEST
UNCOVERED ITEM IN THE REGISTER — IN FLIGHT AS `JOBISSUE` (`gz/jobissue`),
launched 2026-09-21.** Scoping kept below because the lane's report should be
read against it.

*Why it dominates:* the register's 22 is **9 tie-offs + 13 disconnected
modules**, and **TERRAIN holds 7 of the 13** (`pageio`, `normalmap`,
`velocity`, `bake_v2`, `lod`, `sheetseam`, + `measure_governor`) **and 4 of the
9** (I21, I27, I32, I34). No other cluster is close.

*What it is, precisely — this is a BUILD, not a wire.* `zhao_terrain_lod`
exists and its header says its output is *"EXACTLY `zhao_terrain_tess`'s job
port"*. **It is not the SEQUENCER's job port.** The sequencer needs
`job_view_mask`, `job_mat_a`, `job_mat_b` and `job_weight` as well, and
TERRAIN.LOD emits **none of the four**. Measured by the coordinator: the only
thing in the tree driving those four is a **stimulus LFSR in a generated fit
top** (`zhao_terrain_pipe_rpp3_matw18_fit_top.sv`). **There is no production
producer.** Taking LOD's twelve matching fields and inventing the other four is
the hidden-adapter failure the entry refuses by name.

*Three shortcuts are already closed, in writing — do not re-open them:*

* **VIEWMASK closed the view-mask shortcut:** `terr_job_view_mask_i` is already
  2 bits at the boundary, so **the narrowing was never the obstacle**. Driving
  `job_view_mask` from the internal `tis_view_mask` while the rest of the job
  comes from outside *"joins two things that move independently: a worse hidden
  adapter than the one the entry refused to build, wearing a settled ruling as
  cover."* **The view mask rides THE JOB. The job has no producer.**
* **R13 disposes of `job_mat_a`/`_b`/`_weight`:** *"The job port is not widened
  to carry a subpatch-uniform value that is not true."* They are the **WRONG
  CARRIER** — not three fields awaiting an owner. Their honest closure is
  **REMOVAL once a per-triangle layer-E path exists**, which is **a function
  MOVE: nothing leaves until the replacement lands.** So the issuer owes
  **thirteen** fields, not sixteen.
* **B5's "fit a circuit you already know is wrong" objection is SPENT** —
  `proj0_i`/`proj1_i` are `[PROJW-1:0]`, `PROJW = 20`, R83 implemented under
  R98. What survives is *"neither block is instantiated by any production
  root"*, **a composition behind I14, not a format defect.**

*What actually stands, re-measured 2026-09-21 by VIEWMASK:* **B1** the layer-E
reader (R13 puts it **inside TESS** — one absence with a ruled destination),
**B2** neighbour edge levels (`MEASURE.GOVERNOR.md` still refuses in writing),
**B4** `terr_cc_serve_release_i` (boundary input, no internal driver anywhere).

*Two traps in this entry specifically.* It **argues with itself** — it quotes
R13 at blocker 1 and ninety lines later still calls the same question open, and
calls those three fields' owner UNIDENTIFIED. And **an instance name quoted
from a GENERATED file has a shelf life**: B2's LFSR is `u64_src`, not
`u59_src`, because the generator renumbers. **Re-measure every blocker before
quoting it (R165); do not inherit this entry's prose.**

*And my own held decision, resolved by the above:* **do not compose
`zhao_measure_governor` alone with its TERRAIN.LOD group dangling** — that is
R75's "close one gap, open another". **Compose the TERRAIN group as a
subsystem, or not at all.** R223's hold stands for the governor *by itself*;
this packet is the thing that dissolves it.

**0.5. GEOM.WARP — AUTHORISED IN WRITING SINCE 2026-09-20 AND NOBODY HAS TAKEN
IT. Take the next free slot with this.** See **R240**.

**It is NOT a law question, and I nearly made it one.** The core's own
instantiation says *"`zhao_geom_warp.sv` does not exist in this tree"* — **it
does**, 36,227 bytes, built by FIELDW1 on 2026-09-20 with its adapter beside
it. That stale sentence **names the wrong blocker**: the real one is that
**there is no `DrawWarpedForm` command, so no draw can set `d_warp_en_i`** and a
composed warp would sit permanently in its W09 bypass — *function present and
structurally unreachable.*

**And `DrawWarpedForm` is RATIFIED.** `reports/OWNER-RATIFICATION-20260920-WARP.md`
**W04**: *"Add `DrawWarpedForm` at opcode **0x0304**, **96-byte record /
80-byte payload**. `DrawForm 0x0300` and raw vertex format 0 stay byte-for-byte
unchanged."* Ratified by Fabian's commit `4c256137`, whose message is *"implement
this warp architecture and architect implement whatever else is missing when it
comes up."* **W01–W18 are new law as of 2026-09-20.** `spec/commands.zidl`
already records that 0x0304 is spoken for, which is why POSECMD took 0x0305.

**The work, from the contract's MEASURED table (`design/contracts/GEOM.WARP.md`)
and not from the core's parenthesis:**

| # | state | what it costs |
|---|---|---|
| P1 | **CLOSED** by C1 | — |
| P2 | **ABSENT** — `.CLIENTS(2)`, one site | one-line parameter |
| P3/P4 | **PRESENT AND COMPOSED** | — |
| P5 | **DEFERRED, NOT CLOSED** — `prep_value` is one flat array, no context dimension; the per-slot stamp DETECTS a stale scalar but does not ISOLATE two eligible plans | satisfied only if Warp never interleaves with Earth in a frame — **needs a disposition** |
| P6 | **STILL PARTIAL** — `TABLES = 2`, fabric carries 4 | a disposition |
| P7 | **ABSENT** — `.INSTR_N(32)` vs 48 required | one-line parameter |
| P8 | **UNMEASURED** — D1 is merged and `zhao_field_doorbell.sv` carries `BIND_PROGRAM`; whether it satisfies §9.3's BIND/SEAL **has not been measured** | **measure it first** |
| P9 | **NOW EXISTS**, discharging R103 | unmeasured in clocks; `51 + T_run` and `19 + T_run` are **FSM arithmetic, not benchmarks** |

**Do not quote the prerequisite table without re-measuring it (R165) — four of
its nine rows were already corrected once on merge for having been measured
against a base predating packets C1 and D1.**

**1. FORGE.PRIM / FORGE.PRIM_EVAL — now UNBLOCKED by D2.** R199 deferred the
forge program page kind because four of six families have no evaluator; **the
owner chose to pay for the evaluators.** Freeze the page kind, build them.
**Do not take opcode 0x0304 (W04) or 0x0305 (`DrawPosedForm`).**

**2. `form -> clip bank` — IN FLIGHT AS `FORMIDX`, and it may not be a decision
at all.** SHADOWSUB reported in passing that **`zhao_geom_drawjob` already
holds `form_idx_q`** — so the index may be **exposable, not inventable**, and
the question a wire rather than a law. **FORMIDX is verifying that first**,
because it is a passing remark from a lane whose subject was something else. If
it does not hold, this goes to the owner with the three options below,
unchanged. The original statement of the question: `zref::creature_page::Record` keys ladder rows by `form_index`;
`zref::clip_page`'s header carries **no form index, no type key, no handle**.
Wiring the draw through would assert *"the resident bank is this draw's bank"*
and serve **a correct palette for the wrong animal**, invisible to
`bone_mismatch_o` when bone counts match. Three options costed; the cheapest is
one `u24` field and one golden rebuilt. **This should go to the owner with the
options, not be decided in a packet.**

**3. The HUD band (R233, ruled) — IN FLIGHT AS `BANDBUILD`.** 12 M10K, ~595 ALM, zero DSP, 11.1% of frame.
Needs a **TWOD.BAND contract before any `blocks.yml` row**, and the admission
law is already ruled (R235: refuse the sprite whole and **COUNT** it). The
counter owes a positive control — likely a committed mutant, since legal
stimulus may never overflow at a 9x margin.

**4. I29's consumer**, now that POSEREAD established the request side is
determined by `spec/memory_rules.md` §5f.1. Blocked behind item 2 — **and
`FORMIDX` is the packet that unblocks or re-poses it.**

**5. FORGE.CLIFF.** Rivalry decided (R142, adopt `zhao_forge_cliff_ram`), and
the capability is still absent: **no page issuer, no solid-window producer, no
vdist master**, all three re-searched 2026-09-20 and all three still missing.
A real build, and a large one.

---

## FORGE.SHADOW — MAPPED 2026-09-21 BY SHADOWSUB, which composed NOTHING on purpose

**The blocker list it was scheduled against was wrong in four places.** Read
`design/contracts/FORGE.SHADOW.md` before scheduling the follow-up; it carries
the whole map.

* **The chain is SEVEN blocks, not five.** `zhao_view_projq88` and
  `zhao_measure_starve` are also uncomposed and also required. All five original
  blockers stand — re-measured **by instantiation at statement position, not by
  grep**.
* **THE RATIFIED-LAW QUESTION NEEDS NO OWNER DECISION.** R133 called the
  client-A widening a law re-authoring. The law is **R3 `(owner, explicit)`** —
  keep the time-multiplex, no third *port*, schedule proof owed — **and R3 NAMES
  FORGE.SHADOW's instance-centre 1/w as one of the three sharers.** The widening
  was already performed under R68 sub-build 4 (`PAY_W` 16→17, two-bit `OWNER`,
  `2'd2`/`2'd3` unallocated, `owner_unroutable_o` watching). **What is missing
  is a third ARM and the schedule proof nobody has produced.**
* **What WOULD re-author a law is narrower:** an arena-fill path on client A's
  **result** port. Client B has one; A has none and cannot refuse a result.
* **`tap_*` was marked done and is NOT.** `zhao_terrain_heighttap` has one
  requester group, fully connected, and its response carries **no tag and no
  rider**, so a second client needs an arbiter that does not exist. ***Port
  SHAPE was read as port AVAILABILITY*** — the error to watch for everywhere.
* **R197's untextured door IS built and composed**, so the u/v half of the
  vertex wall is discharged.
* **New, and it blocks more than this lane:** `zhao_geom_drawjob` emits **no
  form index** and **no view index**.

**Freed by D2 and worth a slot on its own:** the CMD.EXEC
`SetView.pixel_error` and `SetPresentationContract.view_count` arms. **I14
defers them only because MEASURE.GOVERNOR was parked, and D2 lifted that.**

## DO NOT SPEND A SLOT ON THESE

* **A sixth FORGE.SHADOW *wiring* packet.** The cluster has gone **21 → 21 five
  times**. It needed a subsystem packet and now has one (SHADOWSUB).
* **I27 via a `cmd_*` producer.** TERRCMD measured it: `terr_chk_*` waits on
  `zhao_terrain_devstore`, and **a `cmd_*` producer would close I27 never.**
  (Devstore greps eight times in the core and **all eight are comments**.)
* **Re-measuring I34.** FIELDLANE found **all four** stated blockers spent; the
  one live blocker is prose in `console_inventory.yml` — one return lane against
  the Earth record's four channels — and **§20.8 forbids the shortcut by entry
  number.** I34 is the directive's **Commit G**.

---

## BEFORE COMMISSIONING ANYTHING: re-measure the blockers

**This is the highest-yield hour available and six lanes have now proved it.**
R165 found two of I34's three spent; FIELDLANE then found all four; VIEWMASK
found two of I21's five held open by **rotted citations**, cutting the count
*"from five to three and a half with no RTL changing"*; CFGARM found I14's hold
on I21 already expired, with the rot **live in production RTL**.

**And the inverse trap, R229:** a re-measurement lane's flattering direction is
**finding** rot. POSEPAGE nearly filed one that did not exist. ***"I expected to
file an expiry and did not"* is a real result.**

---

## BRIEF DEFECTS FIXED TODAY — do not reintroduce them

* **Derive the smoke list, never enumerate it.** The script declares **ten**
  forms; my briefs said eight for weeks, omitting `-NoTableLoad` and
  `-BadDescriptor` — **both INVERTED controls that pass WITH one `%Fatal`.**
  ```
  awk '/^param\(/,/^\)/' tests/prod/run_console_core_smoke.ps1 | grep -oE '\[switch\]\$\w+'
  ```
* **Two generators are in `tools/quartus/`, one in `tools/design/`.** A gate at
  a wrong path returns **RC 2 "No such file"** — *a failing gate that was never
  run.*
* **Run BOTH `gen_shell_paired_diff` forms.** The bare `--check` returned RC 0
  *"fresh"* while `--check --mutant` returned RC 1 *"STALE"*, and the stale
  mutant **aborted `cmake --preset` for every lane.**
* **Scale the gate set to the change (R227).** Comment-only RTL owes the static
  gates, the tie-off audit, the register and `-LintOnly` — *nothing else.*

---

# THE QUEUE IS NOW THE OWNER'S SIX, ratified 2026-09-22

**`reports/OWNER-RATIFICATION-20260922-COMPLETION.md` and the source document
`reports/Zhaozhou_proposed_completion_rulings_2026-09-22.txt`. READ THE SOURCE,
not a summary.** Reviewed commit `9a8b329a`.

**THE GENERAL AUTHORIZATION GOVERNS EVERY ITEM BELOW:**

> *"The coordinator owns their implementation details, generated layouts,
> adapters, arbitration and validation. **Do not repeatedly escalate the same
> decision because its implementation needs another field, decoder arm or
> bounded helper.** … **A missing implementation already commissioned here is
> work, not an unresolved policy decision.**"*

**Escalate ONLY** a concrete contradiction with a still-binding owner
requirement, a new externally visible behaviour not covered, or a genuinely
broader permission request — **naming the conflicting clauses and recommending a
resolution.**

**Every packet reports in the owner's format, per item:** policy adopted ·
producer implemented · **production consumer connected** · **real
command-to-output behaviour exercised** · failure cases tested · area/timing
**estimated or measured, kept separate**.

> *"An otherwise green smoke whose upstream fixture never reaches the new path
> does not prove the path."* — **This tree's smoke fails every terrain page's
> CRC, so terrain's composed door never opens. A counter placed past it CANNOT
> BE FIRED, and one lane had to move one mid-packet for exactly that reason.**

**EXPECT THE REGISTER TO RISE.** Five of six commission capability that does not
exist; R214 says contract+silicon owes a ledger row. 22 to 25 preceded 25 to 10.

---

## 1. PARTICLES — explicit `NO_MATERIAL` mode  *(closes a live drop)*

**Particles are currently DROPPED at the clip door.** R197 refuses untextured
geometry that would be sampled; a polygon particle has no material at all, so it
is refused — **every counter healthy, particles simply absent.**

`NO_MATERIAL` is **a lawful mode, never inferred** from a failed lookup, a
sentinel handle, the previous span's material, or the untextured bit. No
resolution request, no fault counter. **R197 stays for `MATERIAL_BACKED`.**
**Material mode is part of the span's IDENTITY** — mode changes honour the
existing drain/ordering so in-flight triangles keep their own profile; **no
independently advancing metadata queue** (that is this repo's own swap defect).

**Covers the WHOLE particle-to-raster path including canonical depth
conversion** — *"not permission to close the task after changing only the
material gate."* CLIPDOOR already located that blocker: `t_d_o` is Q16.16 1/w,
slot 0 is invw24, D-4 forbids a consumer converting, and **`w` exists at
`zhao_part_project` and is dropped at the ladder queue.**

## 2. PROCEDURAL MATERIAL — a real (set handle, record ID) pair

`material_set` = **complete** `handle32[material_set]`; `material_id` = an
**independent u16**. **Reserved `DrawProcedural` payload bytes are authorized**
for the ID; `frame_tick`'s allocation is preserved; both explicit in
`commands.zidl` and **regenerated** into C++/TS/SV bindings and validators.

**WHY NOT THE OBVIOUS PACKING — the coordinator got this wrong and the owner
caught it:** the handle is **24 index + 8 generation bits**, so reading its low
16 as the record ID means **a residency event silently repaints geometry.**

Zero-filled legacy ID bytes select **record 0, a valid index**. **Where an
earlier implementation guessed differently, DISCLOSE the rendering difference —
do not silently overwrite goldens.** An additive opcode is the authorized
fallback **with no further owner round-trip**. Capture the pair on the draw's
**own accepted handshake** and keep it attached through lookup, evaluation,
assembly and clip-door admission.

## 3. TWOD — BOTH `SetPlane` **0x0306** and `DrawSprite` **0x0307**

**VERIFIED FREE 2026-09-22:** 0x0300–0x0305 taken, 0x0310–0x031F sky-reserved.

**The band is built, composed and permanently idle without this.** `SetPlane` is
the restricted backdrop/atmosphere descriptor — **not** a depth-tested plane or
a second unrestricted texture unit. `DrawSprite` is the HUD path; **text stays
glyph sprites authored by software — no font engine, no private sampler.**

**Descriptors are FRAME-SCOPED:** staged, validated, **published as a SEALED
list** at the boundary owning that frame; **neither a later packet nor the next
frame may mutate a list being consumed**, and an empty frame **must not retain
yesterday's HUD.** **Includes the real asset/palette producer** — *"an opcode
plus descriptors referring to data that only the testbench can inject is not
completion."* **R235 preserved: refuse the whole sprite and COUNT it.**

**Budget correction from the owner:** the 24 M10K is the **pixel band**;
display-list storage is additional. **Do not quote 24 as an all-in HUD cost.**

## 4. GEOM.PARAMBUF — a NARROW ENGINE1 window

`[0x06000000,0x06400000)` view 0 · `[0x06400000,0x06800000)` view 1 ·
`[0x06800000,0x06A00000)` shared scratch. **Half-open.** The asset pool at
`[0x06A00000,0x08000000)` stays **read-only to ENGINE1**; no other client gains
access. **No blanket bank-3 permission.**

Overflow-safe extent checks; **a request crossing a boundary is NOT allowed
merely because both endpoints lie in the union.** **Request identity travels
with the request** — *"do not validate a queued request against a later global
view selector"* (this repo's metadata-swap law). **Extend
`mem_guard_no_escape`, never bypass it, and include a DELIBERATE FAULT that
makes the proof FAIL.**

**Covers the arena producer, allocation, chunk management, write route, readers
and consumers** — the existing decoder *"is not the whole subsystem"*, and
*"do not pack fields into a byte vector merely to unpack them again and count
that as external-memory integration."*

## 5. `sparse_fill` — configuration, not a producer

On for `VALID_MODE == 0` (bitmap-valid), **off** for dense-seal. **Refuse the
unsafe combination**, hold the setting **stable for a whole job**, keep the
documented override. **No new command producer.** Demonstrate **identical
rendered output** across LOD levels, both views and backpressure.

## 6. NEIGHBOUR EDGES — build the real producer

**`8'h00` is RETAINED as the conservative fallback until the real producer is
validated, and must NOT be changed to a cheaper constant.** *(The bare literal
is deliberate: a named localparam was removed because `packet_h_tieoff_audit`
counts literals and the name HID FOUR TIE-OFFS from it. Do not reintroduce it.)*

Bounded **prepare/reconcile/emit** sequencing over the frame's admitted terrain
set is authorized; it **may buffer decisions and adjacency metadata**; it does
**not** authorize a second terrain engine or a duplicate world store. Test mixed
adjacent LODs, job-order permutations, deformation, morph transitions, both
views and backpressure.

> *"This is an explicit authorization to build the missing scheduling
> functionality, not a claim that it is already cheap or complete."*

**AND IT CHANGES THE ENDGAME:** *"If a first diagnostic fit uses conservative
edge mode before adaptive reconciliation is complete, label that profile and its
remaining functional/performance limitation; **it is not the requested final
no-caveat full-capability fit.**"*

---

## SUGGESTED ORDER (coordinator's, not the owner's)

**1 → 3 → 2 → 4 → 6**, with **5** folded into whichever terrain lane is open.
Particles first because the path is mapped and something is being *dropped*
today; TWOD next because the hardware exists and is idle; PARAMBUF and the
neighbour-edge scheduler last because both are genuine subsystems.
