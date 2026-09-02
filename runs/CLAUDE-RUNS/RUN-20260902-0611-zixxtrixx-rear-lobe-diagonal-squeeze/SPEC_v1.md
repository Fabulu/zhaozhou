# SPEC v1: Zixxtrixx rear lobe, head travel, diagonal squeeze, deeper compression — Owner Direction 22

**Run ID:** RUN-20260902-0611
**Created:** 2026-09-02 06:11 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Direction 21's pass is "markedly improved" and accepted as a base. Four faults
remain (Owner Direction 22), judged by eye, in motion, at native resolution:

1. **TOP PRIORITY — the rear joins the S at the COLLAPSED pose.** The rear
   carries its own lobe at the deepest squeeze; curvature genuinely changes
   sign behind the support; the tail's curl does not decrease from assembled
   to collapsed. Proven from a RENDER (fixed orthographic side view, last five
   segments distinguishable) — never from a table.
2. **The head travels backward because the S grows and travels backward** —
   the neck does not curl tighter than idle (segments 5-7 <= 14600/21400/25200
   a16) to fake it.
3. **The squeeze presses down AND back, on a diagonal** — the launch then
   releases along that same diagonal, forward.
4. **Compress more.** Visibly below the shipped ~64% of idle height; packing
   taken by kSpringBodyFlattenQ16 + spread, never by intersection.

Plus: Direction 21's other results preserved (no rollover, no clipping,
accepted salto, explosive release); idle S untouched.

---

## Scope

**In scope:** spring pose tables/knots/deform knobs in
`zhaozhou/tools/reel/zixxtrixx.h`; probe band re-derivation in
`zixx_probe.cpp` where deliberate re-authoring moves an envelope;
render-proof evidence; the 22-subject re-render, Archive Generation Thirteen,
encode/assemble, merge mains, one publish.

**Out of scope:** idle S (correct, frozen), neutral geometry/topology,
head/neck geometry, face wedge, pigments, eyes/pupils/orange stripe, fins,
normals, Cool Cross rig, the restored salto wheel, reel CRC three-way
discrepancy, ownership migration, FPGA/RTL, `sacengine`.

---

## Constraints

- Build only via `tools/reel/build-direct.sh` (ONE target per call); fresh
  run-local build dir `build-d22`; never `cmake --build`.
- Render with explicit `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.
- Never `git add -A`; commit and push as work happens.
- Author by eye; measurement on the comparison side only; every value a named
  editable constant.
- Judge from every-frame contact sheets, the fixed side/micro diagnostic
  cameras, and before/after pairs vs the live bank.
- Confirm the judged clip is the changed clip on BOTH paths (golden
  monolithic attack and planned consumers).
- The rear-lobe claim ships ONLY with its render proof. If the rear cannot
  carry a lobe, say so plainly and say what would be required.

---

## Don't Retry (route geometry, proven this run)

- A planar chain cannot close a loop (Jordan): max winding < ~360, and the
  entry/exit must diverge promptly.
- With neck 5-7 capped at idle curl, any 180-deg reversal through the neck
  costs ~400+ mm of height; the crest floor is ~600-650.
- Over-the-top unwraps on the LAST TWO tail stations stab the ground
  mid-sweep (-722); wrap 14-16, keep 17-18 normal-side.
- kSpringChainLag 320 to de-sync the wind: wrecks everything, reverted.
- Authoring assembled as the half-wound pose: hairpin at joint 9/10 and
  the entry side dies; the wind must stay staged absorb(mild) ->
  assembled(grown+quill) -> seating(rear done) -> collapsed.
- The tail cannot pass behind the dive (leg+crest seal that side); the pad
  must run under the coil, tip rearmost of the pad.

## Don't Retry

- Proving the rear curl at the ASSEMBLED pose (the trap that produced three
  false "fixed" claims — the owner judges the COLLAPSED pose).
- Manufacturing head travel by tightening the neck past idle.
- Buying compression depth with interpenetration instead of flatten/spread.
- Judging from evenly spaced stills.
- Widening a probe gate to pass a fault.

---

## Open Questions

- How low can the squeeze bottom before the stacked flattened runs must
  intersect — found by authoring + probe, not assumed.
