# Phase 1 evidence map (Direction 27)

* solo-summary.json / solo-*-perframe.json — per-frame changed-pixel counts
  for each source alone vs the all-gains-zero dark plate (calibrated binary,
  published CRC reproduced first; header-verified reader mlrgb.py).
* solo-<name>-peak-f####.png / -peak-plate.png / -peak-diff.png — the
  strongest frame for each solo source, its plate, and the 4x-amplified
  signed difference.
* base-f*.png — the SHIPPED bank's own frames (what the owner judged).
* v1/ v2/ v3/ — the three authored-by-eye iterations at 2x native.
* v3-contact-sheet.png — every 12th frame of the accepted clip at 1x.
* v3-colour-trace.json — per-frame blue/orange/green presence of the
  accepted bank vs its own dark plate.
* expected-crc.txt — the live bank's 21-subject table the publication render
  must reproduce (moving-light expected 0x65A8D1E5, the accepted v3 draft).
