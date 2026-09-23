#!/usr/bin/env bash
# The pass-25 matrix, in ONE invocation.
#
# IT RUNS A FROZEN COPY, and that is a pass-24 finding rather than tidiness:
# bash reads a script incrementally BY BYTE OFFSET, so editing the file while it
# executes shifts everything after the insertion and execution resumes in the
# wrong place. One leg ran and reported TWICE and nothing said what was skipped.
set -u
REPO=/c/programmieren/zencrifice/manafold-p16/zhaozhou
P=$REPO/runs/CLAUDE-RUNS/RUN-20260922-2146-manafold-pass25-final/P25-RECEIPTS
FROZEN=$REPO/.tmp/p25frozen/gatematrix_p25.sh
mkdir -p "$REPO/.tmp/p25frozen"
cp "$P/gatematrix_p25.sh" "$FROZEN"
export ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross
bash "$FROZEN" "$REPO/.tmp/p25/bin" "$REPO/.tmp/p25matrixlogs" "$P/gate-matrix.txt"
