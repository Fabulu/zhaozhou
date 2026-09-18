> **PREMISE RETRACTED 2026-09-18, BEFORE ANY FIT RAN.** This file argues from
> "a list of two thousand records, in flip-flops". It is not: `zhao_forge_cliff`
> holds **119,808 block memory bits** in the census. I read the DSP column as
> memory. The fit is still worth running -- that block's only row is a
> `timeout` on a dirty tree and the RAM variant has no row at all -- but the
> reasoning below and its 1,500-3,500 ALM range are unfounded and must be
> rewritten before any receipt is read against them.

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

---

# PREDICTION REWRITTEN, 2026-09-18, with the columns read correctly

The retraction at the top of this file stands: the premise below — *"a list of
two thousand records, in flip-flops"* — was wrong. `zhao_forge_cliff` already
holds **119,808 block memory bits** in the whole-machine census. This is the
replacement, and it is a weaker case honestly stated rather than the same case
re-argued.

## What is actually known about this block

| | value | source |
|---|---:|---|
| ALUT in the census | 8,715 | `@whole-machine-map-probe` hierarchy |
| ≈ALM | 5,525 | at the map's own 0.634 ratio |
| registers | 3,855 | same |
| block memory bits | **119,808** | same — it is NOT memoryless |
| DSP | 0 | same |
| its own leaf fit | **`status: timeout`, `rtlCleanAtHead: false`, no ALM at all** | `zhao_block_fit.json` |
| `zhao_forge_cliff_ram`'s leaf fit | **no row of any kind** | `zhao_block_fit.json` |

**The reason to run this is no longer "the list is in flops."** It is that the
fifth-largest block in the machine has never been measured on its own — its one
attempt timed out on a dirty tree — and a declared alternative has never been
measured at all. Both facts have been sitting in the database.

## The question, restated

**Does `zhao_forge_cliff_ram` fit, and at what ALM, M10K and Fmax?** Nothing
more. There is no baseline to compare against except the census figure, which
is a different arrangement (composed, LFSR-driven, sharing a device with 65
other blocks).

Correctness is not this fit's question:
`tests/forge/forge_cliff_ram_differential.cpp` drives both variants from one
stimulus, and the target's own comment says the handshakes and the compaction's
same-address suppression *"are Verilator's and do not need this gate."*

## Prediction, with much wider bars than before

1. **It fits** — i.e. `status: ok` or `failed:structure`, not `timeout`. A leaf
   with one source file and no `min_fmax_mhz` rule.
2. **ALM 3,000–7,000.** That is deliberately wide. The census figure for the
   golden is 5,525 ALM in a composed context, and a standalone leaf carries
   virtual-pin boundary logic that inflates it. I have no measured baseline for
   this block in this mode, so a narrow range would be invented precision.
3. **M10K 6–20**, against the golden's 119,808 census bits ≈ 6 M10K-equivalents
   of payload. If the RAM variant is a *bitmap* as the manifest calls it, it may
   store far less and use fewer.
4. **No claim about the saving.** Comparing a leaf row to a census row is the
   exact error I made three times today. **The number this fit produces cannot
   be subtracted from 8,715 ALUT**, and it must not be entered in any budget as
   a saving. What it can do is tell us the RAM variant is viable and roughly
   how big, which is the precondition for a real comparison.

## What would make the fit worth more than its number

**If the golden also times out again**, then the largest unmeasured block in the
machine is unmeasurable in isolation, and every figure we have for it comes from
one census run. That is worth knowing and is not currently written down
anywhere.

**If the RAM variant fits in minutes where the golden timed out in hours**, the
adoption case is about measurability as much as area — a block you cannot fit is
a block whose budget line is permanently an estimate.
