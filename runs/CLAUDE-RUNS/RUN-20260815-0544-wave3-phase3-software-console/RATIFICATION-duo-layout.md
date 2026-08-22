# RATIFICATION — Duo framebuffer layout (review MAJOR-3)

*Orchestrator decision, 2026-08-15. Resolves the contradiction the Fable review found between `spec/video_rules.md` §1 and §3.1, and between the W3.5 renderer and the W2.2 VIDEO RTL. Must be applied BEFORE merging `wp/w2.2-video`.*

## The conflict, precisely

| Party | Byte layout of a Duo frame |
|---|---|
| **W3.5 renderer** (`reference/src/zrender/internal.hpp:31-40`, `resolve.cpp:66-74`) | ONE 512×240 row-major image; views side-by-side, so each row is [view0 256 px][view1 256 px] — the two views are **interleaved every 256 pixels**. Displayed CRC taken over rows 0..191 of the 512-wide canvas. |
| **W2.2 VIDEO RTL** (`wp/w2.2-video fpga/rtl/video/zhao_scanout_fetch.sv:29-31`) and **spec §3.1** | TWO packed blocks: view 0 = slot bytes `[0, 0x18000)`, view 1 = `[0x18000, 0x30000)`. Each view is **contiguous**. Border rows are not stored. |

Same pixels on screen; completely different bytes in memory. A `.zcap` FRAMEBUFFER_EXPECTED CRC over slot bytes cannot match both.

The apparent §1-vs-§3.1 spec contradiction is a **wording** defect, not a design one: §1's table lists 245,760 B (0x3C000) for Duo, but line 94 already says buffers are "sized for the LARGEST canvas … so a mode switch never" reallocates. 0x3C000 is the **slot allocation** (driven by Z60/Storm full-raster modes); 0x30000 is Duo's **occupancy**. The table mislabels an allocation as a canvas size.

## Decision: the packed two-block layout (§3.1) is law

`view0 = [0, 0x18000)`, `view1 = [0x18000, 0x30000)`, 256×192×2 B each, contiguous. Rationale:

1. **RTL is the harder constraint.** The scanout fetcher reads linear bursts through MEM.GUARD → arbiter with strict priority. Contiguous per-view blocks give one clean burst stream per view; the interleaved layout forces the fetcher to alternate between two memory regions *every 256 pixels mid-line*, which fights the arbiter's isochronous single-burst discipline (W2.5's merger note: scanout must be driven as 16-B single-burst isochronous fetches for the B=40 liveness bound to hold).
2. **Memory.** 0x30000 vs 0x3C000 saves 48 KiB per slot (96 KiB across the double buffer) and stores no constant-black border.
3. **Dual-view rendering writes views independently** (charter §10 — mesh fetched/decoded/skinned once, projected per camera). Contiguous per-view blocks match how both the software renderer and the future tile engine naturally emit a view.
4. §3.1 is the more specific, more recently authored statement, and it is what the RTL already implements and tests against.

## Consequent fixes (assign to the next repo agent)

1. **`spec/video_rules.md` §1 mode table**: relabel the Duo row — slot allocation `0x3C000` (sized for the largest canvas, mode-switch stable) vs Duo occupancy `0x30000`. Add one clarifying sentence so the two sections can never be read as contradictory again. Z60/Storm rows are unaffected (they genuinely fill their canvas).
2. **`reference/src/zrender/` Duo path**: rewrite to the packed two-block layout — each view rendered into its own contiguous region; no interleaved row addressing.
3. **`displayed_crc32c`**: §3.1 says explicitly "The 48 border rows are part of the displayed stream and therefore part of the displayed-frame CRC." The renderer currently CRCs rows 0..191 of the 512-wide canvas — wrong on both counts. The displayed CRC must cover the full 512×240 stream **as scanned out**: black (`16'h0000`) for rows 0..23 and 216..239, and for each of rows 24..215 the concatenation view0-row ‖ view1-row assembled *at scanout*, not in storage.
4. **Regenerate the render goldens** (`tests/render/render_golden.cpp`) — canvas and displayed CRCs both change. Same commit as the fix, reason in the message.
5. **Add a layout regression test**: assert view1's first pixel lives at slot byte offset `0x18000` (not at row 0 column 256), so the interleaved reading can never silently return.

## Note for W2.2 merge

With this ratified, `wp/w2.2-video` needs no change — it already implements the winning layout. The renderer moves to meet it. Verify at merge that `zref_video.cpp:56-58,319` and the renderer agree on both the block offsets and the border-row CRC treatment.
