# Manafold Pass 17 history reconciliation

**Date:** 2026-09-18
**Scope:** owner directions 1–14, Passes 6–16 plans/findings/reviews/QA, current final4 reviews, focused Manafold source/site git history
**Current accepted picture:** Pass 16 final4, 28 subjects / 11,592 frames, renderer MD5 `1255A8F8DEE778F7E76DCC7759D678B0`

## Executive result

There is enough real work for a coherent Pass 17. The pass is not “more eye scaling” and it must not re-open the accepted Pass-16 compositing/effect work. Its centre is **expression whose mechanisms are independently true in the shipping picture**:

1. re-audit and, where necessary, repair individual Front/A/B/C/End antenna authority;
2. author clearly larger and smaller eye forms while correcting the inherited thin-lens/splinter read;
3. keep Manafold's identity readable through `Trick`;
4. make `Fall` loop rather than hard-restart;
5. directly decide whether the repeatedly owed `taunt3` joke now lands, instead of mistaking “a hold exists” for comedy.

The first four are binding/confirmed. The fifth and the antenna surface finish are direct checks at the start of the pass, not assumptions.

## Ranked current scope

### 1. Antenna bones and individual ball authority — **TRUE REMAINDER: independent audit is binding; implementation defect not yet presumed**

**Owner/history evidence**

- Direction 4: “wherever there is one of these balls, there needs to be bones to bend stuff” (`OWNER-DIRECTION-4-2026-09-05.md:82–83`).
- Direction 7: “all the balls need to be able to move individually” in all directions while remaining guided hinges (`OWNER-DIRECTION-7-2026-09-06.md:13–25`), and later says only one wrong-position joint appeared to knead (`:482–495`).
- Direction 9 says none of the requested bones appeared visually and asks for the middle down / outers up configuration (`OWNER-DIRECTION-9-2026-09-07.md:55–61`).
- Direction 11 repeats that the balls still do not move independently and asks for “BIG SCRUTINY” (`OWNER-DIRECTION-11-2026-09-09.md:111–130`).
- Direction 12 again requires a real bone for every ball and visible shipping-bank articulation, not solo capability (`OWNER-DIRECTION-12-2026-09-16.md:11–23`).
- Direction 14 explicitly revokes inherited confidence: audit from zero; every visible ball must own and visibly move independently in production; each carrier needs a failing control (`OWNER-DIRECTION-14-2026-09-18.md:15–22`).

**Current final4 evidence**

- Pass-16 final review sees staggered multi-bend Taunt poses, different Curious configurations and attached rear sockets (`FINAL4-BATCH-01.md:20–22`, `FINAL4-BATCH-04.md:8–14`, `FINAL4-BATCH-07.md:8–14`). This is evidence that the combined rig is live.
- It does **not** isolate each of Front/A/B/C/End in a public performance. Pass-16 findings list mute controls for Front/A/End, not B and C (`PASS-16-FINDINGS.md`, “Failable evidence”). Direction 14 therefore asks a question Pass 16's evidence set does not fully answer.
- Current source is plausible but non-trivial: `HingePlay` has separate neck/A/B/C/End channels (`manafold_clips.h:256–262`), yet nodule centres A/B/C are moved by the *preceding* carrier and child translation (`manafold_clips.h:369–416`). Skin ownership is similarly staggered (`manafold_model.h:229–259`). A name-level bone census is not proof.
- Source prose itself is stale and contradictory: `manafold_rig.h:1` says 12 bones while `kBoneCount` is 16 at `:125`; `:23–24` says re-entry still has no joint while `:118–123` adds RearSocket/ReturnTip carriers. This is exactly why the audit must trace data rather than trust comments.

**Pass-17 acceptance**

- Trace bind -> clip authoring -> pose decode -> skin weights -> deformed visible core for Front/A/B/C/End.
- For each carrier, separately mute the production channel and require a named public-clip visual/trajectory gate to fail. B and C need their own controls; a Front/A/End-only set is incomplete.
- Show all five in public final-resolution phrases, including opposed configurations; a solo diagnostic is supporting evidence only.
- Preserve one continuous non-beaded skin, the straight C return, surface-following rear socket and buried tip.

### 2. Eye form and size acting — **TRUE REMAINDER**

