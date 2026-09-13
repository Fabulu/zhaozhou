// SuperStation One / MiSTer volatile bring-up core.
//
// This is deliberately smaller than the Zhaozhou shell. It proves the pinned
// MiSTer clock/reset/HPS/video contract and leaves every optional external
// interface inactive. The first board load must use this core, then immediately
// return to the known menu.rbf before any larger image is attempted.
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
        "Zhaozhou Board Bring-up;;",
        "-;",
        "T[0],Reset;",
        "V,v1"
    };

    wire forced_scandoubler;
    wire [1:0] buttons;
    wire [127:0] status;

    hps_io #(.CONF_STR(CONF_STR)) hps_io
    (
        .clk_sys(CLK_50M),
        .HPS_BUS(HPS_BUS),
        .EXT_BUS(),
        .gamma_bus(),
        .forced_scandoubler(forced_scandoubler),
        .buttons(buttons),
        .status(status),
        .status_menumask(1'b0)
    );

    wire reset_request = RESET | status[0] | buttons[1];
    reg [2:0] reset_pipe = 3'b111;

    always @(posedge CLK_50M or posedge reset_request) begin
        if (reset_request)
            reset_pipe <= 3'b111;
        else
            reset_pipe <= {reset_pipe[1:0], 1'b0};
    end

    wire core_reset = reset_pipe[2];

    // 640x480 timing from the canonical 50 MHz core clock. CE_PIXEL divides
    // the pixel rate to 25 MHz; the scaler tolerates the small VGA-rate offset.
    localparam H_ACTIVE = 10'd640;
    localparam H_SYNC_START = 10'd656;
    localparam H_SYNC_END = 10'd752;
    localparam H_TOTAL = 10'd800;
    localparam V_ACTIVE = 10'd480;
    localparam V_SYNC_START = 10'd490;
    localparam V_SYNC_END = 10'd492;
    localparam V_TOTAL = 10'd525;

    reg ce_pixel_q = 1'b0;
    reg [9:0] h_count = 10'd0;
    reg [9:0] v_count = 10'd0;
    reg [25:0] heartbeat = 26'd0;

    always @(posedge CLK_50M) begin
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
                  ((h_count < 10'd8) || (h_count >= 10'd632) ||
                   (v_count < 10'd8) || (v_count >= 10'd472));

    reg [23:0] bars;
    always @* begin
        if (!video_active)
            bars = 24'h000000;
        else if (h_count < 10'd80)
            bars = 24'hFFFFFF;
        else if (h_count < 10'd160)
            bars = 24'hFFFF00;
        else if (h_count < 10'd240)
            bars = 24'h00FFFF;
        else if (h_count < 10'd320)
            bars = 24'h00FF00;
        else if (h_count < 10'd400)
            bars = 24'hFF00FF;
        else if (h_count < 10'd480)
            bars = 24'hFF0000;
        else if (h_count < 10'd560)
            bars = 24'h0000FF;
        else
            bars = 24'h000000;
    end

    wire [23:0] pixel = border ? 24'h20FF80 : bars;

    assign CLK_VIDEO = CLK_50M;
    assign CE_PIXEL = ce_pixel_q;
    assign VGA_DE = video_active && !core_reset;
    assign VGA_HS = core_reset ||
                    !((h_count >= H_SYNC_START) && (h_count < H_SYNC_END));
    assign VGA_VS = core_reset ||
                    !((v_count >= V_SYNC_START) && (v_count < V_SYNC_END));
    assign VGA_R = VGA_DE ? pixel[23:16] : 8'h00;
    assign VGA_G = VGA_DE ? pixel[15:8] : 8'h00;
    assign VGA_B = VGA_DE ? pixel[7:0] : 8'h00;

    assign LED_USER = heartbeat[25];

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
        OSD_STATUS
    };

endmodule
