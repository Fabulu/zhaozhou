// procmat_acceptance.cpp -- THE ACCEPTANCE TEST OWNER COMPLETION RULING 2
// NAMES, in the owner's own words:
//
//   "Acceptance: two records in one set, the same record ID in two sets, upper
//    handle-index bits, generation/residency cases governed by the existing
//    handle law, backpressure and alternating procedural draws. Compare against
//    the reference and ASSERT THAT THE CORRECT MATERIAL ACTUALLY REACHES THE
//    RASTER PATH."
//
// ---------------------------------------------------------------------------
// WHAT THIS DRIVER TOUCHES, AND NOTHING ELSE
// ---------------------------------------------------------------------------
//   1. THE PACKET BYTE STREAM. Every draw is a DrawProcedural record packed by
//      the GENERATED packer from `spec/commands.zidl` -- `zhao_pack_draw_
//      procedural` -- and fed a byte at a time into the production decoder and
//      executor. No field is written to a port.
//   2. THE PAGE MEMORY IMAGE. Every forge record is bytes in an array the
//      production page bank reads through its own MEM.GUARD request port, at
//      `zref::forge_page`'s frozen offsets.
//   3. MATERIAL.RESOLVE's ANSWER, which is also THE MEASUREMENT: the window's
//      request port is where the (set, id) pair ARRIVES after the whole chain.
//   4. GEOM.CLIP's READY, and the MEM.GUARD responder's stall -- the two places
//      backpressure is applied.
//
// ---------------------------------------------------------------------------
// THE GENERATION CASE IS THE ONE THAT PROVES THE POINT
// ---------------------------------------------------------------------------
// A handle32 is {index[31:8], generation[7:0]}. The reading this packet removed
// took the material word's LOW SIXTEEN BITS as the record id, so those sixteen
// bits were {index[15:8], generation[7:0]} and A RESIDENCY EVENT -- an ordinary
// generation bump -- SILENTLY SELECTED A DIFFERENT MATERIAL RECORD.
//
// Case 4 issues the same set index at two generations with the same declared
// `material_id`, and requires the window to ask for THE SAME RECORD BOTH TIMES.
// Under the old reading it would have asked for two different ones, and the
// numbers are stated in the check so a reader can see the difference rather
// than take the sentence for it.
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST IS NOT -- SAID HERE RATHER THAN IMPLIED
// ---------------------------------------------------------------------------
// IT IS NOT A RENDERED-PIXEL COMPARISON. The ruling says "compare against the
// reference"; the reference renderer's Phase-3 material model is a SET OF ONE
// (`zref_render.hpp`: "a flat base colour"), so it has no second record to
// disagree about and cannot express the cases above at all. The REFERENCE THAT
// IS PRESENT is the material model this driver holds: a map from (set, id) to a
// record, answered on the window's request port, so every published field is
// compared against THE RECORD OF THE PAIR THE DRAW DECLARED -- not a constant,
// and not "whatever was published".
//
// AND THE CONSOLE SMOKE STILL DOES NOT DRIVE A PROCEDURAL DRAW. Its fixture
// feeds meshes; `run_console_core_smoke.ps1` contains no DrawProcedural record.
// So a green smoke says nothing whatever about this path, which is precisely
// the ruling's "an otherwise green smoke whose upstream fixture never reaches
// the new path does not prove the path". That is why this bench exists and why
// the sentence is here instead of a claim that the smoke covers it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_procmat_acceptance.h"

#include "zhao_abi.h"
#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"
#include "zref/zref_forge_page.hpp"

namespace {

Vtb_procmat_acceptance* top = nullptr;

int checks = 0;
int fails = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s\n", what);
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++fails;
    std::printf("FAIL: %s -- want 0x%llx, got 0x%llx\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got));
  }
}

// ---------------------------------------------------------------------------
// THE MATERIAL MODEL -- the reference this bench compares against
// ---------------------------------------------------------------------------
// A record per (set handle, record id). `sample_count` is the field R197's door
// judges an untextured primitive against, and a forge primitive is untextured
// BY LAW, so a record with sample_count != 0 is the incompatible case the
// ruling requires to stay refused.
struct MatRecord {
  bool has_record = true;
  uint8_t sample_count = 0;
  uint8_t recipe = 0;
  uint8_t weight = 0;
  uint8_t binding = 0;
  uint8_t sample0_modes = 0;
};

