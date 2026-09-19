# Q014 answer — v18-findings-draft

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: b95e4739855745a78bf5c9fa46dc833b
- when: 2026-09-19T18:42:44  seconds: 216  finish: stop  status: ok
- usage: {"completion_tokens": 15138, "completion_tokens_details": {"reasoning_tokens": 3644}, "prompt_tokens": 28163, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 43301}
- inputs: [{"input": "Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md:1-21", "sha256": "c4e32faf91efcc0e", "chars": 4612}, {"input": "Upheaval/creature/Manafold/PASS-17-FINDINGS.md:1-111", "sha256": "752d0c9fc3325a04", "chars": 12361}, {"input": "Upheaval/website/creatures.json:37-37", "sha256": "32f92ba41e1ed209", "chars": 2698}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ARCHIVE-CLOSURE.md:1-94", "sha256": "35cdd71e5931db54", "chars": 5153}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-AUTHORITY-IMPLEMENTATION.md:1-106", "sha256": "e8b0e9292c96ac8e", "chars": 8150}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-MATERIAL-IMPLEMENTATION.md:1-113", "sha256": "3fda0473255e7e04", "chars": 7756}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md:1-128", "sha256": "896c5514a21f93cb", "chars": 10619}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:1-131", "sha256": "bc06392311d43ba3", "chars": 17442}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:1-111", "sha256": "45415db985cf7465", "chars": 12597}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | V18-SWELL-FRONT-ART.md:3 vs V18-SWELL-FRONT-REPAIR.md:6 | ART report is explicitly superseded; its CRC table (ART:96-106) differs from REPAIR §7 (REPAIR:100) | ART line 3 "SUPERSEDED…Its selections (400 pm swells, 1500 pm Front gain) stand; its build, gate, CRC and picture receipts do not." REPAIR line 6 "Supersedes: the build, gate, CRC and picture receipts in V18-SWELL-FRONT-ART.md." |
| 2 | P2 | V18-SWELL-FRONT-ART.md:46 vs V18-SWELL-FRONT-REPAIR.md:65 | kTrickPlantRootMm final value: ART says 1690, REPAIR says 1534 | ART: "kTrickPlantRootMm moved 1706 -> 1690." REPAIR §4.3: "kTrickPlantRootMm 1706 → 1534…This goes beyond the review's expected ~1690." REPAIR is the closure document. |
| 3 | P3 | V18-SWELL-FRONT-ART.md:21 vs V18-SWELL-FRONT-REPAIR.md:104 | ART says shipping constants are "equal to the integer output of the selected 400 pm rung"; REPAIR notes 1 mm rounding on A rx, B rx/rz, End rz | REPAIR §7: "the diagnostic multiplier truncates and the shipping family was rounded, so the 400 rung differs from the selected constants by 1 mm on A rx, B rx/rz and End rz. The ART report's 'equal to the integer output of the 400 pm rung' is therefore approximate." |
| 4 | P2 | (absent from all inputs) | No v18 final-bank manifest SHA-256, total frame count, or final visual verdict exists yet | Wave E (line 111) closes at source commit `0381bdec`; no render-bank receipt, frame count, or bank-manifest hash appears in any provided report. Wave F is still in progress. |
| 5 | P3 | V18-WAVE-E-CLEANUP.md:37 | Legacy control shows "21/22 sequence CRCs equal the Wave-D bank exactly. The 22nd is Crackle." This implicitly confirms the Wave-D bank is the REPAIR bank, not the ART bank. | The ART CRCs (e.g. Hover `0x47CD2048`) differ from REPAIR §7 CRCs (e.g. Hover `0x94122BE4`), so "the Wave-D bank" in Wave E must mean the REPAIR-closure bank. |
| 6 | P3 | V18-WAVE-E-CLEANUP.md:91 | Final bank CRCs are given only as examples (6 of 22); the full `crc-final-live22-plus-crackle-legacy.txt` receipt is not reproduced in the report | "Examples: Hasty 0xC6FB59FB, Drift 0x3A11AB0C, Blown 0x840DA184, Crackle 0xCC1BDA8B, Channel 0xD49861D0, Hover 0x24B3FE60." No total frame count stated. |
| 7 | P3 | V18-ROOT-AUTHORITY-IMPLEMENTATION.md:79 | "post-review support derivation/terminal-instrument correction: current integrated output remains 2,048/2,048 byte-identical to the reviewed evidence generation" — this is a self-consistency claim about a single generation, not a cross-wave identity | No separate control binary or independent witness is named for this identity check; it is stated as a fact within the same report. |

---

## VERSION-18-FINDINGS.md (draft)

