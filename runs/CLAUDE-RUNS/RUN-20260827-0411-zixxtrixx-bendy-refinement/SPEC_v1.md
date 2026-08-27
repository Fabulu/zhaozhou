# SPEC v1 — Zixxtrixx bendy refinement (2026-08-27)

REFINEMENT PASS, not a rebuild. Preserve: S posture, colour layout (blue
face/underside+throat, pink crown, pink dorsal stripe, side eyes), committed
pose probe, ground-contact discipline (idle/walk), ~15 deg showcase camera.

Theme: EVERYTHING ABOUT THIS CREATURE IS BENDY. The walk's caterpillar wave is
the reference. Not a spazz: slow, loose, continuous.

## Shape
- Grounded run longer (3 -> 6 segments), neck shorter (6 -> 4), S exaggerated
  (steeper hook, deeper doubling-back). kStanceSlope + index knobs.
- Upper part slimmer: kTaper front entries reduced ~15%.
- Middle tail prong longer: kSpikeLen 190 -> 280.
- kBodyY retuned by probe after stance change.

## Colour
- TWO greens: dark front (measured bottom-curve/neck band ~ (67,191,105), art
  directed for the read), light everywhere else (~existing 120,184,68 family).
  Body tile: dark band from the throat side over the front rows, fading by
  mid-body. Head tile rear flanks: dark near throat, light near crown.
  Blades: light.

## Face (added feedback)
- Eye: DELETE the invented orange socket. The orange is the drawn pupil: a
  top-to-bottom line swelling in the middle, on the yellow disc. Lift it
  whole from Side.png.
- Googly: kEyeBulgeNum up so orange reads left+right from the FRONT.
- Mouth bigger/more visible.
- Verify with head zooms from BOTH side and front.

## Animation
- apply_stance gains an optional per-segment slope-delta wave; returns the mm
  of root rise needed to keep the grounded belly fixed (sin-sum compensation,
  probe-verified).
- IDLE: stronger S (table), front wave (slow travelling up-down through neck +
  dive lobe), slight head side-to-side yaw, deeper breath bob.
- WALK: keep gait; same front wave + head participation; root comp per key.
- ATTACK: retime to kAttackKeys=220. Higher apex (lift ~5600), forward jump
  (kFwd ~1900 mm), impact DIAGONAL 30 deg from vertical (spin 3333), deep
  authored stick (-420 mm) held 150 keys = 300 frames = 5.0 s at the site's
  60 fps, then extract + regather to close the loop. Screen shake amplified
  ~40x (old values were ~0.3 px: sub-pixel by arithmetic).
- FALL: lateral travelling wave (quat_y) along the whole spine, slow, head
  strongest; keep tumble/rotation.

## Camera
- Attack: TRACKING camera. World-translate by -(fwd, lift*0.85) evaluated
  from the attack's authored flight-path curves (exported as file-scope
  tables), applied by mat4_mul after cam_pitch. Sky untouched (translation
  does not move an infinitely distant sky).

## Evidence
Contact sheets + clip_report per clip before/after; probe numbers for all
four clips; head zoom side+front; two-green check on renders.
