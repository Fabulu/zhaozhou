# The bilinear metadata disagreement, characterised

Packet C moved four of five asynchronous response-side reads onto the class
queue. The fifth — bilinear `sampmeta_m` — is held, and this is what is known.

## The measurements

| falsifier | result |
|---|---|
| bank vs tables, at the **common stream** (shadow) | **1,176 compared, 0 mismatches** |
| CLUT queue vs table, at `disp_clut_tok` | **792 compared, 0 wrong** |
| nearest queue vs table, at `disp_near_tok` | **192 compared, 0 wrong** |
| **bilinear** queue vs table, at `disp_bil_tok` | **768 compared, 32 wrong (4.2%)** |
| bank generation check | **0 mismatches** |

First disagreement, captured rather than inferred:

```
tok=29E01  (class 2 = BIL, slot 39, sidx 2, gen 1)
queue{ nib 0  fmt 1  fv 50  fu 80  bsel 0 }
table{ nib 0  fmt 1  fv 10  fu 30  bsel 0 }
```

**Only the fractions differ.** Nibble, format and byte-select agree.

## What that rules out

**It is not slot recycling.** The bank stores the owner generation each row was
written for and compares it against the generation the response returns under —
`rd_gen_mismatch_o` reports **0** across the whole suite. No row was
overwritten by a different owner while a response was in flight. That was my
first hypothesis and the counter refutes it.

**It is not the bank.** The shadow compares the bank against the live tables at
the common stream: 1,176 comparisons, zero mismatches. At that point they agree.

**It is not the queue mechanism.** The nearest queue carries the same kind of
payload through the same dispatcher and is 192/0, with its reader moved.

## What that leaves

The table's value **changes between the common stream and the bilinear queue's
exit**, for the same slot, same sample index, and same generation. The queue
holds the value from the earlier moment — which is what carrying metadata with a
response is *for* — and the table holds a later one.

So the two are not both descriptions of the same instant, and the question
"which is correct for this tap" is **semantic, not plumbing**. Gate 2 answered
it empirically when the reader was moved: three ARGB4444/bilinear fragments
retired the wrong alpha, so the *table's* later value is what the current decode
depends on.

That is a genuine architectural question for packet C, and it is exactly the
kind the brief's credited-reservation design exists to settle — by making the
response, its metadata and its identity travel as one captured record instead of
three things that happen to line up.

## Status

Four readers moved and verified. The fifth is held on measurement, not caution,
with the disagreement localised to two fields and three hypotheses eliminated.
**Nothing is broken:** gate 2 passes 122 checks, gate 3 reports 392
byte-identical retired records.
