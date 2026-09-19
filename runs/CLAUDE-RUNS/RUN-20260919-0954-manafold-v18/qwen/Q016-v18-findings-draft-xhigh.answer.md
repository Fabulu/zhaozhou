# Q016 answer — v18-findings-draft-xhigh

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 132ae886a4be4d0b8907c6113ef7396d
- when: 2026-09-19T18:56:12  seconds: 424  finish: stop  status: ok
- usage: {"completion_tokens": 29508, "completion_tokens_details": {"reasoning_tokens": 20032}, "prompt_tokens": 28209, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 57717}
- inputs: [{"input": "Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md:1-21", "sha256": "c4e32faf91efcc0e", "chars": 4612}, {"input": "Upheaval/creature/Manafold/PASS-17-FINDINGS.md:1-111", "sha256": "752d0c9fc3325a04", "chars": 12361}, {"input": "Upheaval/website/creatures.json:37-37", "sha256": "32f92ba41e1ed209", "chars": 2698}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ARCHIVE-CLOSURE.md:1-94", "sha256": "35cdd71e5931db54", "chars": 5153}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-AUTHORITY-IMPLEMENTATION.md:1-106", "sha256": "e8b0e9292c96ac8e", "chars": 8150}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-MATERIAL-IMPLEMENTATION.md:1-113", "sha256": "3fda0473255e7e04", "chars": 7756}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md:1-128", "sha256": "896c5514a21f93cb", "chars": 10619}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:1-131", "sha256": "bc06392311d43ba3", "chars": 17442}, {"input": "zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:1-111", "sha256": "45415db985cf7465", "chars": 12597}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md:3,8`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:7` | The superseded Wave-D report still says PASS, but its build/gate/CRC/picture receipts do not stand. | Line 3 says “SUPERSEDED… receipts do not”; line 8 still says “Verdict: PASS”; repair line 7 says it supersedes those receipts. |
| 2 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md:21`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:104` | The “equal to the integer output of the 400 pm rung” claim is only approximate. | ART line 21 says shipping values equal the selected 400 pm rung; repair line 104 says truncation/rounding leaves 1 mm differences on A rx, B rx/rz, End rz. |
| 3 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-AUTHORITY-IMPLEMENTATION.md:66`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-MATERIAL-IMPLEMENTATION.md:69`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:75`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:88` | Multiple renderer hashes exist across Waves B–E, and the final accepted bank provenance is still unfilled. | Each report gives a different production renderer hash; no final accepted v18 bank hash/manifest/visual verdict is shown. |
| 4 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:21` | A previous Wave-D checkpoint claimed mesh green while the actual mesh gate was red. | Line 21 says `manafold-meshcheck` returned RC 1 with 16 degenerate triangles while the crashed worker’s checkpoint claimed all protected normals green. |
| 5 | P2 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:104` | The renderer-side mote-surface fade is not traced by the effect gate. | Line 104 says msmooth traces mote visibility inside fx, not the renderer fade, and retuning requires re-looking. |
| 6 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md:7`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:6-7` | The intermediate Wave-D source base `6f0201e3` is ambiguous because it appears in the superseded report. | ART line 7 names source base `6f0201e3`; repair line 6-7 supersedes receipts but does not restate the source base. |
| 7 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:119`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:5,8` | The repair’s “Waves E/F” wording is stale relative to Wave E closure. | Repair line 119 says history/Flight/Trick remain Waves E/F; Wave E lines 5 and 8 say Wave F not started and history closed. |
| 8 | P3 | `Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md:18`; `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:23,30,37` | “Hasty has no smear everywhere” must be scoped to live subjects because legacy/archive controls keep the old effect. | Direction line 18 says remove it everywhere; Wave E lines 23, 30 and 37 show live off but archived/legacy explicit mist remains. |
| 9 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:53,103` | Drift framing is open, not a Wave-E regression. | Lines 53 and 103 say f260–298 sit at the extreme left edge with antenna/mana partly cropped, a camera/owner question. |
| 10 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:118` | The End-swell stub remains an open owner-glance item. | Line 118 says it is visible at Rest 342, Taunt III 328, Trick 393, present in accepted Wave-C renders. |
| 11 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md:117` | Trick crown framing remains open for Wave F. | Line 117 says planted crown sits at bottom of frame in Wave C and Wave D; Wave F should revisit camera/framing. |
| 12 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md:76,91,99` | Final v18 bank acceptance and final visual verdict are not shown. | Wave E shows 97/97 gate and final-bank CRC examples, but no accepted final bank hash/total frames/every-frame verdict. |
| 13 | P3 | `zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ARCHIVE-CLOSURE.md:64,94` | No production deployment for version 18 is shown. | Line 64 says deployment none for `-AssembleOnly`; line 94 says no deployed site changed. |
| 14 | P3 | `Upheaval/website/creatures.json:37`; not shown | The site-blurb PROVENANCE paragraph must remain a placeholder because final bank provenance is not shown. | Pass-17 blurb style is at creatures.json:37; no final v18 bank provenance appears in the inputs. |

