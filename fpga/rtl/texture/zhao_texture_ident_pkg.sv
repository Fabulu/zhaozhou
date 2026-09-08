// zhao_texture_ident_pkg.sv
//
// ---------------------------------------------------------------------------
// THE TEXTURE ISLAND'S IDENTITY ENCODINGS, NAMED ONCE
// ---------------------------------------------------------------------------
// Owner brief, 2026-09-08, S4.1: "Freeze the existing wire encodings; stop
// rediscovering their slices."
//
// WHY THIS FILE EXISTS
// --------------------
// The P0-C migration widened the owner slot from 4 bits to 6. Five separate
// places had taken the old identity apart by hand, and every one of them stayed
// LEGAL while becoming WRONG:
//
//     uvw_m index          old slot width          in range, wrong row
//     fc_wp                queue pointer           in range, wrong entry
//     fc_rp                queue pointer           in range, wrong entry
//     rsp_class_i  [15:14] class slice             in range, wrong class
//     AUX return   [13 -: 4] of a 6-bit slot       in range, wrong owner
//
// None failed elaboration. None failed lint. All produced plausible wrong data,
// and each cost a debugging round. A re-key's real bill is not the port list --
// it is every place that ever took the old identity apart, and nothing in this
// tree finds those.
//
// The cure the brief specifies is not another namespace or a translation RAM.
// It is a small shared vocabulary, so that the next width change edits ONE file
// instead of being rediscovered five times.
//
// THIS IS A BIT-IDENTICAL DEFINITION, NOT A REDESIGN
// --------------------------------------------------
// The brief is explicit: "It must not widen, truncate or reorder existing
// wires, alter array depths, or introduce a 64-entry generation table." Every
// layout below is the layout already on the wires today. Adopting this package
// must change no bit anywhere; if it does, the package is wrong, not the RTL.
package zhao_texture_ident_pkg;

  // The supported profile, stated rather than implied. The brief: "Do not claim
  // that changing GENW or owner depth works merely because the file has a
  // parameter declaration." These are the widths that are TESTED.
  localparam int unsigned SLOTW = 6;   // 64 owners
  localparam int unsigned GENW  = 8;
  localparam int unsigned SIDXW = 2;   // sample index 0..2 legal, 3 invalid
  localparam int unsigned CLSW  = 2;

  localparam int unsigned OWNERW  = SLOTW + GENW;           // 14
  localparam int unsigned SMPW    = SLOTW + SIDXW + GENW;   // 16
  localparam int unsigned TOKW    = CLSW + SMPW;            // 18
  localparam int unsigned TICKETW = GENW + SLOTW;           // 14

  typedef logic [SLOTW-1:0]   slot_t;
  typedef logic [GENW-1:0]    gen_t;
  typedef logic [SIDXW-1:0]   sidx_t;
  typedef logic [CLSW-1:0]    cls_t;

  typedef logic [OWNERW-1:0]  owner_t;    // {slot, generation}
  typedef logic [SMPW-1:0]    sample_t;   // {slot, sample_index, generation}
  typedef logic [TOKW-1:0]    token_t;    // {class, sample_handle}
  typedef logic [TICKETW-1:0] ticket_t;   // {generation, slot}  -- NOTE THE ORDER

  // ---- the public owner handle: {slot[5:0], generation[7:0]} ---------------
  function automatic owner_t make_owner(input slot_t s, input gen_t g);
    return {s, g};
  endfunction

  function automatic slot_t owner_slot(input owner_t o);
    return o[OWNERW-1 -: SLOTW];
  endfunction

  function automatic gen_t owner_generation(input owner_t o);
    return o[GENW-1:0];
  endfunction

  // ---- the sample handle: {slot[5:0], sample_index[1:0], generation[7:0]} --
  function automatic sample_t make_sample_handle(input slot_t s, input sidx_t i,
                                                 input gen_t g);
    return {s, i, g};
  endfunction

  function automatic owner_t sample_to_owner(input sample_t h);
    // The sample index is DROPPED, not folded in. Every sample of one fragment
    // maps to the same owner, which is the whole point of the encoding.
    return {h[SMPW-1 -: SLOTW], h[GENW-1:0]};
  endfunction

  function automatic sidx_t sample_index(input sample_t h);
    return h[GENW +: SIDXW];
  endfunction

  // Sample index 3 is losslessly representable and is NOT a legal TMU sample.
  // It is a value to REJECT, not a value that cannot arrive -- which is why it
  // gets a name here instead of an assumption at each use.
  function automatic logic sample_index_legal(input sample_t h);
    return sample_index(h) != SIDXW'(3);
  endfunction

  // ---- the route token: {class[1:0], sample_handle[15:0]} ------------------
  function automatic token_t make_token(input cls_t c, input sample_t h);
    return {c, h};
  endfunction

  function automatic cls_t route_class(input token_t t);
    return t[TOKW-1 -: CLSW];
  endfunction

  function automatic sample_t token_sample(input token_t t);
    return t[SMPW-1:0];
  endfunction

  // ---- the T2 ticket: {generation[7:0], slot[5:0]} -------------------------
  //
  // READ THAT LAYOUT AGAIN. It is the public owner's two fields in the OPPOSITE
  // ORDER, and both are 14 bits wide, so confusing them is a type error that no
  // width check can catch. The brief states it flatly: "The public owner and the
  // arithmetic ticket are NOT interchangeable integers."
  //
  // They exist separately because the ticket is consumed by arithmetic that
  // wants the generation in the high bits; the owner is an external identity.
  // Assigning one to the other compiles, elaborates, lints, and silently
  // addresses a different owner.
  function automatic ticket_t owner_to_ticket(input owner_t o);
    return {owner_generation(o), owner_slot(o)};
  endfunction

  function automatic owner_t ticket_to_owner(input ticket_t t);
    return {t[SLOTW-1:0], t[TICKETW-1 -: GENW]};
  endfunction

endpackage