```markdown
# Manafold version 18 findings — Waves A through E (draft)

**Direction:** `OWNER-DIRECTION-19-2026-09-19.md`
**Architecture:** `V18-ARCHITECTURE.md`
**Source commit (Wave E close):** Zhaozhou `0381bdec` on `manafold-v18` [V18-WAVE-E-CLEANUP:6]
**Version-17 archive receipt:** `V17-ARCHIVE-SHA256.txt` [V18-ARCHIVE-CLOSURE:17]
**Final renderer (Wave E):** MD5 and SHA-256 in the Provenance section below
**Final bank manifest SHA-256:** `[not yet available — Wave F in progress]`

> This document covers Waves A through E only. Wave F (Flight vertical travel and the Trick 360° turn) is still in progress; its section is a clearly marked placeholder.

---

## What was actually wrong

1. **The front root and its visible stick could not flex.** JunctionF and the connection between body and Front ball behaved like a stiff peg mounted on the body; no public motion could make them visibly bend [V18-ROOT-AUTHORITY-IMPLEMENTATION:10-16].
2. **Both antenna-to-body connections read as a different material.** The front and rear root swell used the cooler/coarser antenna atlas even where the antenna emerges from and returns to the body, so the connections looked like a separate surface lobe rather than the creature's own skin [V18-ROOT-MATERIAL-IMPLEMENTATION:62].
3. **All five antenna balls still read as large protruding beads.** The version-17 swell sizes (Front 26/31 mm, A 64/86, B 62/82, C 60/78, End 50/62) made every carrier a dominant corner bead on the strap [V18-SWELL-FRONT-ART:23-29].
4. **The Mana menu and Mana lab still occupied live presentation tabs.** Both experiment collections had finished their purpose but remained in the live site, consuming tabs and freshness budget [V18-ARCHIVE-CLOSURE:31].
5. **Particles drew through antenna sticks.** Mote discs could cross a stick's front surface in a single frame, flipping visibility and looking like a glitch; no depth-aware clearance rule existed [V18-WAVE-E-CLEANUP:58-60].
6. **Crackle, Drift and Blown carried old/special mana treatments.** Crackle ran candidate 4 (lightning only) under a night backdrop; Drift and Blown carried a persistent history mist that the owner read as a relic trail [V18-WAVE-E-CLEANUP:16,44,53-54].
7. **Hasty showed a frame-history smear.** The 48×30 creature-following persistent plane (`u02_mist`) was on for every non-slot-7 clip, leaving chunky grey-violet history cells behind the animal on travelling clips [V18-WAVE-E-CLEANUP:16].
8. **Flight did not read as vertical travel.** The creature did not visibly move up and down; the motion lacked unmistakable vertical displacement [OWNER-DIRECTION-19:17]. *(Wave F — not yet addressed.)*
9. **Trick's rotation phrase was a 180° turn and back.** The owner wanted a 180° turn, pause, continue through 360°, overshoot slightly, and correct back before the existing recovery [OWNER-DIRECTION-19:19]. *(Wave F — not yet addressed.)*
10. **The version label needed to move to 18.** All plans, findings, archive labels, site copy, media provenance and publication records must use "Manafold version 18" [OWNER-DIRECTION-19:20].
11. **Art acceptance is visual, not arithmetic.** Gates protect contact, topology, continuity, ownership and regressions; the art values themselves are chosen by looking at complete native-resolution motion [OWNER-DIRECTION-19:21].

---

## What landed

### Wave A — Archive closure [V18-ARCHIVE-CLOSURE]

Before any version-18 encode could overwrite live names, every production-verified version-17 Manafold file was copied to `archive-v17-manafold-*` paths. The 28 WebMs and 28 posters totalled 70,576,645 bytes; all 56 source/archive pairs matched exact SHA-256 and byte length. The source generation was Upheaval `664f415`, Zhaozhou source `18e1d993`, renderer MD5 `67DCAFF8ABC0AA2BA830C439A4CD98C7` [V18-ARCHIVE-CLOSURE:15].

`creatures.json` now carries one archive generation labelled exactly "Version 17 + mana experiments — 2026-09-19" containing three collections: Version 17 (22 non-menu production clips), Mana menu (6 clips), and Mana lab · 2026-09-05 (10 clips). Every version-17 production clip is declared exactly once (22 + 6 = 28). Menu and lab clips no longer consume live tabs; the twelve now-undeclared live menu media files were removed, with their history preserved in byte-identical immutable copies [V18-ARCHIVE-CLOSURE:23-33].

A new `website/tools/checkarchive.py` is wired into every assemble/deploy. It verifies all 56 archive files against the locked digest receipt, checks live source/archive pairs, enforces exactly 22 + 6 + 10 declarations, forbids duplicate media paths, requires Mana menu and Mana lab to be absent from live tabs, and mandates archived playback flags (controls + loop + muted + playsinline, no autoplay, `preload="none"`). Its selftest passes a valid fixture and fires three red legs. `checkfresh.py` now classifies rendered sources using live/archive manifest ownership and explicitly exempts version-17 subjects locked by `archive-v17-*` declarations [V18-ARCHIVE-CLOSURE:37-48].

The complete local gate ran with no skip flags: 716 render entries, 22 live Manafold outer tabs, 13 archive generations, 56/56 locked archive files at 70,576,645 bytes, 22 + 6 + 10 unique declarations, 22 live fresh / 6 archived / 0 stale / 0 absent / 0 unknown, 1,420 declared / 1,420 decoded media, overall RC 0, deployment none (assemble-only) [V18-ARCHIVE-CLOSURE:54-63].

### Wave B — Root rotation authority [V18-ROOT-AUTHORITY-IMPLEMENTATION]

The ratified first draft proposed making the complete nominal Front/End swell half-widths rigid JunctionF/RearSocket cores. The gate arithmetic disproved that before the change shipped: nominal root half-widths of 270/280 mm would have reduced the F–A/C–End translation runs to 270/680 mm, leaving legal compaction at 53/−27 mm against the unchanged 80 mm floor and inverting C–End. No compaction bound, floor, closure tolerance or profile value moved [V18-ROOT-AUTHORITY-IMPLEMENTATION:10-16].

The corrected mechanism derives sampled support from the exact production predicate, gives every non-terminal supported ring one semantic rotation while keeping the exact version-17 translation gradient, and treats terminal rear ring 63 as the explicit buried ReturnTip-only exception. Manafold now uses 27/32 content bones; the four append-only helpers are not visible joints [V18-ROOT-AUTHORITY-IMPLEMENTATION:20-24].

The integrated skin preserves the original F–A/C–End gradient starts, helper fractions and full endpoints; legal remaining runs of 253/128/117/133 mm at the four compaction floors; Root/JunctionF burial, body-following RearSocket, ReturnTip burial and straight closure; and all seven version-17 signed helper IDs [V18-ROOT-AUTHORITY-IMPLEMENTATION:28-32].

`HingePlay` appends `tilt_front/yaw_front` (default zero). A deterministic mechanism stimulus proves both new axes move JunctionF while Neck/A/B/C/RearSocket local channels remain untouched. No shipping clip owns nonzero Front X/Y art yet in Wave B, so no dishonest presentation control was shipped; Wave D adds that control after real public curves exist [V18-ROOT-AUTHORITY-IMPLEMENTATION:38-42].

The root gate (`mspan` category `kCatRootAuthority`) verifies 117/117 Front/End non-terminal swell support vertices with 0/0 wrong rotation palettes, 10 ReturnTip-only terminal vertices over 97,000 posed samples (worst ellipsoid rho 1090.13 pm against the unchanged 1120 pm gate), 339,500 posed ring steps with zero reversal/pinch, and 9,700 helper/carrier rotation samples with zero failures [V18-ROOT-AUTHORITY-IMPLEMENTATION:50-56].

The exact `--fail-root-authority` control restores legacy palettes, produces 81/72 wrong Front/End support vertices, returns RC 1 with only category `0x800`, and leaves all unrelated categories green [V18-ROOT-AUTHORITY-IMPLEMENTATION:58].

Legacy-split control: 2,048/2,048 frames byte-identical to accepted version 17 across Hover, Rest, Taunt, Taunt III and Trick. Unrelated Zixxtrixx Idle: 576/576 frames byte-identical [V18-ROOT-AUTHORITY-IMPLEMENTATION:77-79].

Every frame of the five root-heavy clips was reviewed at reduced full-sheet scale with native/exact-4× root A/B witnesses. The new staged support introduces no buckle, pinch, socket/tip exposure, contact loss, outline closure or gross new root kink. This is not final root likeness evidence; both roots still use the version-17 cooler/coarser antenna atlas and the same swell sizes [V18-ROOT-AUTHORITY-IMPLEMENTATION:83-85].

### Wave C — Root material: body-smooth connections [V18-ROOT-MATERIAL-IMPLEMENTATION]

The sole author of the committed `manafold_page.h` is `tools/pack/mkmanafoldpage.py`. The generator now has named shipping controls: `ROOT_MATERIAL_MODE = "bodyblend"`, `ROOT_FRONT_BLEND_ROWS = 16`, `ROOT_REAR_BLEND_ROWS = 16` [V18-ROOT-MATERIAL-IMPLEMENTATION:15-19].

In `bodyblend` mode, a second deterministic field uses the body pigments, body grain amplitude, body stroke amplitude/frequency and body stroke axis. Quintic C2 row weights make that field authoritative at the front/rear atlas edges and hand continuously to the legacy loop field toward the free middle. Only the loop band changes; body, hinge, eye and star generators remain the existing paths [V18-ROOT-MATERIAL-IMPLEMENTATION:29].

A safe deterministic ladder rendered four candidates from isolated binaries: version-17 legacy, narrow (8/8), medium (16/16), and broad (24/24). Each candidate rendered the same five complete root-heavy clips (Hover 600, Rest 400, Taunt 280, Taunt III 368, Trick 400 — 2,048 frames per candidate, 8,192 reviewed presentation frames) plus native and enlarged multi-angle root plates [V18-ROOT-MATERIAL-IMPLEMENTATION:49-58].

**The 16/16 medium candidate was selected by eye from the complete native motion.** The atlas and support arithmetic only explains why the result is coherent; it did not choose the value [V18-ROOT-MATERIAL-IMPLEMENTATION:67]. The reading:

- legacy retains the proven darker/coarser strip through each body crossing; the root still reads as a separately surfaced lobe;
- narrow (8/8) softens the exact edge but hands back to the coarse loop field inside the long connection, leaving a visible material change through the root support;
- **medium (16/16, selected):** both connections carry the body's pink/grain treatment across the visible root thickening and hand gradually to the antenna field outside it. The base reads as one body surface while A/B/C and the free middle keep the cooler/coarser antenna character;
- broad (24/24) also fuses the roots but carries body treatment far into the free chain and weakens the antenna's distinct middle surface around the outer carriers [V18-ROOT-MATERIAL-IMPLEMENTATION:62-66].

A separate diagnostic rebuilt the selected page with the complete body-inner owner omitted. The native/exact-4× comparison did not expose a clear root seam that justified changing production ink ownership. Wave C makes no change to `add_visible_body_inner_edge()`, its outline gate or render ordering [V18-ROOT-MATERIAL-IMPLEMENTATION:73-75].

The legacy material candidate contains 98,302/98,302 exact version-17 page words. The selected header regenerates byte-identically [V18-ROOT-MATERIAL-IMPLEMENTATION:46]. All gates green; unrelated Zixxtrixx Idle 576/576 frame-identical [V18-ROOT-MATERIAL-IMPLEMENTATION:87-97].

### Wave D — Smaller swells and public Front flex [V18-SWELL-FRONT-REPAIR, supersedes V18-SWELL-FRONT-ART]

> **Supersession note.** `V18-SWELL-FRONT-ART.md` is explicitly marked superseded by `V18-SWELL-FRONT-REPAIR.md` [V18-SWELL-FRONT-ART:3, V18-SWELL-FRONT-REPAIR:6]. The ART report's selections (400 pm swells, 1500 pm Front gain) stand; its build, gate, CRC and picture receipts do not. All Wave-D numbers below are from the REPAIR closure document unless noted.

**Smaller swells.** The existing strict `ZHAO_U02_SWELL_PM` diagnostic rendered complete Hover at 1000/850/700/550/400/250 pm and complete Taunt III at 1000/700/550/400 pm. The 400 pm family was **selected by eye**: 1000 retains the version-17 protruding-bead read; 700 and 550 improve it but the free A/B/C stations still dominate; 400 keeps all five stations plainly thicker than adjacent runs while making them read as local thickening in one continuous antenna; 250 approaches a nearly uniform strap and was rejected [V18-SWELL-FRONT-ART:12-19, V18-SWELL-FRONT-REPAIR:108].

Shipping keeps independent named values (the selected constants, not the diagnostic rung — see Provenance note):

| Carrier | Selected rx/rz mm | Version-17 control rx/rz mm |
|---|---:|---:|
| Front | 10 / 12 | 26 / 31 |
| A | 26 / 34 | 64 / 86 |
| B | 25 / 33 | 62 / 82 |
| C | 24 / 31 | 60 / 78 |
| End | 20 / 25 | 50 / 62 |

[V18-SWELL-FRONT-ART:23-29]. `ZHAO_U02_SWELL_MODE=selected|legacy` is strict. The 400-pm diagnostic rung differs from the shipping constants by 1 mm on A rx, B rx/rz and End rz due to truncation; there is no visual consequence [V18-SWELL-FRONT-REPAIR:104].

**Public Front/JunctionF art.** `HingePlay::tilt_front/yaw_front` carries real, clip-specific C2 performances for ten named clips (Hover, fixed idle, Rest, Drift, Blown, Hasty, Flight, Taunt, Taunt III, Trick approach/righting). The selected global gain is the named 1500 pm owner knob. Front acts on JunctionF after the existing fold-Z authority; Neck, A/B/C/End and signed length remain independent [V18-SWELL-FRONT-ART:35-44]. Ten named public clips clear the unchanged 20 mm visible-core floor; weakest maximum response is Rest at 24.37 mm [V18-SWELL-FRONT-ART:48, V18-SWELL-FRONT-REPAIR:94].

**Trick contact correction.** The smaller selected swell reduced actual Trick penetration from the declared −25 mm. `kTrickPlantRootMm` was set to 1534 (beyond the ART report's intermediate 1690) as a declared Wave-D animation change: for keys [78,148], `build_trick` offsets the root so the carrier-B support centre captured at key 78 stays put while the body balances around it. The probe independently certifies the actual B-swell surface [V18-SWELL-FRONT-REPAIR:63-65]. `kTrickLiftKey` moved 156 → 148: keys 150–155 are genuinely airborne (0.1–0.9 m clear), and restoring 156 would require contact from a body nearly a metre up [V18-SWELL-FRONT-REPAIR:50-52].

**Terminal cap repair.** The pre-repair tree set the final taper key to 0/0, which welded the eight ring vertices into one position and produced 16 degenerate (zero-area) triangles caught by `manafold-meshcheck` RC 1. The repair sets `kReturnTipCapRxMm/RzMm = 2/2` mm, a vanishingly small real ring. Result: CLEAN, 1416 position groups, 4200 edges, identical to Wave C's topology. Terminal burial worst rho 1082.80 pm (gate 1120) [V18-SWELL-FRONT-REPAIR:21-25].

**Presentation fix.** The pre-repair review sheets were rendered with bare `zhao-reel.exe` and no environment, producing no contour ink. All twelve sheets were regenerated from `zhao-reel-cel.exe` with `ZIXX_EXP=celmain` and `ZIXX_LIGHT=diagonal-cool-cross` [V18-SWELL-FRONT-REPAIR:29].

**Gate matrix: 88/88** [V18-SWELL-FRONT-REPAIR:88-96]. Normals 10/10 RC 0. mspan 35/35 RC 1 (including front-flex `0x1000`, swell-size `0x2000`, terminal-cap `0x4800`). msmooth 16/16 RC 1. Protected 27/27. Selectors 9/9 RC 2. Zixxtrixx Idle 576/576 frame-exact [V18-SWELL-FRONT-REPAIR:90-96].

**Visual verdict (looked at).** The 400 selection stands: carriers are a touch thicker than sticks within one continuous strap. Selected vs legacy: the corner beads and top ball are gone; no waist, pinch or seam at either root. Front-flex: the Front angle visibly changes on both axes (strongest in Taunt and Taunt III); root stays fused with body-style material; no kink or shear. Trick support: planted headstand on the dirt, smooth landing, clean spring-off. Production-ink composite: continuous contour around body and antenna, no outline hole [V18-SWELL-FRONT-REPAIR:108-113].

### Wave E — Live presentation cleanup [V18-WAVE-E-CLEANUP]

**History mist retired from every live subject.** `subject_u02_clip` had given every non-slot-7 clip `s.u02_mist = true`, the 48×30 creature-following persistent plane. On travelling clips it left chunky grey-violet history cells behind the animal. That plane was the Hasty smear and the Drift/Blown relic trail. Baseline vs Wave E at the most-changed frames: every "before" frame has the blocky trail, every "after" frame is clean, with creature, ink contour, fold, lightning and motes intact [V18-WAVE-E-CLEANUP:16].

The new builder default is mist off for all 22 live subjects. Archived v17 mana menu (6) and mana lab (10) clips keep `u02_mist = true` explicitly at the call site. `manafold-crackle-legacy` is an explicit mist-on control [V18-WAVE-E-CLEANUP:20-27].

`ZHAO_U02_LIVE_MIST=off|legacy` is strict. `legacy` restores the v17 builder default. `kU02LiveSiteSubjects[22]` sits beside a `static_assert(22)`. The renderer gains executed-block receipts (`u02_mist_frames_run`, `u02_smear_frames_run`) incremented inside the plane blocks; after rendering, `render_scene` compares declared flags and receipts on every live subject and returns RC 5 on any history. The committed `manafold_live_history_gate.py` checks the renderer table against `creatures.json` and renders all 22 in the production invocation [V18-WAVE-E-CLEANUP:29-33].

Normal: 22/22 declared 0/0 executed 0/0 OK. Legacy control: RC 5, 22/22 FAIL with executed mist frames > 0 (e.g. Hover 600, Death Drop 227); 21/22 sequence CRCs equal the Wave-D bank exactly; the 22nd is Crackle (the intended mana change) [V18-WAVE-E-CLEANUP:36-37]. Non-live identity: 13/13 non-live subjects byte-identical [V18-WAVE-E-CLEANUP:39]. `fogprobe-mana` (smear and mist off) is now byte-identical to live Rest [V18-WAVE-E-CLEANUP:40].

**Crackle: normal mana.** Crackle was the only live subject still on old mana: candidate 4 (lightning only) under a night backdrop. It now takes the builder's candidate 9 with no `u02_backdrop` call, sitting under the day sky. `manafold-crackle-legacy` is the verbatim old block and is not live; its CRC is `0xEDDC80D7` = the Wave-D `manafold-crackle` (600 frames) [V18-WAVE-E-CLEANUP:44-46]. The pictures agree: ship 9/day. 600 continuous frames, figures present in every one, wrap matches [V18-WAVE-E-CLEANUP:48-49].

**Drift and Blown.** After history removal, Drift is a continuous right-to-left traverse on candidate 9 with no trail; the relic read is gone. Blown is a continuous phrase of launch, inverted tumble, drop, catch and settle with f291 matching f0; the mana rides the antenna with no trail. Both keep candidate 9 [V18-WAVE-E-CLEANUP:53-54]. Pre-existing open item: Drift f260–298 sit at the extreme left edge with the antenna and mana partly cropped [V18-WAVE-E-CLEANUP:53].

**Particles through the antenna: bounded attempt, shipped small.** The pre-layer route (plan's A/B) was **declined by construction**: pre splats draw while the depth buffer holds only terrain, then the creature composites over them, so every mote inside the silhouette is hidden including motes genuinely in front. That flattens depth, the plan's own stop condition [V18-WAVE-E-CLEANUP:58].

Instead, a narrow candidate uses existing depth. For fold and surge mote bodies only, visibility ramps to zero with a smoothstep as the centre's view depth approaches the antenna surface within 120 mm (`kMoteSurfaceFadeMm = 120`), averaged over a 5×5 disc footprint (`kMoteSurfaceFadeTaps` = 5×5 = 25 depth reads per mote splat) [V18-WAVE-E-CLEANUP:60-67]. The first single-pixel cut was rejected by reasoning (it would drop a mote in one frame as it slides sideways onto a stick at matching depth) [V18-WAVE-E-CLEANUP:62].

**120 mm was chosen by eye.** A 240 mm rung began fading motes genuinely in front, so 120 mm is the shipped value. The change is a light-touch cleanup with the cloud preserved [V18-WAVE-E-CLEANUP:73-74]. `ZHAO_U02_MOTE_SURFACE_FADE=off|on` is strict. Exact-off: the off binary is byte-identical on 23/23 live+legacy subjects and 13/13 non-live subjects [V18-WAVE-E-CLEANUP:65].

**Gate matrix: 97/97** [V18-WAVE-E-CLEANUP:76-84]. Normals 11/11 RC 0. mspan 35/35 RC 1 (identical attribution masks to Wave D). msmooth 16/16 RC 1. Protected legs 27/27. New Wave E rows 8/8 (live-history normal, legacy control, list-drift control, crackle-legacy identity, fade exact-off, fade fires, non-live identity, 6/6 malformed selectors RC 2) [V18-WAVE-E-CLEANUP:80-84].

### Wave F — Flight vertical travel and Trick 360° turn

> **PLACEHOLDER — Wave F is still in progress.**
>
> Two Direction-19 items remain:
>
> 7. **Flight** must be re-authored so the creature visibly moves up and down as flight, preserving framing, loop continuity, antenna/effect smoothness and the no-smear contract [OWNER-DIRECTION-19:17].
> 9. **Trick** must perform the existing 180° turn, pause, continue through a 360° turn, overshoot slightly and correct back before the existing recovery, remaining continuous, planted and readable with no camera chase or contact-gate weakening [OWNER-DIRECTION-19:19].
>
> Carried open items from Waves D and E that Wave F should address: the Trick planted crown sits at the bottom of frame [V18-SWELL-FRONT-REPAIR:117]; Drift f260–298 at the extreme left edge with antenna and mana partly cropped [V18-WAVE-E-CLEANUP:103]; the End-swell stub visible at some angles (Rest 342, Taunt III 328, Trick 393) [V18-SWELL-FRONT-REPAIR:118].
>
> *[Findings, gate receipts, visual verdict and provenance to be filled when Wave F closes.]*

---

## Direction-19 item status

| # | Owner item | Status | Where | Notes |
|---|---|---|---|---|
| 1 | Front root flexibility reopened | **Done** | Wave B [V18-ROOT-AUTHORITY-IMPLEMENTATION], Wave D [V18-SWELL-FRONT-REPAIR] | Root rotation authority integrated; real public Front X/Y curves shipped for ten clips at 1500 pm gain. Weakest response Rest 24.37 mm, floor 20 mm. |
| 2 | Both body connections body-smooth | **Done** | Wave C [V18-ROOT-MATERIAL-IMPLEMENTATION] | 16/16 bodyblend rows selected by eye. Both connections carry body pigment/grain; free middle keeps antenna character. No root-ink exclusion needed. |
| 3 | All antenna balls smaller | **Done** | Wave D [V18-SWELL-FRONT-ART:23-29, V18-SWELL-FRONT-REPAIR:108] | 400 pm family selected by eye. Carriers are local thickening in one continuous antenna, no longer dominant beads. |
| 4 | Archive both experiment collections | **Done** | Wave A [V18-ARCHIVE-CLOSURE] | Mana menu (6) and Mana lab (10) moved to one archive generation. Live tabs freed. 56/56 files locked, 70,576,645 bytes. `checkarchive.py` permanent gate. |
| 5 | Particle/antenna bounded attempt | **Done** | Wave E [V18-WAVE-E-CLEANUP:56-74] | Pre-layer route declined (flattens depth). Mote surface fade shipped: 120 mm smoothstep over 5×5 footprint, fold and surge bodies only. Chosen by eye. |
| 6 | Normalize relic mana | **Done** | Wave E [V18-WAVE-E-CLEANUP:42-54] | Crackle → candidate 9 / day sky (was candidate 4 / night). Drift and Blown kept candidate 9; no new fault exposed. Legacy controls preserved. |
| 7 | Flight unmistakable vertical travel | **Pending – Wave F** | *[placeholder above]* | Not yet addressed. |
| 8 | Hasty no frame-history smear | **Done** | Wave E [V18-WAVE-E-CLEANUP:14-40] | The 48×30 persistent plane was the smear. Mist retired from all 22 live subjects. Renderer assertion + committed gate with two fired controls enforce it. |
| 9 | Trick 180° → pause → 360° → overshoot → correct | **Pending – Wave F** | *[placeholder above]* | Current Trick retains its accepted 180° headstand. The 360° phrase is not yet authored. |
| 10 | Version 18 labeling | **Done** | Wave A [V18-ARCHIVE-CLOSURE:23] and throughout | Archive generation, findings, gates and site copy all use "version 18." |
| 11 | Art acceptance remains visual | **Done** (principle) | Stated in every wave | Wave C material, Wave D swell/Front gain, Wave E fade distance all chosen from complete native motion by eye. Gates protect structure; eyes decide art. |

---

## Failable evidence

- **Wave A:** 56/56 archive SHA-256 + byte-length matches; 22 + 6 + 10 unique declarations; `checkarchive.py` selftest (valid fixture + 3 red legs); `checkfresh.py` selftest (unmanifested subject); assemble gate RC 0, 1,420/1,420 decode [V18-ARCHIVE-CLOSURE:13-15, 46-48, 54-63].
- **Wave B:** `mspan` 32/32 attributed controls RC 1 (31 inherited + root authority); `msmooth` 16/16 RC 1; 9 malformed selector cases RC 2; legacy-split 2,048/2,048 byte-identical to v17; Zixxtrixx Idle 576/576 [V18-ROOT-AUTHORITY-IMPLEMENTATION:72-79].
- **Wave C:** deterministic regeneration byte-exact; legacy 98,302/98,302 page words; generator invalid matrix 4/4 RC 2; all gates green; Zixxtrixx Idle 576/576 [V18-ROOT-MATERIAL-IMPLEMENTATION:87-97].
- **Wave D (closure):** 88/88 gate matrix. Normals 10/10 RC 0; mspan 35/35 RC 1; msmooth 16/16 RC 1; protected 27/27; selectors 9/9 RC 2; Zixxtrixx Idle 576/576 [V18-SWELL-FRONT-REPAIR:88-96].
- **Wave E:** 97/97 gate matrix. Normals 11/11 RC 0; mspan 35/35 RC 1; msmooth 16/16 RC 1; protected 27/27; new Wave E 8/8; 6/6 malformed selectors RC 2; legacy control RC 5 with 22/22 FAIL; non-live 13/13 byte-identical; `fogprobe-mana` byte-identical to live Rest [V18-WAVE-E-CLEANUP:36-40, 80-84].

---

## Correction loops caught by looking

Automated green was not treated as likeness proof:

1. **Wave B:** the ratified first-draft rigid-core proposal was disproved by the gate's own arithmetic before any change shipped; the corrected mechanism was derived from the production predicate [V18-ROOT-AUTHORITY-IMPLEMENTATION:10-16].
2. **Wave C:** the root-ink exclusion was built, rendered and compared; the native/exact-4× picture showed no root seam worth changing ink ownership for, so the diagnostic was retired to `.tmp` [V18-ROOT-MATERIAL-IMPLEMENTATION:73-75].
3. **Wave D (pre-review):** the independent review BLOCKED the first tree on three blockers (terminal taper, Trick support ownership, duplicate Front API). All three were repaired in source [V18-SWELL-FRONT-REPAIR:11-15].
4. **Wave D (closure):** `manafold-meshcheck` caught 16 degenerate triangles that the previous checkpoint had reported green; the 0/0 terminal cap was replaced with a 2/2 mm real ring [V18-SWELL-FRONT-REPAIR:21-23].
5. **Wave D (closure):** the pre-repair review sheets had been rendered in the wrong presentation (no contour ink); all twelve were regenerated from the production cel binary with the correct environment [V18-SWELL-FRONT-REPAIR:27-29].
6. **Wave D (closure):** the coordinator's two audits found four `mprobe` ownership bugs (tie rule, post-deform membership, weight-blind bone test, combined support/depth control); all fixed, with the depth control split into a separate `--fail-trick-support-depth` [V18-SWELL-FRONT-REPAIR:34-38].
7. **Wave E:** the new mote fade had silently reached the crackle-legacy control's surge motes; the legacy block was patched to switch the fade off around its own render, restoring exact identity [V18-WAVE-E-CLEANUP:47].
8. **Wave E:** the first single-pixel particle fade was rejected by reasoning before rendering — it would drop a mote in one frame as it slid sideways onto a stick at matching depth. The 5×5 footprint average replaced it [V18-WAVE-E-CLEANUP:62].

In every case the art values (material rows, swell sizes, Front gain, fade distance) were still selected from complete native-resolution motion by looking.

---

## Provenance

Compact hash and identity record. No hashes are repeated in the prose above.

**Version-17 archive (immutable):**
- Zhaozhou source: `18e1d993`
- Renderer MD5: `67DCAFF8ABC0AA2BA830C439A4CD98C7`
- Archive total: 70,576,645 bytes, 56 files
- Receipt: `V17-ARCHIVE-SHA256.txt`
[V18-ARCHIVE-CLOSURE:15,17]

**Wave E source commit:** Zhaozhou `0381bdec` on `manafold-v18` [V18-WAVE-E-CLEANUP:6]

**Final production renderer (Wave E close):**
- `zhao-reel-cel.exe` MD5 `ae7f03a5c32357d891e7915d758d316f`
- `zhao-reel-cel.exe` SHA-256 `4b789bf1fb2a0fb92b688f11596b3f71b70fe60c944c084e5a37a17a22ecd8b8`
[V18-WAVE-E-CLEANUP:88-89]

**Wave D closure renderer (superseded by Wave E):**
- `zhao-reel-cel.exe` MD5 `0FA8BAE0BF6A738C485FAD2DCB77A64B`
- SHA-256 `2a1654207e2e09dee9a27dec98e6be1bc62d4dd28594ec434705c0f6b42cda30`
[V18-SWELL-FRONT-REPAIR:75-76]

**Gate binaries (Wave E):** listed in `V18-WAVE-E-RECEIPTS/binaries.txt` [V18-WAVE-E-CLEANUP:89]

**Swell shipping constants (selected, not the 400-pm diagnostic rung):**
Front 10/12, A 26/34, B 25/33, C 24/31, End 20/25 (rx/rz mm). The 400-pm rung differs by 1 mm on A rx, B rx/rz, End rz due to integer truncation [V18-SWELL-FRONT-ART:23-29, V18-SWELL-FRONT-REPAIR:104].

**Final bank manifest SHA-256:** `[not yet available — Wave F in progress]`
**Total frame count / final visual verdict:** `[not yet available]`

---

## Delivery status

Waves A through E are closed at source commit `0381bdec` [V18-WAVE-E-CLEANUP:6]. The version-17 archive is immutable and locked [V18-ARCHIVE-CLOSURE:6]. The 22 live subjects have their history mist removed, Crackle carries normal mana, and the mote surface fade is shipped at 120 mm [V18-WAVE-E-CLEANUP:11-12]. No production deployment has been made for version 18; Wave A ran assemble-only [V18-ARCHIVE-CLOSURE:64].

**Wave F (Flight + Trick 360°) is in progress.** The final bank, total frame count, bank-manifest SHA-256, final visual verdict and production deployment will be recorded when Wave F closes.

---

*Draft prepared from Waves A–E reports only. All art values were chosen by eye from complete native-resolution motion; the reports say so where they do. All structural, gate and identity numbers are quoted verbatim from the named source reports. Where the reports disagree (superseded ART vs closure REPAIR; diagnostic rung vs shipping constants), the discrepancy is noted rather than resolved by picking one.*
```

