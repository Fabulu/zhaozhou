#!/usr/bin/env bash
# Pass-23 gate matrix: the pass-22 matrix, + the two repaired gate legs of
# Direction 24 (R6's LINE far leg, R7's per-clip floor), their controls, the
# per-clip press-depth selectors, and an identity ladder that now has a
# PASS-22 rung as well as the pass-21 one.
# Usage: gatematrix_p23.sh <bindir> <logdir> <out.txt>
B="$1"; L="$2"; OUT="$3"; mkdir -p "$L"; : > "$OUT"
REPO=/c/programmieren/zencrifice/manafold-p16/zhaozhou
R="$B/zhao-reel-cel.exe"
export ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross
run() { # id expected_rc cmd...
  local id="$1" exp="$2"; shift 2
  "$@" > "$L/$id.log" 2>&1; local rc=$?
  local tail; tail=$(grep -v '^\s*$' "$L/$id.log" | tail -1 | cut -c1-200)
  local extra; extra=$(grep -m1 -o 'attributed detector fired.*\|MUTANT[^|]*' "$L/$id.log" | tail -1 | cut -c1-160)
  local st=FAIL; [ "$rc" = "$exp" ] && st=PASS
  echo "$st $id rc=$rc exp=$exp | ${extra:-$tail}" >> "$OUT"
}
runmask() { # id expected_mask cmd...  (RC 1 and the exact declared mask)
  local id="$1" m="$2"; shift 2
  "$@" > "$L/$id.log" 2>&1; local rc=$?
  local got; got=$(grep -o 'rear gate mask 0x[0-9A-F]*' "$L/$id.log" | tail -1)
  local st=FAIL; [ "$rc" = 1 ] && [ "$got" = "rear gate mask $m" ] && st=PASS
  echo "$st $id rc=$rc exp=1 mask=${got##* } want=$m" >> "$OUT"
}
crcs() { grep -oE 'manafold-[a-z0-9]+:.*crc32c=0x[0-9A-F]+' "$1" | grep -oE '0x[0-9A-F]+' | tr '\n' ' '; }
idleg() { # id expected-crcs env...   (IDLEG_CLIPS overrides the witness clips)
  local id="$1" want="$2"; shift 2
  mkdir -p "$L/$id"
  env "$@" "$R" "$L/$id" ${IDLEG_CLIPS:-manafold-hover manafold-inspect manafold-drift manafold-hasty} > "$L/$id.log" 2>&1
  local got; got=$(crcs "$L/$id.log")
  local st=FAIL; [ "$got" = "$want" ] && st=PASS
  echo "$st $id crcs=[$got] want=[$want]" >> "$OUT"
  rm -rf "$L/$id"
}
# ---- normals -------------------------------------------------------------
run n-mspan 0 "$B/manafold-spangate.exe"
run n-msmooth 0 "$B/manafold-motiongate.exe"
run n-mprobe 0 "$B/manafold-probe.exe"
run n-mjointpub 0 "$B/manafold-public-jointgate.exe"
run n-mqa 0 "$B/manafold-qa-p12.exe"
run n-mmeshcheck 0 "$B/manafold-meshcheck.exe"
run n-moutline 0 "$B/manafold-outlinegate.exe"
run n-mshell 0 "$B/manafold-shellgate.exe"
run n-mshell-selftest 0 "$B/manafold-shellgate.exe" --selftest
run n-mnodule 0 "$B/manafold-nodule.exe"
run n-meyesize 0 "$B/manafold-eyesize.exe"
run n-mrod 0 "$B/manafold-rodgate.exe" --gate
run n-mrear 0 "$B/manafold-rear-audit.exe" --gate
run n-mrear-dip 0 "$B/manafold-rear-audit.exe" --gate --dip
# ---- mrear mask controls (pass 19 / 20 / 22), each with its exact mask ----
runmask r-rear-frame 0x3 "$B/manafold-rear-audit.exe" --fail-rear-frame
runmask r-rear-joint 0x2 "$B/manafold-rear-audit.exe" --fail-rear-joint
# ! MASK CHANGED 0x4 -> 0x24 IN PASS 23, AND THAT IS THE REPAIR. The legacy
# line law breaks R3's law sweep (0x4) AND R6's real-splat census (0x20). Under
# the pass-22 binary the same mutant printed "non-dot gained distance 0" and
# "R6 DOT: 0 violations" over 11,561,258 line splats, because the far leg
# compared gate_splat_r_px's output against a second call of the same function.
# P23-RECEIPTS/p22bin-fail-line-scale.txt is that silence, recorded.
runmask r-line-scale 0x24 "$B/manafold-rear-audit.exe" --fail-line-scale
runmask r-line-flag 0x4 "$B/manafold-rear-audit.exe" --fail-line-flag
runmask r-rear-strain 0x8 "$B/manafold-rear-audit.exe" --fail-rear-strain
# ! MASK CHANGED 0x10 -> 0x50 IN PASS 23, for the same reason. Switching the
# kneading dent off drops 17 of the 19 hosting clips off R7's list; pass 22's
# R7 called that GREEN ("2 clips reach the press, best rot 36.03 deg") because
# it floored on the bank maximum. P23-RECEIPTS/p22bin-fail-no-dip.txt.
runmask r-no-dip 0x50 "$B/manafold-rear-audit.exe" --fail-no-dip
runmask r-dip-stuck 0x10 "$B/manafold-rear-audit.exe" --fail-dip-stuck
# PASS 22: the dots' law, the dots' ROUTING, and the lightning's answer.
runmask r-dot-scale 0x20 "$B/manafold-rear-audit.exe" --fail-dot-scale
runmask r-dot-flag 0x20 "$B/manafold-rear-audit.exe" --fail-dot-flag
runmask r-knead-shape 0x40 "$B/manafold-rear-audit.exe" --fail-knead-shape
# PASS 23 (Direction 24 items 2 and 3): the three new controls.
#  line-far   : the renderer stops scaling LINE splats while pass 19's law is
#               left intact -- R3's sweep stays green, R6's census must not.
#  knead-clip : slot 18 back to its pass-22 press depth, so ONE clip falls
#               under the per-clip floor while the bank maxima stay green.
#  knead-drop : the loudest clip pressed to zero, so it leaves the hosting list
#               where no per-clip floor could see it.
runmask r-line-far 0x20 "$B/manafold-rear-audit.exe" --fail-line-far
runmask r-knead-clip 0x40 "$B/manafold-rear-audit.exe" --fail-knead-clip
runmask r-knead-drop 0x40 "$B/manafold-rear-audit.exe" --fail-knead-drop
# ---- mrod controls (pass 21) ---------------------------------------------
# [!] mrod RETURNS 0 FOR A CONTROL RUN, BY DESIGN -- a control is an instrument
# demonstration, not a gate failure, and pass 21's own ctl-*.txt receipts record
# RC=0 beside "the control FIRED". Asserting rc=1 here was wrong, and asserting
# rc=0 alone would be worthless: a control that silently stopped firing would
# then PASS. So this leg reads what the gate actually claims -- the control
# fired AND its observed failure set matched its declared one.
runctl() { # id cmd...
  local id="$1"; shift
  "$@" > "$L/$id.log" 2>&1; local rc=$?
  local st=FAIL
  if [ "$rc" = 0 ] && grep -q "the control FIRED" "$L/$id.log"      && grep -q " -> ATTRIBUTED" "$L/$id.log"; then st=PASS; fi
  local line; line=$(grep -m1 -o "CONTROL.*control FIRED.*\|expected=0x[0-9A-Fa-f]* observed=0x[0-9A-Fa-f]* -> [A-Z]*" "$L/$id.log" | tail -1 | cut -c1-160)
  echo "$st $id rc=$rc exp=0+FIRED+ATTRIBUTED | ${line}" >> "$OUT"
}
for c in rig-pass20 rod-twist ball-blend joint-step; do
  runctl d-mrod-$c "$B/manafold-rodgate.exe" --gate --fail-$c; done
