Alright, gimme your elaborate and full expert solution at fixing the shell MHz
Full shell timing-closure plan

First, a correction to my interim update: no asynchronous GPU/video clock grouping has landed yet. The raw Quartus report still says 83.4 MHz. What changed is that the newly preserved path report proves the 83.4 MHz path is a deliberately monitored vid_clk → gpu_clk crossing, not the shell’s real synchronous critical path.

The real synchronous result is:

Path	Slack at 100 MHz	Equivalent ceiling
Raw starvation-counter CDC	−1.991 ns	misleading 83.4 MHz
CMD.DMA header-validation path	−0.875 ns	about 92 MHz
Record-framer wide-write path	−0.765 ns	about 93 MHz

The only reported hold failure is on that same raw starvation-counter CDC. The worst actual synchronous hold path is positive, around +0.250 ns.

So this is not a 17% architectural disaster. It is an 8% timing-closure problem concentrated in two identifiable pieces of control logic.

My verdict: 100 MHz is very likely attainable without redesigning the shell architecture. The right solution is one CDC repair, one nearly trivial CMD.DMA dependency cut, and one proper rewrite of the record framer.

1. Repair the starvation-counter CDC instead of merely hiding it
What exists now

starvation_o is a 64-bit counter in vid_clk. The GPU side directly samples the whole bus into starve_samp, and at the frame-tick window compares the raw bus with the previous sample:

starve_samp <= starvation_o;

if (tick_d1 && (starvation_o != starve_samp))
    cdc_err <= 1'b1;

The design’s premise is that the counter only moves during active video and is quiescent in the vblank sample window. That is why the repository correctly recognizes the −1.991 ns setup and −0.952 ns hold results as meaningless synchronous timing measurements. But the current SDC deliberately keeps gpu_clk and vid_clk related, so TimeQuest continues reporting this crossing as the worst path.

The runtime tripwire is useful, but I would not call the raw 64-bit crossing a finished CDC design. A value comparison can detect that the bus changed; it cannot make a metastable sample safe.

The proper solution: a one-entry asynchronous snapshot mailbox

The video side should publish one stable starvation snapshot per frame using a request/acknowledgement toggle.

Conceptually:

// vid_clk domain
logic [63:0] starve_hold_vid;
logic        starve_req_tog_vid;
logic        starve_ack_sync_vid;

always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
        starve_hold_vid    <= 64'd0;
        starve_req_tog_vid <= 1'b0;
    end else if (frame_snapshot_event) begin
        if (starve_req_tog_vid == starve_ack_sync_vid) begin
            starve_hold_vid    <= starvation_o;
            starve_req_tog_vid <= ~starve_req_tog_vid;
        end else begin
            starve_mailbox_overflow <= 1'b1;
        end
    end
end

On gpu_clk:

// 3-FF synchronizer for request toggle
req_g1 <= starve_req_tog_vid;
req_g2 <= req_g1;
req_g3 <= req_g2;

if (req_g2 != req_g3) begin
    starve_snapshot_gpu <= starve_hold_vid;
    starve_valid_gpu    <= 1'b1;
    starve_ack_tog_gpu  <= req_g2;
end

Then synchronize the acknowledgement back to vid_clk.

This is a bundled-data crossing:

The 64-bit value is captured before the request toggle.
The source holds it unchanged until acknowledgement.
The toggle spends multiple GPU clocks in synchronizers.
By the time the GPU captures the bus, it has been stable for vastly longer than any routing skew.
Only one snapshot is produced per request.
An overrun becomes a meaningful sticky error rather than a vague “the counter moved while I looked at it.”

A full async FIFO would also work, but it is unnecessary for one 64-bit value per frame.

Timing constraints after the repair

I would initially add named, targeted CDC exceptions, not globally mark all GPU/video clocks asynchronous. The board PLL relationship is not frozen, and the shell has several other deliberate GPU/video handoffs that deserve an explicit audit.

The constraints should:

Exclude the request and acknowledgement paths into the first synchronizer flip-flops from normal setup/hold analysis.
Exclude the held 64-bit mailbox bus from ordinary clock-relative analysis.
Apply a separate maximum skew or datapath-delay sanity check to the mailbox bus.
Preserve a machine-readable list of every exception and the corresponding RTL CDC primitive.
Fail the flow if a new, unregistered GPU/video transfer appears outside that list.

Once every GPU/video crossing is an explicit synchronizer, toggle mailbox or async FIFO, the two clocks can safely be grouped asynchronously in the provisional characterization project.

I would also split the fit verdict into:

synchronousTimingPassed
cdcStructurePassed
cdcPhysicalChecksPassed

