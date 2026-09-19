# Q019 answer — v18-findings-final

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 187bd83e84b340429a65acfadd8719ba
- when: 2026-09-19T19:49:09  seconds: 622  finish: stop  status: ok
- usage: {"completion_tokens": 44636, "completion_tokens_details": {"reasoning_tokens": 30163}, "prompt_tokens": 26364, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 71000}
- inputs: [{"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/qwen/Q016-v18-findings-draft-xhigh.answer.md:1-207", "sha256": "cababf67fd21ea70", "chars": 31040}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-F-PERFORMANCES.md:1-123", "sha256": "ed79bff903ce5622", "chars": 13069}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-FINAL-BANK-INTEGRITY.md:1-165", "sha256": "7db7adc2147a87fd", "chars": 15760}, {"input": "Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md:1-21", "sha256": "c4e32faf91efcc0e", "chars": 4612}, {"input": "Upheaval/website/creatures.json:37-37", "sha256": "32f92ba41e1ed209", "chars": 2698}]
- kind: task  continues Q016

## Answer

## FINDINGS

No P1 found in the material shown.

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-F-PERFORMANCES.md:79`; `.../V18-FINAL-BANK-INTEGRITY.md:36,76` | Q3's worst Trick step is 178 mm/key in the Wave F report but 172.7 mm in the final report; doc states both with their configuration. | WAVE-F:79 "178 mm/key at peak turn speed"; FINAL:36 "Q3's worst Trick step is 172.7 mm" (key 111). |
| 2 | P2 | `.../V18-FINAL-BANK-INTEGRITY.md:129,136` | Every-frame review was at ≤1600 px sheet scale; fine detail (root seams, ink, particle crossings) not eye-judged; Hasty no-smear rests on the gate, not the eye. | FINAL:129 "I cannot judge fine detail"; FINAL:136 "too small here to confirm by eye". |
| 3 | P2 | `.../V18-FINAL-BANK-INTEGRITY.md:78` | Declared residual: a Trick-only teleport of 135–240 mm passes Q3; only guard is the report's coverage argument (Q6b/c/d inside window, authored Y+C2 outside). | FINAL:78 "a Trick-only teleport between 135 and 240 mm would pass Q3 … Acceptable". |
| 4 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:103` | Wave F renderer SHA-256 is shown truncated; only its MD5 is complete. | "SHA-256 `6aa39a2a…ed27`". |
| 5 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:83`; `.../V18-FINAL-BANK-INTEGRITY.md:12,93` | Bank scopes differ: Wave F's 33/36 covered 22 live + Crackle-legacy + 13 non-live; the acceptance bank is 22 live only. | WAVE-F:83; FINAL:12 "22 subjects, 7,992 frames". |
| 6 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:66`; `.../V18-FINAL-BANK-INTEGRITY.md:52` | Two closure matrices exist: 115/115 (Wave F) and 113/113 (integrated, post-pin); final-bank closure cited is 113/113. | Both lines. |
| 7 | P3 | `.../V18-FINAL-BANK-INTEGRITY.md:124` | Key 78's frame differs from the legacy render with zero pin offset; probable cause not traced. | "probable cause is a neighbour-reading post-pass such as `finalize_rear_follow`. This was not traced." |
| 8 | P3 | `.../V18-FINAL-BANK-INTEGRITY.md:5,165` | Status is READY-TO-ENCODE only; encode packet and any production deployment receipt are not shown. | FINAL:165 "The next packet is to encode exactly these 22 subjects". |
| 9 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:64,111`; `.../V18-FINAL-BANK-INTEGRITY.md:160` | Drift still has no framing margin at either end with a constant aim (owner call). | WAVE-F:64 "no constant aim buys margin at both ends". |
| 10 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:112`; `.../V18-FINAL-BANK-INTEGRITY.md:161` | End-swell stub unchanged since Wave D; remains open. | Both. |
| 11 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:113`; `.../V18-FINAL-BANK-INTEGRITY.md:162` | Flight underside reads darker at troughs; pre-existing v17 shading, out of scope. | Both. |
| 12 | P3 | `.../V18-FINAL-BANK-INTEGRITY.md:82,163` | TRAVEL float row printed but not gated inside Trick's exempt window (0 mm today); later sloped ground would need gating. | FINAL:82. |
| 13 | P3 | `.../V18-FINAL-BANK-INTEGRITY.md:159`; `.../V18-WAVE-F-PERFORMANCES.md:94` | Plant pin ships with toggle kept; owner can still restore the old ~178 mm slide via `ZHAO_U02_TRICK_PLANT_PIN=legacy`. | FINAL:159. |
| 14 | P3 | `.../V18-WAVE-E-CLEANUP.md:104`; `.../V18-FINAL-BANK-INTEGRITY.md:129` | 120 mm mote-surface fade still not traced by the effect gate; final sheet review cannot judge particle crossings at scale. | Both. |
| 15 | P3 | `Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md:18`; `.../V18-WAVE-E-CLEANUP.md:30-32,37` | "Remove everywhere" scoped to the 22 live subjects; legacy control + archive keep the old effect by design — verify owner wording says live subjects. | DIRECTION:18 "everywhere it survives"; Wave E legacy control fires on every live subject. |
| 16 | P3 | `.../V18-WAVE-F-PERFORMANCES.md:103`; `.../V18-FINAL-BANK-INTEGRITY.md:9` | Two production renderer MD5s (Wave F `e17c…`, final `0f08…`); expected — final build adds the plant pin (`db2bcf0e`); final provenance uses the final one. | Both. |
| 17 | P3 | `.../V18-SWELL-FRONT-ART.md:3,8` | Superseded Wave-D report still prints PASS; per coordinator correction all Wave D receipts in the final findings cite the repair report. | ART:3 "SUPERSEDED"; ART:8 "Verdict: PASS". |

## Answer

### 1. Final `VERSION-18-FINDINGS.md` (complete)

```markdown
# Manafold version 18 findings — final, Waves A–F

