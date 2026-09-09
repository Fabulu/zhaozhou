# `zhao_prod_top` clears its elaboration errors and hits a different wall

2026-09-09. Five defects were repaired in the generated production top this
afternoon, taking Verilator from 12 errors to **0 errors and 0 warnings**. The
map queued behind them was the real gate, because lint-clean is not
`quartus_map` in this repository -- two SystemVerilog forms are on record that
lint with zero diagnostics and fail synthesis.

## The repairs are validated, and the status field cannot show it

| row | status | seconds |
|---|---|---|
| `zhao_prod_top` (before) | `failed:quartus_map.exe` | **59.9** |
| `zhao_prod_top@map-import-fix` | `failed:quartus_map.exe` | **2,629.7** |

**Identical status, 44 times the runtime.** Anyone comparing the two rows on
`status` alone would conclude nothing changed. The whole signal is in `seconds`,
which is the same lesson this repo already records about `failed:structure` -- a
status word summarises an outcome and discards the reason.

The previous run died in a minute on the struct-typed port bug the generator's
own comment describes. This one ran forty-four minutes and was still
**elaborating entities** when I stopped it, with no `Error` lines. The
elaboration faults are gone.

## But the map does not complete on this machine

It was stopped deliberately, and the diagnosis came before the kill rather than
after. Sampled over a 60-second window while it had been running 38 minutes:

```
user cpu    :  0.78 s      <- the actual work
kernel cpu  : 48.09 s
read ops    :  0
write ops   :  0
page faults :  0
working set :  7.35 GB     (11.2 GB committed)
```

**Eighty percent of a core in kernel time, doing no I/O, taking no page faults,
and writing nothing to its log for four minutes.** Not the disk-full case -- 219
GB free. Not out of commit -- 15 GB of headroom. Useful computation had stopped.

Confirmed rather than assumed before killing: the log was byte-identical across a
further 40-second window.

### This is a known wall, and prod_top is the same class of thing

CLAUDE.md records it about the composed shell:

> the composed `zhao_shell_top` does not fit this machine. Measured 2026-08-18
> on Quartus 17.0.2 Lite, `quartus_map` committed 28.4 GB against 24 GB of RAM
> and thrashed at near-zero CPU until it was killed.

`zhao_prod_top` instantiates **74 blocks**, each with its own subtree. It is not
the console -- its own header says it is a RESOURCE top, wired to nothing -- but
it is the same size problem, and it now behaves the same way.

## The consequence: this top may not be worth mapping at all

The question it exists to answer is stated in its header: *"what does the planned
console cost when counted ONCE?"*

**`tools/budget/dsp_census.py` answers that question without a 74-instance map**,
by summing the per-block rows the ledger already holds -- with the two-sided
bound attached, the unmeasured blocks named, and the map-only rows flagged for
contributing DSP but no ALM. It is seconds rather than hours, it degrades
gracefully as blocks are measured, and it cannot hit a capacity wall.

What the top gives that the census cannot is Quartus's own view of the whole set
at once -- cross-block optimisation, and a single authoritative number. On a
machine where it will not map, that is a number nobody can have.

**Recommendation, not a decision:** stop treating `zhao_prod_top`'s map as a
gate. Keep the generator and its checks -- they caught five real defects today,
and `check_prod_manifest.py` needs the top to exist and be fresh -- but read the
budget from the census. If the top is ever wanted as a fit, it needs a machine
with more memory or a split into two or three partial tops, and that is a
decision about what the measurement is worth.

## What is still unproven

**Synthesizability.** Elaboration proceeding for 44 minutes without an error is
strong evidence the five repairs are correct, and it is not the same as
`quartus_map` completing. The forms that lint clean and fail synthesis fail
*during* synthesis, and this run never reached it. The claim "zhao_prod_top is
synthesizable" remains unmade.
