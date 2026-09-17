// shell_paired_diff_directed.cpp -- the Packet-H paired-traffic differential.
//
// THE CLAUSE is "unaffected behaviour matches under paired traffic". The
// structural half lives in `packet_h_sibling_diff`, which proves the sibling's
// carried-over instances have not drifted TEXTUALLY. Text is not behaviour, and
// that tool's own header says so. This is the behavioural half: one stimulus,
// both shells, and every output outside the changed path compared cycle for
// cycle.
//
// WHAT MAKES IT POSSIBLE is a decision taken when the sibling was seeded.
// `zhao_shell_top_v2` is a strict SUPERSET of `zhao_shell_top` -- not one port
// was removed, including seven `job_*` inputs the V2 pipe does not consume,
// which are kept and sunk rather than deleted. The sink's comment in the shell
// says exactly why: "a sibling whose port list has drifted cannot be driven by
// the same harness." This file is that cheque being cashed.
//
// WHAT IT DELIBERATELY DOES NOT CLAIM. Four outputs are declared divergent in
// tools/design/gen_shell_paired_diff.py -- the four the swapped bin pipe drives
// directly -- each with its reason. That list was NINETEEN names in its first
// draft, reasoned from the outside ("the render path changed, so the render
// counters must differ"), and reading the historical shell refuted most of it:
// `ring_wr_*` comes from the command scheduler, not the replaced slot manager,
// which drives no top-level output at all. Fifteen speculative exemptions would
// have been fifteen outputs never compared, in a tool whose entire value is the
// comparison -- and it would have passed.
//
// So: 91 outputs compared, 4 exempt. If a compared output diverges, the honest
// responses are to fix the shell or to add the name WITH a demonstrated reason.
// Quietly widening the list until it passes produces a differential that proves
// nothing and looks green doing it.
//
// AND ONE ASYMMETRY, stated rather than hidden. No phase below drives the
// render triangle path. The two shells do not consume it the same way: the
// historical shell's binner takes `render_tri_valid_i` and the geometric
// coefficients, while the V2 pipe takes the `tri_*` plane bundle, which is a
// SIBLING-ONLY input with no V1 counterpart. Driving `render_tri_valid_i` would
// feed one pipe and starve the other, so whatever came out would be a
// comparison of two different stimuli -- the mismatched-poses mistake from
// CLAUDE.md, in a testbench. What this differential covers is every domain
// Packet H claims not to have touched, under load, which is exactly what the
// clause says. The render path's own equivalence is not in scope and is not
// claimed anywhere.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_shell_paired_diff.h"

#include "zhao_sim.hpp"

using zhao::check;

