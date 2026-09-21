# FINDINGS — VIEWMASK (entry I21, TERRAIN.GROUP_SEQ's subpatch job port)

**2026-09-21. Register 21 → 21. Comment-only: no RTL, no port change.**

Transcribed by the coordinator from the packet's commit message (`81aa10ff`).
The harness refused the lane's own write to this path, as its brief predicted;
the lane put its findings in the commit and said so, which is the protocol.

**Headline: the struck decision held, and settling it did NOT close the entry.
That is the finding.**

---

## 1. The struck decision held — re-measured in this tree, not inherited

**There is no player mask anywhere in this tree.** Five independent sources:

* `spec/commands.zidl`, `SubmitTerrainSet` 0x0230: `u8 view_mask;` — *"which
  views this set was unioned for"*.
* **T5** (`OWNER-RULINGS-BUILDABILITY-20260902.md`) names it `view_mask:u8` in
  **both** the command and the 32-byte record; canonical order is the
  view-union key.
* `spec/video_rules.md` §3.1, ratified 2026-08-15 (MAJOR-3): **View 0 (P1),
  View 1 (P2)**. Not a probability — a ratified table.
* `zref_sw_stream.hpp` accumulates `e.view_mask |= view_bit` and tests
  `c.view_mask == 0x3` for dual: **bits [1:0] only**.
* `zhao_geom_group_seq.sv` `$fatal`s unless `NVIEWS == 2`.

**One correction to the dossier:** that file is in `fpga/rtl/geometry/`, not
`fpga/rtl/geom/`. *The claim holds; the path did not.*

### Where the false premise came from, and it is the lane's best finding

**The core's own phrase "T5's per-player tag" is the ONLY source of the player
reading. T5 does not use the word for this field.**

> **Six passes quoted the comment back as ratified law and it outranked
> `video_rules` for five weeks. A caution invented in a comment and cited by its
> neighbours is indistinguishable from a ruling.**

This is `CLAUDE.md`'s broken-instrument law applied to *prose*: the comment was
never checked against a primary source because it read like one.

## 2. And settling it does NOT close I21

**`terr_job_view_mask_i` is ALREADY 2 bits at the core boundary — the narrowing
was never the obstacle.** The other fifteen job fields arrive on that same
boundary from the **absent subpatch issuer**.

Driving `job_view_mask` from the internal `tis_view_mask` while the rest of the
job comes from outside **joins two things that move independently: a worse
hidden adapter than the one the entry refused to build, wearing a settled ruling
as cover.**

> **The view mask rides THE JOB. The job has no producer.**

## 3. The five blockers, each re-measured 2026-09-21 — three stand, one settled, one half-expired

**B1 — LAYER-E READER: STANDS.** *Evidence rotted; the claim got stronger.*
*"7,622 appears in zero files under `fpga/`"* is **false** — `zhao_terrain_pageio.sv`
carries it twice. But pageio names E's head only as a foreign-byte boundary of
layer D's burst window, **writes those bytes back unchanged**, and
`pageio_rtl_directed` **asserts layers A, C and E byte-identical after a bake**.
*A block handles E's bytes and interprets none.* `pagestream` still reads exactly
`A_OFF`, `B_OFF`, `C_OFF` and nothing else.

**B2 — NEIGHBOUR EDGE LEVELS: STANDS.** *Stale instance name.* The LFSR is
`u64_src`, not `u59_src` — **the generator renumbers instances, so an instance
name quoted from a GENERATED file has a shelf life.** `MEASURE.GOVERNOR.md`
still refuses in writing: *"The camera POSITIONS, dual and `edge_*` are NOT
here."*

**B3 — VIEW-MASK RECONCILIATION: SETTLED.** Never an owner question.

**B4 — `terr_cc_serve_release_i`: STANDS.** Boundary input on core and board,
reaching `u_terrain_compcache.serve_release_i`. No internal driver: a grep over
`.sv`/`.cpp` returns only the port, the pass-throughs, the consumer's own
edge-detect, and comments.