**Owner/history evidence**

- Eyes are the expressive centre and should exceed Zixxtrixx (`OWNER-DIRECTION-1-2026-09-04.md:7–15,44–54,107–118`; `OWNER-DIRECTION-2-2026-09-04.md:68–92,118–128`).
- Direction 4 asks for a shaped, pointed lens rather than a simple primitive (`OWNER-DIRECTION-4-2026-09-05.md:116–145`). Later Direction 7 says the basic modelling was liked, but its positioning and two-part life still needed work (`OWNER-DIRECTION-7-2026-09-06.md:720–786`).
- Pass-14 visual comparison finds the current face a long asymmetric dagger rather than the concept's fatter almond, with a star too small for its length (`PASS-14-REVIEW.md:56–85`). Pass-15 repeats the current constants and the white-splinter failure (`PASS-15-REVIEW.md:90–122`).
- The present source still ships `kEyeLongMm=270`, `kEyeWideMm=84` (3.2:1) (`manafold_art.h:950–960`). Commit `6c2fc2fb` is explicit that the surface-follow ladder does **not** fix the splinter.
- Direction 14 now makes the next action unambiguous: visibly grow and shrink the eyes for expression and do not blindly scale the inherited fault (`OWNER-DIRECTION-14-2026-09-18.md:20`).

**Current final4 evidence**

- Final4 establishes a protected baseline: two-eye/head read survives all reviewed clips and body energy (`FINAL4-BATCH-01.md`, `03`, `04`, `06`, `07`). It does not demonstrate a large/small size performance because that channel does not exist yet.
- Existing blink/squint, travel, star+white parenting and expression lean are real and should be retained: `blink_at` is used throughout the bank (`manafold_clips.h:1063–1072` and clip call sites), travel carriers rotate the child frame, and `kEyeExpressLeanA16` ships (`manafold_art.h:1481–1506`).
- `kEyeSurfaceFollowPm=0` is a documented taste choice, not automatically the fix (`manafold_art.h:1494–1535`). Do not turn it merely because zero looks suspicious; judge oblique eyes in the new size ladder.

**Pass-17 acceptance**

- Named, editable per-eye form/size channels produce plainly larger and smaller eyes in authored public beats, with asymmetry allowed.
- Correct the long empty violet points/white-splinter read by eye before scaling the form.
- Star and white remain one rigid unit, stay legible at travel extremes and do not contact each other or disappear into the bouncing body.
- Review native 240p front, three-quarter and oblique extremes; preserve blink, travel, roll and camera-relative readability.

### 3. `Trick` loses the creature's identity during its central rotation — **TRUE REMAINDER**

**History/current evidence**

- Pass-16 findings deliberately left the pre-existing rotating-body aspect outside Directions 12/13 (`PASS-16-FINDINGS.md`, “Independent-review correction loop”).
- Final4 native review confirms roughly f115–f295 is a broad, nearly featureless pink rear/back mass while face and antenna identity rotate away (`FINAL4-BATCH-07.md:20–28`). It is mechanically sound but artistically suppresses the only face for too long.

**Pass-17 acceptance**

- Keep the headstand/comedy and authored ground contact; re-author body/camera/pose timing so face and antenna identity remain readable through the central phrase.
- Full every-frame sheet plus native key grid; no “fix” that merely shortens the evidence crop or removes the clip.

### 4. `Fall` hard-restarts at the loop boundary — **TRUE REMAINDER**

**History/current evidence**

- Travelling wrap faults recur through Passes 13–14; Pass 16 fixed Hasty/Flight but explicitly did not pull historical Fall into scope (`PASS-16-FINDINGS.md`).
- Final4 enlarged seam review proves f338 is grounded recovery and f339 jumps to the high opening pose, close to f0000 (`FINAL4-BATCH-03.md:8–12,26–28`).

**Pass-17 acceptance**

- Author a continuous end->start loop or a deliberate non-loop presentation contract; no high-pose teleport.
- Inspect the last 24 + first 24 frames at enlarged and native scale. Preserve the accepted descent, contact and recovery.

### 5. `taunt3` comedy / bank-level expressiveness — **NEEDS DIRECT CHECK, likely remainder**

**History evidence**

