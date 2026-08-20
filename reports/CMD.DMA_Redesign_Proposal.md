Yes. And I would make a more fundamental change than merely forcing blit_buf into M10Ks.

The redesign in one sentence

Split the command-packet DMA from the debug framebuffer blitter; keep a small real RAM for atomic command packets, but stream framebuffer data directly into a leased, invisible framebuffer slot and publish that slot only after the CRC and all SDRAM writes have completed.

CMD.SCHEDULER
    │
    ├── frame-ring claim
    │       ▼
    │   CMD.DMA
    │   ├── streaming packet validator
    │   ├── small synchronous packet RAM
    │   └── 64-bit verified stream ──► CMD.DECODER
    │
    └── DebugFrameBlit
            ▼
        DEBUG.FRAMEBLIT
        ├── inactive-slot lease
        ├── one 64-byte elastic buffer
        ├── streaming CRC-32C
        ├── guarded SDRAM writes
        └── atomic READY publication

The full-canvas buffer disappears entirely.

First: the new commit is useful, but not the solution

While I was tracing this, main advanced to 3a97980. The agent repacked the buffer from 245,760 byte-array entries into 30,720 64-bit entries, reducing Quartus elaboration from 16.2 GB to 2.7 GB. All existing tests and the CRC formal lane remain green. That is a good emergency cleanup, but the design still contains the same 1.97-Mbit full-canvas staging buffer, still cannot infer M10K, and still raises the question of whether one framebuffer should live inside CMD.DMA at all.

My answer is: no, it should not.

There are actually three CMD.DMA design defects
1. The debug blitter does not belong in command DMA

The current block performs two unrelated transactions:

fetch, validate and release sealed command packets;
copy entire debug framebuffers from HPS DDR into local SDRAM.

That coupling is why a debug-only transport path can prevent the command front end—and therefore the entire shell—from fitting. DebugFrameBlit is explicitly a debug-umbrella command, never game-facing.

Make it a separate block, probably DEBUG.FRAMEBLIT. Production builds can eventually omit it entirely.

2. The command-packet side already contradicts its own contract

The contract says CMD.DMA accepts frame packets up to the 1 MiB frame-slot bound. But the actual RTL defaults to a 4 KiB slot_buf and explicitly rejects packets where 40 + command_bytes > SLOT_BUF_BYTES.

So the current implementation already has a hidden effective maximum of 4 KiB.

That is not necessarily a bad maximum—commands should mostly contain handles and semantic operations, not megabytes of inline assets—but it must be an intentional architectural constant rather than an accidental buffer parameter.

3. The stated 1 MiB-per-frame throughput target is physically impossible today

The HPS simulation profile permits one 64-byte burst at a time, with 16 cycles to the first beat and eight one-cycle beats. Even ignoring request overhead:

1 MiB / 64 bytes = 16,384 bursts
16,384 × (16 + 8) = 393,216 GPU cycles

The shortest mode frame is 217,984 GPU cycles. The HPS bridge alone therefore cannot deliver 1 MiB inside one shortest-mode frame. The current decoder-facing output is also only one byte per clock, which would require another 1,048,576 cycles.

The target needs to be rewritten. Either:

freeze a realistic command-packet maximum, probably somewhere around 4–16 KiB after measuring captures; or
later widen and pipeline the HPS bridge substantially.

Do not make CMD.DMA enormous to satisfy a target that the upstream bridge cannot satisfy anyway.

Part 1: new CMD.DMA

CMD.DMA should return to being a command DMA only.

Packet storage

Use a real synchronous 64-bit RAM:

localparam int CMD_WORDS = CMD_PACKET_MAX_BYTES / 8;


logic [63:0] packet_ram [0:CMD_WORDS-1];
logic [63:0] packet_rd_q;


always_ff @(posedge clk) begin
    if (packet_wr_en)
        packet_ram[packet_wr_addr] <= hps_rsp_i.data;


    if (packet_rd_en)
        packet_rd_q <= packet_ram[packet_rd_addr];
end

Important synthesis rules:

no array initialization;
no writes inside an asynchronous-reset process;
no combinational array reads;
reset only validity bits, pointers and state.

A provisional 16 KiB packet RAM would require roughly 13 M10Ks—well under 3% of the device’s 553 M10Ks. A 4 KiB RAM requires only around four. The final size should come from measured command-packet distributions rather than the 1 MiB ring stride.

Fetch and validation

The packet is fetched once into RAM while validation happens on the incoming 64-bit beats.

The first 64-byte burst supplies the entire fixed header. Capture the header fields into ordinary registers; do not later random-index packet RAM to read them.

