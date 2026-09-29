> # CORRECTED AGAIN 2026-09-29: THE SIX PROBES ELIMINATED NOTHING
>
> This file says the six styles **eliminated** five candidate causes. **They did
> not, and variant R1 proved it.**
>
> R1 lifted the store's read out of its two enclosing enables and left the
> address alone. It **INFERRED** -- 25,344 -> 48,384 memory bits (+23,040, exactly
> 384 x 60), registers 24,962 -> 1,862. So one of the ENABLES is the blocker, and
> **STYLE=3 tested exactly that shape and inferred**, which this file recorded as
> ruling it out.
>
> **Why STYLE=3 was a false elimination:** its enable was `we_i | ~hazard_c` with
> `we_i` a module PORT, and its address was a port too. At the top of a standalone
> map a port is available at the clock edge, so the synthetic case was easier than
> the real one in ways that had nothing to do with the property under test.
>
> **THE RULE THIS ESTABLISHES, and it is the useful part:** a synthetic probe that
> INFERS eliminates nothing. It can only ever show that the flow works and that
> *some* arrangement of the named property infers. Only a probe that FAILS
> isolates a cause, and only a reduction of the real module can produce one.
>
> So the correct reading of the table below is: **six arrangements infer, and the
> real module does not.** That is a statement about the probe, not about
> production. What still stands unaltered is the SIZE -- 384 x 60 = 23,040 bits,
> now confirmed three independent ways (parameter trace, register delta +7,688 vs
> a predicted 7,680, memory delta +23,040) -- and the fact that the array can be
> an M10K at all.

# The Field exec's uop store: measured, and five hypotheses refuted

Coordinator, 2026-09-29. Device `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2,
`-MapOnly`. Probe: `fpga/rtl/synth/zhao_probe_uopstore.sv` (committed,
lint-clean under `-Wall` at every style).

> ## CORRECTED 2026-09-29 BEFORE ANYONE QUOTED IT: THE STORE IS 384 ENTRIES, NOT 256
>
> This file first said 256 x 60 = 15,360 bits, taken from the module's DEFAULT
> `PLAN = 32`. **The console composes `.PROGS(8)` and `.INSTR_N(48)`**, and
> `zhao_field_host_v2` passes `.CTX(PROGS)` and `.PLAN(INSTR_N)`, so production is
> **CTX = 8, PLAN = 48 -> 384 entries, 384 x 60 = 23,040 bits.**
>
> That is 50% larger than stated, and it raises the store's share of the exec's
> 24,795 own registers from ~62% to **~93%** -- so the store accounts for very
> nearly all of that node's flip-flops.
>
> **The committed probe uses PLAN = 32 and is therefore the wrong depth.** Its
> value as an inference-flow control and as the record of five refutations is
> unaffected; its value as a size or cost measurement was never there and is now
> explicitly none.
>
> The external reviewer asked for exactly this -- "record the production LANES,
> CTX, REGS, PLAN and LONGQ, not just module defaults" -- and it moved the number
> immediately. Reading a module default as a production parameter is its own
> error, and it is the same shape as every other error in this exchange: a figure
> that felt measured because it came from source.

---

## The finding

`zhao_field_v3_exec.sv:302` declares

```systemverilog
uop_t store[0:(CTX*PLAN)-1];   // CTX=8, PLAN=32 -> 256 entries
```

with `uop_t` = `{op[7:0], dst[4:0], a[4:0], b[4:0], c[4:0], imm[31:0]}` = **60
bits**, so the array is **256 × 60 = 15,360 bits**. It is written at one address
(`:1090`), read at one address into a register (`:1200`), and never reset.

**It is in flip-flops.** Two independent measurements:

| | own registers | subtree memory bits |
|---|---:|---:|
| `zhao_field_v3_exec:u_exec` (composed console) | **24,795** | 25,344 |
| `zhao_field_v3_rf:u_rf` (its register file) | 6 | 24,576 |
| `zhao_field_v3_exec` **mapped standalone** | — | **25,344** |

Only **768** memory bits exist in the exec outside its register file, and the
standalone map reports the *same* 25,344 as the composed one. So the store holds
~15,360 bits in flops — **about 62% of that node's own registers** — and **the
blocker is intrinsic to the module, not a consequence of composition.**

**Quartus names the cause itself**, in a line that sat in a 26 MB report the whole
time:

```
Info (276007): RAM logic "...zhao_field_v3_exec:u_exec|store" is uninferred
               due to asynchronous read logic
