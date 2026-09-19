# Q011 review-wave-d-probe
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent bug review of committed commit 50803207 (Manafold v18 Wave D), file manafold_probe.cpp: the Trick ground-contact probe that must prove carrier-B antenna support OWNS contact at every planted key and sub-frame midpoint (sub 0/1), within depth -60..-5 mm, with positive controls. Earlier review demanded four hardenings: (1) ties with non-support vertices must not count as owned; (2) region membership must use the UNDEFORMED bind vertex, not post-deform; (3) membership must require nonzero weight on the carrier bone; (4) the wrong-support control must fail ONLY through ownership (depth failure attributed separately). Y up, ground y=0, Q16.16 fixed point.

## Questions
1. For each of hardenings 1-4: implemented correctly? Cite lines. Any way the check can still pass while a non-support vertex owns contact?
2. Do the positive controls (wrong-support, and any separate depth control) fire for the right reason and ONLY that reason?
3. Is the expected sample count derived from the same window the animation uses, so a window change cannot silently shrink coverage?
4. P1/P2 only, else "none found".

## Inputs
show:50803207:tools/reel/manafold_probe.cpp