During the rest of the fetch:

update CRC-32C over the command payload;
structurally walk records;
check opcode, declared record size, alignment and command count;
capture the trailing payload CRC;
write each 64-bit beat into packet RAM.

There is a useful alignment property here:

payload begins at byte 36  → offset 4 inside an 8-byte beat
all records are multiples of 16 bytes

Therefore every record header begins at the same byte position within an eight-byte beat. The opcode and record size are always available together without a byte-addressed RAM.

After the final beat:

compare payload CRC;
compare walked count;
set the packet-valid gate;
replay the packet from synchronous RAM.

This preserves the current strong command law: zero bytes reach the decoder unless the whole packet is valid.

Widen the decoder-facing stream

Replace:

pkt_valid_o
pkt_ready_i
pkt_byte_o[7:0]

with something like:

pkt_valid_o
pkt_ready_i
pkt_data_o[63:0]
pkt_keep_o[7:0]
pkt_last_o
pkt_len_o[31:0]

The current shell record framer can initially adapt this into its existing record representation. Eventually CMD.DECODER should consume the 64-bit stream directly.

Keeping a one-byte interface permanently would preserve an artificial throughput wall even after the full-canvas buffer is removed.

Part 2: new DEBUG.FRAMEBLIT

This is where I disagree with the currently suggested two-pass DDR solution.

Do not preserve the wrong atomicity boundary

The existing contract says no VRAM write may occur before the complete source CRC passes. That requirement is what forced the whole framebuffer buffer.

But raw writes to an inactive, uncommitted framebuffer slot are not visible.

The actual externally meaningful commit point is when the slot becomes READY. The shell already only toggles slot readiness when blit_status == 0, and FRAMECTL only swaps to a committed READY slot.

So amend the law from:

No byte is written to VRAM before CRC verification.

to:

No framebuffer slot becomes visible or READY before every byte has been written, all writes have retired, and the CRC matches.

That lets the framebuffer slot itself serve as the transaction buffer. That is what double buffering is for.

New blit transaction

The flow becomes:

1. Acquire exclusive lease on an invisible framebuffer slot.
2. Validate mode, length, source and lease.
3. Read one 64-byte source burst into a tiny chunk buffer.
4. Issue one guarded 64-byte local-SDRAM write.
5. Drain the eight 64-bit beats with proper ready/valid backpressure.
6. Repeat to end of canvas while updating CRC.
7. Wait until every SDRAM write has retired.
8. CRC match  → publish slot READY.
   CRC fail   → release slot as FREE, never publish it.

Only one 64-byte buffer is necessary:

logic [63:0] chunk [0:7];

That is 512 data bits instead of 1.97 million.

A two-buffer ping-pong version would use 1,024 bits and allow the next HPS burst to fill while the previous chunk waits for local-SDRAM service, but I would first implement the single-buffer version. The old implementation also performed the HPS read and VRAM write as separate phases, so the single-buffer version should not fundamentally worsen total transaction cost.

Why this is better than two HPS-DDR passes

Two-pass DDR sounds attractive because it preserves “zero guard writes on reject,” but the pixel arena is currently just a raw HPS address with no descriptor or ownership state.

That creates a time-of-check/time-of-use problem:

pass 1 verifies bytes A
HPS mutates the arena
pass 2 commits bytes B

To make two-pass verification sound, you would first need a sealed pixel-arena descriptor or lease that forbids HPS writes between both passes.

The single-pass transactional-slot design has no such problem. If HPS changes the source during the read, the resulting stream fails its expected CRC and the dirty inactive slot is never published.

The missing framebuffer lease

The current guard verifies only that writes fall inside the command-granted slot and span. It does not know whether that slot is currently displayed.

That must be fixed before speculative writes are allowed.

Introduce a framebuffer-slot lease owned by the shell/frame-control seam:

input  logic        fb_lease_valid_i;
input  logic        fb_lease_slot_i;
input  logic [15:0] fb_lease_generation_i;


output logic        fb_lease_release_o;
output logic        blit_publish_o;

The slot manager should enforce a small state machine:

FREE → WRITING → READY → DISPLAYED → FREE
                ↘ failure → FREE

A slot is leasable only if it is:

not currently displayed;
not already READY or committed for the next swap;
not already being written.

The dst_slot supplied in DebugFrameBlit remains in the frozen ABI, but it must match the granted lease. It is no longer trusted merely because it is 0 or 1.

Fix write-data backpressure

The current DMA emits guard_wvalid_o without a corresponding guard_wready_i; the shell catches overflow with a sticky error after the fact.

Replace that with an actual handshake:

