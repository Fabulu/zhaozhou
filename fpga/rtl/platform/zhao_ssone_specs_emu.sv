// SuperStation One / MiSTer physical Zhaozhou specification runner.
//
// The proven board wrapper stays unchanged in zhao_ssone_bringup.sv. This
// separately named image executes committed CRC, raster-fill, and packed-DSP
// RTL vectors in fabric, then presents the latched result and signature.
module emu
(
    `include "sys/emu_ports.vh"
);

    assign ADC_BUS = 'z;
    assign USER_OUT = '1;
    assign {UART_RTS, UART_TXD, UART_DTR} = '0;
    assign {SD_SCK, SD_MOSI, SD_CS} = 'z;

    assign {
        SDRAM_DQ,
        SDRAM_A,
        SDRAM_BA,
        SDRAM_CLK,
        SDRAM_CKE,
        SDRAM_DQML,
        SDRAM_DQMH,
        SDRAM_nWE,
        SDRAM_nCAS,
        SDRAM_nRAS,
        SDRAM_nCS
    } = 'z;

    assign {
        DDRAM_CLK,
        DDRAM_BURSTCNT,
        DDRAM_ADDR,
        DDRAM_DIN,
        DDRAM_BE,
        DDRAM_RD,
        DDRAM_WE
    } = '0;

    assign VGA_SL = '0;
    assign VGA_F1 = 1'b0;
    assign VGA_SCALER = 1'b0;
    assign VGA_DISABLE = 1'b0;
    assign HDMI_FREEZE = 1'b0;
    assign HDMI_BLACKOUT = 1'b0;
    assign HDMI_BOB_DEINT = 1'b0;

    assign AUDIO_S = 1'b0;
    assign AUDIO_L = '0;
    assign AUDIO_R = '0;
    assign AUDIO_MIX = '0;

    assign LED_POWER = 2'b00;
    assign LED_DISK = 2'b00;
    assign BUTTONS = 2'b00;

    assign VIDEO_ARX = 13'd4;
    assign VIDEO_ARY = 13'd3;

    localparam CONF_STR = {
        "Zhaozhou Hardware Specs;;",
        "-;",
        "T[0],Reset;",
        "V,v1"
    };

    wire forced_scandoubler;
    wire [1:0] buttons;
    wire [127:0] status;
    wire clk_core;
    wire pll_locked;

    // The MiSTer top-level clock-control blocks require CLK_VIDEO to come from
    // a PLL output, not directly from a package clock pin. This is the pinned
    // Template_MiSTer 50 MHz -> 20 MHz core PLL.
    pll pll
    (
        .refclk(CLK_50M),
        .rst(RESET),
        .outclk_0(clk_core),
        .locked(pll_locked)
    );

    hps_io #(.CONF_STR(CONF_STR)) hps_io
    (
        .clk_sys(clk_core),
        .HPS_BUS(HPS_BUS),
        .EXT_BUS(),
        .gamma_bus(),
        .forced_scandoubler(forced_scandoubler),
        .buttons(buttons),
        .status(status),
        .status_menumask(1'b0)
    );

    wire reset_request = RESET | !pll_locked | status[0] | buttons[1];
    reg [2:0] reset_pipe = 3'b111;

    always @(posedge clk_core or posedge reset_request) begin
        if (reset_request)
            reset_pipe <= 3'b111;
        else
            reset_pipe <= {reset_pipe[1:0], 1'b0};
    end

    wire core_reset = reset_pipe[2];

    wire spec_done;
    wire spec_pass;
    wire [7:0] spec_fail_code;
    wire [7:0] spec_active_test;
    wire [31:0] spec_completed;
    wire [31:0] spec_signature;

    zhao_ssone_spec_tests u_spec_tests (
        .clk(clk_core),
        .rst_n(!core_reset),
        .done_o(spec_done),
        .pass_o(spec_pass),
        .fail_code_o(spec_fail_code),
        .active_test_o(spec_active_test),
        .completed_o(spec_completed),
        .signature_o(spec_signature)
    );

    // The pinned template PLL emits 20 MHz. Dividing pixel enable by two and
    // using the template's 638x262 raster gives about 59.9 frames/s. These are
    // the known-good MiSTer template timing. Only the active pixels encode the
    // independently checked Zhaozhou block result.
    localparam H_ACTIVE = 10'd529;
    localparam H_SYNC_START = 10'd544;
    localparam H_SYNC_END = 10'd590;
    localparam H_TOTAL = 10'd638;
    localparam V_ACTIVE = 10'd240;
    localparam V_SYNC_START = 10'd245;
    localparam V_SYNC_END = 10'd248;
    localparam V_TOTAL = 10'd262;

    reg ce_pixel_q = 1'b0;
    reg [9:0] h_count = 10'd0;
    reg [9:0] v_count = 10'd0;
    reg [25:0] heartbeat = 26'd0;

    always @(posedge clk_core) begin
        if (core_reset) begin
            ce_pixel_q <= 1'b0;
            h_count <= 10'd0;
            v_count <= 10'd0;
            heartbeat <= 26'd0;
        end else begin
            ce_pixel_q <= ~ce_pixel_q;
            heartbeat <= heartbeat + 1'd1;

            if (ce_pixel_q) begin
                if (h_count == H_TOTAL - 1'd1) begin
                    h_count <= 10'd0;
                    if (v_count == V_TOTAL - 1'd1)
                        v_count <= 10'd0;
                    else
                        v_count <= v_count + 1'd1;
                end else begin
                    h_count <= h_count + 1'd1;
                end
            end
        end
    end

    wire video_active = (h_count < H_ACTIVE) && (v_count < V_ACTIVE);
    wire border = video_active &&
                  ((h_count < 10'd8) || (h_count >= 10'd521) ||
                   (v_count < 10'd8) || (v_count >= 10'd232));

    wire signature_band = video_active && (h_count < 10'd512)
                          && (v_count >= 10'd72) && (v_count < 10'd168);
    wire signature_bit = spec_signature[5'd31 - h_count[8:4]];
    wire fail_bit = spec_fail_code[3'd7 - h_count[8:6]];

    reg [23:0] pixel;
    always @* begin
        if (!video_active) begin
            pixel = 24'h000000;
        end else if (!spec_done) begin
            pixel = h_count[5] ? 24'h2050A0 : 24'h102850;
        end else if (spec_pass) begin
            pixel = 24'h083818;
            if (signature_band)
                pixel = signature_bit ? 24'h80FFC0 : 24'h104828;
            if (border)
                pixel = 24'h40FF80;
        end else begin
            pixel = 24'h580808;
            if (signature_band)
                pixel = fail_bit ? 24'hFFFFFF : 24'h300000;
            if (border)
                pixel = 24'hFF3030;
        end
    end

    assign CLK_VIDEO = clk_core;
    assign CE_PIXEL = ce_pixel_q;
    assign VGA_DE = video_active && !core_reset;
    assign VGA_HS = !core_reset &&
                    ((h_count >= H_SYNC_START) && (h_count < H_SYNC_END));
    assign VGA_VS = !core_reset &&
                    ((v_count >= V_SYNC_START) && (v_count < V_SYNC_END));
    assign VGA_R = VGA_DE ? pixel[23:16] : 8'h00;
    assign VGA_G = VGA_DE ? pixel[15:8] : 8'h00;
    assign VGA_B = VGA_DE ? pixel[7:0] : 8'h00;

    assign LED_USER = spec_pass ? heartbeat[24] : heartbeat[20];

    wire _unused = &{
        1'b0,
        forced_scandoubler,
        HDMI_WIDTH,
        HDMI_HEIGHT,
        CLK_AUDIO,
        SD_MISO,
        SD_CD,
        DDRAM_BUSY,
        DDRAM_DOUT,
        DDRAM_DOUT_READY,
        UART_CTS,
        UART_RXD,
        UART_DSR,
        USER_IN,
        OSD_STATUS,
        spec_active_test,
        spec_completed
    };

endmodule