namespace {

using Dut = Vzhao_shell_paired_diff;

int g_gpu_edges = 0;

// The shell's frozen ratio is vid = gpu/2 and audio is slower again. A domain
// that never ticks is a domain whose outputs trivially agree, which is the
// flattering way for this test to pass.
void tick(Dut& d) {
  d.gpu_clk = 0;
  d.eval();
  d.gpu_clk = 1;
  d.eval();
  ++g_gpu_edges;
  if ((g_gpu_edges & 1) == 0) {
    d.vid_clk = 0;
    d.eval();
    d.vid_clk = 1;
    d.eval();
  }
  if ((g_gpu_edges % 24) == 0) {
    d.audio_clk = 0;
    d.eval();
    d.audio_clk = 1;
    d.eval();
  }
}

void report(Dut& d, const char* phase) {
  if (!d.mismatch_sticky_o) return;
  std::printf(
      "[shell_paired_diff] DIVERGED during %s: first compared-output index %u, "
      "%u mismatching cycles. The index resolves to a port name in the comment "
      "table at the foot of tests/shell/zhao_shell_paired_diff.sv.\n",
      phase, d.mismatch_first_o, d.mismatch_count_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

#if defined(ZHAO_PAIRED_DIFF_MUTANT)
  // The same binary against the committed mutant harness. Its ctest carries
  // WILL_FAIL, so this run is EXPECTED to report a divergence -- printed here
  // so a reader of the log never has to guess which of the two builds produced
  // a given result.
  std::printf(
      "[shell_paired_diff] MUTANT BUILD: px_valid_o's V2 side is "
      "negated. A divergence here is the control working.\n");
#endif

  // Everything to a defined value before reset is released. An x on a shared
  // input propagates into both shells and makes them agree on garbage, which
  // is a pass this test must not be able to produce.
  dut.gpu_clk = 0;
  dut.vid_clk = 0;
  dut.audio_clk = 0;
  dut.rst_n = 0;
  for (int i = 0; i < 3; ++i) {
    dut.hps_state_i[i] = 0;
    dut.hps_byte_len_i[i] = 0;
  }
  dut.ring_wr_ready_i = 1;
  dut.hps_req_grant_i = 0;
  dut.hps_rd_valid_i = 0;
  dut.hps_rd_data_i = 0;
  dut.hps_rd_last_i = 0;
  dut.pad_present_i = 0;
  for (int i = 0; i < 4; ++i) {
    dut.pad_buttons_i[i] = 0;
    dut.pad_lx_i[i] = 0;
    dut.pad_ly_i[i] = 0;
    dut.pad_rx_i[i] = 0;
    dut.pad_ry_i[i] = 0;
  }
  dut.aud_wr_valid_i = 0;
  dut.aud_wr_l_i = 0;
  dut.aud_wr_r_i = 0;
  dut.cnt_snap_ready_i = 1;
  dut.render_frame_begin_i = 0;
  dut.render_frame_end_i = 0;
  dut.render_grid_w_i = 2;
  dut.render_grid_h_i = 2;
  dut.render_tri_valid_i = 0;
  // 103 bits of `zhao_guard_req_t`, which Verilator hands over as a VlWide<4>
  // rather than a scalar -- assigning 0 to it does not compile, which is the
  // friendly way for a wide port to tell you it is wide.
  for (int i = 0; i < 4; ++i) dut.geom_guard_req_i[i] = 0;
  dut.render_fb_base_i = 0;
  dut.render_fb_stride_i = 0;
  dut.fb_writer_i = 0;
  dut.phy_dq_i = 0;

  // The sibling-only inputs. They do not participate in the comparison -- V1
  // has no such ports -- but they must be defined or the V2 shell runs on x.
  dut.cfg_valid_i = 0;
  dut.cfg_op_i = 0;
  dut.cfg_page_generation_i = 0;
  dut.cfg_selector_i = 0;
  for (int i = 0; i < 3; ++i) dut.cfg_row_i[i] = 0;
  dut.cfg_crc32_i = 0;
  dut.cfg_rsp_ready_i = 1;
  dut.pal_load_valid_i = 0;
  dut.pal_load_op_i = 0;
  dut.pal_load_slot_i = 0;
  dut.pal_load_gen_i = 0;
  dut.pal_load_idx_i = 0;
  dut.pal_load_rgb565_i = 0;
  dut.pal_load_crc_ok_i = 0;
  dut.fill_req_ready_i = 1;
  dut.fill_data_valid_i = 0;
  dut.fill_data_i = 0;
  dut.fill_refused_i = 0;
  dut.sheet_req_ready_i = 1;
  dut.blank_cmd_i = 0;
  dut.scanout_ack_i = 0;
  dut.frame_swap_valid_i = 0;
  dut.frame_swap_slot_i = 0;

  dut.eval();
  for (int i = 0; i < 8; ++i) tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- PHASE A: quiet ------------------------------------------------------
  // Two shells doing nothing must agree. If this fails, nothing after it means
  // anything -- so it is checked separately rather than folded into the total.
  for (int i = 0; i < 400; ++i) tick(dut);
  report(dut, "the quiet phase");
  check(dut.mismatch_sticky_o == 0, "the two shells agree while idle", 0, dut.mismatch_sticky_o);

  // ---- PHASE B: audio and pads --------------------------------------------
  // Domains Packet H did not touch at all, driven hard. The audio path has its
  // own clock and its own occupancy accounting; the pad path has gap detection
  // that depends on WHEN samples arrive, not just what they are.
  for (int i = 0; i < 4000; ++i) {
    dut.aud_wr_valid_i = (i % 3) == 0;
    dut.aud_wr_l_i = static_cast<uint16_t>(i * 37);
    dut.aud_wr_r_i = static_cast<uint16_t>(0xFFFF - i * 11);
    dut.pad_present_i = static_cast<uint8_t>((i / 512) & 0xF);
    for (int p = 0; p < 4; ++p) {
      dut.pad_buttons_i[p] = static_cast<uint32_t>(i * (p + 1));
      dut.pad_lx_i[p] = static_cast<uint16_t>(i + p);
      dut.pad_ly_i[p] = static_cast<uint16_t>(i - p);
    }
    tick(dut);
  }
  dut.aud_wr_valid_i = 0;
  report(dut, "audio and pad traffic");
  check(dut.mismatch_sticky_o == 0, "the two shells agree under audio and pad traffic", 0,
        dut.mismatch_sticky_o);

  // ---- PHASE C: the HPS bridge --------------------------------------------
  // Grants and read returns, with backpressure toggling, so the arbiter and
  // the write FIFO both see stalls rather than an unobstructed run.
  for (int i = 0; i < 4000; ++i) {
    dut.hps_req_grant_i = (i % 7) != 0;
    dut.hps_rd_valid_i = (i % 5) == 0;
    dut.hps_rd_data_i = 0x0123456789ABCDEFull ^ static_cast<uint64_t>(i);
    dut.hps_rd_last_i = (i % 40) == 0;
    dut.ring_wr_ready_i = (i % 11) != 0;
    dut.cnt_snap_ready_i = (i % 13) != 0;
    for (int s = 0; s < 3; ++s) {
      dut.hps_state_i[s] = static_cast<uint8_t>((i >> (s * 2)) & 3u);
      dut.hps_byte_len_i[s] = static_cast<uint32_t>(64 + s * 16);
    }
    tick(dut);
  }
  dut.hps_rd_valid_i = 0;
  report(dut, "HPS bridge traffic");
  check(dut.mismatch_sticky_o == 0, "the two shells agree under HPS traffic", 0,
        dut.mismatch_sticky_o);

  // ---- PHASE D: scanout ----------------------------------------------------
  // The video front end, which Packet H changed the PRODUCER of (the swap
  // path) but not the scanout itself. This is the phase most likely to find
  // something, and it is the reason the divergent list is kept short.
  for (int i = 0; i < 8000; ++i) {
    dut.scanout_ack_i = (i % 3) != 0;
    dut.blank_cmd_i = (i > 6000 && i < 6200);
    tick(dut);
  }
  report(dut, "scanout traffic");
  check(dut.mismatch_sticky_o == 0, "the two shells agree under scanout traffic", 0,
        dut.mismatch_sticky_o);

  // ---- THE ACTIVITY WITNESS ------------------------------------------------
  // Everything above asserts that 91 outputs AGREED. Two shells producing
  // nothing agree perfectly, so without this the whole file passes on a stuck
  // clock, an unreleased reset, or a stimulus that never reaches anything --
  // and it passes loudly, with four green checks.
  //
  // The floor is deliberately a floor and not an exact number. Some compared
  // outputs cannot move under this stimulus at all: the SDRAM PHY pins need a
  // memory model, and the render counters need the triangle path this
  // differential explicitly does not drive. Pinning an exact count would make
  // the test fail whenever the shell legitimately gained an output, which is
  // how a coverage assertion gets deleted.
  std::printf(
      "[shell_paired_diff] %d gpu cycles, %u of 91 compared outputs toggled, "
      "%u mismatching cycles\n",
      g_gpu_edges, dut.toggled_count_o, dut.mismatch_count_o);
  check(dut.toggled_count_o >= 20,
        "at least 20 of the 91 compared outputs actually moved -- the shells "
        "were compared doing something, not compared doing nothing",
        1, dut.toggled_count_o >= 20);

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("shell_paired_diff_directed"));
}