output logic        guard_wvalid_o;
input  logic        guard_wready_i;
output logic [63:0] guard_wdata_o;
output logic        guard_wlast_o;

The beat pointer advances only on:

guard_wvalid_o && guard_wready_i

The 64-byte chunk buffer naturally keeps the data stable while stalled.

Wait for physical write retirement before publishing

Another current weakness is that “all data beats entered the shell queue” is not the same as “all SDRAM writes completed.”

The VRAM arbiter already returns word credits when bursts retire, routed to the issuing client. MEM.GUARD currently ignores those credits.

Expose them to the blitter:

input logic [7:0] guard_retired_words_i;

Maintain:

bytes_issued
bytes_retired

The slot may become READY only when:

bytes_issued  == byte_len
bytes_retired == byte_len
crc_final     == expected_crc
no bridge error
no guard violation
lease still valid

That closes the real atomicity boundary.

Proposed state machines
CMD.DMA
C_IDLE
C_HEADER_REQUEST
C_HEADER_RECEIVE
C_HEADER_CHECK
C_PAYLOAD_REQUEST
C_PAYLOAD_RECEIVE_AND_VALIDATE
C_PACKET_CHECK
C_REPLAY_PRIME
C_REPLAY
C_DONE
C_ERROR
DEBUG.FRAMEBLIT
B_IDLE
B_ACQUIRE_LEASE
B_VALIDATE
B_READ_REQUEST
B_READ_CHUNK
B_GUARD_REQUEST
B_GUARD_VERDICT
B_WRITE_CHUNK
B_NEXT_CHUNK
B_WAIT_RETIRE
B_CRC_DECIDE
B_PUBLISH
B_ABORT

The HPS burst reader and CRC-32C lane can be shared internally because CMD.DMA and DEBUG.FRAMEBLIT already serialize access to the single bridge port. But they should remain separate modules and contracts.

Formal properties I would require

For CMD.DMA:

pkt_valid → header_gate && payload_gate && walk_gate
error → no packet beat emitted
emitted byte stream == fetched packet
reset → no partial packet publication
packet length never exceeds configured packet RAM

For DEBUG.FRAMEBLIT:

publish → CRC matched
publish → issued_bytes == retired_bytes == expected_len
publish → lease was continuously valid
guard write → leased slot
guard write → slot is not displayed
CRC failure → no publish
bridge/guard failure → no publish
reset during transaction → no publish
each destination byte written exactly once on success
wvalid && !wready → data and last remain stable

And the key composition test:

Keep slot 0 displayed.
Stream a deliberately bad blit into leased slot 1.
Prove slot 0’s displayed CRC never changes.
Prove slot 1 never becomes READY.
Send a good complete blit into slot 1.
Prove it publishes only after the final SDRAM retirement.
Swap and prove displayed CRC equals the expected framebuffer.

That proves the new safety law at the point the player actually observes.

Resource result I would expect

The exact numbers still require Quartus, but structurally:

Item	Current	Redesigned
Blit staging	1.97 Mbit	512–1,024 bits
Command packet storage	byte-array flops	4–16 KiB synchronous M10K
Debug blit HPS reads	1 full pass	1 full pass
Local framebuffer writes	1 full pass	1 full pass
DSP use	0	0
CRC machine	existing	existing/shared
Atomicity	zero raw writes before CRC	zero visible publication before CRC + retirement

Also, the current buffer is oversized: the ABI says Duo stores 196,608 bytes, while 245,760 is the larger displayed/allocation figure. Even a retained real-RAM buffer would not need the full current parameter—but it would still be an unjustifiable amount of M10K for a debug transport.

Implementation order
Split DEBUG.FRAMEBLIT out of CMD.DMA.
Add framebuffer lease and explicit write-data backpressure.
Implement the 64-byte transactional blit stream.
Route arbiter retirement credits back to the blitter.
Replace slot-ready-on-done with explicit publish-after-retirement.
Convert command packet storage to synchronous 64-bit M10K.
Widen packet output to 64 bits.
Rewrite CMD.DMA’s packet-size and throughput contract.
Run block fit before touching anything else.
Run the composed shell fit once the full-canvas array is gone.
Verdict

I would not spend 192—or even 154—M10Ks preserving the current contract. I would not read the framebuffer twice either.

The right design is:

strict full-packet buffering for the small command packet, because decoder side effects genuinely require atomic release;
speculative writes into a leased invisible framebuffer slot for the giant debug image, because framebuffer publication already supplies the transaction boundary.

That removes the pathological storage, keeps one-pass bandwidth, detects mutable HPS sources through the CRC, and strengthens the actual safety property: a bad or incomplete image can never become visible.
