# `v3own` is not a memory candidate, and the brief's blueprint does not fit it

2026-09-09. This corrects `GATE2-ALM-DIAGNOSIS-V3OWN-IS-81-PERCENT-20260909.md`
for the third and final time. **The first two corrections made the remedy sound
harder; this one says the remedy does not apply.** Anyone planning from the
earlier sections would have spent real time on a conversion that cannot happen.

## The three positions I held, in order

1. *"Eleven arrays are read combinationally through a dynamic index, so the
   Decrufter pattern applies -- 384 bits convert today."* Wrong.
2. *"A registered address is not enough; the eligibility expression binds four
   arrays in one cycle, so it is a protocol change and the owner must decide."*
   Also wrong -- the pipeline already has a `c3` stage, so the address IS
   available a cycle early and no protocol change is needed for the read timing.
3. **The actual structure makes all of it moot.**

## What the RTL actually does

```systemverilog
always_comb begin
  for (int unsigned i = 0; i < OWNERS; i++) begin
    live_n_c[i] = live_q[i];   req_n_c[i] = req_q[i];   iss_n_c[i] = iss_q[i];
    clm_n_c [i] = clm_q [i];   cmt_n_c[i] = cmt_q[i];   rdy_n_c[i] = rdy_q[i];
    cbi_n_c [i] = cbi_q [i];   crs_n_c[i] = crs_q[i];   fcl_n_c[i] = fcl_q[i];
    fdn_n_c [i] = fdn_q [i];   ...
  end
  // targeted modifications to selected entries follow
end

always_ff @(posedge clk) begin
  for (int unsigned i = 0; i < OWNERS; i++) begin
    live_q[i] <= live_n_c[i];  req_q[i] <= req_n_c[i];  ...
  end
end
```

**Every one of the eleven per-owner arrays is read in full and written in full on
every clock.** That is the next-state register-file idiom: copy the whole array,
modify the entries that changed, write the whole array back.

**An M10K has one or two write ports. This needs sixty-four.** No amount of
read-side restructuring changes that, because the constraint is on the WRITE
side and I never looked at it until now. The dynamic-index combinational READ
that `check_ram_inference.py` flags is real, and it is not the governing fact.

## So the brief's blueprint does not fit this block

Brief 1.5 lists the pattern among the existing blueprints:

> *Memory-backed descriptor/identity transport: the texture Decrufter replaces
> asynchronous indexed fabric payload with synchronous, atomic records.*

That fits a **payload store** -- a descriptor written once when a sample is
planned and read once when its response returns. The metadata bank, the
descriptor bank and the UV join are all that shape, which is why the Decrufter
worked on them.

`v3own`'s per-owner arrays are not a payload store. They are the **live status
of all 64 owners**, any subset of which can change in any cycle: a fence phase
change, a completion, an issue, a retirement. The state is not transported; it
is maintained. Those are different problems and the same remedy does not serve
both.

## What that means for the 2,707 ALM

It stands, and the gate-2 diagnosis stands: `v3own` is 25% of the island and 81%
of the redline overage. What changes is the **shape of any remedy**:

* **Not memory.** Ruled out by the write structure, not by difficulty.
* The cost is the state itself -- roughly 1,472 bits of per-owner status that
  genuinely must be flip-flops because all of it is live -- plus the per-entry
  next-state logic and the wide dynamic-index reads on top.
* Real levers, none of them evaluated here and none of them "memory-first":
  narrow the per-owner state; reduce how many of the eleven arrays exist;
  restrict which entries can change per cycle so the update logic shrinks; or
  reconsider whether 64 simultaneous owners is the right number for the
  workload. Each is a design change with its own contract implications.

## The lesson, since I paid for it three times

I concluded "the remedy applies", then "the remedy is blocked on the owner", then
"the remedy does not apply" -- and each step came from reading one layer further
into the same file. The first two were published with confident framing.

**Every one of those reversals came from looking at a part of the code I had not
read yet**: first the consumer of the read, then the pipeline stage feeding the
address, then the write path. The write path is the one that decided it, and it
was the last place I looked because the tool that started the investigation
(`check_ram_inference.py`) reports on READS.

A tool that answers one question well will shape the whole investigation around
that question. `check_ram_inference` asks "can this be RAM, judging by its
reads", and I let it frame a problem whose answer was on the write side. That is
not a fault in the tool -- its header is honest about what it scans. It is a
fault in taking its output as the shape of the problem rather than as one input
to it.

---

# A FOURTH CORRECTION, and this one wrongly blamed a tool

The triage in the gate-2 report said:

> `cbi_q`, `clm_q` and `fcl_q` have **exactly two references each, both inside
> assertions, and ZERO writes** -- and that block is guarded by `` `ifndef
> SYNTHESIS ``. Quartus defines `SYNTHESIS`, so they are excluded before
> synthesis sees them and cost nothing.
>
> **That is a false-positive class in `check_ram_inference.py`.**

**Both sentences are false.** All three arrays are fully live synthesis state:

```
cbi_q   246 declared   1139 next-state read   1267 reset   1291 write
        1473  c1f_cbi_q <= cbi_q [c0f_slot_q]      <- a SYNTHESIS read
clm_q   238 declared   1136 next-state read   1264 reset   1288 write
        1395  c1t_clm_q <= clm_q [c0t_slot_q]      <- two SYNTHESIS reads
        1436  c1a_clm_q <= clm_q [c0a_slot_q]
fcl_q   261 declared   1141 next-state read   1269 reset   1293 write
        1474  c1f_fcl_q <= fcl_q [c0f_slot_q]      <- a SYNTHESIS read
```

Six to eight references each, written at reset and again every clock, and read
at registered pipeline addresses in ordinary synthesizable code.

## The cause: a whitespace-sensitive grep

I searched for ``cbi_q\[`` -- the name immediately followed by a bracket. This
file writes ``cbi_q [i] <= ...`` and declares ``cbi_q  [OWNERS]``, **with a space
before the bracket**. Every next-state read, every write and every declaration
was invisible to my check. What survived were the assertion lines, which happen
to be written ``cbi_q[gen_chk_s]`` without the space.

So the pattern returned exactly the two references that made the array look
simulation-only, and hid the six that prove it is not. **The instrument reported
the comfortable answer** -- three fewer arrays to account for -- which is the
direction this repository has been burned in more than any other.

## And `check_ram_inference.py` was RIGHT

I recorded a "false-positive class" against that tool on the strength of the
broken grep, and said its fix "needs a preprocessor-aware scan". **It needs
nothing.** It correctly identified all eleven arrays as combinationally read
through a dynamic index. My verification was broken, not its detection.

That accusation is withdrawn. Blaming a working tool because a hand-check
disagreed with it is worse than the original error: the tool is now slightly
less trusted for having been right.

## What changes, and what does not

**The count is eleven, not eight.** All eleven per-owner arrays are live state,
read and written in full every clock, and none is a memory candidate -- which
strengthens rather than weakens this document's conclusion.

`v3own` at 2,706.7 ALM, 25% of the island and 81% of the redline overage, is
unchanged. So is the finding that the memory-first remedy does not apply.
