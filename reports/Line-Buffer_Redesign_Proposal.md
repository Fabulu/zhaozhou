Zhaozhou Scanout Line-Buffer Redesign Proposal
Decision

Keep the two ping-pong scanout line buffers. They are necessary because the video raster cannot stall while SDRAM has variable latency.

Redesign their storage as a registered-read, dual-clock, simple dual-port RAM:

GPU clock: one 64-bit write port;
video clock: one 64-bit registered read port;
no reset or initialization on the memory array;
explicit one-cycle read latency;
serializer generates the address one pixel cycle ahead;
no RAM read at all for a starved line.

This preserves the exact external pixel timing while allowing Quartus to infer block RAM.

Why the current implementation fails

The current storage is:

logic [63:0] mem [0:1][0:127];

That is two 512-pixel RGB565 line buffers, represented as 256 words of 64 bits: 16,384 bits total.

The write port is clocked, but the read is asynchronous:

assign rd_word = mem[rd_buf][rd_addr];

Quartus therefore leaves the array in registers instead of mapping it into RAM. The composed synthesis report identified this exact asynchronous read as the reason inference failed.

Unlike CMD.DMA’s full-canvas buffer, the line buffer itself is not an architectural mistake. It is small, necessary and exactly the sort of storage the FPGA’s RAM blocks are intended to hold. The mistake is only its read-port description.

Do not fix it with a naïve output register

This is insufficient:

always_ff @(posedge vid_clk)
    rd_word <= mem[rd_buf][rd_addr];

The existing serializer asks for the word containing the current pixel:

rd_buf  = display_buf;
rd_addr = x[8:2];

and immediately selects a 16-bit lane using x[1:0]. Adding one cycle of latency without changing address generation would shift the image:

pixel 0 would receive stale data;
each 64-bit word boundary would be wrong;
the first pixel of every line would be especially vulnerable;
the frame CRC would change despite the memory containing correct bytes.

The serializer’s existing line-boundary law already makes the proper solution natural: it decides the next line’s buffer and freshness at the final cycle of the preceding raster line.

New storage primitive

Introduce a small reusable inferred-memory primitive, for example:

module zhao_dc_sdp_ram #(
  parameter int unsigned DATA_W = 64,
  parameter int unsigned ADDR_W = 8
) (
  input  logic                  wr_clk,
  input  logic                  wr_en,
  input  logic [ADDR_W-1:0]     wr_addr,
  input  logic [DATA_W-1:0]     wr_data,

  input  logic                  rd_clk,
  input  logic                  rd_en,
  input  logic [ADDR_W-1:0]     rd_addr,
  output logic [DATA_W-1:0]     rd_data
);

  logic [DATA_W-1:0] mem [0:(1 << ADDR_W)-1];

  // No reset on stored data. Ownership/validity decides whether it matters.
  always_ff @(posedge wr_clk) begin
    if (wr_en)
      mem[wr_addr] <= wr_data;
  end

  // Fixed one-read-clock latency.
  always_ff @(posedge rd_clk) begin
    if (rd_en)
      rd_data <= mem[rd_addr];
  end

endmodule

The important rules are:

the memory is not initialized in synthesizable RTL;
the memory is not touched in either asynchronous-reset branch;
reads occur only inside the rd_clk process;
rd_data is meaningful only after a valid read request;
same-address cross-clock read/write behaviour is outside the legal protocol and must be proved unreachable.

The current source says the 16-Kbit structure is “one M10K.” That claim should be removed until Quartus reports the actual packing. The contract should state the logical capacity—16,384 bits—and cite measured RAM-block usage afterward rather than guessing it.

Flatten the two buffers into one RAM address space

Use the buffer selector as the address’s most significant bit:

logic [7:0] ram_wr_addr;
logic [7:0] ram_rd_addr;

assign ram_wr_addr = {fill_buf, fill_addr};
assign ram_rd_addr = {rd_req_buf, rd_req_addr};

