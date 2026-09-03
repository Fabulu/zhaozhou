#!/usr/bin/env bash
# Phase 1 existence-proof renders: dark plate + each source alone (natural and
# extreme gain). One subject each; every invocation explicit.
set -euo pipefail
cd "$(dirname "$0")"
BIN=build-work/bin/zhao-reel-cel.exe
run() {
  local name="$1" solo="$2" boost="$3"
  mkdir -p "scratch/$name"
  env ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
      ZIXX_ML_SOLO="$solo" ${boost:+ZIXX_ML_BOOST=$boost} \
      "$BIN" "scratch/$name" zixxtrixx-moving-light 2>&1 | tail -1
  grep sequence_crc32c "scratch/$name/zixxtrixx-moving-light/meta.txt"
}
run none    none   ""
run warm1   warm   ""
run blue1   blue   ""
run orange1 orange ""
run green1  green  ""
run blue8   blue   8
run orange8 orange 8
run green8  green  8
echo ALL-SOLO-RENDERS-DONE
