# Packet H driver contract: where every new input gets its value

*2026-09-17. Computed, not remembered. This is the work list for
`fpga/rtl/common/zhao_shell_top_v2.sv`, which does not exist, and it exists
because the roadmap's own instruction is that the file "should be started with
the protocol in front of you, not bolted onto the end of a timing campaign".*

The sibling shell swaps two blocks and adds five organs. The swapped blocks
grow enormously:

```
zhao_geom_bin_pipe  ->  zhao_geom_bin_pipe_v2     63 ports -> 166
zhao_video_slotmgr  ->  zhao_video_slotmgr_v2     22 ports -> 74
```

Ports are not the work, though. **Inputs are the work**, because every one of
them is a wire somebody has to decide the source of, and an input nobody
decided about is a `PINMISSING` at the next fit or — worse — a tie-off that
looks deliberate. There are **59 new inputs**. Twelve already have a producer.
The other forty-seven are this packet.

---

## 1. Already solved: driven by an organ (12)

| block | input | driver |
|---|---|---|
| `geom_bin_pipe_v2` | `frame_fault_clear_valid_i` | `zhao_renderer_lease_v2.frame_fault_clear_valid_o` |
| `video_slotmgr_v2` | `render_req_{valid,slot,mode}_i` | `zhao_renderer_lease_v2.render_req_*_o` |
| `video_slotmgr_v2` | `blit_req_valid_i` | `zhao_video_blit_lease_v2.mgr_req_valid_o` |
| `video_slotmgr_v2` | `rsp_ready_i` | both leases, demultiplexed by `rsp_writer` |
| `video_slotmgr_v2` | `term_{valid,writer,slot,generation,publish,fault}_i` | `zhao_video_terminal_adapter_v2.term_*_o` |

`blit_req_{slot,mode}_i` belong in this table too — they come from
`zhao_video_blit_lease_v2.mgr_req_{slot,mode}_o` — and the survey filed them
under NEW because the stems differ. That is the tool being conservative in the
safe direction: it reports more work than exists rather than less, which is the
opposite of the failure mode this repository keeps finding.

**All of this is composed and tested today** by
`tests/shell/zhao_shell_v2_lease_path.sv`: three organs, both leases, one legal
frame end to end, both writers contending, and a stalled blitter that does not
stop the renderer. 43 checks. It is the prerequisite, not the packet.

---

## 2. The eighty-four-bit tuple nobody has defined — CLOSED, same day

> **Update, later on 2026-09-17.** Done. `fpga/rtl/video/zhao_fb_tuple_pkg.sv`
> defines the layout once, with pack and six accessors;
> `zhao_fb_tuple_contract.sv` checks at elaboration that the fields tile the 84
> bits with no hole and no overlap; and `fb_tuple_directed.cpp` checks the bit
> positions against literals written longhand, because a round trip through
> pack and unpack is structurally blind here — both read the same constants, so
> they agree even when a constant is wrong.
>
> The committed mutant swaps the writer and slot bits: it tiles perfectly, so
> the elaboration check passes, and it round-trips perfectly, so only the
> literal sees it. **It reported MISSED on its first run** — the sample record
> had `writer = 1` and `slot = 1`, and swapping two identical bits produces an
> identical tuple. The stimulus could not reach the fault.
>
> The rest of this section is left as written, because the reasoning is why the
> package exists.

**The sharpest finding here, and it is a hazard rather than a task.**

`video_slotmgr_v2` takes the swap echo as six separate fields:

```
swap_writer_i       1        swap_mode_i         2
swap_slot_i         1        swap_base_i        32
swap_generation_i  16        swap_span_i        32
                                        total = 84
```

`zhao_fb_ready_cdc_v2` carries "one frozen 84-bit tuple", and
`zhao_video_ready_bridge_v2` hands the shell `cdc_swap_tuple_o`, 84 bits wide.
The widths agree exactly, so the intended correspondence is not in doubt.

**The field ORDER is not defined anywhere in the tree.** The CDC is deliberately
layout-agnostic — it moves 84 bits and never looks inside — and
`fb_ready_cdc_v2_directed.cpp` drives arbitrary values and checks only that what
went in comes out. So the packing and the unpacking are BOTH Packet H's, both
unwritten, and nothing that exists today would notice if they disagreed.

