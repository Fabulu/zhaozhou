// zhao_texture_ident_probe.sv
//
// A thin combinational wrapper so `zhao_texture_ident_pkg`'s functions can be
// exercised from the C++ harness. It holds no state and ships nowhere: it is a
// fixture, and `fpga/rtl/synth/` is where fixtures live for FIT purposes, but
// this one is simulation-only and never enters a fit closure.
module zhao_texture_ident_probe
  import zhao_texture_ident_pkg::*;
(
    input  var logic [5:0]  slot_i,
    input  var logic [7:0]  gen_i,
    input  var logic [1:0]  sidx_i,
    input  var logic [1:0]  cls_i,

    output var logic [13:0] owner_o,
    output var logic [5:0]  owner_slot_o,
    output var logic [7:0]  owner_gen_o,

    output var logic [15:0] sample_o,
    output var logic [13:0] sample_owner_o,
    output var logic [1:0]  sample_idx_o,
    output var logic        sample_legal_o,

    output var logic [17:0] token_o,
    output var logic [1:0]  token_class_o,
    output var logic [15:0] token_sample_o,

    output var logic [13:0] ticket_o,
    output var logic [13:0] ticket_back_o
);

  assign owner_o        = make_owner(slot_i, gen_i);
  assign owner_slot_o   = owner_slot(owner_o);
  assign owner_gen_o    = owner_generation(owner_o);

  assign sample_o       = make_sample_handle(slot_i, sidx_i, gen_i);
  assign sample_owner_o = sample_to_owner(sample_o);
  assign sample_idx_o   = sample_index(sample_o);
  assign sample_legal_o = sample_index_legal(sample_o);

  assign token_o        = make_token(cls_i, sample_o);
  assign token_class_o  = route_class(token_o);
  assign token_sample_o = token_sample(token_o);

  assign ticket_o       = owner_to_ticket(owner_o);
  assign ticket_back_o  = ticket_to_owner(ticket_o);

endmodule
