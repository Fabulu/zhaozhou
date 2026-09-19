# Manafold version 18 — clip, mana, smear and site recon

**Date:** 2026-09-19
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md`
**Baseline:** accepted version-17 renderer `67DCAFF8ABC0AA2BA830C439A4CD98C7`
**Verdict:** **The requested clip/site faults reproduce. Hasty's visible “smear” is not the retired smear preset; it is the still-live persistent mist buffer. Drift and Blown already use the normal candidate-9 mana, so their relic read shares that same speed-driven trail. Crackle is the one genuine old mana subject: candidate 4 plus the special night backdrop.**

## Baseline pictures

Complete version-17 contact sheets and native witnesses were reviewed before source conclusions:

- `V18-RELIC-MANA-BASELINE-NATIVE.png` — Drift, Crackle, Blown and Hasty;
- `V18-HASTY-TRAIL-4X.png` — the blocky cyan/grey history around Hasty;
- `V18-FLIGHT-BASELINE-NATIVE.png` — repeated high/low Flight witnesses;
- `V18-TRICK-BASELINE-NATIVE.png` — approach, 180-degree plant/pause and return.

The pictures show:

- Drift and Blown carry broad pale cyan history around the otherwise current connected figure;
- Hasty has the same chunky wrong-frame-buffer tail the owner calls the old smear;
- Crackle is visually isolated on a dark-violet showcase backdrop with sparse free lightning rather than the ordinary fold-plus-strand substance;
- Flight does move vertically, but four short repeated bobs make the displacement read as ordinary bounce/pitch rather than an unmistakable flight phrase;
- Trick currently reaches 180 degrees, holds one planted orientation, and reverses the same turn during recovery. It has no planted full spin.

## Exact mana subject graph

### The ordinary shipping mechanism

`subject_u02_clip()` assigns candidate **9** to every non-diagnostic Manafold clip (`tools/reel/zhao_reel.cpp:5852-5879`). Candidate 9 is the current Channel stack: `mana_fold(... kRampAqua ...)` plus the continuously living free lightning strand (`tools/reel/manafold_fx.h:3728-3739`). Every ordinary subject also sets the retired `u02_smear` preset to zero.

Each subject still passes its own clip slot to `mana_fill()` (`zhao_reel.cpp:4506-4534`). The slot salts figure choice, path/mote identity and timing through `fold_phase()` / lightning hashes, while the clip's actual key count keeps the clocks periodic. Thus “normal mana” means the same candidate-9 mechanism, not byte-identical figures across every action.

### Drift — already candidate 9

`manafold-drift` is a plain `subject_u02_clip(slot 1, ...)` with no mana override (`zhao_reel.cpp:8503`). It already receives candidate 9 and `u02_smear=0`. Its special properties are travelling staging/camera and its slot-seeded fold/lightning choreography.

The conspicuous relic read in the baseline is therefore **not an old candidate**. It is chiefly the persistent creature-relative mist plane being speed-fed and lagged around the moving/rotating creature. Do not invent an effect-seed override first. The narrow first A/B is ordinary candidate 9 with the live mist plane disabled; only if the remaining fold itself still reads wrong should a canonical effect-salt comparison be added.

### Blown — already candidate 9

`manafold-blown` also calls ordinary `subject_u02_clip(kBlownSlot, ...)` and changes only framing/note (`zhao_reel.cpp:5836-5847,8671-8682`). It already uses candidate 9 and no frame-history smear preset. Its launch/root speed drives the same mist plane to its strongest broad trail, which is plainly visible through the airborne phrase.

Again, the minimal normalisation is removal of the persistent mist trail, not changing candidate 9. Preserve the independently streaming antenna and the current fold/strand continuity.

### Crackle — genuinely separate old presentation

`manafold-crackle` explicitly replaces candidate 9 with candidate **4** (`zhao_reel.cpp:8860-8869`). Candidate 4 is free lightning only (`manafold_fx.h:3725-3727`), with no folded connected figure. It also calls `u02_backdrop()`, retaining the dedicated dark-violet night presentation.

This is the one exact match for “relic with weird mana.” The narrow version-18 normalisation is a same-binary comparison of:

1. current candidate 4 + night backdrop (exact control);
2. ordinary candidate 9 on the ordinary shipping backdrop;
3. candidate 9 while retaining night only if the owner-facing read remains genuinely better.

Start with option 2 as the requested normal mechanism; do not preserve the backdrop merely because its comments are long.

## Hasty: the visible smear is the mist history buffer

`subject_u02_clip()` unconditionally sets `s.u02_smear = 0` for every live clip (`zhao_reel.cpp:5867-5879`), and Hasty has no override (`:8509`). The retired 96x60 smear compositor is gated by a preset whose gain is zero (`zhao_reel.cpp:3687-3796`), so the owner is not seeing that path.

The visible blocks come from the separate **48x30 persistent mist plane**:

- every live clip enables `s.u02_mist` except the form diagnostic;
- the buffer persists/decays, shifts by only 82% of creature screen motion and deliberately leaves an 18% lag (`manafold_fx.h:1526-1596`);
- it uses 8x blocks, jitter, hard clears and `keep_pm=420` (`:1587-1655`);
- feed and composite strength are both multiplied by posed-root speed (`zhao_reel.cpp:3598-3671`; `manafold_fx.h:1774-1804`).

Hasty no longer travels in X, but its deep vertical bob and pitched body move the posed body anchor every frame (`manafold_clips.h:2619-2657`). That motion feeds the persistent mist and leaves exactly the blocky cyan/grey tail visible in `V18-HASTY-TRAIL-4X.png`.

Owner Direction 19 supersedes the old praise quoted in the mist comments: “we're getting rid of that everywhere.” The structurally honest version-18 fix is to retire `u02_mist` on **all live Manafold subjects**, not rename it or tune Hasty alone. Preserve:

- the version-17 body **contour mist/shell** (`u02_shell`), which is a different non-history material layer;
- current particles and candidate-9 mana;
- mist diagnostic/archived experiment subjects as provenance, with explicit controls.

A same-binary live-mist `on|off` selector should reproduce the old trail. The normal gate should assert no live subject allocates/feeds either history plane; diagnostic/archive subjects may remain reachable by name.

Removing this plane is also the first and cheapest correction for Drift/Blown's relic read.

## Flight: why the vertical phrase under-reads

Flight already owns one coherent clock (`manafold_clips.h:4525-4628`):

- `kFlightBobAmpMm = 300`;
- `kFlightBobPeriodKeys = 44` over a 176-key clip, therefore four complete bobs;
- the same clock drives root Y, pitch derivative, breath and lagged A/B/C motion;
- X travel was retired, so the camera no longer has lateral motion to distinguish Flight from a stronger hover.

The native baseline shows real high/low positions, but four relatively short repetitions plus large pitch/breath/effect changes distribute attention across many signals. The motion reads as a repeated bounce in place rather than one or two broad ascents/descents whose subject is flight.

Safe same-binary ladder dimensions:

1. amplitude only: current plus several larger named `kFlightBobAmpMm` rungs;
2. cadence only: four cycles versus two or three integer cycles across the clip;
3. only after those are looked at, combine the visually best amplitude/cadence.

Do not pick either value from pixel displacement. Review all 352 frames at native resolution and protect:

- exact first/last pose/effect seam;
- frame containment at high and low extremes;
- derivative-locked pitch and breath;
- lagged independent carrier motion and signed spans;
- no live mist/smear history.

A committed 3D root-Y trace may verify the selected amplitude/cycles, but the moving picture chooses them.

## Trick: source-legible 180, pause, planted 360, overshoot, correction

### Current version-17 phrase

`build_trick()` has 200 keys (`manafold_clips.h:2927-3028`):

- keys 42..78 ease the root to the pure-X 180-degree contact turn;
- keys 78..148 hold that same half-turn at `kTrickPlantRootMm`;
- keys 148..186 reverse the half-turn with the existing small overshoot/recovery;
- a continuous +90-degree local face yaw follows the flip envelope so both eyes remain readable while upside down;
- the old show-off yaw is parked at zero except for a small sinusoidal diagnostic control;
- the contact declaration covers keys 78..<156 (`manafold_art.h:2698-2757`, `manafold_probe.cpp:106-122,193-273`).

### Recommended version-18 interpretation

The physically coherent reading of the new direction is **not** another pitch turn from 180 to 360. Continuing the pure-X pitch would put the loop peak above the body at 360 and require the body root below ground to preserve the same support.

Instead:

1. keep the accepted pure-X 180-degree plant by key 78;
2. retain a readable pause at 180;
3. while still inverted, make one full **yaw spin about the planted vertical/contact axis**;
4. overshoot that yaw slightly, correct to exactly one turn;
5. then use the existing pure-X righting/recovery.

A yaw spin preserves the support point's vertical coordinate on flat ground and reads as the requested show-off 360 while planted. The existing dead `showoff yaw` channel is the natural source owner, but replace its small sine with a named monotone C2 key table; do not overload `kFlip` or the stable face correction.

Proposed authoring structure (timings remain knobs and require a full-motion ladder):

- plant at 78;
- pause with spin yaw 0;
- C2 advance spin yaw monotonically through 65536 during the planted hold;
- small named overshoot above 65536;
- C2 correction back to exactly 65536 (orientation identity) before key 148;
- existing righting begins only after the spin correction is complete.

The local +90 face yaw remains tied to the inversion envelope. During the full yaw it should naturally show the face, sides and rear, then return to the accepted readable face when the 360 correction reaches identity. Do not camera-track the face through the spin.

Required controls/gates:

- same-binary version-17 no-spin control;
- spin gain and overshoot/timing selectors with strict parsing;
- unwrapped yaw progress trace proving a full monotone revolution plus only the authored overshoot correction;
- carrier/effect angular step/accel/jerk and all 31/16 existing controls;
- exact contact throughout the planted window and frame containment;
- strengthen the contact instrument to identify the antenna support region, not merely “some mesh vertex is deepest,” before citing it under a rotating planted pose;
- native/4x complete 400-frame comparison for face, antenna support, plant, spin and recovery.

No clip-length increase is structurally required; the current 70-key planted interval has room for a pause, spin and small correction, but timing is an art ladder, not a conclusion from key count.

## Site archive contract

The current live collections are:

- Mana menu (`website/creatures.json:64-111`), six items;
- Mana lab (`:267-332`), ten items.

`assemble.py` separates `archive:true` records from live tabs, groups them by `archive_generation`, and renders archived collections with `autoplay=False`, `preload="none"` (`website/tools/assemble.py:135-178,276-304`). A looping archived WebM therefore gets **controls + loop + muted**, never autoplay.

Minimal manifest move:

- add `"archive": true` to both collection objects;
- give both the same new generation label, e.g. `"Version 17 experiments — 2026-09-19"`;
- update the archive introduction from twelve to thirteen generations;
- keep the item captions/notes and media bytes;
- do not clone either object as a second live/archive declaration.

Before version-18 media overwrites anything, coordinate this with the normal full version-17 archive step: the six menu WebMs/posters must be copied once to immutable version-17 archive names and the archived menu items repointed there. The ten lab files are not part of the current 28-subject encode and will not be overwritten, but may remain on their existing paths or be renamed once for consistent provenance. The final version-17 archive generation should contain each clip exactly once—never a complete-bank copy plus a duplicate menu collection.

This data-only move stays within `MAX_ARCHIVE_GENERATIONS = 19`; it reduces live-tab count by two and cannot break the 40-tab wiring. `checkmedia.py` must still decode every archived file, while `checkfresh.py` checks only subjects present in the version-18 scratch bank.

## Protected contracts

Version-18 clip/site work must retain:

- candidate-9 persistent figure/strand identity and all 16 effect controls;
- no active 96x60 smear plane, and after this pass no live 48x30 mist history plane;
- contour shell/mist as the creature's material layer;
- clip-periodic clocks, held tails and loop seams;
- Drift/Blown/Hasty motion identity while normalising presentation;
- Flight framing, independent carriers and effect continuity;
- Trick antenna contact, readable version-17 face correction, fixed camera and recovery;
- archived menu/lab media and provenance;
- exact `noindex, nofollow`, playback, freshness and full decode gates.

## Recommended wave order

1. **Site archive first:** preserve/repoint version-17 menu bytes, mark menu/lab collections archived, run assemble/playback/decode; no production render dependency.
2. **History-plane retirement + mana normalisation:** add a strict live-mist control, disable the persistent mist on all live subjects, move Crackle to candidate 9/ordinary backdrop, and render Drift/Crackle/Blown/Hasty A/Bs before touching their motion.
3. **Flight art ladder:** amplitude and cadence independently, then combined selected rung; complete 352-frame review.
4. **Trick architecture/instrument:** full-yaw table and support-region contact check before timing art; then render pause/spin/overshoot ladders.
5. Merge these only after root/carrier waves from `V18-ANTENNA-ROOT-RECON.md` settle, then run one integrated exact bank.

No renderer architecture, new mana candidate, collision system or Quartus fit is justified by this recon.
