# FINDINGS — HUDBAND (entry I17's HUD half, structure 3)

**2026-09-21. Branch `gz/hudband` at `111d69c5`, merged. ZERO FILES CHANGED.
Register 22 → 22.**

**A packet that built nothing and settled the entry.** Transcribed by the
coordinator; the harness refused the lane's own write.

---

## 1. The arithmetic, and why it is trustworthy

For an **L-line** store at **384 wide × 17 bits** (`hud_rgb_i[15:0]` plus
`hud_valid_i`, read off the port list), the floor over **all six** Cyclone V
aspect ratios is exactly:

```
  ceil(0.75 x L)   M10K
```

**It was cross-checked against two numbers this tree stated before the lane
arrived:**

* **L = 240 → 180**, reproducing R222's own table *including* its 360 / 207 /
  204 rows;
* **L = 9 at 16 bits → 7**, which is **`POST.COMPOSITE`'s contract figure** for
  its nine-line ring.

> **A formula that reproduces an independently authored contract number is
> worth more than one that only reproduces itself.**

**The band needs L = 16 → 12 M10K.** And **the 17th bit is free at every L** —
depth binds, width is slack at 512×20 — **so there is no reason to reach for a
transparent colour key.**

### The coordinator's 48 was the right number by the REFUSED method

R222 offered *"~48 M10K at B=32"*, flagged as shape arithmetic. **The number is
reachable**: B=32 double-buffered is L=64, and `ceil(0.75 × 64) = 48`. **But the
route written down — `384·B·17·2 ÷ 10,240` — gives 40.8**, which is **logical
bits ÷ capacity: the exact inference R222 had refused three paragraphs earlier**
as the thing that made 153 unreachable.

> ***"Right number, refused method"* is how a bad practice survives a correct
> answer.**

## 2. The trickle is avoided, and the defect was never in the buffer

```
  structure 2 (line ring)   sum of HEIGHTS x a line-time = 122,880 clk = 133% of frame
  structure 3 (band)        sum of AREAS                 =  10,240 clk =  11.1%
```

**A 9× margin.** Structure 2 spent a whole **384-clock line-time on 32 pixels of
work**.

**And the two can use the IDENTICAL memory.** What killed structure 2 was
**TWOD.SPRITE holding `busy_q` for one whole descriptor — descriptor-major
order, not ring size.**

> **A structure rejected for the cost of its memory was actually rejected for
> its WALK ORDER, and the note said "ring".**

**B does not have to be as tall as the tallest sprite** — the cursor is exactly
what removes that. B and L are set by a **leaky bucket** (rate 384 px/line,
burst `(L−B)·384`): a full-width 32-row status bar sits **exactly at rate**, and
40 glyphs over it need 10 lines of burst. **B=4, L=16 clears it with 12 M10K.**

**The cursor needs NO multiply** (it accumulates across bands as the sprite
already accumulates across rows), and **the scanout address is a COUNTER,
because `hud_req_*` is a monotonic sweep — not the random access I17 calls it.**

## 3. ALM

**~595, band 500–900, ZERO DSP.** ~700 flops counted structurally, times the
**measured** `0.849 ALM/register` from the console's own fit row.

**The sort / Y-bucket the brief anticipated is NOT needed** — a 64-descriptor
extent re-walk is **4.2% of frame at B=4**.

## 4. THE FINDING IT DID NOT GO LOOKING FOR

**R222's premise was false.** It refused the frame store partly because *"no fit
has ever measured this console's M10K occupancy."*

**`reports/OWNER-DECISIONS-20260920.md`, dated the day before, line 14:**

```
zhao_console_core@console-core-first-light,  47,582 ALM / 151 DSP / 306 M10K
```

**R222's closing sentence cites 47,582 ALM — from that same row — while
declaring the M10K absent.** *One row, two columns, one quoted and one called
missing.* The same document already reasons with it at line 766: *"against 306
of 553 already used."*

**Carrying every caveat — dirty tree, no FIELD, "do not quote it as the
console's size" — they all UNDERSTATE, so 306 is a FLOOR: usable to REFUSE,
never to LICENSE.**

```
  306  floor
+  38  FIELD's two clean leaf rows
= 344 / 553

  the double-buffered frame store, the form that actually works:
  704 / 553   OVER THE DEVICE
```

**And it cannot coexist with the 185-M10K TERRAIN deviation store** the same
dossier puts to the owner. **So the refusal gets STRONGER, not weaker.**

**Also: that row ran on `5CEBA9F31C7`, not the target part — its printed
percentages are against the wrong device by 2.2×.**

## 5. Built: nothing, deliberately

**No RTL, no `blocks.yml` row, no contract** — searched **with the scope named**:
127 contract files, a tree-wide grep, two TWOD rows in `blocks.yml`. R214's test
is not met until a contract exists.

**The design REUSES `zhao_twod_sprite`** via band-clipped descriptors rather
than growing a second walker. *"I17's risk this time is rebuilding what exists,
not missing it"* — the entry has twice recorded built blocks as missing.

**The descriptor half is untouched, and one thing to add to that decision: the
band imposes NO new requirement on the record.**

## 6. Gates

**Change class is *zero files modified***, so no smoke form, port gate or
wrapper mutant can change its answer, and Quartus is forbidden. **The Always
list was run anyway, all 13 RC 0, as proof the tree measured is the tree at
head.**

Plus `check_ram_inference.py` **run live with a positive control** — and the
committed **`RAM-INFERENCE-SCAN.txt` is STALE** (no `post_composite` section, no
WEAK SIGNAL annotation). `ring_q`'s only flag is the tool's own known false
positive, **and that flat one-write-address shape is the band buffer's exact
shape, sitting inside the 306.**

## 7. What I17 still needs

1. **An owner choice among three now-costed structures** — *(coordinator: taken
   as R233; the band is ruled, because the HUD looks identical under all three
   and only cost and schedule differ, so no picture was owed.)*
2. **A TWOD.BAND contract before any row.**
3. **The descriptor record.**
4. **A leaf fit to turn the 12 into a measurement.**

**And one honest price the lane put beside the band:** *its admission test is
stricter than a frame store's, and must be decided **before rasterising**,
because `TWOD.SPRITE.md` forbids partial sprites.* **→ Ruled R235: refuse the
sprite whole, and COUNT it.**