---

## Draft site blurb

```
MANAFOLD, version 18 — the antenna is the body, the body is the antenna.

THE CONNECTIONS ARE SKIN. The front and rear roots carry the body's own pink and grain across the visible thickening, then hand gradually to the cooler antenna surface. No separate lobe, no different texture, no seam. The free middle keeps its distinct character.

THE BALLS ARE THICKENINGS, NOT BEADS. All five carriers — Front, A, B, C and End — are still plainly thicker than their sticks, but they read as local bulges in one continuous strap. The old corner-bead read is gone.

THE FRONT ROOT FLEXES. A real public X/Y performance drives JunctionF on ten clips, so the antenna's front connection visibly bends with the body instead of sitting on it like a peg. The rest of the rig stays independent.

NO MORE GHOST TRAIL. The chunky grey-violet history plane that followed Hasty, Drift and Blown is retired from every live subject. A renderer assertion and a committed gate with two fired controls now make it impossible to ship.

CRACKLE IS ON THE DAY SKY. The old candidate-4 lightning-under-night relic is replaced by the bank's normal fold and lightning figures with a mote cloud, same as Channel and every other live subject. Drift and Blown keep their mana, now clean.

PARTICLES RESPECT THE STICKS. A narrow depth-aware fade dims motes as their centres approach an antenna surface within 120 mm, averaged over a small footprint so no single frame pops. Motes genuinely in front still overlap. The pre-layer route was declined because it flattens depth.

THE ARCHIVE IS LOCKED. The Mana menu and Mana lab are immutable, byte-identical copies behind a permanent SHA-256 gate. Thirteen generations, zero live tabs consumed.

PROVENANCE. [final bank provenance — fill after the final render]
```

