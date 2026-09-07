# 17 bits per entry of the residency status word are not in the fitted design

*2026-09-07. The gate `min_memory_bits: 167936` was about to be relaxed on the
reasoning that the missing bits were "almost certainly MLAB". The fit report
harvested to answer that question refutes it. **The gate is correct and the
design is not.***

---

## The arithmetic, every term verified from the source

```
SETS = 256    WAYS = 4    PINW = 6    SEQW = 16
STATW = PINW + 3 + 32 + SEQW                     = 57
KEYW  = 32 + 16 + 16 + 32 + GENW + 3             = 107

declared   statram  57 x 256 x 4 =  58,368
           keyram  107 x 256 x 4 = 109,568
                            total = 167,936      <- the rule's threshold, correctly derived
inferred                          = 150,528
shortfall                         =  17,408      = 17 bits x 1,024 entries
```

## The inferred memory, exhaustively

Every `altsyncram` row in the RAM Summary, not a sample:

```
  g_bank[0..3].keyram   depth 256  width 107  size 27,392 each
  g_bank[0..3].statram  depth 256  width  40  size 10,240 each
  TOTAL                                               150,528
```

The total matches the fitter's own `blockMemoryBits` exactly, so **there is no
ninth memory**. `keyram` gets its full declared 107 bits; `statram` gets 40 of
its declared 57. Quartus states the inference without complaint:

```
Info (276029): Inferred altsyncram megafunction from "|altsyncram:g_bank[0].statram[0][56]__2"
  Info (286033): Parameter WIDTH_A set to 40
  Info (286033): Parameter NUMWORDS_A set to 256
```

No `Warning`, no truncation message, no uninferred-RAM note naming `statram`.

## The 17 bits are not anywhere else

| candidate | evidence | verdict |
|---|---|---|
| MLAB / LUTRAM | `[d] ALMs used for memory = 0`, `Memory LABs = 0` | **no** |
| flip-flops | 1,226 registers against 17,408 bits | **no** |
| optimised away as unused | "Registers Removed During Synthesis" lists **3**, all `s0_ev~n`, "Lost fanout" | **no** |
| a second memory | RAM Summary totals to the fitter's own figure | **no** |

**That fit report exists only because it was added to the harvest this morning,
after this same gate could not be judged.** The note it was added to answer said
the bits were "almost certainly MLAB, at 17/40 utilisation being cheaper than
another M10K". `Memory LABs = 0` settles that: they are not.

## Every benign explanation checked and eliminated

* **The fields are written from constants** — no. `swd_c = s_pack(..., s0_crc,
  s0_seq)` and `s0_crc` is input-derived.
* **`s_pack` does not fill the word** — no.
  `s_pack = {pin, bd, f, mips, crc, seq}` is 6+1+1+1+32+16 = **57 bits**, no
  padding.
* **Nothing reads the fields** — no. All six accessors have call sites, and two
  are *comparisons*: `s0_crc != s_crc(s)` and `s_pin(s) != {PINW{1'b1}}`.
* **The fields never actually change, so synthesis proves them constant** — no,
  and this was the last one worth checking because it is the only mechanism by
  which a tool may legitimately drop storage. `EV_PIN` writes
  `s_pin(s) + PINW'(1)` and `EV_UNPIN` writes `s_pin(s) - PINW'(1)`, so the
  pin count genuinely varies; `EV_*` also ORs `s0_bd`/`s0_f`/`s0_mips` into
  the flags. Nothing in the word is a fixed value.
* **Quartus complained and it was missed** — no. The map report's entire
  warning set is 4,001 virtual-pin notices, one summary of them, and one about
  a clock port fed by a virtual pin. **No stuck-at, no undriven net, no
  truncation, no uninferred-RAM message naming `statram`.** The tool is silent,
  which is itself the most uncomfortable part of this.

## What the missing bits are

The split is at 40, and the word is `{pin[6], bd, f, mips, crc[32], seq[16]}`
from the top. The low 40 bits are `seq(16)` plus **24 bits of the CRC**. So the
17 that are absent are:

```
  pin           6 bits    the concurrent-pin count
  bd, f, mips   3 bits    modified / dirty / mips-stale
  crc[31:24]    8 bits    the top byte of the page CRC
```

**Eight bits of a CRC, on the block whose job is validating page residency.**
The split does not fall on a field boundary, which is what makes it look like a
width truncation rather than a design decision.

## What this does NOT establish

* **Not that the block is broken in silicon.** It establishes that the fitted
  netlist has nowhere to hold those bits and that no report accounts for them.
  A post-fit netlist simulation would settle it and has not been run.
* **Not that Quartus is at fault.** A tool silently dropping storage would be
  extraordinary; the likelier reading is still that something in the RTL makes
  the bits provably redundant in a way three checks did not find.

## The experiment that settles it, and it is cheap

Fit a variant with `statram` **declared 40 bits wide** and everything else
identical.

* If the resource numbers come out **the same**, the top 17 bits were never in
  the design and the RTL as written does not describe what is being built.
* If they come out **different**, the 17 bits are present in the current fit in
  a form no report names, and the reports are the problem.

Either answer is worth having, and it costs one block fit.

## The immediate consequence

**`min_memory_bits: 167936` stays exactly where it is.** It was derived
correctly from the declarations, it is failing correctly, and the note beside it
that reasoned toward relaxing it is now wrong on its central claim. A gate about
to be relaxed on a diagnosis its own evidence refutes is the failure this
repository has recorded twice already — `min_m10k: 17`'s capacity argument that
ignored MLAB, and `max_m10k: 0`'s scope error on GEOM.PROJECT. This is the
third, caught before the relaxation rather than after.
