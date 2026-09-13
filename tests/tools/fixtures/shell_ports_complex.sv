package shell_ports_fixture_pkg;
  typedef struct packed {
    logic [2:0] tag;
    logic       valid;
  } fixture_req_t;
endpackage

module shell_ports_fixture
  import shell_ports_fixture_pkg::*;
#(
  parameter int WIDTH = 9,
  parameter int LANES = 2,
  parameter int TWICE_LANES = LANES * 2
)(
  input  var logic signed [WIDTH-1:0] signed_i,
  input      logic        [3:0]       first_i, second_i,
  input      logic                    fixture_req_t,
  input      fixture_req_t            request_i,
  output     logic        [WIDTH-1:0] lanes_o [LANES-1:0],
  output var logic                    ready_o [TWICE_LANES-1:0]
);
endmodule