---

## CONTINUATION

- **Done:**
  - All five Waves A–E mapped to the eleven Direction-19 items with status (8 done, 2 pending-Wave-F, 1 principle). [direction table above]
  - Wave A archive: 56/56 files, 70,576,645 bytes, `checkarchive.py` permanent gate, 13 archive generations. [V18-ARCHIVE-CLOSURE:13-15,31,57]
  - Wave B root authority: 27/32 bones, 4 append-only helpers, 117/117 swell support, 339,500 ring steps, legacy-split 2,048/2,048 byte-identical. [V18-ROOT-AUTHORITY-IMPLEMENTATION:20,50-56,77]
  - Wave C material: 16/16 bodyblend rows selected by eye, 98,302/98,302 legacy page words, no ink-ownership change. [V18-ROOT-MATERIAL-IMPLEMENTATION:43,46,75]
  - Wave D (closure REPAIR, not ART): 400 pm swell family (Front 10/12 … End 20/25), 1500 pm Front gain, kTrickPlantRootMm 1534, kReturnTipCap 2/2 mm, 88/88 gate matrix. [V18-SWELL-FRONT-REPAIR:94-96,108]
  - Wave E: mist off 22/22, Crackle candidate 9/day, Drift/Blown kept, mote surface fade 120 mm / 5×5, 97/97 gate matrix, renderer MD5 `ae7f03a5…`. [V18-WAVE-E-CLEANUP:11-12,36-37,64,88-89]
  - FINDINGS table lists 7 items: 1 supersession (ART→REPAIR), 1 value conflict (kTrickPlantRootMm 1690 vs 1534), 1 rounding note (400-pm rung vs shipping constants), 1 missing final-bank manifest, 1 implicit CRC-bank identification, 1 partial CRC receipt, 1 self-consistency claim without independent witness.