**Directions:** `OWNER-DIRECTION-19-2026-09-19.md` [OWNER-DIRECTION-19-2026-09-19.md:1]
**Inventory:** not shown
**Architecture:** `V18-ARCHITECTURE.md` is referenced, but its text is not shown [V18-ARCHIVE-CLOSURE.md:5; V18-ROOT-AUTHORITY-IMPLEMENTATION.md:5; V18-ROOT-MATERIAL-IMPLEMENTATION.md:5]
**Scope:** Waves A–F closed; the exact final bank is `READY-TO-ENCODE` with no blocking item [V18-WAVE-F-PERFORMANCES.md:6; V18-FINAL-BANK-INTEGRITY.md:5].
**Report status:** `V18-SWELL-FRONT-ART.md` is superseded; all Wave D build/gate/CRC/picture receipts stand in `V18-SWELL-FRONT-REPAIR.md`, and this document cites it for those receipts [V18-SWELL-FRONT-REPAIR.md:7; coordinator correction].

## Owner Direction 19 map

| # | Direction-19 item | Status | What was done, in plain language |
|---:|---|---|---|
| 1 | Front root flexibility is reopened [OWNER-DIRECTION-19-2026-09-19.md:11] | done | Root rotation authority was integrated, and real public JunctionF X/Y curves were selected so the front connection flexes in the shipping motion [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:18-24,42; V18-SWELL-FRONT-ART.md:33-44; V18-SWELL-FRONT-REPAIR.md:94]. |
| 2 | Both body connections must become body-smooth [OWNER-DIRECTION-19-2026-09-19.md:12] | done | The root material `bodyblend` was selected at `16 / 16` rows front/rear; no root-ink exclusion was shipped because the pictures did not justify it [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43,62-67,71-75]. |
| 3 | All visible antenna balls become smaller [OWNER-DIRECTION-19-2026-09-19.md:13] | done | The 400 pm family was selected by eye, with named sizes Front `10 / 12`, A `26 / 34`, B `25 / 33`, C `24 / 31`, End `20 / 25` mm [V18-SWELL-FRONT-ART.md:14,25-29 (selection record; superseded for receipts); V18-SWELL-FRONT-REPAIR.md:104,108]. |
| 4 | Archive both experiment collections [OWNER-DIRECTION-19-2026-09-19.md:14] | done | Version 17 and both experiment collections were archived; 56/56 source/archive pairs matched, 70,576,645 bytes were locked, and menu/lab no longer consume live tabs [V18-ARCHIVE-CLOSURE.md:10-14,21-31,43]. |
| 5 | Particle/antenna intersections get a bounded attempt [OWNER-DIRECTION-19-2026-09-19.md:15] | done-differently | The pre-layer route was declined because it flattens depth; instead a small 120 mm mote-surface fade was shipped [V18-WAVE-E-CLEANUP.md:58-64,74]. |
| 6 | Normalize relic mana [OWNER-DIRECTION-19-2026-09-19.md:16] | done | Crackle moved to normal candidate 9 under the day sky; Drift and Blown kept candidate 9 after the history trail was removed [V18-WAVE-E-CLEANUP.md:42-54]. |
| 7 | Flight needs unmistakable vertical travel [OWNER-DIRECTION-19-2026-09-19.md:17] | done | Wave F re-authored Flight: two slow climb-and-sink cycles on an 800 mm amplitude with a 500 mm cruise lift, and a camera change that keeps the apex in frame; in-frame travel grew from about 10 px to about 70 px [V18-WAVE-F-PERFORMANCES.md:8-26]. |
| 8 | Hasty must have no frame-history smear [OWNER-DIRECTION-19-2026-09-19.md:18] | done (live subjects) | The old 48×30 creature-following history plane was identified as the Hasty smear and is now off for all 22 live subjects, with an exact legacy control; the archived version-17 generation and the `legacy` control intentionally keep the old effect [V18-WAVE-E-CLEANUP.md:16,22,30-32,36-37; coordinator scoping correction]. |
| 9 | Trick gets a longer rotational phrase [OWNER-DIRECTION-19-2026-09-19.md:19] | done | Trick now does the 180 → pause → 360 → slight overshoot → correct → existing righting, pivoting about its planted antenna [V18-WAVE-F-PERFORMANCES.md:6,30-52]; the integration packet pinned the planted-tip slide the legacy wobble had left at about 178 mm [V18-FINAL-BANK-INTEGRITY.md:16-34]. |
| 10 | This is version 18 [OWNER-DIRECTION-19-2026-09-19.md:20] | done | The Wave A–F reports, the final bank report and the site archive use "Manafold version 18" [V18-ARCHIVE-CLOSURE.md:1; V18-WAVE-F-PERFORMANCES.md:1; V18-FINAL-BANK-INTEGRITY.md:1]. |
| 11 | Art acceptance remains visual [OWNER-DIRECTION-19-2026-09-19.md:21] | done | Picture values were chosen by eye from complete native motion — Wave F recorded 16 looks, each written down — while gates checked structure, contacts, continuity and regressions [V18-ROOT-MATERIAL-IMPLEMENTATION.md:67; V18-WAVE-F-PERFORMANCES.md:6; V18-FINAL-BANK-INTEGRITY.md:38]. |

## What was actually wrong

1. **The front root still read like a stiff body-mounted peg.** The version-17 roots still used the cooler/coarser antenna atlas and the same swell sizes, so Wave B had to integrate root rotation authority before real Front motion could be judged [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:85].
2. **The roots still read as a separate surface.** The legacy material retained a darker/coarser strip through the body crossings, and the root still read as a separately surfaced lobe [V18-ROOT-MATERIAL-IMPLEMENTATION.md:62].
3. **The antenna balls still read as protruding beads.** At the `1000` rung the version-17 protruding-bead read remained [V18-SWELL-FRONT-ART.md:16].
4. **The live history trail was still present.** The old `u02_mist` plane gave travelling clips a chunky grey-violet history trail; that plane was the Hasty smear and the Drift/Blown relic trail [V18-WAVE-E-CLEANUP.md:16].
5. **Crackle was still on an old mana treatment.** It was the only live subject still on an old mana: candidate 4 under the night backdrop [V18-WAVE-E-CLEANUP.md:44].
6. **Particles could still draw through antenna sticks.** The "through" read came from a flat mote disc's centre crossing a stick's front surface, so a narrow depth-based fade was needed [V18-WAVE-E-CLEANUP.md:58-60].
7. **Flight and Trick were still not version-18, and the plant was not truly planted.** Flight still needed its vertical phrase, Trick still awaited the planted full turn, and the untouched no-spin plant's balance wobble let the antenna contact wander about 178 mm through the pause [V18-SWELL-FRONT-ART.md:108; V18-WAVE-F-PERFORMANCES.md:94].

