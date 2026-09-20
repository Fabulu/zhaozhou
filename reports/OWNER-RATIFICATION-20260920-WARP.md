# Owner ratification — GEOM.WARP decisions W01–W18, 2026-09-20

## Authority

These eighteen decisions are ratified **by this document's source**, and by
nothing older:

* **Document:** `reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt`
  (revision 1.1, 2,445 lines).
* **Owner commit:** `4c256137bba97bb272696782bb1756c10407f70b`, authored by
  Fabian, Sun 20 Sep 2026 12:23:32 +0200, message *"Agent please read -
  implement this warp architecture and architect implement whatever else is
  missing when it comes up"*. That commit adds the directive and nothing else
  (1 file changed, 2,445 insertions).
* **Ratified:** 2026-09-20, run `RUN-20260919-1656-gaps-to-zero`, WARP packet.

The directive states its own status at lines 9–16 and it governs how this file
is written:

> *"This is an owner-ready architecture proposal, not a claim that the described
> hardware, ABI additions, tests, or measurements already exist... Decisions
> labelled W01–W18 below are NEW resolutions of the missing Warp contract.
> Supplying this document as an owner directive can ratify those decisions; **the
> implementation must record that ratification in the repo, not attribute it to
> an older ruling that never said it**."*

So: **W01–W18 are new law as of 2026-09-20.** None of them is attributed to
R7, to the 2026-09-18 revocation, to the 2026-08-31 cut, or to any ruling in
`reports/OWNER-RULINGS-20260919-EVENING.md`. R7 makes GEOM.WARP *mandatory*; it
says nothing about how Warp works, and it is cited below only for that.

## What this document does NOT ratify

The directive states at lines 18–22 that its review **ran no Quartus, no
Verilator, no project test suite and no board**, and claims **no ALM, DSP, RAM,
timing or full-workload result**. Every cost, cycle and capacity figure in it is
therefore an *estimate to be checked*, not a measurement. Ratifying W01–W18
ratifies the **semantics, laws and prohibitions**; it does not ratify any
number quoted as a cost. Wherever this implementation repeats one of those
figures it says so at the point of use.

Section 18.2's "at least 79 preload/clear cycles" is the clearest example: it is
explicitly *"arithmetic from the FSM, not a benchmark"* (line 1488). It is
treated here as a hypothesis to measure, and it is measured — see
`FINDINGS-warp.md`.

## The decisions

| # | one-line law | where it binds |
|---|---|---|
| W01 | Warp has **FIFTEEN** input lanes, not fourteen; six output lanes. Correct the parenthetical count in `spec/form/field-ir.md` §7.1; drop no named field to make prose add up. | spec, reference, RTL lane packing |
| W02 | Application space: position is the existing **skinned world-space Q16.16** position; input normal is the existing **range-reduced, NON-unit** world direction carried as signed fixed-point words; displacement is world-space Q16.16; output normal is a **replacement** world direction. | zref, RTL |
| W03 | Application arithmetic: `Pout = componentwise canonical saturating ADD(Pin, d)`. `Nout` = the program's three output normal words, adapted by the **existing common range-reduction rule** for LIGHT. No Jacobian, finite differences, extra normalization, blend with the old normal, or amplitude multiplier. | zref, RTL |
| W04 | Add **`DrawWarpedForm` at opcode 0x0304**, 96-byte record / 80-byte payload. `DrawForm 0x0300` and raw vertex format 0 stay **byte-for-byte** unchanged. 0x0312–0x031F remain sky-reserved. | `spec/commands.zidl` + every generated consumer |
| W05 | **Snapshot every draw.** Program handle, time, params, attribute mode, attribute resource and displacement bound belong to *that* draw. **No mutable global `current_warp` register.** | CMD.EXEC descriptor sidecar |
| W06 | Supply all four attributes honestly: **INLINE4** constants per draw and **STREAM4** per-vertex data from a versioned `WARP_ATTRIBUTES` resource. Do not steal format-0 reserved bytes, commandeer formats 1/2, or synthesize attributes from guessed UV units. | ABI, resource kind, reader |
| W07 | **One shared program-binding authority.** handle → canonical program → prepared plan / resident slot is a common Field service. A handle is not a content hash; a content hash is not a physical resident slot. | `fpga/rtl/field/` |
| W08 | Bind identity to **actual transfers**: draw-instance cookie + meshlet/batch identity + vertex ordinal. `source_id` remains **attribution, not identity**. | join, tokens |
| W09 | **Preserve the ordinary path.** `DrawForm` disables Warp, performs zero Warp lookups/evaluations, and preserves every existing output. A bypass throughput regression is measured and resolved, not excused. | regression gate |
| W10 | **Publish no partially warped meshlet.** On active Warp failure, poison and drain that meshlet via the existing batch refusal/release mechanism. An absent output must not look like a zero result. | RTL |
| W11 | **Visibility is conservative BEFORE fetch/deform.** Every active Warp draw declares a world-space componentwise displacement bound; expand visibility bounds before culling, account for deformation in LOD error, and **runtime-check returned displacements** against the declaration. | MESHFETCH / CULL |
| W12 | **One Field fabric, one projector complex.** Add a logical Warp *client*. No private execution engine, math bank, normalizer or projector. | composition |
| W13 | **Three separate performance claims**: application II; Field evaluation throughput; whole-frame affordability. **None proves the other two.** | reporting |
| W14 | Program legality unchanged: common Warp opcode whitelist, canonical **48-op** ceiling. A legal program need not be realtime. A missing hardware lowering is **recorded explicitly**, never relabelled illegal nor answered by an unannounced old engine. | cost model |
| W15 | **Honour mandatory service capacity.** Warp may not steal reserved Earth or other guaranteed profile work. No invented fixed percentage, vertex quota or cycle denominator in RTL. | admission |
| W16 | **All shared-host limitations are real prerequisites** — lane capacity, output maps, program/table lifetime, prepared-uniform identity, complete result capture, commit/run coherence. **Do not bury a second Field loader inside GEOM.WARP to avoid doing this.** | `fpga/rtl/field/` |
| W17 | **Freeze numeric and content ownership.** Artist choices stay authored data or named constants. The normal-deformation law is authored by the **Warp program**, not guessed by the shell. | CLAUDE.md rule 6 |
| W18 | **Close by end-to-end evidence**: real command → real binding → real Field v3 execution → coherent warped vertex/normal → lighting/projection → reference-checked result. A built block, a zero gap counter, or a picture **alone is not sufficient**. | acceptance |