- **Open:**
  - No v18 final-bank manifest SHA-256, total frame count, or "PASS N/N" visual verdict exists yet. These arrive with Wave F.
  - No production deployment URL or byte count for v18.
  - The 16 remaining final-bank CRCs (only 6 examples given in Wave E) are in `V18-WAVE-E-RECEIPTS/crc-final-live22-plus-crackle-legacy.txt`, not reproduced in the report.
  - Wave F will re-author Flight (vertical travel) and Trick (360° phrase). Carried open items: Trick crown framing, Drift left-edge crop, End-swell stub.

- **Next chunk:** Draft the Wave F findings section (Flight vertical travel + Trick 360° turn), fill the placeholder, update the Direction-19 status table (items 7 and 9 → done), produce the final bank provenance block (manifest SHA-256, frame count, visual verdict), finalize the site blurb PROVENANCE paragraph, and run a second-pass consistency check on the full VERSION-18-FINDINGS.md against all wave reports. **Inputs needed:** the Wave F closure report(s), the final bank render receipt (frame count, bank-manifest SHA-256, per-subject CRCs), the final production-deploy verification receipt, and the updated `creatures.json` blurb line.

## Coordinator verdict

**rejected** — superseded unread - ran at medium effort before the owner's xhigh correction; rerun as Q016 at xhigh
