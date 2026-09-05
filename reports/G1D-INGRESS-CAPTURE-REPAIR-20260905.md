# G1-D: the island never captured its fragments

**Date** 2026-09-05
**Block** `zhao_texture_island_top`, `zhao_texture_fragrob`
**Test** `tests/texture/island_composed_directed.cpp`, 24 checks
**Commits** `b3b267d3` (repair), `a4faca2e` (identity gate), `f9bc57d4`
(mutation evidence), `dcc28d18` (enforcement), `b3a8a4f6` (palette)

## What was wrong

The composed island read **every per-fragment attribute straight off its own
input pins at the point each was consumed** — ten separate signals, including
PERSPUV's `u/w` and `v/w` numerators, the material recipe, weight, sample
count, binding, LOD, base colour and sample class.

A fragment spends about twelve clocks in RCP24 and PERSPUV before FRAGROB
accepts it. So each tap sampled whatever fragment happened to be at the
boundary twelve clocks later, and once submission stopped the pins simply held
the last fragment's values.

## The measurement that found it

Recording the **identity** of every retired fragment, using the `out_tag_o`
port that was already exposed. One run:

```
retired 64 tags, 39 missing, 7 duplicated
12 13 14 15 16 16 16 16 20 21 22 23 24 24 24 24 28 29 30 31 32 32 32 32
37 40 40 40 48 48 48 52 56 56 56 59 60 61 62 63 63 63 ... 63   (24 times)
```

64 fragments went in and 64 came out. Only **25 distinct fragments existed**,
39 were lost outright, and the last one was delivered 24 times because the
input pins held their final value after submission stopped.

Three further measurements localised it in one pass each:

* The jobs the combiner counted matched exactly what that **actual retired
  set** predicts — 140 = 140. The combiner was therefore doing correct work on
  the wrong fragments, and was fully exonerated.
* Logging the same tags one stage earlier showed the **identical sequence
  arriving at FRAGROB**, so nothing downstream was responsible.
* Logging the slot each fragment was written into showed FRAGROB's allocation
  and ordered retire were **perfect** — the head slot walked `0..15` exactly
  four times. The reorder buffer was innocent and had been handed wrong data.

## The repair

The token needed to fix it **already existed and was already carried end to
end**: `tok_r` is stamped on admission, RCP24 returns it as `r_tok_o`, PERSPUV
carries it through as `tag_o`. Nothing consulted it.

Attributes are now stored at admission in a 64-entry table and read back at the
two points where the token reappears. The six reserved `ctx` bits at `[21:16]`
carry the token, so a consumer downstream of FRAGROB — which sees only the
retiring context — can index the table too. The sample **class** is keyed by
FRAGROB slot instead, because a TMU request identifies its fragment by slot and
nothing else travels with it; FRAGROB now reports where an accepted fragment
landed.

## What moved, all in one run

| | before | after | what the drive pattern calls for |
|---|---|---|---|
| combine jobs by recipe | `0 0 0 4 0 0 24 112` | `0 32 32 32 0 0 48 32` | `0 32 32 32 0 0 48 32` |
| bilerp / palette | 131 / 61 | **96 / 96** | 96 / 96 |
| aux accepted | 37 | **22** | 22 |
| distinct fragments out | 25 of 64 | **64 of 64** | 64 |

Eight fragments per recipe against `zref_material`'s job table, 32 fragments
per sample class at three samples each, and 22 fragments with `(i % 3) == 0`.
**The old numbers were not explained by anything.** The new ones are each
predicted independently by the test's own drive pattern.

## Three wrong hypotheses, and why that is the finding

All three were written down confidently before the experiment that killed them:

1. *"The recipe field arrives OR-ed together."* Refuted by driving a **fixed**
   recipe, which gave exact counts. A corrupted field cannot produce exact
   counts.
2. *"Fragments are mis-associated with their neighbour's recipe."* Refuted by
   varying how **often** the recipe changes: the total moved to 140, 180 and
   232 around an expected 176, and re-association cannot inflate a total.
3. *"Jobs are being issued more than once."* Refuted by the identity
   measurement, which showed the jobs matched the retired set exactly.

Three rounds of reasoning about **aggregate counts** produced three wrong
answers. One measurement of **per-fragment identity** produced the right one
immediately. The counters said "something is wrong" and structurally could not
say what, because they aggregate away the very field that was broken.

