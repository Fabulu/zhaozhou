# Review of R2 — he is right about my instrument, and wrong about what it hides

**Reviewer:** repository agent, 2026-09-28. **Branch head reviewed:** `0e749b8f`
(unchanged, as R2 states). **Verdict: five corrections accepted, one central
claim rejected on repository evidence — and the correct figure is worse for both
of us than either document says.**

---

## 1. Accepted, verified, and my error

### 1a. The tool bug is real

`map_entity_attrib.parse(path: str)` takes a **path**; `v2_state_lever.load_rows`
passed it `open(src).read()` — the **contents**. The advertised raw-`.map.rpt`
route never worked. **The JSON route did, and produced every published number, so
the figures are unaffected** — but a usage nobody exercised is a usage nobody
could trust. Fixed; the raw route now parses 1,758 nodes to the same totals.

### 1b. The M10K figure is wrong, and the rounding was the small half

He reports that 74,646 bits was floored to 7 where capacity needs 8. Correct
(7.29 → 8). **The larger error is the unit.** Quartus does not pack two arrays
into one M10K, so **N small arrays cost N blocks regardless of total bits.**
Measured at HEAD: **88 live arrays, ~128k declared bits, ≥88 blocks — not 8.**
A 12× error, and he pulled the thread.

### 1c. The classifier is hierarchy-sensitive — and worse than he argued

His synthetic counterexample is real, and the live data is more damning. My
census **excludes a node's entire register count if any descendant holds a RAM**:

| | registers |
|---|---:|
| total `reg_own` | 279,210 |
| counted by my census | 132,083 |
| **excluded for having any memory in subtree** | **147,127** |

**It excludes more than it counts**, including the single largest holder,
`zhao_field_v3_exec` at **24,795 own registers**. It is neither an upper nor a
lower bound on bankable storage. `v2_state_lever.py` is now **deprecated as a
budget instrument** in its own header, pointing at the right one.

He is also right that "no DSP and no RAM" is not "event-rate housekeeping" — it
catches soft multipliers and dividers. My own evidence named
`zhao_field_v3_mulbank` (3,328 ALUTs, 8 registers) and then the architectural
paragraph quietly treated the class as coordination. **Invocation rates were
never established by those counts.** Accepted.

### 1d. "400 nodes rather than five engine rewrites" is a false choice

Correct. The concentration curve is **exclusive** attribution; replacing one
coherent engine removes many descendants at once. The curve says the mass is not
in a few nodes; it does **not** say how much one replacement reaches. R3 drops
the framing and keeps the curve with that caveat printed beside it.

### 1e. The workload is partly defined, and I checked

`design/contracts/FIELD.SEQ.EARTH.md` at the reviewed commit specifies 1,089
lattice vertices per full patch, **297** four-wide update groups (corrected from
273 on 2026-08-27), a 128-association stress frame, and a **frame acceptance
ceiling of ≤850,000 Field/Earth-slice clocks**, with backpressure and cache-miss
tests. My "no numeric envelope exists" was too strong: `V1-RELEASE-DEFINITION.md`
has none, **the contracts do**. Gate 0 becomes *extract and reconcile*, not
*start from blank paper*.

---

## 2. Rejected — the storage lever is ~5%, not 31%, and not more than 31%

R2 argues my 0.521 ALM/register is not an upper bound because two further
conversions measured 0.671 and 0.590, so storage "might recover about this much"
and possibly more. **Both of us are optimistic, and the repository already
settled this two days before either document was written.**

Commit **`7d049e9f`, 2026-09-26 — "The flop-array programme is DONE, and its own
work list was a ghost list."** Its recorded density table:

| conversion | bits | ~M10K | ALM bought | **ALM per M10K** |
|---|---:|---:|---:|---:|
| `zhao_geom_drawjob` pal_q (**done**) | 98,304 | 10 | 24,576 | **2,458** |
| `zhao_geom_lodstate` st_q (**done**) | 9,216 | 1 | 2,304 | **2,304** |
| `zhao_forge_assemble` pos_q+inv_q (**done**) | 34,840 | 4 | 8,710 | **2,178** |
| **the remaining tail** | ~36,280 | **~40** | ~9,070 | **~225** |

