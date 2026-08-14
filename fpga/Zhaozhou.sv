// Zhaozhou.sv — MiSTer framework glue ONLY (charter 22: "framework glue only;
// core logic lives under fpga/rtl").
//
// PLACEHOLDER (W1). Intentionally empty: the MiSTer sys/ framework import is
// BLOCKED-on-hardware (ZH-000, plan 1.F) — no Quartus, no board on this
// machine. The Verilator lane consumes fpga/rtl/ directly and never this file.
// When sys/ is vendored (see fpga/sys/PROVENANCE.md), this module wires
// sys_top <-> the fpga/rtl core per Template_MiSTer.
module Zhaozhou (
    input  wire clk_27mhz,
    input  wire [1:0] btn_n,
    output wire [5:0] vga_r,
    output wire [5:0] vga_g,
    output wire [5:0] vga_b,
    output wire       vga_hs,
    output wire       vga_vs,
    inout  wire [31:0] sram_dq
);
    // Stub: hold outputs inactive until the framework lands.
    assign vga_r = 6'd0;
    assign vga_g = 6'd0;
    assign vga_b = 6'd0;
    assign vga_hs = 1'b1;
    assign vga_vs = 1'b1;
    assign sram_dq = 32'bz;
    wire _unused = &{1'b0, clk_27mhz, btn_n};
endmodule
