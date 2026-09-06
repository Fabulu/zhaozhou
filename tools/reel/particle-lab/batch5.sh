#!/usr/bin/env bash
# The EDGE stamps are part of the particle SHAPE axis: at 2x zoom the white
# smear in the middle of every fold is the edge CORE, stamped at a hard-coded
# gain of 1000 -- which is exactly what the dead knob kFoldEdgeCoreGainPm=430
# was written to dim and never got to.
cd /c/programmieren/zencrifice/manafold-p11-L
./runvar.sh E-CORE430 PL_EDGE_CORE_G=430
./runvar.sh E-CORE250 PL_EDGE_CORE_G=250
./runvar.sh E-THIN    PL_EDGE_CORE_G=430 PL_EDGE_CORE_R=2 PL_EDGE_HALO_R=5
./runvar.sh E-CRISP   PL_EDGE_CORE_G=430 PL_EDGE_JIT=10
echo BATCH5_DONE