**Ten times worse, against ~238 free blocks.** R2's two extra data points *are*
rows two and three of that table — the conversions that already landed. He says
so himself ("already-spent savings… we must not subtract them again"), and he is
right; he simply does not carry the consequence, which is that **what remains is
the tail, not more of the same.**

Re-run at HEAD with the instrument that commit built for exactly this
(`check_ram_inference.py --rank --against=<map.rpt>`), 291 ranked arrays split:

| | arrays | declared bits |
|---|---:|---:|
| NOT IN THE COMPOSED MAP | 94 | 418,426 |
| ALREADY INFERRING | 109 | 397,901 |
| **live and still in flops** | **88** | **128,466** |

And the largest "live" row — `forge_cliff_ram prio_mem_r`, 65,536 bits — sits in
a module holding **562 own registers**, so it is plainly not in flops at all.
Excluding it: **~63,000 bits across 87 arrays, needing ≥87 blocks, worth
~15,750 ALM at 4 flops/ALM — ~181 ALM per M10K.**

**So the remaining storage lever is 4–7% of the design.** My 31% was wrong.
R2's "possibly more than 31%" is wrong in the same direction and by more.

**Why I made the error is the part worth keeping:** I built a census and read it
as a work list, four days after writing a commit whose entire lesson is *"its
NOISE is not a work list."* The instrument that answers this question already
existed, with an `--against` flag added for precisely this failure.

---

## 3. Accepted with a correction that strengthens his case

### The clock is not a free parameter — the contracts already fix a floor

R2 argues timing must be designed for, not assigned. Agreed, and his own citation
supplies the number neither document states:

**≤850,000 Field/Earth-slice clocks for the 128-association stress frame**, at
60 fps, is **51 MHz for the Field engine alone at 100% duty**, before any other
engine, before reserve.

That is a **ratified contract setting a hard clock floor**. R0's 60 MHz leaves
~15% headroom on one engine. The timing witness must be measured against **51 MHz
as a contractual minimum**, not 60 as a guess.

### The projector affine identity is sound and is not already exploited

`row(i,j) = A·h(i,j) + B·i + C·j + D` for `x = x₀+i·p`, `z = z₀+j·p` follows
directly from `m₀x + m₁y + m₂z + m₃`. It replaces three products with one plus
prepared stepping — **3 products per row, 9 per vertex, down to 3.**

I checked the premise he asked me to check: **`zhao_project_core` takes arbitrary
`vx_i/vy_i/vz_i` and does not exploit lattice structure.** The idea is live.

Caveats to carry: it applies to the **regular lattice only** (not creatures,
objects, giants); accumulator width must absorb stepping to i,j = 32 without
intermediate rounding — his deliberately early-rounded failing control is the
right instrument; `zhao_project_core` declares **contract latency fixed 36**, so
a variant path is a contract change; and the generic path must remain.

**He is right about the general point and it cuts at my R1:** nine products per
terrain vertex was not an architecture-independent lower bound either, and I
built a whole "rate-bound lever ≈ 0" argument on top of counts like it.

---

## 4. What I still hold against R2

**The residual framing.** He is right that my 108,856-ALM "unfunded residual" is
an extrapolation from V1, not a derived V2 requirement, and that converting ALUT
savings into ALM deductions without a placed result is not grounded. **Accepted —
R3 drops it as a V2 requirement.**

But the underlying difficulty does not go away by being reframed, and it is
*worse* than R1 said now that storage is ~5%: V1 is 222,666 estimated ALM against
41,910, and the levers anyone has measured total well under 10%. **Neither of us
should present a whole-machine forecast. Both of us should stop implying the gap
is smaller than the evidence supports.**

**The cluster-replacement experiment is the right one** and I accept it over my
E1. A distribution of conversion ratios cannot measure the value of deleting
duplicated ownership and transport — and per §2, the conversions are largely done
anyway.

---

## 5. Net

R2 improves R1 on instrument correctness, on the workload, on the false choice,
and on challenging operation counts. R1 improves R0 on stating the ratio and on
refusing an underived table. **Neither document has a funded forecast, and the
one lever both of us leaned on is spent.** That is the honest position, and it
makes R2's proposed next step — measure one coherent replacement end to end —
the only move that produces new information.
