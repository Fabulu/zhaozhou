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

#include "Vtb_cmd_exec_pair.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"

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

struct Run {
  bool done = false;
  uint8_t err = 0;
  uint32_t commands = 0;
  uint32_t verdict_cycle = 0;
  std::vector<CfgWrite> cfg;       // matrix words, cfg addresses 0..15
  std::vector<CfgWrite> profiles;  // depth-profile writes, cfg address 18
  std::vector<StampOut> stamps;
  std::vector<DrawOut> draws;
  std::vector<UploadOut> uploads;
  uint32_t uploads_issued = 0, upload_overflow = 0;
  uint32_t committed = 0, abandoned = 0, views = 0, issued = 0;
  uint32_t overflow = 0, refused = 0, truncated = 0, unsupported = 0;
  uint32_t draws_issued = 0, draw_overflow = 0, draw_truncated = 0;
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
              uint32_t upl_mask = 0xFFFFFFFFu) {
  Vtb_cmd_exec_pair dut;
  dut.rst_n = 0;
  dut.pkt_valid_i = 0;
  dut.pkt_byte_i = 0;
  dut.pkt_len_i = 0;
  dut.stamp_ready_i = 1;
  dut.proj_cfg_ready_i = 1;
  dut.draw_ready_i = 1;
  dut.upl_ready_i = 1;
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
    dut.eval();

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

    zhao::tick(dut);
    if (moved) ++i;
    if (upl_fires) r.uploads.push_back(u);
    if (stamp_fires) r.stamps.push_back(s);
    // THE SEVENTEENTH STEP. Since ac4f293d every committed view also writes
    // SetView's depth profile to cfg address 18. This bench predates it and
    // counted every cfg write as a matrix word, so eleven checks read 17 and
    // 34 where they meant 16 and 32 -- red since that commit. The profile
    // write is recorded on its own and asserted in case 1, not dropped.
    if (cfg_fires) (w.addr == 18 ? r.profiles : r.cfg).push_back(w);
    if (draw_fires) r.draws.push_back(d);

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
  r.draws_issued = dut.draws_issued_o;
  r.draw_overflow = dut.draw_overflow_o;
  r.draw_truncated = dut.draw_src_truncated_o;
  r.uploads_issued = dut.uploads_issued_o;
  r.upload_overflow = dut.upload_overflow_o;
  return r;
}

// ---- record builders, all layout from the generated packers ---------------