### VERSION-18-FINDINGS.md (draft)

```markdown
# Manafold version 18 findings — draft, Waves A–E

**Directions:** `OWNER-DIRECTION-19-2026-09-19.md` [OWNER-DIRECTION-19-2026-09-19.md:1]
**Inventory:** not shown
**Architecture:** `V18-ARCHITECTURE.md` is referenced, but its text is not shown [V18-ARCHIVE-CLOSURE.md:5; V18-ROOT-AUTHORITY-IMPLEMENTATION.md:5; V18-ROOT-MATERIAL-IMPLEMENTATION.md:5]
**Scope:** Waves A–E only. Wave F is still in progress and is left as a placeholder [V18-WAVE-E-CLEANUP.md:5].

## Owner Direction 19 map

| # | Direction-19 item | Status | What was done, in plain language |
|---:|---|---|---|
| 1 | Front root flexibility is reopened [OWNER-DIRECTION-19-2026-09-19.md:11] | done | Root rotation authority was integrated, and real public JunctionF X/Y curves were selected so the front connection can flex in the shipping motion [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:18-24,42; V18-SWELL-FRONT-ART.md:33-44; V18-SWELL-FRONT-REPAIR.md:94]. |
| 2 | Both body connections must become body-smooth [OWNER-DIRECTION-19-2026-09-19.md:12] | done | The root material `bodyblend` was selected at `16 / 16` rows front/rear; no root-ink exclusion was shipped because the pictures did not justify it [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43,62-67,71-75]. |
| 3 | All visible antenna balls become smaller [OWNER-DIRECTION-19-2026-09-19.md:13] | done | The 400 pm family was selected by eye, with named sizes Front `10 / 12`, A `26 / 34`, B `25 / 33`, C `24 / 31`, End `20 / 25` mm [V18-SWELL-FRONT-ART.md:14,25-29; V18-SWELL-FRONT-REPAIR.md:104,108]. |
| 4 | Archive both experiment collections [OWNER-DIRECTION-19-2026-09-19.md:14] | done | Version 17 and both experiment collections were archived; 56/56 source/archive pairs matched, 70,576,645 bytes were locked, and menu/lab no longer consume live tabs [V18-ARCHIVE-CLOSURE.md:10-14,21-31,43]. |
| 5 | Particle/antenna intersections get a bounded attempt [OWNER-DIRECTION-19-2026-09-19.md:15] | done-differently | The pre-layer route was declined because it flattens depth; instead a small 120 mm mote-surface fade was shipped [V18-WAVE-E-CLEANUP.md:58-64,74]. |
| 6 | Normalize relic mana [OWNER-DIRECTION-19-2026-09-19.md:16] | done | Crackle moved to normal candidate 9 under the day sky; Drift and Blown kept candidate 9 after the history trail was removed [V18-WAVE-E-CLEANUP.md:42-54]. |
| 7 | Flight needs unmistakable vertical travel [OWNER-DIRECTION-19-2026-09-19.md:17] | pending-Wave-F | No version-18 Flight vertical phrase was shipped in Waves A–E [V18-SWELL-FRONT-ART.md:108; V18-WAVE-E-CLEANUP.md:5]. |
| 8 | Hasty must have no frame-history smear [OWNER-DIRECTION-19-2026-09-19.md:18] | done | The old 48×30 creature-following history plane was identified as the Hasty smear and is now off for all 22 live subjects, with an exact legacy control and renderer assertion [V18-WAVE-E-CLEANUP.md:16,22,30-32,36-37]. |
| 9 | Trick gets a longer rotational phrase [OWNER-DIRECTION-19-2026-09-19.md:19] | pending-Wave-F | The planted 180 → pause → 360 → overshoot → correct phrase is not yet shipped [V18-SWELL-FRONT-ART.md:108; V18-WAVE-E-CLEANUP.md:5]. |
| 10 | This is version 18 [OWNER-DIRECTION-19-2026-09-19.md:20] | done | The Wave A–E reports and the site archive use “Manafold version 18” [V18-ARCHIVE-CLOSURE.md:1; V18-ROOT-AUTHORITY-IMPLEMENTATION.md:1; V18-ROOT-MATERIAL-IMPLEMENTATION.md:1; V18-WAVE-E-CLEANUP.md:1]. |
| 11 | Art acceptance remains visual [OWNER-DIRECTION-19-2026-09-19.md:21] | done | The picture values were chosen by eye from complete native motion, while gates checked structure, contacts, continuity and regressions [V18-ROOT-MATERIAL-IMPLEMENTATION.md:67; V18-SWELL-FRONT-ART.md:14; V18-WAVE-E-CLEANUP.md:74,99]. |

## What was actually wrong

1. **The front root still read like a stiff body-mounted peg.** The version-17 roots still used the cooler/coarser antenna atlas and the same swell sizes, so Wave B had to integrate root rotation authority before real Front motion could be judged [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:85].
2. **The roots still read as a separate surface.** The legacy material retained a darker/coarser strip through the body crossings, and the root still read as a separately surfaced lobe [V18-ROOT-MATERIAL-IMPLEMENTATION.md:62].
3. **The antenna balls still read as protruding beads.** At the `1000` rung the version-17 protruding-bead read remained [V18-SWELL-FRONT-ART.md:16].
4. **The live history trail was still present.** The old `u02_mist` plane gave travelling clips a chunky grey-violet history trail; that plane was the Hasty smear and the Drift/Blown relic trail [V18-WAVE-E-CLEANUP.md:16].
5. **Crackle was still on an old mana treatment.** It was the only live subject still on an old mana: candidate 4 under the night backdrop [V18-WAVE-E-CLEANUP.md:44].
6. **Particles could still draw through antenna sticks.** The “through” read came from a flat mote disc’s centre crossing a stick’s front surface, so a narrow depth-based fade was needed [V18-WAVE-E-CLEANUP.md:58-60].
7. **Flight and Trick were still not version-18.** Flight still needed its vertical phrase, and Trick still awaited the planted full turn [V18-SWELL-FRONT-ART.md:108].

## What landed

### Wave A — archive closure

The live version-17 media were locked as an immutable byte archive before any version-18 encode could overwrite live names. The archive contained 28 WebMs plus 28 posters, with 56/56 source/archive pairs matching exact SHA-256 and byte length, totaling 70,576,645 bytes [V18-ARCHIVE-CLOSURE.md:10-14].

The site now has one archive generation labelled exactly `Version 17 + mana experiments — 2026-09-19`, containing three collections: `Version 17` with exactly 22 non-menu production clips, `Mana menu` with exactly six clips, and `Mana lab · 2026-09-05` with exactly ten clips [V18-ARCHIVE-CLOSURE.md:21-29]. Menu and lab no longer consume live tabs, and the twelve now-undeclared live menu files were removed without deleting their immutable copies [V18-ARCHIVE-CLOSURE.md:31-33].

The permanent `checkarchive.py` gate verifies the locked archive, live/archive pairs, declarations, duplicate paths, absence of menu/lab from live tabs, and archived playback attributes [V18-ARCHIVE-CLOSURE.md:37-44]. The local assemble gate ran with RC 0, showing 22 live Manafold outer tabs, 13 archive generations, 22 live fresh / 6 archived freshness, 1,420 declared / 1,420 decoded media files, and no deployment [V18-ARCHIVE-CLOSURE.md:54-64].

### Wave B — root authority

Manafold now uses 27/32 content bones, with four append-only helpers that are not visible joints: `FrontRootDelta`, `RearPreRootDelta`, `RearRootTurnMid`, and `RearRootDelta` [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:20-24].

The integrated skin preserves the original F–A/C–End gradient starts, helper fractions, full endpoints, burial behavior, and all seven version-17 signed helper IDs [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:26-32]. `ZHAO_U02_ROOT_AUTHORITY=integrated|legacy-split` is strict, and `legacy-split` restores the exact version-17 root palettes [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:34].

This wave deliberately did not ship a dishonest `normal|mute` presentation control because no shipping clip owned nonzero Front X/Y art yet; Wave D would add that only after real public curves existed [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:42]. The root gate checked compiled skin, shipping keys/midpoints, terminal rear exception, helper/carrier rotations, and 339,500 posed ring steps with zero reversal/pinch [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:48-56].

This was not final root likeness evidence: both roots still used the version-17 cooler/coarser antenna atlas and the same swell sizes [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:85].

### Wave C — root material

The page generator now has named shipping controls `ROOT_MATERIAL_MODE = "bodyblend"`, `ROOT_FRONT_BLEND_ROWS = 16`, and `ROOT_REAR_BLEND_ROWS = 16` [V18-ROOT-MATERIAL-IMPLEMENTATION.md:15-18].

The safe deterministic ladder compared four candidates: version-17 legacy, narrow `8 / 8`, medium `16 / 16`, and broad `24 / 24` [V18-ROOT-MATERIAL-IMPLEMENTATION.md:39-44]. The selected medium candidate was chosen from the complete native motion, not from the atlas arithmetic [V18-ROOT-MATERIAL-IMPLEMENTATION.md:67].

The moving read was: legacy retained a separately surfaced lobe; narrow left a visible material change through the root support; medium made both connections carry the body’s pink/grain treatment while A/B/C and the free middle kept the cooler antenna character; broad carried body treatment too far into the free chain [V18-ROOT-MATERIAL-IMPLEMENTATION.md:62-65].

Wave C made no change to `add_visible_body_inner_edge()`, its outline gate, or render ordering, because a no-inner-ink diagnostic did not expose a root seam that justified changing production ink ownership [V18-ROOT-MATERIAL-IMPLEMENTATION.md:71-75].

### Wave D — smaller swells and public Front flex

The 400 pm swell family was selected by eye: `1000` retained the bead read, `700` and `550` improved it but the free stations still dominated, `400` kept all five stations plainly thicker while reading as local thickening, and `250` flattened toward a nearly uniform strap [V18-SWELL-FRONT-ART.md:16-19; V18-SWELL-FRONT-REPAIR.md:108].

The shipping selected family is Front `10 / 12`, A `26 / 34`, B `25 / 33`, C `24 / 31`, End `20 / 25` mm, compared with exact version-17 controls Front `26 / 31`, A `64 / 86`, B `62 / 82`, C `60 / 78`, End `50 / 62` mm [V18-SWELL-FRONT-ART.md:23-29]. The repair notes that the diagnostic 400 rung is not an exact integer match to the shipping constants because of truncation/rounding, with no visual consequence [V18-SWELL-FRONT-REPAIR.md:104].

Real public Front/JunctionF X/Y performances were authored for the named clips, and the selected global gain is the named `1500 pm` owner knob [V18-SWELL-FRONT-ART.md:33-44]. The repair report confirms that `HingePlay::tilt_front/yaw_front` is the only Front X/Y authority and that the gate checked Front X/Y in 10 clips, with weakest Rest at `24.37 mm` above the unchanged 20 mm floor [V18-SWELL-FRONT-REPAIR.md:15,94].

The Wave-D review blockers were repaired: terminal taper was restored, Trick support ownership was hardened, and the duplicate Front API was removed [V18-SWELL-FRONT-REPAIR.md:11-15]. A zero-radius terminal cap mesh fault was found and repaired with a `2/2` mm real cap, and the wrong-presentation review sheets were regenerated from the production cel renderer [V18-SWELL-FRONT-REPAIR.md:21-29].

The closure gate matrix passed 88/88 [V18-SWELL-FRONT-REPAIR.md:88]. The production renderer receipt is in the repair report [V18-SWELL-FRONT-REPAIR.md:75].

### Wave E — live presentation cleanup

The old live history mist was retired from all 22 live subjects. The cause was confirmed by picture: `subject_u02_clip` had given every non-slot-7 clip the persistent 48×30 creature-following plane, and that plane was the Hasty smear and the Drift/Blown relic trail [V18-WAVE-E-CLEANUP.md:16].

The live builder default is now mist off, with `ZHAO_U02_LIVE_MIST=off|legacy` as the strict control. `legacy` restores the version-17 builder default and produced RC 5, with 21/22 sequence CRCs equal to the Wave-D bank; the 22nd is Crackle because of the intended mana change [V18-WAVE-E-CLEANUP.md:30-32,37].

Crackle now uses the builder’s candidate 9 with no `u02_backdrop` call, placing it under the day sky and ending the deliberate Crackle/Channel backdrop pairing. Channel keeps its night backdrop [V18-WAVE-E-CLEANUP.md:42-44]. The exact legacy control `manafold-crackle-legacy` preserves the old bytes [V18-WAVE-E-CLEANUP.md:46].

Drift and Blown kept candidate 9 after the history removal. Drift is a continuous right-to-left traverse with no trail, and Blown is a continuous launch/tumble/drop/catch/settle phrase with f291 matching f0 [V18-WAVE-E-CLEANUP.md:51-54].

The bounded particle attempt shipped as a small mote-surface fade. The pre-layer route was declined because pre splats would draw while the depth buffer held only terrain and then be covered by the creature, flattening depth [V18-WAVE-E-CLEANUP.md:58]. The shipped route uses existing depth: for fold and surge mote bodies only, visibility ramps to zero as the disc centre’s view depth approaches the antenna surface within `kMoteSurfaceFadeMm` [V18-WAVE-E-CLEANUP.md:60]. The chosen shipped value is `120 mm`, because `240` began fading motes genuinely in front [V18-WAVE-E-CLEANUP.md:69-74].

The Wave E gate matrix passed 97/97 [V18-WAVE-E-CLEANUP.md:76]. The production renderer receipt is in the Wave E report [V18-WAVE-E-CLEANUP.md:88].

## Failable evidence

- **Wave A:** `checkarchive.py` and `checkfresh.py` were wired into assemble/deploy; the local gate showed 22 live fresh / 6 archived / 0 stale / 0 absent / 0 unknown, 1,420 declared / 1,420 decoded, and RC 0 [V18-ARCHIVE-CLOSURE.md:37-48,54-63].
- **Wave B:** `mspan` added root-authority checks; normal gates were RC 0, and the root-authority control restored legacy palettes and fired only its attributed category [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:48-58,73].
- **Wave C:** deterministic regeneration was byte-exact, legacy material words were 98,302 / 98,302 exact, and the protected gate matrix stayed green [V18-ROOT-MATERIAL-IMPLEMENTATION.md:87-96].
- **Wave D:** the repair gate matrix was 88/88, including mspan 35/35 attributed, msmooth 16/16 attributed, protected legs 27/27, and 9/9 malformed selectors returning RC 2 [V18-SWELL-FRONT-REPAIR.md:88-95].
- **Wave E:** the gate matrix was 97/97, including live-history normal/legacy/list-drift, Crackle-legacy identity, fade exact-off/fires, non-live identity, and 6/6 malformed selectors returning RC 2 [V18-WAVE-E-CLEANUP.md:76-84].

## Correction loops caught by looking

1. **Wave C material ladder:** the atlas arithmetic explained coherence, but the 16/16 value was chosen from complete native motion [V18-ROOT-MATERIAL-IMPLEMENTATION.md:67].
2. **Wave D review blockers:** the pre-review tree had terminal taper, Trick support ownership, and duplicate Front API problems; the repair closed them in source [V18-SWELL-FRONT-REPAIR.md:11-15].
3. **Wave D mesh fault:** a zero-radius terminal cap produced 16 degenerate triangles even though an earlier checkpoint had reported green; the repair restored a `2/2` mm real cap [V18-SWELL-FRONT-REPAIR.md:21-23].
4. **Wave D presentation:** review sheets rendered without the production environment were replaced by sheets from the production cel renderer [V18-SWELL-FRONT-REPAIR.md:27-29].
5. **Wave E fade:** the first centre-pixel fade was rejected by reasoning, and the shipped form averages over a 5×5 disc footprint [V18-WAVE-E-CLEANUP.md:62].
6. **Wave E Crackle:** the pictures agreed that candidate 9 under the day sky should ship, and the legacy relic read was preserved as a control [V18-WAVE-E-CLEANUP.md:48].

## WAVE F — Flight + Trick 360 (in progress; placeholder)

- **Flight vertical travel:** pending-Wave-F. Direction 19 item 7 asks for unmistakable up/down flight travel [OWNER-DIRECTION-19-2026-09-19.md:17]. Wave E explicitly did not start Wave F [V18-WAVE-E-CLEANUP.md:5], and the earlier Wave-D report says Flight still needs its version-18 vertical phrase [V18-SWELL-FRONT-ART.md:108].
- **Trick 180 → pause → 360 → overshoot → correct:** pending-Wave-F. Direction 19 item 9 asks for the longer rotational phrase while preserving the accepted approach, plant, readable face and pause [OWNER-DIRECTION-19-2026-09-19.md:19]. The earlier Wave-D report says Trick still awaits Wave F’s planted full turn [V18-SWELL-FRONT-ART.md:108].
- **Open carried items:** Trick crown framing and the End-swell stub are carried into Wave F/final review [V18-SWELL-FRONT-REPAIR.md:117-118; V18-WAVE-E-CLEANUP.md:105].
- **No final Wave F picture, gate, or provenance claim is made in this draft.**

## Exact final-bank acceptance

**Not shown.** The inputs show Wave E’s 97/97 gate matrix, final-bank CRC examples, and committed every-frame sheets, but they do not show an accepted final version-18 bank hash, total frame count, every-frame final review verdict, or production deployment receipt [V18-WAVE-E-CLEANUP.md:76,91,99].

## Delivery status

- Wave A was closed locally and was ready for independent commit/push, with no production renderer source, version-18 media, main branch, or deployed site changed [V18-ARCHIVE-CLOSURE.md:94].
- Wave A’s generated site page was produced locally with deployment set to none because the gate ran assemble-only [V18-ARCHIVE-CLOSURE.md:64-66].
- No version-18 production deployment receipt is shown in the provided inputs.

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
| Final bank provenance | `[final bank provenance — fill after the final render]` | not shown |
```

