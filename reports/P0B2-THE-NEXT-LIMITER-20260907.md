# The island's new limiter, traced end to end

After P0-B the composed island reports 78.80 MHz. Its worst path family is now a
single coherent one — **all forty worst paths** in
`zhao_texture_island_top@p0b-island.setup.rpt` launch and land in the same two
places:

| | count |
|---|---|
| launch `zhao_raster_rcp24_svc:u_rcp|c_pend[]` | **40 of 40** |
| end `zhao_raster_perspuv_svc:u_persp|e_num_v[][]` | 22 |
| end `zhao_raster_perspuv_svc:u_persp|e_num_u[][]` | 18 |

Worst is −2.690 ns. One structural change could move all forty, which is a much
better position than a scattered tail.

## The path, traced through the source

1. `zhao_raster_rcp24_svc.sv:207-219` — the COMPLETION scan. A combinational
   NCTX-way priority loop over every context:

       if (!done_v && c_val[i] && !c_pend[i] && c_ph[i] == PH_MX1
           && !(s1_v_q && s1_i_q == i) && !(m1_v_q && m1_i_q == i))

2. `:261` — `assign r_valid_o = done_v;` The scan's result IS the output valid,
   unregistered.
3. `zhao_texture_island_top.sv:737` — `.v_valid_i(rcp_r_valid)` feeds
   `zhao_raster_perspuv_svc` directly.
4. Inside perspuv that valid gates the writes into `e_num_u`/`e_num_v`.

So a **combinational completion scan in one block reaches another block's
register write enables**, with the block boundary adding wire delay in the
middle.

## This is P0-B's defect, on the other side of the block

P0-B fixed the ISSUE side: a round-robin scan over `c_val`/`c_pend` was reaching
the context RAM's address port in its own cycle, and an S1 registered issue
record broke it. That was worth **+12.03 MHz** in composition.

The COMPLETION side has the identical shape and was untouched: a scan over the
same `c_pend` array reaching a consumer's write enables in its own cycle. §5.2's
event list names both — *"ELIGIBLE → SELECTED/RESERVED → … → DONE_HELD → FREE"*
— and D is described as *"hold a complete tagged answer until the consumer
accepts it"*, which is a REGISTER, not a scan feeding the consumer directly.

## The obvious fix, and the reason it is not obviously free

Register `done_v`/`r_valid_o` at rcp24_svc's output boundary — the same
treatment `zhao_raster_perspuv_svc`'s own output got (96.62 → 105.19 MHz
standalone) and the DONE queue got in §16.3.

**What has to be checked before building it**, because the last two "obvious"
timing fixes each cost something unexpected:

* **Throughput.** Registering completion adds a cycle to a context's residency.
  With NCTX=8 in the island (`#(.NCTX(8))` at island_top:537) rather than 16,
  there is less parallelism to hide it than the standalone block has.
  `raster_rcp24_svc_directed` measures 4.01 clocks per reciprocal and 4.00
  multiplier launches; both must hold.
* **The free-on-acceptance law.** §5.3: *"Completion becomes free only at the
  consumer handshake, not when a DONE ticket or read request is produced."* A
  registered completion must not free the context when the register fills.
* **Area.** The S1 register cost +159 ALM standalone and +14 composed. A
  completion register is wider — it carries `r_o`, `k_o`, `d_zero_o`, `r_tok_o`
  — so the standalone figure will look worse and, on today's evidence, will
  again overstate the composed cost.

## Status

Diagnosis complete and traced to four source lines. **Not built** — the island
reseed is running and it is the measurement that decides whether P0-B's +12 MHz
is real before another change is layered on top of it. Building the next fix
while the previous one is still unconfirmed is how two changes become one
unattributable number.
