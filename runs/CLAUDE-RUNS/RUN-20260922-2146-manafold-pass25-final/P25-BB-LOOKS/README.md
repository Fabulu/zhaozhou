# Pass 25, back-ball packet: what each plate decides

Every plate is PRODUCTION INK (`ZIXX_EXP=celmain`,
`ZIXX_LIGHT=diagonal-cool-cross`), built with the committed `tools/reel/plates.py`
through the committed `rgbframe` reader -- never a hand-rolled one.

Frames are CONSECUTIVE, not evenly spaced, on every A/B plate. The complaint is
about motion between frames, and a plate of every fourth frame cannot show it.

| plate | what it decides |
|---|---|
| `01-hover-backball-BEFORE-AFTER-4x-consecutive.jpg` | **the packet's verdict.** Six consecutive Hover frames at 4x, undamped above and shipped below. Before: the rear rod's width and lean change every other frame. After: it holds one shape and drifts, while the loop still visibly breathes. |
| `02-hover-gain-ladder-3x-consecutive.jpg` | the gain chosen. Four consecutive frames x four rungs (0 / 500 / 700 / 1000) on the ORBITING camera the subject actually uses. 1000 reads carried; 700 reads calm and alive. |
| `03-fixedcam-ladder-6x-consecutive.jpg` | the same judgement on the FIXED three-quarter camera, where every moving pixel is the creature and not the orbit. Eight consecutive frames x three rungs at 6x. |
| `04-fixedcam-whole-creature-ladder.jpg` | that the antenna still reads as one coherent living loop at every rung -- no rip, no detachment, no mush -- across four points of the clip. |
| `05-fixedcam-ladder-worst-jerk-frames-5x.jpg` | the three WORST samples by measured back-ball jerk, sampled by badness rather than by index, across all five rungs. |
| `06-inspect-BEFORE-AFTER-tighter-camera.jpg` | that the second subject on this bake improves the same way at its closer framing, and that its front ball, eyes and body are where they were. |
| `07-hover-ALLFRAMES-production-ink-after.jpg` | all 600 shipped frames: continuous, no blackouts, no pops, the loop seam closes. |

## One plate deliberately NOT kept

A per-pixel temporal sweep (max-min of luminance over a window of consecutive
frames) was built to picture "how much does this region move". It read
87.5 / 87.7 / 88.1 / 88.3 across the WHOLE ladder -- flat, confident and
useless, because the mana lightning moves independently of anything the damping
touches and saturates the measure. Discarded rather than quoted. The judgement
was made from the consecutive-frame plates above, and the numbers that check it
are 3D, on the posed surface, in `P25-BB-RECEIPTS`.