A permanently red timingPassed caused by an expected crossing is useless; it already concealed the actual −0.875 ns path.

CDC acceptance tests

The mailbox needs formal properties proving:

The source value remains stable while request and acknowledgement differ.
The source cannot overwrite an unacknowledged value.
Every source request causes exactly one destination-valid pulse.
The destination never fabricates a snapshot.
The acknowledgement corresponds to the request just consumed.

Simulation should deliberately stop using only coincident 2:1 clock edges for this test. Run randomized relative phase and several awkward clock ratios. The shell’s ordinary deterministic harness can retain its frozen clocks; the CDC test should be adversarial.

Expected result: the false 83.4 MHz path and false hold violation leave the normal timing report. Based on the preserved path list, the shell’s reported WNS should then become the real −0.875 ns, with synchronous hold still positive.

2. Cut the exact CMD.DMA path currently limiting the shell

This is the easiest real timing win.

What the path actually is

The path is:

hdr_win[28][5]
    → several header-validation comparisons
    → ok_v / success branch
    → crc_pay_r clock enable

It is not a long CRC calculation. The report shows six logic levels, including multiple LessThan stages and the ok_v ladder, ending at the enable of crc_pay_r. The total data delay is 10.117 ns, plus unfavorable clock skew.

hdr_win[28] is part of command_bytes. In M_HDR_CHK, the block validates:

header completeness,
magic,
ABI version,
reserved flags,
command-byte alignment,
command count,
descriptor length,
staging-buffer bounds,
frame-slot bounds,
header CRC,
resource epoch.

Only after every test succeeds does it execute:

crc_pay_r <= 32'hFFFF_FFFF;
cw        <= 2'd0;
m         <= M_SEED_PREP;

That makes the entire validation ladder part of the clock-enable cone for crc_pay_r.

Minimal exact fix

Reset crc_pay_r unconditionally on the final M_HCRC cycle, before entering M_HDR_CHK:

