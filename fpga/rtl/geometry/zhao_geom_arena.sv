// zhao_geom_arena.sv — the GEOM.BINNER chunk allocator: a bump allocator over
// a FIXED arena, with the overflow wall the ledger's "safe overflow" demands
// (phase 5, ZH-058 / ZH-026).
//
// This module is a separate file for exactly the reason zhao_raster_fill.sv is:
// it is the piece tests/formal/geom_binner_arena_bounds.sby proves, and a proof
// of a COPY of the allocator would be worthless. The .sby instantiates this
// module — the same bytes zhao_geom_binner synthesises.
//
// THE LAW IT ENFORCES (design/blocks.yml, GEOM.BINNER): "Safe overflow: excess
// triangles degrade to next-frame, never scribble." The "never scribble" half
// is this module and it is absolute: `alloc_ok_o` is FALSE whenever the arena
// is full, so `alloc_ptr_o` is never presented as valid outside [0, CHUNKS).
// There is no wrap, no modulo, no reuse of a live chunk, and no path by which
// a caller that respects `alloc_ok_o` can address arena entry CHUNKS or beyond.
//
// The whole arena is handed back in ONE cycle by `release_i` (the frame
// boundary). Chunks are never individually freed: a tile list lives for exactly
// one frame, so a free list would be state and arithmetic bought for nothing.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_geom_binner).

module zhao_geom_arena #(
  // Number of chunks in the arena. Must be a power of two so PTR_W is exact.
  parameter int unsigned CHUNKS = 256,
  parameter int unsigned PTR_W  = 8    // $clog2(CHUNKS)
) (
  input  logic clk,
  input  logic rst_n,

  input  logic             release_i,    // hand the WHOLE arena back (frame edge)
  input  logic             alloc_i,      // request one chunk this cycle
  output logic             alloc_ok_o,   // granted — alloc_ptr_o is the chunk
  output logic [PTR_W-1:0] alloc_ptr_o,
  output logic             full_o,       // the wall: no further grant until release
  output logic [PTR_W:0]   used_o        // 0..CHUNKS
);

  // used_r counts allocated chunks and saturates at CHUNKS by construction:
  // it only ever increments on a GRANT, and a grant requires used_r < CHUNKS.
  localparam logic [PTR_W:0] CAP = (PTR_W+1)'(CHUNKS);

  logic [PTR_W:0] used_r;

  assign used_o      = used_r;
  assign full_o      = (used_r == CAP);
  assign alloc_ok_o  = alloc_i && !full_o;
  assign alloc_ptr_o = used_r[PTR_W-1:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)            used_r <= {(PTR_W+1){1'b0}};
    else if (release_i)    used_r <= {(PTR_W+1){1'b0}};
    else if (alloc_ok_o)   used_r <= used_r + {{PTR_W{1'b0}}, 1'b1};
  end

endmodule : zhao_geom_arena
