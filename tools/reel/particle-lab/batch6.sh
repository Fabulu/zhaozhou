#!/usr/bin/env bash
cd /c/programmieren/zencrifice/manafold-p11-L
# The pileup test: the edge is stamped every kBoltStampMm=22 mm along a path of
# up to 18 links x 4 segments x 24 stamps. Additive over pink can only whiten
# (09-ENGINE-GOTCHAS S4), so the shape's white middle is the outline stamping
# itself hundreds of times. Two ways to thin it: fewer stamps, or a thinner one.
./runvar.sh E-SPARSE  PL_EDGE_STAMP=48
./runvar.sh E-HALOG   PL_EDGE_HALO_G=90
./runvar.sh E-CORELOW PL_CORE_GAIN=650
echo BATCH6_DONE