M_HCRC: begin
    crc_hdr_r <= fold_o;
    cw        <= cw + 2'd1;

    if (cw == 2'd3) begin
        crc_pay_r <= 32'hFFFF_FFFF;  // moved here
        cw        <= 2'd0;
        m         <= M_HDR_CHK;
    end
end

Then remove crc_pay_r <= 32'hFFFF_FFFF from the successful branch of M_HDR_CHK.

This does not change observable behavior:

crc_pay_r is irrelevant when the header fails.
M_SEED remains unreachable unless header validation succeeds.
Every accepted packet begins payload accumulation from the same initial CRC.
It removes the dependency from the header validation ladder to the payload CRC register.

This is the rare beautiful timing fix that should cost:

zero extra cycles,
zero interface change,
essentially zero area,
and no arithmetic change.

I would expect the entire hdr_win → crc_pay_r family to disappear, not merely improve by a few tenths.

If the header ladder itself remains critical

After that change, refit before doing anything else.

Should paths from hdr_win to done_status, need_total, or the state register remain near zero slack, split header processing into explicit stages:

M_HDR_PARSE
    latch magic/version/flags/cb/cc/crc/epoch
    compute packet_total = 40 + command_bytes

M_HDR_TEST
    calculate individual violation bits in parallel

M_HDR_DECIDE
    priority-encode those registered bits
    preserve the exact fail-safe error order

The expensive comparisons then terminate at one-bit registers. The next cycle’s priority encoder operates only on a small vector of booleans rather than on several 32-bit values.

That adds two or three clocks once per frame packet. Three 100 MHz clocks are 30 nanoseconds out of a 16.67 millisecond frame—roughly 0.00018% of the frame. There is no rational reason to force all those checks through one cycle.

After validation, narrow the internal quantities:

packet position/countdown: 13 bits for a maximum 4,096-byte packet,
command bytes: 12 bits for at most 4,056 command bytes,
command count: 8 bits,
record length: 8 bits.

The DMA’s staging limit is 4,096 bytes, and its legal record-size table tops out well below 256 bytes. Any narrowing should carry explicit high-bit rejection or assertions so the proof lives with the optimization.

Expected result after the minimal move: WNS should likely advance from −0.875 ns to the record framer’s current −0.765 ns family.

3. Rewrite the record framer as streaming hardware rather than one giant expression

This is the main RTL job.

Why the current framer is slow

In one GPU clock, the current logic does all of the following:

Compares a 32-bit packet position against 36.
Calculates pkt_len - 4.
Performs another 32-bit comparison against that result.
Determines whether it is in the record region.
Performs a 16-bit add and compare to decide whether the current byte completes the record.
Checks queue-full status.

Computes a variable byte position inside a 128-bit payload:

w_final[8*(f_rpos - 16) +: 8] = pkt_byte;
Uses that decision to enable a 144-bit queue write.

The path report shows seven logic levels and a 10.607 ns data delay from f_pos[1] to numerous recq bits. One intermediate write-control node fans out across approximately the full record width.

The current source confirms this exact structure.

Replace absolute position arithmetic with phases and countdowns

Use a tiny parser state machine:

typedef enum logic [1:0] {
    PF_IDLE,
    PF_HEADER,
    PF_RECORDS,
    PF_TRAILER
} parser_phase_e;

Registers:

logic [5:0]  header_left;  // 36 bytes
logic [11:0] command_left; // <= 4056 bytes
logic [2:0]  trailer_left; // 4 bytes

logic [7:0]  rec_pos;
logic [7:0]  rec_last;
logic [15:0] rec_opcode_q;
logic [127:0] rec_payload_q;

At the start of a verified packet:

header_left  <= 6'd36;
command_left <= pkt_len[11:0] - 12'd40;
trailer_left <= 3'd4;
phase        <= PF_HEADER;

Each accepted byte decrements the relevant counter. There is no longer any need for:

f_pos >= 36
f_pos < pkt_len - 4

on every byte.

A 13-bit total-length latch may be preferable if the exact value 4,096 has to be represented. The record-region count itself only needs 12 bits.

Replace record-completion arithmetic with an 8-bit countdown

When record-header byte 3 arrives, latch:

rec_last <= {pkt_byte, rec_len_low} - 8'd1;

Then final-byte detection is just:

record_finishes = (rec_pos == rec_last);

Or use a rec_left countdown and finish at rec_left == 1.

The DMA has already validated each record’s size and opcode before releasing the packet, so the framer is not responsible for repeating the semantic validation. It should nevertheless have a sticky tripwire or assertion for impossible high record-length bits.

Replace the dynamic payload insert with a fixed shift

The framer only retains record bytes [16,32), the four payload dwords consumed by the scheduler.

Capture them as a shift register:

logic capture_payload;

assign capture_payload =
    (rec_pos >= 8'd16) && (rec_pos < 8'd32);

if (accept_byte && capture_payload)
    rec_payload_q <= {pkt_byte, rec_payload_q[127:8]};

After bytes 16 through 31:

rec_payload_q[7:0]     = byte 16
rec_payload_q[15:8]    = byte 17
...
rec_payload_q[127:120] = byte 31

That is exactly the required little-endian dword layout.

For the final byte of a 32-byte record, queue this fixed expression:

payload_after_byte =
    (rec_pos == 8'd31)
        ? {pkt_byte, rec_payload_q[127:8]}
        : rec_payload_q;

This works because:

A 16-byte record has no payload.
A 32-byte record’s final byte is always byte 31.
For records longer than 32 bytes, all retained payload bytes were captured long before the record’s final byte.

There is no legitimate need for a variable 128-bit part-select here.

Put a registered skid stage before the wide queue

Do not write the 144-bit queue on the same cycle that the parser discovers the record boundary.

Instead:

logic         push_pending;
logic [143:0] push_data_q;

if (accept_byte && record_finishes) begin
    push_data_q  <= {rec_opcode_q, payload_after_byte};
    push_pending <= 1'b1;
end

In a separate queue-write section:

if (push_pending && queue_can_accept) begin
    recq[rq_wp] <= push_data_q;
    rq_wp       <= rq_wp + 1'b1;
    push_pending <= 1'b0;
end

Now the recq write path starts at a single registered push_pending bit and registered push_data_q, not at:

f_pos
 → pkt_len subtraction
 → region comparisons
 → record-position arithmetic
 → dynamic byte insertion
 → 144-bit write

The parser can still consume the next record’s bytes while the pending record is written. Legal records are at least 16 bytes long, so there is enormous time to drain the one-entry skid stage before another record can finish.

When the actual record queue is full, the pending register can hold one complete record. The stream only stalls when the next final byte would need to overwrite that pending entry. This is at least as safe as the current “stall the record’s final byte” law and generally more elastic.

Simplify queue occupancy

Replace pointer subtraction on the control path with an explicit queue count:

logic [FQW:0] rq_count;

assign rq_empty = (rq_count == 0);
assign rq_full  = (rq_count == FRAMER_Q);

Update it using the four {push,pop} cases. This makes full/empty intent clear and removes another subtractor from the queue-control cone.

Should the queue itself later appear in a read-side critical path, the next lever is a registered-output FIFO. With only eight 144-bit records and hundreds of free memory blocks, spending several M10Ks or a thousand flip-flops is irrelevant. But I would not do that before the registered push stage is measured.

Framer verification

The rewrite needs a sequence differential against the current implementation or an independent parser oracle:

All legal record sizes: 16, 32, 48, 64, 96, 112 and 176 bytes.
Exact payload byte order.
Zero-command packet.
Multiple mixed-size records.
Backpressure on the first, middle and final byte.
Queue full exactly on a final byte.
Simultaneous queue pop and pending push.
Scheduler refusing records during a blit.
No duplicated or lost record after stalls.
push_pending replacement and drain.
Packet trailer handling.
Sticky error behavior for impossible stream shapes.

Mutation cases should specifically attack:

byte 31 insertion,
final-byte detection,
queue-full final-byte stall,
simultaneous push/pop,
opcode byte order,
payload shift direction,
transition from header to records and records to trailer.

The existing shell golden and Duo integration suites must remain green; those are exactly the tests most likely to see a framer-induced scheduler deadlock. The shell test corpus already has full composed harnesses rather than only isolated unit tests.

Expected result: the large f_pos → recq[*] family disappears entirely.

4. Refit before touching the next candidate

After the CDC and the two real path fixes, run the exact same fixed-seed BALANCED fit and preserve:

setup paths,
hold paths,
full negative-slack census,
source/destination hierarchy,
endpoint counts,
TNS by clock,
clock-transfer report,
all constraints and exceptions.

Do not implement the fourth optimization based only on today’s top-100 list. Removing these two families changes placement, and the next critical path must be measured anew.

Based on the present report, the plausible next candidates are around −0.5 to −0.6 ns:

HPS arbiter state into CMD.DMA CRC control.
Remaining header-ladder status paths.
Smaller record-framer paths.
A few cross-module request/control paths.

Those may all move or disappear when the large framer and header cones are removed.

Likely optional fourth fix: register the HPS response at the DMA boundary

Should zhao_hps_arbiter → zhao_cmd_dma remain critical, pipeline the complete response bundle:

always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
        rsp_q <= '0;
    end else begin
        rsp_q <= hps_rsp_i;
    end
end

The bundle must keep these aligned:

beat_valid,
data,
last,
error,
grant/status fields.

The DMA is already latency-agnostic and can continue accepting one beat per clock through a one-stage pipeline. The cost is one initial cycle per burst, not reduced sustained bandwidth.

This cuts combinational influence from arbiter state and bridge control into CRC and fetch registers.

Again: only do it if the post-framer fit names that family.

5. Keep fitter settings boring until the RTL closes

The current project uses:

OPTIMIZATION_MODE = BALANCED
FITTER_EFFORT = STANDARD FIT
SEED = 1

The shell occupies only 17.8% of the provisional FPGA, so area is not a constraint. Adding registers, duplicating a little control logic, or using a few memory blocks is cheap.

Do not start by turning up fitter effort. The repository already measured that experiment:

	BALANCED	High-performance effort
Worst setup	−0.639 ns	−1.389 ns
Failing endpoints	125	17
Worst hold	+0.250 ns	−1.103 ns
Hold failures	0	2
ALMs	7,648	8,147

It reduced the count of failing endpoints but worsened the worst path and created hold failures, so BALANCED was correctly restored.

After the RTL has positive slack under BALANCED, higher effort may be rerun as an experiment, not as the basis for claiming closure.

Likewise, do not seed-lottery the design into one lucky pass. Keep seed 1 authoritative for A/B comparisons. After closure, run several additional seeds as robustness evidence; they do not replace the reproducible baseline.

6. Do not use fake timing fixes

Several tempting moves would make the report greener without making the hardware better.

Do not declare packet logic multicycle merely because it runs rarely

CMD.DMA header validation runs once per packet, but the destination registers currently sample it on the next clock. A multicycle constraint is only lawful when the RTL itself prevents the destination from sampling until the later cycle.

The correct action is to add the actual state/register boundary. Then the timing constraint follows the hardware rather than inventing behavior the hardware does not have.

Do not false-path synchronous CMD.DMA or framer paths

They are real same-clock paths. Their rarity is irrelevant to setup timing.

Do not lower the GPU target to 92 MHz

The renderer’s workload budgets assume 100 MHz and 1,666,667 compute clocks per frame. Lowering the target would quietly remove around 8% of the capacity the rest of the architecture is being sized against.

A temporary 90 MHz development image is fine. It is not closure.

Do not globally cut GPU/video before auditing every crossing

Fix the starvation snapshot structurally, enumerate the existing mode/swap/CRC/tick crossings, and only then apply broad asynchronous grouping if it matches the eventual PLL architecture.

Do not combine all three RTL changes before fitting

One measured change per commit:

CDC repair.
Fit.
CMD.DMA dependency cut.
Fit.
Framer rewrite.
Fit.

The project has already learned repeatedly that a well-reasoned change can move placement in an unexpected direction. Preserve causality.

7. The acceptance bar should be higher than WNS = +0.001 ns

The renderer is not in this shell cone. When Field, terrain, texture, raster and geometry arrive, the shell will be re-placed amid much more logic and routing pressure. A shell that barely passes by a few picoseconds is not useful headroom.

I would define closure as:

Check	Required
GPU setup WNS at 10.000 ns	at least +0.5 ns
Preferred GPU setup margin	+0.8 to +1.0 ns
GPU synchronous hold WNS	at least +0.2 ns
Video setup/hold	green
Audio setup/hold	green
Unexplained clock transfers	zero
Named CDC structural/formal checks	green
Shell golden/integration tests	green
Full negative-slack path count	zero

After passing 10 ns, run a challenge fit at 9.5 ns. A 9.5 ns pass corresponds to roughly 105 MHz and is a reasonable minimum cushion before composing the renderer. A 9.0 ns shell pass would be even healthier, but I would not delay all other work indefinitely for that number.

The final physical board will need another closure pass with:

actual PLL-generated clocks,
actual clock relationships,
package pins,
real I/O delays,
SDRAM PHY constraints,
reset deassertion checks,
and multicorner setup/hold analysis.

The present virtual-pin provisional-device result is an architecture checkpoint, not board sign-off.

Expected progression

This is the path progression I would expect, with the caveat that every line after the current one must be measured:

Stage	Expected critical result
Current raw report	−1.991 ns CDC artefact
CDC mailbox + named constraints	−0.875 ns CMD.DMA
Move crc_pay_r initialization	approximately −0.765 ns framer
Phased framer + registered push	likely −0.6 ns or better elsewhere
Optional HPS-response/header pipeline	positive slack
Margin cleanup	+0.5 ns or better

The first two synchronous fixes are not speculative “maybe the fitter likes this” tweaks. Each one directly removes the exact combinational chain visible in the preserved path report.

The work order I would hand the implementation agent
Run A — starvation snapshot mailbox

Deliver:

Request/acknowledgement toggle mailbox.
64-bit held video-domain snapshot.
Exactly one GPU-domain snapshot pulse per publication.
Sticky overflow error.
Removal of direct raw-bus sampling.
Formal mailbox properties.
Random-phase CDC simulation.
Named SDC exceptions plus separate CDC characterization.
Composed fit.

Stop and report the new synchronous WNS before changing CMD.DMA.

Run B — CMD.DMA header dependency cut

Deliver:

Move crc_pay_r initialization to the final M_HCRC cycle.
Remove header-success dependency from its enable.
Directed/random/nightly DMA tests.
CRC-gate formal proof.
Mutation proving that payload accumulation cannot start without header success.
Composed fit.

Only split the header ladder further if the new report still names it.

Run C — record-framer rebuild

Deliver:

Parser phases instead of 32-bit absolute-position comparisons.
Narrow countdown registers.
8-bit record completion counter.
Fixed payload shift rather than dynamic part-select.
One-entry registered push/skid stage.
Explicit queue count.
Old/new accepted-record sequence differential.
Full randomized backpressure.
Shell golden and Duo integration gates.
Mutation sweep.
Composed fit.
Run D — residual closure

From the new full path census only:

Pipeline the HPS response if the arbiter→DMA family remains.
Split any residual header predicate group if it remains.
Add local register duplication only where the path report proves high fanout.
Refit after each isolated change.
Run E — margin
10 ns authoritative fit with at least +0.5 ns WNS.
9.5 ns challenge fit.
Multiseed robustness sample.
Slow-corner setup and fast-corner hold reports.
Final CDC exception manifest.
Archive every report against the exact source commit.
Bottom line

No shell redesign is indicated.

The path report has reduced “the shell is too slow” to:

one raw multi-clock counter crossing that should become a proper mailbox;
one header-validation ladder unnecessarily controlling a CRC initialization;
one byte-stream parser doing a subtraction, several compares, a dynamic 128-bit insertion and a 144-bit write in the same clock.

Those are ordinary, highly actionable FPGA timing problems. The shell has huge area headroom, and none of these fixes endangers throughput.

I would bet strongly that this shell can reach 100 MHz under the current provisional target in a small number of disciplined fit iterations. The much larger unanswered question remains whether the future composed renderer can also close there—but the shell itself is not fucked.
