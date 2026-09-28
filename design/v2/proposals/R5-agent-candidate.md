# R5 — short, because the argument is settled and the measuring has started

**Revision:** R5, 28 September 2026. Answers `R4_FIELD_EXPERIMENT_CANDIDATE.md`
and `R3_EXTERNAL_REVIEW.md`. **This is the last document from me that is not
attached to a measurement**, as agreed.
**Baseline:** branch `design/zhaozhou-v2-rfc`, source head `814687ae`.

---

## 1. Every R4 correction is accepted, verified in source

| R4 correction | verdict |
|---|---|
| The 51 MHz "floor" inverts the inequality | **accepted** — a cycle CEILING plus a deadline gives a relationship, not a minimum clock; and 51 MHz would spend 100% of the frame anyway |
| `ALREADY INFERRING` cannot identify an array | **accepted** — `check_ram_inference.py:901` is `"ALREADY INFERRING" if mem >= bits`, module-subtree memory against this array's size |
| The declaration recogniser misses typedef'd arrays | **accepted** — `:174` is `^\s*(?:logic\|reg\|bit)`, and there is a real 15,360-bit instance (§3) |
| "One array per M10K" is too broad | **accepted** — Cyclone V packed mode exists; my original bits÷10,240 error stands for different reasons (width, ports, fragmentation) |
| ~15,750 ALM is capacity arithmetic | **accepted** — it is `bits ÷ 4`; the older density table shares the flaw |
| Other ranked rows share the cliff row's problem | **accepted** |

**I therefore quote no percentage for remaining storage.** Not 31%, not 4–7%, not
a better number. R4's wording is adopted verbatim: *the known large in-place
conversions are already incorporated; the remaining candidates appear less
attractive; their net saving is not established by this scan; broader state
reorganisation is a separate, unmeasured question.*

## 2. The clock: my number was wrong and the contract's is higher

`design/contracts/FIELD.SEQ.EARTH.md:48-53`, quoted by neither of us before:

> *"Designed for the shared **100 MHz** GPU domain; a lower private clock is
> accepted only after the COMPLETE Earth slice — not a leaf core — proves the
> deadline below with reserve. By the regenerated cost model, **~80 MHz is the
> lowest credible clock**; the measured v2 leaf **59.22 MHz is not** (108% of its
> own budget before integration)."*

So the ratified position is **100 MHz design point, ~80 MHz lowest credible,
59.22 MHz already rejected by name.** My invalid derivation landed at 51 — below
the value the contract had already refused. Against the measured record: **20% of
74 placed leaf fits reach 100 MHz, 55% reach 80 MHz**, and the only composed
console placement is 18.5 MHz.

R4's test is adopted: **useful work per second, complete latency and reserve
together**, not MHz in isolation.

## 3. The one durable result of four rounds

`zhao_field_v3_exec.sv:302` — `uop_t store[0:255]` = **15,360 bits, in
flip-flops**, ~62% of that node's 24,795 own registers. Quartus states it
directly in the console map:

```
Info (276007): RAM logic "...zhao_field_v3_exec:u_exec|store" is uninferred
               due to asynchronous read logic
```

This is the instrument blind spot R4 found by reading a regex, with a real
instance, inside the slice both candidates independently chose.

**And my explanations for it keep being wrong.** `fpga/rtl/synth/zhao_probe_uopstore.sv`
is committed, lint-clean under `-Wall` at every style, and maps in ~35 s:

| style | structure | result |
|---|---|---|
| 0 | shipped addressing, 32-bit signed `int'()` index | **INFERS** — hypothesis 1 refuted |
| 1 | narrow unsigned `{ctx,pc}` address | INFERS |
| 2 | plain packed-vector array (positive control) | INFERS — flow is live |
| 3 | read enable derived from the read register | **INFERS** — hypothesis 2 refuted |
| 4 | array written inside an **async-reset** process | **INFERS** - hypothesis 3 refuted |

**All five report an identical Simple Dual Port, 256 deep, 14,848 bits.** Every
structural difference I could name between the probe and production still infers.

**So the blocker is in the production context and this probe does not reproduce
it.** That is a result, not a failure: it eliminates the typedef, the signed index
expression, the enable derived from read data, and the asynchronously reset
process. The method now inverts — map the real `zhao_field_v3_exec` standalone and
bisect **down** from production rather than building **up** from a simplification.
**Constructing a probe that infers proves nothing about a module that does not.**

**Hypothesis 2 was wrong for a reason worth recording:** the enable uses the
*already-registered* value, so there was never a combinational loop — and the
same is true of production's `s1_uop_r`. I read a pipeline register as if it were
a wire.

**The probe's own first version was also unfaithful**: writing `dst/a/b/c` as
constants let Quartus fold 20 of 60 bits away and report `width=40`. Fixed. An
unfaithful probe returning a plausible verdict is still a broken instrument, and
this one nearly shipped.

**And when the cause is found, it is priced as a schedule change, not a saving.**
Banking the store means the issue path absorbs a read-latency stage — R4's point
exactly. No ALM figure is claimed; `15,360 ÷ 4` is the arithmetic R4 just
corrected.

## 4. R4's programme, adopted

* **The comparison boundary**, verbatim: prepared request → input generation and
  binding → admission → operand reads → short and long ops →
  result/status/presence capture → command-ordered reduction → publication. A
  shorter sub-experiment **declares the work it omits** and may not claim the
  Earth deadline.
* **The candidate organisation**: immutable programs separated from mutable
  contexts without reducing distinct-program capacity; uniforms carrying their
  own instance identity; banking driven by **real port demand**, with small
  ready/valid bitmaps and active operands staying in registers. **My blanket "no
  arrays in flip-flops" rule does not return** — `zhao_field_v3_rf` is the
  counter-proof: 24,576 memory bits with **six** registers, already correct.
* **Operand bandwidth preserved.** The exec documents a seven-operand supply
  requirement; a smaller register file that quietly serialises those reads is the
  failure mode most likely to make a "smaller" engine worse.
* **Destination ownership until retirement**; result carries context and
  generation, destination actually written.
* **One table, not a verdict paragraph.** Adopted as the acceptance format.
* **Envelope work in parallel, not as a gate.** I extract the existing contract
  commitments and bring specific combined profiles with consequences,
  distinguishing an existing promise from a new interpretation — and the bounded
  Field experiment does not wait on it.

## 5. What I actually think, once

Four rounds have produced **one** durable, tool-confirmed fact about the central
mechanism: a specific 15,360-bit array does not become memory, and the reason is
still being bisected after **three** refuted hypotheses. Everything else either
survived as a caution or was withdrawn — my 31%, my 4–7%, my 51 MHz, my
"rate-bound is exhausted", my "400 nodes not five rewrites", R0's allocation,
R2's expectation that storage had more to give.

**The sign of the error has never been predictable.** Twice I was too pessimistic,
twice too optimistic. That is the argument for R4's programme and against anyone's
next forecast, mine included. **No probability of success appears in this
document.**

**Not proved, and I will not imply otherwise:** that V2 fits; that it does not;
that storage is spent; that storage has more to give.
