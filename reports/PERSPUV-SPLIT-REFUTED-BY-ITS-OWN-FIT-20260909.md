# The perspuv port-split is refuted by the fit that followed it — and Quartus merged 384 of the registers back

2026-09-09. Follows `REGISTER-BREACH-IS-SYSTEMIC-NOT-V3OWN-20260909.md`, which
found `zhao_raster_perspuv_svc` is the island's largest register consumer at
3,240 (19.9%) and **4.63× its §3.3 register budget** — the worst single breach in
the island.

That report left the governing question open: *is the register overrun a wrong
budget, or genuine over-pipelining?* For this block the answer is neither, and
it was already waiting to be read. No fit was run.

## The registers are a token table, not pipeline depth

Accounting every declared array in `zhao_raster_perspuv_svc.sv` at `NTOK=16`,
`TAGW=16`:

| | bits |
|---|---:|
| `_q` pipeline state (p0–p3, both axes, plus control) | **594** |
| the 16-entry token table (`e_num_u/v`, `e_mant_u/v`, `e_q_u/v`, `e_k`, `e_tag`, `e_val`, `e_have`, `e_sat`, `e_dz`, `wq`) | **3,376** |
| declared total | 3,970 |
| measured Dedicated Logic Registers | 3,240 |

**The token table is 85% of it.** The pipeline itself — four stages across two
axes, carrying 64-bit products — is 594 bits, comfortably *inside* the 700-bit
budget line for the "perspective pair pipeline".

So this block is **not over-pipelined**. It holds a 16-entry transaction table
that §3.3's budget line never mentioned — the same shape of finding as
`v3own`'s, in a different block, and further evidence that the budget's model
omitted per-token state generally rather than in one place.

## The split was already tried, and this fit is its verdict

The RTL carries a long comment explaining that `e_num` and `e_mant` were **split
per axis** so that each array has one read address and one write address, which
is what a simple dual-port memory can do. It closes:

> Diagnosed in `reports/PERSPUV-REGISTER-DIAGNOSIS-20260905.md` as **STATIC
> ANALYSIS, explicitly not a measurement. The fit that follows this change is
> what confirms or refutes it.**

That fit exists. Provenance checked first, because comparing a current file to an
old measurement is the standing trap:

* split landed in `7d71235a`, 2026-09-06
* `@g2-prod` source commit `82a4f317`, 2026-09-09, and the split signals are
  present in that commit

So the comparison is valid, and **it refutes**. The island's Fitter RAM Summary
lists exactly one array from this block:

```
zhao_raster_perspuv_svc:u_persp|altsyncram:e_tag_rtl_0 ; Simple Dual Port ; 16 x 14 ; 224 bits
```

`e_tag` inferred. `e_num_u`, `e_num_v`, `e_mant_u`, `e_mant_v`, `e_q_u`, `e_q_v`
and `e_k` all stayed in flip-flops.

## And the `e_mant` split was actively undone — 384 registers merged

The map report's register-merging section:

```
e_mant_v[0][0]  Merged with  e_mant_u[0][0]
e_mant_v[1][0]  Merged with  e_mant_u[1][0]
...
```

**384 registers**, the entire duplicated mantissa array. `e_num_v` and `e_q_v`:
**0 merged** — they come from different sources, so they legitimately stayed
split. 552 registers were merged in this block in total.

The comment's own justification is what caused it:

> Two copies of one 24-bit value is 384 bits of duplication traded against an
> array that cannot be memory at all. **Both copies are written at `tail_q` from
> the same `r_mant_i` on the same clock, so they cannot disagree.**

That last sentence is exactly the condition under which register merging fires.
Two registers provably always equal are one register, and Quartus collapsed them.

**So the trade did not happen in either direction.** The duplication cost nothing
— the tool reclaimed it — and the split achieved nothing, because the merged
array again carries two unrelated read addresses (`pk_i[0]`, `pk_i[1]`), which is
the 2R structure an M10K cannot provide. The technique is self-defeating whenever
the copies are written identically: the property that makes duplication *safe* is
the property that makes it *removable*.

To make such a split stick, the copies must be distinguishable to the tool — a
different write condition or clock — or merging must be suppressed for them
explicitly. Neither is free, and neither is a change to make speculatively.

## What blocks `e_num` is NOT determined by the evidence available

`e_num_u` kept its split (0 merged), has one write address (`tail_q`) and one read
address (`pk_i[0]`), is 512 bits against `e_tag`'s inferred 224 — and did not
infer. So **read-address count is not sufficient**, which is the load-bearing
assumption of the 2026-09-05 diagnosis.

`tools/quartus/check_ram_inference.py` reports every array in the block as
*"written from an ASYNC-RESET process — WEAK SIGNAL, measured false positives"*.
An async reset that clears all entries is a real Quartus inference blocker, and
it is the obvious candidate — **but `e_tag` carries the identical flag and
inferred anyway, so it cannot be the discriminator.** The tool's own header says
this signal has measured false positives; that self-labelling is what stopped a
third confident wrong answer today.

Three arrays *are* settled: `e_val`, `e_sat` and `e_have` each have **two or more
distinct write addresses**, which the island brief §5.3 forbids by name. Those
can never be memory, regardless of anything else.

**I am not going to guess at the rest.** Two speculations were already corrected
today — a stale Mosaic DSP figure and an overstated seed claim — and the honest
state is that the discriminator between `e_tag` and `e_num_u` is not visible in
the reports on disk.

## The cheap experiment, and a process finding that matters more

`e_k` is already positioned as the control, deliberately: 6×16 = 96 bits, *the
same geometry as `e_tag` which did infer*, differing only in read-address count.
The RTL says so and says it was "left as evidence".

The experiment that settles this is small: change one array's reset or split and
see whether it appears in the RAM Summary.

**And it does not need a fit.** RAM inference is decided at **map** — the RAM
Summary is produced by `quartus_map`, and `run_block_map.ps1` exists and runs in
the tens-of-seconds-to-minutes class rather than 1.5–4 hours. So this entire
question, and the whole register-inference class behind the island's largest
breach, is answerable **without spending the scarce resource**. That reframes it
from "blocked behind a fit" to "cheap, once someone authorises an RTL probe".

What map cannot answer is what the change costs in placed ALMs and Fmax. So the
sequence is: map to learn whether it infers, and only then a fit to learn whether
it was worth it.

## What this does not establish

**Not a remedy.** Nothing here says the token table *can* be memory-backed. It
says the one attempt so far did not work, that part of it was mechanically
reversed, and that the reason is not yet known.

**Not a criticism of the split.** It was correctly labelled static analysis and
correctly invited the fit to refute it. That is the loop working.

**Not a new measurement.** Every number is read from the existing `@g2-prod` map
and fit reports, the ledger, and the RTL.

**Not authorisation.** No RTL was changed and no job was launched. The register
breach remains the owner's decision, now with its largest component understood:
a 16-entry token table the budget did not price, in a pipeline that is otherwise
inside its budget.
