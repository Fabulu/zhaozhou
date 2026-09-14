// zhao_packet_g_connected_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-G seam selector. NOT SHIPPED. Compile
// immediately before tb_packet_g_lease_cdc and define at most one selector.
`default_nettype none

`ifdef ZHAO_PACKET_G_CONNECTED_MUTANT_RAW_TERMINAL_READY
  // Historical shell defect: raw writer publication, rather than the slot
  // authority's accepted clean READY event, is the CDC source.
  `define ZHAO_PACKET_G_CONNECTED_READY_VALID(clean_ready, terminal_valid, terminal_publish) \
    (((terminal_valid) && (terminal_publish)) || ((clean_ready) && 1'b0))
`endif

`default_nettype wire
