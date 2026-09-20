# FINDINGS — GEOM.LOD lane (gz/geomlod)

*Transcribed by the coordinator: the harness blocks subagents from writing
report files, so this is the packet's own final report, verbatim in substance.*

**Branch `gz/geomlod` at `feac837d`.** Register **29 at entry and 29 at
`feac837d`** — the brief said 27; it read 29 at base `1fbfacc9`, because the
brief's number came from a tree with the hostdbg merge already in it.

**That is the headline: R68 closes no register entry and could not.** See the
refusals.

Commits: `431854a9` (sub-builds 1+2) · `d70586d4` (R26) · `742405dd`
(sub-build 4) · `feac837d` (sub-build 3).

---

## R68 — all four sub-builds BUILT and tested; none composed

1. **Frozen kind-8 ladder layout** — `spec/cartridge.md` §4c, a partial lift
   (`body_off` leaves the Phase-12 body untouched). Keyed by the MESH_STREAM
   handle index, which is the key `zhao_geom_drawjob` already uses, so there is
   **no second mapping law**. Model: `zref_creature_page.hpp`.
2. **Packer** `tools/pack/mkcreatureladder.py --check` plus a committed golden.
   Three statements of the layout are pinned to ONE artefact: packer → golden →
   `zref::build` byte-for-byte → the RTL reads that file.
3. **`zhao_geom_ladderbank`** (545 checks, 9 counters fired). DOUBLE-BANKED,
   and the property that matters is stated as such: an illegal record four rows
   in must leave the previous page's rows **identical**, not merely fewer.
4. **`zhao_geom_lodstate`** (29 checks) — the per-instance LodState
   `zhao_geom_lod` has lacked since phase 8. **The fifteenth false-absence
   claim, answered.** Ticks once per DRAW, structurally, from drawjob's own
   FSM; differenced against `zref::creature::lod_update` as a live model over
   60 frames × 4 creatures, and the ladder visited all four rungs.
5. **`zhao_geom_projradius` + `zhao_view_projscale`** (2,492 checks, the oracle
   CALLED rather than transcribed). **It refused the near-miss**, which is the
   valuable part: `zhao_part_project`'s marker half-extent is S12.8 of `R/w`
   while the creature law is `kx·R·vw/(2w)`. They differ by `kx·vw/2`, and
   adopting the first would have put every creature rungs coarse with nothing
   downstream able to tell.
6. **R26 `cam*_thresh_q8_o`** (74 new checks, and differenced on all 5,370 +
   146,039 existing cosim frames). Degrade is law G2 read from the other end.

## Refused, with the exact blocker

* **FORGE.SHADOW** — R68 was necessary, not sufficient. The caster is now real
  and **the blocker MOVED to its output**: `vtx_*` is a world-vertex hull and
  GEOM.SETUP takes screen triangles. Searched every `vtx_`-shaped input in
  `fpga/rtl` including `synth/` and the probes — nothing consumes a
  world-vertex fan. **Composing `lodstate` alone would dangle the caster at the
  core boundary and put the register UP by one.**
* **Client-A 1/w stream** — `GEOM_PAY_A_W` is full (3+12 bits with the owner
  tag at 15); it needs widening to 17 plus a front-mux on part_project's
  geometry arm. Worth spending only once the consumer above exists.
* **MEASURE.GOVERNOR** — `thresh_q8` now has a consumer; `pixel_error` and the
  starvation latch are buildable; `proj0/1_i` needed a decision → **ruling
  R73**.
* **GEOM.WARP** — `spec/commands.zidl` has **zero** occurrences of "warp":
  there is no verb, so nothing can order one. `zhao_field_host` is composed at
  `IN_LANES(12)/OUT_LANES(4)/CLIENTS(2)` and warp needs 14/6 and a third
  client, which are core-boundary port widths. `zref::GeomWarp` does not exist.
  The contract is fifteen sections of "Deliberately unwritten" citing a revoked
  ruling, and its `upstream: [FIELD.SEQ.WARP]` is circular.
* **FORGE.PRIM / PRIM_EVAL** — no forge page kind exists (kind 4 is a
  heightfield). **FORGE.CLIFF** — no page issuer, no 34×34 solid-bit window, no
  vdist master. **`zhao_geom_parambuf`** — an SDRAM-record layer, blocked on the
  behavioural SDRAM model.
* **I50** — NOT refused; scoped and not started. The consumer is REAL
  (drawjob's palette). It needs: a doorbell contract (the record widths exist
  nowhere in `design/`), a **64-byte** node record (the beat is 56 B, not a
  burst multiple), `zhao_geom_loomfeed` on `zhao_part_hps`'s shape **with
  R54's refusal path**, `.N(4)→.N(5)` at `zhao_console_core.sv:9415` (index 4,
  below PART.STATE), and a sentence in `SW.STREAM.md`.

## Owner decisions found — all three answered, 2026-09-20

* **D-A — the camera PROJECTION SCALE has no producer anywhere.** → **R73**:
  derive it, do not add an ABI field.
* **D-B — creature ladder state per INSTANCE or per (INSTANCE, CAMERA)?** →
  **R74**: per (instance, camera).
* **D-C — FORGE.SHADOW's hull needs a consumer.** → **R75**: the arena route.

## Two things routed elsewhere

* **`lint_zhao_console_board` RED, and INHERITED** — 8
  `%Warning-PINCONNECTEMPTY` from R64's retired mip planes.
  `git diff 1fbfacc9 HEAD` on the core is **empty** and `git log -S m17_valid_o`
  blames `91335fe2` (terrain5). *Coordinator's note: the FIELD lane repaired
  exactly this, with a waiver scoped to those eight pins and R64 cited beside
  them; the board lint PASSES on the merged tree in 24.7 s. Two packets found
  the same inherited red independently, which is the system working.*
* **The `-Mutant` smoke is flaky IN ITS BUILD** — `COMPILE FAILED:
  ...ConstPool__0__Slow.cpp` on the first run; **that file compiles clean by
  hand with the same flags**, and the identical re-run passed. *Coordinator's
  note: I hit the same transient twice the same morning on two other variants.
  Two independent sightings → **ruling R76**. Their "clean by hand with the
  same flags" is the observation that weakens my memory-pressure hypothesis.*