std::vector<uint8_t> setViewRecord(uint8_t view_id, uint32_t source_id, int32_t first_word) {
  zhao_abi::ZhRecordSetView rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_SET_VIEW;
  rec.hdr.record_bytes = 96;
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
  rec.payload.geometry_tokens = 0;
  rec.payload.fragment_tokens = 0;
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

// PublishResource (R17), packed by the GENERATED packer like every record here.
std::vector<uint8_t> publishRecord(uint32_t k) {
  zhao_abi::ZhRecordPublishResource rec{};
  rec.hdr.opcode = zhao_abi::ZHAO_OP_PUBLISH_RESOURCE;
  rec.hdr.record_bytes = 48;
  rec.hdr.source_id = 0x90u + k;
  rec.payload.resource = (0x2Au << 24) | (0x00ABC0u + k);  // {gen 0x2A, index}
  rec.payload.hps_addr_lo = 0x3000'0000u + k * 0x1000u;
  rec.payload.hps_addr_hi = k;                  // nonzero for k>0: carried, not narrowed
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
  check(u.vram == 0x054D'F000u + k * 0x100u, (tag + " vram").c_str(), 0x054DF000u + k * 0x100u, u.vram);
  check(u.len == 256u + 64u * k, (tag + " length").c_str(), 256u + 64u * k, u.len);
  check(u.crc == 0xC0FF'EE00u + k, (tag + " crc32c").c_str(), 0xC0FFEE00u + k, u.crc);
  check(u.gen == 0x0102u + k, (tag + " new generation").c_str(), 0x0102u + k, u.gen);
  check(u.epoch == 9u + k, (tag + " epoch").c_str(), 9u + k, u.epoch);
  check(u.slot == 5u + k, (tag + " dst slot").c_str(), 5u + k, u.slot);
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

  // ---- 2. the same packet with one payload byte flipped -------------------
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

    check(r.done, "case2: reached a verdict", 1, r.done ? 1 : 0);
    check(r.err == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, "case2: the payload CRC fails",
          zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, r.err);
    check(r.abandoned == 1, "case2: packets_abandoned_o fires", 1, r.abandoned);
    check(r.committed == 0, "case2: packets_committed_o", 0, r.committed);
    // The assertion this block exists for: a staged SetView and a staged stamp
    // BOTH evaporate, and the surface sheet is never scarred by a packet that
    // failed its CRC.
    check(r.cfg.empty(), "case2: NO matrix word was written", 0, r.cfg.size());
    check(r.stamps.empty(), "case2: NO stamp was dispatched", 0, r.stamps.size());
    check(r.views == 0, "case2: views_written_o", 0, r.views);
    check(r.issued == 0, "case2: stamps_issued_o", 0, r.issued);
  }

  // ---- 3. a view_id the bank cannot address -------------------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(setViewRecord(7, 0x0000'0007u, 0x0022'0000));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case3: the RECORD is legal ABI", zhao_abi::ZH_ABI_OK,
          r.err);
    check(r.refused == 1, "case3: view_range_refused_o fires", 1, r.refused);
    // Refused, not masked. view_id 7 must NOT land in view 1.
    check(r.cfg.empty(), "case3: nothing was written to either view", 0, r.cfg.size());
    check(r.views == 0, "case3: views_written_o", 0, r.views);
    check(r.committed == 1, "case3: the packet still commits", 1, r.committed);
  }

  // ---- 4. a source_id whose high half does not fit ------------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(
        surfaceStampRecord(0x0001'0005u, kPatch, kOp, kTag, kStrength, kTx, kTy, kRadius, kRing));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.err == zhao_abi::ZH_ABI_OK, "case4: well formed", zhao_abi::ZH_ABI_OK, r.err);
    check(r.truncated == 1, "case4: stamp_src_truncated_o fires", 1, r.truncated);
    check(r.stamps.size() == 1, "case4: the stamp is still dispatched", 1, r.stamps.size());
    if (r.stamps.size() == 1)
      check(r.stamps[0].src_id == 5, "case4: the low half is what is carried", 5,
            r.stamps[0].src_id);
  }

  // ---- 5. more stamps than the ring holds ---------------------------------
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

    check(r.err == zhao_abi::ZH_ABI_OK, "case5: the PACKET is well formed", zhao_abi::ZH_ABI_OK,
          r.err);
    check(r.overflow == 1, "case5: stamp_overflow_o fires", 1, r.overflow);
    check(r.abandoned == 1, "case5: the packet is refused whole", 1, r.abandoned);
    check(r.committed == 0, "case5: packets_committed_o", 0, r.committed);
    check(r.stamps.empty(), "case5: not one stamp escaped", 0, r.stamps.size());
    check(r.issued == 0, "case5: stamps_issued_o", 0, r.issued);
  }

  // ---- 6. the collapse law -------------------------------------------------
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

    check(r.views == 2, "case6b: both views commit", 2, r.views);
    check(r.cfg.size() == 32, "case6b: thirty-two words", 32, r.cfg.size());
    if (r.cfg.size() == 32) {
      check(r.cfg[0].view == 0, "case6b: view 0 drains first", 0, r.cfg[0].view);
      check(r.cfg[0].data == 0x0066'0000u, "case6b: view 0 data", 0x0066'0000u, r.cfg[0].data);
      check(r.cfg[16].view == 1, "case6b: view 1 drains second", 1, r.cfg[16].view);
      check(r.cfg[16].data == 0x0055'0000u, "case6b: view 1 data", 0x0055'0000u, r.cfg[16].data);
    }
  }

  // ---- 7. the same work under stamp backpressure --------------------------
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
      const std::string tag = "case7[mask " + std::to_string(m) + "]";

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

  // ---- 8. the matrix bank refuses, and refuses hard -----------------------
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
      const std::string tag = "case8[cfg mask " + std::to_string(m) + "]";

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

  // ---- 9. one DrawForm, whole -------------------------------------------
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

    check(r.err == zhao_abi::ZH_ABI_OK, "case9: the packet is well formed", zhao_abi::ZH_ABI_OK,
          r.err);
    checkNothingEscapedEarly(r, "case9");
    check(r.committed == 1, "case9: packets_committed_o", 1, r.committed);
    check(r.draws_issued == 1, "case9: draws_issued_o", 1, r.draws_issued);
    check(r.draws.size() == 1, "case9: exactly one dispatch left the block", 1, r.draws.size());
    check(r.draw_overflow == 0, "case9: draw_overflow_o", 0, r.draw_overflow);
    check(r.draw_truncated == 0, "case9: draw_src_truncated_o", 0, r.draw_truncated);
    // DrawForm now has an arm, so it must NOT be counted as unsupported.
    check(r.unsupported == 2, "case9: unsupported_o is BeginFrame + EndFrame only", 2,
          r.unsupported);
    if (r.draws.size() == 1) {
      const DrawOut& d = r.draws[0];
      check(d.form == 0x1234'56A7u, "case9: form handle", 0x1234'56A7u, d.form);
      check(d.material_set == 0x89AB'CD03u, "case9: material_set handle", 0x89AB'CD03u,
            d.material_set);
      check(d.transform == 0x0F1E'2D5Cu, "case9: transform handle", 0x0F1E'2D5Cu, d.transform);
      check(d.viewport_mask == 0x03u, "case9: viewport_mask", 0x03u, d.viewport_mask);
      check(d.semantic_weight == 0x77u, "case9: semantic_weight", 0x77u, d.semantic_weight);
      check(d.flags == 0xBEEFu, "case9: flags, the record's LAST two bytes", 0xBEEFu, d.flags);
      check(d.src_id == 0x0021u, "case9: the record header's source_id", 0x0021u, d.src_id);
    }
  }

  // ---- 10. the draw ring overflows, and the packet is refused WHOLE -------
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

    check(r.err == zhao_abi::ZH_ABI_OK, "case10: the PACKET is well formed -- the refusal is ours",
          zhao_abi::ZH_ABI_OK, r.err);
    check(r.draw_overflow == 1, "case10: draw_overflow_o FIRED", 1, r.draw_overflow);
    check(r.abandoned == 1, "case10: the poisoned packet is abandoned", 1, r.abandoned);
    check(r.committed == 0, "case10: nothing was committed", 0, r.committed);
    check(r.draws.empty(), "case10: not one form was dispatched", 0, r.draws.size());
    check(r.cfg.empty(), "case10: and the view did not commit either", 0, r.cfg.size());
  }

  // ---- 11. a source_id whose high half cannot fit ------------------------
  // The SurfaceStamp narrowing rule, applied to the draw arm and counted on
  // its OWN counter: one flag for both records would attribute a form's
  // truncated id to a stamp.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(drawFormRecord(0x00CD'1234u, 0xAAAAu, 0xBBBBu, 0xCCCCu, 2, 9, 0x0007u));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);

    check(r.draw_truncated == 1, "case11: draw_src_truncated_o FIRED", 1, r.draw_truncated);
    check(r.truncated == 0, "case11: and the STAMP counter did not move", 0, r.truncated);
    check(r.draws_issued == 1, "case11: the draw still dispatches", 1, r.draws_issued);
    if (r.draws.size() == 1)
      check(r.draws[0].src_id == 0x1234u, "case11: the low half is carried", 0x1234u,
            r.draws[0].src_id);
  }

  // ---- 12. VIEWS BEFORE DRAWS, under backpressure ------------------------
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
      const std::string tag = "case12[draw mask " + std::to_string(m) + "]";

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

  // ---- 13. PublishResource (R17): two uploads and a form, committed --------
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
    check(r.err == zhao_abi::ZH_ABI_OK, "case13: well formed", zhao_abi::ZH_ABI_OK, r.err);
    checkNothingEscapedEarly(r, "case13");
    check(r.committed == 1, "case13: committed", 1, r.committed);
    check(r.uploads.size() == 2, "case13: two uploads handed to MEM.UPLOAD", 2, r.uploads.size());
    check(r.uploads_issued == 2, "case13: uploads_issued_o", 2, r.uploads_issued);
    check(r.unsupported == 2, "case13: only BeginFrame/EndFrame unsupported now", 2, r.unsupported);
    if (r.uploads.size() == 2) {
      checkUpload(r.uploads[0], 0, "case13 upload 0");
      checkUpload(r.uploads[1], 1, "case13 upload 1");
      check(r.uploads[0].cycle > r.verdict_cycle, "case13: no upload before the verdict", 1,
            r.uploads[0].cycle > r.verdict_cycle ? 1 : 0);
      if (r.draws.size() == 1)
        check(r.uploads[0].cycle < r.draws[0].cycle, "case13: the upload starts before the form", 1,
              r.uploads[0].cycle < r.draws[0].cycle ? 1 : 0);
    }
  }

  // ---- 14. five uploads against UPL_Q = 4: refused WHOLE, counted ----------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    for (uint32_t k = 0; k < 5; ++k) b.append_record(publishRecord(k));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_OK, "case14: well formed", zhao_abi::ZH_ABI_OK, r.err);
    check(r.upload_overflow == 1, "case14: upload_overflow_o fires", 1, r.upload_overflow);
    check(r.abandoned == 1, "case14: the packet is abandoned", 1, r.abandoned);
    check(r.uploads.empty(), "case14: NO upload leaves -- not four of five", 0, r.uploads.size());
  }

  // ---- 15. a payload-CRC failure carries no upload out ---------------------
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishRecord(0));
    b.end_frame(0);
    std::vector<uint8_t> p = b.seal(1, 1, 0);
    p[36 + 32 + 20] = static_cast<uint8_t>(p[36 + 32 + 20] ^ 0x5Au);  // inside hps_addr_lo
    const Run r = runPacket(p, 0xFFFFFFFFu);
    check(r.err == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, "case15: payload CRC fails",
          zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, r.err);
    check(r.abandoned == 1, "case15: abandoned", 1, r.abandoned);
    check(r.uploads.empty(), "case15: NO upload of a packet that failed its CRC", 0, r.uploads.size());
  }

  // ---- 16. MEM.UPLOAD busy: the commit does NOT wait for it ----------------
  // Ready held low for the whole run. The packet must still commit and its
  // form still dispatch; the upload waits in the pending queue, lost nowhere.
  {
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishRecord(0));
    b.append_record(drawFormRecord(0x70u, 0xF00Du, 0xBEA7u, 0xC0DEu, 1, 0, 0));
    b.end_frame(0);
    const Run r = runPacket(b.seal(1, 1, 0), 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u);
    check(r.committed == 1, "case16: committed with MEM.UPLOAD busy", 1, r.committed);
    check(r.draws.size() == 1, "case16: the form was not held behind the upload", 1, r.draws.size());
    check(r.uploads.empty() && r.uploads_issued == 0, "case16: the upload is still pending", 0,
          r.uploads_issued);
  }

  return zhao::report_and_exit("cmd_exec_directed");
}
