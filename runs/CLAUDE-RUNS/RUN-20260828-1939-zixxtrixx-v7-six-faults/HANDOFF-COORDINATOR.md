# HANDOFF — written by the coordinator, from artefacts

The v7 agent was about to hit its limit. It was asked to write its own
`HANDOFF.md`; **this file is the independent version, reconstructed from committed
artefacts so the handoff does not depend on a dying process finishing a document.**
If `HANDOFF.md` also exists, read both — that one carries what was in its head,
this one carries what is on disk.

## State at handoff (2026-08-28 ~22:15)

**All gates GREEN**, from the run's own `final-*.txt`:

| gate | result |
| --- | --- |
| `zixx-probe` | 8,251 hits, all within authored allowances |
| `zixx-choreo` | root decomposition reproduces the approved salto |
| `zixx-planner` | golden preset preserved, plans hit locked intercepts |
| `zixx-striketip` | nose→tip reach 3911 mm, angle −73.4° (the pose's own weapon vector) |
| `meshcheck` | **OK** — a NEW gate this run |
| `zhao-reel --check` | all sequence CRCs match |

**24 webm on disk**, newest rendered minutes ago — up from 17, so the experimental
subjects are rendering. **Not deployed.** 37 evidence frames committed.

**Work is COMMITTED through `3278dd9` (22:13).** The uncommitted files in the
zhaozhou tree belong to the FPGA lane (`zhao_field_v3_svcpath.sv`, field sweep
logs, `captures/failures/*`), **not** to the modelling work — do not assume they
are yours and do not commit them.

## What landed this run (from commit messages + evidence)

* `sidecmp-19-final.png` — the side gate carried to its 19th iteration.
* **`meshcheck` is a new committed gate.** `4b3803d` records the hit fold's
  two-key crush as a **DECLARED exception** (4.93× / +291 mm, measured, worst-key
  renders judged) with the gate set just above it. That is the right shape: the
  owner asked to "really bend the hit part out of shape", so the deformation is
  authored and the gate documents it rather than forbidding it.
* `walk-junction-final.png` — walk junction reported seam-free (owner item 4).
* `6dc0e46` — fall cameras re-widened for `kFallLift 1371`; the loop had been
  **cropping the frame top**, which is a shot fault masquerading as an animation
  fault. Compare `fall-it4` vs `fall-it7` sheets.
* `ae41d98` — experiments: **even blur kernels broadcast-crashed the tooth page**;
  the noise helper now forces odd kernels. A real trap, already paid for.
* `exp-contour-*` evidence exists, so the contour experiment has begun.
* `harvest.py`, `run-experiments.sh`, `run-site-clips.sh`, `closeout.sh` — the run
  built its own tooling; reuse it rather than reinventing.

## The 17 owner items — the queue

Priority order: **head/neck block first** (it is what he checks first every time),
then the rigidity pass, then the salto family, then the experiments LAST.

1. **Fins**: unrotate the blades, **roll the tail end instead** so they inherit it.
   Keep "further apart". (The 80° instruction was mine and ambiguous — he meant
   the tail.)
2. **Flat snout must round** — from the terminal rings of the one continuous tube.
   **No head-shell overlay**: "without attaching a weird helmet" is a hard
   constraint; that overlay was deleted once for self-intersection.
3. **Eyes** forward, DOWN a little, more bulge, still side-mounted. Stay between
   the two recorded failures: bulge 42 became a lateral BRIM (head read as a hat);
   the orange rings once MET across the crown (a chinstrap).
4. **Neck seams**, pose-dependent → geometry or normals, not texture. Diagnose
   **unlit + normal-viz** first. (`walk-junction-final.png` suggests progress.)
5. **Too fat behind the neck, for too long — "you barely see the S anymore."**
   A profile SHAPE fault; the uniform girth-850 ladder cannot fix it. Shorten the
   fat run, steepen the drop. Law: body slimmer → **neck widest** → head second.
6. **Tail-balance fall** rigid "like a stick"; clip generally "wonky".
7. **Taunt** rigid — "all parts of body should bend."
8. **Hit**: wobble is APPROVED, impact is not. Deformation at the struck stations.
9. **Salto camera jitter** — likely tracking a point on the SPINNING body; track
   the ballistic path instead.
10. **Six-salto is not an elegant wheel.** Continuous theta was necessary but not
    sufficient: **the curl must be HELD while theta advances.** Fallback he
    offered: take the authored 3-salto's curves and give them more revolutions.
11. **Wings on the flying target** — reel-only prop, cheap, keep out of the site
    card and page set.
12. **Mouth up toward the snout.** `Front.png` GOVERNS marking placement (it is
    only distrusted for FORM). Do not widen it — it was once 101° of circumference.
13. **Stray triangle, right eye, `death2`.** Pose-dependent → suspect SKINNING (a
    mis-bound vertex is invisible until a pose separates the bones). Find it with
    a committed probe; **do not paper over it.**
14. **Taunt gets the Indian head-wobble** — a ROLL/tilt, not a yaw. Check the
    head-aim bone even supports roll. Deliberate style exception: **fast crisp
    gesture over a slow loose body.**
15. **Texture experiments — a MENU, not a blend.** Each effect ISOLATED: contour,
    directional strokes, wax build-up, paper tooth, wobbled boundaries, drawn
    markings, mis-registration, boil (temporal, derived from the KEY not
    wall-clock), cel shading. **Plus** cel+contour combined, **plus** thick-outline
    cel and thick-outline normal. Two cel weights (hard two-tone, soft three-tone).
16. Palette and the circumferential colour law are FIXED. Paper white / graphite /
    ink black are colours of the MEDIUM, not the creature.
17. Normal creature must render **byte-identically** through all experiments.

## Standing laws

Side.png owns FORM; Front.png owns MARKINGS. Author by eye, render, look, compare.
Wobble is not jitter. Ground contact authored, measured with the probe never
inferred from a frame. Goldens cmp'd **against the folder of record**, never a
staging copy, with provenance on deliberate re-pins. Never `cmake --build`.
`--check` redirected to a FILE. `zlib.crc32`, never process-salted `hash()`.
Do not touch `C:\programmieren\sacengine`. Never delete
`Upheaval/website/public/renders/archive-2026-08-2[78]-*`. **Do not run
deploy.ps1** — the coordinator verifies and publishes.

Outline thickness: decide deliberately whether it lives in the texture (shrinks
with distance, mips away in the shot that matters) or as a render-time silhouette
(consistent on screen), and say which and why.
