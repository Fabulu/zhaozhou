// cmd_exec_directed.cpp -- CMD.EXEC, driven through the real pair.
//
// The DUT is `tb_cmd_exec_pair`: CMD.DECODER and CMD.EXEC on one forked byte
// stream, so the verdict this test relies on is the one the shipped decoder
// actually produces, at the cycle it actually produces it. See that file's
// header for why the executor is not driven alone.
//
// THE LAYOUT IS A DIFFERENTIAL, not a shared constant. The RTL reads its byte
// offsets from `fpga/rtl/generated/zhao_abi_pkg.sv`; this test writes its
// records through `zhao_pack_set_view` / `zhao_pack_surface_stamp` in
// `runtime/include/zhao_abi.h`. Both come from the same .zidl and NEITHER is
// hand-written here, so a field that lands at the wrong offset on one side
// fails against the other. A test that used the RTL's own constants to build
// its stimulus would agree with any offset whatsoever.
//
// WHAT EACH CASE IS FOR. Every counter this block exposes is fired by name
// below, because CLAUDE.md is explicit that a counter asserted zero and never
// seen to move is a claim rather than evidence:
//
//   1  packets_committed_o, views_written_o, stamps_issued_o, unsupported_o
//      -- and THE ARCHITECTURAL ASSERTION: not one console write happens at or
//      before the verdict cycle.
//   2  packets_abandoned_o, on a real payload-CRC failure, with zero writes.
//   3  view_range_refused_o, on a view_id the two-view bank cannot address.
//   4  stamp_src_truncated_o, on a source_id whose high half is nonzero.
//   5  stamp_overflow_o, on a packet with STAMP_Q+1 stamps -- reachable with
//      LEGAL stimulus, so it needs no committed mutant.
//   6  the collapse law: two SetViews for one view commit ONCE; two views
//      commit twice.
//   7  the same packet as case 1 under stamp backpressure: identical results,
//      only slower.
//   8  the matrix bank refusing at every phase: 32 words, none lost, none
//      doubled, in order.
//   9  the DRAW arm: all six DrawForm fields plus the header source_id, read
//      back off the dispatch. `flags` is the record's LAST two bytes and is
//      the case that catches `df_flags_c`'s removal.
//  10  draw_overflow_o, on DRAW_Q+1 forms -- LEGAL stimulus, so no mutant --
//      and the poisoned packet is abandoned WHOLE.
//  11  draw_src_truncated_o, and the proof that the stamp counter stayed put.
//  12  VIEWS BEFORE DRAWS, measured: the first form leaves after the last
//      matrix word, under three draw-ready patterns, in submission order.
//  17  R25 SetEnvironment: two records, ONE environment out after the verdict,
//      the last record's four bank fields; envs_issued_o; not unsupported.
//  18  R25: an abandoned packet's environment never leaves.

#include "Vtb_cmd_exec_pair.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"
#include "zref/zref_post.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using zhao::check;

struct CfgWrite {
  uint32_t cycle;
  uint8_t view;
  uint8_t addr;
  uint32_t data;
};

struct StampOut {
  uint32_t cycle;
  uint32_t patch;
  uint8_t operation;
  uint8_t tag;
  uint16_t strength;
  int32_t tx;
  int32_t ty;
  int32_t radius;
  int32_t ring_width;
  uint16_t src_id;
};

struct DrawOut {
  uint32_t cycle;
  uint32_t form;
  uint32_t material_set;
  uint32_t transform;
  uint8_t viewport_mask;
  uint8_t semantic_weight;
  uint16_t flags;
  uint16_t src_id;
};

struct UploadOut {
  uint32_t cycle;
  uint32_t index;
  uint8_t kind;
  uint64_t hps;
  uint32_t vram, len, crc;
  uint16_t epoch, gen;
  uint8_t slot;
};

struct EnvOut {
  uint32_t cycle;
  uint16_t yaw, pitch, sun, ambient;
};

// R18/R33: one token load, as it left the executor.
struct TokOut {
  uint32_t cycle = 0;
  bool budget = false;  // true = the contract's ceiling, false = a view request
  uint32_t g0 = 0, g1 = 0, f0 = 0, f1 = 0, sh = 0;  // ceiling (budget)
  uint32_t view = 0, geom = 0, frag = 0;            // request
};

struct PvWrite {
  uint32_t cycle;
  uint8_t sel, addr;
  uint64_t lo64;
  uint8_t hi8;
};

