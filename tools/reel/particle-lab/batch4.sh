#!/usr/bin/env bash
cd /c/programmieren/zencrifice/manafold-p11-L
./runvar.sh Z-TINY   PL_HALO_RMIN=4  PL_HALO_RMAX=6
./runvar.sh Z-FAT    PL_HALO_RMIN=11 PL_HALO_RMAX=15
./runvar.sh Z-MIXED  PL_HALO_RMIN=4  PL_HALO_RMAX=14
./runvar.sh Z-BODYSM PL_CORE_OF_HALO=900
./runvar.sh Z-BODYBG PL_CORE_OF_HALO=2400
echo BATCH4_DONE
