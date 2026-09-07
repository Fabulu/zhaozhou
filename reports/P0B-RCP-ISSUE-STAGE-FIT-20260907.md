# P0-B fit — and the seed noise that decides how to read it

`zhao_raster_rcp24_svc@p0b-s1`, commit `586a2cab`, 2,895 s.
Baseline `zhao_raster_rcp24_svc`, commit `e85da610`.

| | baseline | @p0b-s1 | delta |
|---|---|---|---|
| ALM | 1,041 | **1,200** | **+159** |
| registers | 1,101 | **1,037** | −64 |
| DSP | 6 | 6 | — |
| reported Fmax | 68.46 | 64.89 | −3.57 |
| core→core | 80.33 | 72.64 | −7.69 |

## The Fmax numbers do not support a conclusion, and here is the proof

The ledger already contains two rows for this block that share **the same source
commit** `1c0a7f44` and differ only in fitter seed:

| row | reported Fmax |
|---|---|
| `zhao_raster_rcp24_svcseed2` | **68.63** |
| `zhao_raster_rcp24_svcseed3` | **63.93** |

**4.70 MHz of spread on identical RTL.** My change moved the reported number by
3.57 MHz, which is *inside that band*. So this fit does not establish that the
S1 register made the block slower, and it would not have established the reverse
either. A single-seed comparison on this block cannot resolve a difference this
small, and saying "the scheduler change cost 3.6 MHz" would be a measurement
claim the measurement does not carry.

This is the same lesson as the `129.18 → 114.00` line earlier today wearing
different clothes: the number moved, and the reason it moved is not what the
number says.

**What IS outside the noise: ALM +159.** Area is far less seed-sensitive than
Fmax, and a 15% area increase for one pipeline register is a real cost that has
to be paid for by something.

## The structural prediction held, and it names the next step

Recorded before the fit: *"no worst path of the shape `c_val[..] ->
c_m.raddr_a[..]`; if that family survives, the register did not break the cone."*

| | worst internal path |
|---|---|
| baseline | `m1_i_q[1] -> c_m.raddr_a[0]~7` |
| `svcseed2` | `c_val[4] -> c_m.raddr_a[2]~3` |
| **`@p0b-s1`** | **`c_val[0] -> Add7~21`** |

The `c_m.raddr_a` **endpoint is gone**. The register did break the
selection-to-context-read-address hop, which is exactly what it was for.

What did NOT change is the **launch point**: `c_val` still starts the worst
path. The NCTX-way priority scan over `c_val && c_pend` is still slow; it now
feeds an adder instead of a RAM address port.

§5.2 anticipated precisely this and says what to do about it:

> If the remaining local eligibility search itself is still too slow, use
> two-level arbitration: small fixed context groups supply registered heads,
> then a central round-robin selects among those heads. **Do not immediately add
> hierarchy, replicated queues and a new context CAM before the first measured
> cut establishes what remains.**

The first measured cut has now established what remains. The scan is the
remaining cost, and two-level arbitration is the sanctioned next move — not a
guess, and not something to have built first.

## ANSWERED — the composed fit landed, and it reversed the reading

`zhao_texture_island_top@p0b-island`, 9,364 s, same change, measured in
composition:

| | island before | island after |
|---|---|---|
| reported Fmax | 66.77 | **78.80 (+12.03)** |
| ALM | 13,601 | **13,615 (+14)** |
| worst-path slack | −3.243 | −2.690 |

**Both of this report's leaf-fit readings were misleading, in opposite
directions:**

* The leaf fit showed reported Fmax **−3.57 MHz**, which this report correctly
  refused to call a regression because it sat inside a 4.70 MHz seed band. In
  composition the same change is **+12.03 MHz** — six times the ~2 MHz bar
  pre-registered in G1-D §4.3f and 2.6× the seed band. The caution was right and
  the leaf number was worthless.
* The leaf fit showed **+159 ALM**, which this report called "outside the noise
  and a real cost". In composition the island grew **+14**. That statement was
  wrong: area is less seed-sensitive than frequency, but it is not less
  CONTEXT-sensitive, and a leaf fit priced this register at eleven times what it
  costs among its neighbours.

**The correction worth carrying forward** is not "leaf fits read low on
frequency". It is that a leaf fit measures a block wired to PADS, and both its
timing and its area answer a question about that boundary rather than about the
design. This report's own closing line — *"the next honest measurement is a
composed island fit"* — was the right call, and it is now discharged.

## What this does not decide

The island's −3.243 ns family is the number that actually matters, and only a
composed fit can report it. A leaf fit's reported Fmax is gated by port paths a
block boundary cannot avoid, and this block's own seed spread is wider than the
effect being looked for. **The next honest measurement is a composed island
fit**, where the RCP sits among its real neighbours and the seed noise is
diluted across a much larger design.

Correctness is unaffected and was verified before the fit: 5 checks, exactly
4.00 multiplier launches per reciprocal (no duplicate-select), and
`island_composed_directed` 119 checks green.
