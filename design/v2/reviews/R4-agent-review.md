# Review of R4 — he is right on every correction, and the contract says something worse than my wrong number

**Reviewer:** repository agent, 2026-09-28. **Branch reviewed:** `9ece4a0a`
(unchanged, as R4 states). **Verdict: all six corrections accepted and verified
in source. One of them replaces my number with a larger constraint, not a
smaller one. And the blind spot he found by reading a regex has a real 15,360-bit
instance inside the slice we both chose.**

---

## 1. The 51 MHz "floor" — he is right, my derivation was invalid

I wrote that `≤850,000 Field/Earth-slice clocks × 60 fps = a hard floor of
51 MHz`. **That converts a cycle CEILING into a clock MINIMUM, which is only
valid for an implementation that consumes the entire allowance.** A candidate
completing the same work in 500,000 cycles needs 30 MHz. The contract bounds
cycles from above; with the frame deadline it gives a *relationship* between
cycles and clock, not a floor.

He is also right that it is not even *sufficient*: at 850,000 cycles, 51 MHz
consumes 100% of the frame and fails any reserve. **Withdrawn.**

### But the contract states the frequency directly, and it is higher

`design/contracts/FIELD.SEQ.EARTH.md:48-53`, which neither of us had quoted:

> *"Designed for the shared **100 MHz** GPU domain; a lower private clock is
> accepted only after the COMPLETE Earth slice — not a leaf core — proves the
> deadline below with reserve. By the regenerated cost model, **~80 MHz is the
> lowest credible clock**; the measured v2 leaf **59.22 MHz is not** (108% of
> its own budget before integration)."*

So the ratified position is **100 MHz design point, ~80 MHz lowest credible, and
59.22 MHz already rejected by name.** My invalid derivation landed at 51 —
**below** the contract's own rejected value. The correction removes a bad
argument and leaves a worse constraint, stated rather than computed.

Against the measured record: **only 20% of 74 placed leaf fits reach 100 MHz;
55% reach 80 MHz**; a design's clock is its worst path; and the sole composed
console placement is 18.5 MHz. His demand that timing shape the implementation
from the start is correct, and better supported by the contract than by me.

He is also right that the test is **useful work per second, complete latency and
reserve together** — not MHz in isolation. R5 adopts that.

## 2. The scanner criticisms — verified in the code, both of them

### 2a. `ALREADY INFERRING` cannot mean what its label says

`tools/quartus/check_ram_inference.py:901`:

```python
note = ("   <- composed: %d own reg, %d mem bits%s"
        % (own, mem, "  ALREADY INFERRING" if mem >= bits else ""))
```

`mem` is the **module's subtree** memory; `bits` is **this array's** declared
size. His counterexample is the code: a module holding array A (8,192 bits, in
flops) beside an unrelated 16,384-bit RAM labels **A** as already inferring.
**My 94/109/88 split is therefore not a valid partition**, and I presented it as
one. Accepted.

### 2b. The declaration recogniser is blind to typedef'd arrays

`check_ram_inference.py:174`:

```python
DECL = re.compile(r"^\s*(?:logic|reg|bit)\s*(?:signed\s+)?(?:\[[^\]]*\]\s*)*" ...)
```

An array declared with a user-defined type never enters the scan. He named
`uop_t store[0:(CTX*PLAN)-1]` as the example and said its mapping remained to be
checked. **I checked it — see §4. He was right to flag it and right not to claim
it.**

## 3. Three further corrections, all accepted

* **"Quartus cannot pack two arrays into one M10K" is too broad.** The Cyclone V
  handbook describes **packed mode**: two eligible independent single-port
  memories share one M10K when each fits within half the block. Withdrawn as a
  universal law; it remains true that *width, ports, replication and
  fragmentation* underprice memory if you divide total bits by 10,240, which was
  my original error.
* **~15,750 ALM is capacity arithmetic, not a measured saving.** It is
  `bits ÷ 4`. It says nothing about how much selection logic disappears, how the
  remainder packs, or what the new memory interface costs. **The older density
  table shares the flaw**, so quoting it did not launder it.
* **Other ranked rows have the cliff row's problem.** A queue listed at 12,288
  declared bits in a module with 1,315 own registers and 6,144 memory bits
  cannot be 12,288 flip-flops. I caught it in the largest row and then treated
  the rest of the column as sound. Accepted.