This expresses the hardware honestly:

256 words × 64 bits
one GPU-clock write port
one video-clock read port

The line-buffer ownership state machine, completion toggles and consumption credits remain outside the memory primitive.

Hide the read latency with one-cycle lookahead

Add rd_en and change the serializer’s outputs from “read the current pixel’s word” to “request the next pixel’s word.”

Conceptually:

output logic        rd_en;
output logic        rd_req_buf;
output logic [6:0]  rd_req_addr;
input  logic [63:0] rd_word_q;

The serializer computes:

logic [15:0] x_ahead;

assign x_ahead = x + 16'd1;

always_comb begin
  if (line_last) begin
    // The next cycle is x=0 of the next raster line.
    rd_en       = next_real && next_fresh;
    rd_req_buf  = next_buf;
    rd_req_addr = 7'd0;
  end else begin
    // The next cycle remains on this raster line.
    rd_en       = line_real && line_fresh;
    rd_req_buf  = display_buf;
    rd_req_addr = x_ahead[8:2];
  end
end

At the video-clock edge:

the RAM samples this lookahead request;
VIDEO.MODE advances x;
the RAM’s registered output becomes the word for the new x;
the existing x[1:0] lane selector extracts the current pixel.

The pixel mux remains unchanged:

always_comb begin
  unique case (x[1:0])
    2'b00:   px_buf = rd_word_q[15:0];
    2'b01:   px_buf = rd_word_q[31:16];
    2'b10:   px_buf = rd_word_q[47:32];
    default: px_buf = rd_word_q[63:48];
  endcase
end
Boundary examples

At the last cycle of line N:

request: next buffer, word 0
clock edge
current raster position becomes x=0 of line N+1
RAM output becomes word 0
pixel 0 uses bits 15:0

At pixel x=3:

request: word 1, which contains pixels 4–7
clock edge
current raster position becomes x=4
RAM output becomes word 1
pixel 4 uses bits 15:0

No external video pipeline stage is added. The latency is absorbed completely by address lookahead.

Do not read starved or refilling buffers

The current serializer ignores rd_word when a line is starved, but the asynchronous read still electrically addresses the memory while the GPU may be refilling that same buffer.

With a true dual-clock RAM, that can create precisely the sort of read-during-write ambiguity Quartus warned about in AUDIO.FIFO.

Therefore:

rd_en = current_or_next_line_is_real
     && current_or_next_line_is_fresh;

A starved line must:

issue no RAM reads;
display last_px;
count starvation exactly as it does now.

The serializer already resets last_px to black and uses it whenever a real line is not fresh, so visible startup behaviour does not depend on RAM power-up contents.

This also creates a clean collision law:

The GPU may write only an EMPTY or FILLING buffer. The video side may read only a buffer whose completed fill has been accepted as fresh. Therefore a legal RAM read and a legal RAM write can never target the same buffer generation.

The existing ping-pong ownership protocol already provides almost all of this guarantee.

What must remain unchanged

This patch should not redesign the buffer-ownership crossing.

Keep:

EMPTY → FILLING → FULL → EMPTY;
the GPU-to-video completion toggles;
the video-to-GPU consumption toggles;
freshness sampled at the edge ending the previous raster line;
a fresh line consuming one complete stable buffer;
a starved line displaying only held pixels;
vblank-only spacing around aborting a FULL buffer.

The existing known abort-toggle hazard remains governed by the established vblank spacing law. The RAM inference fix neither worsens nor solves that separate CDC issue. Do not broaden this patch by changing both mechanisms at once.

Formal changes

The current formal property compares the combinational rd_word with the shadow value at the currently presented address. With a synchronous read, the expected request is delayed by one vid_clk.

Track the accepted request:

logic       rd_req_q_valid;
logic       rd_req_q_buf;
logic [6:0] rd_req_q_addr;

