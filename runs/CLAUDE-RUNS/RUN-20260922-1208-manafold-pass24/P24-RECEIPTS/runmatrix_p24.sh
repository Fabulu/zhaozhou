#!/usr/bin/env bash
# The pass-24 matrix, in ONE invocation.
set -u
REPO=/c/programmieren/zencrifice/manafold-p16/zhaozhou
P=$REPO/runs/CLAUDE-RUNS/RUN-20260922-1208-manafold-pass24/P24-RECEIPTS
export ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross
bash "$P/gatematrix_p24.sh" "$REPO/.tmp/p24/bin" "$REPO/.tmp/p24matrixlogs" "$P/gate-matrix.txt"