This is `CLAUDE.md`'s own law in a new costume. "Counters see what pictures
cannot" is true, and its converse is also true: **counters cannot see identity,
and identity is what a transport bug destroys.** When a count is wrong, measure
the identity of the things being counted before theorising about the count.

## Two defects this exposed

Both were **masked** by the carriage bug and became visible only once it was
fixed. Neither was tuned away; the first is now fixed and the second is
architectural and recorded.

### 1. The CLUT path returned black — now fixed, and it was two faults

All 32 CLUT-class fragments retired with `rgb == 0`; all 32 bilinear ones did
not. `cnt_palette_lookups_o` moved healthily the whole time.

**Fault one: the test never loaded a palette.** It sent `LD_WRITE` 64 times and
no `LD_BEGIN`, so `loading_r` was never set and not one write was accepted;
`LD_END` grants residency only when all 256 entries have arrived, so 64 would
not have sufficed either.

**Fault two: the island asked the palette the wrong question.**

```systemverilog
.lu_slot_i(disp_clut_tok[$clog2(PAL_SLOTS)-1:0])   // tok[1:0]
.lu_gen_i (disp_clut_tok[GENW-1:0])                // tok[7:0]
```

Two **overlapping** slices of the same word, so the "slot" was the low two bits
of the "generation" — and that generation was FRAGROB's residency counter,
which has nothing to do with a palette upload. This is the owner's v2 Appendix
B line exactly: *"A provenance source ID is not an internal transaction-routing
ID."* A palette slot and generation are a **material binding**, so they now
travel with the fragment through the ingress capture, keyed by FRAGROB slot the
way the sample class is. The routing token's fields are named in localparams
instead of being sliced by hand twice, which is what let two overlapping ranges
sit unnoticed.

```
before   96 lookups, 96 STALE, 0 cold    all 32 CLUT fragments black
after    96 lookups,  0 stale, 0 cold    all 32 coloured
```

**A correction belongs here.** The first diagnosis of the blackness asserted
*"it is NOT the palette being unloaded"*, reasoning that the entry values are
all non-zero. That checked the data and not the protocol; the values were
irrelevant because no write was ever accepted. The negative claim was published
without the evidence a positive one would have required — the third time in
this pass that a confident statement preceded the cheap experiment that would
have settled it.

Staleness and coldness are now **counters** rather than internal wires, and are
asserted at zero. "The lookup happened" and "the lookup found its palette" are
different facts, and only the first was observable — which is exactly why a
path answering every request with a miss indication looked healthy for a whole
pass.

### 2. Order is not preserved

```
... 48 49 50 51 56 60 57 61 62 63 58 59 52 53 54 55
10 fragments out of place, maximum displacement 8
```

FRAGROB is not the fault. It retires strictly in **allocation** order, so the
retire order *is* the order fragments reached it, and the permutation is
already present at its input. The variable-latency services ahead of it
complete out of order, and FRAGROB's ordered retire is measured against its own
arrivals rather than against ingress. Steady-state backpressure hides this by
keeping the chain in lockstep, so the disorder appears only while the pipeline
**drains** — which is why the tail permutes and why no earlier test saw it.

That is the **misplaced ordering boundary** the owner's recovery architecture
v2 names in priority 6: allocate the fragment record *before* the reciprocal
work and retire only after material combination. Repairing it is that
rearchitecture, not a patch. The committed check is a regression guard on the
measured bound, so the defect cannot quietly worsen while it waits.

## Evidence quality

The owner's v2 brief lists **evidence quality** as one of four acceptance
questions: *"Do tests detect wrong work?"* That is answered by mutation, not by
assertion.

```
assign fr_f_ctx = fr_f_ctx_in       (the exact historical bug)
    38 missing, 38 duplicated, 63 out of order
    jobs by recipe 0 0 4 4 0 0 30 112

fc_rp = fc_wp                        (read live ingress at read point 2)
    63 missing, 11 duplicated, 52 foreign
    jobs by recipe all zero
```

**Under the first mutation the old aggregate checks still pass.** `moved >= 2`
sees four counters moved and is satisfied; the old single-fragment colour check
samples a fragment that happens to be non-zero. The identity gate fails
immediately. The checks that were in the tree while the bug was live would have
gone on not catching it.

