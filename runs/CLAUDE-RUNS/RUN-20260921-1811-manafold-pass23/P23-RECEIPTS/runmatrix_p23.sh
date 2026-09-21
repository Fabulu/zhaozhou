#!/usr/bin/env bash
# The pass-23 matrix, in ONE invocation, with every expectation declared here.
set -u
REPO=/c/programmieren/zencrifice/manafold-p16/zhaozhou
P=$REPO/runs/CLAUDE-RUNS/RUN-20260921-1811-manafold-pass23/P23-RECEIPTS
export P21_CRCS="0x200AA3E7 0x1CFE8375 0xFB17B7D6 0x425AA389 "
export P22_CRCS="0xEFCCD8FA 0x6B1077D0 0xB6AB88AA 0x6B85677A "
export P22_DOTS_CRCS="0xE379F918 0x1CFE8375 0x148ABAF0 0x40E54E64 "
export P23_CRCS="0xEFCCD8FA 0x6B1077D0 0xF376C81F 0x6B85677A "
export P23_PRESS_LIVE_CRCS="0x200AA3E7 0x1CFE8375 0xD243EDE4 0x425AA389 "
export P23_CLIP_OFF="1:730,14:635,17:635,18:590"
bash "$P/gatematrix_p23.sh" "$REPO/.tmp/p23/bin" "$REPO/.tmp/p23matrixlogs" "$P/gate-matrix.txt"
