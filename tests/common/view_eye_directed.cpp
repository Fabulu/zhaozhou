// view_eye_directed.cpp -- zhao_view_eye, the two views' camera positions
// (owner ruling R63).
//
// WHAT THIS PROVES, and what it deliberately does not.
//
// `tests/command/cmd_exec_directed.cpp` cases 22-24 prove the PRODUCER half:
// that `SetView.eye[3]` reaches projector configuration addresses 19/20/21 in
// the right order, for the right view, once per record, and not at all for a
// refused `view_id`. This file proves the STORE half, which is the other end
// of the same traverse: that the block holding those addresses keeps each
// word where its reader expects it and leaves everything else alone.
//
// The three claims, and why each is here rather than assumed:
//
//   1. PER VIEW AND PER AXIS. Six registers, nine ways to mis-decode. A block
//      that indexed by `cfg_view_i` on the wrong arm, or that wrote x on the
//      y address, passes any test that writes one view with one value.
//
//   2. IT IGNORES EVERY OTHER ADDRESS. This block SNOOPS a bus it does not
//      own: addresses 0..15 are matrix words, 16/17 the viewport rect and 18
//      the depth profile, all of them `zhao_project_core`'s. A decode that
//      caught one of those would corrupt the eye every frame with a value that
//      is itself perfectly legal -- a camera at a matrix coefficient -- and no
//      counter anywhere would move. So the negative case is walked EXHAUSTIVELY
//      over the whole five-bit address space rather than sampled.
//
//   3. RESET IS THE ORIGIN, not whatever was in the flops. `spec/commands.zidl`
//      makes zero a legal eye (the world origin) rather than an absent one, and
//      that is a DECISION -- a console that never issues an eye measures LOD
//      distance from (0,0,0). Asserting it here is what stops the decision from
//      being quietly rewritten by a reset the block does not have.
//
// NOT PROVEN HERE: that the eye is the RIGHT camera. That is `zhao_terrain_lod`'s
// arithmetic and its own tests. This file is about carriage.
#include "Vzhao_view_eye.h"
#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using Dut = Vzhao_view_eye;

// Present one configuration write and retire it. `cfg_we_i` is a pulse the
// executor owns; the block has no handshake and takes a write the cycle it is
// offered, so one tick is the whole transaction.
void cfg_write(Dut& dut, uint8_t view, uint8_t addr, uint32_t data) {
  dut.cfg_we_i = 1;
  dut.cfg_view_i = view;
  dut.cfg_addr_i = addr;
  dut.cfg_data_i = data;
  zhao::tick(dut);
  dut.cfg_we_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  zhao::tick(dut);
}

