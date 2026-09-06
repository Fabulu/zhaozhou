#!/usr/bin/env bash
cd /c/programmieren/zencrifice/manafold-p11-L
# RE-RUN: the first attempt at these two rendered with a binary whose patch had
# silently failed its assertion, so they were duplicates of the shipped look and
# were deleted rather than reported. (CLAUDE.md's stale-binary tell: a number
# that did not move after a change that must have moved it.)
./runvar.sh E-SPARSE  PL_EDGE_STAMP=48
./runvar.sh E-CORELOW PL_CORE_GAIN=650
./runvar.sh K7-MODERATE PL_EDGE_CORE_R=2 PL_EDGE_HALO_R=5 PL_CORE_OF_HALO=2200 PL_PCON=4
./runvar.sh K8-LEAN PL_EDGE_CORE_R=2 PL_EDGE_HALO_R=5 PL_CORE_OF_HALO=2000 \
    PL_HALO_RMIN=5 PL_HALO_RMAX=11 PL_PCON=4 PL_MOTE_COUNT=30 PL_WANDER=5
echo BATCH8_DONE