- “Taunt can be more fun” and theatrical taunts recur in Directions 3, 5 and 9. The end-of-Pass-12 inventory still says `taunt3` is not funny (`OWNER-INVENTORY.md:726–738`).
- Pass-14 review distinguishes measurable holds from a joke and identifies non-extreme holds, a collapsed loop at the punchline, continuing yaw and a dead middle (`PASS-14-REVIEW.md:189–212`).
- Commit `1e9aa9a3` made it a performance, and Pass 16 final4 confirms continuous articulation and no lightning bars (`FINAL4-BATCH-07.md:16–18`). That review did **not** say the joke lands.
- The old cross-creature requirement that Manafold exceed Zixxtrixx in expression was explicitly refuted after Pass 13 (`OWNER-INVENTORY.md:726–738`) and has not had a fresh side-by-side after Pass 16.

**Direct check / acceptance**

- Before editing, compare final4 `taunt3` and Taunt against Zixxtrixx's accepted taunts at native scale and answer one question: is there an unmistakable comic extreme/punchline?
- If no: re-author one extreme silhouette, a clean arrival/hold/exit and a front-readable face; coordinate it with the new eye-size and independently moving balls rather than creating a separate grab-bag task.
- Final Pass-17 review includes the side-by-side; component motion alone is not evidence of greater expression.

### 6. Antenna surface finish / taxonomy — **NEEDS DIRECT CHECK inside item 1, not independent scope yet**

- Direction 9 asked for more distinct antenna texture and further front/mid slimming (`OWNER-DIRECTION-9-2026-09-07.md:21–29`). Pass 12 records the third front-lobe cut, 12% mid-band thinning (`PASS-12-FINDINGS-A.md:204–208`) and a stronger distinct antenna grain (`PASS-12-FINDINGS-B.md:347–364`).
- Pass-14 review later called the close-up a corrugated vacuum hose and noted a ribbed base seam (`PASS-14-REVIEW.md:77–89,290–300`). Pass-16 final4 says the skin stays continuous and attached but did not make a native texture/smoothness comparison.
- Include one native/4x surface plate in the Direction-14 audit. If the hose/seam still reads, fix it; if not, mark closed with the picture. Do not assume the old report still describes final4.

## Closed or superseded — do not re-open without new final4 evidence

| Historical item | Current disposition / evidence |
|---|---|
| Lightning over flesh | **CLOSED P16.** Hover/Inspect show nearer antenna/body interrupting all layers (`FINAL4-BATCH-04.md:8–14`). |
| Frame-history smear | **SUPERSEDED by D12, CLOSED P16.** All live variants reviewed smear-free; Hasty/Flight centred (`FINAL4-BATCH-03.md:14–20`). |
| Folded lightning variety / 360 turns / particle independence | **CLOSED P16.** D13 implementation `db9f4d5f`; Channel/Rest/Cyan/Mana Aqua native reviews show connected changing figures with independent motes (`FINAL4-BATCH-01/04/05/06`). |
| Taunt-III lightning bars / Channel fused moment | **CLOSED for final4.** Suspected bars specifically do not reproduce (`FINAL4-BATCH-07.md:16–18`); current site prose was corrected. |
| Rear socket stuck inside / straight return / body-follow | **CLOSED visually for P16; re-audited structurally under D14.** Every batch reports attachment intact; P16 source adds RearSocket/ReturnTip. Do not treat the old stuck-ball picture as current. |
| Ordinary Lasso has no mana | **CLOSED P16.** Both ordinary and Mana lasso show release/flight/catch/return without white-bar collapse (`FINAL4-BATCH-04.md:16–18`; `07.md:12–14`). |
| Shell opaque/narrow; body/head line missing | **CLOSED P16.** 800/750 transmission and post-energy contour survive final4 (`FINAL4-BATCH-04.md:8–14`; `06.md:12–14`). |
| Hasty/Flight disappear at wrap | **CLOSED P16.** Centred, clean loops (`FINAL4-BATCH-03.md:14–20`). |
| Deaths resurrect / lack drop / gutter teleport | **CLOSED.** Final4 deaths are continuous into stable corpse holds (`FINAL4-BATCH-02.md:12–18`). |
| Blown missing launch / unreadable apex | **CLOSED in final4.** Complete launch/tumble/descent/recovery remains framed (`FINAL4-BATCH-01.md:8–10`). |
| Blink never shipped | **CLOSED since P12.** `blink_at` is live across clips; do not rebuild blinking as the new eye-size channel. |
| More shapes; rotate/stretch/knead | **CLOSED P16/D13.** Twelve figures, topology crossfade and occasional full turns; protect the approved swirl. |
| Eye travel “does not move” / fixed-camera slot regression | **CLOSED by P15 EYE/EYE2 (`5a44af35`, `c269b448`, `6c2fc2fb`).** Preserve travel and subject-aware camera bake while changing form/size. |
| Front-lobe/mid-band sizing and distinct antenna pigment | **IMPLEMENTED P12.** Only current visual finish remains to check; do not mechanically retune old numbers. |
| Alternate Mana-menu mechanisms do not all draw the standard figure | **NOT A DEFECT.** Final4 explicitly accepts the alternate fill/boil mechanisms (`FINAL4-BATCH-05.md`). |
| Hardware cost / lightning primitive / persistence engine | **SEPARATE HARDWARE LANE**, not Pass 17 (`OWNER-INVENTORY.md:637–649`). |

