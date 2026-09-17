# Packet H — the channel map, derived from the interfaces

*2026-09-17. Written from the four organs' actual port lists plus
`zhao_video_slotmgr_v2`, not from the architecture prose, so it can be checked
against the RTL rather than against a paraphrase.*

## Why this exists

`fpga/rtl/common/zhao_shell_top_v2.sv` does not exist, and it is the only thing
between a closed G8B and Packet I's promotion. The obstacle is not the size of
the file — it is that **`zhao_video_slotmgr_v2` keeps eight of `zhao_video_
slotmgr`'s twenty-two ports**, removes the entire lease/publish/release
interface, and adds sixty-five for a writer-aware protocol. There is no
transformation from the V1 wiring to the V2 one; the flow has to be re-plumbed
through three of the new organs.

So the first real work is learning where each channel goes. That is this file.

## The five blocks and what each one is for

| block | ports | role |
|---|---:|---|
| `zhao_video_slotmgr_v2` | 73 | the manager: owns slots, grants writer-tagged leases, takes terminal returns, publishes ready |
| `zhao_renderer_lease_v2` | 35 | the renderer's client: one held frame request in, one held admitted frame out, **and the sole driver of V3's frame-clear handshake** |
| `zhao_video_terminal_adapter_v2` | 27 | funnels the blit's publish/release and the renderer's terminal return into the manager's single `term_*` channel |
| `zhao_video_ready_bridge_v2` | 30 | GPU↔video CDC for ready/swap, blank/scanout handshaking, **and the producer of `lease_open`** |
| `zhao_engine1_raw_last_v2` | 21 | ENGINE1's raw/last framing; independent of the lease path |

## The channel map

```
                    ready_bridge.lease_open_o
                              |
                              v
  frame_req_* ----> [ renderer_lease_v2 ] ----render_req_*----> [ slotmgr_v2 ]
  (shell render                 ^                                    |
   request)                     |                                    |
                                +--------------rsp_*-----------------+
                                   (writer-tagged; this leaf takes
                                    writer 1, the blit side takes 0)
                                |
                                +-- frame_fault_clear_{valid_o,ready_i} --> V3
                                |
                                +-- frame_* (writer, slot, generation, mode,
                                    base, span, width, height, stride,
                                    view1_y, view1_offset) --> the render path

  blit publish/release --+
                         +--> [ terminal_adapter_v2 ] --term_*--> [ slotmgr_v2 ]
  renderer term ---------+

  [ slotmgr_v2 ] --ready_*--> [ ready_bridge_v2 ] --cdc_swap_*--> [ slotmgr_v2 ]
                                       ^  |
                          scanout_ack_i|  | blank_ack_o / blank_cmd_i
```

**Five facts that fall out of it, each checkable against the port lists:**

1. **`lease_open` flows backwards relative to intuition** — the CDC bridge
   produces it (`lease_open_o`) and the lease consumes it (`lease_open_i`). It
   is the Packet-H reset-epoch barrier, and its comment is explicit that it
   *"gates creation of new renderer work, never retirement of work this leaf
   already owns."* Wire it the other way and the barrier gates the wrong thing.
2. **`rsp_*` is SHARED and writer-tagged.** `zhao_renderer_lease_v2` raises
   `rsp_ready_o` only for `rsp_writer_i == 1`. The blit path must take writer 0
   off the same channel. A composition that gives the renderer a private
   response channel will look correct and deadlock the blit.
3. **The renderer never publishes directly.** Its terminal return goes through
   `terminal_adapter_v2` alongside the blit's publish/release, and the adapter
   presents the manager's single `term_*` channel. That is the mechanism behind
   the gate clause about a sequence mismatch draining and closing owner counts.
4. **Frame geometry is derived, not carried.** `frame_width_o`, `height`,
   `stride`, `view1_y`, `view1_offset` come out of the lease from the mode plus
   the manager's immutable response — the interface comment says *"the identity
   is the complete immutable manager response; geometry is mode-derived."* The
   shell must not compute them a second time.
5. **`zhao_engine1_raw_last_v2` is not on this path at all.** Its eighteen
   "unmatched" ports in the wiring survey are the ENGINE1 raw/last framing, and
   its counterparts exist under other names in `zhao_mem_guard`,
   `zhao_vram_arbiter`, `zhao_render_asset_mux` and `zhao_geom_mem_adapter`. It
   can be composed independently of the lease re-plumbing.

## What is still owed before the shell can be written

* the **frame-fault clear handshake's ordering**: the lease requests one clear
  only after a newly accepted renderer lease AND old-work drain, and withholds
  the new frame until acceptance — the sequencing is in section 13's Packet-H
  paragraph and is not derivable from port names;
* the **reset-barrier entry** for RCP qerr, expander wq overflow, consumed UV
  mismatch, invalid authoritative owner-mask identity and metajoin sidx3 — five
  faults that bypass the normal quiet/clear path;
* the **sequence-abort RELEASE control**.

Those three are behaviour, not wiring, and each has an explicit gate clause. The
map above is the part that could be settled by reading interfaces, and it is
settled.

## Recommended first step, which is NOT the shell

Compose these five blocks in a **test harness** and drive one legal transaction
end to end — frame request, lease grant, frame admitted, terminal return, ready
published, swap acknowledged. It exercises every channel above, it fails loudly
if any of the five facts is wired wrong, and it is a few hundred lines rather
than two thousand. The shell then composes a protocol that has already been
shown to work, instead of debugging the protocol and the composition at once.