always_ff @(posedge vid_clk) begin
  rd_req_q_valid <= rd_en;
  if (rd_en) begin
    rd_req_q_buf  <= rd_req_buf;
    rd_req_q_addr <= rd_req_addr;
  end
end

Then prove:

If the previous cycle issued a legal read for a buffer under active
consumption, and the completed fill wrote the watched address, the current
rd_word equals the shadow value from that completed fill.

In the single-clock formal surrogate, this can be expressed with $past or the request registers.

Add these obligations:

rd_en → requested buffer belongs to a fresh line
fresh displayed pixel → previous cycle issued the matching read
fresh displayed pixel at x=0 → previous request came from line_last
fresh displayed pixel at x=4 → previous request addressed word 1
legal fill write and legal read never target the same buffer generation
rd_word may be arbitrary whenever rd_en was false

The existing never-torn proof remains the primary property; only its one-cycle observation point moves.

Directed and differential tests

All existing displayed CRCs must remain byte-identical. No golden should be repinned merely because the RAM became synchronous.

The existing patterned-frame and never-torn tests are already strong enough to catch broad shifts, but add explicit probes for:

pixel x=0 of every fresh line;
the x=3 → x=4 64-bit word boundary;
the final active pixel of 320-, 384- and 512-pixel modes;
Duo border-to-view transition y=23 → y=24;
Duo view-to-border transition y=215 → y=216;
frame wrap with a pending mode change;
fresh-line recovery immediately after starvation;
no RAM read request during a starved line.

The current directed suite already compares complete patterned rows and full displayed-frame CRCs, including Duo and starvation scenarios, so these should pass without expected-output changes.

Synthesis acceptance gate

Run a focused Quartus fit before another 42-minute shell synthesis.

Acceptance requires:

no Info (276007) uninferred-RAM message for zhao_scanout_linebuf;
nonzero block-memory usage matching the 16-Kbit logical payload;
no large register implementation of mem;
no read-during-write ambiguity warning for the line buffer;
identical directed, random, formal and displayed-CRC results;
video-clock timing closure with the registered read;
composed synthesis proceeds past the previous register-capacity failure.

Do not record a guessed M10K count. Record what Quartus actually places.

House-wide prevention

This is now a recurring failure class:

TEXTURE.CACHE;
CMD.DMA;
FORGE.CLIFF;
VIDEO.SCANOUT line buffer;
potentially ambiguous behaviour in AUDIO.FIFO.

Adopt a machine-wide memory contract for every array intended to infer RAM:

logical depth × width
write ports and clocks
read ports and clocks
fixed read latency
reset policy
read enable
read-during-write semantics
legal collision policy
expected storage class
commit-pinned synthesis evidence

Add a report gate that fails whenever Quartus reports an intended memory as uninferred. Source-level comments saying “this should become M10K” are not evidence.

Separate AUDIO.FIFO follow-up

AUDIO.FIFO is related but should not be patched inside this change. Its memory did infer, but Quartus warned that dual-clock read-during-write behaviour may not match simulation.

That needs its own small review:

define whether a same-address cross-clock collision is reachable;
prove it unreachable from the gray-pointer FIFO protocol, or
introduce a registered prefetch word whose validity is controlled by the synchronized pointers;
test the exact empty→nonempty and wraparound boundaries;
require a clean Quartus report or explicitly accepted DONT_CARE collision semantics.

The audio path has enormous timing slack compared with scanout, so a registered read-ahead stage is inexpensive. But it is a distinct async-FIFO correctness question, not the same line-buffer timing patch.

Verdict

The scanout design should remain:

SDRAM fetch
    ↓ GPU clock
two ping-pong line buffers in block RAM
    ↓ registered dual-clock read
one-cycle-ahead serializer
    ↓ video clock
one RGB565 pixel every cycle

This removes the register explosion without changing a single displayed pixel, preserves the never-torn line law, prevents reads from buffers being refilled, and converts the line buffer into the small, ordinary dual-port RAM it was always meant to be.
