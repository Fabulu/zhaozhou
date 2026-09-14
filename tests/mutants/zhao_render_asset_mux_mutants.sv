// zhao_render_asset_mux_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-E3 selector shim. NOT SHIPPED.
// This file declares no module and copies no production RTL. A private inverse
// build compiles it immediately before the exact zhao_render_asset_mux source
// and defines exactly one selector. Ordinary production/direct builds define
// none. The exact mux contains both the collision sentinel and the independent
// invariants which observe these changed expressions.
`default_nettype none

// The marker makes every pair of selectors a collision without maintaining a
// fifteen-pair matrix. The exact mux rejects the collision at compile/elaboration.
`ifdef ZHAO_RENDER_ASSET_MUTANT_HOLD_GUARD_VALID
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Keep VALID through WAIT_VERDICT. A denied request sees guard READY still
    // high there and is physically accepted twice.
    `define ZHAO_RENDER_ASSET_GUARD_VALID_EXPR(is_offer, is_verdict) ((is_offer) || (is_verdict))
  `endif
`endif

`ifdef ZHAO_RENDER_ASSET_MUTANT_DRIFT_SUBOWNER
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Drift after the first four raw halfwords: the second physical 64-bit group
    // of a texture line is sent to geometry instead of the captured owner.
    `define ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR(owner_texture, raw_count) \
            ((owner_texture) && ((raw_count) < 6'd4))
  `endif
`endif

`ifdef ZHAO_RENDER_ASSET_MUTANT_TEXTURE_PACK64
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Route every texture halfword through the geometry 64-bit packer.
    `define ZHAO_RENDER_ASSET_ROUTE_TEXTURE_EXPR(owner_texture, raw_count) 1'b0
  `endif
`endif

`ifdef ZHAO_RENDER_ASSET_MUTANT_TERMINATE_BEAT7
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Release a texture owner after seven halfwords.
    `define ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR(exact_count) 6'd7
  `endif
`endif

`ifdef ZHAO_RENDER_ASSET_MUTANT_ACCEPT_BEAT9
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Accept a ninth texture halfword as part of the line.
    `define ZHAO_RENDER_ASSET_TEXTURE_TERMINAL_EXPR(exact_count) 6'd9
  `endif
`endif

`ifdef ZHAO_RENDER_ASSET_MUTANT_DENIAL_SILENCE
  `ifdef ZHAO_RENDER_ASSET_MUTANT_SELECTED
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_RENDER_ASSET_MUTANT_SELECTED
    // Consume a real guard denial without delivering the cache refusal pulse.
    `define ZHAO_RENDER_ASSET_DENY_PULSE_EXPR(intended_pulse) 1'b0
  `endif
`endif

`default_nettype wire