## Contradictory or stale prose — separate from creature defects

1. **Source header is stale:** `manafold_rig.h:1` says 12 bones but `:125` says 16; `:23–24` says re-entry has no joint while `:118–123` adds RearSocket/ReturnTip. Correct during the audit so future investigators do not inherit two architectures.
2. **Five-carrier vs three-nodule vocabulary:** the current card's “Nodule taunt” still describes a “three-ball shimmy” / “three antenna nodules” (`website/creatures.json:210–215`). That may correctly mean the three free upper nodules A/B/C, but Direction 14 now uses Front/A/B/C/End as five visible carriers. Clarify the distinction; do not silently rename anatomy to make a gate pass.
3. **Already corrected in the current working card:** old claims that Inspect was byte-identical to Hover, Trick was still the Pass-12 ground fault, Fall awaited Wave 2b, Taunt awaited rework, and Taunt-III/Channel bars remained live were stale. The current pending `creatures.json` edits replace those claims; keep that correction separate from Pass-17 creature code.
4. Historical archive captions may truthfully describe old generations. Do not “clean” them into current claims.

## Recommended coherent Pass-17 sequence

### A. Instrument the antenna claim before changing it

- One production ownership table generated from actual weights/pose carriers: visible core -> controlling transform -> authored clip channel.
- Per-carrier Front/A/B/C/End mute controls, each independently fired on a named public phrase.
- Effects-on native grids and trajectories; body-follow/socket/straight-return/continuous-skin red legs.
- Correct stale source comments only after the traced graph is known.

### B. One expression system, not separate eye/taunt patches

- Author a fattened, splinter-free base lens by eye, then named per-eye large/small form channels.
- Use those channels together with independent antenna poses in Taunt/Curious/Startle and, if the direct check fails, `taunt3`'s punchline.
- Preserve blink, star+white parenting, travel, roll and bouncing-body follow.

### C. Bounded clip surgery

- `Fall`: repair only the f338->f339/end-start contract.
- `Trick`: keep the headstand and contact; reframe/retime the long back-facing interval so identity remains visible.
- Do not reopen effects, shell, deaths, lasso or Hasty/Flight absent a new regression.

### D. Acceptance and publication

- Direct clean build and committed gates with fired red legs.
- One exact-generation 28-subject render; native/every-frame review in isolated image contexts.
- Side-by-side expression comparison against accepted Zixxtrixx taunts and final4 Manafold.
- Archive Pass 16 before overwrite; encode, freshness, full decode, commit, push, integrate, publish and production-byte verify.

## Protected Pass-16 wins

Protect: round bouncy body; camera-relative readable eyes, blink/travel/roll and star+white unit; continuous non-beaded antenna skin; straight return limb; body-following rear socket and buried tip; lightning depth; no live frame-history smear; broad pigmented shell transmission; inner body/head contour under energy; both lasso throws; Hasty/Flight clean centred loops; final4 deaths; folded connected lightning, twelve-shape vocabulary, topology-aware morphs, occasional full turns and independent particle field; violet-night/readable lighting; all authored ground contacts.
