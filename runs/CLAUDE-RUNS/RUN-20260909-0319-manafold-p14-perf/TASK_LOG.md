# RUN-20260909-0319-manafold-p14-perf — IMPL-PERF, Manafold pass 14

Lane: `C:\programmieren\zencrifice\manafold-p14-perf\{zhaozhou,Upheaval}`
Owned file: `zhaozhou/tools/reel/manafold_clips.h`.
`manafold_art.h` clip-constant region: NOT YET GRANTED (IMPL-FACE holds it).

Items: **R4** (taunt3 gets a joke), **R7 retime/tumble half** (blown's apex).
**BLOCKED**: R2(b) bounce, R2(c) death/idle squash — wait on FACE's 32-segment
mesh ablation.

---

## Where I am (write this BEFORE reading any long-job result — CLAUDE.md)

* [0319] Run opened. Read: Upheaval/CLAUDE.md, 07-MOTION-STYLE (§8a/§8b),
  PASS-14-PLAN §4 R4/R7/R2, §5 waves, §6 protected, PASS-13-REVIEW §2.3/§2.4/§2.5,
  09-ENGINE-GOTCHAS 0–21, 10-GATE-CHECKLIST §0 + 5–40.
* [0330] Baseline `--clean` cel build. **BUILD_RC=0** (read from the process,
  not a pipeline). md5 `54c34c91a3463f8cc5b2b59be275278f` -> `build/BASELINE.md5`.
* [0332] Rendering BEFORE plates: taunt3 (368 f), blown (292 f), trick (400 f),
  shipping env `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.

## Frame/key arithmetic, established (so nobody re-derives it wrong)

`subject_u02_clip`: `s.frames = keys * 2`. **frame = key * 2.**

| clip | keys | frames |
|---|---|---|
| taunt3 | 184 | 368 |
| blown | 146 | 292 |
| trick | 200 | 400 |

So the review's frame numbers map to keys as:

* taunt3 front-on balloon **f304–f354 = keys 152–177**.
* trick handstand **f172–f286 = keys 86–143**.
* blown's felt hang **f99–f190 = keys 50–95**.

## R4 — the reading of the source, before any edit

`build_taunt3` (clips.h:3123) already has §8a's shape ON PAPER:
antic 4–14, shrug attack 14–24 (`punch_ease`), hold 24–52 (28 keys), lean
60–100 (`fold_ease`), shimmy 100–146, flick 146–149 (3 keys, `punch_ease`),
hold 149–173 (24 keys), release to 183. Pass 13 landed §8a AND §8b (the
dismissal turns the body, `kTaunt3FlickYawA16`).

**So the beats are not the fault, and re-timing them a third time would be
gotcha §18's exact trap.** What the review measured — median frame-to-frame
motion 2.41, range 1.0–5.5, *never approaching zero* — has four named causes
that all run UNDERNEATH the holds and were never switched off:

1. `hover_at(f, K, ..., kBobAmpAMm*3/2, kBobAmpBMm, K/46, K/92)` — a **198 mm**
   bob at 4 cycles/clip plus 50 mm at 2. It never stops. On a 1.6 m body this
   alone is the motion floor.
2. `compress_at(f, K, K/46, ...)` — the breathing squash, 4 cycles, never stops.
3. `apply_twinkle(..., sinp(f, K, 2))` — the eyes drift for all 184 keys.
4. **The previous beats' TAILS decay straight through the next hold.**
   `kShrug` is still falling 220 -> 0 from key 94 to 183; `kLean` is still
   falling 150 -> 0 from key 118 to 183. Both are live under the punchline.

⚠ And the punchline's angle: `kTaunt3FlickYawA16 = 7600` (~41.7°) is held from
key ~152 to ~176 — **exactly frames 304–352, the review's front-on balloon
zone.** The clip's one held beat is parked at the creature's worst angle, and
the yaw that §8b correctly added is what puts it there. Verify by eye on the
before sheet before touching it.

## R7 — the reading of the source, before any edit

`kBlownHangKeys = 8` already, so "cut the hang" is largely done. The *felt*
90-frame hang is a different mechanism: the rise is `1-(1-t)^2` and the fall is
`t^2`, and **both are flat at the apex**. Last quarter of the rise moves 6% of
the height; first quarter of the fall the same. Rise span 38 keys, fall span 46:
so ~9.5 + 8 + ~11.5 = **~29 keys / 58 frames within 6% of peak height.**

And `kTumble` **plateaus across exactly that window** (`{apex,1000},{apex+hang,1000}`)
by deliberate pass-13 design — "the one still moment is still in rotation as
well as in height". That is precisely the fault the review names: *nothing is
happening on the apex.*

Planned mechanism: **decouple the tumble from the height.** Rotation runs
monotone across the whole flight (no plateau); make it a full revolution so it
still lands right-side up at the catch, preserving pass 12's contact fix
structurally rather than by a returning curve.

⚠ `manafold_clips.h:~3068` comment says the penetration was "at key 163" —
stale from the 196-key version (`kBlownKeys` is 146). False structural comment,
gate checklist 13. Fix it in the same commit.

## Instruments (checklist 40: a metric must be proved on a known answer)

Not building a new mask. `tools/reel/trajplot.py --bg` is the committed one and
its `selftest` proves the mask can fail on a creature-free frame. For the HOLD
question I will calibrate against a frame whose answer is already known and
independent of me: **`trick` f172–f286 must show a plateau** (the review found
the handstand hold by eye) **and taunt3-before must show none** (the review
found no hold by eye). A metric that cannot reproduce both is not used.

---

## [RESUMED 2026-09-09] Rebased onto origin/main (32-segment mesh landed)

`wip/p14-perf` rebased onto `d4d907ce`. Upheaval fast-forwarded to `e2fded0`.
**Unblocked:** R2(a) answered SEGMENTS, not normals — so R2(b)/(c) are open and
bouncing harder is safe.

## ⚠ THE CALIBRATION FAILED FIRST, AND THAT IS THE FINDING

Ran `holdmeter.py calibrate` before touching the clip (checklist 40). **It could
not separate the two clips**: zero holds in BOTH `trick` (known positive) and
`taunt3` (known negative). Had I skipped the calibration and simply run the
meter on taunt3, it would have said "no hold" — the true answer — **for the
wrong reason, and would then have said "no hold" about my fix as well.**

Two causes, both named in the tool's own docstring and neither acted on:

1. **The threshold asked the wrong question.** `0.5 x median` is "is this
   SLOWER THAN TYPICAL". trick's real 120-frame handstand sits at 0.86–1.15
   against a 0.843 line — missed by four hundredths. Fixed: the line is now a
   multiple of the **FLOOR (p5)**, the clip's own zero.
2. **The mana fold's floor.** `MANA_ABLATE=1` removes it AT THE SOURCE — an
   ablation, not a mask. With mana on, the separation survives only x1.4–x1.5;
   with it off, x1.2–x1.8. **1.5 is the middle of the robust band.**

Added `HOLD_RANGE_MIN` (peak/floor >= 4): a hold is only meaningful in a clip
that has an ATTACK. Both real clips are 9.1; every degenerate signal is <= 2.2.
It does **not** gate out the negative — taunt3 is 9.1 and is judged on its runs.

**Calibrated result:** trick holds at frames **155–198, 240–257, 265–286**
(the review's by-eye window is f172–286); taunt3 **0 holds in 367 pairs**.

## Where I am
* Meter trustworthy. Next: read `build_taunt3`, author the beats, `--clean`
  build, render taunt3 + trick + blown, re-measure, contact-sheet every frame.

## [R4 landed] and where I went next

R4 shipped: two holds where there were none (frames 51-80, 320-336), the
punchline is a squat turned-away pose instead of an edge-on stub. R2(b) and R7
rode the same build. Pushed as `8e0853c6`.

⚠ **The first push "succeeded" and had been REJECTED.** `git push -q ... | tail;
echo $?` reported 0 -- that is `echo`'s status, and the rejection hints were
right there in the output. CLAUDE.md's "read the build's exit code, not the
pipeline's" is not only about builds. Redone with `--force-with-lease` (the
branch had been rebased) and a real exit code.

## R2(c) -- and the corpse's fault was one line

`if (dead) c.deform[f] = zc::DeformSample{}` -- **bit zero**. The intent was
D9 SS11.2's eternal rest and the comment says "⚠ THE DEFORM STOPS", which is
right. But **zero flatten is not stillness, it is the round BIND POSE**: the
animal inhaled to 16500 while alive and then died into a perfect ball. The
comparison's "a slightly smaller, slightly lower blob" was that, exactly.

The corpse holds an authored sag now. Three things that cost time and are worth
having written down:

1. **The corpse-zero rule lived in TWO places.** Fixing `manafold-qa-p12` left
   `manafold_probe.cpp` failing on its own copy -- the identical fault CLAUDE.md
   records against `flat_staged_slot`, in the same file, again. Both now read
   `u02::corpse_sample()`, so they can disagree only by failing to compile.
2. **I broke the `--fail-lane` leg and the leg told me.** Substituting lane 0's
   DATA while keeping each lane's own RULE judged a held sag against a bit-zero
   rule; the leg reported faults and its self-check said "the leg did not take
   effect". The leg must take lane 0's criterion too. Found by running it.
3. **1350 was too timid and only the picture said so.** 34% flatten is visible
   on a 4x crop and invisible on the eight-tile strip -- and the strip is the
   test the comparison applies. 2400 (60%) reads.

⚠ **4 QA failures are PRE-EXISTING, not mine.** Verified by stashing the whole
change, rebuilding and re-running: lanes 2 and 3 hold a frozen span stretch
(3407 / 8781) on both deaths and the gate has been red on it. Same 4 before and
after. Not my item; reported.

## Where I am
* R4, R2(b), R2(c), R7 all authored and looked at. mprobe RC=0, all four QA
  legs behave. Next: the findings doc and the honest list of what I left.

## CLOSING STATE

**All four items shipped.** zhaozhou `b37e0910` on `wip/p14-perf` (pushed),
Upheaval `69245ba` on `main` (pushed). Both trees clean. Deliverable is
`Upheaval/creature/Manafold/PASS-14-FINDINGS-PERF.md` with 13 plates in
`pass14-plates-perf/`.

Gates at close: `mprobe` **RC=0** (clearance contract holds; blown 305 mm; both
deaths' declared eternal-rest penetration still -20 mm against a declared -25).
`manafold-qa-p12` **RC=1 with the SAME 4 failures as before pass 14** (lanes 2/3
frozen span stretch on both deaths), proved pre-existing by stashing and
rebuilding. All four QA failable legs behave. `holdmeter selftest` OK and the
calibration separates.

⚠ **Two loose ends for whoever picks this up**, both in the findings §8:
the full expressiveness PAIR PLATE was not re-rendered (only the numeric half),
and the post-mortem lamp lead -- which §1a shows is real -- is unopened and
belongs to REEL.

**No process of this lane is running.** Verified by name sweep at close; nothing
was ever killed by image name (CLAUDE.md's identify-before-you-kill), and the
`quartus_map` that was alive at open was never touched.
