# Q003 review2-trick-360-design
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou
review: Q001

## Brief
Check exactly these four claims from the answer, nothing else:
C1. Section 2: pre-multiplying `quat_y(spin)` onto `g.q[kBRoot]` keeps the planted antenna support in place, because the support sits at the root's XZ. (Consider: the planted-support pivot in the code corrects only root Y; where is the support relative to the root in X/Z during the headstand?)
C2. Section 3: every key literal that must move when the plant is lengthened has been identified (kFlip, kRootY, kGazeDown, kSquash, kBalFade arrays). Look for any OTHER hard-coded key numbers in build_trick that depend on the old timing.
C3. Section 4: the two quintic segments give zero velocity and acceleration at the start, the join and the end.
C4. Section 4: at p = 1040 per-mille, theta = 68172 in angle16 units (65536 = 360 degrees).
Then list at most three important things the answer missed.

## Questions
1. C1..C4: CONFIRMED / REFUTED / UNVERIFIABLE with evidence.
2. Up to three misses.

## Inputs