std::map<std::pair<uint32_t, uint16_t>, MatRecord> gMaterials;

// Every request the window made, in order. THIS IS THE MEASUREMENT.
struct Ask {
  uint32_t set;
  uint16_t id;
};
std::vector<Ask> gAsks;

// Every beat that entered GEOM.CLIP, with the pair it carried.
struct Entered {
  uint32_t set;
  uint16_t id;
  uint16_t src_id;
  uint8_t sample_count;
};
std::vector<Entered> gEntered;

// Every forge record CMD.EXEC handed the page bank, as it decoded it.
struct Decoded {
  uint32_t program;
  uint32_t material_set;
  uint16_t material_id;
  uint16_t frame_tick;
};
std::vector<Decoded> gDecoded;

// ---- the resolver state machine (the driver plays MATERIAL.RESOLVE) -------
int gRspDelay = 0;
bool gRspPending = false;
uint32_t gPendSet = 0;
uint16_t gPendId = 0;

void driveResolver() {
  // Accept a request. `req_ready_i` is held high unless a case lowers it.
  if (top->req_valid_o && top->req_ready_i && !gRspPending) {
    gAsks.push_back(Ask{top->req_material_set_o,
                        static_cast<uint16_t>(top->req_material_id_o)});
    gPendSet = top->req_material_set_o;
    gPendId = static_cast<uint16_t>(top->req_material_id_o);
    gRspPending = true;
    gRspDelay = 3;   // a few clocks, so the window's ST_WAIT is real
  }
  if (gRspPending && gRspDelay > 0) --gRspDelay;

  top->rsp_valid_i = 0;
  if (gRspPending && gRspDelay == 0) {
    const auto it = gMaterials.find({gPendSet, gPendId});
    const bool found = (it != gMaterials.end());
    const MatRecord r = found ? it->second : MatRecord{false, 0, 0, 0, 0, 0};
    top->rsp_valid_i = 1;
    top->rsp_has_record_i = (found && r.has_record) ? 1 : 0;
    top->rsp_sample_count_i = r.sample_count;
    top->rsp_material_recipe_i = r.recipe;
    top->rsp_recipe_weight_i = r.weight;
    top->rsp_base_binding_i = r.binding;
    top->rsp_sample0_modes_i = r.sample0_modes;
  }
}

void cyc() {
  driveResolver();
  top->eval();
  // Sample everything that is valid THIS cycle, before the edge.
  if (top->cmd_forge_take_o) {
    gDecoded.push_back(Decoded{top->cmd_forge_program_o,
                               top->cmd_forge_material_o,
                               static_cast<uint16_t>(top->cmd_forge_material_id_o),
                               static_cast<uint16_t>(top->cmd_forge_frame_tick_o)});
  }
  if (top->cl_in_valid_o && top->cl_in_ready_i) {
    gEntered.push_back(Entered{top->cl_material_set_o,
                               static_cast<uint16_t>(top->cl_material_id_o),
                               static_cast<uint16_t>(top->cl_src_id_o),
                               static_cast<uint8_t>(top->pub_sample_count_o)});
  }
  // The resolver's handshake retires on the same edge the window takes it.
  const bool rsp_taken = (top->rsp_valid_i && top->rsp_ready_o);
  // R197's door consumes a refused triangle; `d_leave_i` is the shell door
  // taking one, which this bench plays as "one clock after it entered".
  top->d_leave_i = (top->cl_in_valid_o && top->cl_in_ready_i) ? 1 : 0;
  top->clk = 1;
  top->eval();
  top->clk = 0;
  top->eval();
  if (rsp_taken) gRspPending = false;
  top->d_leave_i = 0;
}

void idleInputs() {
  top->rst_n = 0;
  top->dec_rst_n = 1;
  top->pkt_valid_i = 0;
  top->pkt_byte_i = 0;
  top->pkt_len_i = 0;
  top->mem_we_i = 0;
  top->mem_addr_i = 0;
  top->mem_data_i = 0;
  top->mem_stall_i = 0;
  top->pub_valid_i = 0;
  top->pub_tag_i = 0;
  top->pub_base_i = 0;
  top->pub_extent_i = 0;
  top->req_ready_i = 1;
  top->rsp_valid_i = 0;
  top->rsp_has_record_i = 0;
  top->rsp_sample_count_i = 0;
  top->rsp_material_recipe_i = 0;
  top->rsp_recipe_weight_i = 0;
  top->rsp_base_binding_i = 0;
  top->rsp_sample0_modes_i = 0;
  top->cl_in_ready_i = 1;
  top->d_reject_i = 0;
  top->d_leave_i = 0;
}