## Conflicts this ratification resolves against existing repo text

The directive's own conflict register C01–C12 (lines 246–301) names the
documents that are now **wrong** and must be corrected rather than inherited.
The three that change existing written law:

* **C01 / W01** — `spec/form/field-ir.md` §7.1 says "(14)" over fifteen listed
  Warp input fields. The **fields** are right and the **count** is wrong. This
  is a prose correction, *not* a numeric ISA change.
* **C02** — `design/contracts/GEOM.WARP.md` is a fifteen-section blank citing the
  2026-08-31 §6.3 deferral. That deferral was revoked by the owner 2026-09-18
  and GEOM.WARP made mandatory by R7. The contract is replaced; the superseded
  deferral is annotated, not deleted.
* **C03** — `design/blocks.yml`'s GEOM.WARP entry lists `inputs:
  [instanced_transforms, displaced_vertices]` and `upstream: [GEOM.LOOM, ...]`.
  The **control** dependency on LOOM is retained; the **data** dependency is
  corrected: Warp consumes **post-skin vertices and normals**, never LOOM's
  matrix stream. Treating LOOM's transform matrices as vertices is on the
  directive's prohibited list (line 57).

## Relationship to the standing rulings

* **R7** (owner, explicit) makes GEOM.WARP mandatory. It is the *reason* this
  work exists and it ratifies **none** of W01–W18.
* **R91** is load-bearing for W13/W15 and is a **measured** ruling, unlike the
  directive's estimates: the composed Earth front costs **≥46 clocks/point**
  against a 10,416-clock allowance (**~481%**), and
  `design/contracts/FIELD.SEQ.EARTH.md:105` excludes that arrangement in
  writing. Any Warp submission path over the same host inherits that cost.
  R91's chosen lever — uniforms loaded once per association, 46 → ~3 clocks —
  is the same lever I5 needs. See `FINDINGS-warp.md` for what this packet
  measured rather than assumed.
* **R43** (FIELD's program loader: a doorbell contract on the R14 pattern,
  SW.STREAM stages the plan and writes the existing loader words, CMD.EXEC does
  only handle → program-hash lookup) is the pattern W07/W16's binding service
  must extend rather than replace.
* **R44** (promote `zhao_probe_walk_earth` / `zhao_probe_patch_acc` out of
  `fpga/rtl/synth/` rather than rebuilding them) is the precedent for reusing
  what is on disk; W16's prohibition on a second Field loader is the same rule.
* **R60, R81, R82** govern how this packet's evidence is gathered.

## Amendment rule

Any of W01–W18 may be amended by the owner in one line. Where this
implementation had to choose a value the directive left open, that value is a
**named, editable constant** (CLAUDE.md rule 6) and this file or
`FINDINGS-warp.md` says where it lives.