# the three retired names are a HARD ERROR, not a silent alias (pass-21 repair)
for c in rod-bend rod-flicker rod-uniform; do
  run d-mrod-retired-$c 2 "$B/manafold-rodgate.exe" --gate --fail-$c; done
# ---- mspan controls ------------------------------------------------------
for s in F-A A-B B-C C-E; do run s-rigid-$s 1 "$B/manafold-spangate.exe" --fail-rigid-span $s; done
for s in F-A A-B B-C C-E; do run s-clamp-$s 1 "$B/manafold-spangate.exe" --fail-clamp-negative $s; done
for s in F-A A-B B-C C-E; do run s-over-$s 1 "$B/manafold-spangate.exe" --fail-overcompact $s; done
for s in A B C E; do run s-drift-$s 1 "$B/manafold-spangate.exe" --fail-delta-drift $s; done
for f in e-start e-mid e-presocket posed-order order antenna-snap accent-switch hold-tremor compress-wrap final-dwell root-authority front-flex swell-size terminal-cap walk-pairing dent-pin; do
  run s-$f 1 "$B/manafold-spangate.exe" --fail-$f; done
for m in F A B C E; do run s-mute-$m 1 "$B/manafold-spangate.exe" --fail-mute $m; done
# ---- msmooth controls ----------------------------------------------------
for f in lightning-switch shape-blackout particle-reseed surge-reseed loop-seam mote-count mote-role weight-wrap morph-reverse brightness-seam stamp-count final-dwell mote-visibility palette-raw-clock palette-hard-switch death-effect-cutoff; do
  run m-$f 1 "$B/manafold-motiongate.exe" --fail-$f; done