`tools/rtl/check_ingress_capture.py` now enforces the *rule* rather than the
instances, and is wired into `npm run design:report`. Its first version failed
its own mutation test — it caught a direct pin tap but passed the capture word
laundered through one extra wire, which is precisely the historical bug. A gate
that passes the defect it was built for is worse than no gate; it now tracks
that alias, and refuses to run if the alias no longer exists.

## Two corrections the owner's brief forced on the repair itself

The recovery architecture v2 arrived while this was being fixed, and its §2.2
and §2.3 diagnose the same defect independently from source alone. Two of its
rulings applied to **my repair**, not to the original code.

**It packed fields into the caller's context.** The first version of the capture
wrote recipe, weight, sample count and token into bits `[34:16]` of the caller's
own context word. §2.3 rules that out directly — *"Packing recipe bits into that
word is not a valid way to retain an independently opaque context and world
X/Z … Do not silently overwrite caller-owned bits."* Fixing a late-capture bug
by destroying caller data is not a fix. The token is now a **typed field** on
FRAGROB (`f_tok_i` / `o_tok_o`) carried beside the context, and the context is
stored and forwarded verbatim.

**The packed word is gone entirely.** Every carried attribute is now its own
named array. The packed form costs nothing to write and cost one whole defect
to find — the palette's slot and generation were two hand-written slices of the
same token that **overlapped**. Named fields cannot overlap.

That refactor is behaviour-preserving: every measured number is identical
before and after, which is why it is a separate commit.

**An honest limit, recorded in the source.** This island consumes only the low
16 bits of the context, as the tag behind `out_tag_o`. The rest is carried
through FRAGROB intact, but nothing reads it and no port exposes it, so **no
test can observe that it survives**. What is established is the weaker and still
necessary property that the island no longer overwrites it. Surfacing it would
cost 64 output pins on a block where 405 of 2,000 summarised paths already start
at virtual pins, so it waits for a consumer that needs it.

## Invalid-input poison

Priority 3 also asks for a poison test, and it is the behavioural counterpart to
the static gate: the gate proves nobody **wrote** a late read, the poison proves
nobody **performs** one — including reads the gate's contract does not know to
look for.

Every ingress port is driven with legal-to-present, wrong-to-use values on every
cycle `frag_valid_i` is low, with a deliberate three-cycle gap after every
fourth fragment so the poison lands **mid-stream** while earlier fragments are
still in flight, not only during the drain. It alternates rather than holding a
constant, because a constant makes the recipe constant too and a
wrong-but-constant recipe still yields a tidy-looking histogram — which is how
the original defect survived being looked at.

With poison active every measurement is unchanged and 25 checks pass.

**And its mutation result is worth reading carefully.** Reintroducing two late
reads gives:

```
CLUT colour 20 of 32, palette lookups 38 of 96   -> 2 checks FAIL
combine jobs by recipe 0 32 32 32 0 0 48 32      -> unchanged, CORRECT
```

The job histogram is **completely blind** to that mutation, because the combiner
reads its recipe from the capture table rather than from the mutated path. An
aggregate that looks right is not evidence that the thing it aggregates is
right — the same lesson as the original bug, arriving from the other direction.

## What this invalidates

**The 7,720 ALM / 69.05 MHz island measurement is stale.** The repair adds a
64-entry attribute table with combinational reads on PERSPUV's input path
(MLAB/LUT-RAM, not M10K, because that handshake is combinational off RCP24's
output). No prediction of the new area or frequency is offered: the island's
last obvious explanation was worth 4 MHz of 36, and the honest lesson from that
is not to guess the size of an effect before measuring it.

Per the owner's priority 5 — *"Do not launch an expensive complete-island refit
after each speculative glue edit"* — the island re-fit is deliberately batched,
and the corrected combiner leaf fit is the next bounded attribution experiment.

## What is NOT claimed

* Not that the island is correct. The ordering defect above is real and
  architectural, and no claim is made about anything this test does not drive.
* Not that the island is timing-closed or that its resources are known. Every
  number in the fit tables predates this change.
* Not that which upstream service reorders has been identified. The maximum
  displacement equalling RCP24's eight contexts is suggestive and was not
  measured.
* Not a claim about the composed console. This is one island.