// One TerrainField record as it left CMD.EXEC. `handle` is the handle32 the
// command carries and is deliberately NOT called a hash: FIELD.PROGCACHE keys
// its directory by CONTENT hash and nothing in hardware publishes
// {handle -> hash}, so naming this field `hash` here would bake the confusion
// entry I34 spends a paragraph refusing into the test as well.
struct TfldOut {
  uint32_t cycle = 0;
  int32_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;
  uint32_t handle = 0;
  uint16_t cmd = 0;
  uint32_t start_tick = 0;
  uint32_t duration = 0;
  uint32_t p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

struct Run {
  bool done = false;
  uint8_t err = 0;
  uint32_t commands = 0;
  uint32_t verdict_cycle = 0;
  std::vector<CfgWrite> cfg;       // matrix words, cfg addresses 0..15
  std::vector<CfgWrite> profiles;  // depth-profile writes, cfg address 18
  // R63: the eye, cfg addresses 19/20/21. Kept OUT of `cfg` for the same
  // reason the profile is: `cfg` means "a matrix word", and eleven checks
  // count its size against 16 and 32. Folding three more writes in would not
  // fail -- it would move those numbers to 19 and 38 and read as a bug in the
  // matrix walk.
  std::vector<CfgWrite> eyes;
  std::vector<StampOut> stamps;
  std::vector<DrawOut> draws;
  std::vector<UploadOut> uploads;
  std::vector<TokOut> toks;
  uint32_t contracts = 0;
  uint32_t uploads_issued = 0, upload_overflow = 0;
  uint32_t committed = 0, abandoned = 0, views = 0, issued = 0;
  uint32_t overflow = 0, refused = 0, truncated = 0, unsupported = 0;
  uint32_t draws_issued = 0, draw_overflow = 0, draw_truncated = 0;
  std::vector<EnvOut> envs;
  uint32_t envs_issued = 0;
  // R35/R36: the look as the run LEFT it, every table write, and the hold.
  std::vector<PvWrite> pv;
  zref::post::look::Look look;
  uint32_t looks_applied = 0, grade_written = 0, post_refused = 0, grade_overflow = 0;
  uint32_t look_busy_cycles = 0, look_busy_first = 0;
  // TerrainField 0x0200 (entry I34 build item (a)): every record that LEFT the
  // executor, with the cycle, plus the three counters.
  std::vector<TfldOut> tflds;
  uint32_t tflds_issued = 0, tfld_overflow = 0, tfld_truncated = 0;
};

/**
 * Stream one packet through the pair and collect everything that left the
 * executor, each tagged with the cycle it left on.
 *
 * `stamp_mask` gates SURFACE.STAMP's ready from a bit pattern. The decoder is
 * one-shot -- its S_DONE holds the verdict until reset -- so every packet gets
 * a fresh DUT, which is also what makes the counter assertions below absolute
 * rather than deltas.
 */
Run runPacket(const std::vector<uint8_t>& pkt, uint32_t stamp_mask,
              uint32_t cfg_mask = 0xFFFFFFFFu, uint32_t draw_mask = 0xFFFFFFFFu,
              uint32_t upl_mask = 0xFFFFFFFFu, uint32_t env_mask = 0xFFFFFFFFu,
              uint32_t idle_from = 0, uint32_t tfld_mask = 0xFFFFFFFFu) {
  Vtb_cmd_exec_pair dut;
  dut.rst_n = 0;
  dut.pkt_valid_i = 0;
  dut.pkt_byte_i = 0;
  dut.pkt_len_i = 0;
  dut.stamp_ready_i = 1;
  dut.proj_cfg_ready_i = 1;
  dut.draw_ready_i = 1;
  dut.upl_ready_i = 1;
  dut.env_ready_i = 1;
  dut.tfld_ready_i = 1;
  dut.post_idle_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  Run r;
  size_t i = 0;
  uint32_t cyc = 0;
  uint32_t drain = 0;
  // The commit drain is bounded: 32 matrix words plus two clocks per stamp,
  // times the worst stall pattern. 4,096 idle cycles after the verdict is
  // three orders over that, and a run that needs more is a hang, not a slow
  // test.
  const uint32_t kDrainCycles = 4096;
  const uint32_t kGuard = static_cast<uint32_t>(pkt.size()) * 8 + 8192;

  while (cyc < kGuard) {
    const bool have = (i < pkt.size());
    dut.pkt_valid_i = have ? 1 : 0;
    dut.pkt_byte_i = have ? pkt[i] : 0;
    dut.pkt_len_i = static_cast<uint32_t>(pkt.size());
    dut.stamp_ready_i = ((stamp_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    // The matrix bank's refusal: in the composer this is `!proj_cfg_we_i`,
    // the host cfg port taking the cycle. A word refused here must be
    // RE-PRESENTED unchanged, never skipped and never duplicated.
    dut.proj_cfg_ready_i = ((cfg_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    // The draw dispatch's consumer is the console boundary (entry I41), so it
    // can refuse for as long as it likes and every form must still arrive,
    // once, in submission order.
    dut.draw_ready_i = ((draw_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    // MEM.UPLOAD's ready: in the composition it is high only in its S_IDLE.
    dut.upl_ready_i = ((upl_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    dut.env_ready_i = ((env_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    // TERRAIN.PATCH's section 9.1 list intake can refuse -- its `fld_add_ready_o`
    // goes low while the patch is mid-issue -- so the arm must hold the record
    // and re-present it unchanged, never skip and never duplicate.
    dut.tfld_ready_i = ((tfld_mask >> (cyc & 31)) & 1u) ? 1 : 0;
    // The post lease: busy until idle_from, idle after (R36's door).
    dut.post_idle_i = (cyc >= idle_from) ? 1 : 0;
    dut.eval();

    if (dut.post_look_busy_o) {
      if (r.look_busy_cycles++ == 0) r.look_busy_first = cyc;
    }
    if (dut.post_pv_we_o) {
      PvWrite pw;
      pw.cycle = cyc;
      pw.sel = static_cast<uint8_t>(dut.post_pv_sel_o);
      pw.addr = static_cast<uint8_t>(dut.post_pv_addr_o);
      pw.lo64 = uint64_t(dut.post_pv_data_o[0]) | (uint64_t(dut.post_pv_data_o[1]) << 32);
      pw.hi8 = static_cast<uint8_t>(dut.post_pv_data_o[2] & 0xFFu);
      r.pv.push_back(pw);
    }

    const bool moved = have && dut.pkt_ready_o;
    // The stamp handshake is a pre-edge view, like every ready/valid pair in
    // this tree's benches.
    const bool stamp_fires = (dut.stamp_valid_o != 0) && (dut.stamp_ready_i != 0);
    StampOut s;
    if (stamp_fires) {
      s.cycle = cyc;
      s.patch = dut.stamp_patch_o;
      s.operation = static_cast<uint8_t>(dut.stamp_operation_o);
      s.tag = static_cast<uint8_t>(dut.stamp_tag_o);
      s.strength = static_cast<uint16_t>(dut.stamp_strength_o);
      s.tx = static_cast<int32_t>(dut.stamp_tx_o);
      s.ty = static_cast<int32_t>(dut.stamp_ty_o);
      s.radius = static_cast<int32_t>(dut.stamp_radius_o);
      s.ring_width = static_cast<int32_t>(dut.stamp_ring_width_o);
      s.src_id = static_cast<uint16_t>(dut.stamp_src_id_o);
    }

    const bool cfg_fires = (dut.proj_cfg_we_o != 0) && (dut.proj_cfg_ready_i != 0);
    CfgWrite w;
    if (cfg_fires) {
      w.cycle = cyc;
      w.view = static_cast<uint8_t>(dut.proj_cfg_view_o);
      w.addr = static_cast<uint8_t>(dut.proj_cfg_addr_o);
      w.data = dut.proj_cfg_data_o;
    }

    const bool draw_fires = (dut.draw_valid_o != 0) && (dut.draw_ready_i != 0);
    DrawOut d;
    if (draw_fires) {
      d.cycle = cyc;
      d.form = dut.draw_form_o;
      d.material_set = dut.draw_material_set_o;
      d.transform = dut.draw_transform_o;
      d.viewport_mask = static_cast<uint8_t>(dut.draw_viewport_mask_o);
      d.semantic_weight = static_cast<uint8_t>(dut.draw_semantic_weight_o);
      d.flags = static_cast<uint16_t>(dut.draw_flags_o);
      d.src_id = static_cast<uint16_t>(dut.draw_src_id_o);
    }

    const bool upl_fires = (dut.upl_valid_o != 0) && (dut.upl_ready_i != 0);
    UploadOut u{};
    if (upl_fires) {
      u.cycle = cyc;
      u.index = dut.upl_index_o;
      u.kind = static_cast<uint8_t>(dut.upl_kind_o);
      u.hps = dut.upl_hps_addr_o;
      u.vram = dut.upl_vram_addr_o;
      u.len = dut.upl_len_o;
      u.crc = dut.upl_crc_o;
      u.epoch = static_cast<uint16_t>(dut.upl_epoch_o);
      u.gen = static_cast<uint16_t>(dut.upl_new_gen_o);
      u.slot = static_cast<uint8_t>(dut.upl_dst_slot_o);
    }
    const bool env_fires = (dut.env_valid_o != 0) && (dut.env_ready_i != 0);
    EnvOut ev{cyc, static_cast<uint16_t>(dut.env_sun_yaw_o), static_cast<uint16_t>(dut.env_sun_pitch_o),
              static_cast<uint16_t>(dut.env_sun_colour_o), static_cast<uint16_t>(dut.env_ambient_o)};

    const bool tfld_fires = (dut.tfld_valid_o != 0) && (dut.tfld_ready_i != 0);
    TfldOut tf{};
    if (tfld_fires) {
      tf.cycle = cyc;
      tf.x0 = static_cast<int32_t>(dut.tfld_x0_o);
      tf.z0 = static_cast<int32_t>(dut.tfld_z0_o);
      tf.x1 = static_cast<int32_t>(dut.tfld_x1_o);
      tf.z1 = static_cast<int32_t>(dut.tfld_z1_o);
      tf.handle = dut.tfld_handle_o;
      tf.cmd = static_cast<uint16_t>(dut.tfld_cmd_o);
      tf.start_tick = dut.tfld_start_tick_o;
      tf.duration = dut.tfld_duration_o;
      // 256 bits arrives as eight 32-bit words, lane k in word k.
      for (int k = 0; k < 8; ++k) tf.p[k] = dut.tfld_params_o[k];
    }

    // The two token loads are one-cycle pulses with no ready: sampled pre-edge.
    if (dut.tok_budget_valid_o) {
      TokOut k;
      k.cycle = cyc;
      k.budget = true;
      k.g0 = dut.tok_budget_geom0_o;
      k.g1 = dut.tok_budget_geom1_o;
      k.f0 = dut.tok_budget_frag0_o;
      k.f1 = dut.tok_budget_frag1_o;
      k.sh = dut.tok_budget_shared_o;
      r.toks.push_back(k);
    }
    if (dut.tok_vreq_valid_o) {
      TokOut k;
      k.cycle = cyc;
      k.view = dut.tok_vreq_view_o;
      k.geom = dut.tok_vreq_geom_o;
      k.frag = dut.tok_vreq_frag_o;
      r.toks.push_back(k);
    }

    zhao::tick(dut);
    if (env_fires) r.envs.push_back(ev);
    if (moved) ++i;
    if (upl_fires) r.uploads.push_back(u);
    if (stamp_fires) r.stamps.push_back(s);
    // THE SEVENTEENTH STEP. Since ac4f293d every committed view also writes
    // SetView's depth profile to cfg address 18. This bench predates it and
    // counted every cfg write as a matrix word, so eleven checks read 17 and
    // 34 where they meant 16 and 32 -- red since that commit. The profile
    // write is recorded on its own and asserted in case 1, not dropped.
    if (cfg_fires) {
      if (w.addr == 18) r.profiles.push_back(w);
      else if (w.addr >= 19 && w.addr <= 21) r.eyes.push_back(w);
      else r.cfg.push_back(w);
    }
    if (draw_fires) r.draws.push_back(d);
    if (tfld_fires) r.tflds.push_back(tf);

    if (dut.decode_done_o && !r.done) {
      r.done = true;
      r.err = static_cast<uint8_t>(dut.decode_error_o);
      r.commands = dut.decode_commands_o;
      r.verdict_cycle = cyc;
    }

    ++cyc;
    if (r.done) {
      if (++drain >= kDrainCycles) break;
    }
  }

  r.committed = dut.packets_committed_o;
  r.abandoned = dut.packets_abandoned_o;
  r.views = dut.views_written_o;
  r.issued = dut.stamps_issued_o;
  r.overflow = dut.stamp_overflow_o;
  r.refused = dut.view_range_refused_o;
  r.truncated = dut.stamp_src_truncated_o;
  r.unsupported = dut.unsupported_o;
  r.tflds_issued = dut.tflds_issued_o;
  r.tfld_overflow = dut.tfld_overflow_o;
  r.tfld_truncated = dut.tfld_src_truncated_o;
  r.draws_issued = dut.draws_issued_o;
  r.draw_overflow = dut.draw_overflow_o;
  r.draw_truncated = dut.draw_src_truncated_o;
  r.uploads_issued = dut.uploads_issued_o;
  r.upload_overflow = dut.upload_overflow_o;
  r.envs_issued = dut.envs_issued_o;
  r.contracts = dut.contracts_applied_o;
  r.look.bloom_gain = static_cast<uint8_t>(dut.post_bloom_gain_o);
  r.look.grade_valid = dut.post_grade_valid_o != 0;
  r.look.echo_arm = dut.post_echo_arm_o != 0;
  // Nine-bit signed ports: sign-extend what Verilator hands over as unsigned.
  auto s9 = [](uint32_t v) { return static_cast<int16_t>((v & 0x100u) ? int(v & 0x1FFu) - 512 : int(v & 0x1FFu)); };
  r.look.bias_r = s9(dut.post_bias_r_o);
  r.look.bias_g = s9(dut.post_bias_g_o);
  r.look.bias_b = s9(dut.post_bias_b_o);
  r.look.flash = static_cast<uint16_t>(dut.post_flash_rgb_o);
  r.look.flash_amount = static_cast<uint8_t>(dut.post_flash_amt_o);
  r.look.ink = static_cast<uint16_t>(dut.post_ink_rgb_o);
  r.looks_applied = dut.post_looks_applied_o;
  r.grade_written = dut.grade_entries_written_o;
  r.post_refused = dut.post_refused_o;
  r.grade_overflow = dut.grade_overflow_o;
  return r;
}

// ---- record builders, all layout from the generated packers ---------------

std::vector<uint8_t> setViewRecord(uint8_t view_id, uint32_t source_id, int32_t first_word,
                                   uint32_t gtok = 0, uint32_t ftok = 0,
                                   int32_t eye_x = 0, int32_t eye_y = 0, int32_t eye_z = 0) {
  zhao_abi::ZhRecordSetView rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_VIEW;
  // FROM THE TYPE, NOT A LITERAL. This was `96` until ruling R63 grew the
  // record to 112, and the literal is why sixty-odd checks in this file went
  // red at once instead of one. `sizeof` is checked by a static_assert in the
  // generated header, so it cannot drift from the wire.
  rec.hdr.record_bytes = static_cast<uint16_t>(sizeof(zhao_abi::ZhRecordSetView));
  rec.hdr.source_id = source_id;
  rec.payload.view_id = view_id;
  rec.payload.viewport_id = 0;
  rec.payload.flags = 0;
  // Sixteen distinguishable words, written out BY NAME in declaration order.
  // Not a loop over a pointer into the struct: the point of this stimulus is
  // that word k carries the value k, so `m00` must be seen at cfg address 0.
  // If the RTL wrote them to the wrong address, in the wrong order, or
  // byte-swapped, the compare in case 1 says which.
  zhao_abi::ZhMat4fx& vp = rec.payload.view_projection;
  vp.m00 = first_word + 0;
  vp.m01 = first_word + 1;
  vp.m02 = first_word + 2;
  vp.m03 = first_word + 3;
  vp.m10 = first_word + 4;
  vp.m11 = first_word + 5;
  vp.m12 = first_word + 6;
  vp.m13 = first_word + 7;
  vp.m20 = first_word + 8;
  vp.m21 = first_word + 9;
  vp.m22 = first_word + 10;
  vp.m23 = first_word + 11;
  vp.m30 = first_word + 12;
  vp.m31 = first_word + 13;
  vp.m32 = first_word + 14;
  vp.m33 = first_word + 15;
  rec.payload.pixel_error = 0;
  rec.payload.geometry_tokens = gtok;
  rec.payload.fragment_tokens = ftok;
  // R63: the eye. Defaulted to the origin so every existing case in this file
  // keeps meaning exactly what it meant -- a zero eye is a legal eye, not an
  // absent one, and the three cfg writes happen either way.
  rec.payload.eye[0] = eye_x;
  rec.payload.eye[1] = eye_y;
  rec.payload.eye[2] = eye_z;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_view(rec, out);
  return out;
}

std::vector<uint8_t> surfaceStampRecord(uint32_t source_id, uint32_t patch, uint8_t op, uint8_t tag,
                                        uint16_t strength, int32_t tx, int32_t ty, int32_t radius,
                                        int32_t ring) {
  zhao_abi::ZhRecordSurfaceStamp rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SURFACE_STAMP;
  rec.hdr.record_bytes = 64;
  rec.hdr.source_id = source_id;
  rec.payload.brush = 0;
  rec.payload.patch = patch;
  rec.payload.operation = op;
  rec.payload.tag = tag;
  rec.payload.strength = strength;
  rec.payload.transform.tx = tx;
  rec.payload.transform.ty = ty;
  rec.payload.transform.r00 = 0x00010000;
  rec.payload.transform.r01 = 0;
  rec.payload.transform.r10 = 0;
  rec.payload.transform.r11 = 0x00010000;
  rec.payload.radius = radius;
  rec.payload.ring_width = ring;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_surface_stamp(rec, out);
  return out;
}

// TerrainField 0x0200. `p` is p0..p7 -- the eight Earth parameter lanes,
// Q16.16 LE, in declaration order (field-ir.md 7.1). The remaining 32 bytes of
// the 64-byte blob are the mandatory-zero tail and are LEFT ZERO here: the
// executor does not police that law (CMD.DECODER owns the verdict) and a test
// that filled them would be asserting a rule nobody implements.
std::vector<uint8_t> terrainFieldRecord(uint32_t source_id, uint32_t program, int32_t x0,
                                        int32_t z0, int32_t x1, int32_t z1, uint32_t start_tick,
                                        uint32_t duration_ticks, const uint32_t p[8]) {
  zhao_abi::ZhRecordTerrainField rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_TERRAIN_FIELD;
  rec.hdr.record_bytes = 112;
  rec.hdr.source_id = source_id;
  rec.payload.program = program;
  rec.payload.footprint.x0 = x0;
  rec.payload.footprint.y0 = z0;  // rectfx.y IS world Z on a terrain footprint
  rec.payload.footprint.x1 = x1;
  rec.payload.footprint.y1 = z1;
  rec.payload.start_tick = start_tick;
  rec.payload.duration_ticks = duration_ticks;
  for (int k = 0; k < 8; ++k) {
    rec.payload.parameters[4 * k + 0] = static_cast<uint8_t>(p[k] & 0xFFu);
    rec.payload.parameters[4 * k + 1] = static_cast<uint8_t>((p[k] >> 8) & 0xFFu);
    rec.payload.parameters[4 * k + 2] = static_cast<uint8_t>((p[k] >> 16) & 0xFFu);
    rec.payload.parameters[4 * k + 3] = static_cast<uint8_t>((p[k] >> 24) & 0xFFu);
  }
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_terrain_field(rec, out);
  return out;
}

std::vector<uint8_t> drawFormRecord(uint32_t source_id, uint32_t form, uint32_t material_set,
                                    uint32_t transform, uint8_t viewport_mask,
                                    uint8_t semantic_weight, uint16_t flags) {
  zhao_abi::ZhRecordDrawForm rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_DRAW_FORM;
  rec.hdr.record_bytes = 32;
  rec.hdr.source_id = source_id;
  rec.payload.form = form;
  rec.payload.material_set = material_set;
  rec.payload.transform = transform;
  rec.payload.viewport_mask = viewport_mask;
  rec.payload.semantic_weight = semantic_weight;
  rec.payload.flags = flags;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_draw_form(rec, out);
  return out;
}

// SetPresentationContract, the token CEILING (R18/R33), by the generated packer.
std::vector<uint8_t> contractRecord(uint32_t g0, uint32_t g1, uint32_t f0, uint32_t f1,
                                    uint32_t sh) {
  zhao_abi::ZhRecordSetPresentationContract rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;
  rec.hdr.record_bytes = 48;
  rec.hdr.source_id = 0x31u;
  rec.payload.mode = zhao_abi::VIDEO_DUO;
  rec.payload.view_count = 2;
  rec.payload.geometry_tokens[0] = g0;
  rec.payload.geometry_tokens[1] = g1;
  rec.payload.fragment_tokens[0] = f0;
  rec.payload.fragment_tokens[1] = f1;
  rec.payload.shared_tokens = sh;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_presentation_contract(rec, out);
  return out;
}

// PublishResource (R17), packed by the GENERATED packer like every record here.
std::vector<uint8_t> publishRecord(uint32_t k) {
  zhao_abi::ZhRecordPublishResource rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_PUBLISH_RESOURCE;
  rec.hdr.record_bytes = 48;
  rec.hdr.source_id = 0x90u + k;
  // handle32 {index:24, generation:8}, index HIGH (zcon::detail::handle32).
  rec.payload.resource = ((0x00ABC0u + k) << 8) | 0x2Au;
  rec.payload.hps_addr_lo = 0x3000'0000u + k * 0x1000u;
  rec.payload.hps_addr_hi = k;  // nonzero for k>0: carried, not narrowed
  rec.payload.vram_dst = 0x054D'F000u + k * 0x100u;
  rec.payload.length = 256u + 64u * k;
  rec.payload.crc32c = 0xC0FF'EE00u + k;
  rec.payload.new_generation = static_cast<uint16_t>(0x0102u + k);
  rec.payload.epoch = static_cast<uint16_t>(9u + k);
  rec.payload.dst_slot = static_cast<uint8_t>(5u + k);
  rec.payload.kind = static_cast<uint8_t>(11u + k);
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_publish_resource(rec, out);
  return out;
}

void checkUpload(const UploadOut& u, uint32_t k, const std::string& tag) {
  check(u.index == 0x00ABC0u + k, (tag + " index (handle[23:0])").c_str(), 0x00ABC0u + k, u.index);
  check(u.kind == 11u + k, (tag + " kind").c_str(), 11u + k, u.kind);
  check(u.hps == ((static_cast<uint64_t>(k) << 32) | (0x3000'0000u + k * 0x1000u)),
        (tag + " hps address, all 64 bits").c_str(), 0, 0);
  check(u.vram == 0x054D'F000u + k * 0x100u, (tag + " vram").c_str(), 0x054DF000u + k * 0x100u,
        u.vram);
  check(u.len == 256u + 64u * k, (tag + " length").c_str(), 256u + 64u * k, u.len);
  check(u.crc == 0xC0FF'EE00u + k, (tag + " crc32c").c_str(), 0xC0FFEE00u + k, u.crc);
  check(u.gen == 0x0102u + k, (tag + " new generation").c_str(), 0x0102u + k, u.gen);
  check(u.epoch == 9u + k, (tag + " epoch").c_str(), 9u + k, u.epoch);
  check(u.slot == 5u + k, (tag + " dst slot").c_str(), 5u + k, u.slot);
}

std::vector<uint8_t> setEnvironmentRecord(uint32_t source_id, uint16_t yaw, uint16_t pitch,
                                          uint16_t sun, uint16_t ambient) {
  zhao_abi::ZhRecordSetEnvironment rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_ENVIRONMENT;
  rec.hdr.record_bytes = 48;
  rec.hdr.source_id = source_id;
  rec.payload.sun_yaw = yaw;
  rec.payload.sun_pitch = pitch;
  rec.payload.sun_colour.bits = sun;
  rec.payload.ambient.bits = ambient;
  // tint / fog are carried by the record and are not the bank's: set them to
  // values that would show up if the arm read the wrong offsets.
  rec.payload.tint.bits = 0xA5A5u;
  rec.payload.tint_strength = 0x77u;
  rec.payload.fog = zhao_abi::FOG_LINEAR;
  rec.payload.fog_near = 0x00050000;
  rec.payload.fog_far = 0x00090000;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_environment(rec, out);
  return out;
}

// SetPost (R36), packed by the GENERATED packer.
std::vector<uint8_t> setPostRecord(uint8_t gain, uint8_t flags, uint8_t amt, int16_t br, int16_t bg,
                                   int16_t bb, uint16_t flash, uint16_t ink) {
  zhao_abi::ZhRecordSetPost rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_POST;
  rec.hdr.record_bytes = 32;
  rec.hdr.source_id = 0x77u;
  rec.payload.bloom_gain = gain;
  rec.payload.flags = flags;
  rec.payload.flash_amount = amt;
  rec.payload.bias_r = br;
  rec.payload.bias_g = bg;
  rec.payload.bias_b = bb;
  rec.payload.flash.bits = flash;
  rec.payload.ink.bits = ink;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_post(rec, out);
  return out;
}

// SetGradeTable (R36): entries from the zref EMITTER's product vectors.
std::vector<uint8_t> setGradeRecord(const zref::post::look::GradeRecord& g) {
  zhao_abi::ZhRecordSetGradeTable rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_GRADE_TABLE;
  rec.hdr.record_bytes = 96;
  rec.hdr.source_id = 0x78u;
  rec.payload.curve = g.curve;
  rec.payload.first = g.first;
  rec.payload.count = g.count;
  for (unsigned i = 0; i < 72; ++i) rec.payload.vectors[i] = g.vectors[i];
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_grade_table(rec, out);
  return out;
}

// A deterministic, non-trivial curve set and a matrix with negative and
// cross-channel terms, so a transpose or a sign-extension error changes bits.
struct GradeFixture {
  uint8_t r[32], g[64], b[32];
  int16_t m[9] = {int16_t(18000), int16_t(-1200), int16_t(700), int16_t(-900), int16_t(16384),
                  int16_t(2100), int16_t(300), int16_t(-2500), int16_t(19000)};
  std::vector<zref::post::look::GradeRecord> recs;
  GradeFixture() {
    for (unsigned i = 0; i < 32; ++i) r[i] = uint8_t((i * 255u) / 31u);
    for (unsigned i = 0; i < 64; ++i) g[i] = uint8_t(255u - (i * 255u) / 63u);
    for (unsigned i = 0; i < 32; ++i) b[i] = uint8_t((i * i * 255u) / (31u * 31u));
    zref::post::look::emit_grade_table(r, g, b, m, [&](const zref::post::look::GradeRecord& x) {
      recs.push_back(x);
    });
  }
};

// Check one RTL table write against entry k of a zref record.
void checkPv(const PvWrite& w, const zref::post::look::GradeRecord& g, unsigned k,
             const std::string& tag) {
  int32_t p[3];
  zref::post::look::grade_entry(g.vectors, k, p);
  uint64_t lo;
  uint8_t hi;
  zref::post::look::pack_pv(p, &lo, &hi);
  check(w.sel == g.curve && w.addr == g.first + k, (tag + " curve and entry").c_str(),
        g.curve * 64u + g.first + k, w.sel * 64u + w.addr);
  check(w.lo64 == lo && w.hi8 == hi, (tag + " product vector, all 72 bits").c_str(), 0,
        (w.lo64 == lo && w.hi8 == hi) ? 0 : 1);
}

// The stamp this test uses wherever the values themselves are not the point.
constexpr uint32_t kPatch = 0x0A0B0C0Du;
constexpr uint8_t kOp = 1;
constexpr uint8_t kTag = 0x5Au;
constexpr uint16_t kStrength = 0xBEEFu;
constexpr int32_t kTx = -0x00012345;
constexpr int32_t kTy = 0x00067890;
constexpr int32_t kRadius = 0x00028000;
constexpr int32_t kRing = 0x00008000;

/** Assert nothing left the executor at or before the verdict. */
void checkNothingEscapedEarly(const Run& r, const char* what) {
  uint32_t early_cfg = 0, early_stamp = 0;
  for (const CfgWrite& w : r.cfg)
    if (w.cycle <= r.verdict_cycle) ++early_cfg;
  for (const StampOut& s : r.stamps)
    if (s.cycle <= r.verdict_cycle) ++early_stamp;
  check(early_cfg == 0, (std::string(what) + ": no matrix write at or before the verdict").c_str(),
        0, early_cfg);
  check(early_stamp == 0, (std::string(what) + ": no stamp at or before the verdict").c_str(), 0,
        early_stamp);
  uint32_t early_draw = 0;
  for (const DrawOut& d : r.draws)
    if (d.cycle <= r.verdict_cycle) ++early_draw;
  check(early_draw == 0, (std::string(what) + ": no draw at or before the verdict").c_str(), 0,
        early_draw);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // ---- 1. one SetView + one SurfaceStamp, committed -----------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(0, 0x0000'0007u, 0x0011'0000));
    b.append_record(
        surfaceStampRecord(0x0000'0009u, kPatch, kOp, kTag, kStrength, kTx, kTy, kRadius, kRing));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.done, "case1: reached a verdict", 1, r.done ? 1 : 0);
    check(r.err == zhao_abi::ZH_ABI_OK, "case1: the packet is well formed", zhao_abi::ZH_ABI_OK,
          r.err);
    check(r.commands == 4, "case1: four records walked", 4, r.commands);

    checkNothingEscapedEarly(r, "case1");

    check(r.committed == 1, "case1: packets_committed_o", 1, r.committed);
    check(r.abandoned == 0, "case1: packets_abandoned_o", 0, r.abandoned);
    check(r.views == 1, "case1: views_written_o", 1, r.views);
    check(r.issued == 1, "case1: stamps_issued_o", 1, r.issued);
    // BeginFrame and EndFrame are ABI records this block has no arm for.
    check(r.unsupported == 2, "case1: unsupported_o counts BeginFrame + EndFrame", 2,
          r.unsupported);
    check(r.overflow == 0, "case1: stamp_overflow_o", 0, r.overflow);
    check(r.refused == 0, "case1: view_range_refused_o", 0, r.refused);
    check(r.truncated == 0, "case1: stamp_src_truncated_o", 0, r.truncated);

    // THE PAYLOAD. Sixteen words, in address order, with the values the packer
    // put on the wire.
    check(r.cfg.size() == 16, "case1: sixteen matrix words written", 16, r.cfg.size());
    if (r.cfg.size() == 16) {
      for (uint32_t k = 0; k < 16; ++k) {
        const CfgWrite& w = r.cfg[k];
        const std::string tag = "case1: matrix word " + std::to_string(k);
        check(w.view == 0, (tag + " view").c_str(), 0, w.view);
        check(w.addr == k, (tag + " address").c_str(), k, w.addr);
        check(w.data == static_cast<uint32_t>(0x0011'0000 + static_cast<int32_t>(k)),
              (tag + " data").c_str(), static_cast<uint32_t>(0x0011'0000 + k), w.data);
      }
    }

    check(r.profiles.size() == 1, "case1: one depth-profile write (cfg 18)", 1, r.profiles.size());
    if (r.profiles.size() == 1)
      check(r.profiles[0].data == 0 && r.profiles[0].view == 0,
            "case1: profile WORLD_LONG (flags 0) for view 0", 0, r.profiles[0].data);
    check(r.stamps.size() == 1, "case1: one stamp dispatched", 1, r.stamps.size());
    if (r.stamps.size() == 1) {
      const StampOut& s = r.stamps[0];
      check(s.patch == kPatch, "case1: stamp patch handle", kPatch, s.patch);
      check(s.operation == kOp, "case1: stamp operation", kOp, s.operation);
      check(s.tag == kTag, "case1: stamp tag", kTag, s.tag);
      check(s.strength == kStrength, "case1: stamp strength", kStrength, s.strength);
      check(s.tx == kTx, "case1: stamp transform.tx", static_cast<uint32_t>(kTx),
            static_cast<uint32_t>(s.tx));
      check(s.ty == kTy, "case1: stamp transform.ty", static_cast<uint32_t>(kTy),
            static_cast<uint32_t>(s.ty));
      check(s.radius == kRadius, "case1: stamp radius", static_cast<uint32_t>(kRadius),
            static_cast<uint32_t>(s.radius));
      check(s.ring_width == kRing, "case1: stamp ring_width", static_cast<uint32_t>(kRing),
            static_cast<uint32_t>(s.ring_width));
      check(s.src_id == 9, "case1: stamp source_id low half", 9, s.src_id);
    }
  }

  // ---- 2. R18/R33: the token CEILING, then each view's REQUEST ----------
  // Counts, field for field off the generated packers; the ceiling leaves
  // FIRST, then view 0's request, then view 1's, all before the first matrix
  // word -- so a request always meets its own packet's ceiling. The contract is
  // an executed record now, so unsupported_o counts BeginFrame + EndFrame only.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(contractRecord(40000u, 30000u, 90000u, 80000u, 5000u));
    b.append_record(setViewRecord(1, 0x0000'0008u, 0x0022'0000, 0xFFFF'FFF0u, 20000u));
    b.append_record(setViewRecord(0, 0x0000'0007u, 0x0011'0000, 12345u, 67890u));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.done && r.err == zhao_abi::ZH_ABI_OK, "case2: well formed", 1, r.done ? 1 : 0);
    check(r.committed == 1, "case2: committed", 1, r.committed);
    check(r.contracts == 1, "case2: contracts_applied_o", 1, r.contracts);
    check(r.unsupported == 2, "case2: the contract is no longer 'unsupported'", 2, r.unsupported);
    check(r.toks.size() == 3, "case2: one ceiling and two requests left", 3, r.toks.size());
    if (r.toks.size() == 3) {
      const TokOut& c = r.toks[0];
      check(c.budget, "case2: the CEILING leaves first", 1, c.budget ? 1 : 0);
      check(c.g0 == 40000u && c.g1 == 30000u && c.f0 == 90000u && c.f1 == 80000u && c.sh == 5000u,
            "case2: the five counts, as sent", 1,
            c.g0 == 40000u && c.g1 == 30000u && c.f0 == 90000u && c.f1 == 80000u && c.sh == 5000u);
      check(!r.toks[1].budget && r.toks[1].view == 0 && r.toks[1].geom == 12345u &&
                r.toks[1].frag == 67890u,
            "case2: view 0's request, as sent", 1,
            !r.toks[1].budget && r.toks[1].view == 0 && r.toks[1].geom == 12345u &&
                r.toks[1].frag == 67890u);
      // view 1 asked for MORE than its ceiling: the executor forwards it as sent
      // -- the CLAMP is MEASURE.TOKENS', one authority per level (R18).
      check(!r.toks[2].budget && r.toks[2].view == 1 && r.toks[2].geom == 0xFFFF'FFF0u &&
                r.toks[2].frag == 20000u,
            "case2: view 1's request, as sent (the clamp is the guard's)", 1,
            !r.toks[2].budget && r.toks[2].view == 1 && r.toks[2].geom == 0xFFFF'FFF0u &&
                r.toks[2].frag == 20000u);
      check(r.toks[0].cycle < r.toks[1].cycle && r.toks[1].cycle < r.toks[2].cycle,
            "case2: ceiling, then view 0, then view 1", 1, 1);
      const uint32_t first_cfg = r.cfg.empty() ? 0xFFFFFFFFu : r.cfg[0].cycle;
      check(r.toks[2].cycle < first_cfg, "case2: every token load precedes the first matrix word",
            1, r.toks[2].cycle < first_cfg ? 1 : 0);
      check(r.toks[0].cycle > r.verdict_cycle, "case2: nothing leaves before the verdict", 1,
            r.toks[0].cycle > r.verdict_cycle ? 1 : 0);
    }
  }

  // ---- 2b. a packet that FAILS its CRC loads NO tokens --------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(contractRecord(1u, 2u, 3u, 4u, 5u));
    b.append_record(setViewRecord(0, 0x0000'0007u, 0x0011'0000, 6u, 7u));
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    p[36 + 32 + 20] = static_cast<uint8_t>(p[36 + 32 + 20] ^ 0xFFu);  // a contract count byte
    const Run r = runPacket(p, 0xFFFFFFFFu);
    check(r.abandoned == 1, "case2b: abandoned", 1, r.abandoned);
    check(r.toks.empty() && r.contracts == 0, "case2b: no ceiling and no request escaped", 0,
          r.toks.size() + r.contracts);
  }

  // ---- 3. the same packet with one payload byte flipped -------------------
  // The positive control for packets_abandoned_o, and the whole reason this
  // block stages instead of acting.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(0, 0x0000'0007u, 0x0011'0000));
    b.append_record(
        surfaceStampRecord(0x0000'0009u, kPatch, kOp, kTag, kStrength, kTx, kTy, kRadius, kRing));
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    // A matrix byte, well inside the record stream: the header CRC still
    // passes, so the decoder reaches ZH_ABI_BAD_PAYLOAD_CRC on the last byte
    // -- after the executor has already staged the whole SetView.
    p[36 + 32 + 24] = static_cast<uint8_t>(p[36 + 32 + 24] ^ 0xFFu);
    const Run r = runPacket(p, 0xFFFFFFFFu);

    check(r.done, "case3: reached a verdict", 1, r.done ? 1 : 0);
    check(r.err == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, "case3: the payload CRC fails",
          zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, r.err);
    check(r.abandoned == 1, "case3: packets_abandoned_o fires", 1, r.abandoned);
    check(r.committed == 0, "case3: packets_committed_o", 0, r.committed);
    // The assertion this block exists for: a staged SetView and a staged stamp
    // BOTH evaporate, and the surface sheet is never scarred by a packet that
    // failed its CRC.
    check(r.cfg.empty(), "case3: NO matrix word was written", 0, r.cfg.size());
    check(r.stamps.empty(), "case3: NO stamp was dispatched", 0, r.stamps.size());
    check(r.views == 0, "case3: views_written_o", 0, r.views);
    check(r.issued == 0, "case3: stamps_issued_o", 0, r.issued);
  }

  // ---- 4. a view_id the bank cannot address -------------------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(7, 0x0000'0007u, 0x0022'0000));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case4: the RECORD is legal ABI", zhao_abi::ZH_ABI_OK,
          r.err);
    check(r.refused == 1, "case4: view_range_refused_o fires", 1, r.refused);
    // Refused, not masked. view_id 7 must NOT land in view 1.
    check(r.cfg.empty(), "case4: nothing was written to either view", 0, r.cfg.size());
    check(r.views == 0, "case4: views_written_o", 0, r.views);
    check(r.committed == 1, "case4: the packet still commits", 1, r.committed);
  }

  // ---- 5. a source_id whose high half does not fit ------------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(
        surfaceStampRecord(0x0001'0005u, kPatch, kOp, kTag, kStrength, kTx, kTy, kRadius, kRing));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case5: well formed", zhao_abi::ZH_ABI_OK, r.err);
    check(r.truncated == 1, "case5: stamp_src_truncated_o fires", 1, r.truncated);
    check(r.stamps.size() == 1, "case5: the stamp is still dispatched", 1, r.stamps.size());
    if (r.stamps.size() == 1)
      check(r.stamps[0].src_id == 5, "case5: the low half is what is carried", 5,
            r.stamps[0].src_id);
  }

  // ---- 6. more stamps than the ring holds ---------------------------------
  // STAMP_Q is 8 in the harness; nine stamps is a legal packet the console
  // cannot execute, and it is refused WHOLE rather than eight-ninths applied.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    for (int k = 0; k < 9; ++k) {
      b.append_record(surfaceStampRecord(static_cast<uint32_t>(k), kPatch,
                                         static_cast<uint8_t>(k & 1), kTag, kStrength, kTx, kTy,
                                         kRadius, kRing));
    }
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case6: the PACKET is well formed", zhao_abi::ZH_ABI_OK,
          r.err);
    check(r.overflow == 1, "case6: stamp_overflow_o fires", 1, r.overflow);
    check(r.abandoned == 1, "case6: the packet is refused whole", 1, r.abandoned);
    check(r.committed == 0, "case6: packets_committed_o", 0, r.committed);
    check(r.stamps.empty(), "case6: not one stamp escaped", 0, r.stamps.size());
    check(r.issued == 0, "case6: stamps_issued_o", 0, r.issued);
  }

  // ---- 7. the collapse law -------------------------------------------------
  {
    // Two SetViews for the SAME view: idempotent state, last one wins, ONE
    // commit. This is the property that makes the shadow bank bounded.
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(0, 1, 0x0033'0000));
    b.append_record(setViewRecord(0, 2, 0x0044'0000));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.views == 1, "case6a: two SetViews for one view commit once", 1, r.views);
    check(r.cfg.size() == 16, "case6a: sixteen words, not thirty-two", 16, r.cfg.size());
    if (r.cfg.size() == 16)
      check(r.cfg[0].data == 0x0044'0000u, "case6a: the SECOND SetView is the one that lands",
            0x0044'0000u, r.cfg[0].data);
  }
  {
    // Two DIFFERENT views: two commits, thirty-two words, view 0 first.
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(1, 1, 0x0055'0000));
    b.append_record(setViewRecord(0, 2, 0x0066'0000));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.views == 2, "case7b: both views commit", 2, r.views);
    check(r.cfg.size() == 32, "case7b: thirty-two words", 32, r.cfg.size());
    if (r.cfg.size() == 32) {
      check(r.cfg[0].view == 0, "case7b: view 0 drains first", 0, r.cfg[0].view);
      check(r.cfg[0].data == 0x0066'0000u, "case7b: view 0 data", 0x0066'0000u, r.cfg[0].data);
      check(r.cfg[16].view == 1, "case7b: view 1 drains second", 1, r.cfg[16].view);
      check(r.cfg[16].data == 0x0055'0000u, "case7b: view 1 data", 0x0055'0000u, r.cfg[16].data);
    }
  }

  // ---- 8. the same work under stamp backpressure --------------------------
  // The result must be identical, only slower. A drain that drops a stamp when
  // its consumer is slow is the defect this case exists to catch.
  {
    const uint32_t masks[2] = {0xAAAAAAAAu, 0x11111111u};
    for (int m = 0; m < 2; ++m) {
      zhao::ZhaoFrameBuilder b;
      b.begin_frame(1, 0, 0, 0);
      b.append_record(setViewRecord(0, 7, 0x0011'0000));
      for (int k = 0; k < 4; ++k) {
        b.append_record(surfaceStampRecord(static_cast<uint32_t>(0x20 + k), kPatch,
                                           static_cast<uint8_t>(k & 1), kTag, kStrength, kTx, kTy,
                                           kRadius, kRing));
      }
      b.end_frame(0);
      const Run r = runPacket(b.seal(1, 1, 0), masks[m]);
      const std::string tag = "case8[mask " + std::to_string(m) + "]";

      check(r.err == zhao_abi::ZH_ABI_OK, (tag + ": well formed").c_str(), zhao_abi::ZH_ABI_OK,
            r.err);
      checkNothingEscapedEarly(r, tag.c_str());
      check(r.committed == 1, (tag + ": committed").c_str(), 1, r.committed);
      check(r.views == 1, (tag + ": views_written_o").c_str(), 1, r.views);
      check(r.issued == 4, (tag + ": all four stamps issued").c_str(), 4, r.issued);
      check(r.stamps.size() == 4, (tag + ": all four stamps observed").c_str(), 4, r.stamps.size());
      check(r.cfg.size() == 16, (tag + ": sixteen matrix words").c_str(), 16, r.cfg.size());
      // In order, and each carrying its own source id -- a drain that reissued
      // the head would show the same id twice, which is the shape of the
      // re-submission defect CLAUDE.md records under "Counters see what
      // pictures cannot".
      for (size_t k = 0; k < r.stamps.size() && k < 4; ++k) {
        check(r.stamps[k].src_id == 0x20 + k,
              (tag + ": stamp " + std::to_string(k) + " source id").c_str(), 0x20 + k,
              r.stamps[k].src_id);
      }
    }
  }

  // ---- 9. the matrix bank refuses, and refuses hard -----------------------
  // In the composed core the host cfg port owns addresses 16 and 17 and wins
  // any cycle it wants, so CMD.EXEC's write can be refused at any point in the
  // drain. Every word must still land, ONCE, in order. A drain that skipped a
  // refused word would leave one matrix coefficient stale -- a camera that is
  // subtly wrong, with every counter still reading right.
  {
    const uint32_t cfg_masks[3] = {0xAAAAAAAAu, 0x11111111u, 0x00000001u};
    for (int m = 0; m < 3; ++m) {
      zhao::ZhaoFrameBuilder b;
      b.begin_frame(1, 0, 0, 0);
      b.append_record(setViewRecord(1, 3, 0x0077'0000));
      b.append_record(setViewRecord(0, 4, 0x0088'0000));
      b.append_record(
          surfaceStampRecord(0x31u, kPatch, kOp, kTag, kStrength, kTx, kTy, kRadius, kRing));
      b.end_frame(0);
      const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, cfg_masks[m]);
      const std::string tag = "case9[cfg mask " + std::to_string(m) + "]";

      check(r.err == zhao_abi::ZH_ABI_OK, (tag + ": well formed").c_str(), zhao_abi::ZH_ABI_OK,
            r.err);
      checkNothingEscapedEarly(r, tag.c_str());
      check(r.committed == 1, (tag + ": committed").c_str(), 1, r.committed);
      check(r.views == 2, (tag + ": both views").c_str(), 2, r.views);
      check(r.cfg.size() == 32, (tag + ": thirty-two words, none lost, none doubled").c_str(), 32,
            r.cfg.size());
      if (r.cfg.size() == 32) {
        for (uint32_t k = 0; k < 32; ++k) {
          const uint32_t base = (k < 16) ? 0x0088'0000u : 0x0077'0000u;
          const uint8_t view = (k < 16) ? 0 : 1;
          check(r.cfg[k].view == view, (tag + ": word " + std::to_string(k) + " view").c_str(),
                view, r.cfg[k].view);
          check(r.cfg[k].addr == (k & 15u), (tag + ": word " + std::to_string(k) + " addr").c_str(),
                k & 15u, r.cfg[k].addr);
          check(r.cfg[k].data == base + (k & 15u),
                (tag + ": word " + std::to_string(k) + " data").c_str(), base + (k & 15u),
                r.cfg[k].data);
        }
      }
      // The stamp drain sits behind the cfg drain, so a bank that refuses for
      // a long time must not lose the stamp queued behind it.
      check(r.issued == 1, (tag + ": the stamp behind the drain still lands").c_str(), 1, r.issued);
    }
  }

  // ---- 10. one DrawForm, whole -------------------------------------------
  // Every one of DrawForm's six fields plus the header's source_id, packed by
  // `zhao_pack_draw_form` and read back off the dispatch port. The values are
  // deliberately all-different and byte-asymmetric: a transposed field, a
  // byte-swapped handle or an offset one out fails on the value and names the
  // field, rather than producing a plausible draw nobody can distinguish.
  //
  // `flags` IS THE INTERESTING ONE and 0xBEEF is chosen for it. It occupies
  // record bytes 30..31 of a 32-byte record, so its HIGH byte arrives on the
  // very cycle the record completes and the ring write happens. Without
  // `df_flags_c`'s bypass the ring captures the register one shift early and
  // this reads 0xEFxx. That is the whole reason the bypass exists, and this is
  // the case that would catch its removal.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(drawFormRecord(0x0000'0021u, 0x1234'56A7u, 0x89AB'CD03u, 0x0F1E'2D5Cu, 0x03u,
                                   0x77u, 0xBEEFu));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case10: the packet is well formed", zhao_abi::ZH_ABI_OK,
          r.err);
    checkNothingEscapedEarly(r, "case10");
    check(r.committed == 1, "case10: packets_committed_o", 1, r.committed);
    check(r.draws_issued == 1, "case10: draws_issued_o", 1, r.draws_issued);
    check(r.draws.size() == 1, "case10: exactly one dispatch left the block", 1, r.draws.size());
    check(r.draw_overflow == 0, "case10: draw_overflow_o", 0, r.draw_overflow);
    check(r.draw_truncated == 0, "case10: draw_src_truncated_o", 0, r.draw_truncated);
    // DrawForm now has an arm, so it must NOT be counted as unsupported.
    check(r.unsupported == 2, "case10: unsupported_o is BeginFrame + EndFrame only", 2,
          r.unsupported);
    if (r.draws.size() == 1) {
      const DrawOut& d = r.draws[0];
      check(d.form == 0x1234'56A7u, "case10: form handle", 0x1234'56A7u, d.form);
      check(d.material_set == 0x89AB'CD03u, "case10: material_set handle", 0x89AB'CD03u,
            d.material_set);
      check(d.transform == 0x0F1E'2D5Cu, "case10: transform handle", 0x0F1E'2D5Cu, d.transform);
      check(d.viewport_mask == 0x03u, "case10: viewport_mask", 0x03u, d.viewport_mask);
      check(d.semantic_weight == 0x77u, "case10: semantic_weight", 0x77u, d.semantic_weight);
      check(d.flags == 0xBEEFu, "case10: flags, the record's LAST two bytes", 0xBEEFu, d.flags);
      check(d.src_id == 0x0021u, "case10: the record header's source_id", 0x0021u, d.src_id);
    }
  }

  // ---- 11. the draw ring overflows, and the packet is refused WHOLE -------
  // DRAW_Q is 4 in the bench, so five forms in one packet is LEGAL stimulus
  // that reaches the guard. The counter fires without a committed mutant, and
  // the packet is abandoned rather than half-drawn: a frame with four of its
  // five creatures in it is a wrong frame, not a degraded one.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(0, 0x40u, 0x0099'0000));
    for (uint32_t k = 0; k < 5; ++k)
      b.append_record(drawFormRecord(0x50u + k, 0x1000u + k, 0x2000u + k, 0x3000u + k, 1,
                                     static_cast<uint8_t>(k), static_cast<uint16_t>(0x0100u + k)));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case11: the PACKET is well formed -- the refusal is ours",
          zhao_abi::ZH_ABI_OK, r.err);
    check(r.draw_overflow == 1, "case11: draw_overflow_o FIRED", 1, r.draw_overflow);
    check(r.abandoned == 1, "case11: the poisoned packet is abandoned", 1, r.abandoned);
    check(r.committed == 0, "case11: nothing was committed", 0, r.committed);
    check(r.draws.empty(), "case11: not one form was dispatched", 0, r.draws.size());
    check(r.cfg.empty(), "case11: and the view did not commit either", 0, r.cfg.size());
  }

  // ---- 12. a source_id whose high half cannot fit ------------------------
  // The SurfaceStamp narrowing rule, applied to the draw arm and counted on
  // its OWN counter: one flag for both records would attribute a form's
  // truncated id to a stamp.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(drawFormRecord(0x00CD'1234u, 0xAAAAu, 0xBBBBu, 0xCCCCu, 2, 9, 0x0007u));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.draw_truncated == 1, "case12: draw_src_truncated_o FIRED", 1, r.draw_truncated);
    check(r.truncated == 0, "case12: and the STAMP counter did not move", 0, r.truncated);
    check(r.draws_issued == 1, "case12: the draw still dispatches", 1, r.draws_issued);
    if (r.draws.size() == 1)
      check(r.draws[0].src_id == 0x1234u, "case12: the low half is carried", 0x1234u,
            r.draws[0].src_id);
  }

  // ---- 13. VIEWS BEFORE DRAWS, under backpressure ------------------------
  // The ordering claim the header makes structurally, measured rather than
  // argued: every matrix word of this packet retires BEFORE the first form
  // leaves. A draw that overtook its own packet's SetView would render the
  // first frame of every camera move through the previous camera -- a wrong
  // picture with every counter balancing, which no output check can see.
  //
  // Also the submission ORDER: draws are events and do not collapse, so three
  // forms must arrive as three forms, in the order the packet wrote them, and
  // a hostile ready pattern must not reorder or lose one.
  {
    const uint32_t draw_masks[3] = {0xFFFFFFFFu, 0x55555555u, 0x00000003u};
    for (int m = 0; m < 3; ++m) {
      zhao::ZhaoFrameBuilder b;
      b.begin_frame(1, 0, 0, 0);
      b.append_record(setViewRecord(0, 0x60u, 0x00AA'0000));
      b.append_record(setViewRecord(1, 0x61u, 0x00BB'0000));
      for (uint32_t k = 0; k < 3; ++k)
        b.append_record(drawFormRecord(0x70u + k, 0xF00Du + k, 0xBEA7u + k, 0xC0DEu + k,
                                       static_cast<uint8_t>(1 + k), static_cast<uint8_t>(k),
                                       static_cast<uint16_t>(0xAB00u + k)));
      b.end_frame(0);
      const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, draw_masks[m]);
      const std::string tag = "case13[draw mask " + std::to_string(m) + "]";

      check(r.err == zhao_abi::ZH_ABI_OK, (tag + ": well formed").c_str(), zhao_abi::ZH_ABI_OK,
            r.err);
      checkNothingEscapedEarly(r, tag.c_str());
      check(r.committed == 1, (tag + ": committed").c_str(), 1, r.committed);
      check(r.cfg.size() == 32, (tag + ": both views' words").c_str(), 32, r.cfg.size());
      check(r.draws.size() == 3, (tag + ": three forms, none lost, none doubled").c_str(), 3,
            r.draws.size());
      check(r.draws_issued == 3, (tag + ": draws_issued_o").c_str(), 3, r.draws_issued);

      if (r.cfg.size() == 32 && r.draws.size() == 3) {
        const uint32_t last_cfg = r.cfg[31].cycle;
        check(r.draws[0].cycle > last_cfg,
              (tag + ": the first form leaves AFTER the last matrix word").c_str(), 1,
              r.draws[0].cycle > last_cfg ? 1 : 0);
        for (uint32_t k = 0; k < 3; ++k) {
          check(r.draws[k].form == 0xF00Du + k,
                (tag + ": form " + std::to_string(k) + " in submission order").c_str(), 0xF00Du + k,
                r.draws[k].form);
          check(r.draws[k].flags == 0xAB00u + k,
                (tag + ": form " + std::to_string(k) + " flags").c_str(), 0xAB00u + k,
                r.draws[k].flags);
          check(r.draws[k].src_id == 0x70u + k,
                (tag + ": form " + std::to_string(k) + " source_id").c_str(), 0x70u + k,
                r.draws[k].src_id);
        }
      }
    }
  }

  // ---- 14. PublishResource (R17): two uploads and a form, committed --------
  // Every MEM.UPLOAD request field off the generated offsets, in submission
  // order, AFTER the verdict, and BEFORE the form (EX_UPL precedes EX_DRAW).
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishRecord(0));
    b.append_record(drawFormRecord(0x70u, 0xF00Du, 0xBEA7u, 0xC0DEu, 1, 0, 0));
    b.append_record(publishRecord(1));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_OK, "case14: well formed", zhao_abi::ZH_ABI_OK, r.err);
    checkNothingEscapedEarly(r, "case14");
    check(r.committed == 1, "case14: committed", 1, r.committed);
    check(r.uploads.size() == 2, "case14: two uploads handed to MEM.UPLOAD", 2, r.uploads.size());
    check(r.uploads_issued == 2, "case14: uploads_issued_o", 2, r.uploads_issued);
    check(r.unsupported == 2, "case14: only BeginFrame/EndFrame unsupported now", 2, r.unsupported);
    if (r.uploads.size() == 2) {
      checkUpload(r.uploads[0], 0, "case14 upload 0");
      checkUpload(r.uploads[1], 1, "case14 upload 1");
      check(r.uploads[0].cycle > r.verdict_cycle, "case14: no upload before the verdict", 1,
            r.uploads[0].cycle > r.verdict_cycle ? 1 : 0);
      if (r.draws.size() == 1)
        check(r.uploads[0].cycle < r.draws[0].cycle, "case14: the upload starts before the form", 1,
              r.uploads[0].cycle < r.draws[0].cycle ? 1 : 0);
    }
  }

  // ---- 15. five uploads against UPL_Q = 4: refused WHOLE, counted ----------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    for (uint32_t k = 0; k < 5; ++k) b.append_record(publishRecord(k));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_OK, "case15: well formed", zhao_abi::ZH_ABI_OK, r.err);
    check(r.upload_overflow == 1, "case15: upload_overflow_o fires", 1, r.upload_overflow);
    check(r.abandoned == 1, "case15: the packet is abandoned", 1, r.abandoned);
    check(r.uploads.empty(), "case15: NO upload leaves -- not four of five", 0, r.uploads.size());
  }

  // ---- 16. a payload-CRC failure carries no upload out ---------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishRecord(0));
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    p[36 + 32 + 20] = static_cast<uint8_t>(p[36 + 32 + 20] ^ 0x5Au);  // inside hps_addr_lo
    const Run r = runPacket(p, 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, "case16: payload CRC fails",
          zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, r.err);
    check(r.abandoned == 1, "case16: abandoned", 1, r.abandoned);
    check(r.uploads.empty(), "case16: NO upload of a packet that failed its CRC", 0,
          r.uploads.size());
  }

  // ---- 17. MEM.UPLOAD busy: the commit does NOT wait for it ----------------
  // Ready held low for the whole run. The packet must still commit and its
  // form still dispatch; the upload waits in the pending queue, lost nowhere.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishRecord(0));
    b.append_record(drawFormRecord(0x70u, 0xF00Du, 0xBEA7u, 0xC0DEu, 1, 0, 0));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u);
    check(r.committed == 1, "case17: committed with MEM.UPLOAD busy", 1, r.committed);
    check(r.draws.size() == 1, "case17: the form was not held behind the upload", 1,
          r.draws.size());
    check(r.uploads.empty() && r.uploads_issued == 0, "case17: the upload is still pending", 0,
          r.uploads_issued);
  }

  // ---- 18. R25: SetEnvironment, committed -- the LAST record wins ---------
  // Two SetEnvironment records around a SetView, the ambient/colour values
  // chosen so a byte-swap or an offset slip shows. Exactly ONE environment
  // leaves, after the verdict, carrying the SECOND record's four bank fields;
  // it is no longer counted as unsupported (BeginFrame + EndFrame still are).
  // env_ready is held low for a while to show the offer waits, unchanged.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setEnvironmentRecord(0x31u, 0x1122u, 0x3344u, 0x5566u, 0x7788u));
    b.append_record(setViewRecord(0, 0x0000'0007u, 0x0011'0000));
    b.append_record(setEnvironmentRecord(0x32u, 0xA1B2u, 0x0C3Du, 0xBDF7u, 0x4208u));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu, 0xFFFF0000u);
    check(r.err == zhao_abi::ZH_ABI_OK, "case18: the packet is well formed (SetEnvironment implemented)",
          zhao_abi::ZH_ABI_OK, r.err);
    check(r.committed == 1, "case18: committed", 1, r.committed);
    check(r.envs.size() == 1, "case18: ONE environment presented for two records", 1, r.envs.size());
    check(r.envs_issued == 1, "case18: envs_issued_o", 1, r.envs_issued);
    check(r.unsupported == 2, "case18: SetEnvironment is no longer unsupported", 2, r.unsupported);
    if (!r.envs.empty()) {
      const EnvOut& e = r.envs[0];
      check(e.cycle > r.verdict_cycle, "case18: the environment leaves AFTER the verdict", 1,
            e.cycle > r.verdict_cycle ? 1 : 0);
      check(e.yaw == 0xA1B2u, "case18: sun_yaw of the LAST record", 0xA1B2u, e.yaw);
      check(e.pitch == 0x0C3Du, "case18: sun_pitch", 0x0C3Du, e.pitch);
      check(e.sun == 0xBDF7u, "case18: sun_colour", 0xBDF7u, e.sun);
      check(e.ambient == 0x4208u, "case18: ambient", 0x4208u, e.ambient);
    }
  }

  // ---- 19. R25: an ABANDONED packet's environment never leaves ------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setEnvironmentRecord(0x33u, 0x1111u, 0x2222u, 0x3333u, 0x4444u));
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    // a pad byte of the SetEnvironment record: header CRC intact, payload CRC not
    p[36 + 32 + 40] = static_cast<uint8_t>(p[36 + 32 + 40] ^ 0xFFu);
    const Run r = runPacket(p, 0xFFFFFFFFu);
    check(r.abandoned == 1, "case19: the packet is abandoned", 1, r.abandoned);
    check(r.envs.empty() && r.envs_issued == 0,
          "case19: a staged environment is DISCARDED with its packet", 0,
          static_cast<uint32_t>(r.envs.size()) + r.envs_issued);
  }
  // THE CLOSING BRACE ABOVE WAS DROPPED BY THE 2026-09-20 MERGE, which spliced
  // the post lane's cases in after the geom lane's case 18 without closing it.
  // Every block below was then nested inside case 18 and the file did not
  // compile -- so `cmd_exec_directed` was not running on the merged tree at all.
  //
  // REPAIRED 2026-09-20 (owner ruling R62). THREE blocks were numbered 17 --
  // the token ceiling, SetEnvironment and SetPost -- and one label numbered 17
  // named 35 checks across all three, so a failure message named no block at
  // all. Every
  // block is now renumbered in file order, 1 through 27.
  //
  // The numbers are comments and string literals, not assertions, so nothing in
  // the compiler can notice the next collision. `tools/budget/check_case_labels.py`
  // is what notices: it refuses a duplicate block number and refuses a "caseN:"
  // label sitting in a block that is not N -- which is the likelier defect, a
  // label copied along with the code it labels. Run it, do not re-derive it.

  // ---- 20. SetPost + two SetGradeTables + a form: the look and the table ---
  // Field for field against zref::post::look, the table entries generated by
  // the zref EMITTER (grade_product_vector) and read back off `pv_*` in order,
  // AFTER the verdict and BEFORE the form (EX_POST precedes EX_DRAW).
  {
    GradeFixture gf;
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setPostRecord(0x9Cu, 0x03u, 0x40u, -256, 17, 255, 0xF81Fu, 0x07E0u));
    b.append_record(setGradeRecord(gf.recs[0]));   // R 0..7
    b.append_record(setGradeRecord(gf.recs[11]));  // G 56..63 (R is 4 records, G 8)
    b.append_record(drawFormRecord(0x70u, 0xF00Du, 0xBEA7u, 0xC0DEu, 1, 0, 0));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    zref::post::look::Look want;
    const bool ok = zref::post::look::apply_set_post(0x9Cu, 0x03u, 0x40u, -256, 17, 255, 0xF81Fu,
                                                     0x07E0u, &want);
    check(ok, "case20: zref accepts the look", 1, ok ? 1 : 0);
    check(r.err == zhao_abi::ZH_ABI_OK && r.committed == 1, "case20: committed", 1, r.committed);
    check(r.unsupported == 2, "case20: SetPost/SetGradeTable are no longer unsupported", 2,
          r.unsupported);
    check(r.looks_applied == 1, "case20: one look applied", 1, r.looks_applied);
    check(r.look.bloom_gain == want.bloom_gain && r.look.grade_valid == want.grade_valid &&
              r.look.echo_arm == want.echo_arm,
          "case20: bloom gain, grade-valid and the ECHO ARM as zref", 1, 1);
    check(r.look.bias_r == want.bias_r && r.look.bias_g == want.bias_g && r.look.bias_b == want.bias_b,
          "case20: the three biases, sign and all (-256 and 255 are the edges)", 0,
          (r.look.bias_r - want.bias_r) + (r.look.bias_b - want.bias_b));
    check(r.look.flash == want.flash && r.look.flash_amount == want.flash_amount &&
              r.look.ink == want.ink,
          "case20: flash colour, flash amount and ink", 1, 1);
    check(r.pv.size() == 16 && r.grade_written == 16, "case20: sixteen table entries written", 16,
          r.pv.size());
    if (r.pv.size() == 16) {
      for (unsigned k = 0; k < 8; ++k) checkPv(r.pv[k], gf.recs[0], k, "case20 R" + std::to_string(k));
      for (unsigned k = 0; k < 8; ++k)
        checkPv(r.pv[8 + k], gf.recs[11], k, "case20 G" + std::to_string(56 + k));
      check(r.pv.front().cycle > r.verdict_cycle, "case20: no table write before the verdict", 1,
            r.pv.front().cycle > r.verdict_cycle ? 1 : 0);
      if (r.draws.size() == 1)
        check(r.pv.back().cycle < r.draws[0].cycle, "case20: the table is in before the form leaves", 1,
              r.pv.back().cycle < r.draws[0].cycle ? 1 : 0);
    }
    check(r.look_busy_cycles > 0, "case20: the pass-start hold was raised while it streamed", 1,
          r.look_busy_cycles > 0 ? 1 : 0);
  }

  // ---- 21. REFUSE, NEVER MASK: five bad records, the packet still commits ---
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setPostRecord(1, 0x04u, 0, 0, 0, 0, 0, 0));      // unassigned flag bit
    b.append_record(setPostRecord(1, 0x00u, 0, 256, 0, 0, 0, 0));    // bias past nine bits
    b.append_record(setPostRecord(1, 0x00u, 0, 0, 0, -257, 0, 0));   // ... and below
    zref::post::look::GradeRecord bad;
    bad.curve = 3; bad.first = 0; bad.count = 1;                     // no such curve
    b.append_record(setGradeRecord(bad));
    bad.curve = 1; bad.first = 60; bad.count = 5;                    // past G's 64 entries
    b.append_record(setGradeRecord(bad));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    zref::post::look::Look z;
    const bool zr = zref::post::look::apply_set_post(1, 0x04u, 0, 0, 0, 0, 0, 0, &z) ||
                    zref::post::look::apply_set_post(1, 0, 0, 256, 0, 0, 0, 0, &z) ||
                    zref::post::look::apply_set_post(1, 0, 0, 0, 0, -257, 0, 0, &z) ||
                    zref::post::look::grade_record_ok(3, 0, 1) ||
                    zref::post::look::grade_record_ok(1, 60, 5);
    check(!zr, "case21: zref refuses all five", 0, zr ? 1 : 0);
    check(r.post_refused == 5, "case21: post_refused_o counts all five", 5, r.post_refused);
    check(r.committed == 1, "case21: a refused RECORD does not refuse the packet", 1, r.committed);
    check(r.looks_applied == 0 && r.pv.empty(), "case21: nothing applied, nothing written", 0,
          r.looks_applied + r.pv.size());
    check(r.look.bloom_gain == 0 && !r.look.echo_arm, "case21: the identity look stands", 0,
          r.look.bloom_gain);
  }

  // ---- 22. more entries than GRADE_Q = 16: refused WHOLE, counted ----------
  {
    GradeFixture gf;
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setPostRecord(5, 0x02u, 0, 0, 0, 0, 0, 0));
    for (unsigned i = 0; i < 3; ++i) b.append_record(setGradeRecord(gf.recs[i]));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_OK, "case22: well formed", zhao_abi::ZH_ABI_OK, r.err);
    check(r.grade_overflow >= 1, "case22: grade_overflow_o fires", 1, r.grade_overflow >= 1 ? 1 : 0);
    check(r.abandoned == 1, "case22: the packet is abandoned", 1, r.abandoned);
    check(r.pv.empty() && r.looks_applied == 0 && !r.look.echo_arm,
          "case22: NO entry and NO look leaves -- not sixteen of twenty-four", 0,
          r.pv.size() + r.looks_applied);
  }

  // ---- 23. THE DOOR: a busy post lease holds the look, then lets it in ------
  // Idle held LOW until long after the verdict. Nothing may reach POST.COMPOSITE
  // (or POST.ECHO's arm) while a pass is in flight; once idle, all of it lands.
  {
    GradeFixture gf;
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setPostRecord(0x11u, 0x02u, 0, 0, 0, 0, 0, 0));
    b.append_record(setGradeRecord(gf.recs[4]));
    b.end_frame(0);
    const std::vector<uint8_t> pkt = b.seal(1, 1, 0);
    const uint32_t door = static_cast<uint32_t>(pkt.size()) + 1500u;
    const Run r = runPacket(pkt, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu, door);   // env_mask, then the lease door
    check(r.committed == 1 && r.looks_applied == 1 && r.pv.size() == 8,
          "case23: the look and the table land once the lease is idle", 9,
          r.looks_applied + r.pv.size());
    uint32_t early = 0;
    for (const PvWrite& w : r.pv)
      if (w.cycle < door) ++early;
    check(early == 0 && r.look_busy_first >= door,
          "case23: nothing crossed the door while the pass was in flight", 0, early);
    check(r.look.echo_arm && r.look.bloom_gain == 0x11u, "case23: the echo is ARMED by it", 1,
          r.look.echo_arm ? 1 : 0);
  }

  // ---- 24. a packet with no post records never waits on the door -----------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(drawFormRecord(0x70u, 0xF00Du, 0xBEA7u, 0xC0DEu, 1, 0, 0));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu);
    check(r.committed == 1 && r.draws.size() == 1,
          "case24: no post state -> the commit passes a busy lease without waiting", 1,
          r.draws.size());
    check(r.look_busy_cycles == 0, "case24: and never holds a pass start", 0, r.look_busy_cycles);
  }

  // ---- 25. THE EYE (ruling R63) reaches cfg 19/20/21, per view ------------
  //
  // The gap R63 closes is that TERRAIN.LOD wants a world-space camera and
  // `SetView` carried only the FUSED view-projection. This case is the
  // producer half of the traverse: a SetView with a distinguishable eye per
  // view, and the assertion that the executor puts each of the three fx16
  // words on its own configuration address, for the right view, with no
  // sign or byte-order damage.
  //
  // NEGATIVE VALUES ON PURPOSE. The eye is fx16 signed and a camera west or
  // below the origin is ordinary, so `eye_y` of view 1 is negative. Pack the
  // field unsigned or shift it arithmetically by mistake and this is the
  // check that says so; three positive words would not.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    // View 0's eye and view 1's eye differ in every word, so a commit walk
    // that carried the WRONG VIEW's shadow -- the failure the shared
    // `sv_dirty` bit exists to prevent -- cannot pass by coincidence.
    b.append_record(setViewRecord(0, 0x90u, 0x0011'0000, 0, 0,
                                  0x0012'3456, 0x0007'8000, static_cast<int32_t>(0xFFF8'0000)));
    b.append_record(setViewRecord(1, 0x91u, 0x0022'0000, 0, 0,
                                  static_cast<int32_t>(0xFFED'CBA9), 0x0000'0001, 0x7FFF'FFFF));
    b.end_frame(0);
    const Run r = runPacket(b.seal(2, 2, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu);
    check(r.committed == 1, "case25: committed", 1, r.committed);
    // Two views, three words each. Exactly six: a repeat would mean the walk
    // re-presented an accepted word, which `proj_cfg_ready_i` makes possible
    // and which no result-checking assertion would notice (CLAUDE.md,
    // "counters see what pictures cannot").
    check(r.eyes.size() == 6, "case25: six eye writes, two views x three words", 6,
          r.eyes.size());
    // And the matrix walk is UNDISTURBED -- 32 words for two views. If the
    // three new steps had been spliced into the 0..15 range instead of
    // appended, this is the number that would move.
    check(r.cfg.size() == 32, "case25: the matrix walk is still 32 words", 32, r.cfg.size());
    check(r.views == 2, "case25: both views written", 2, r.views);
    if (r.eyes.size() == 6) {
      const int32_t want[2][3] = {
          {0x0012'3456, 0x0007'8000, static_cast<int32_t>(0xFFF8'0000)},
          {static_cast<int32_t>(0xFFED'CBA9), 0x0000'0001, 0x7FFF'FFFF}};
      // The walk commits view 0 whole, then view 1 whole, and within a view
      // the order is x, y, z. Asserting the ORDER and not just the multiset
      // is deliberate: an address/data mux that swapped two arms would
      // deliver the right six values to the wrong three registers.
      for (unsigned k = 0; k < 6; ++k) {
        const unsigned v = k / 3, w = k % 3;
        const CfgWrite& e = r.eyes[k];
        check(e.view == v, "case25: eye write view", v, e.view);
        check(e.addr == 19 + w, "case25: eye write address", 19 + w, e.addr);
        check(static_cast<int32_t>(e.data) == want[v][w], "case25: eye word value",
              static_cast<uint32_t>(want[v][w]), e.data);
      }
    }
  }

  // ---- 26. the eye is IDEMPOTENT STATE, like the matrix beside it ---------
  //
  // Case 6a proves two SetViews for one view commit ONCE and the second wins.
  // The eye must obey the same law or a view can run with this frame's camera
  // and last frame's eye -- the mismatch nothing downstream can detect, since
  // both values are individually legal.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(0, 1, 0x0033'0000, 0, 0, 111, 222, 333));
    b.append_record(setViewRecord(0, 2, 0x0044'0000, 0, 0, 444, 555, 666));
    b.end_frame(0);
    const Run r = runPacket(b.seal(2, 2, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu);
    check(r.views == 1, "case26: two SetViews for one view commit once", 1, r.views);
    check(r.eyes.size() == 3, "case26: and write the eye once, not twice", 3, r.eyes.size());
    if (r.eyes.size() == 3) {
      check(static_cast<int32_t>(r.eyes[0].data) == 444, "case26: the SECOND eye lands (x)", 444,
            r.eyes[0].data);
      check(static_cast<int32_t>(r.eyes[1].data) == 555, "case26: the SECOND eye lands (y)", 555,
            r.eyes[1].data);
      check(static_cast<int32_t>(r.eyes[2].data) == 666, "case26: the SECOND eye lands (z)", 666,
            r.eyes[2].data);
    }
    // The same record's matrix must be the second one too. Two shadows, one
    // dirty bit: if the eye and the matrix could be committed under separate
    // flags this is where they would disagree.
    if (!r.cfg.empty())
      check(r.cfg[0].data == 0x0044'0000u, "case26: matrix and eye come from the SAME record",
            0x0044'0000u, r.cfg[0].data);
  }

  // ---- 27. a REFUSED view_id writes no eye -------------------------------
  //
  // `view_id >= 2` is refused, never masked (case 7's law). The eye rides the
  // same `sv_ok` gate, so an out-of-range view must not smuggle three words
  // into view 0's registers -- which is exactly what a missing gate on the
  // three new shift arms would do, silently, while every counter agreed.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(7, 0x92u, 0x0022'0000, 0, 0, 0x7BAD'0001, 0x7BAD'0002,
                                  0x7BAD'0003));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu);
    check(r.refused == 1, "case27: the out-of-range view_id is refused", 1, r.refused);
    check(r.eyes.empty(), "case27: and no eye word is written for it", 0, r.eyes.size());
  }

  // ---- 28. TerrainField 0x0200: EVERY lane traverses, not just the footprint
  //
  // Entry I34's build item (a). The thing this case exists to refuse is the
  // shape the directive's 20.8 forbids on the consumer side and which is just
  // as available here: emitting the footprint, which is the easy half, and
  // leaving the uniforms zero. A field program run with ten zeroed uniform
  // lanes is "a field program applied to every vertex, invisible in the
  // result" -- entry I34's own words -- so every one of the sixteen values is
  // given a DIFFERENT non-trivial value and checked by name. A swap, a shear
  // or a dropped lane cannot survive this; a footprint-only implementation
  // fails twelve checks.
  {
    const uint32_t kProgram = 0xC0DE'0042u;
    const int32_t kX0 = -12'345'678, kZ0 = 0x0011'2233, kX1 = 0x7FFF'0000, kZ1 = -1;
    const uint32_t kStart = 0xABCD'1234u, kDur = 0x0000'0F00u;
    uint32_t p[8];
    for (int k = 0; k < 8; ++k) p[k] = 0x1000'0001u + static_cast<uint32_t>(k) * 0x0111'0111u;

    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(terrainFieldRecord(0x1234u, kProgram, kX0, kZ0, kX1, kZ1, kStart, kDur, p));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.done && r.err == zhao_abi::ZH_ABI_OK, "case28: well formed", 1, r.done ? 1 : 0);
    check(r.committed == 1, "case28: packets_committed_o", 1, r.committed);
    check(r.tflds.size() == 1, "case28: one TerrainField left the executor", 1, r.tflds.size());
    check(r.tflds_issued == 1, "case28: tflds_issued_o", 1, r.tflds_issued);
    check(r.tfld_overflow == 0, "case28: tfld_overflow_o", 0, r.tfld_overflow);
    check(r.tfld_truncated == 0, "case28: source_id fitted 16 bits", 0, r.tfld_truncated);
    // The arm must no longer be counted as a record nobody executes.
    check(r.unsupported == 2, "case28: unsupported_o counts BeginFrame + EndFrame ONLY", 2,
          r.unsupported);
    if (!r.tflds.empty()) {
      const TfldOut& t = r.tflds[0];
      check(t.x0 == kX0, "case28: footprint x0", static_cast<uint32_t>(kX0),
            static_cast<uint32_t>(t.x0));
      check(t.z0 == kZ0, "case28: footprint z0 (rectfx.y0)", static_cast<uint32_t>(kZ0),
            static_cast<uint32_t>(t.z0));
      check(t.x1 == kX1, "case28: footprint x1", static_cast<uint32_t>(kX1),
            static_cast<uint32_t>(t.x1));
      check(t.z1 == kZ1, "case28: footprint z1 (rectfx.y1)", static_cast<uint32_t>(kZ1),
            static_cast<uint32_t>(t.z1));
      check(t.handle == kProgram, "case28: program HANDLE (not a content hash)", kProgram,
            t.handle);
      check(t.cmd == 0x1234u, "case28: cmd index is the record source_id, low 16", 0x1234u, t.cmd);
      check(t.start_tick == kStart, "case28: start_tick, the R2 age uniform's origin", kStart,
            t.start_tick);
      check(t.duration == kDur, "case28: duration_ticks, the R3 phase uniform's span", kDur,
            t.duration);
      for (int k = 0; k < 8; ++k) {
        char tag[64];
        snprintf(tag, sizeof tag, "case28: uniform lane p%d (R%d)", k, 4 + k);
        check(t.p[k] == p[k], tag, p[k], t.p[k]);
      }
    }
    // The commit law: not one console-visible bit moves before the verdict.
    if (!r.tflds.empty())
      check(r.tflds[0].cycle > r.verdict_cycle,
            "case28: nothing left the executor at or before the verdict", 1,
            r.tflds[0].cycle > r.verdict_cycle ? 1 : 0);
  }

  // ---- 29. more than TFLD_Q records REFUSES THE PACKET WHOLE --------------
  //
  // TFLD_Q defaults to 4. Five field records must emit NOTHING -- not four,
  // not "the first four" -- because a section 3.4 sum missing one of its lanes
  // is a plausible wrong terrain, and a half-applied packet is the one failure
  // mode nobody would see. This is the stamp arm's declared-bound law, and the
  // counter is the evidence that the refusal happened rather than a hang.
  {
    uint32_t p[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    for (uint32_t k = 0; k < 5; ++k)
      b.append_record(terrainFieldRecord(0x20u + k, 0x0BAD'0000u + k, static_cast<int32_t>(k),
                                         static_cast<int32_t>(k), static_cast<int32_t>(k + 1),
                                         static_cast<int32_t>(k + 1), 100u + k, 7u, p));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.tfld_overflow == 1, "case29: tfld_overflow_o fires once for the packet", 1,
          r.tfld_overflow);
    check(r.tflds.empty(), "case29: NOTHING is emitted -- refused whole, never half-applied", 0,
          r.tflds.size());
    check(r.tflds_issued == 0, "case29: tflds_issued_o stays zero", 0, r.tflds_issued);
  }

  // ---- 30. exactly TFLD_Q records all arrive, in order, under backpressure -
  //
  // The both-polarity control for case 29: four is the bound, not over it, so
  // all four must arrive. And with the list intake refusing on most cycles the
  // arm must HOLD each record and re-present it unchanged -- never skipping
  // one, never issuing one twice. `tflds_issued_o` counting four while five
  // records arrived, or four arriving out of order, are the two defects a
  // result-checking test cannot see.
  {
    uint32_t p[8] = {9, 8, 7, 6, 5, 4, 3, 2};
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    for (uint32_t k = 0; k < 4; ++k)
      b.append_record(terrainFieldRecord(0x30u + k, 0xFEED'0000u + k, static_cast<int32_t>(k * 16),
                                         static_cast<int32_t>(k * 32),
                                         static_cast<int32_t>(k * 48),
                                         static_cast<int32_t>(k * 64), 900u + k, 11u + k, p));
    b.end_frame(0);
    // The intake says yes on one cycle in eight.
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                            0xFFFFFFFFu, 0, 0x01010101u);
    check(r.tfld_overflow == 0, "case30: four records is the bound, not over it", 0,
          r.tfld_overflow);
    check(r.tflds.size() == 4, "case30: all four arrive under backpressure", 4, r.tflds.size());
    check(r.tflds_issued == 4, "case30: tflds_issued_o counts four, not three and not five", 4,
          r.tflds_issued);
    for (size_t k = 0; k < r.tflds.size() && k < 4; ++k) {
      char tag[80];
      snprintf(tag, sizeof tag, "case30: record %zu arrives in submission order", k);
      check(r.tflds[k].handle == 0xFEED'0000u + static_cast<uint32_t>(k), tag,
            0xFEED'0000u + static_cast<uint32_t>(k), r.tflds[k].handle);
      snprintf(tag, sizeof tag, "case30: record %zu is unchanged by the stall", k);
      check(r.tflds[k].start_tick == 900u + static_cast<uint32_t>(k), tag,
            900u + static_cast<uint32_t>(k), r.tflds[k].start_tick);
    }
  }

  // ---- 31. a source_id that does not fit 16 bits is COUNTED ---------------
  //
  // `fld_add_cmd_i` is 16 bits and source_id is u32 on the wire, so the arm
  // narrows it exactly as the stamp and draw arms do. The dropped half is a
  // number, not a silence -- and it is counted on the way OUT, so an abandoned
  // packet's records never reach the counter.
  {
    uint32_t p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(terrainFieldRecord(0x0007'ABCDu, 0x1111'2222u, 1, 2, 3, 4, 5u, 6u, p));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.tflds.size() == 1, "case31: the record still commits", 1, r.tflds.size());
    check(r.tfld_truncated == 1, "case31: tfld_src_truncated_o sees the dropped high half", 1,
          r.tfld_truncated);
    if (!r.tflds.empty())
      check(r.tflds[0].cmd == 0xABCDu, "case31: and the low half is what is carried", 0xABCDu,
            r.tflds[0].cmd);
  }

  return zhao::report_and_exit("cmd_exec_directed");
}