### Draft site blurb

```text
MANAFOLD, version 18 — smaller carriers, fused roots, clean live mana, and a bounded particle answer.

THE BALLS ARE SMALLER, NOT GONE. The five antenna carriers were chosen by eye from the 400 pm family: Front 10/12, A 26/34, B 25/33, C 24/31 and End 20/25 mm. Each one still reads thicker than its stick, but as local thickening in one continuous antenna rather than version-17 beads.

THE ROOTS BELONG TO THE BODY. The front and rear roots now carry the body’s pink/grain treatment across the visible thickening, then hand gradually back to the cooler antenna surface. The selected 16/16 body-blend was chosen from complete native motion, and no root-ink exclusion was shipped.

THE FRONT CONNECTION FLEXES. Real public JunctionF X/Y curves now move the front antenna connection in the shipping motion, so the front ball and its connection are no longer a stiff body-mounted peg.

THE LIVE MANA IS CLEAN. The old creature-following history mist is off for all 22 live subjects. Hasty no longer drags its old smear, and Drift and Blown no longer carry their relic trails. Crackle now uses the normal candidate-9 day-sky presentation, with an exact legacy control preserved for review.

PARTICLES STAY HONEST. The pre-layer particle route was declined because it would flatten depth. A small 120 mm surface fade thins fold/surge motes as their disc centres meet an antenna surface, while motes genuinely in front still overlap.

THE EXPERIMENTS ARE ARCHIVED. The old Mana menu and Mana lab are out of the live tabs and live as one immutable version-17 experiment generation, with their media and provenance preserved.

FLIGHT AND TRICK ARRIVE IN THE FINAL WAVE. The version-18 Flight vertical phrase and the planted Trick 360 correction are still being finished; their final picture and provenance will be added with the final render.

PROVENANCE. [final bank provenance — fill after the final render]
```