void hardReset() {
  delete top;
  top = new Vtb_procmat_acceptance;
  gAsks.clear();
  gEntered.clear();
  gDecoded.clear();
  gRspPending = false;
  gRspDelay = 0;
  idleInputs();
  top->clk = 0;
  for (int i = 0; i < 6; ++i) {
    top->eval();
    top->clk = 1;
    top->eval();
    top->clk = 0;
    top->eval();
  }
  top->rst_n = 1;
  for (int i = 0; i < 4; ++i) cyc();
}

// ---------------------------------------------------------------------------
// THE FORGE PAGE, at `zref::forge_page`'s frozen offsets
// ---------------------------------------------------------------------------
void put32(std::vector<uint8_t>& m, size_t off, uint32_t v) {
  for (int b = 0; b < 4; ++b) m[off + b] = static_cast<uint8_t>((v >> (8 * b)) & 0xFF);
}
void put16(std::vector<uint8_t>& m, size_t off, uint16_t v) {
  m[off] = static_cast<uint8_t>(v & 0xFF);
  m[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

constexpr uint32_t kFx1 = 0x00010000u;   // 1.0 in S15.16

// One RIBBON record. The family is the ribbon so the RIBBON evaluator runs --
// which is also the path R241's `frame_tick` feeds, through the bank's
// `tick_phase_base + frame_tick` add.
void writeRecord(std::vector<uint8_t>& m, size_t base, uint32_t program_index,
                 uint16_t src_id) {
  using namespace zref::forge_page;
  put32(m, base + kOffProgramIndex, program_index & kProgramIndexMask);
  m[base + kOffFamily] = kFamRibbon;
  m[base + kOffSweep] = kSweepLinear;
  m[base + kOffSegments] = 2;
  m[base + kOffSides] = 1;     // an OPEN family's ring is the width-axis pair
  m[base + kOffViewMask] = 3;
  m[base + kOffBranchCount] = 0;
  put16(m, base + kOffSrcId, src_id);
  // anchor0 -> anchor1 along +x, one unit apart.
  put32(m, base + kOffAnchor0 + 0, 0);
  put32(m, base + kOffAnchor0 + 4, 0);
  put32(m, base + kOffAnchor0 + 8, kFx1);
  put32(m, base + kOffAnchor1 + 0, kFx1);
  put32(m, base + kOffAnchor1 + 4, 0);
  put32(m, base + kOffAnchor1 + 8, kFx1);
  // axis_u is the ribbon's WIDTH axis; axis_v / axis_w its two jitter axes.
  put32(m, base + kOffAxisU + 0, 0);
  put32(m, base + kOffAxisU + 4, kFx1);
  put32(m, base + kOffAxisU + 8, 0);
  put32(m, base + kOffAxisV + 0, 0);
  put32(m, base + kOffAxisV + 4, 0);
  put32(m, base + kOffAxisV + 8, kFx1);
  put32(m, base + kOffRadius0, kFx1 / 4);
  put32(m, base + kOffRadius1, kFx1 / 4);   // a ribbon's two radii MUST be equal
  put32(m, base + kOffAmp, 0);
  put32(m, base + kOffBranchAmp, 0);
  put32(m, base + kOffBranchRadius, 0);
  put32(m, base + kOffSeed, 0x1234abcdu);
  put16(m, base + kOffTickPhaseBase, 0);
  put32(m, base + kOffAxisW + 0, kFx1);
  put32(m, base + kOffAxisW + 4, 0);
  put32(m, base + kOffAxisW + 8, 0);
}

// The page: a 64-byte header then `count` records. Program indices 1..count.
std::vector<uint8_t> buildPage(uint32_t count) {
  using namespace zref::forge_page;
  std::vector<uint8_t> m(kHeaderBytes + kRecordBytes * count, 0);
  put32(m, 0, kMagic);
  put16(m, 4, kVersion);
  put16(m, 6, static_cast<uint16_t>(count));
  for (uint32_t i = 0; i < count; ++i) {
    writeRecord(m, kHeaderBytes + kRecordBytes * i, i + 1,
                static_cast<uint16_t>(0x0100 + i));
  }
  return m;
}

void loadMemory(const std::vector<uint8_t>& img) {
  for (size_t i = 0; i < img.size(); ++i) {
    top->mem_we_i = 1;
    top->mem_addr_i = static_cast<uint16_t>(i);
    top->mem_data_i = img[i];
    cyc();
  }
  top->mem_we_i = 0;
  cyc();
}

void publishPage(uint32_t extent) {
  top->pub_valid_i = 1;
  top->pub_tag_i = zref::forge_page::kPageKind;
  top->pub_base_i = 0;
  top->pub_extent_i = extent;
  cyc();
  top->pub_valid_i = 0;
  // The header read is a request, a verdict and eight beats.
  for (int i = 0; i < 64; ++i) cyc();
}

// ---- the command packet ---------------------------------------------------
// A handle32 is {index[31:8], generation[7:0]} -- `spec/commands.zidl`'s law,
// written here once so every case reads as the owner's own vocabulary.
constexpr uint32_t handle32(uint32_t index, uint8_t generation) {
  return (index << 8) | generation;
}

std::vector<uint8_t> drawProceduralRecord(uint32_t source_id, uint32_t program,
                                          uint32_t material_set, uint16_t material_id,
                                          uint16_t frame_tick) {
  zhao_abi::ZhRecordDrawProcedural r{};
  r.hdr.opcode = zhao_abi::ZHAO_OP_DRAW_PROCEDURAL;
  r.hdr.record_bytes = 64;
  r.hdr.source_id = source_id;
  r.payload.program = program;
  r.payload.material_set = material_set;
  r.payload.material_id = material_id;
  r.payload.frame_tick[0] = static_cast<uint8_t>(frame_tick & 0xFF);
  r.payload.frame_tick[1] = static_cast<uint8_t>((frame_tick >> 8) & 0xFF);
  // An identity 2D transform, and FORGE_RIBBON -- `forge_kind` for FAM_RIBBON,
  // which is (0 + 1) mod 6 = 1. The rotation is real and `zhao_forge_pagebank`
  // refuses a straight-through value, so this is the reference's own function
  // rather than a literal.
  r.payload.transform.r00 = static_cast<int32_t>(kFx1);
  r.payload.transform.r11 = static_cast<int32_t>(kFx1);
  r.payload.kind = static_cast<zhao_abi::forge_kind>(
      zref::forge_page::kind_of_family(zref::forge_page::kFamRibbon));
  r.payload.screen_error = static_cast<int32_t>(kFx1);
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_draw_procedural(r, out);
  return out;
}

uint8_t gLastErr = 0xFF;

// CMD.DECODER IS ONE-SHOT: its S_DONE holds the verdict until reset, so a
// second packet needs the decoder re-armed. The rest of the chain -- the page
// bank's adopted page, the window's publication -- must NOT be reset with it.
void rearmDecoder() {
  top->dec_rst_n = 0;
  for (int i = 0; i < 4; ++i) cyc();
  top->dec_rst_n = 1;
  for (int i = 0; i < 2; ++i) cyc();
}

void pushPacket(const std::vector<uint8_t>& pkt) {
  size_t i = 0;
  gLastErr = 0xFFu;
  top->pkt_len_i = static_cast<uint32_t>(pkt.size());
  for (int guard = 0; guard < 400000 && i < pkt.size(); ++guard) {
    top->pkt_valid_i = 1;
    top->pkt_byte_i = pkt[i];
    top->eval();
    const bool taken = top->pkt_ready_o != 0;
    cyc();
    if (top->decode_done_o) gLastErr = top->decode_error_o;
    if (taken) ++i;
  }
  top->pkt_valid_i = 0;
  for (int k = 0; k < 32; ++k) {
    cyc();
    if (top->decode_done_o) gLastErr = top->decode_error_o;
  }
}

// Run the chain until it goes quiet. BOUNDED, never `while (busy)`: an
// unbounded loop hangs on exactly the defect it exists to catch.
void settle(int budget = 60000) {
  for (int i = 0; i < budget; ++i) cyc();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  // =========================================================================
  // THE PAIRS THIS TEST USES, and each is one clause of the ruling's list.
  // =========================================================================
  // SET A and SET B differ in their INDEX. SET A2 is SET A's index at a
  // DIFFERENT GENERATION -- the residency case.
  const uint32_t kSetA  = handle32(0x0000A1, 0x11);
  const uint32_t kSetB  = handle32(0x0000B2, 0x11);
  const uint32_t kSetA2 = handle32(0x0000A1, 0x22);   // same index, new generation
  // UPPER HANDLE-INDEX BITS: an index that uses bits 23..16, which the old
  // low-sixteen reading threw away entirely.
  const uint32_t kSetHi = handle32(0xFE0001, 0x33);

  hardReset();

  // Two records in ONE set (ids 0 and 7), the SAME id in TWO sets, and the
  // upper-index set. Record 0 is a VALID INDEX under this ruling, not a
  // sentinel, so it is given a real record with its own fields.
  gMaterials[{kSetA, 0}] = MatRecord{true, 0, 1, 0x11, 0x21, 0};
  gMaterials[{kSetA, 7}] = MatRecord{true, 0, 2, 0x12, 0x22, 0};
  gMaterials[{kSetB, 7}] = MatRecord{true, 0, 3, 0x13, 0x23, 0};
  gMaterials[{kSetA2, 7}] = MatRecord{true, 0, 2, 0x12, 0x22, 0};
  gMaterials[{kSetHi, 1}] = MatRecord{true, 0, 4, 0x14, 0x24, 0};

  const std::vector<uint8_t> page = buildPage(3);
  loadMemory(page);
  publishPage(static_cast<uint32_t>(page.size()));

  check_eq(top->pb_pages_o, 1, "the page was ADOPTED (magic, version and extent)");
  check_eq(top->pb_denied_o, 0, "no MEM.GUARD denial while reading the header");

  // =========================================================================
  // 1. ALTERNATING PROCEDURAL DRAWS, five of them, one packet
  //
  //    program 1 every time, so the GEOMETRY is identical across all five and
  //    the only thing that differs is the material pair. A case that also
  //    varied the program could not say which of the two the chain carried.
  // =========================================================================
  struct Draw {
    uint32_t src;
    uint32_t set;
    uint16_t id;
    uint16_t tick;
    const char* what;
  };
  const Draw kDraws[] = {
      {0x0001, kSetA,  0, 0x0000, "set A record 0 -- ZERO IS A VALID INDEX"},
      {0x0002, kSetA,  7, 0x1234, "set A record 7 -- TWO RECORDS IN ONE SET"},
      {0x0003, kSetB,  7, 0x0000, "set B record 7 -- THE SAME ID IN TWO SETS"},
      {0x0004, kSetHi, 1, 0x0000, "the upper-index set -- bits 23..16 used"},
      {0x0005, kSetA2, 7, 0x0000, "set A at a NEW GENERATION, same record id"},
  };

  // THREE THEN TWO, AND THE SPLIT IS A REAL CONSOLE LIMIT RATHER THAN A BENCH
  // CONVENIENCE. `zhao_cmd_exec`'s FORGE_Q is 4 and its forge records are
  // published WHOLE at the packet's verdict -- nothing leaves the queue until
  // the packet commits -- so a packet carrying FIVE procedural draws overflows
  // and is refused ENTIRELY, with `forge_overflow_o` counting it. That was
  // measured here, not assumed: the first version of this test issued all five
  // in one packet and read `forges_issued_o == 0`. The limit is recorded in
  // FINDINGS; this test issues 3 + 2, which also makes the draws ALTERNATE
  // ACROSS A PACKET BOUNDARY, a harder case than within one.
  {
    zhao::ZhaoFrameBuilder b;
    for (int i = 0; i < 3; ++i) {
      const Draw& d = kDraws[i];
      b.append_record(drawProceduralRecord(d.src, handle32(1, 0x05), d.set, d.id, d.tick));
    }
    pushPacket(b.seal(1, 1, 1));
  }
  check_eq(gLastErr, 0, "the first procedural packet VALIDATED (ZH_ABI_OK)");
  settle();
  rearmDecoder();
  {
    zhao::ZhaoFrameBuilder b;
    for (int i = 3; i < 5; ++i) {
      const Draw& d = kDraws[i];
      b.append_record(drawProceduralRecord(d.src, handle32(1, 0x05), d.set, d.id, d.tick));
    }
    pushPacket(b.seal(2, 2, 1));
  }
  check_eq(gLastErr, 0, "the second procedural packet VALIDATED (ZH_ABI_OK)");
  settle();

  check_eq(top->cmd_forges_issued_o, 5, "CMD.EXEC issued all five forge records");
  check_eq(top->cmd_unsupported_o, 0, "none fell through to `unsupported`");
  check_eq(top->cmd_forge_overflow_o, 0, "the forge queue did not overflow");
  check_eq(top->pb_draws_o, 5, "the page bank answered all five draws");
  check_eq(top->pb_lookup_miss_o, 0, "every draw found its program record");
  check_eq(top->pb_refused_kind_o, 0, "the forge_kind ROTATION agreed with the page");
  check_eq(top->pb_bad_record_o, 0, "no record was refused as illegal");

  // ---- the executor decoded BOTH same-bytes allocations -------------------
  check_eq(gDecoded.size(), 5, "five records reached the page bank's handshake");
  if (gDecoded.size() == 5) {
    for (size_t i = 0; i < 5; ++i) {
      check_eq(gDecoded[i].material_set, kDraws[i].set,
               "CMD.EXEC decoded the COMPLETE 32-bit set handle");
      check_eq(gDecoded[i].material_id, kDraws[i].id,
               "CMD.EXEC decoded material_id from ITS OWN BYTES");
      // R241 D-TICK-A, whose behaviour this ruling requires PRESERVED. Draw 2
      // carries a nonzero tick; under the old pad list that record would have
      // been REFUSED at validation as a nonzero mandatory-zero pad, so this
      // check also witnesses that the declaration fixed that.
      check_eq(gDecoded[i].frame_tick, kDraws[i].tick,
               "R241's frame_tick still decodes, from the same two bytes");
    }
    // THE FORBIDDEN READING, stated as a number. Draw 1's set handle is
    // 0x0000A111, whose low sixteen bits are 0xA111 -- the old law's record id.
    check(gDecoded[0].material_id != static_cast<uint16_t>(kDraws[0].set & 0xFFFFu),
          "the id is NOT the set handle's low half (the forbidden double read)");
  }

  // ---- the measurement: what the window ASKED FOR -------------------------
  // The window resolves per SPAN, and every consecutive draw here declares a
  // different pair, so there is one ask per draw and they are in draw order.
  check_eq(gAsks.size(), 5, "the window asked its resolver once per draw");
  if (gAsks.size() == 5) {
    for (size_t i = 0; i < 5; ++i) {
      check_eq(gAsks[i].set, kDraws[i].set, kDraws[i].what);
      check_eq(gAsks[i].id, kDraws[i].id, "... and the RECORD ID it declared");
    }
    // THE GENERATION CASE, spelled out. Draws 1 and 4 are the same set INDEX
    // at generations 0x11 and 0x22 with the same declared record id.
    check_eq(gAsks[1].id, gAsks[4].id,
             "A GENERATION BUMP DOES NOT CHANGE THE SELECTED RECORD");
    check(gAsks[1].set != gAsks[4].set,
          "... while the SET HANDLE it asks with is the new generation's");
    // Under the reading this packet removed the ids would have been the two
    // handles' low halves -- 0xA111 and 0xA122 -- i.e. DIFFERENT RECORDS for
    // the same material at two residency generations.
    check(static_cast<uint16_t>(kSetA & 0xFFFFu) !=
              static_cast<uint16_t>(kSetA2 & 0xFFFFu),
          "and the OLD reading would have differed -- 0xA111 vs 0xA122");
    // The upper-index case: the old reading kept only bits 15..0 of the
    // handle, so a set differing only above bit 15 was indistinguishable.
    check((kSetHi & 0xFFFFu) == (handle32(0x000001, 0x33) & 0xFFFFu),
          "the upper-index set is INVISIBLE to a low-16 reading of the handle");
    check_eq(gAsks[3].set, kSetHi, "... and the window got the WHOLE handle");
  }

  // ---- the correct material REACHED THE RASTER PATH -----------------------
  check(!gEntered.empty(), "triangles entered GEOM.CLIP");
  check_eq(top->geom_untex_refused_o, 0,
           "no forge triangle was refused: every material is non-sampling");
  check_eq(top->mw_no_record_o, 0, "every pair RESOLVED to a record");
  check_eq(top->mw_mode_refused_o, 0, "no contradictory material declaration");
  check_eq(top->mw_err_unpublished_o, 0, "nothing entered before a publication");
  check_eq(top->mw_err_underflow_o, 0, "the window's occupancy never underflowed");
  check_eq(top->asm_mat_skew_o, 0,
           "mat_skew_o is PUT: no primitive's material moved under its triangles");
  check_eq(top->pb_denied_o, 0, "no MEM.GUARD denial across five draws");

  // EVERY beat that entered carried a pair THIS TEST DECLARED, and the pairs
  // arrive in draw order. A chain that dropped the id would have entered
  // beats with record 0 throughout and every counter would still balance.
  {
    size_t di = 0;
    bool order_ok = true;
    bool pair_ok = true;
    uint32_t last_set = 0xFFFFFFFFu;
    uint16_t last_id = 0xFFFFu;
    for (const Entered& e : gEntered) {
      if (gMaterials.find({e.set, e.id}) == gMaterials.end()) pair_ok = false;
      if (e.set != last_set || e.id != last_id) {
        // a new span: it must be the NEXT draw's pair, never a repeat or a skip
        if (di >= 5 || e.set != kDraws[di].set || e.id != kDraws[di].id) order_ok = false;
        ++di;
        last_set = e.set;
        last_id = e.id;
      }
    }
    check(pair_ok, "every entered beat carried a pair this test actually declared");
    check(order_ok, "the spans arrive in DRAW ORDER, one per draw");
    check_eq(di, 5, "five spans entered -- a later draw did not absorb an earlier one");
  }

  const uint32_t kEnteredClean = static_cast<uint32_t>(gEntered.size());
  check(kEnteredClean > 0, "the clean run produced a triangle count to compare");

  // =========================================================================
  // 2. BACKPRESSURE -- the same five draws, with GEOM.CLIP's ready pulsed and
  //    the page memory stalled. The ruling names backpressure and the only
  //    honest place to apply it is where the chain has a join to get wrong.
  //
  //    THE ASSERTION IS THAT NOTHING CHANGES. Same spans, same order, same
  //    pairs, same triangle count, `mat_skew_o` still put.
  // =========================================================================
  hardReset();
  loadMemory(page);
  publishPage(static_cast<uint32_t>(page.size()));

  for (int part = 0; part < 2; ++part) {
    if (part == 1) rearmDecoder();
    zhao::ZhaoFrameBuilder b;
    const int lo = (part == 0) ? 0 : 3;
    const int hi = (part == 0) ? 3 : 5;
    for (int k = lo; k < hi; ++k) {
      const Draw& d = kDraws[k];
      b.append_record(drawProceduralRecord(d.src, handle32(1, 0x05), d.set, d.id, d.tick));
    }
    const std::vector<uint8_t> pkt = b.seal(static_cast<uint32_t>(part + 1),
                                            static_cast<uint32_t>(part + 1), 1);
    size_t i = 0;
    int phase = 0;
    top->pkt_len_i = static_cast<uint32_t>(pkt.size());
    for (int guard = 0; guard < 800000 && i < pkt.size(); ++guard) {
      top->pkt_valid_i = 1;
      top->pkt_byte_i = pkt[i];
      top->cl_in_ready_i = ((phase % 5) < 2) ? 1 : 0;
      top->mem_stall_i = ((phase % 7) == 0) ? 1 : 0;
      ++phase;
      top->eval();
      const bool taken = top->pkt_ready_o != 0;
      cyc();
      if (top->decode_done_o) gLastErr = top->decode_error_o;
      if (taken) ++i;
    }
    top->pkt_valid_i = 0;
    // Keep stalling while the chain drains, then release so it can finish.
    for (int i2 = 0; i2 < 40000; ++i2) {
      top->cl_in_ready_i = ((i2 % 5) < 2) ? 1 : 0;
      top->mem_stall_i = ((i2 % 7) == 0) ? 1 : 0;
      cyc();
    }
    top->cl_in_ready_i = 1;
    top->mem_stall_i = 0;
    settle();
  }

  check_eq(gLastErr, 0, "the packet validated under backpressure too");
  check_eq(top->cmd_forges_issued_o, 5, "all five records still issued");
  check_eq(top->pb_draws_o, 5, "the page bank still answered all five");
  check_eq(top->pb_denied_o, 0, "a STALL is not a DENIAL -- the responder held ready low");
  check_eq(top->asm_mat_skew_o, 0,
           "mat_skew_o STILL PUT under backpressure -- the join has no cadence to skew");
  check_eq(top->mw_err_underflow_o, 0, "the window's drain law held under backpressure");
  check_eq(gAsks.size(), 5, "the same five asks, under backpressure");
  if (gAsks.size() == 5) {
    bool same = true;
    for (size_t i = 0; i < 5; ++i) {
      if (gAsks[i].set != kDraws[i].set || gAsks[i].id != kDraws[i].id) same = false;
    }
    check(same, "and each ask is the SAME pair as the clean run");
  }
  check_eq(static_cast<uint32_t>(gEntered.size()), kEnteredClean,
           "the SAME number of triangles reached GEOM.CLIP as without backpressure");

  // =========================================================================
  // 3. THE FAILURE CASES
  //
  //    (a) R197 IS NOT WEAKENED. A forge primitive is untextured BY LAW, so a
  //        material that SAMPLES is the incompatible combination, and it must
  //        still be refused and counted -- "an untextured mesh with a sampling
  //        material does not become legal merely because particles now work".
  //    (b) A PAIR THAT RESOLVES TO NOTHING is counted, not substituted. The
  //        ruling: "Do not silently substitute an unrelated set or record when
  //        resolution fails."
  // =========================================================================
  hardReset();
  loadMemory(page);
  publishPage(static_cast<uint32_t>(page.size()));

  const uint32_t kSetTex = handle32(0x00C0DE, 0x44);
  gMaterials[{kSetTex, 3}] = MatRecord{true, 2, 5, 0x15, 0x25, 0x01};  // SAMPLES
  // (set A, record 9) is deliberately absent from the model.

  {
    zhao::ZhaoFrameBuilder b;
    b.append_record(drawProceduralRecord(0x0011, handle32(1, 0x05), kSetTex, 3, 0));
    b.append_record(drawProceduralRecord(0x0012, handle32(1, 0x05), kSetA, 9, 0));
    b.append_record(drawProceduralRecord(0x0013, handle32(1, 0x05), kSetA, 0, 0));
    pushPacket(b.seal(2, 2, 1));
  }
  settle();

  check_eq(gLastErr, 0, "the failure-case packet still VALIDATED at the ABI");
  check(top->geom_untex_refused_o > 0,
        "R197 STILL REFUSES an untextured primitive under a SAMPLING material");
  check(top->mw_no_record_o > 0,
        "a pair that resolves to no record is COUNTED, not substituted");
  check_eq(top->asm_mat_skew_o, 0, "and no material moved under a primitive");
  {
    bool saw_tex = false, saw_missing = false, saw_good = false;
    for (const Ask& a : gAsks) {
      if (a.set == kSetTex && a.id == 3) saw_tex = true;
      if (a.set == kSetA && a.id == 9) saw_missing = true;
      if (a.set == kSetA && a.id == 0) saw_good = true;
    }
    check(saw_tex, "the sampling material's OWN pair reached the resolver");
    check(saw_missing, "the missing record's OWN pair reached the resolver");
    check(saw_good, "and the third draw's pair was asked for unchanged");
    // NOT SUBSTITUTED: nothing entered GEOM.CLIP carrying a pair the draws did
    // not name, and in particular the refused draw did not enter under the
    // previous span's material.
    bool clean = true;
    for (const Entered& e : gEntered) {
      const bool named = (e.set == kSetTex && e.id == 3) ||
                         (e.set == kSetA && e.id == 9) ||
                         (e.set == kSetA && e.id == 0);
      if (!named) clean = false;
      // The sampling material's triangles must NOT have entered at all.
      if (e.set == kSetTex) clean = false;
    }
    check(clean, "no beat entered under a pair its draw did not declare");
  }

  // =========================================================================
  // 4. A DRAW WHOSE PROGRAM IS NOT IN THE PAGE -- the miss is counted and the
  //    material machinery is NOT consulted for it. A bank that answered from
  //    the previous record would put draw N-1's primitive under draw N's pair.
  // =========================================================================
  hardReset();
  loadMemory(page);
  publishPage(static_cast<uint32_t>(page.size()));
  {
    zhao::ZhaoFrameBuilder b;
    b.append_record(drawProceduralRecord(0x0021, handle32(0x7777, 0x05), kSetA, 7, 0));
    pushPacket(b.seal(3, 3, 1));
  }
  settle();
  check_eq(top->pb_lookup_miss_o, 1, "a program the page does not hold MISSES, counted");
  check_eq(top->asm_jobs_o, 0, "and no primitive was assembled for it");
  check_eq(gAsks.size(), 0, "the resolver was never asked -- nothing reached the window");
  check_eq(gEntered.size(), 0, "and nothing entered GEOM.CLIP");

  std::printf("[procmat_acceptance] %d checks, %d failures\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
