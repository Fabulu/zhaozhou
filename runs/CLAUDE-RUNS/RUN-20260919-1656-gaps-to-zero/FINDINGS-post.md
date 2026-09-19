# FINDINGS — post lane (gz/post 71252004; merged as 40ebd7d8)

Transcribed by the coordinator. Register 49 -> 46.

## Closed
* I15: `zhao_post_fbread` reads the finished back buffer in raster order (41 checks), on ENGINE0 under the render lease;
  three requesters share it inside `zhao_post_lease`. No new client id.
* `zhao_mem_guard`: an ENGINE0 fb-read arm and an echo-capture write arm; no-escape re-proved (a throwaway mutant that
  drops the capture lease check fails it).
* I16: the compositor output is written back in place through the same `zhao_raster_fbwrite`, switched at the phase
  change (no second producer); the echo tap feeds POST.ECHO.
* POST.ECHO BUILT (R7): contract, then `zref::post::echo`, then `zhao_post_echo` (33 checks against zref).
* Traverse: in the smoke, the post-pass framebuffer and the capture both equal the raster's frame word for word over 92,160 px.

## Defects fixed along the way
`zhao_raster_fbwrite` treated a late verdict as a refusal and marked the frame fatal; ENGINE0 writes could reach the
arbiter before their data was queued; `zhao_mem_share_n` could not carry a write; the smoke used a 128-byte stride
where the Z60 rule says 768.

## Instrument defects
The first "identical" check compared ZEROS, because the raster draws black while the material is unbound. The packet's own
guard caught it, and the frame is now pre-filled with a non-zero pattern. The bench's guard model gave its verdict a cycle
late (fixed before any number was quoted).

## False claims
I15 "post needs a new memory client" (ENGINE0 sharing suffices). POST.ECHO's "deliberately unwritten" contract sections
are now written.

## Refused: I17 (a) look values, (b) grading table, (c) the gather/HUD tag rule. Rulings R34–R38.
Post throughput is 730,312 gpu cycles per pass against a budget of about 103,680 items (R38).
Cost estimate (no fit): about 740 ALM, 0–1 DSP, 3 M10K.