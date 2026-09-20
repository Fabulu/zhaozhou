# Q008 hostregwin-noescape
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

`zhao_host_regwin` is a NEW block: a register aperture on the Cyclone V HPS's
LIGHTWEIGHT bridge, so the ARM can read the console's measurement organs. It is
the only path by which the host reads `zhao_measure_histogram`'s bins (tenant 0,
`zhao_host_reg_hist`) and `zhao_debug_trace`'s ring (tenant 1,
`zhao_host_reg_trace`). It answers reads and refuses writes.

Its author's claim, which is what you are checking, is that isolation between
tenants is STRUCTURAL rather than a bounds check: the tenant is `addr[15:12]`,
the word offset is `addr[11:2]`, and the offset wire handed to a tenant is only
ten bits wide -- so there is allegedly no wire on which one tenant could be
given another tenant's address. The author also claims that every refusal is
ANSWERED and COUNTED and that the aperture never hangs: misaligned address,
unmapped region, the tenant's own refusal, and a tenant that does not answer
within 255 cycles.

You are shown the aperture and both tenants, complete.

## Questions

1. Can a read ever be answered with data that belongs to a DIFFERENT tenant, or
   to a different word of the same tenant? Look specifically at what happens
   when a new request is offered while a previous response is still in flight,
   and at whether the captured tenant/offset and the captured data are loaded by
   the SAME register enable. (A checker whose two operands move together cannot
   fire; the same shape defeats a response mux.)
2. Can the aperture HANG? Name every state in which `h_ready_o` is low and say
   what lowers it and what raises it again. Is there any input sequence -- an
   abandoned request, a tenant that acknowledges and then goes quiet, a timeout
   that expires in the same cycle the tenant finally answers -- after which no
   further request can ever be accepted?
3. Is a WRITE ever able to change tenant state? The `armed` bit is documented as
   readable here and writable only through the command stream. Check that a
   write is refused BEFORE anything is presented to a tenant, not after.
4. Is the timeout counter's terminal value reachable and does it reset on every
   path out of the wait state? A counter that saturates without clearing would
   make the SECOND slow read time out immediately.
5. Are the counters (`hostreg_reads_o`, `hostreg_writes_o`, the four refusals,
   `hostreg_stall_cycles_o`) each incremented exactly once per event? Name any
   that can double-count or miss, and say on what stimulus.

Answer only about what is shown. If a question depends on how the block is
wired into the console, say so and say what you would need to see; do not guess.

## Inputs

fpga/rtl/debug/zhao_host_regwin.sv
fpga/rtl/debug/zhao_host_reg_hist.sv
fpga/rtl/debug/zhao_host_reg_trace.sv