## What landed

### Wave A — archive closure

The live version-17 media were locked as an immutable byte archive before any version-18 encode could overwrite live names: 28 WebMs plus 28 posters, 56/56 source/archive pairs matching exact SHA-256 and byte length, totaling 70,576,645 bytes [V18-ARCHIVE-CLOSURE.md:10-14].

The site now has one archive generation labelled exactly `Version 17 + mana experiments — 2026-09-19`, containing `Version 17` with exactly 22 non-menu production clips, `Mana menu` with exactly six clips, and `Mana lab · 2026-09-05` with exactly ten clips [V18-ARCHIVE-CLOSURE.md:21-29]. Menu and lab no longer consume live tabs; the twelve now-undeclared live menu files were removed without deleting their immutable copies [V18-ARCHIVE-CLOSURE.md:31-33].

The permanent `checkarchive.py` gate verifies the locked archive, live/archive pairs, declarations, duplicates, absence of menu/lab from live tabs, and archived playback attributes [V18-ARCHIVE-CLOSURE.md:37-44]. The local assemble gate ran with RC 0: 22 live Manafold outer tabs, 13 archive generations, 22 live fresh / 6 archived freshness, 1,420 declared / 1,420 decoded media files, no deployment [V18-ARCHIVE-CLOSURE.md:54-64].

### Wave B — root authority

Manafold now uses 27/32 content bones, with four append-only helpers that are not visible joints: `FrontRootDelta`, `RearPreRootDelta`, `RearRootTurnMid`, and `RearRootDelta` [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:20-24].

The integrated skin preserves the original F–A/C–End gradient starts, helper fractions, full endpoints, burial behavior, and all seven version-17 signed helper IDs [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:26-32]. `ZHAO_U02_ROOT_AUTHORITY=integrated|legacy-split` is strict, and `legacy-split` restores the exact version-17 root palettes [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:34].

This wave deliberately did not ship a dishonest `normal|mute` presentation control because no shipping clip owned nonzero Front X/Y art yet; Wave D added that only after real public curves existed [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:42]. The root gate checked compiled skin, shipping keys/midpoints, the terminal rear exception, helper/carrier rotations, and 339,500 posed ring steps with zero reversal/pinch [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:48-56].

This was not final root likeness evidence: both roots still used the version-17 cooler/coarser antenna atlas and the same swell sizes [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:85].

### Wave C — root material

The page generator now has named shipping controls `ROOT_MATERIAL_MODE = "bodyblend"`, `ROOT_FRONT_BLEND_ROWS = 16`, and `ROOT_REAR_BLEND_ROWS = 16` [V18-ROOT-MATERIAL-IMPLEMENTATION.md:15-18].

The safe deterministic ladder compared four candidates: version-17 legacy, narrow `8 / 8`, medium `16 / 16`, and broad `24 / 24`; the selected medium candidate was chosen from the complete native motion, not from the atlas arithmetic [V18-ROOT-MATERIAL-IMPLEMENTATION.md:39-44,67].

The moving read was: legacy retained a separately surfaced lobe; narrow left a visible material change through the root support; medium made both connections carry the body's pink/grain treatment while A/B/C and the free middle kept the cooler antenna character; broad carried body treatment too far into the free chain [V18-ROOT-MATERIAL-IMPLEMENTATION.md:62-65].

Wave C made no change to `add_visible_body_inner_edge()`, its outline gate, or render ordering, because a no-inner-ink diagnostic did not expose a root seam that justified changing production ink ownership [V18-ROOT-MATERIAL-IMPLEMENTATION.md:71-75].

### Wave D — smaller swells and public Front flex (receipts per the repair report)

The 400 pm swell family was selected by eye: `1000` retained the bead read, `700` and `550` improved it but the free stations still dominated, `400` kept all five stations plainly thicker while reading as local thickening, and `250` flattened toward a nearly uniform strap [V18-SWELL-FRONT-ART.md:16-19 (selection record; superseded for receipts); V18-SWELL-FRONT-REPAIR.md:108].

The shipping family is Front `10 / 12`, A `26 / 34`, B `25 / 33`, C `24 / 31`, End `20 / 25` mm, compared with exact version-17 controls Front `26 / 31`, A `64 / 86`, B `62 / 82`, C `60 / 78`, End `50 / 62` mm [V18-SWELL-FRONT-ART.md:23-29]. The repair notes that the diagnostic 400 rung is not an exact integer match to the shipping constants because of truncation/rounding, with no visual consequence [V18-SWELL-FRONT-REPAIR.md:104].

Real public Front/JunctionF X/Y performances were authored for the named clips, with the selected global gain the named `1500 pm` owner knob [V18-SWELL-FRONT-ART.md:33-44]. `HingePlay::tilt_front/yaw_front` is the only Front X/Y authority, and the gate checked Front X/Y in 10 clips, weakest Rest at `24.37 mm` above the unchanged 20 mm floor [V18-SWELL-FRONT-REPAIR.md:15,94].

The review blockers were repaired: terminal taper restored, Trick support ownership hardened, duplicate Front API removed [V18-SWELL-FRONT-REPAIR.md:11-15]. A zero-radius terminal cap mesh fault (16 degenerate triangles, RC 1 from `manafold-meshcheck`, against a checkpoint that had claimed green) was repaired with a `2/2` mm real cap, and the wrong-presentation review sheets were regenerated from the production cel renderer [V18-SWELL-FRONT-REPAIR.md:21-29].

The closure gate matrix passed 88/88, and the production renderer receipt is in the repair report [V18-SWELL-FRONT-REPAIR.md:75,88].

### Wave E — live presentation cleanup