Note what a disagreement would look like, because it is the reason this is
called out rather than left as an implementation detail: a rotated layout
produces a swap echo with a *plausible* slot and a *plausible* generation that
simply are not the frame's. The manager would refuse it as stale and be right
to; the screen would hold the previous frame; and every counter in the machine
would balance. That is this repository's own recurring shape — two things
corrupted in lockstep, with the instruments agreeing.

**So the layout is defined ONCE, as named constants next to the field list, used
by both the pack and the unpack, with a round-trip test.** Not two literal bit
ranges written eighty lines apart.

---

## 3. The forty-seven, grouped by where they have to come from

### 3.1 V3 programming: config, palette, page generation (20)

```
cfg_{valid,op,page_generation,selector,row,crc32}_i, cfg_rsp_ready_i
pal_load_{valid,op,slot,idx,rgb565,gen,crc_ok}_i
pg_{valid,op,src_id,tag,status,strength}_i
```

The historical shell has a command scheduler and an HPS bridge and **no V3
programming channel at all**. These are not a rename of anything: they are the
binding/palette/page path from section 6 of the composition architecture
arriving at the shell boundary for the first time. Decide once whether they are
new shell top-level ports or new decode off `u_sched`, and do not decide it
per-signal.

### 3.2 Packet-D attribute carriage (7)

```
tri_{area2,invw_plane,u_over_w_plane,v_over_w_plane,flat_request,
     continuation_tail,fragment_state}_i
```

The post-Early-Z attribute ABI. Produced upstream by the binner/projector, and
the one to watch is `tri_invw_plane_i` — CLAUDE.md records that `GEOM.DEPTHQUANT`
was corrected to consume `w` rather than `1/w`, that both projectors grew
`out_w_o`, and that `zhao_geom_wcache`'s 75-bit payload was never widened. A
replay cache between a fixed producer and a fixed consumer is a frozen copy of
yesterday's agreement. Check the width before wiring, not after.

### 3.3 Packet-E ENGINE1 share (4)

```
fill_{req_ready,data_valid,data,refused}_i
```

The guarded share, and `fill_refused_i` has a ruling already: the packet
description says it is treated **only** through Packet E's typed recoverable
path. Not a fault, not a drop.

### 3.4 Structural fault entry (4)

```
fault_{valid,writer,slot,generation}_i
```

The reset-barrier law's input side. Five distinct faults — RCP qerr, expander
wq overflow, consumed UV mismatch, invalid authoritative owner-mask identity,
metajoin sidx3 — must reach this port, bypass normal quiet/clear, RELEASE the
lease and produce no READY or publication. This is the largest single clause of
the Packet-H gate and it is four wires.

### 3.5 READY/swap CDC return (7)

```
ready_ready_i, swap_{writer,slot,generation,mode,base,span}_i
```

From `zhao_video_ready_bridge_v2`. See section 2 — six of these seven are the
tuple.

### 3.6 Remaining (5)

```
frame_clear_word_i     the V3 clear payload
sheet_req_ready_i      Surface Sheet backpressure
blit_req_{slot,mode}_i belong in section 1
test_{start_enable,attr_cov_enable}_i   tie low in production, and SAY so
```

The two `test_*` enables are the ones to write a comment against rather than a
tie-off: a test hook wired to zero in the top is invisible, and a test hook
wired to something by accident is worse.

---

## 4. What this does not say

It does not say the shell is nearly written. Sections 3.1 through 3.4 are
design, not wiring — the roadmap's correction that Packet H "is a PROTOCOL job,
not a wiring job" stands, and this document is that correction made specific.

It also does not cover the 100 new OUTPUTS, which need consumers or an explicit
decision to leave them unread, nor the eighteen historical instances the sibling
carries unchanged. Those are real and they are easier.

**The honest state:** the lease path — both writers, the shared response, the
terminal join, the frame-clear handshake — is composed, driven and tested. The
V3 programming, attribute, share and fault paths are specified and unwired. The
file is not started, and starting it before section 2's layout is pinned down
would be starting it wrong.

---

*Computed by a throwaway script whose first two versions were both wrong in the
flattering direction — it read `\bmodule\s+\w+`, matched the phrase "module
inputs," inside a header comment, and reported that `zhao_geom_bin_pipe` has
**zero ports**; then, anchored, it read the parameter list as the port list and
reported zero again. Both times the contract came back short and looked
finished. The counts above are the ones that reproduce the roadmap's
independently-recorded 63 -> 166 and 22 -> 74.*
