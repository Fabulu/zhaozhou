#!/usr/bin/env bash
cd /c/programmieren/zencrifice/manafold-p11-L
./runvar.sh Q12  PL_MOTE_COUNT=12 PL_WANDER=2
./runvar.sh Q22  PL_MOTE_COUNT=22 PL_WANDER=4
./runvar.sh Q60  PL_MOTE_COUNT=60 PL_WANDER=9
./runvar.sh Q90  PL_MOTE_COUNT=90 PL_WANDER=14
echo BATCH2_DONE
