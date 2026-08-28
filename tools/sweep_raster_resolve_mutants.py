#!/usr/bin/env python3
"""The mutant table for zhao_raster_resolve.sv (RASTER.RESOLVE).

WHY THIS SWEEP EXISTS
---------------------
`reports/SWEEP_COVERAGE_AUDIT.md` lists RASTER.RESOLVE among the modules with a
test lane and no mutation sweep. It is in the raster tier -- the hot path
everything visible passes through -- and it is the last block before pixels
become framebuffer bytes.

Reading it to write this table turned up four files that stated the dither law
wrongly (corrected in 2ee5f28 and e706f69). The code was right in every one of
them; only the prose had rotted. That is worth remembering while reading the
mutations below: this block is unusually well built, and the interesting
question is not whether it is correct but whether its tests would NOTICE if it
stopped being.

WHAT THIS BLOCK IS FOR, WHICH DECIDES WHAT TO MUTATE
----------------------------------------------------
It resolves one finished 16x16 tile to RGB565 with an ordered dither, and hands
a CRC of the resolved bytes to DEBUG.CRC. Its laws, in the order the header
cites them:

  * the dither, from reference/src/zrender/resolve.cpp:
        q = min(MAXQ, floor((v*MAXQ + B*16 + 8) / 255))
    for ALL THREE channels; only MAXQ and QW differ (31/5, 63/6, 31/5).
  * the `min` clamps are the 2026-08-16 WHITE RAIL. Without them green at
    B >= 8 with g >= 252 quantizes to 64 and WRAPS a six-bit field, turning
    full white into a white/magenta checkerboard.
  * the Bayer phase is ABSOLUTE, bayer4[(tile_y + row) & 3][(tile_x + col) & 3],
    NOT bayer4[row & 3][col & 3]. The header says so explicitly, and notes the
    two agree for a 16-aligned tile grid -- which makes it precisely the kind
    of law a test can pass without exercising.
  * video_rules 3: [15:11] R, [10:5] G, [4:0] B, little-endian halfwords.
  * stars_and_flares 1: "the tag byte is NEVER dithered" -- it rides out
    untouched and is NOT part of the CRC, because the CRC covers framebuffer
    bytes and the tag never reaches VRAM.
  * capture_format 2: CRC-32C via the GENERATED step, init and xorout
    0xFFFF_FFFF, over the resolved bytes LOW BYTE THEN HIGH.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/raster/zhao_raster_resolve.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the dither constants -------------------------------------------
    # R01 restores the RETIRED pre-white-rail law. test_green_amplitude
    # measures that 2,037 of 4,096 (g, Bayer) pairs can tell the two apart, so
    # if this ever survives, that test has stopped reaching the RTL.
    ("R01 green goes back to the retired 32/16 amplitude",
     "  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(16), .RND(8))",
     "  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(32), .RND(16))"),
    ("R02 red takes green's quantization headroom",
     "  zhao_raster_quant #(.MAXQ(31), .QW(5), .AMP(16), .RND(8))\n"
     "    u_qr (.v_i(px_r), .bayer_i(bay), .q_o(c_r5));",
     "  zhao_raster_quant #(.MAXQ(63), .QW(5), .AMP(16), .RND(8))\n"
     "    u_qr (.v_i(px_r), .bayer_i(bay), .q_o(c_r5));"),
    ("R03 the rounding term is dropped, so every channel floors",
     "  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(16), .RND(8))",
     "  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(16), .RND(0))"),
    # Four mutations here first orphaned a signal and so failed the LINTER
    # rather than the tests. A mutant that cannot build is a discard, not
    # evidence. Each keeps its defect and every operand.
    ("R04 red and blue are swapped at the source",
     "    px_r   = tr_data_i[63:56];\n"
     "    px_g   = tr_data_i[55:48];\n"
     "    px_b   = tr_data_i[47:40];",
     "    px_r   = tr_data_i[47:40];\n"
     "    px_g   = tr_data_i[55:48];\n"
     "    px_b   = tr_data_i[63:56];"),

    # ---- the Bayer phase, which is ABSOLUTE ------------------------------
    # R05 is the law the header calls out by name. The tile grid is 16-aligned,
    # so tile-relative and absolute phase AGREE unless a test places a tile at
    # a base whose low bits differ -- exactly the case that is easy to omit.
    ("R05 the tile's row phase bits are swapped, so the phase is wrong off-grid",
     "    ph_y = tile_yp_r + ret_addr[5:4];   // (tile_y + row) & 3",
     "    ph_y = {tile_yp_r[0], tile_yp_r[1]} + ret_addr[5:4];  // (tile_y + row) & 3"),
    ("R06 the Bayer phase is transposed",
     "    bay  = bayer4(ph_y, ph_x);",
     "    bay  = bayer4(ph_x, ph_y);"),
    ("R07 two Bayer cells are swapped",
     "      4'b01_00: bayer4 = 4'd12;  4'b01_01: bayer4 = 4'd4;",
     "      4'b01_00: bayer4 = 4'd4;   4'b01_01: bayer4 = 4'd12;"),
    ("R08 the row phase uses the column bits",
     "    ph_x = tile_xp_r + ret_addr[1:0];   // (tile_x + col) & 3",
     "    ph_x = tile_xp_r + ret_addr[5:4];   // (tile_x + col) & 3"),

    # ---- the RGB565 packing (video_rules 3) ------------------------------
    ("R09 the RGB565 field order is reversed",
     "  assign px565 = {c_r5, c_g6, c_b5};",
     "  assign px565 = {c_b5, c_g6, c_r5};"),

    # ---- the tag: never dithered, never in the CRC -----------------------
    ("R10 the tag is mixed with the blue channel instead of carried whole",
     "    px_tag = tr_data_i[39:32];",
     "    px_tag = tr_data_i[39:32] ^ tr_data_i[47:40];"),
    ("R11 the tag is read from the wrong FIFO slice on the way out",
     "  assign fb_tag_o    = fifo_q[rptr][23:16];",
     "  assign fb_tag_o    = fifo_q[rptr][15:8];"),

    # ---- the tile CRC (capture_format 2) ---------------------------------
    # R12: the CRC walks the resolved bytes LOW then HIGH. Swapping the two
    # steps is the classic byte-order defect and produces a perfectly
    # plausible-looking CRC.
    ("R12 the CRC folds the resolved halfword HIGH byte first",
     "    crc_next = zhao_abi_pkg::zhao_crc32c_step(\n"
     "                 zhao_abi_pkg::zhao_crc32c_step(crc_r, fb_rgb565_o[7:0]),",
     "    crc_next = zhao_abi_pkg::zhao_crc32c_step(\n"
     "                 zhao_abi_pkg::zhao_crc32c_step(crc_r, fb_rgb565_o[15:8]),"),
    ("R13 the CRC is published without its final inversion",
     "          crc_out_r <= ~crc_next;",
     "          crc_out_r <= crc_next;"),
    # R14 anchors on the START-OF-TILE init, not the reset: the CRC is seeded
    # per tile, and a tile that inherits the previous tile's accumulator is a
    # far more plausible defect than one that never resets at all.
    ("R14 a tile inherits the previous tile's CRC accumulator",
     "          rptr      <= 1'b0;\n"
     "          crc_r     <= CRC_INIT;",
     "          rptr      <= 1'b0;\n"
     "          crc_r     <= crc_r;"),

    # ---- the stream handshake --------------------------------------------
    ("R15 the last-pixel marker fires one pixel early",
     "  assign push_d = {(ret_addr == 8'd255), ret_addr, px_tag, px565};",
     "  assign push_d = {(ret_addr == 8'd254), ret_addr, px_tag, px565};"),
    ("R16 the output claims valid while the skid FIFO is empty",
     "  assign fb_valid_o  = (fcount != 2'd0);",
     "  assign fb_valid_o  = 1'b1;"),
    ("R17 a pixel pops when EITHER side is ready, not both",
     "  assign pop         = fb_valid_o && fb_ready_i;",
     "  assign pop         = fb_valid_o || fb_ready_i;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique.

    MIXED LINE ENDINGS ARE REAL AND THEY DEFEAT A SINGLE GUESS. This used to
    pick one ending -- CRLF if the file contained any -- and translate the
    anchor to it. A file edited by a tool that writes LF into an otherwise
    CRLF file then has BOTH, and a multi-line anchor silently matches zero
    times in the region that differs. Two engine mutants failed exactly that
    way on 2026-08-28 while every single-line anchor in the same table worked.

    So both forms are tried. A multi-line anchor that matches under either is
    accepted; one that matches under neither still raises, and one that
    matches under both is still ambiguous and raises too.
    """
    for nl in ("\r\n", "\n"):
        o = old.replace("\n", nl)
        n = new.replace("\n", nl)
        count = gold.count(o)
        if count == 1:
            if o == n:
                raise ValueError("mutant identical to base")
            return gold.replace(o, n, 1)
        if count > 1:
            raise ValueError("anchor matches %d times" % count)
    raise ValueError("anchor matches 0 times (tried CRLF and LF)")


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives --
# writing a proof before the evidence is how a hole acquires a note.
EQUIVALENT = {}


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--equiv":
        proof = EQUIVALENT.get(argv[2])
        if proof is None:
            return 1
        print(proof)
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