**B5 — `cam0`/`cam1` SCALE: THE FORMAT HALF HAS EXPIRED AND THE ENTRY DID NOT
KNOW.** Ruling R165's shape exactly.

* *"R83 IS IMPLEMENTED NOWHERE"* is **false** — it is implemented under R98, in
  both ports, exactly as terrain8 said it would take.
* *"`zhao_measure_governor.sv` still declares `proj0_i`/`proj1_i` as [15:0]"* is
  **false** — they are `[PROJW-1:0]` with `PROJW = 20`, and `zhao_view_projq88.sv`
  declares `proj0_o`/`proj1_o` the same off its own `PROJW = 20`. The governor
  header now carries R83's saturation table and *"DONE: PROJW = 20 (Q12.8,
  ceiling 4095.996)"*, with `STEPS` derived from `PROJW` rather than written twice.

**So the "fit a circuit you already know is wrong" objection is SPENT. What
survives is smaller:** neither block is instantiated by any production root, and
`prod_manifest` names the remaining dependency itself — projq88 is *"additionally
dependent on I14's still-open half (the viewport rect at projector cfg address 17
has no CMD producer)"*.

> **Blocker 5 is now a composition behind entry I14, not a format defect.**

## 4. R13 disposes of a second piece, and the entry was arguing with itself

I21 quotes R13 at blocker 1 and then **ninety lines later still calls the same
question open** — *"(b) is a contract conflict … and it is the owner's"* — and
calls `job_mat_a`/`_b`/`_weight`'s owner **UNIDENTIFIED**. *One entry, two
positions.*

R13's last clause decides something specific: *"The job port is not widened to
carry a subpatch-uniform value that is not true."* Those three fields are the
**WRONG CARRIER and are not to be fed** — not three fields awaiting an owner, but
three fields a ruling says should not carry the value, **whose honest closure is
their REMOVAL once a per-triangle layer-E path exists to replace them.** A
function move; nothing leaves until the replacement lands.

R13 does not supply the reader (B1) and puts it **inside TESS**: a build, not a
wire here. *That third of I21 is now ONE absence with a ruled destination.*

## 5. What the lane deliberately did NOT do

**No counted refusal for `view_mask[7:2]`.** The dossier says decide it *"in the
composer's commit"*. **There is no composer's commit:** `terrain_cmd.sv` forwards
byte 22 uninterpreted, `terrain_seq.sv` carries all eight, and **no block in
hardware reads a bit of it.** *A refusal counter guarding a field nothing
consumes is a guard that cannot be shown to matter.* Residue recorded in the
entry for whoever composes the consumer.

**No manufactured register rise.** `zhao_view_projq88`, `_projscale` and `_eye`
have **no `design/blocks.yml` rows AND no contracts** under `design/contracts/`.
That looked like R214's PAGEIO shape and **is not**: PAGEIO wrote a row for a
capability that **had a contract**. Inventing `VIEW.*` rows to read 22 would be
manufacturing a capability.

> **An honest 21 beats a dishonest 22.**

## 6. For the handover

**I21's real remaining shape is three absences and one composition:**

1. a **layer-E reader inside TESS** (R13-destined);
2. the **inter-patch edge levels** (contract-refused);
3. the **compose-cache retirement pulse**;
4. the **projscale → projq88 → governor composition, behind I14**.

The deviation store's **185 M10K of 553 (33%)** is a **cost, not an absence**.
*(Coordinator note: read that against R222, which refused the HUD store at 180
M10K of the same 553. Two features asking 365 between them, on a device whose
M10K occupancy has never been measured.)*

**The view-mask question should leave the decision board for good.**

### And the process finding the lane ends on

> **TWO blockers here were held open by rotted citations: re-measuring five took
> under an hour and cut the open count from five to three and a half with no RTL
> changing. Budget that on every entry before commissioning work against it.**