The old live history mist was retired from all 22 live subjects. The cause was confirmed by picture: `subject_u02_clip` had given every non-slot-7 clip the persistent 48×30 creature-following plane — the Hasty smear and the Drift/Blown relic trail [V18-WAVE-E-CLEANUP.md:16].

The live builder default is now mist off, with `ZHAO_U02_LIVE_MIST=off|legacy` as the strict control; `legacy` restores the version-17 builder default and fired the gate (RC 5), with 21/22 sequence CRCs equal to the Wave-D bank and the 22nd (Crackle) differing only by the intended mana change [V18-WAVE-E-CLEANUP.md:30-32,37].

Crackle now uses the builder's candidate 9 with no `u02_backdrop` call — day sky, ending the deliberate Crackle/Channel pairing; Channel keeps its night backdrop. The exact legacy control `manafold-crackle-legacy` preserves the old bytes [V18-WAVE-E-CLEANUP.md:42-46].

Drift and Blown kept candidate 9 after the history removal: Drift is a continuous right-to-left traverse with no trail, Blown a continuous launch/tumble/drop/catch/settle with f291 matching f0 [V18-WAVE-E-CLEANUP.md:51-54].

The bounded particle attempt shipped as a small mote-surface fade: the pre-layer route was declined because pre splats would draw while the depth buffer held only terrain and then be covered by the creature, flattening depth; the shipped route ramps mote visibility to zero as the disc centre's view depth approaches the antenna surface within the shipped `120 mm` (`240` began fading motes genuinely in front), averaged over a 5×5 disc footprint [V18-WAVE-E-CLEANUP.md:58-62,69-74].

The gate matrix passed 97/97, and the production renderer receipt is in the Wave E report [V18-WAVE-E-CLEANUP.md:76,88].

### Wave F — Flight, Trick, Drift (every value chosen by looking; 16 recorded looks)

**Flight.** The version-17 bob was four symmetric 300 mm bounces moving about 10 px at half scale, and the v17 trough cleared the dirt by only 210 mm, so a deeper bob had to carry the whole flight higher [V18-WAVE-F-PERFORMANCES.md:10]. One clock still drives everything through a single periodic phase warp, so the loop seam is exact for every rung [V18-WAVE-F-PERFORMANCES.md:12]. Shipped knobs, each chosen by looking: cycles 4→2, amplitude 300→800 mm, new cruise lift 500 mm (keeping the v17 trough), rise fraction 0x8000→24000 (0.366, quick climb / long glide), top hang 4000 (floats at the apex), pitch lead 4096 (nose leads the climb by about 5 keys), camera k 250000→220000 with bias 0→+5000 (the apex antenna had been cut at 900 mm) [V18-WAVE-F-PERFORMANCES.md:14-22]. With v17-neutral values the clip reproduces the version-17/Wave-E bytes exactly (`0x5B272AB3`) [V18-WAVE-F-PERFORMANCES.md:24]. Every frame of the shipped clip was looked at: 352 populated frames, no crop, no pop, continuous wrap; against v17 the native travel is roughly 70 px against about 10, with apexes lifting the whole body above the horizon and troughs carrying it low over the dirt [V18-WAVE-F-PERFORMANCES.md:26].

**Trick.** Kept: the v17 pure-X half-turn plant, the +90° face yaw, the fixed camera, the plant/lift keys, the righting, and `kTrickKeys = 200` [V18-WAVE-F-PERFORMANCES.md:30]. The key count deliberately does not grow: every oscillator in the clip is periodic in K, and growing K would re-phase the whole clip and break the loop seam, so the turn is authored inside the existing 70-key plant hold, re-partitioned into touchdown→pause 78–100, one full turn to 1000 plus overshoot 100–128, C2 correction back to exactly 1000 at 128–140, a "ta-da" hold 140–148, then the existing righting [V18-WAVE-F-PERFORMANCES.md:32-40]. The spin is authored as unwrapped micro-turns in two quintic segments evaluated exactly in int64; a whole number of turns is skipped outright, so the pause and the post-correction hold are byte-identical to the no-spin clip [V18-WAVE-F-PERFORMANCES.md:43]. The yaw is world-vertical and pivots about the planted support, with the root X/Z absorbing exactly the horizontal displacement the yaw gives the support (zero outside the turn) [V18-WAVE-F-PERFORMANCES.md:44]. Selected values: start/turn/settle 100/128/140, overshoot `kTrickSpinOvershootPm = 40` (14.4°), gain 1000 [V18-WAVE-F-PERFORMANCES.md:47]. A named Trick camera (k 330000, bias −10000) gives 0 top-touch frames and puts the planted crown on a band of dirt instead of the bottom edge, closing the Wave-D framing item [V18-WAVE-F-PERFORMANCES.md:52].

**Drift framing.** The lopsided journey was plain framing, not the wrap the renderer comment blamed: a constant horizontal aim (`kU02DriftCamBiasX = 14000`, not a tracker) keeps the whole traverse in frame, with leftmost ink at column 1 (f297) and rightmost 9 px from the edge (f0); the antenna and mana are never cut, where v17 cut them for about 40 frames. Residual (owner call): the traverse is about as wide as the frame, so no constant aim buys margin at both ends — that would need a shorter traverse or a pulled-back camera [V18-WAVE-F-PERFORMANCES.md:64].

**Gates — 115/115.** Normals 11/11 RC 0 (mmeshcheck `CLEAN 1416/4200`), mspan 35/35 byte-identical to HEAD, msmooth 16/16 identical to Wave E, protected legs 27/27, the new Q6a/Q6b/Q6c/Q7 each firing only its own category, and 14/14 malformed selectors returning RC 2 [V18-WAVE-F-PERFORMANCES.md:66-81]. Q6a: pause 78..100, peak 1039.98 pm at key 128, exactly 1000.00 pm by key 140, 1 reversal, worst step 69.4 pm/key under the 90 bound, off-axis 0.00004; Q6c support drift shipped at 15.75 mm under the 24 mm bound; Q7 Flight: 2 maxima, 800.0 mm, seam step/accel/jerk 77.5/4.8/1.07 within interior 86.4/9.4/1.45 [V18-WAVE-F-PERFORMANCES.md:73-76]. mprobe: carrier B owns 82/82 key+midpoint samples through keys 100..140 (depth −27..−18 mm), the whole window stays 140/140, and Flight clearance is 211 mm [V18-WAVE-F-PERFORMANCES.md:77]. Two gate adjustments were declared in source: mqa Q3 gains a 240 mm Trick root-step ceiling (the body orbits its support at peak turn speed — a C2 move, not a teleport), and mprobe's TRAVEL float gate leaves Trick's declared contact keys to the contact contract [V18-WAVE-F-PERFORMANCES.md:78-80]. The one-invocation bank of 22 live + Crackle-legacy + 13 non-live subjects was 33/36 byte-identical to the Wave-E receipts; the three that differ are exactly the intended Flight/Trick/Drift [V18-WAVE-F-PERFORMANCES.md:83].