int32_t eye(const Dut& dut, unsigned view, unsigned axis) {
  const uint32_t v[2][3] = {{dut.eye0_x_o, dut.eye0_y_o, dut.eye0_z_o},
                            {dut.eye1_x_o, dut.eye1_y_o, dut.eye1_z_o}};
  return static_cast<int32_t>(v[view][axis]);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  // Reset by hand, not `zhao::reset`: that helper drives `in_valid`/`in_data`,
  // which is the harness's byte-stream shape and not this block's. A snooping
  // configuration bank has neither.
  dut.rst_n = 0;
  dut.cfg_we_i = 0;
  dut.cfg_view_i = 0;
  dut.cfg_addr_i = 0;
  dut.cfg_data_i = 0;
  dut.eval();
  zhao::tick(dut);
  zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  // ---- 1. reset is the origin --------------------------------------------
  for (unsigned v = 0; v < 2; ++v)
    for (unsigned a = 0; a < 3; ++a)
      zhao::check(eye(dut, v, a) == 0, "reset: the eye is the world origin", 0,
                  static_cast<uint64_t>(eye(dut, v, a)));

  // ---- 2. six registers, six distinguishable values ----------------------
  //
  // NEGATIVE AND EXTREME ON PURPOSE. fx16 is signed and a camera below or west
  // of the origin is ordinary, so half of these are negative and two sit at the
  // ends of the range. A block that stored the word unsigned, or that sliced it
  // to fewer than 32 bits, passes on small positives and fails here.
  const int32_t want[2][3] = {
      {0x0012'3456, static_cast<int32_t>(0xFFF8'0000), 0x7FFF'FFFF},
      {static_cast<int32_t>(0x8000'0000), 0x0000'0001, static_cast<int32_t>(0xFFED'CBA9)}};
  for (unsigned v = 0; v < 2; ++v)
    for (unsigned a = 0; a < 3; ++a)
      cfg_write(dut, static_cast<uint8_t>(v), static_cast<uint8_t>(19 + a),
                static_cast<uint32_t>(want[v][a]));

  for (unsigned v = 0; v < 2; ++v)
    for (unsigned a = 0; a < 3; ++a)
      zhao::check(eye(dut, v, a) == want[v][a], "each word lands in its own register",
                  static_cast<uint64_t>(static_cast<uint32_t>(want[v][a])),
                  static_cast<uint64_t>(static_cast<uint32_t>(eye(dut, v, a))));

  // ---- 3. every OTHER address is somebody else's -------------------------
  //
  // Walk the whole five-bit space, both views, writing a value that would be
  // unmistakable if it leaked. Nothing may move.
  unsigned leaked = 0;
  for (unsigned v = 0; v < 2; ++v) {
    for (unsigned addr = 0; addr < 32; ++addr) {
      if (addr >= 19 && addr <= 21) continue;
      cfg_write(dut, static_cast<uint8_t>(v), static_cast<uint8_t>(addr), 0xDEAD'BEEFu);
      for (unsigned vv = 0; vv < 2; ++vv)
        for (unsigned a = 0; a < 3; ++a)
          if (eye(dut, vv, a) != want[vv][a]) ++leaked;
    }
  }
  zhao::check(leaked == 0, "writes to addresses 0..18 and 22..31 move no eye register", 0, leaked);

  // ---- 4. a write with `cfg_we_i` low changes nothing ---------------------
  //
  // The enable is the whole gate -- there is no ready, no address strobe and no
  // second qualifier -- so an arm that decoded the address without it would
  // latch every bus cycle. Offer a correct address and data with `we` low.
  dut.cfg_we_i = 0;
  dut.cfg_view_i = 0;
  dut.cfg_addr_i = 19;
  dut.cfg_data_i = 0x0BAD'0BADu;
  zhao::tick(dut);
  zhao::tick(dut);
  zhao::check(eye(dut, 0, 0) == want[0][0], "an offered word with `we` low is not taken",
              static_cast<uint64_t>(static_cast<uint32_t>(want[0][0])),
              static_cast<uint64_t>(static_cast<uint32_t>(eye(dut, 0, 0))));

  // ---- 5. last write wins, per register ----------------------------------
  //
  // The eye is idempotent view state: two SetViews in a packet commit once
  // (cmd_exec_directed case 23), but nothing stops two PACKETS, and the bank
  // must simply hold the newest. Rewrite one word and check the other five did
  // not follow it.
  cfg_write(dut, 1, 20, 0x0000'2222u);
  zhao::check(eye(dut, 1, 1) == 0x2222, "a rewritten word takes the new value", 0x2222,
              static_cast<uint64_t>(static_cast<uint32_t>(eye(dut, 1, 1))));
  unsigned disturbed = 0;
  for (unsigned v = 0; v < 2; ++v)
    for (unsigned a = 0; a < 3; ++a)
      if (!(v == 1 && a == 1) && eye(dut, v, a) != want[v][a]) ++disturbed;
  zhao::check(disturbed == 0, "and disturbs none of the other five", 0, disturbed);

  std::printf("  eye0 = (%d, %d, %d)  eye1 = (%d, %d, %d)\n", eye(dut, 0, 0), eye(dut, 0, 1),
              eye(dut, 0, 2), eye(dut, 1, 0), eye(dut, 1, 1), eye(dut, 1, 2));
  return zhao::report_and_exit("view_eye_directed");
}
