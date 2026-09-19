# Q007 arbiter-pending-fix
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

An earlier review found a HIGH bug in `zhao_hps_arbiter_n`: the arbiter sampled client requests only in
A_IDLE, so a client that PULSES its request for one cycle (CMD.DMA) lost the request if another client owned
the bridge at that moment, and then waited forever. Commit 525a3f6d adds "rule 6b": a per-client PENDING slot
that captures a request while the client is not the owner and serves it on a later idle window. The same
commit fixes two clients (MEM.UPLOAD and DEBUG.FRAMEBLIT) that ignored a bridge refusal `err` while holding
their request. Bridge facts: one burst in flight; it accepts when idle, raises busy on the same edge and grant
the NEXT cycle; a request while busy is a protocol violation; a malformed request is answered `err|last`
with NO grant.

## Questions

1. Is the pending capture correct for BOTH client styles: a HOLDER (keeps valid until granted) and a PULSER
   (one cycle)? Can a HOLDER now be served TWICE (once from pending, once from its still-held live request)?
2. Priority and starvation: does serving pending slots preserve "lower index wins", and are the wait counters
   still correct (neither double-counting nor freezing)?
3. Can a pending slot hold a STALE request: the client pulsed, then withdrew or changed its request before
   service?
4. The `err` fixes in the two clients: does each now leave its request state cleanly on a refusal with no
   grant, and can either still spin?
5. Do the new directed cases fail on the old RTL for the right reason?

## Inputs

fpga/rtl/memory/zhao_hps_arbiter.sv
show:525a3f6d:fpga/rtl/memory/zhao_hps_arbiter.sv
show:525a3f6d:fpga/rtl/mem/zhao_mem_upload.sv
show:525a3f6d:tests/memory/hps_arbiter_n_directed.cpp