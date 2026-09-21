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

| packet | target | branch |
|---|---|---|
| **DELTALAW** | R231's depth-law repair; may close **I32** | `gz/deltalaw` |
| **SHADOWSUB** | the FORGE.SHADOW subsystem, under D2 | `gz/shadowsub` |
| **GOURAUDBUILD** | D1 — reconnect the lit vertex colour | `gz/gouraudbuild` |

---

## NEXT SLOTS, best first

**1. FORGE.PRIM / FORGE.PRIM_EVAL — now UNBLOCKED by D2.** R199 deferred the
forge program page kind because four of six families have no evaluator; **the
owner chose to pay for the evaluators.** Freeze the page kind, build them.
**Do not take opcode 0x0304 (W04) or 0x0305 (`DrawPosedForm`).**

**2. `form -> clip bank` — an OWNER DECISION, surfaced by POSEREAD, not yet
put to him.** `zref::creature_page::Record` keys ladder rows by `form_index`;
`zref::clip_page`'s header carries **no form index, no type key, no handle**.
Wiring the draw through would assert *"the resident bank is this draw's bank"*
and serve **a correct palette for the wrong animal**, invisible to
`bone_mismatch_o` when bone counts match. Three options costed; the cheapest is
one `u24` field and one golden rebuilt. **This should go to the owner with the
options, not be decided in a packet.**

**3. The HUD band (R233, ruled).** 12 M10K, ~595 ALM, zero DSP, 11.1% of frame.
Needs a **TWOD.BAND contract before any `blocks.yml` row**, and the admission
law is already ruled (R235: refuse the sprite whole and **COUNT** it). The
counter owes a positive control — likely a committed mutant, since legal
stimulus may never overflow at a 9x margin.

**4. I29's consumer**, now that POSEREAD established the request side is
determined by `spec/memory_rules.md` §5f.1. Blocked behind item 2.

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
