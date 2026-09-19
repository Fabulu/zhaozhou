# Manafold version 18 — antenna-root / carrier / particle recon

**Date:** 2026-09-19
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md`
**Baseline:** accepted version-17 renderer `67DCAFF8ABC0AA2BA830C439A4CD98C7`
**Verdict:** **The reported faults reproduce. Root texture and skin-authority mismatches are proven source causes; particle overlap already depth-tests and has one bounded layer-order experiment.**

## Native picture

The complete Hover orbit and enlarged root crops reproduce all three owner observations:

- both body connections visibly change from the body’s smoother bright pigment to a darker, cooler, much coarser antenna grain;
- the front and rear thickened regions read as lobes laid into the body rather than one uninterrupted body surface, with dark internal separation at some angles;
- A/B/C and the two junction swellings dominate the stick gauge as conspicuous protrusions;
- grey/aqua motes visibly overlay antenna pixels in several Channel frames, including `f0030`, `f0090`, `f0150`, `f0210`, `f0270`, `f0330`, `f0390` and `f0419`.

Durable baseline evidence:

- `V18-ROOT-BASELINE-HOVER-NATIVE.png` — eight complete orbit views;
- `V18-ROOT-BASELINE-HOVER-4X.png` — the same moving connections enlarged exactly 4×;
- `V18-PARTICLE-ANTENNA-BASELINE-4X.png` — eight Channel overlap witnesses.

The root fault is not a thumbnail illusion. At 4×, the material boundary, separately shaded tube surface and body-only black boundary are independently visible.

## Exact production graph

### One antenna mesh, but not one body mesh

`tools/reel/manafold_model.h:86-407` builds the entire antenna as one closed `RingPart`; the five “balls” are not sphere parts. `make_loop()` adds smooth swell profiles directly to the same ring skin (`:145-190`) and assigns each ring a two-bone palette (`:192-273`). This correctly prevents bead seams *inside the antenna*.

The body is a separate closed `RingPart` (`manafold_model.h:26-77`). `tools/reel/manafold.h:106-125` compiles body and antenna as separate overlapping surfaces. Their buried ends/caps intersect the body; they are not welded to its topology. Consequently:

- body and loop vertices do not share an attachment ring;
- the compiler’s area-weighted normals are shared only at **exactly equal positions** (`reference/src/zcreature/creature_core.cpp:839-909`), which these intersecting surfaces generally do not have;
- the tube keeps tube/topology normals while the body keeps ellipsoid normals at the crossing.

That separate-normal intersection is a real contributor to the “warped/different surface” read. A texture-only repair can remove the most obvious pigment discontinuity but cannot mathematically weld or normal-match two overlapping closed surfaces.

### The different texture is explicitly authored

The visible material mismatch is not a stale generated page or lighting accident:

- body and loop use the same atlas/page and grey fallback, but different V bands: body `8..120`, loop `136..200` (`manafold_art.h:3405-3421`, `manafold_model.h:401-406`);
- `tools/pack/mkmanafoldpage.py:41-45,65-97` deliberately makes the antenna a cooler/deeper pigment with grain `0.44` and stroke amplitude `0.24`, versus the body’s `0.26` / `0.10`;
- the comment explicitly says the antenna “should not look like the same surface as the body,” which version 18 now supersedes **at both connection regions**.

V is monotone along the chain (`creature_core.cpp:954-959`), so the narrow fix does not require a runtime material system: author the first and last portions of the loop atlas band as body-pigment/body-grain transitions, leaving the middle antenna character editable. Regenerate and commit `tools/reel/manafold_page.h` with the generator. A texture ladder must be rendered; row arithmetic can locate the zones but cannot choose their final widths.

### Internal ink can advertise the intersection

The exterior cel line uses the union mask and does not draw a seam inside a true union. But Direction 12/17 also renders a body-only auxiliary and calls `add_visible_body_inner_edge()` (`zhao_reel.cpp:3324-3345,3482-3507`). Where body depth owns a pixel beside nearer loop coverage, that pass deliberately restores the body boundary. Around the emergence/re-entry zones this can draw a dark separation exactly where version 18 wants visual fusion.

There is no loop-only/root-only ownership mask. A future repair should not globally remove the accepted body/head contour. The bounded options, in order, are:

1. render a body-texture root ladder first and see how much of the “black seam” was contrast;
2. if still visible, add a production-derived root exclusion to the body-inner-edge owner, scoped from posed connection geometry—not a hand-painted 2D mask;
3. stop before adding an unconditional repaint or broad contour suppression.

## Why both connection lobes warp

The swell profile and the rigid carrier core are two different station windows.

### Front

- visible swell peak: `kKnuckleAtJfMm = 320`, half-width `270` (`manafold_art.h:732,745`), so it spans arc `50..590`;
- JunctionF/Neck pivot station used by skinning: `250` (`manafold_model.h:95-97`);
- Front rigid core: only `250 ± 70 = 180..320`; Root→JunctionF transition is `90..180`, and JunctionF→Neck transition starts at `320` and ends at `410` (`manafold_model.h:210-240`).

Therefore the **swell peak sits exactly on the first ring of the outgoing JunctionF→Neck blend**, and one visual lobe spans Root, JunctionF and Neck authorities. Relative JunctionF/Neck motion deforms the visible swelling even though each individual gate correctly reports a carrier. This is a proven source reason for a lobe that looks warped while the mechanism passes.

### Rear

- visible End swell: peak `2660`, half-width `280`, span `2380..2940`;
- RearSocket rigid core: `2660 ± 120 = 2540..2780`;
- the same swelling crosses the incoming `SpanDeltaEPreSocket→RearSocket` blend and outgoing `RearSocket→ReturnTip` blend (`manafold_model.h:225-273`).

Those operands are deliberately independent: the chain carries closure/signed length, RearSocket follows the deformed body surface, and ReturnTip is a buried Root sibling (`manafold_rig.h:116-133`; `manafold_clips.h:573-663`). A long swell spanning all three can visibly shear at exactly the body crossing.

### Consequence

Do **not** start by changing closure, moving the body targets or rebuilding topology. First align root swell/core authority so the visually thick region is not split across independently moving palettes. Candidate structural wave:

- author explicit Front and End root-core/profile stations;
- keep the buried Root→Front and C→End signed attachment laws;
- make each visible root thickening stay within one carrier-owned core, with zero-slope transition outside it;
- retain same-binary controls restoring the current split-authority arrangement.

This is comparison-side diagnosis, not a radius prescription. Native render decides the final profile.

## Front flexibility gap

The rig has two co-located bones at the front pivot (`kBJunctionF` and `kBNeck`, arc 0), but the channels are asymmetric:

- `HingePlay` has `tilt_neck/yaw_neck` and A/B/C/End fields, **no JunctionF tilt/yaw** (`manafold_clips.h:313-324`);
- `loop_pose()` applies HingePlay’s extra axes to Neck, A/B/C and RearSocket, while JunctionF receives only its rest yaw/fold (`:375-418`);
- ambient fold/knead gives JunctionF in-plane Z grip/wag; Neck gets Z grip and an X wag (`:2091-2192`);
- a few public beats rotate JunctionF, but there is no general Front position/target channel and no signed body→Front length channel. Signed spans begin Front→A.

The Front visible core is JunctionF-owned. Much of the apparent front flexibility happens on Neck *above* that core, so the front ball/root itself reads stiffer than A/B/C.

The smallest structural extension is append-only Front fields in `HingePlay` and named C2 Front X/Y/Z art channels composed on JunctionF. Keep Neck independent so the body emergence and the post-Front stick do not become one rigid lever. No new bone/helper is justified by recon yet; the existing two co-located rotations are sufficient to render a flexibility ladder once the visual root core stops straddling their blend.

## Carrier-size inventory

All five carriers are swell amplitudes in `manafold_art.h`; only `make_loop()` consumes them. No separate ball topology must be rebuilt.

- Front swell add: `26 × 31 mm`, on top of the junction taper;
- A: `64 × 86 mm`;
- B: `62 × 82 mm`;
- C: `60 × 78 mm`;
- End: `50 × 62 mm`.

The ordinary stick taper in the free runs is roughly `44..58 mm` in-plane and `20..30 mm` across (`kLoopBladeRxMm/RzMm`). Thus the current A/B/C stations are visually much thicker than the sticks even before perspective/ink. The owner’s request can be tested safely by a named multiplier over only the five swell amplitudes; preserve baseline taper, station positions, half-widths, folds, signed spans and carrier cores. Render identity plus several reductions from one binary if a diagnostic multiplier is added; choose the winner by complete native motion. The acceptance floor is visual: each station remains plainly thicker than adjacent sticks, without reading as a protruding bead.

Do not shrink `kLoopBlade*` as a proxy. That would make the sticks thinner and can make the balls look **more** dominant.

## Particle-through-antenna disposition

The problem is **not a missing depth-test flag**:

- free lightning uses `lightning_push()`, which routes through `lightning_depth_test()`;
- surge motes and fold motes call `mana_push(... depth_test=true ...)` (`manafold_fx.h:2191-2194,3640-3644`);
- the renderer tests every splat pixel against creature depth (`zhao_reel.cpp:3797-3808`).

The splat uses one projected centre depth for its entire billboard. A mote whose centre is genuinely in front of a stick correctly wins depth and its disc overlaps the antenna. The baseline plate shows this repeatedly; it reads as “through” even though the depth comparison is internally consistent.

**Bounded easy experiment:** route only fold/surge mote halo+core splats to the existing **pre-creature** layer (`ManaSplat::pre=true`), behind the creature but still visible in the empty O. The renderer already has this layer; the creature will occlude dots wherever sticks/body exist, with no collision solver and no particle suppression. Keep lightning figures/endpoints unchanged. Add a strict same-binary `post|behind-creature` control and review Channel, Crackle, Hover, Taunt III, deaths and Lasso.

Stop if the behind-creature rung makes the field look pasted behind the whole animal or removes important foreground depth. A geometric nearest-segment repulsion/collision system is outside the owner’s bounded budget and is not recommended.

## Protected contracts

Any implementation must keep:

- one open-O continuous antenna skin, both ends deliberately buried;
- exact body-following RearSocket and buried ReturnTip;
- signed extension/compaction, posed-ring ordering and all 31 span controls;
- five independently attributable public carriers;
- complete lower/inside ink and foreground lightning depth;
- body shell/contour mist, eye registration, contacts and all version-17 clip seams;
- no change to unrelated creatures or hardware/RTL.

## Recommended wave order

1. **Instrumentation/baseline:** add exact root-zone profile/skin/material receipts and same-binary swell/particle controls. Do not author values yet.
2. **Structural root authority:** keep visible Front/End thickening inside one carrier-owned core; add Front multi-axis channels through existing JunctionF/Neck bones. Re-run closure, spans, public mutes and body-follow.
3. **Material/ink integration:** body-like atlas transitions at both roots; only if native A/B still shows a seam, narrowly suppress body-inner ink from posed root ownership.
4. **Art ladder:** reduce all five swell amplitudes by eye while preserving thicker-than-stick read; author Front flexibility from full motion.
5. **Bounded particle A/B:** compare current post particles with behind-creature mote layer. Ship only if the read improves without flattening the field.

No generic topology rebuild, collision system or Quartus fit is justified by this recon.
