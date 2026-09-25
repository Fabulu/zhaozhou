#!/usr/bin/env bash
# PASS-26 gate matrix (Direction 27, Hasty's hurry).
#
# The pass-25 matrix CARRIED FORWARD IN FULL -- every normal, every mute, every
# attributed control, every mask leg and the whole per-item identity ladder --
# plus this pass's own legs:
#
#   * e-p25off : ZHAO_U02_HASTY_HURRY=off reproduces the PASS-25 BANK exactly,
#                all 22 subjects. The exact-off control, as a gate.
#   * e-item-hasty : the hurry alone changes EXACTLY manafold-hasty and nothing
#                else. Without this second leg, "21 subjects are identical" and
#                "the hurry is dead" look the same in a CRC -- the pass-25
#                matrix learned that from item 3 and it is carried here.
#   * h-fire-* : every pass-26 control moves the clip's pixels.
#   * h-rej-*  : every malformed or out-of-range value is REFUSED with RC 2.
#   * n-screenmotion-selftest : the new instrument's own six legs.
#
#   * e-backball-live : the positive control for the repair below.
#
# TWO FLOOR REPAIRS, and they are the same act twice.
#
# $HASTYOFF -- the pass-24 and pass-23 identity floors need
# ZHAO_U02_HASTY_HURRY=off, because the hurry is ON by default and those ladders
# reproduce banks that predate it. A floor leg left without it would fail on
# hasty alone and read as a regression in a clip that pass never touched.
#
# $BBOFF -- AND THEY WERE ALREADY FAILING, on HOVER and INSPECT, before pass 26
# touched anything. The back-ball packet landed AFTER the pass-25 matrix receipt
# was committed and the full matrix was never re-run, so the floors kept a PASS
# from a tree that no longer existed -- carried through pass 25's close, its
# review and its production verification. Full diagnosis at the BBOFF
# definition below and in P26-NOTES/FINDINGS-04-inherited-matrix-fault.md.
#
# The rule both of them are instances of: A FLOOR MUST SWITCH OFF EVERY
# MECHANISM ADDED SINCE THE BANK IT CLAIMS TO REPRODUCE, and each new mechanism
# has to be added to the floors on the pass that introduces it.
#
# Inherited header follows.
# ----------------------------------------------------------------------------
# Pass-25 gate matrix: the pass-24 matrix carried forward in full, plus
#   * mbolt's two new legs (B3 ROLLOUT, B4 MIRROR) and the new
#     --fail-mirror-drift control, which is the one the pass-24 reviewer asked
#     for: the gate used to hold its own copy of the per-subject lightning
#     configuration and nothing bound it to the renderer's;
#   * the pass-25 selectors, strict in both directions;
#   * a PER-ITEM identity ladder on all 22 live subjects whose FLOOR is now
#     pass 24 (reproduced through ZHAO_U02_BOLT_ROLLOUT=pass24, because item 1's
#     predecessor was per SUBJECT and no bank-wide flag can express it) and
#     which also still reaches pass 23.
#   * AND A POSITIVE CONTROL ON ITEM 3, which ships no byte change. A knob that
#     changes nothing and a knob that is DEAD look identical in a CRC, and this
#     exact knob was dead from pass 20 to pass 24. So the matrix asserts both:
#     at its shipped value it changes nothing, and moved off it, it changes
#     exactly the three subjects that play slot 0.
# Usage: gatematrix_p25.sh <bindir> <logdir> <out.txt>
B="$1"; L="$2"; OUT="$3"; mkdir -p "$L"; : > "$OUT"
P24=/c/programmieren/zencrifice/manafold-p16/zhaozhou/runs/CLAUDE-RUNS/RUN-20260922-1208-manafold-pass24/P24-RECEIPTS
P25=/c/programmieren/zencrifice/manafold-p16/zhaozhou/runs/CLAUDE-RUNS/RUN-20260922-2146-manafold-pass25-final/P25-RECEIPTS
P26=/c/programmieren/zencrifice/manafold-p16/zhaozhou/runs/CLAUDE-RUNS/RUN-20260925-1535-manafold-pass26-hasty/P26-RECEIPTS
# The pass-25 REVIEWER's receipt, and why the exact-off leg uses it instead of
# $P25/crcs-ship.txt. That file was written by the pass-25 MATRIX (commit
# 82218e2a) and the back-ball packet landed AFTER it, so it records hover
# 0x8124751D where the pass-25 tree actually renders 0x89EBA648 -- it is the
# same staleness $BBOFF repairs, in the shipping row rather than the floor.
#
# PASS 25'S SHIPPED BANK IS NOT IN DOUBT. Two LATER receipts certify it and they
# agree with each other, with the pass-25 tree, and with an independent rebuild
# of it here: P25-BB-RECEIPTS/crcs-backball.txt and this file both record
# hover 0x89EBA648, inspect 0x3A179F08, crackle 0x370F7F3E, hasty 0xDC044A02,
# and a reference binary built from git f66d107c in a detached worktree
# reproduces all four exactly. Only the matrix's own crcs-ship.txt is stale.
# The reviewer's is preferred because its provenance is an INDEPENDENT BUILD.
P25REV=/c/programmieren/zencrifice/manafold-p16/zhaozhou/runs/CLAUDE-RUNS/RUN-20260922-2146-manafold-pass25-final/P25-REVIEW-RECEIPTS
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
# STDOUT AND STDERR MUST BE SEPARATE FILES HERE (pass-24 finding): the reel
# writes its per-subject CRC line on stdout and celmain telemetry on stderr, and
# merged they INTERLEAVE and truncate a CRC mid-word.
LIVE22="manafold-hover manafold-inspect manafold-channel manafold-trick \
manafold-damage manafold-hasty manafold-flight manafold-fall manafold-hit \
manafold-taunt manafold-taunt2 manafold-death-drop manafold-death-gutter \
manafold-lasso manafold-blown manafold-taunt3 manafold-drift manafold-curious \
manafold-startle manafold-rest manafold-pirouette manafold-crackle"
# LC_ALL=C on every sort: a leg once failed on locale collation alone.
crcs() {
  grep -oE 'manafold-[a-z0-9-]+: [0-9]+ frames, [0-9]+ unique colours, sequence_crc32c=0x[0-9A-F]+' "$1" \
    | sed 's/, [0-9]* unique colours//' | LC_ALL=C sort
}
bank() { # id env...   -> writes $L/$id.crc
  local id="$1"; shift
  mkdir -p "$L/$id"
  env "$@" "$R" "$L/$id" $LIVE22 > "$L/$id.out" 2> "$L/$id.err"
  crcs "$L/$id.out" > "$L/$id.crc"
  rm -rf "$L/$id"
}
idfile() { # id expected-file (CRLF tolerated on the committed receipts)
  local id="$1" want="$2"
  local n; n=$(wc -l < "$L/$id.crc")
  local st=FAIL
  if [ "$n" = 22 ] && diff -q <(tr -d '\r' < "$want") "$L/$id.crc" > /dev/null; then st=PASS; fi
  echo "$st $id 22-subject bank identical to $(basename "$want") (rows=$n)" >> "$OUT"
}
idchanged() { # id base expected-changed-names
  local id="$1" base="$2" want="$3"
  local got; got=$(diff "$L/$id.crc" "$base" | grep '^<' | sed 's/^< //' | cut -d: -f1 | LC_ALL=C sort | tr '\n' ' ')
  local st=FAIL; [ "$got" = "$want" ] && st=PASS
  echo "$st $id changed=[$got] want=[$want]" >> "$OUT"
}
idcount() { # id base expected-count
  local id="$1" base="$2" want="$3"
  local got; got=$(diff "$L/$id.crc" "$base" | grep -c '^<')
  local st=FAIL; [ "$got" = "$want" ] && st=PASS
  echo "$st $id changed=$got want=$want" >> "$OUT"
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
run n-mbolt 0 "$B/manafold-boltgate.exe" --gate
# mbolt controls. mbolt returns 0 for a control that FIRED and 1 for one that
# did not -- a control which stops firing is the failure.
run d-mbolt-no-avoid 0 "$B/manafold-boltgate.exe" --gate --fail-no-avoid
run d-mbolt-no-split 0 "$B/manafold-boltgate.exe" --gate --fail-no-split
run d-mbolt-fat-rod 0 "$B/manafold-boltgate.exe" --gate --fail-fat-rod
# PASS 25: B4's control. It cannot be fired by any legal stimulus -- the two
# lists agree and a static_assert already guards the renderer's copy -- so this
# is the deliberate fault the detector's silence is checked against.
run d-mbolt-mirror-drift 0 "$B/manafold-boltgate.exe" --gate --fail-mirror-drift
# PASS 25: the POSITIVE CONTROL ON THE GATE'S OWN EXIT CODE. B1/B3 both read
# zero at the shipping configuration and a detector reading zero is a claim.
run d-mbolt-envoff-red 1 env ZHAO_U02_BOLT_AVOID=off "$B/manafold-boltgate.exe" --gate
# PASS 25: and a clearance the ladder MEASURED as leaving residue must be red.
# 70 mm leaves 11 intersections on blown; this leg is the proof that the
# non-monotonicity recorded beside kBoltRodClearanceMm is real and is caught.
run d-mbolt-clr70-red 1 env ZHAO_U02_BOLT_CLEARANCE_MM=70 "$B/manafold-boltgate.exe" --gate
# ---- mrear mask controls (pass 19 / 20 / 22 / 23) ------------------------
runmask r-rear-frame 0x3 "$B/manafold-rear-audit.exe" --fail-rear-frame
runmask r-rear-joint 0x2 "$B/manafold-rear-audit.exe" --fail-rear-joint
runmask r-line-scale 0x24 "$B/manafold-rear-audit.exe" --fail-line-scale
runmask r-line-flag 0x4 "$B/manafold-rear-audit.exe" --fail-line-flag
runmask r-rear-strain 0x8 "$B/manafold-rear-audit.exe" --fail-rear-strain
runmask r-no-dip 0x50 "$B/manafold-rear-audit.exe" --fail-no-dip
runmask r-dip-stuck 0x10 "$B/manafold-rear-audit.exe" --fail-dip-stuck
runmask r-dot-scale 0x20 "$B/manafold-rear-audit.exe" --fail-dot-scale
runmask r-dot-flag 0x20 "$B/manafold-rear-audit.exe" --fail-dot-flag
runmask r-knead-shape 0x40 "$B/manafold-rear-audit.exe" --fail-knead-shape
runmask r-line-far 0x20 "$B/manafold-rear-audit.exe" --fail-line-far
runmask r-knead-clip 0x40 "$B/manafold-rear-audit.exe" --fail-knead-clip
runmask r-knead-drop 0x40 "$B/manafold-rear-audit.exe" --fail-knead-drop
# ---- mrod controls (pass 21) ---------------------------------------------
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
sel dot-scale ZHAO_U02_MANA_DOT_SCALE=wide
sel dot-full ZHAO_U02_MANA_DOT_FULL_PX=10
sel dot-strength ZHAO_U02_MANA_DOT_STRENGTH_PM=1001
sel dot-strength-neg ZHAO_U02_MANA_DOT_STRENGTH_PM=-1
sel dot-strength-empty ZHAO_U02_MANA_DOT_STRENGTH_PM=
sel knead-shape ZHAO_U02_FOLD_DIP_SHAPE_PM=3001
sel knead-shape-bogus ZHAO_U02_FOLD_DIP_SHAPE_PM=abc
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
sel bolt-avoid-bogus ZHAO_U02_BOLT_AVOID=on
sel bolt-avoid-empty ZHAO_U02_BOLT_AVOID=
sel bolt-clear-high ZHAO_U02_BOLT_CLEARANCE_MM=401
sel bolt-clear-neg ZHAO_U02_BOLT_CLEARANCE_MM=-1
sel bolt-clear-bogus ZHAO_U02_BOLT_CLEARANCE_MM=abc
sel bolt-split-zero ZHAO_U02_BOLT_SPLIT_N=0
sel bolt-split-high ZHAO_U02_BOLT_SPLIT_N=17
sel bolt-split-bogus ZHAO_U02_BOLT_SPLIT_N=x
sel bolt-comp-high ZHAO_U02_BOLT_SPLIT_COMPENSATE=2
sel rear-clip-empty ZHAO_U02_REAR_AMBIENT_CLIP_PM=
sel rear-clip-nopair ZHAO_U02_REAR_AMBIENT_CLIP_PM=0
sel rear-clip-high ZHAO_U02_REAR_AMBIENT_CLIP_PM=0:1001
sel rear-clip-slot ZHAO_U02_REAR_AMBIENT_CLIP_PM=24:400
sel rear-clip-comma ZHAO_U02_REAR_AMBIENT_CLIP_PM=0:400,
sel front-clip-high ZHAO_U02_FRONT_FLEX_CLIP_PM=0:3001
sel front-clip-slot ZHAO_U02_FRONT_FLEX_CLIP_PM=24:1000
sel front-clip-bogus ZHAO_U02_FRONT_FLEX_CLIP_PM=abc
sel eye-amb-high ZHAO_U02_EYE_AMBIENT_PM=1001
sel eye-amb-neg ZHAO_U02_EYE_AMBIENT_PM=-1
sel eye-amb-bogus ZHAO_U02_EYE_AMBIENT_PM=x
sel eye-clip-high ZHAO_U02_EYE_AMBIENT_CLIP_PM=0:1001
sel eye-clip-slot ZHAO_U02_EYE_AMBIENT_CLIP_PM=24:600
sel eye-clip-nopair ZHAO_U02_EYE_AMBIENT_CLIP_PM=5
sel rear-calm-high ZHAO_U02_REAR_CARRIER_CALM_PM=1001
sel rear-calm-bogus ZHAO_U02_REAR_CARRIER_CALM_PM=abc
# PASS 25 selectors, strict in both directions like every selector here.
sel rollout-bogus ZHAO_U02_BOLT_ROLLOUT=on
sel rollout-empty ZHAO_U02_BOLT_ROLLOUT=
sel rollout-case ZHAO_U02_BOLT_ROLLOUT=All
sel calm-clip-empty ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=
sel calm-clip-nopair ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0
sel calm-clip-novalue ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:
sel calm-clip-high ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:1001
sel calm-clip-neg ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:-1
sel calm-clip-slot ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=24:500
sel calm-clip-bogus ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=abc
sel calm-clip-comma ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:500,
# ---- live-history gate (Wave E) ------------------------------------------
LH="python $REPO/tools/reel/manafold_live_history_gate.py --renderer $R"
run e-live-history-normal 0 $LH --out "$L/lh-normal"
run e-live-history-legacy 0 $LH --out "$L/lh-legacy" --control legacy
run e-live-history-list-drift 0 $LH --out "$L/lh-drift" --control list-drift
# ---- identity, PER ITEM, on all 22 live subjects ------------------------
#
# THE PASS-25 CONTRACT.
#   p24off : all three items at their pass-24 values reproduce the pass-24 bank
#            EXACTLY, all 22. Item 1's predecessor is per SUBJECT, so this uses
#            the named ZHAO_U02_BOLT_ROLLOUT=pass24 selector rather than a
#            bank-wide flag that would reproduce pass 23 instead.
#   p23off : and the floor still reaches pass 23 with every mechanism off.
#   item1  : the rollout + the raised clearance alone changes ALL 22, which is
#            what a rollout to all 22 means.
#   item2  : the eye raise alone changes 19 and must leave CURIOUS, STARTLE and
#            TAUNT III untouched -- the authored beats, gain 0.
#   item3  : ships NO byte change, asserted...
#   item3-live : ...and the SAME knob moved off its shipped value changes
#            exactly hover, inspect and crackle. Without this second leg,
#            "item 3 changes nothing" is indistinguishable from the knob being
#            dead -- which is what it WAS from pass 20 to pass 24.
# PASS 26: the hurry is ON by default, so every ladder reproducing a bank
# from BEFORE pass 26 must switch it off. Spliced into p24off, p23off,
# item1, item2, item3 and item3-live -- all six describe pre-26 banks.
HASTYOFF="ZHAO_U02_HASTY_HURRY=off"
P24OFF="ZHAO_U02_BOLT_ROLLOUT=pass24 ZHAO_U02_BOLT_CLEARANCE_MM=46"
EYEOFF="ZHAO_U02_EYE_AMBIENT_CLIP_PM=0:600,1:600,2:600,5:600,6:600,8:600,9:600,10:600,11:600,12:600,13:600,14:600,17:600,18:600,19:600,20:600,22:600,23:600"
CALMOFF="ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:1000,23:1000"
# PASS 26 REPAIR OF AN INHERITED FAULT. The historic floors were MISSING the
# back-ball off-flag and had been failing, unnoticed, since the back-ball packet
# landed. Chased in P26-NOTES/FINDINGS-04:
#
#   * e-p24off and e-p23off go red on HOVER and INSPECT -- subjects pass 26
#     never touched -- while CRACKLE matches exactly. That is the tell:
#     kBackBallDampClipPm is 700 on SLOT 0 and 0 on slot 23, and "slot 0 is two
#     subjects: hover AND inspect, one bake under two names". The owner declined
#     the slot-23 offer, so crackle carries no damping and is unaffected.
#   * The PASS-25 REFERENCE BINARY (git f66d107c, detached worktree) misses the
#     receipt too, by itself: hover 0xB7BE913A against the receipt's 0x8EDC6DE3.
#     So this is not pass 26's, and the two binaries render the floor
#     byte-identically (0 of 600 frames differ).
#   * The pass-25 matrix receipt was committed at 82218e2a; the back-ball packet
#     landed AFTER it (af8c97c1 / b3c5760e, 1,134 insertions in tools/reel).
#     THE FULL MATRIX WAS NEVER RE-RUN AFTER IT. The floors kept a PASS from a
#     tree that no longer existed, and it was carried through the pass's close,
#     its review and its production verification.
#   * Proof: floor + BBOFF on the pass-25 binary gives hover 0x8EDC6DE3 and
#     inspect 0xA7972F35 -- the receipt exactly, both.
#
# A floor must switch off EVERY mechanism added since the bank it claims to
# reproduce, and each new mechanism has to be added to the floors on the pass
# that introduces it. That is what $HASTYOFF does for pass 26 and what this
# does, late, for the back-ball packet.
BBOFF="ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0"
bank e-p24off $P24OFF $EYEOFF $CALMOFF $HASTYOFF $BBOFF
idfile e-p24off "$P24/crcs-ship.txt"
bank e-p23off $HASTYOFF $BBOFF ZHAO_U02_BOLT_AVOID=off ZHAO_U02_BOLT_SPLIT_N=1 $EYEOFF \
  ZHAO_U02_EYE_AMBIENT_PM=0 ZHAO_U02_REAR_AMBIENT_CLIP_PM=0:400,23:400 \
  ZHAO_U02_FRONT_FLEX_CLIP_PM=0:1000,23:1000 $CALMOFF
idfile e-p23off "$P24/baseline-pass23-crcs.txt"
bank e-item1 $EYEOFF $CALMOFF $HASTYOFF $BBOFF
idcount e-item1 "$L/e-p24off.crc" 22
bank e-item2 $P24OFF $CALMOFF $HASTYOFF $BBOFF
idchanged e-item2 "$L/e-p24off.crc" "manafold-blown manafold-channel manafold-crackle manafold-damage manafold-death-drop manafold-death-gutter manafold-drift manafold-fall manafold-flight manafold-hasty manafold-hit manafold-hover manafold-inspect manafold-lasso manafold-pirouette manafold-rest manafold-taunt manafold-taunt2 manafold-trick "
bank e-item3 $P24OFF $EYEOFF $HASTYOFF $BBOFF
idfile e-item3 "$P24/crcs-ship.txt"
bank e-item3-live $P24OFF $EYEOFF $HASTYOFF $BBOFF ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:300,23:300
idchanged e-item3-live "$L/e-p24off.crc" "manafold-crackle manafold-hover manafold-inspect "
# ---- PASS 26's OWN IDENTITY LEGS ----------------------------------------
# The exact-off control, as a gate: hurry=off IS the pass-25 bank, all 22.
bank e-p25off $HASTYOFF
idfile e-p25off "$P25REV/crcs-reviewer-ship.txt"
# ...and the hurry alone changes EXACTLY hasty. Both legs or neither: a bank
# that matches pass 25 with the knob off proves nothing by itself about
# whether the knob does anything.
bank e-item-hasty
idchanged e-item-hasty "$L/e-p25off.crc" "manafold-hasty "
# THE BACK-BALL FLAG'S OWN POSITIVE CONTROL. The floors pass again only because
# BBOFF was added; if that flag were dead they would pass for the wrong reason
# and nobody would know. So: the SAME flag against the SHIPPING configuration
# must change exactly the two subjects that carry slot-0 damping.
# And the shipping bank itself.
bank e-ship
idfile e-ship "$P26/crcs-ship.txt"
# ...and only NOW the back-ball positive control, because it diffs against
# e-ship.crc and that file has to exist first.
#
# ⚠ THE FIRST VERSION RAN IT BEFORE `bank e-ship` AND REPORTED changed=[] --
# which is exactly what a DEAD FLAG looks like. `diff` wrote "No such file or
# directory" to stderr, the leg captured stdout, and an empty result became a
# confident-looking "this knob changes nothing". A positive control that cannot
# read its own baseline is worse than no control: it fails in the shape of the
# finding it exists to rule out.
bank e-backball-live $BBOFF
idchanged e-backball-live "$L/e-ship.crc" "manafold-hover manafold-inspect "
# ---- PASS 26 CONTROLS: every one FIRED, every bad value REFUSED ----------
# A control nobody has seen move the output is not a control. `hfire` renders
# hasty with the knob moved and requires the frames to DIFFER from the shipping
# render; `hrej` requires RC 2 for a malformed or out-of-range value.
SHIPDIR="$L/h-ship"
rm -rf "$SHIPDIR"; mkdir -p "$SHIPDIR"
"$R" "$SHIPDIR" manafold-hasty > "$L/h-ship.out" 2> "$L/h-ship.err"
hfire() { # id env value
  local id="$1" e="$2" v="$3"; local o="$L/h-$id"
  rm -rf "$o"; mkdir -p "$o"
  env "$e=$v" "$R" "$o" manafold-hasty > "$L/h-$id.out" 2> "$L/h-$id.err"
  local rc=$?
  local d; d=$(python "$REPO/tools/reel/framediffcount.py" \
                 "$SHIPDIR/manafold-hasty" "$o/manafold-hasty")
  rm -rf "$o"
  local st=FAIL; [ "$rc" = 0 ] && [ "${d:-0}" -gt 0 ] && st=PASS
  echo "$st h-fire-$id rc=$rc | $e=$v FIRED: $d/240 frames differ" >> "$OUT"
}
hrej() { # id env value
  local id="$1" e="$2" v="$3"
  env "$e=$v" "$R" "$L/h-rej" manafold-hasty > "$L/h-rej-$id.log" 2>&1; local rc=$?
  local st=FAIL; [ "$rc" = 2 ] && st=PASS
  echo "$st h-rej-$id rc=$rc exp=2 | $e='$v' refused, not clamped" >> "$OUT"
}
hfire hurry      ZHAO_U02_HASTY_HURRY          off
hfire bobcycles  ZHAO_U02_HASTY_BOB_CYCLES     9
hfire bobamp     ZHAO_U02_HASTY_BOB_AMP_MM     260
hfire surge      ZHAO_U02_HASTY_SURGE_A16      0
hfire traverse   ZHAO_U02_HASTY_TRAVERSE_PM    500
hfire camfollow  ZHAO_U02_HASTY_CAM_FOLLOW_PM  1000
hfire camk       ZHAO_U02_HASTY_CAM_K          200000
hfire eyedrive   ZHAO_U02_HASTY_EYE_DRIVE_PM   1100
hfire eyecheck   ZHAO_U02_HASTY_EYE_CHECK_PM   900
hfire squint     ZHAO_U02_HASTY_SQUINT_PM      0
hfire brow       ZHAO_U02_HASTY_BROW_PM        800
hrej hurry-word  ZHAO_U02_HASTY_HURRY          maybe
hrej hurry-empty ZHAO_U02_HASTY_HURRY          ""
hrej bob-lo      ZHAO_U02_HASTY_BOB_CYCLES     0
hrej bob-hi      ZHAO_U02_HASTY_BOB_CYCLES     41
hrej bob-junk    ZHAO_U02_HASTY_BOB_CYCLES     13x
hrej amp-hi      ZHAO_U02_HASTY_BOB_AMP_MM     601
hrej surge-lo    ZHAO_U02_HASTY_SURGE_A16      -1
hrej trav-hi     ZHAO_U02_HASTY_TRAVERSE_PM    3001
hrej follow-hi   ZHAO_U02_HASTY_CAM_FOLLOW_PM  1001
hrej camk-lo     ZHAO_U02_HASTY_CAM_K          59999
hrej camk-hi     ZHAO_U02_HASTY_CAM_K          600001
hrej eyed-lo     ZHAO_U02_HASTY_EYE_DRIVE_PM   399
hrej eyed-hi     ZHAO_U02_HASTY_EYE_DRIVE_PM   1901
hrej eyec-hi     ZHAO_U02_HASTY_EYE_CHECK_PM   1901
hrej squint-hi   ZHAO_U02_HASTY_SQUINT_PM      1001
hrej brow-lo     ZHAO_U02_HASTY_BROW_PM        -1001
hrej brow-hi     ZHAO_U02_HASTY_BROW_PM        1001
rm -rf "$SHIPDIR" "$L/h-rej"
# ---- meyesize's REGISTRATION leg, fired ---------------------------------
# Pass 26 moved the expression-slot list out of the gate and into
# u02::eye_expression_slot(), because the gate held one copy of that fact and
# the clips held the other -- and this gate correctly went red the first time
# hasty started acting with its eyes. A registration check that has only ever
# been seen GREEN is a claim, so both directions are fired here:
#
#   h-eyesize-off  : with the hurry off, slot 8 is not an expression slot and
#                    allocates no track. Still green -- the leg does not simply
#                    stop looking at slot 8, it expects the other answer.
#   h-eyesize-flat : flatten hasty's authored eye size to identity AND mute its
#                    ambient eye layer. The track is then ALLOCATED BUT
#                    ALL-IDENTITY, which is a registered clip that does not act,
#                    and the leg must fire. Legal stimulus throughout -- no
#                    mutant needed, because this guard IS reachable.
run h-eyesize-off 0 env ZHAO_U02_HASTY_HURRY=off "$B/manafold-eyesize.exe"
run h-eyesize-flat 1 env ZHAO_U02_HASTY_EYE_DRIVE_PM=1000 \
  ZHAO_U02_HASTY_EYE_CHECK_PM=1000 ZHAO_U02_EYE_AMBIENT_CLIP_PM=8:0 \
  "$B/manafold-eyesize.exe"
# And mqa in the exact-off configuration, which must also stay green: the
# root-continuity ceiling applies to pass 25's clip too.
run h-mqa-off 0 env ZHAO_U02_HASTY_HURRY=off "$B/manafold-qa-p12.exe"
# ---- the screenmotion instrument's own selftest -------------------------
# New tool this pass. Its six legs include the PAN-ONLY leg that is the whole
# reason it exists (creature and ground moved together -> relative +0.00), and
# a sub-pixel leg for the rate regime this clip lives in. An instrument that
# has never been shown to fail is not evidence.
run n-screenmotion-selftest 0 python $REPO/tools/reel/screenmotion.py selftest
# The pass-21 rig exact-off leg, carried forward.
run e-rig-pass20-mspan 0 env ZHAO_U02_RIG=pass20 "$B/manafold-spangate.exe"
run e-rig-pass20-mrear 0 env ZHAO_U02_RIG=pass20 "$B/manafold-rear-audit.exe" --gate
echo "total $(wc -l < "$OUT") pass $(grep -c '^PASS' "$OUT") fail $(grep -c '^FAIL' "$OUT")"
