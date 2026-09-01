# SPEC v1: Zixxtrixx final explicit whole-S animation candidate

**Run ID:** RUN-20260831-1956
**Created:** 2026-08-31 19:56 UTC+02:00
**Status:** Complete — independent-review correction accepted; final direct probe and meshcheck pass
**Previous Version:** N/A

---

## Objective

Produce a pushed implementation candidate, from fresh current-main clones, that finishes Zixxtrixx animation without integration or publication. First author one visually convincing deepest fixed-side pose as a broad enlarged two-lobe S moving strongly down and back, then build the exact required entry, hold and release around it using explicit owner-editable whole-centreline key poses rather than procedural slope/gain iteration. Only after the spring passes side, high-three-quarter and micro visual review may the bounded pupil, fall, hit, landing/KO/death, idle and moving-light corrections be finished.

---

## Starting State and Ownership

- zhaozhou current-main base: `16aed6c5e3c11281c802083745647c84f417fb5d`
- Upheaval current-main base: `2ad25aac4c3edd4a8238880537ca00c235684923`
- Supplied older bases `7c646b07dd3d993b498baec2336143b336073415` and `f80e70a4067f31a4deae80a47a55a62f8fbd94b4` are verified ancestors.
- Feature branch in each clone: `feature/zixxtrixx-whole-s-final`.
- Sole implementation artist/animator; no delegation.
- Shared checkouts and the hardware lane are read-only and untouched. All build/render outputs are lane-local.

---

## Scope

**In Scope:**

- Add missing durable Owner Direction 19 to the isolated Upheaval feature branch and keep AGENT-PACK pointers current.
- Inspect current main and read-only `origin/zixxtrixx-whole-s-groundspring`; selectively reimplement only safe needed infrastructure already absent from main: generic deformation sidecar, independent presentation-midpoint ownership, correct 0..64 influence/contact logic, finless full/micro probe labels, diagnostic subjects, shared consumer ordering and accepted moving-light strength.
- Replace rejected local slope/gain generation with a small named set of explicit complete centreline poses: exact grounded, absorb, assembled and collapsed/deepest. Controls are absolute station headings/turns or direct fixed-length 2D chain targets; deterministic reconstruction derives rigid bones and rotations from the accepted centreline.
- Keep station 14 as the fixed named support unless one deliberate visual representation change proves another fixed support necessary. Root compensation is derived from each sampled centreline/support only; no independently authored root retreat or anticipation lift.
- Exact spring timing: entry/absorb keys 0–6; down/back collapse 6–12; deepest hold 12–18; collapsed→assembled→absorb→exact grounded S keys 18–22; intact-S rigid lift 22–24; airborne wheel gather 24–28.
- Preserve authored finless full/micro ground bite through key 22 and every true presentation midpoint before lift.
- Share anticipation and release across golden/planned attacks, one-/multi-turn jumps and six-/nine-salto consumers.
- After spring acceptance only: selective clip-specific pupil acting; monotonic fall hesitation without reversal; stronger station-local hit bends; distinct landing, KO and deaths with seams intact; idle diagnosis under explicit celmain/Cool Cross; preserve the accepted moving inspection light or selectively reimplement its reviewed controls if main lacks them.
- Commit a posed-vertex probe that walks real deformed full/micro vertices at integer keys and authored presentation midpoints.

**Out of Scope:**

- Shared-main integration, archive generation, full twenty-two-subject bank, live website media, site assembly, deployment or publication.
- Neutral model/proportion/pigment/topology/normal/texture/lighting reconstruction.
- Any worktree, Sacengine, CMake/Ninja/Verilator build path, shared checkout write, hardware-process inspection, broad catalogue rerender/redecode or historical CRC investigation absent a changed-code regression signal.

---

## Art and Representation Method

1. Author the collapsed/deepest whole silhouette first at fixed side, with named complete pose controls and no deformation used to counterfeit its centreline.
2. It must read as a broad, enlarged, low two-lobe S with positive volume and obvious down/back travel—not a C, capsule, arch, hairpin, crossing, rod or buried fragment.
3. Render and look at native 384x240. Measurements may compare/protect the chosen art, never select the pose.
4. Maximum three visual rungs with the first explicit representation. If it cannot produce the silhouette, change representation once rather than micro-tune.
5. Hard cap eight visual rungs total. Preserve/report a blocker rather than silently continuing.
   **Authorized representation exception (coordinator, after rung 8):** exactly one additional visual rung may replace failed interpolation topology with explicit complete whole-body presentation-midpoint poses through the existing per-channel authored-midpoint provenance. It may not retune any accepted integer heading table, timing, support, deformation or root law. First reuse coherent adjacent/bridge whole poses; inspect every key and true half-key 18–22 at side, high-three-quarter and micro. Any remaining fragment ends the pass with no further rung.
