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

> **DECIDED 2026-09-17: new shell top-level ports, passed straight through,
> named exactly as the V2 blocks name them.** Not a new op space in
> `zhao_cmd_scheduler`.
>
> The reasons, in order of weight. The historical shell's entire configuration
> surface is already top-level and harness-driven — its own header says
> *"harness = HPS, D10"*, and `hps_*`, `pad_*` and the FRAME_RING view all
> arrive that way; a programming channel that arrives differently from every
> other one would be the odd thing to explain. A command-stream decoder is a
> second design with its own ABI, its own ledger ops and its own tests, and
> **Packet H's gate does not ask for one** — inventing it inside this packet is
> how a packet stops closing. And the sibling is registered
> `excluded:not-yet-adopted`: it is a candidate being measured, so the
> programming sequence belongs under test control where the gate's
> owner-and-hierarchy census can see it.
>
> **Cheap to reverse, which is the point.** A decoder inserted later sits
> *behind* these same ports and changes nothing the V2 blocks see. If the
> decision is wrong it costs one instantiation, not a re-plumb.
>
> It does carry a real cost and it should be said plainly: it is about twenty
> more top-level ports on an already large module, and the Packet-H gate's
> *"every new port is connected"* clause will be checked against every one of
> them.

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

## 3.7 Two things looked up rather than assumed

*Added later on 2026-09-17, while deciding section 3.1.*

**Nothing in the tree drives the V3 programming channel for real.** Every
instantiation of `cfg_valid_i`, `pal_load_valid_i` and `pg_valid_i` outside the
V2 blocks themselves is a harness: `zhao_prod_top.sv` drives them from generated
stimulus slices (`u61_src[133 +: 1]` and friends), and
`zhao_raster_texture_v3_fit_top.sv` fabricates a palette/config sequence to give
the fitter something to measure. So section 3.1 is not "find where this comes
from" — there is nowhere. It is a new external interface, and because it is the
shell's *boundary* it is worth one explicit decision rather than twenty
implicit ones.

**The fault discipline, by contrast, has a worked reference.**
`zhao_raster_texture_v3_fit_top.sv:286-404` implements exactly the law Packet
H's gate demands, compactly enough to read in one sitting:

* a recoverable frame fault is captured into a HELD clear request before it can
  affect job admission, and the handshake has priority over an old fault level
  sampled on the same edge — the same-edge rule the gate spells out;
* the **lifetime structural fault is deliberately absent from that request**,
  with the reason stated: only reset recovers that condition;
* the cause vector distinguishes them. Bit 1 is the recoverable frame fault and
  is **not** in the blocking set, because the clear handshake is what resolves
  it; bits 0 (setup/programming rejection), 2 (fragment error) and 3 (raster
  abort) are.

That is the shape section 3.4's four `fault_*` wires need, and it means the
largest clause of the gate is transcription plus judgement rather than
invention. Worth stating that bit 0 looked dead on a first read — assigned only
in reset in the lines that were on screen — and is not: it is set three times in
the setup state machine forty lines up.

## 3.8 The two swaps cannot land separately

*Established 2026-09-17 by trying to do the smaller one first.*

The obvious way to start the file is one swap at a time: put
`zhao_video_slotmgr_v2` and the lease organs in, keep `zhao_geom_bin_pipe`,
lint, commit, then do the bin pipe. **It does not work, and the reason is not
tidiness.**

`zhao_renderer_lease_v2` holds `frame_fault_clear_valid_o` in `ST_CLEAR` and
does not admit the frame until the handshake completes. **The V1 bin pipe has no
clear port at all** — it is one of the 43 inputs the V2 adds. So with the V1 bin
pipe retained the clear has no consumer, the lease never leaves `ST_CLEAR`, and
no frame is ever admitted. That is a deadlock, not a tie-off, and it would
present as "the sibling shell renders nothing" with every block innocent.

`ZHAO_RENDERER_LEASE_CLEAR_REQUIRED` exists and would skip the clear. It is a
mutant seam; using it to make an intermediate commit lint is changing shipped
behaviour with a macro to get past a checkpoint, which is the opposite of what
the seam is for.

**So Packet H's shell is one landing, and the increments have to happen in the
harness rather than in the file.** That is what
`tests/shell/zhao_shell_v2_lease_path.sv` is for, and why the next step is to
compose `zhao_geom_bin_pipe_v2` into it and exercise the frame-clear ordering —
a named clause of the gate that nothing currently tests.

### What the seeding step did establish

The file was seeded once from `zhao_shell_top.sv` — hash-verified against the
protected `00fdd238...5450783` first, so the sibling could only ever derive from
the version everything else was measured against — renamed at both ends, and it
**lints clean at 59 modules**. So the eighteen untouched instances transplant
without damage and the baseline is sound.

The seed was then **deleted rather than committed**, because a sibling that is
byte-identical to a protected file except for its name is two shells to maintain
and evidence of nothing. It gets committed when it is actually the V2. The
procedure is recorded here so the next attempt does not rediscover it: verify
the hash, seed, rename both ends, lint, then swap both blocks together.

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
