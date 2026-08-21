# DEBUG.FRAMEBLIT Integration Corrections

> **Agent: please read this file completely before integrating
> `DEBUG.FRAMEBLIT` into `zhao_shell_top`.**
>
> Reviewed against `main` at:
> `55cf8c9213ec9ff749713e6c4748a15f3e294edd`
>
> This is a correction and integration proposal, not a claim that the existing
> unit-tested block is useless. The central design is sound: stream into an
> invisible leased framebuffer slot, calculate the CRC in one pass, and publish
> only after the transaction succeeds. The implementation has, however, not yet
> closed several shell-level safety seams that its contract claims to close.

---

## Executive summary

`DEBUG.FRAMEBLIT` correctly removes the need for the old 1,966,080-bit
whole-canvas staging buffer. That is the right architectural direction.

Before it replaces the old blitter in `CMD.DMA`, the following issues need to be
resolved:

1. **Slot-1 writes currently use the wrong address.**
   `guard_req_o.addr` is slot-relative, while `MEM.GUARD` expects an absolute
   framebuffer address.

2. **The implementation does not wait for physical SDRAM retirement.**
   Its `retired` counter currently means “the chunk was handed downstream,” not
   “the SDRAM controller retired the writes.”

3. **Failure release and successful publication carry no lease identity.**
   A bare pulse is not enough to prove which slot and generation are being
   released or published.

4. **Abort currently releases immediately, even when writes may still be in
   flight.**
   That permits a slot to be reused while older writes can still arrive.

5. **A lease can lapse after the last explicit check and before the publication
   pulse.**
   Publication must check the live lease at the publication edge.

6. **Lease loss does not immediately stop external side effects.**
   The current block can finish reading and proceed toward guarded writes even
   after `lease_held` has been cleared.

7. **The HPS bridge request is not acknowledged.**
   The block asserts a request for one state and advances without observing the
   bridge’s real `req_grant`.

8. **The shell still instantiates the legacy blitter inside `CMD.DMA`.**
   The new block is unit-verified but is not yet part of the running machine, and
   the old full-canvas buffer still remains in the composed design.

The integration wave should correct these points, remove all blit machinery from
`CMD.DMA`, and immediately rerun composed Quartus synthesis.

---

# 1. Slot-1 addressing is currently wrong

The current RTL emits:

```systemverilog
guard_req_o.addr = zhao_pkg::ZHAO_VRAM_ADDR_BITS'(off);
