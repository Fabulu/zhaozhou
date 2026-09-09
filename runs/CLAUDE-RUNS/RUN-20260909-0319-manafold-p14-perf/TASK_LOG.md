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
