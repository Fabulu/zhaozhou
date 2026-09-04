# Direction 30: the animation is untouched — evidence

Bounded to the acceptance question, three independent lines:

1. **zixx-probe: PASS** (committed pose/contact probe, every key + midpoint,
   declared 3D contact, balance/taunt/fall/impact/spring/jump/limit/overlap
   gates), run on the Direction 30 build before media work and re-run before
   publish.

2. **Gate-off CRC identity, all 22 subjects** (`evidence-gateoff-identity.txt`):
   the Direction 30 build with ZIXX_SUNS=off reproduces the pre-suns bank's
   per-subject sequence CRC32C exactly, including moving-light 0x65A8D1E5
   (the published multiplicative clip). The lighting change is inert when
   gated off, so it cannot have moved a vertex.

3. **Ink-mask silhouette identity, every frame of every subject** (committed
   `diag/inkmask.py`, ink colour 26/24/22): the set of silhouette-ink pixels
   is identical frame-by-frame between the shipping suns-on render and the
   pristine pre-suns render for all 22 subjects (6674 sun-clip frames plus
   600 moving-light frames). Identical ink set = identical silhouette =
   identical motion; only shading inside the silhouette changed.

Frame counts of the shipping render match the published bank exactly
(attack 560, balance 493, damage 400, death 192, death2 240, fall 288,
hit 300, hitfloor 188, idle 576, jump-multi 241, jump-one 241,
knockdown 196, look 384, moving-light 600, run 96, salto-dummy 295,
salto-fly 313, salto-nine 435, salto-six 333, slow-taunt 239, taunt 224,
walk 160).