# ---- protected legs ------------------------------------------------------
for m in F A B C E; do run p-mjoint-$m 0 "$B/manafold-public-jointgate.exe" --fail-mute $m; done
for m in F A B C E; do run p-mnodule-$m 0 "$B/manafold-nodule.exe" --fail-mute $m; done
run p-mnodule-ignore 0 "$B/manafold-nodule.exe" --fail-ignore
run p-mprobe-mirror 1 "$B/manafold-probe.exe" --fail-mirror
run p-mprobe-outline 1 "$B/manafold-probe.exe" --fail-outline
run p-mprobe-scale 1 "$B/manafold-probe.exe" --fail-scale-inverse
run p-mprobe-support 1 "$B/manafold-probe.exe" --fail-trick-support
run p-mprobe-support-depth 1 "$B/manafold-probe.exe" --fail-trick-support-depth
run p-moutline-owner 1 "$B/manafold-outlinegate.exe" --selftest-no-opening-owner
run p-moutline-repaint 1 "$B/manafold-outlinegate.exe" --selftest-force-post-repaint
for c in lane seam eyesnap startle-step rootstep fall-wrap; do run p-mqa-$c 0 "$B/manafold-qa-p12.exe" --fail-$c; done
run p-meyesize-L 0 "$B/manafold-eyesize.exe" --fail-mute L
run p-meyesize-R 0 "$B/manafold-eyesize.exe" --fail-mute R
run p-meyesize-wrong 0 "$B/manafold-eyesize.exe" --fail-wrong-bone
for c in trick-spin-gain trick-spin-ease trick-spin-pivot trick-plant-pin flight-seam; do run f-mqa-$c 0 "$B/manafold-qa-p12.exe" --fail-$c; done
# ---- selectors (strict, RC 2) --------------------------------------------
S="$L/selscratch"
sel() { local id="$1"; shift; run "f-sel-$id" 2 env "$@" "$R" "$S" manafold-trick; }
sel spin-bogus ZHAO_U02_TRICK_SPIN=bogus
sel start-range ZHAO_U02_TRICK_SPIN_START_KEY=78
sel short-seg ZHAO_U02_TRICK_SPIN_TURN_KEY=104
sel overshoot ZHAO_U02_TRICK_SPIN_OVERSHOOT_PM=300
sel gain ZHAO_U02_TRICK_SPIN_GAIN_PM=-1
sel trick-bias ZHAO_U02_TRICK_CAM_BIAS=abc
sel trick-k ZHAO_U02_TRICK_CAM_K=1
sel cycles ZHAO_U02_FLIGHT_BOB_CYCLES=0
sel amp ZHAO_U02_FLIGHT_BOB_MM=-5
sel rise ZHAO_U02_FLIGHT_RISE_FRAC16=100
sel warp ZHAO_U02_FLIGHT_RISE_FRAC16=24576 ZHAO_U02_FLIGHT_TOP_HANG16=10000
sel flight-k ZHAO_U02_FLIGHT_CAM_K=5
sel drift-bx ZHAO_U02_DRIFT_CAM_BX=x
sel lift ZHAO_U02_FLIGHT_LIFT_MM=3000
sel plant-pin ZHAO_U02_TRICK_PLANT_PIN=bogus
sel pin-release ZHAO_U02_TRICK_PIN_RELEASE_KEYS=99
sel rear-frame ZHAO_U02_REAR_SOCKET_FRAME=root
sel rear-follow ZHAO_U02_REAR_SOCKET_FOLLOW_PM=1001
sel rear-ambient ZHAO_U02_REAR_AMBIENT_GAIN_PM=abc
sel line-scale ZHAO_U02_MANA_LINE_SCALE=wide
sel line-full ZHAO_U02_MANA_LINE_FULL_PX=10
sel end-swell-rx ZHAO_U02_END_SWELL_RX_MM=81
sel end-ball-rz ZHAO_U02_END_BALL_RZ_MM=abc
sel end-ball-half ZHAO_U02_END_BALL_HALF_MM=10
sel end-ball-at ZHAO_U02_END_BALL_AT_MM=2300
sel end-ball-support ZHAO_U02_END_BALL_AT_MM=2600 ZHAO_U02_END_BALL_HALF_MM=280
sel span-limit ZHAO_U02_REAR_SPAN_LIMIT=yes
sel span-travel ZHAO_U02_REAR_SPAN_TRAVEL_MM=10
sel span-soft ZHAO_U02_REAR_SPAN_SOFT_MM=5
sel span-knee ZHAO_U02_REAR_SPAN_TRAVEL_MM=100 ZHAO_U02_REAR_SPAN_SOFT_MM=200
sel dip-pm ZHAO_U02_KNEAD_DIP_PM=1001
sel dip-depth ZHAO_U02_KNEAD_DIP_DEPTH_MM=abc
sel dip-fold ZHAO_U02_KNEAD_DIP_FOLD_PM=-1
sel fold-dip ZHAO_U02_FOLD_DIP_PM=2000
sel dip-fold-hi ZHAO_U02_KNEAD_DIP_FOLD_PM=3001
sel bow-bogus ZHAO_U02_REAR_BOW=curve
sel bow-sign ZHAO_U02_REAR_BOW_SIGN=2
sel bow-onset ZHAO_U02_REAR_BOW_ONSET_MM=2001
sel bow-maxa ZHAO_U02_REAR_BOW_MAX_A16=32001
sel dent-solver ZHAO_U02_KNEAD_DIP_SOLVER=fold
sel dent-swing ZHAO_U02_KNEAD_DENT_SWING_PM=1001
sel dent-overpress ZHAO_U02_KNEAD_DENT_OVERPRESS_PM=3001
sel dent-depth ZHAO_U02_KNEAD_DENT_DEPTH_PM=6001
sel dip-ramp ZHAO_U02_KNEAD_DIP_RAMP_KEYS=61
sel dip-ramp-zero ZHAO_U02_KNEAD_DIP_RAMP_KEYS=0
sel dent-cross ZHAO_U02_KNEAD_DENT_CROSS_PM=801
sel dent-duck ZHAO_U02_KNEAD_DENT_DUCK_PM=1001
sel rig-bogus ZHAO_U02_RIG=bogus
sel ball-pm ZHAO_U02_BALL_PM=4001
# PASS 22 selectors
sel dot-scale ZHAO_U02_MANA_DOT_SCALE=wide
sel dot-full ZHAO_U02_MANA_DOT_FULL_PX=10
sel dot-strength ZHAO_U02_MANA_DOT_STRENGTH_PM=1001
sel dot-strength-neg ZHAO_U02_MANA_DOT_STRENGTH_PM=-1
sel dot-strength-empty ZHAO_U02_MANA_DOT_STRENGTH_PM=
sel knead-shape ZHAO_U02_FOLD_DIP_SHAPE_PM=3001
sel knead-shape-bogus ZHAO_U02_FOLD_DIP_SHAPE_PM=abc
# PASS 23 selectors: the per-clip press-depth ladder, strict in both directions.
# The last one is the interesting bound -- a slot whose shipped entry is 0
# declares that its clip hosts NO dip, and a knob that could make it host one
# would reintroduce pass 20's permanently-red unreachable leg through a new door.
sel clip-pm-empty ZHAO_U02_KNEAD_DIP_CLIP_PM=
sel clip-pm-nopair ZHAO_U02_KNEAD_DIP_CLIP_PM=18
sel clip-pm-novalue ZHAO_U02_KNEAD_DIP_CLIP_PM=18:
sel clip-pm-high ZHAO_U02_KNEAD_DIP_CLIP_PM=18:1001
sel clip-pm-neg ZHAO_U02_KNEAD_DIP_CLIP_PM=18:-1
sel clip-pm-slot ZHAO_U02_KNEAD_DIP_CLIP_PM=23:500
sel clip-pm-bogus ZHAO_U02_KNEAD_DIP_CLIP_PM=abc
sel clip-pm-comma ZHAO_U02_KNEAD_DIP_CLIP_PM=18:500,
sel clip-pm-sep "ZHAO_U02_KNEAD_DIP_CLIP_PM=18:500;20:600"
sel clip-pm-nonhosting ZHAO_U02_KNEAD_DIP_CLIP_PM=15:500
# ---- live-history gate (Wave E) ------------------------------------------
LH="python $REPO/tools/reel/manafold_live_history_gate.py --renderer $R"
run e-live-history-normal 0 $LH --out "$L/lh-normal"
run e-live-history-legacy 0 $LH --out "$L/lh-legacy" --control legacy
run e-live-history-list-drift 0 $LH --out "$L/lh-drift" --control list-drift
# ---- identity ------------------------------------------------------------
# ⚠ THE PASS-22 CONTRACT: both new mechanisms switched off reproduce the
# pass-21 bank byte for byte, on a FAR pair and a NEAR pair. Drift and Hasty are
# in the witness list deliberately -- they are the clips the dot law actually
# changes, so a leg that sampled only hover/inspect could not fail.
# ! PASS 23 ADDS A RUNG. The pass-23 change is a per-clip PRESS DEPTH, which
# moves the POSE and so is not switched off by either pass-22 knob. Its own
# exact-off is the ladder knob set back to the four values pass 22 shipped, and
# the pass-21 leg has to carry that too or it would be asserting a bank nobody
# can build. $P23_CLIP_OFF is those four values.
idleg e-identity-pass22 "$P22_CRCS" ZHAO_U02_KNEAD_DIP_CLIP_PM=$P23_CLIP_OFF
idleg e-identity-pass21 "$P21_CRCS" ZHAO_U02_KNEAD_DIP_CLIP_PM=$P23_CLIP_OFF ZHAO_U02_MANA_DOT_SCALE=legacy ZHAO_U02_FOLD_DIP_SHAPE_PM=0
# The positive controls. Neither identity leg may be vacuous:
#  - with the dots ON the pass-21 CRCs must NOT appear;
#  - with the pass-23 press depths LIVE the pass-21 CRCs must NOT appear either,
#    which is what makes the new rung load-bearing rather than decorative.
idleg e-dots-live "$P22_DOTS_CRCS" ZHAO_U02_KNEAD_DIP_CLIP_PM=$P23_CLIP_OFF ZHAO_U02_FOLD_DIP_SHAPE_PM=0
idleg e-press-live "$P23_PRESS_LIVE_CRCS" ZHAO_U02_MANA_DOT_SCALE=legacy ZHAO_U02_FOLD_DIP_SHAPE_PM=0
# And the pass-23 shipping bank itself, so a silent drift of the new depths is
# caught by a CRC and not only by an eye.
idleg e-identity-pass23 "$P23_CRCS"
# The pass-21 rig exact-off leg, carried forward.
run e-rig-pass20-mspan 0 env ZHAO_U02_RIG=pass20 "$B/manafold-spangate.exe"
run e-rig-pass20-mrear 0 env ZHAO_U02_RIG=pass20 "$B/manafold-rear-audit.exe" --gate
echo "total $(wc -l < "$OUT") pass $(grep -c '^PASS' "$OUT") fail $(grep -c '^FAIL' "$OUT")"