```

**Our own scanner could not see the array at all** — its declaration recogniser
was `^\s*(?:logic|reg|bit)`, so every typedef-declared array was invisible. Found
by the external R4 review reading the regex. Widened 2026-09-28; exactly **two**
arrays were hidden tree-wide, and this is the larger.

## Five hypotheses, five refutations

| style | what it varied | ALUT | reg | mem bits | memories | verdict |
|---|---|---:|---:|---:|---:|---|
| 0 | shipped `int'()`-cast address | 0 | 0 | 14,848 | 1 | **INFERS** |
| 1 | narrow unsigned `{ctx,pc}` address | 0 | 0 | 14,848 | 1 | INFERS |
| 2 | plain packed array — **positive control** | 0 | 0 | 14,848 | 1 | INFERS |
| 3 | read enable derived from read data | 1 | 0 | 14,848 | 1 | **INFERS** |
| 4 | array written in an **async-reset** process | 59 | 9 | 14,848 | 1 | **INFERS** |
| 5 | split read/write addresses, read address from a **flop array** | 26 | 40 | 14,848 | 1 | **INFERS** |

All six produce an identical Simple Dual Port, 256 deep. **Eliminated:** the
typedef, the signed index expression, an enable derived from read data, an
asynchronously reset process, and an address path through a combinationally-read
flop array.

**Hypothesis 3 failed for a reason worth keeping:** the enable uses the
*already-registered* value, so there was never a combinational loop — and the
same is true of production's `s1_uop_r`. I had read a pipeline register as a wire.

**The probe's first version was also unfaithful:** writing `dst/a/b/c` as
constants let Quartus fold 20 of 60 bits away and report `width=40`. All stored
fields are now data-dependent, and the reported width moved to 58.

## What this means for method

**Building up from a simplification has now failed five times.** Every structural
difference I can name in isolation infers, so the cause is most likely an
*interaction* the probe cannot reach by construction. **Constructing a probe that
infers proves nothing about a module that does not.**

The remaining method is to bisect **down** from the real module. The standalone
map (240 s) makes that affordable, and the measurement above makes it worth doing:
this is the first concrete, tool-confirmed blocker in the V2 exchange, and it sits
inside the Field prepared-context slice both candidates independently chose for
the first replacement experiment.

## What is NOT claimed

* **No ALM saving.** `15,360 ÷ 4` is capacity arithmetic, not an integrated area
  measurement — the error R4 corrected in R3.
* **Not that the repair is free.** Banking the store puts a read-latency stage in
  the issue path, which is a schedule change. State reorganisation is priced
  inside a schedule, not counted from declarations.
* **Not that this rescues anything.** It is one array.

---

# PAIRED STANDALONE MEASUREMENT, 2026-09-29 -- the size is now confirmed twice

R5's external feedback asked for "a paired standalone comparison under identical
settings" before any delta is quoted. Here it is, and it confirms the 384-entry
correction by an independent route.

| row | params | registers | comb ALUTs | est. ALMs | memory bits |
|---|---|---:|---:|---:|---:|
| `zhao_field_v3_exec@storediag` | module defaults (PLAN=32) | 17,274 | 7,553 | 11,946 | 25,344 |
| `zhao_field_v3_exec@prodparams` | **PLAN=48** (production) | **24,962** | 10,287 | 17,100 | **25,344** |

**Register delta: +7,688. Predicted if the store is entirely in flip-flops:
(384 − 256) × 60 = 7,680.** A 0.1% match, and it can only hold if every bit of
the additional 128 entries became a flip-flop.

**Two further confirmations fall out of the same pair:**

* **The composed console is at PLAN=48.** The PLAN=48 standalone reports 24,962
  registers against the composed exec's 24,795 own registers — 0.7% apart. The
  parameter trace said so (`.INSTR_N(48)` → `.PLAN(INSTR_N)` → engine → core →
  exec); this measures it.
* **Memory bits do not move: 25,344 at both parameterisations.** The store
  contributes *zero* memory either way, and `u_rf`'s 24,576 plus 768 accounts for
  all of it. Adding 7,680 bits of array changed the register count and left the
  memory untouched.

So: **the store is 384 × 60 = 23,040 bits in flip-flops — 92% of that module's
24,962 registers.** Quartus's attribution, from the receipt tool:

```
array            : store
VERDICT          : UNINFERRED
quartus says     : asynchronous read logic
device / tool    : 5CSEBA6U23I7 / Quartus Prime Lite 17.0.2
```

## Still not claimed

* **No ALM saving.** `estimatedAlms` is a pre-placement estimate; the 11,946 →
  17,100 difference between the two rows is a *parameterisation* difference, not
  a conversion delta. Nothing here says what banking the store would recover.
* **No schedule cost either.** The production read at `:1200` is already inside a
  clocked process, so a synchronous RAM does **not** inherently need an extra
  architectural cycle — an earlier claim of mine, withdrawn. Whether it needs one
  depends on ports, enables, collision semantics and timing, and that is an
  experiment.
* **The cause remains unknown.** Six probe styles infer; the reduction of the
  real module has not been done.