**Instrument honesty, recorded on the way.** The prep's camera premise was wrong (the house u02 bias is 0, not 14000) and was caught by the neutral byte check against the Wave-E bank before any art was chosen; the screen-space trackers failed on both Flight (the magenta mask lost the body at the trough) and Drift, and a bottom-gap metric read the dirt as ink — none was used for a decision, and every numeric framing check was first run on a known-negative and a known-positive frame [V18-WAVE-F-PERFORMANCES.md:50,87-92].

### Integration packet — plant pin, integrated gate, exact final bank

**The planted-tip slide.** In the untouched no-spin plant, the legacy balance wobble made the weighted antenna contact wander 178 mm through the pause (v17/Wave-D behaviour) — flagged as an owner call in Wave F and fixed in the integration packet [V18-WAVE-F-PERFORMANCES.md:94; V18-FINAL-BANK-INTEGRITY.md:18]. The support-point X/Z pivot built for the spin now runs across the whole contact window, keys 78..147: at every planted key the root X/Z absorbs carrier B's displacement from touchdown, the spin compensation is added on top unchanged, and at the lift the held offset releases to zero with a C2 quintic over `kTrickPinReleaseKeys = 12` keys (0.4 s) riding the righting; the wobble, pause timing, flex, and spin timing are untouched [V18-FINAL-BANK-INTEGRITY.md:20-24]. New gate Q6d PLANT PIN (bound 24 mm, about 1.5 px at the Trick camera): shipped 12.99 mm at key 131 against the legacy control's 178.46 mm, which fires Q6d only [V18-FINAL-BANK-INTEGRITY.md:28-34]. The toggle `ZHAO_U02_TRICK_PLANT_PIN=pinned|legacy` is strict (RC 2 on a bad value; `ZHAO_U02_TRICK_PIN_RELEASE_KEYS` accepts 2..30), and `legacy` reproduces the Wave-F Trick `0xBF695C69` byte for byte [V18-FINAL-BANK-INTEGRITY.md:26]. Decision by eye — A/B at native scale, a per-frame contact slit-scan, the release, and 3x plant crops: the pinned version reads as balancing on a planted tip, the legacy as sliding, and it does not read worse, so the pin ships and the toggle stays [V18-FINAL-BANK-INTEGRITY.md:38-44].

