# Q018 review-wave-f-gates
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent review of the CHECKERS added in commit d7d51171 (Manafold v18 Wave F): manafold_qa_p12.cpp (mqa) and manafold_probe.cpp. New controls: spin gain, spin ease, spin pivot, flight seam, plus whole-spin contact ownership. Two bounds were loosened and declared: a Trick-specific 240 mm per-key root-step ceiling (the body swings round the planted antenna at 178 mm/key), and the probe's travel check now leaves Trick's contact keys to the contact check. House rules: a detector whose two operands move together cannot fire; each positive control must fire ONLY its own category; never narrow a gate just to pass.

## Questions
1. For each new check: what are the two operands compared, and could they move together (structurally blind)? Is the spin unwrapped from the relative root quaternion against an independent no-spin reference?
2. Does each control (spin gain / ease / pivot / flight seam) fail only through its own category? Cite lines.
3. The two loosened bounds: is each scoped to Trick only (and only its contact keys), bounded by a named constant, and unable to hide a fault in another clip or outside the window?
4. P1/P2 only in FINDINGS; else "none found".

## Inputs
show:d7d51171:tools/reel/manafold_qa_p12.cpp
show:d7d51171:tools/reel/manafold_probe.cpp