**Consequence: I withdraw the percentage entirely.** Not "4–7%" and not a better
figure — *no percentage*, because no instrument here can produce one. His wording
is the right one: the known large in-place conversions are already incorporated;
the remaining candidates look less attractive; their net saving is not
established by this scan; and broader state reorganisation is a separate,
unmeasured question.

## 4. New evidence: the blind spot has a real instance, in our chosen slice

`zhao_field_v3_exec.sv:302` — `uop_t store[0:(CTX*PLAN)-1]`, 256 entries of a
60-bit packed struct = **15,360 bits**. One write address (`:1090`), one read
address into a register (`:1200`), never reset. The shape that should become an
M10K.

Measured on the 2026-09-28 console map:

| node | own reg | subtree mem |
|---|---:|---:|
| `zhao_field_v3_exec:u_exec` | **24,795** | 25,344 |
| `zhao_field_v3_rf:u_rf` | 6 | 24,576 |

**768 memory bits exist in the exec outside its register file, and no
`altsyncram` under `u_exec` carries the name `store`.** So the 15,360 bits are in
flip-flops — **~62% of the exec's own registers** — and neither my census nor the
ranked scan ever counted them.

**Hypothesis, now under test rather than asserted:** production indexes the array
with a 32-bit **signed** expression, `store[(int'(up_ctx_i) * PLAN) +
int'(up_pc_i)]`. To infer a RAM Quartus must prove the index lies in 0..255.
`fpga/rtl/synth/zhao_probe_uopstore.sv` (committed, lint-clean under `-Wall`,
Quartus-17 syntax checked) maps three styles: production casts, a narrow unsigned
`{ctx, pc}` address with bit-identical arithmetic, and a plain packed-vector
array as **positive control — read first, because if it does not infer the other
two say nothing.**

**Explicitly not claimed:** that the repair is free. An M10K read adds a latency
stage the issue path must absorb — a schedule change, not a rename. Nor
`15,360 ÷ 4` ALM of saving; that is the arithmetic R4 just corrected.

**This is one array, not a rescue.** Its value is that it is the first precise,
reproducible thing in four rounds, and it sits inside the experiment we agreed on.

## 5. Accepted without reservation

* **Field-first, with his comparison boundary** — prepared request → input
  generation and binding → admission → operand reads → short and long ops →
  result/status/presence capture → command-ordered reduction → publication. A
  shorter sub-experiment must **declare the work it omits** and may not claim
  the Earth deadline.
* **The candidate organisation:** immutable programs separated from mutable
  contexts (without reducing distinct-program capacity); uniforms carrying their
  own instance identity; banking driven by **real port demand**, with small
  ready/valid bitmaps and active operands staying in registers. **My blanket "no
  arrays in flip-flops" rule does not return under another name** — he is right
  to fence it, and `zhao_field_v3_rf` is the proof: 24,576 bits of memory with
  six registers, already correct.
* **Preserve operand bandwidth.** The exec documents a seven-operand supply
  requirement; a smaller register file that quietly serialises those reads
  surrenders the budget. This is the failure mode most likely to make a
  "smaller" engine worse.
* **Destination ownership until retirement** — result carries context and
  generation, and the destination is actually written, not merely observable.
* **One table, not another verdict paragraph.** Adopted verbatim as R5's
  acceptance format.
* **Stop the essay loop.** Agreed, and this is the last document from me that is
  not attached to a measurement.

## 6. Where I still differ, narrowly

**One point of substance:** he writes that R3's bleakness "comes from interpreting
the old design". True — and the interpretations were wrong in the direction of
*less* opportunity twice (phase-3's "no lever", R3's "storage settled") and
*more* opportunity once (R1's 31%). The lesson is not that the outlook is better
or worse; it is that **every estimate any of us has made without an instrument
has been wrong, and the sign has not been predictable.** That argues for his
programme, not for a revised forecast — which is why R5 contains none.

**One point of process:** the joint envelope. He is right that I should not hand
the owner a blank sheet, and that the bounded Field experiment must not wait on
it. Adopted. R5's envelope task is *extract, then present specific combined
profiles with consequences*, distinguishing an existing promise from a new
interpretation.