Blurb sourcing, not part of the site copy: ball sizes / 400 family [V18-SWELL-FRONT-ART.md:14,25-29; V18-SWELL-FRONT-REPAIR.md:104,108]; root blend / no ink exclusion [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43,62-67,71-75]; front flex [V18-ROOT-AUTHORITY-IMPLEMENTATION.md:18-24,42; V18-SWELL-FRONT-ART.md:33-44; V18-SWELL-FRONT-REPAIR.md:15,94]; live mist / Hasty / Drift / Blown / Crackle [V18-WAVE-E-CLEANUP.md:8,16,22,30-32,42-54]; particle fade [V18-WAVE-E-CLEANUP.md:58-74]; archive [V18-ARCHIVE-CLOSURE.md:10-14,21-31,43]; Flight/Trick pending [V18-WAVE-E-CLEANUP.md:5; V18-SWELL-FRONT-ART.md:108].

## CONTINUATION
- Done: Drafted VERSION-18-FINDINGS.md for Waves A–E and mapped all eleven Direction-19 items. Established: Wave A archived v17 with 56/56 locked files, 70,576,645 bytes, 22 live/6 menu/10 lab [V18-ARCHIVE-CLOSURE.md:13-14,27-29,61]; Wave C selected bodyblend 16/16 by eye [V18-ROOT-MATERIAL-IMPLEMENTATION.md:43,67]; Wave D selected 400 pm swells and 1500 pm Front gain, with repair closure 88/88 [V18-SWELL-FRONT-ART.md:14,44; V18-SWELL-FRONT-REPAIR.md:7,88]; Wave E removed live history mist from 22 subjects, shipped Crackle candidate 9 and 120 mm mote fade, and passed 97/97 [V18-WAVE-E-CLEANUP.md:8,22,42,74,76]. Latest source commit is Zhaozhou 0381bdec [V18-WAVE-E-CLEANUP.md:6]; latest production renderer hash is at [V18-WAVE-E-CLEANUP.md:88].
- Open: Final bank provenance, final visual verdict, production deployment, and Wave F Flight/Trick are not shown. Also open/ambiguous: Drift framing [V18-WAVE-E-CLEANUP.md:53,103], End-swell stub [V18-SWELL-FRONT-REPAIR.md:118], Trick framing [V18-SWELL-FRONT-REPAIR.md:117], fade not gate-traced [V18-WAVE-E-CLEANUP.md:104], and intermediate Wave-D commit `6f0201e3` appears only in the superseded report [V18-SWELL-FRONT-ART.md:7].
- Next chunk: When the Wave F report and final bank receipt exist, fill the Wave F placeholder, final bank provenance, site-blurb provenance, and delivery status. Inputs needed: Wave F report, final accepted bank hash/manifest, every-frame final review, and production verification/deployment receipt.