6. Once deepest passes, author assembled and absorb complete poses and timing around it; inspect every spring sample at fixed side and high-three-quarter, then micro.
7. Add subordinate flatten/spread only from assembled→collapsed, with exact reversal before the airborne seam. Local skull attitude is optional, tiny and nonessential.

### Visual rung journal

| Rung | Representation | Change | Fixed-side verdict | High-3Q / micro verdict | Decision |
|---:|---|---|---|---|---|
| 0 | current main baseline | untouched direct build | pending | pending | establish before |

---

## Acceptance

- Key 0 is the exact signature grounded S.
- The complete finless body, through the tail tube, visibly absorbs into an enlarged S; the tail follows and never leads.
- A distinct assembled S precedes compression.
- The whole S, head, neck and body bend drastically down and backward until nearly flat on the ground while retaining positive volume and two readable lobes.
- No early lift, rear spear, global retreat, tail kick, rod, C, capsule, arch, hairpin, crossing or terrain-hidden fragmentation.
- Station-14 support stays planted through sampled centreline-derived compensation; deliberate ground bite persists through key 22 at full and micro integer/midpoint samples.
- Release exactly reverses collapsed→assembled→absorb→grounded before rigid lift and wheel gather. Full/micro/deformation/attachments/normals remain valid and deformation is exact identity outside authorised compression.
- Golden/planned attack, one-/multi-turn jump and six-/nine-salto consumers share silhouettes/order.
- Spring sheets: every-frame fixed-side, high-three-quarter and micro, plus attack/jump release and concise before/after, all reviewed at native resolution.
- Secondary changes remain bounded and visually distinct, with required seams and accepted identity preserved.

---

## Named Validation Budget

**Budget: Final Whole-S Focused Candidate.** Every command must answer one listed question; stop when answered.

1. One lane-local `tools/reel/build-direct.sh --output <RUN>/workbench/direct --clean all` after source/layout work; recompile every dependent `.cpp`.
2. Up to eight native visual rungs, starting with one deepest fixed-side pose; side is decisive. High-three-quarter and micro only after side passes.
3. Every true spring sample for accepted fixed-side/high-three-quarter/micro subjects; focused release sheets for one attack and one jump, plus parity diagnostics for all consumer classes.
4. One committed posed-vertex sweep over accepted integer keys and presentation midpoints, full and micro independently; midpoint/deformation/mesh checks and exact identity outside spring.
5. Targeted every-frame or badness-ranked evidence only for changed pupil/fall/hit/landing/KO/death/idle/light clips. No full bank.
6. `git diff --check`, focused tests for touched code, tracked-clean proof and untracked evidence inventory.

---

## Don't Retry

- Do not copy the abandoned lane’s rejected iteration-93 source or resume its dirty lane.
- Do not run another procedural local slope/gain table loop.
- Do not freeze assembled/collapsed endpoints and interpolate a single shared absolute-slope clock through them: iterations 3–93 produced rods, Cs, capsules, arches, hairpins, crossings or buried fragments.
- Do not use equivalent-turn bands at stations 5–12; iteration 93 was broad only at isolated samples and fragmented at keys 7–8 and 10.5–11.5 with an upright 8.5 kink.
- Do not use radial deformation, skull attitude, root retreat/lift or probe thresholds to counterfeit a failed centreline.
- Do not select art values from measurements or gates.
- Do not rerender/redecode the full catalogue, repin historical reel CRCs, edit live media, archive, integrate or deploy.

---

## Open Questions / Cheap Reversible Assumptions

- Start with absolute per-station 2D headings for four complete poses because fixed-length chain reconstruction naturally preserves length and yields bone rotations; if the first three rungs cannot create the broad low S, switch once to direct fixed-length 2D chain targets.
- Keep station 14 fixed unless the whole-pose representation visibly pivots/slides there; no support search or measured optimisation.
- Reuse existing safe main infrastructure before porting anything from the read-only reviewed branch; inspect diffs at commits `f412fca` / `b3cc7e3` selectively, never merge/cherry-pick the branch wholesale.
