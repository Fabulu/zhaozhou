# P19 review/QA scratch notes
## Source review
- Legacy toggles: legacy-root returns authored unchanged; hinge_play amb==1000 path exact; knead wag x*1000/1000 exact. Midpoints: nlerp of composed keys (hemisphere-safe), rear_relative recomputed per midpoint. Sound.
- R1 rel operand is structurally tautological: HingeD world = Root*qd and RearSocket = Root*qd*Authored, so rel == |Authored| whatever qd is. Blind to any fault in qd (both sides move together). The centreline-bend half of R1 (skinned ring centroids) is independent and is the real detector. Legacy control fires because compose is bypassed.
- R2 measured | |rel_i| - |rel_i-1| | = change of bend MAGNITUDE, a lower bound; blind to constant-magnitude sweeps. FIXED to true step angle(P^T Q). Worst 2.39 -> 2.76 (Hover s225). Controls: frame 7.16 (0x3), joint x3 7.99 (0x2).
- Ambient ladder under true step: 600 -> 4.07, 650 -> 4.40, 1000 -> 6.67. Ceiling 4.0 sits on a looked-at-and-rejected rung = art lock.
- R3 was a pure-function law test; production producers never exercised. ADDED census over Drift+Hover (line and non-line populations) + --fail-line-flag control (0x4 only).
## Look 1-2 (Inspect overview, rear 3x f0/20/40/218)
- v18: stepped second piece beside the entry (f20, f40), stub juts beside the arm (f218). Ship: one continuous tube entering the body in all four. Item 1 FIXED.
- Ship End: only a gentle flare at the waterline; no ball read. Coordinator concern looks real; compare against A/B/C next.
## Look 3 (Inspect f300/340/560 2x ship vs v18)
- v18 f300/f340: forked, torn rear strut. Ship: one clean strut into the body. A/B/C read as mild corner knuckles; End reads as NOTHING (faint flare). Direction 19 item 3 violated for End -> ladder.
## Look 4 (End ladder, Inspect f300/f40 2x)
- ship/B(swell 36/45): flare only. C(ball 2560/150, 30/36): rounded swelling just proud of the body, reads as a ball in family with A/B/C. D(2590/120, 26/34): similar, smaller/higher. E(42/50): bold knob, edging toward bead.
## Look 5 (4x f300): ship plain strut; D barely registers; E hard shoulder/knob; C soft shoulder into a rounded swelling, in family with A/B/C. CHOOSE C (2560/150, 30/36) pending motion check.
## Look 6 (Hover/Rest native, lines 360 vs legacy)
- Same figure, white core and navy surround intact, a touch less halo at 360. Proportional to the smaller creature, which is the ask. KEEP 360 (285 would hold mid-distance lines at close-up weight).
## Look 7 (Drift 2x, 360 vs v18): v18 = fat glowing blob over the antenna (f120/180/250). 360 = fine outlined loop at the creature's scale. Item 3 FIXED.
## Look 8-9 (Hover swallow f366-400, 2x and 4x rev vs pre-review ship)
- The right strut turns at the entry through f377-392 as a rounded elbow; outline continuous, no crease/tear. With the End ball it reads as the ball joint turning. ACCEPT 35 deg as the authored beat (lever kSwallowEndJointA16PerMm if the owner wants it softer).
## Look 10-11 (Rest f342, Taunt3 f328, Trick f393, Channel f210, Lasso f170; 2.5x rev vs v18)
- Rev: every rear entry is a smooth rounded thickening into the body; v18 shows the entry nub/notch (Rest, Lasso). Trick ~identical but for thinner lines. Connection whole on every clip.
## Look 12-13 (Pirouette f42-49, Inspect f208-232 every ~3 frames, rev vs v18)
- v18: extra stub flicking beside the rear base (Inspect f208-224); a jag at the base (Pirouette f44-47). Rev: one strut descending smoothly, gentle thickening at the entry, nothing flicks. Rear calm but alive. Item 2 FIXED.
## Look 14: Hover every-frame sheet (rev): continuous, no pop/dropout.
