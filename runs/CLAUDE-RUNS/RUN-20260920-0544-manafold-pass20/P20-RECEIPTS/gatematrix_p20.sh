#!/usr/bin/env bash
# pass-19 gate matrix (v18 integrated matrix + the pass-19 rear/line legs;
# review: + --fail-line-flag and the End swell/ball selectors). Usage: gatematrix_int.sh <bindir> <logdir> <out.txt>
B="$1"; L="$2"; OUT="$3"; mkdir -p "$L"; : > "$OUT"
REPO=/c/programmieren/zencrifice/manafold-p16/zhaozhou
run() { # id expected_rc cmd...
  local id="$1" exp="$2"; shift 2
  "$@" > "$L/$id.log" 2>&1; local rc=$?
  local tail; tail=$(grep -v '^\s*$' "$L/$id.log" | tail -1 | cut -c1-200)
  local extra; extra=$(grep -m1 -o 'attributed detector fired.*\|MUTANT[^|]*' "$L/$id.log" | tail -1 | cut -c1-160)
  local st=FAIL; [ "$rc" = "$exp" ] && st=PASS
  echo "$st $id rc=$rc exp=$exp | ${extra:-$tail}" >> "$OUT"
}
# normals
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
# pass 19: rear frame / End joint / mana line (manafold-rear-audit --gate)
runmask() { # id expected_mask cmd...  (RC 1 and the exact declared mask)
  local id="$1" m="$2"; shift 2
  "$@" > "$L/$id.log" 2>&1; local rc=$?
  local got; got=$(grep -o 'rear gate mask 0x[0-9A-F]*' "$L/$id.log" | tail -1)
  local st=FAIL; [ "$rc" = 1 ] && [ "$got" = "rear gate mask $m" ] && st=PASS
  echo "$st $id rc=$rc exp=1 mask=${got##* } want=$m" >> "$OUT"
}
run n-mrear 0 "$B/manafold-rear-audit.exe" --gate
runmask r-rear-frame 0xB "$B/manafold-rear-audit.exe" --fail-rear-frame
runmask r-rear-joint 0xA "$B/manafold-rear-audit.exe" --fail-rear-joint
runmask r-line-scale 0x4 "$B/manafold-rear-audit.exe" --fail-line-scale
runmask r-line-flag 0x4 "$B/manafold-rear-audit.exe" --fail-line-flag
# pass 20: R4 STRAIN + R5 DIP
runmask r-rear-strain 0x8 "$B/manafold-rear-audit.exe" --fail-rear-strain
runmask r-no-dip 0x10 "$B/manafold-rear-audit.exe" --fail-no-dip
# PASS 20 PACKET 7: the dip now ships ON, through the DENT solver. This leg
# judges R5 on the shipping configuration; the two legs below hold the CARRIED
# solver and the pass-19 bow to their exact bytes, so switching the mechanism
# on cannot hide a change to the one it replaced.
run n-mrear-dip 0 "$B/manafold-rear-audit.exe" --gate --dip
# identity: the carried solver and the pass-19 bow, byte for byte
crcs() { grep -oE 'manafold-[a-z0-9]+:.*crc32c=0x[0-9A-F]+' "$1" | grep -oE '0x[0-9A-F]+' | tr '
' ' '; }
idleg() { # id expected-crcs env...
  local id="$1" want="$2"; shift 2
  mkdir -p "$L/$id"
  env ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross "$@" "$R" "$L/$id"       manafold-hover manafold-inspect manafold-taunt3 > "$L/$id.log" 2>&1
  local got; got=$(crcs "$L/$id.log")
  local st=FAIL; [ "$got" = "$want" ] && st=PASS
  echo "$st $id crcs=[$got] want=[$want]" >> "$OUT"
  rm -rf "$L/$id"
}
# mspan controls
for s in F-A A-B B-C C-E; do run s-rigid-$s 1 "$B/manafold-spangate.exe" --fail-rigid-span $s; done
for s in F-A A-B B-C C-E; do run s-clamp-$s 1 "$B/manafold-spangate.exe" --fail-clamp-negative $s; done
for s in F-A A-B B-C C-E; do run s-over-$s 1 "$B/manafold-spangate.exe" --fail-overcompact $s; done
for s in A B C E; do run s-drift-$s 1 "$B/manafold-spangate.exe" --fail-delta-drift $s; done
for f in e-start e-mid e-presocket posed-order order antenna-snap accent-switch hold-tremor compress-wrap final-dwell root-authority front-flex swell-size terminal-cap; do run s-$f 1 "$B/manafold-spangate.exe" --fail-$f; done
for m in F A B C E; do run s-mute-$m 1 "$B/manafold-spangate.exe" --fail-mute $m; done
# msmooth controls
for f in lightning-switch shape-blackout particle-reseed surge-reseed loop-seam mote-count mote-role weight-wrap morph-reverse brightness-seam stamp-count final-dwell mote-visibility palette-raw-clock palette-hard-switch death-effect-cutoff; do run m-$f 1 "$B/manafold-motiongate.exe" --fail-$f; done
# protected legs
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
run p-mqa-lane 0 "$B/manafold-qa-p12.exe" --fail-lane
run p-mqa-seam 0 "$B/manafold-qa-p12.exe" --fail-seam
run p-mqa-eyesnap 0 "$B/manafold-qa-p12.exe" --fail-eyesnap
run p-mqa-startle-step 0 "$B/manafold-qa-p12.exe" --fail-startle-step
run p-mqa-rootstep 0 "$B/manafold-qa-p12.exe" --fail-rootstep
run p-mqa-fall-wrap 0 "$B/manafold-qa-p12.exe" --fail-fall-wrap
run p-meyesize-L 0 "$B/manafold-eyesize.exe" --fail-mute L
run p-meyesize-R 0 "$B/manafold-eyesize.exe" --fail-mute R
run p-meyesize-wrong 0 "$B/manafold-eyesize.exe" --fail-wrong-bone
# Wave F + integration controls (attributed)
for c in trick-spin-gain trick-spin-ease trick-spin-pivot trick-plant-pin flight-seam; do run f-mqa-$c 0 "$B/manafold-qa-p12.exe" --fail-$c; done
# selectors (strict, RC 2)
R="$B/zhao-reel-cel.exe"; S="$L/selscratch"
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
# pass 20 selectors
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
# pass 20 packet 6: the walk, the swing
sel dent-solver ZHAO_U02_KNEAD_DIP_SOLVER=fold
sel dent-swing ZHAO_U02_KNEAD_DENT_SWING_PM=1001
sel dent-overpress ZHAO_U02_KNEAD_DENT_OVERPRESS_PM=3001
sel dent-depth ZHAO_U02_KNEAD_DENT_DEPTH_PM=6001
run s-walk-pairing 1 "$B/manafold-spangate.exe" --fail-walk-pairing
# pass 20 CLOSE: the specified-but-never-built G10, R5's second arm, the ramp knob
run s-dent-pin 1 "$B/manafold-spangate.exe" --fail-dent-pin
runmask r-dip-stuck 0x10 "$B/manafold-rear-audit.exe" --fail-dip-stuck
sel dip-ramp ZHAO_U02_KNEAD_DIP_RAMP_KEYS=61
sel dip-ramp-zero ZHAO_U02_KNEAD_DIP_RAMP_KEYS=0
sel dent-cross ZHAO_U02_KNEAD_DENT_CROSS_PM=801
sel dent-duck ZHAO_U02_KNEAD_DENT_DUCK_PM=1001
# live-history gate (Wave E)
LH="python $REPO/tools/reel/manafold_live_history_gate.py --renderer $R"
run e-live-history-normal 0 $LH --out "$L/lh-normal"
run e-live-history-legacy 0 $LH --out "$L/lh-legacy" --control legacy
run e-live-history-list-drift 0 $LH --out "$L/lh-drift" --control list-drift
# ⚠ THE PASS-19 CONTRACT IS THIS ONE, and it is the leg that must never move:
# the dip switched OFF plus the pass-19 bow reproduces P19-FINAL-BANK-INTEGRITY's
# own recorded CRCs exactly. The two carried-solver legs below are WITHIN-pass
# identities of a selectable alternative mechanism, and they were re-baselined at
# the pass-20 close because the dip's SHARED schedule moved (the 30-key ramp
# floor and the re-trimmed per-clip shares feed both solvers, by design).
idleg e-identity-pass19 "0xA2D0E051 0x779615BB 0x75BC4777 " ZHAO_U02_KNEAD_DIP_PM=0 ZHAO_U02_REAR_BOW=legacy
idleg e-identity-dipoff "0xEFE5AFC1 0x9E71DF79 0xC81598AA " ZHAO_U02_KNEAD_DIP_PM=0
idleg e-identity-carried "0x40AA70E8 0xF6BD3444 0xC81598AA " ZHAO_U02_KNEAD_DIP_SOLVER=carried
idleg e-identity-legacy "0x46A9C936 0x99CCAC21 0x75BC4777 " ZHAO_U02_KNEAD_DIP_SOLVER=carried ZHAO_U02_REAR_BOW=legacy
echo "total $(wc -l < "$OUT") pass $(grep -c '^PASS' "$OUT") fail $(grep -c '^FAIL' "$OUT")"
