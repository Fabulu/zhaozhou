# P21 look notes (written after each look, in order)

Renderer: `.tmp/p21-arch/bin/zhao-reel-cel.exe` built direct from branch
`manafold-pass21` @ 457cfe89, production ink (`ZIXX_EXP=celmain`,
`ZIXX_LIGHT=diagonal-cool-cross`). Subjects: manafold-inspect (600 f, CRC
matches pass-20 bank config), manafold-channel (420), manafold-taunt (280).
Probe: `P21-PROBES/manafold_p21_curvature.cpp` on slots 0, 2, 11, 5, 21.

1. **INSPECT all-frames sheet (600 tiles, 96x60).** Bank is complete, no
   blank/torn frame. Too small to judge joints; used only to pick view ranges
   (f60-150 and f270-360 show the loop in profile, f0-50 / f180-230 from the
   rear quarter).
2. **Concept/Side.png.** Three balls at the loop's corners, the bands between
   them drawn STRAIGHT and thick; the rear band runs straight down into the
   body; the front band emerges straight from the head. It is ball-and-stick.
   No band in the drawing curves between two balls.
3. **turn-heat-0 (ring x sample, Inspect).** Bright stripes at rings 17-19,
   24-26, 32-34 -- i.e. 1-3 rings BEFORE balls A (20), B (27), C (35), while the
   ball rings themselves are dark (straight). Then a forest of stripes on the
   rear: 36-39, 45-46, 49-50, 52, 55. Rings 21-23, 28-31, 40-44, 47-48 are
   dark: the rods between are straight. The picture IS "too many joints, not
   on the balls".
4. **rate-heat-0.** Front hinge stripes are dim (they move slowly, <= 4.6
   deg/sample). The rear stripes flicker: ring 36-37 flashes to 28 deg/sample
   at f73-74 and f219, ring 55 at f165/f47/f150. The End part's "spazz" lives
   in the rear polygon's corners, not in the End ball itself (End ball ring
   57: turn 0, speed <= 3 mm/sample).
5. **INSPECT-WORST-NATIVE (f73/74/79/165/150/47/219/285).** At native the
   rear return reads as a fat curled band with a lumpy elbow below C; f150/165
   show the loop with a "thumb" sticking out at C and bumps along the top run.
6. **INSPECT-REAR-EVENT-4X (f73-79).** The rear band leaves C, bulges outward
   (the bow), then kinks back toward the body with a second bump before the
   End: two visible extra corners on one run, and the outline steps at each.
7. **INSPECT-FOLDED-4X (f123/134/150/165).** With the loop squeezed (C-E chord
   ~350 mm) the top run bends mid-way (the pre-ball corner at ring 24-26
   reads as a knee before B), and the rear band folds back against B-C as a
   wedge. f134: the whole loop is a 6-7 sided polygon with corners that are
   not where the balls are.
8. **CHANNEL-TAUNT-EVENTS-4X (C076-078, T007/055/056, C019, T088).** Channel's
   rear event (f76-78, ring 36 at 23 deg/sample): the rear band's elbow below C
   changes shape between consecutive frames while the rest of the loop barely
   moves. Taunt f7/f56 (ring 55 at 16 deg/sample): the band into the body
   kinks into a wedge and back. The front corners on C019/T088 read as lumps
   slightly OFF the corner: the corner is one ring before the ball.
9. **INSPECT-FRONT-JOINTS-4X (f110/123/300/330).** In profile the loop is a
   polygon whose corners are ~1 ring before each ball; the ball sits on the
   straight piece after the corner. On f300/f330 the front rod bends visibly
   in its middle (ring 12, where the Neck rotation's blend begins) before the
   corner at A. The runs A-B and B-C themselves are straight.
10. **Attribution (probe, Inspect, per-ring max turn RATE deg/sample):**
    shipping r36=28.5 r37=24.3 r45=2.7 r46=5.3 r49=6.0 r50=8.0 r52=9.5 r55=11.3;
    REAR_BOW=legacy r36=1.2 r37=8.6 r45..r50=0.1-0.2 r52=4.0 r55=3.4 (and the
    mean turn at rings 45-50 goes to 0.0: those corners ARE the bow);
    End ambient 0 + C calm 0: r52=14.3 r55=15.6 (WORSE) -- the rotations pass 19
    damped are not the cause. The End spazz is the bow's shape law.
11. **Image budget:** 12 images looked at (sheet, concept, 2 heat maps, native
    grid, 5 crop plates, 2 heat maps re-read), all <= 1600 px JPEG q80 or PNG.
