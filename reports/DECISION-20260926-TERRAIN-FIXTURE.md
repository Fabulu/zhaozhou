# DECISION RECORD — the smoke fixture will SHOW TERRAIN, and `raster pixels=2560` will change

**Taken 2026-09-26 under `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`.**
Format per the directive: question, chosen option, reason and alternatives,
constraints and cost, consequences.

---

## The question

PROJCOLLAPSE measured why terrain draws no pixels in the composed console, and
the answer is **not a defect**. The projection is correct, the carriage is
innocent, and the cull is lawful:

* the played terrain pages have an **all-zero body**, so layer A (*"Top base
  height, 33×33, height16"*) is a **constant** — all 81 lattice vertices at
  world y = 0;
* `tests/prod/smoke_geom_fixture_gen.cpp:116-121` puts the eye at **world
  y = 0** and maps world Y to screen Y;
* so **the ground plane passes through the eye, and a plane through the eye
  projects to a line.** All three corners carry y = 8192 — exactly the viewport
  centre. The cross product is arithmetically zero.

Proven by reversal with the committed `-TerrainRelief` control: give layer A a
real height field and **`culled` goes 256 → 0**.

**And with relief they still do not draw**, because the patch is **sub-pixel**:
one metre of lattice is 18.5 subpixels, a 1 m cell is 0.072 px, and the whole
32 m patch is ~2.3 px wide at this camera.

**So making terrain visible requires moving the fixture, and that changes
`raster pixels=2560` — a reference-derived gate number.** PROJCOLLAPSE named it
and correctly refused to decide it inside a diagnosis packet.

## The decision

**The fixture will be changed so terrain is actually drawn, and the gate number
will move.** I am taking this rather than referring it up.

## Why, and the alternatives

**It is a technical decision and the delegation covers it.** The directive's
list of what the delegation does *not* cover is explicit — delete a feature, cut
16 fields to 4, remove Gouraud or detail normals, shrink the guaranteed giant,
replace a live path with testbench stimulus, waive a correctness failure, call
reduced work equivalent. **Changing a bench so it exercises a live path is the
opposite of the one that sounds closest**: it replaces testbench *silence* with
a real path, it does not replace a live path with stimulus.

**And I had been treating it as the owner's, which is the error this session has
already paid for four times.** `CLAUDE.md`'s new chapter says a refusal is an
instrument that goes blind in the flattering direction; deferring a decision
*feels* careful and is not audited the way a build is. I wrote that today and
then did it again within the hour.

**The alternatives, and why not:**

* **Leave the fixture flat and close I13 on counter evidence.** Refused. That is
  the disconnected-implementation shape the register exists to catch, and four
  packets have already refused variants of it.
* **Keep `raster pixels=2560` frozen and add terrain elsewhere.** Refused. The
  number is *derived from a generator that models no terrain*; freezing it makes
  the gate assert the absence of the feature being built.
* **Add an epsilon or clamp so flat triangles survive the zero-area test.**
  Refused outright, and PROJCOLLAPSE refused it too. The area is arithmetically
  zero from a *correct* projection. That would ship a wrong pixel past a gate —
  precisely the failure I13's two arithmetic laws exist to prevent.

## Constraints and cost

* **The reference must move with the fixture.** The oracle and the RTL must
  still agree bit-for-bit; a new pixel count is only trustworthy if the
  reference generator produces the same scene. **Regenerating the reference is
  part of the change, not a follow-up.**
* **The new number must be DECLARED**, with the old one and the reason recorded
  beside it, so the next reader sees a deliberate move and not drift.
* **The relief must be authored, not random.** It is a fixture, so it should be
  the simplest field that makes the geometry non-degenerate and legible.
* **`-TerrainRelief` already exists** as a committed control with its own TAG
  and `+define+`; the plain form's behaviour today is the negative control and
  should be kept as one.

## Consequences

* `raster pixels` changes from 2,560 to a new declared value. Every gate and
  document quoting 2,560 must be updated **in the same commit** — this file's
  own law about fixing the rules file when you fix the thing it describes.
* The terrain arm becomes genuinely exercised end to end for the first time,
  which makes the **mosaic consumer's dead RTL** (`mosaic_tile_w`/`tx_w`/`ty_w`,
  declared and connected, read by nobody) a *testable* gap rather than a
  documented one.
* **This does not by itself close I13.** I13 additionally needs a real mosaic
  consumer built and `zhao_terrain_normalmap` given a detail port on a composed
  block. Stated plainly so the packet is not mistaken for closure.

**Superseded by this record:** my own statement to the owner earlier today that
the fixture question *"needs a decision from you"*. It did not; it needed one
from me.