**Integrated gate and build.** A clean direct build from source `db2bcf0e` (which adds the plant pin over Wave F's `d7d51171`/`bf51325f`) supplied both the gates and the bank renderer; CMake was not run and no CMake result is claimed [V18-FINAL-BANK-INTEGRITY.md:7,48]. The integrated gate matrix passed 113/113, including the five Wave F/integration controls each firing only its own category (Q6a, Q6b, Q6c, Q6d, Q7) and 16/16 malformed selectors returning RC 2 [V18-FINAL-BANK-INTEGRITY.md:52-58]. Exact-off identities re-rendered with the final binary: Flight v17-neutral `0x5B272AB3` (Wave E), Trick `SPIN=none` + `PLANT_PIN=legacy` + v17 camera (k 360000, bias 0) `0x22563A37` (Wave E), Trick `PLANT_PIN=legacy` `0xBF695C69` (Wave F), Drift `CAM_BX=0` `0x3A11AB0C` (Wave E), crackle-legacy `0xEDDC80D7` (Wave F), Zixxtrixx Idle `0x1408F885` (HEAD) [V18-FINAL-BANK-INTEGRITY.md:63-70].

**The exact final bank.** The single hashed renderer received all 22 live subjects in one invocation under the production environment (`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, live mist unset), RC 0 [V18-FINAL-BANK-INTEGRITY.md:86]. The P17-narrowed validator checked every header, byte count, dimension, contiguous name, meta subject/frame count and per-frame receipt row: **PASS — 22 subjects, 7,992 frames, 0 errors** [V18-FINAL-BANK-INTEGRITY.md:88-93]. Against the Wave-F final bank, 21/22 subjects are byte-identical and only Trick changed (`0xBF695C69` → `0xAB4D78E9`), the intended pin; against the same-binary `legacy` render, Trick differs only on f156..f318 (keys 78..159: the contact window plus the release) [V18-FINAL-BANK-INTEGRITY.md:121-123]. One untraced note: key 78's frame differs even though its pin offset is exactly zero; the probable cause is a neighbour-reading post-pass such as `finalize_rear_follow` [V18-FINAL-BANK-INTEGRITY.md:124].

**Per-subject visual verdict.** Every one of the 22 complete every-frame sheets was looked at and every subject is PASS (Drift PASS with the owner call below) [V18-FINAL-BANK-INTEGRITY.md:129-147]. Coverage caveat, stated by the reviewer: the sheets were read at their ≤1600 px JPEG scale, where continuity, pops, framing, seams and gross reads can be judged but fine detail (root seams, ink ownership, particle crossings) cannot; for the 21 byte-identical subjects that detail transfers from the Wave D/E/F reviews through exact byte equality, and Hasty's no-smear claim rests on the 22/22 live-history gate plus Wave E rather than on confirmation at scale [V18-FINAL-BANK-INTEGRITY.md:129,136].

## Failable evidence

- **Wave A:** `checkarchive.py` and `checkfresh.py` were wired into assemble/deploy; the local gate showed 22 live fresh / 6 archived / 0 stale / 0 absent / 0 unknown, 1,420 declared / 1,420 decoded, RC 0 [V18-ARCHIVE-CLOSURE.md:37-48,54-63].
- **Wave B:** `mspan` added root-authority checks; normal gates RC 0, and the root-authority control restored legacy palettes and fired only its attributed category [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:48-58,73].
- **Wave C:** deterministic regeneration was byte-exact, legacy material words were 98,302 / 98,302 exact, and the protected gate matrix stayed green [V18-ROOT-MATERIAL-IMPLEMENTATION.md:87-96].
- **Wave D:** repair gate matrix 88/88, including mspan 35/35 attributed, msmooth 16/16 attributed, protected legs 27/27, and 9/9 malformed selectors returning RC 2 [V18-SWELL-FRONT-REPAIR.md:88-95].
- **Wave E:** gate matrix 97/97, including live-history normal/legacy/list-drift, Crackle-legacy identity, fade exact-off/fires, non-live identity, and 6/6 malformed selectors returning RC 2 [V18-WAVE-E-CLEANUP.md:76-84].
- **Wave F:** gate matrix 115/115; Q6a peak 1039.98 pm / exactly 1000.00 pm by key 140; Q6c 15.75 mm vs 24 mm bound; Q7 2 maxima, 800.0 mm, seam 77.5/4.8/1.07 within 86.4/9.4/1.45; mprobe 82/82 and 140/140, Flight clearance 211 mm; 14/14 selectors RC 2; byte identities including the 33/36 bank comparison [V18-WAVE-F-PERFORMANCES.md:66-83].
- **Final bank:** integrated matrix 113/113; Q6d 12.99 mm vs 178.46 mm legacy control; 16/16 selectors RC 2; all six exact-off identities; validator PASS 22 subjects / 7,992 frames / 0 errors; 21/22 byte-identical to Wave F with only Trick changed [V18-FINAL-BANK-INTEGRITY.md:52-70,88-93,121-123].

## Correction loops caught by looking

1. **Wave C material ladder:** the atlas arithmetic explained coherence, but the 16/16 value was chosen from complete native motion [V18-ROOT-MATERIAL-IMPLEMENTATION.md:67].
2. **Wave D review blockers:** the pre-review tree had terminal taper, Trick support ownership, and duplicate Front API problems; the repair closed them in source [V18-SWELL-FRONT-REPAIR.md:11-15].
3. **Wave D mesh fault:** a zero-radius terminal cap produced 16 degenerate triangles even though an earlier checkpoint had reported green; the repair restored a `2/2` mm real cap [V18-SWELL-FRONT-REPAIR.md:21-23].
4. **Wave D presentation:** review sheets rendered without the production environment were replaced by sheets from the production cel renderer [V18-SWELL-FRONT-REPAIR.md:27-29].
5. **Wave E fade:** the first centre-pixel fade was rejected by reasoning; the shipped form averages over a 5×5 disc footprint [V18-WAVE-E-CLEANUP.md:62].
6. **Wave E Crackle:** the pictures agreed candidate 9 under the day sky should ship, with the legacy relic read preserved as a control [V18-WAVE-E-CLEANUP.md:48].
7. **Wave F premise:** the prep's "14000 house bias" was wrong (it is 0); the neutral byte check against the Wave-E bank caught it before art was chosen [V18-WAVE-F-PERFORMANCES.md:50,87].
8. **Wave F trackers:** three instrument failures (Flight mask loss at the trough, Drift lighting, dirt-as-ink) were all rejected; decisions used only checks validated on known negatives/positives [V18-WAVE-F-PERFORMANCES.md:88-92].
9. **Plant pin:** the pause the brief said to keep was still sliding about 178 mm; the pin was added in the integration packet, judged by eye, and shipped with its legacy toggle [V18-WAVE-F-PERFORMANCES.md:94; V18-FINAL-BANK-INTEGRITY.md:18,38-44].

## Open items (owner calls, non-blocking)

1. The plant pin ships with the toggle kept; `ZHAO_U02_TRICK_PLANT_PIN=legacy` restores the old slide [V18-FINAL-BANK-INTEGRITY.md:26,159].
2. Drift has no framing margin at either end with a constant aim [V18-WAVE-F-PERFORMANCES.md:64,111; V18-FINAL-BANK-INTEGRITY.md:160].
3. The End-swell stub is unchanged since Wave D [V18-WAVE-F-PERFORMANCES.md:112; V18-FINAL-BANK-INTEGRITY.md:161].
4. Flight's underside reads darker at the troughs; pre-existing version-17 shading [V18-WAVE-F-PERFORMANCES.md:113; V18-FINAL-BANK-INTEGRITY.md:162].
5. The TRAVEL float row is printed but not gated inside Trick's exempt window (0 mm today); a later move to sloped ground would need gating [V18-FINAL-BANK-INTEGRITY.md:82,163].
6. Q3 residual: a Trick-only step between 135 and 240 mm would pass Q3; inside the contact window Q6b/c/d cover root X/Z, outside it the root is authored Y plus a C2 release; declared acceptable [V18-FINAL-BANK-INTEGRITY.md:78].
7. Carried from Wave E: the renderer-side mote-surface fade is not traced by the effect gate, and the final sheet review cannot judge particle crossings at scale; retuning the fade requires re-looking [V18-WAVE-E-CLEANUP.md:104; V18-FINAL-BANK-INTEGRITY.md:129].

## Exact final-bank acceptance

Accepted: **READY-TO-ENCODE**, no blocking item; the open items above are recorded owner calls and do not block the encode [V18-FINAL-BANK-INTEGRITY.md:5].

| Item | Value | Source |
|---|---|---|
| Source commit | Zhaozhou `db2bcf0e` on `manafold-v18` (adds the Trick plant pin; Wave F was `d7d51171`/`bf51325f`; Wave E was `0381bdec`) | [V18-FINAL-BANK-INTEGRITY.md:7; V18-WAVE-E-CLEANUP.md:6] |
| Renderer (one binary for gates + bank) | `zhao-reel-cel.exe`, MD5 `0f082622d4ca0c58d012d1f0de555723`, SHA-256 `52596ea411e57c2a7a2474dc39b39c76834e380d59e401a16760bb67d015ed03` | [V18-FINAL-BANK-INTEGRITY.md:8-10,48] |
| Bank manifest SHA-256 | `bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd` | [V18-FINAL-BANK-INTEGRITY.md:12] |
| Bank | 22 subjects, 7,992 frames, 2,209,692,096 bytes; validator 0 errors | [V18-FINAL-BANK-INTEGRITY.md:12,93] |
| Per-subject CRC32C and ordered-frame SHA-256 | in `bank-manifest.tsv` | [V18-FINAL-BANK-INTEGRITY.md:95-118] |
| Visual verdict | all 22 subjects PASS (Drift with owner call); coverage caveat as stated | [V18-FINAL-BANK-INTEGRITY.md:129-147] |

## Delivery status

- The exact final bank is ready to encode from the raw frame root; the next packet is to encode exactly these 22 subjects and not to re-render [V18-FINAL-BANK-INTEGRITY.md:13,165].
- No encode receipt, production verification, or production deployment receipt is shown in the provided inputs.
- Waves A–E delivery, for the record: Wave A closed locally with deployment none (assemble-only); no deployed site changed [V18-ARCHIVE-CLOSURE.md:64,94].

## Provenance

| Item | Value | Source |
|---|---|---|
| Version-17 accepted Zhaozhou source | `18e1d993` | [PASS-17-FINDINGS.md:7; V18-ARCHIVE-CLOSURE.md:15] |
| Version-17 accepted Zhaozhou evidence | `a4d08762` | [PASS-17-FINDINGS.md:8] |
| Version-17 accepted renderer MD5 | `67DCAFF8ABC0AA2BA830C439A4CD98C7` | [PASS-17-FINDINGS.md:9; V18-ARCHIVE-CLOSURE.md:15] |
| Version-17 accepted renderer SHA-256 | `D60EBA80C2547032A603365837690FB14B551D90D96FB5973F1C976C185C3FE9` | [PASS-17-FINDINGS.md:10] |
| Version-17 exact-bank manifest SHA-256 | `bc2d4d0bdce632815f42cca208763fb04bca62034e285ed1ef707c7f28d55d09` | [PASS-17-FINDINGS.md:11] |
| Version-17 Upheaval source generation | `664f415` | [V18-ARCHIVE-CLOSURE.md:15] |
| Wave A generated index.html | 425,157 bytes, SHA-256 `86626fcc13ae29fcc8c5df81111b8ad4becb00316bf27dd22a50f841b75720f7` | [V18-ARCHIVE-CLOSURE.md:66] |
| Wave C source base | `df072d47` | [V18-ROOT-MATERIAL-IMPLEMENTATION.md:6] |
| Wave C selected medium renderer MD5 | `95080A676C3B5315A9DBEF58D9D10BDA` | [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43] |
| Wave C final working-tree renderer MD5 / SHA-256 | `A5C94FEE8E0CA8A5FD19CC954E8B1DB6` / `F9135C534F9B341FB494723ACA74E885320AC704C072D70776CD40EE3E3487D7` | [V18-ROOT-MATERIAL-IMPLEMENTATION.md:69] |
| Wave D repair production renderer MD5 / SHA-256 | `0FA8BAE0BF6A738C485FAD2DCB77A64B` / `2a1654207e2e09dee9a27dec98e6be1bc62d4dd28594ec434705c0f6b42cda30` | [V18-SWELL-FRONT-REPAIR.md:75] |
| Wave E Zhaozhou source commit | `0381bdec` on `manafold-v18` | [V18-WAVE-E-CLEANUP.md:6] |
| Wave E production renderer MD5 / SHA-256 | `ae7f03a5c32357d891e7915d758d316f` / `4b789bf1fb2a0fb92b688f11596b3f71b70fe60c944c084e5a37a17a22ecd8b8` | [V18-WAVE-E-CLEANUP.md:88] |
| Wave F production renderer MD5 | `e17c5306313e3bdca9763646d06986b5` (SHA-256 shown truncated in the report as `6aa39a2a…ed27`) | [V18-WAVE-F-PERFORMANCES.md:103] |
| Wave F source commits | `d7d51171` / `bf51325f` | [V18-FINAL-BANK-INTEGRITY.md:7] |
| Final source commit | `db2bcf0e` on `manafold-v18` | [V18-FINAL-BANK-INTEGRITY.md:7] |
| Final bank renderer MD5 / SHA-256 | `0f082622d4ca0c58d012d1f0de555723` / `52596ea411e57c2a7a2474dc39b39c76834e380d59e401a16760bb67d015ed03` | [V18-FINAL-BANK-INTEGRITY.md:9-10] |
| Final bank manifest SHA-256 | `bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd` | [V18-FINAL-BANK-INTEGRITY.md:12] |
| Final bank | 22 subjects, 7,992 frames, 2,209,692,096 bytes | [V18-FINAL-BANK-INTEGRITY.md:12,93] |
```

### 2. Final live-card blurb

```text
MANAFOLD, version 18 — smaller carriers, fused roots, clean live mana, unmistakable Flight, and a planted full turn.

THE BALLS ARE SMALLER, NOT GONE. The five antenna carriers were chosen by eye from the 400 pm family: Front 10/12, A 26/34, B 25/33, C 24/31 and End 20/25 mm. Each one still reads thicker than its stick, but as local thickening in one continuous antenna rather than version-17 beads.

THE ROOTS BELONG TO THE BODY. The front and rear roots now carry the body's pink/grain treatment across the visible thickening, then hand gradually back to the cooler antenna surface. The selected 16/16 body-blend was chosen from complete native motion, and no root-ink exclusion was shipped.

THE FRONT CONNECTION FLEXES. Real public JunctionF X/Y curves now move the front antenna connection in the shipping motion at the named 1500 pm Front gain, so the front ball and its connection are no longer a stiff body-mounted peg.

THE LIVE MANA IS CLEAN. The old creature-following history mist is off for all 22 live subjects. Hasty no longer drags its old smear, and Drift and Blown no longer carry their relic trails; the archived version-17 generation and the legacy control intentionally keep the old effect for comparison. Crackle now uses the normal candidate-9 day-sky presentation.

FLIGHT ACTUALLY FLOWS. The version-17 bounce is gone: two slow climb-and-sink cycles ride an 800 mm amplitude on a 500 mm cruise lift, with the apex floating and the nose leading the climb. In frame the travel grew from about 10 pixels to about 70 at half scale, the apex lifts the whole body above the horizon, and the troughs carry it low over the dirt.

THE TRICK PLANTS AND TURNS. The accepted approach, plant and pause are kept; the clip then turns a full 360, overshoots slightly and corrects back before the existing righting, pivoting about its planted antenna. The planted tip no longer slides: the support is pinned through the whole contact window, so it stands nearly still on the dirt — 12.99 mm instead of the old 178.46 mm — with the legacy slide kept behind a toggle.

DRIFT HOLDS ITS FRAMING. The relic's long leftward traverse no longer crops its antenna or mana; a constant camera bias keeps the whole journey in frame.

PARTICLES STAY HONEST. The pre-layer particle route was declined because it would flatten depth. A small 120 mm surface fade thins fold/surge motes as their disc centres meet an antenna surface, while motes genuinely in front still overlap.

THE EXPERIMENTS ARE ARCHIVED. The old Mana menu and Mana lab are out of the live tabs and live as one immutable version-17 experiment generation, with their media and provenance preserved.

PROVENANCE. Accepted source is zhaozhou db2bcf0e. The exact one-binary renderer MD5 is 0f082622d4ca0c58d012d1f0de555723; bank manifest SHA-256 is bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd. All 22 live subjects — 7,992 frames — were rendered together in one invocation and passed the bank integrity gate and the every-frame review as one generation.
```

Blurb sourcing, not part of the site copy: ball sizes / 400 family [V18-SWELL-FRONT-ART.md:14,25-29 (selection record; superseded for receipts); V18-SWELL-FRONT-REPAIR.md:104,108]; root blend / no ink exclusion [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43,62-67,71-75]; front flex / 1500 pm gain [V18-SWELL-FRONT-ART.md:33-44 (superseded for receipts); V18-SWELL-FRONT-REPAIR.md:15,94]; live mist / Hasty / Drift / Blown / Crackle / live-subject scope [V18-WAVE-E-CLEANUP.md:8,16,22,30-32,42-54]; Flight knobs + 10→70 px + apex/trough read [V18-WAVE-F-PERFORMANCES.md:10,14-22,26]; Trick phrase / beats / pivot / overshoot [V18-WAVE-F-PERFORMANCES.md:6,30-52]; plant pin 12.99 mm vs 178.46 mm + toggle [V18-FINAL-BANK-INTEGRITY.md:26,28-34,38-44]; Drift framing [V18-WAVE-F-PERFORMANCES.md:64]; particle fade [V18-WAVE-E-CLEANUP.md:58-74]; archive [V18-ARCHIVE-CLOSURE.md:10-14,21-31,43]; PROVENANCE [V18-FINAL-BANK-INTEGRITY.md:7-12,48,86-93].

### 3. FINDINGS table

See the `## FINDINGS` table at the top of this answer (17 rows: 3×P2, 14×P3, no P1).

## CONTINUATION
- Done: Finalized VERSION-18-FINDINGS.md (Waves A–F) and the live-card blurb. Wave F filled from the report: Flight shipped at cycles 2, amplitude 800 mm, cruise lift 500 mm, camera k 220000/bias +5000, 352 frames, ~70 px vs ~10 px [V18-WAVE-F-PERFORMANCES.md:14-26]; Trick K stayed 200, beats 78–100/100–128/128–140/140–148, overshoot 40 pm, camera k 330000/bias −10000 [V18-WAVE-F-PERFORMANCES.md:32-52]; Drift constant bias 14000, antenna/mana never cut [V18-WAVE-F-PERFORMANCES.md:64]; gates 115/115 [V18-WAVE-F-PERFORMANCES.md:66]. Integration packet: plant pin keys 78..147, Q6d 12.99 mm vs 178.46 mm legacy, toggle `ZHAO_U02_TRICK_PLANT_PIN` [V18-FINAL-BANK-INTEGRITY.md:20-34]; integrated gate 113/113 [V18-FINAL-BANK-INTEGRITY.md:52]. Final provenance filled: commit `db2bcf0e`, renderer MD5 `0f082622…`, SHA-256 `52596ea4…`, manifest SHA-256 `bdaac548…`, 22 subjects / 7,992 frames / 2,209,692,096 bytes, validator 0 errors [V18-FINAL-BANK-INTEGRITY.md:7-12,93]. All 22 per-subject verdicts PASS with the sheet-scale coverage caveat carried [V18-FINAL-BANK-INTEGRITY.md:129-147]. Coordinator corrections applied: REPAIR cited for Wave D receipts; "no smear" scoped to 22 live subjects; plant-pin fix documented; all art values marked chosen by eye.
- Open: encode packet + production verification/deployment receipt not shown [V18-FINAL-BANK-INTEGRITY.md:5,165]; residuals to check — 178 vs 172.7 mm Q3 step across reports, Q3 135–240 mm Trick teleport residual, TRAVEL exemption un-gated inside window, key 78 diff untraced, Wave F SHA-256 truncated, Drift margin, End-swell stub, Flight underside shading, plant-pin owner preference, fade not gate-traced.
- Next chunk: when the encode/production receipt exists, update Delivery status and confirm blurb publication; verify the seven open items above against the receipt. Inputs needed: encode log, deployed-site verification, any owner calls on open items.

## Coordinator verdict

**partial** — xhigh, 22.5k prompt, 30k reasoning, 622s. spot 4/4: provenance source db2bcf0e, renderer MD5 0f082622..., manifest bdaac548..., 22/7,992 all exact vs FINAL-BANK-INTEGRITY:9,12,93; 172.7 vs 178 mm conflict correctly explained. USE WITH CORRECTIONS: (1) blurb/findings must not say the bank 'passed the every-frame review' - say every frame was reviewed on complete contact sheets at sheet scale, with fine detail resting on earlier waves' byte-identical reviews (FINAL:129,136); (2) the Trick checker self-certification gap (Q018 #1) is being fixed in a checker-only commit - may be mentioned as a checker hardening, not an art change.
