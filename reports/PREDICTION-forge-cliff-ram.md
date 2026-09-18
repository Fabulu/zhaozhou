# The next leaf fit after `@packet-h-texorder`: `zhao_forge_cliff_ram`

Named in advance, with its question stated, per the batching law.

## How this was found, because the route matters

Three separate things pointed at it and none of them knew about the others:

1. **`uncashed_cheques.py` check 1** has been reporting
   `zhao_forge_cliff_ram  PENDING  fit target, never measured` — manifest note
   *"the FORGE.CLIFF bitmap-RAM CANDIDATE beside the golden"*.
2. **The memoryless census** puts `zhao_forge_cliff` at the top of the list:
   **8,715 ALUT ≈ 5,525 ALM, 3,855 registers, ZERO M9K** — the largest single
   block in the machine that touches no memory.
3. **The block's own contract** says what those flops hold: rim-edge
   enumeration with a worst case of **2,048 rim edges** and a per-page budget of
   512. A list of two thousand records, in flip-flops, beside 430 idle M10K.

And a fourth fact that makes the measurement overdue rather than merely
available: **`zhao_forge_cliff`'s only row in `zhao_block_fit.json` is
`status: timeout`, `rtlCleanAtHead: false`, with no ALM recorded at all.** The
largest memoryless block in the design has never had a clean leaf fit, and its
RAM alternative has never had any fit whatsoever. The 8,715 ALUT above is from
the whole-machine census, which is the only place it has ever been measured.

## The question this fit answers, exactly

**What does the bitmap-RAM FORGE.CLIFF cost in ALM and M10K, against 8,715 ALUT
(≈5,525 ALM) and 0 M10K for the flop-based golden?**

Correctness is not this fit's question and does not need it:
`tests/forge/forge_cliff_ram_differential.cpp` already drives both against one
stimulus, and the target's own comment says the handshakes and the compaction's
same-address suppression *"are Verilator's and do not need this gate"*.

## Prediction

1. **ALM 1,500–3,500**, i.e. a saving of **2,000–4,000 ALM** against the census
   figure. A 2,048-entry list moved into M10K should leave enumeration logic,
   the two degrade passes and the handshakes behind.
2. **M10K 2–8.** 2,048 entries of a rim edge — two vertex indices plus a span —
   is plausibly 40–64 bits, so 80–130 Kbit, and an M10K is 20 Kbit.
3. **Fmax is not the question** and there is no `min_fmax_mhz` on this target.
   Recorded only as context.
4. **The `zhao_forge_cliff` golden may not fit at all**, since its only attempt
   timed out. If the RAM variant fits cleanly and the golden does not, that is
   itself the result — an unmeasurable block replaced by a measurable one.

## What would falsify the reasoning rather than the numbers

**If the RAM variant is not materially smaller**, then the 8,715 ALUT is not the
list at all — it is the enumeration and the two degrade passes, which no memory
removes — and the whole memoryless ranking needs re-reading as "blocks with
large combinational cores" rather than "blocks holding data in flops". That
would be worth far more than the saving, because it would redirect the next
several candidates.

**If it is dramatically smaller — say under 1,000 ALM — check the differential
before celebrating.** A bitmap that drops the budget or the degrade order would
be small and wrong, and this fit measures area, not law.
