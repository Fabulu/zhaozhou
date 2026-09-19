# Q004 post-fbread-lease
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

POST.COMPOSITE runs after the raster has drained and before the frame is published, inside the
render engine's framebuffer LEASE. It uses the render engine's own memory client, ENGINE0, and
the memory guard admits ENGINE0 only while `fb_writer == 1` (the render holds the lease). Three
requesters share ENGINE0 inside `zhao_post_lease`: the raster's writer, the post source reader
`zhao_post_fbread`, and the post write-back/echo. `zhao_post_fbread` reads the finished back
buffer in raster order.

## Questions

1. Can `zhao_post_lease` grant ENGINE0 to post's reader or writer while the raster still has writes
   in flight (i.e. before the raster has DRAINED), so post reads pixels that are not final?
2. Can a response beat for one requester be delivered to another (a mis-steer when two requests
   overlap, or a request issued while a response is still streaming)?
3. Does `zhao_post_fbread` read EXACTLY the frame: every pixel once, in raster order, correct at
   the row stride and at the last row and column, for both 1-view and 2-view (Duo) geometry? Check
   the address arithmetic for widths and overflow.
4. Backpressure: if post's consumer stalls mid-row, is any beat lost or duplicated?
5. Any counter or guard that cannot fire (both operands behind one enable, or a condition that cannot arise)?

## Inputs

fpga/rtl/compositor/zhao_post_fbread.sv
fpga/rtl/compositor/zhao_post_lease.sv