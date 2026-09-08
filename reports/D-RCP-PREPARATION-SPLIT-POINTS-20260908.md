# Packet D: where the RCP preparation cone actually splits

Post-fit brief §6. Groundwork from source; **no RTL changed, nothing measured.**

## The cone, read from `zhao_raster_rcp24_svc.sv`

```
d_i                                          :71   input port
  -> 24-position leading-zero search          :118  e_c, a 24-iteration for loop
  -> variable shift   m_c = d_i << e_c        :120  barrel shift by e_c
  -> seed ROM lookup  rom(m_c) -> seed_c      :125-129
  -> zero classification v_zero_c = (d_i==0)  :133
  -> free-context scan / selected write       :142, c_x[...] <=
```

That is the brief's description confirmed line by line: *"a 24-position leading-
zero search, variable shift, ROM-index formation, seed lookup and selected-
context write."*

The V3 fit measures this at **15.416 ns of data delay** — the deepest cone in
the island, 3.9 ns deeper than the palette family, and flattered into apparent
parity by 3.342 ns of favourable clock skew.

## The split, mapped onto the actual source

| stage | takes | from source |
|---|---|---|
| **N0** accept/reserve | `d_i`, token, chosen context index | the free-context scan at `:142`; reserve **here**, not at N2 |
| **N1** normalize | `e_c` (`:118`) and `m_c = d_i << e_c` (`:120`) from N0's registered raw value | split further into LZ-count and shift if one stage is still long |
| **N2** seed/init | `seed_c` from N1's registered `m_c` (`:125-129`); commit `c_x`/`c_m`/`c_w`/`c_k`/zero/token; **only now** make the context pending | |

The zero classification `v_zero_c` is trivially cheap and can ride N0 or N1; it
is not on the long chain.

## The trap this must avoid, and why it is live here

> *"Do not merely add an input flop and leave the entire original long cone from
> that flop to `c_x`: that would move the bad path from a port to a register
> without reducing its combinational depth."*

This is not hypothetical for this design. My own path census found the port
boundary is worth **41 picoseconds** — the worst internal path is −2.093 against
the port path's −2.134. So flopping `d_i` and changing nothing else would move
the path's *origin*, report a different worst-path family, and buy essentially
nothing. It would look like a fix in the report and be one in neither the data
delay nor the Fmax.

**The measurable claim for packet D is a reduction in DATA DELAY on this chain**,
not a change in which endpoint the report names.

## An adjacent path the source already documents

`:172-174` records a separate known family:

```
zhao_raster_rcp24_svc:u_rcp|c_val[5] -> ...|c_m.raddr_a[0]   -3.243 ns
```

— the round-robin priority scan over `c_val`/`c_pend`. That is **worse than
−2.134** in the specimen it was measured on, and it is a different structure
from the preparation cone. It is not part of packet D and it is not addressed by
any of A-E. Recorded so it is not rediscovered as a surprise after the
preparation split lands.

## Ordering constraints the brief attaches

* Reserve at **acceptance**, or the added pipeline can allocate the same
  apparently-free context repeatedly. This is the same
  reservation-versus-acceptance distinction as M6 and v3own §11.1 — its third
  appearance in this subsystem.
* One acceptance event: `external_fire = frag_valid && owner_ready && rcp_ready`
  must stay one event even when `rcp_ready` becomes "preparation reservation
  available".
* Initialization and micro-job writeback can land on the same edge for
  **different** contexts. One writer per row per event must remain explicit, and
  the reserved preparation context must be provably not the one in S1/M1
  writeback.
* The reciprocal arithmetic, ROM, rounding, overflow and truncation are
  unchanged. The existing service stays as the oracle.
