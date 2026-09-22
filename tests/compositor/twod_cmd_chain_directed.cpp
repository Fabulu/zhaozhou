// twod_cmd_chain_directed.cpp -- THE RULING'S ACCEPTANCE TEST.
//
// ===========================================================================
// WHAT THE OWNER ASKED FOR, VERBATIM
// ===========================================================================
// Completion ruling 2026-09-22, item 3:
//
//   "A command-fed two-player HUD, text/glyph sprites, overlapping ordered
//    sprites, legal planes, a disabled/empty next frame, malformed descriptors
//    and whole-sprite overflow. EXERCISE ACTUAL ASSET READS AND FINAL
//    COMPOSITED PIXELS WITHOUT DESCRIPTOR OR TEXEL INJECTION AT A DOWNSTREAM
//    BOUNDARY."
//
// and, from the completion-report section:
//
//   "An otherwise green smoke whose upstream fixture never reaches the new path
//    does not prove the path."
//
// ===========================================================================
// SO THIS DRIVER TOUCHES EXACTLY THREE THINGS
// ===========================================================================
//   1. the PACKET BYTE STREAM -- every descriptor and every asset request is a
//      record packed by the GENERATED packer from `spec/commands.zidl`;
//   2. the HPS BRIDGE -- every texel is a byte in an arena the loader reads;
//   3. the COMPOSITED PIXELS out of POST.COMPOSITE.
//
// It does not write a descriptor port, a page-store word, a binding, a palette
// entry or a HUD colour. Those are the downstream boundaries the ruling names,
// and the whole point of the file is that nothing reaches them except through
// the chain.
//
// ===========================================================================
// THE EVIDENCE IS A BEFORE/AFTER PAIR, AND THAT IS DELIBERATE
// ===========================================================================
// Every pixel case renders the SAME frame twice -- once with no HUD and once
// with it -- and asserts the exact SET of pixels that changed. A test that only
// asserted "the sprite's pixels are the glyph's colour" would pass while the
// HUD also overwrote half the screen, and this repository's own law is that a
// regression is only visible in a before/after pair. The unchanged half is the
// half that catches a band writing outside its rectangle.
// ===========================================================================
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_twod_chain.h"

#include "zhao_abi.h"
#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"
#include "zref/zref_twod.hpp"

namespace {

using zhao::check;

// The wrapper's parameters, restated so a change on either side is a test
// failure rather than a silent disagreement.
constexpr int kW        = 64;
constexpr int kH        = 16;
constexpr int kMaxDesc  = 16;
constexpr uint32_t kPageWords = 1024;
constexpr uint32_t kPalSlots  = 4;
constexpr uint32_t kPageSlots = 8;
constexpr uint32_t kSlotWords = kPageWords / kPageSlots;   // 128
constexpr uint16_t kEpoch     = 0x2A2Au;
constexpr uint32_t kArenaBase = 0x1000'0000u;

constexpr uint16_t kWorld = 0x0842;   // the resolved world's flat colour

Vtb_zhao_twod_chain* top = nullptr;

// ---- the HPS arena, and a bridge that behaves like the real one -----------
// THE BRIDGE DELIVERS AT FULL RATE, BACK TO BACK, because the real one does:
// one 64-bit beat per cycle once granted, and `err` only at the request. A
// model that politely waited for the consumer would have let a loader that
// drained while the beats landed pass -- and that loader would drop three
// beats in four with every counter still balancing.
std::vector<uint8_t> gArena;
bool     gBursting  = false;
uint32_t gBurstAddr = 0;
int      gBeat      = 0;
int      gGrantWait = 0;
int      gGrantDelay = 0;
int      gErrOnce    = 0;

void driveBridge() {
  top->hps_grant_i      = 0;
  top->hps_beat_valid_i = 0;
  top->hps_data_i       = 0;
  top->hps_last_i       = 0;
  top->hps_err_i        = 0;

  if (!gBursting) {
    if (top->hps_req_valid_o) {
      if (gErrOnce > 0) { --gErrOnce; top->hps_err_i = 1; return; }
      if (gGrantWait < gGrantDelay) { ++gGrantWait; return; }
      gGrantWait = 0;
      gBursting  = true;
      gBurstAddr = top->hps_req_addr_o;
      gBeat      = 0;
      top->hps_grant_i = 1;
    }
    return;
  }

  uint64_t d = 0;
  const uint32_t off = gBurstAddr - kArenaBase + static_cast<uint32_t>(gBeat) * 8u;
  for (int b = 0; b < 8; ++b) {
    const uint32_t i = off + static_cast<uint32_t>(b);
    const uint64_t byte = (i < gArena.size()) ? gArena[i] : 0u;
    d |= byte << (8 * b);
  }
  top->hps_beat_valid_i = 1;
  top->hps_data_i       = d;
  top->hps_last_i       = (gBeat == 7) ? 1 : 0;
  ++gBeat;
  if (gBeat == 8) gBursting = false;
}

void cyc() {
  driveBridge();
  top->eval();
  top->clk = 1; top->eval();
  top->clk = 0; top->eval();
}

void idleInputs() {
  top->pkt_valid_i = 0;
  top->pkt_byte_i  = 0;
  top->pkt_len_i   = 0;
  top->frame_start_i = 0;
  top->frame_w_i = kW;
  top->frame_h_i = kH;
  top->view_split_i = 0;
  top->view_sel_i = 0;
  top->cfg_epoch_i = kEpoch;
  top->s_valid_i = 0;
  top->s_rgb_i = 0;
  top->o_ready_i = 1;
  top->hps_grant_i = 0;
  top->hps_beat_valid_i = 0;
  top->hps_data_i = 0;
  top->hps_last_i = 0;
  top->hps_err_i = 0;
  top->dec_rst_n = 1;
}

// Re-arm CMD.DECODER for a second packet WITHOUT emptying the page store: its
// S_DONE holds the verdict until reset, and a full reset would undo the asset
// load this frame's descriptors name. See the wrapper's `dec_rst_n` comment.
void rearmDecoder() {
  top->dec_rst_n = 0;
  for (int i = 0; i < 4; ++i) cyc();
  top->dec_rst_n = 1;
  for (int i = 0; i < 2; ++i) cyc();
}

void hardReset() {
  delete top;
  top = new Vtb_zhao_twod_chain;
  gBursting = false; gBeat = 0; gGrantWait = 0; gGrantDelay = 0; gErrOnce = 0;
  idleInputs();
  top->rst_n = 0;
  top->clk = 0;
  for (int i = 0; i < 4; ++i) { top->eval(); top->clk = 1; top->eval(); top->clk = 0; top->eval(); }
  top->rst_n = 1;
  for (int i = 0; i < 4; ++i) cyc();
}

// ---- the packet stream ----------------------------------------------------
// CMD.DECODER is one-shot: its S_DONE holds the verdict until reset, so ONE
// packet per device. That is a property of the block and not of this bench,
// and it is why each case below builds a fresh chain.
uint8_t gLastErr = 0xFF;
uint32_t gLastCommands = 0;

void pushPacket(const std::vector<uint8_t>& pkt) {
  size_t i = 0;
  gLastErr = 0xFFu;
  gLastCommands = 0;
  top->pkt_len_i = static_cast<uint32_t>(pkt.size());
  for (int guard = 0; guard < 400000 && i < pkt.size(); ++guard) {
    top->pkt_valid_i = 1;
    top->pkt_byte_i  = pkt[i];
    driveBridge();
    top->eval();
    const bool taken = top->pkt_ready_o != 0;
    top->clk = 1; top->eval();
    top->clk = 0; top->eval();
    // `decode_done_o` IS A ONE-CYCLE PULSE and it can land on the packet's own
    // last byte. The first version of this driver only watched for it in the
    // tail loop below and read 0xFF -- an UNOBSERVED verdict, which looks
    // exactly like a refused packet and is not one.
    if (top->decode_done_o) {
      gLastErr = top->decode_error_o;
      gLastCommands = top->decode_commands_o;
    }
    if (taken) ++i;
  }
  top->pkt_valid_i = 0;
  // Let the verdict land and the pending upload queue drain into TWOD.ASSET.
  for (int k = 0; k < 64; ++k) {
    cyc();
    if (top->decode_done_o) { gLastErr = top->decode_error_o; gLastCommands = top->decode_commands_o; }
  }
}

// Wait for every asset transfer to finish. BOUNDED, not a `while (busy)`: a
// loop with no bound hangs on exactly the defect it is there to catch.
void drainLoads(int budget = 20000) {
  for (int i = 0; i < budget; ++i) cyc();
}

// ---- a frame -------------------------------------------------------------
struct Pass {
  std::vector<uint16_t> rgb;
  std::vector<uint8_t>  seen;
  int  emitted = 0;
  bool completed = false;
  Pass() : rgb(static_cast<size_t>(kW) * kH, 0), seen(static_cast<size_t>(kW) * kH, 0) {}
};

// The frame TICK, then the compositor's pass. The tick is what seals the
// descriptor list -- through the band's `list_restart_o`, not through this
// signal directly -- and the pass is what reads it.
constexpr int kPreSweep = 4000;

Pass runFrame(uint16_t world) {
  Pass p;
  top->s_valid_i = 0;
  top->o_ready_i = 1;
  top->frame_start_i = 1;
  cyc();
  top->frame_start_i = 0;

  // THE FILLER NEEDS A LEAD, AND THE CONSOLE GIVES IT ONE FOR FREE. The tick
  // is a fact about the FRAME; the compositor's pass does not begin until
  // render and resolve are done, which is most of a frame. `zhao_twod_band`'s
  // own header says what happens without that lead -- "it has to fill band 0
  // while the reader crosses it" -- and the first version of this bench waited
  // only for the publish walk (132 clocks) and then swept immediately. Every
  // sprite in the TOP BANDS then drew nothing at all, because the reader
  // reached them before the filler did. That is the band behaving exactly as
  // documented and a bench measuring a condition the console never has.
  for (int i = 0; i < kPreSweep; ++i) cyc();

  const int kPix = kW * kH;
  int in_idx = 0;
  for (int c = 0; c < 400000 && !p.completed; ++c) {
    top->s_valid_i = (in_idx < kPix) ? 1 : 0;
    top->s_rgb_i   = world;
    // `o_ready_i` IS HELD HIGH, AND THAT IS A DELIBERATE CHOICE WITH A COST.
    // POST.COMPOSITE's HUD port is a one-cycle-latency random access whose
    // ADDRESS ADVANCES WITH THE PIPE, so the alignment between a request and
    // the pixel that consumes it is only exact when the pipe does not stall.
    // Throttling `o_ready_i` here moved the whole HUD one pixel the OTHER way,
    // which says the held-address case is a seam worth a directed test of its
    // own and NOT that either block is wrong -- `post_composite_dev.hpp`'s own
    // driver re-presents the response only on a step, and nothing in the
    // composition does that for the band. Recorded in FINDINGS rather than
    // papered over, and left at one pixel per clock here so this file measures
    // the descriptor path rather than that seam.
    top->o_ready_i = 1;
    driveBridge();
    top->eval();
    const bool accepted_in  = top->s_valid_i && top->s_ready_o;
    const bool accepted_out = top->o_valid_o && top->o_ready_i;
    if (accepted_out) {
      const int ox = top->o_x_o, oy = top->o_y_o;
      if (ox >= 0 && ox < kW && oy >= 0 && oy < kH) {
        const size_t idx = static_cast<size_t>(oy) * kW + ox;
        p.rgb[idx] = top->o_rgb_o;
        p.seen[idx] = 1;
      }
      ++p.emitted;
      if (top->o_last_o) p.completed = true;
    }
    top->clk = 1; top->eval();
    top->clk = 0; top->eval();
    if (accepted_in) ++in_idx;
  }
  top->s_valid_i = 0;
  for (int i = 0; i < 16; ++i) cyc();
  return p;
}

// ---- the records ----------------------------------------------------------
std::vector<uint8_t> setPlaneRecord(uint32_t src, int slot, int role, int blend,
                                    int opacity, int fmt, int wrap, int vm, int pal,
                                    int width, int height, int flags, int base,
                                    int lstride, int lheight,
                                    int32_t a = 0x10000, int32_t b = 0,
                                    int32_t c = 0, int32_t d = 0x10000,
                                    int32_t u0 = 0, int32_t v0 = 0,
                                    int32_t line_scroll = 0) {
  zhao_abi::ZhRecordSetPlane r{};
  r.hdr.opcode = zhao_abi::ZHAO_OP_SET_PLANE;
  r.hdr.record_bytes = 64;
  r.hdr.source_id = src;
  r.payload.slot = static_cast<uint8_t>(slot);
  r.payload.role = static_cast<uint8_t>(role);
  r.payload.blend = static_cast<uint8_t>(blend);
  r.payload.opacity = static_cast<uint8_t>(opacity);
  r.payload.format = static_cast<uint8_t>(fmt);
  r.payload.wrap = static_cast<uint8_t>(wrap);
  r.payload.view_mask = static_cast<uint8_t>(vm);
  r.payload.palette_id = static_cast<uint8_t>(pal);
  r.payload.width = static_cast<uint16_t>(width);
  r.payload.height = static_cast<uint16_t>(height);
  r.payload.flags = static_cast<uint16_t>(flags);
  r.payload.base = static_cast<uint16_t>(base);
  r.payload.lstride = static_cast<uint8_t>(lstride);
  r.payload.lheight = static_cast<uint8_t>(lheight);
  r.payload.a = a; r.payload.b = b; r.payload.c = c; r.payload.d = d;
  r.payload.u0 = u0; r.payload.v0 = v0;
  r.payload.line_scroll = line_scroll;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_set_plane(r, out);
  return out;
}

std::vector<uint8_t> drawSpriteRecord(uint32_t src, int x, int y, int w, int h,
                                      int base, int lstride, int lheight,
                                      int fmt, int pal, int blend, int vm,
                                      uint16_t tint, int order, uint16_t src_id,
                                      int32_t u = 0, int32_t v = 0, int flags = 0) {
  zhao_abi::ZhRecordDrawSprite r{};
  r.hdr.opcode = zhao_abi::ZHAO_OP_DRAW_SPRITE;
  r.hdr.record_bytes = 64;
  r.hdr.source_id = src;
  r.payload.x = static_cast<int16_t>(x);
  r.payload.y = static_cast<int16_t>(y);
  r.payload.w = static_cast<uint16_t>(w);
  r.payload.h = static_cast<uint16_t>(h);
  r.payload.base = static_cast<uint16_t>(base);
  r.payload.lstride = static_cast<uint8_t>(lstride);
  r.payload.lheight = static_cast<uint8_t>(lheight);
  r.payload.format = static_cast<uint8_t>(fmt);
  r.payload.palette_id = static_cast<uint8_t>(pal);
  r.payload.blend = static_cast<uint8_t>(blend);
  r.payload.view_mask = static_cast<uint8_t>(vm);
  r.payload.tint.bits = tint;
  r.payload.order = static_cast<uint8_t>(order);
  r.payload.flags = static_cast<uint8_t>(flags);
  r.payload.src_id = src_id;
  r.payload.u = u; r.payload.v = v;
  r.payload.a00 = 0x10000; r.payload.a01 = 0;
  r.payload.a10 = 0;       r.payload.a11 = 0x10000;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_draw_sprite(r, out);
  return out;
}

std::vector<uint8_t> publishTwodPage(uint32_t src, uint32_t hps_addr, uint32_t len,
                                     uint32_t crc, uint16_t epoch, uint8_t dst_slot,
                                     uint8_t kind = 15) {
  zhao_abi::ZhRecordPublishResource r{};
  r.hdr.opcode = zhao_abi::ZHAO_OP_PUBLISH_RESOURCE;
  r.hdr.record_bytes = 48;
  r.hdr.source_id = src;
  r.payload.resource = 0x00'1234'00u | 0x11u;
  r.payload.hps_addr_lo = hps_addr;
  r.payload.hps_addr_hi = 0;
  r.payload.vram_dst = 0;
  r.payload.length = len;
  r.payload.crc32c = crc;
  r.payload.new_generation = 1;
  r.payload.epoch = epoch;
  r.payload.dst_slot = dst_slot;
  r.payload.kind = kind;
  std::vector<uint8_t> out;
  zhao_abi::zhao_pack_publish_resource(r, out);
  return out;
}

// ---- the glyph sheet ------------------------------------------------------
// An 8x8 RGB565 page whose every texel is a DIFFERENT colour, so a sprite that
// lands in the right place with the wrong row origin -- the defect a uniform
// sheet cannot see -- reads as a wrong colour rather than as a pass.
constexpr uint16_t kGlyphW = 8, kGlyphH = 8;
uint16_t glyphTexel(int u, int v) {
  return static_cast<uint16_t>(0x8000u | (static_cast<uint16_t>(v) << 6) | (u + 1));
}

// A 64-byte-aligned body: 64 words = 128 bytes, exactly two bursts.
std::vector<uint8_t> glyphPageBytes() {
  std::vector<uint8_t> b;
  for (int v = 0; v < kGlyphH; ++v)
    for (int u = 0; u < kGlyphW; ++u) {
      const uint16_t t = glyphTexel(u, v);
      b.push_back(static_cast<uint8_t>(t & 0xFF));
      b.push_back(static_cast<uint8_t>(t >> 8));
    }
  return b;
}

void stageArena(const std::vector<uint8_t>& body) {
  gArena.assign(body.begin(), body.end());
  gArena.resize(((gArena.size() + 63u) / 64u) * 64u, 0u);
}

uint32_t crcOf(const std::vector<uint8_t>& b) {
  return zhao_abi::zhao_crc32c(0, b.data(), b.size());
}

// ---- comparison helpers ---------------------------------------------------
struct DiffSet {
  std::vector<std::pair<int,int>> at;
  bool allSeen = true;
};

DiffSet diffPasses(const Pass& a, const Pass& b) {
  DiffSet d;
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      const size_t i = static_cast<size_t>(y) * kW + x;
      if (!a.seen[i] || !b.seen[i]) d.allSeen = false;
      if (a.rgb[i] != b.rgb[i]) d.at.emplace_back(x, y);
    }
  return d;
}

// EVERY COUNTER IN THE CHAIN, PRINTED ONLY WHEN A PIXEL CHECK FAILS. It exists
// because the first eleven failures in this file were all the same shape: the
// pixels were wrong and no counter in the composition said anything. A pixel
// mismatch and a counter dump together name the block; either alone names the
// symptom. The band's own `fill_band_q`, `rd_band_q`, `st_q` and
// `outstanding_q` are HIERARCHICAL READS from the wrapper -- no production
// port, nothing the design can see -- and they are what identified a scan
// stuck in S_CLOSE waiting for a pixel that by contract would never arrive.
void dumpCounters(const char* tag) {
  std::printf("  [%s] staged p/s=%u/%u pub p/s=%u/%u refused p/s=%u/%u ovf=%u "
              "autodis=%u bindconf=%u | band descr=%u adm=%u refbud=%u ovf=%u "
              "written=%u under=%u midsweep=%u ordinv=%u | plane pix=%u "
              "refrole=%u refblend=%u dis=%u | sprite ref=%u skipview=%u | smp "
              "bindmiss=%u pageoob=%u fmtref=%u palref=%u clut=%u 565=%u\n",
              tag,
              top->tc_planes_staged_o, top->tc_sprites_staged_o,
              top->tc_planes_published_o, top->tc_sprites_published_o,
              top->tc_plane_refused_o, top->tc_sprite_refused_o,
              top->tc_list_overflow_o, top->tc_slots_auto_disabled_o,
              top->tc_bind_conflict_o,
              top->band_descriptors_o, top->band_sprites_admitted_o,
              top->band_sprites_refused_budget_o, top->band_desc_overflow_o,
              top->band_pixels_written_o, top->band_underrun_o,
              top->band_desc_mid_sweep_o, top->band_order_inversion_o,
              top->plane_pixels_o, top->plane_refused_role_o,
              top->plane_refused_blend_o, top->plane_disabled_o,
              top->sprite_refused_o, top->sprite_skipped_view_o,
              top->smp_bind_missing_o, top->smp_page_oob_o,
              top->smp_fmt_refused_o, top->smp_pal_refused_o,
              top->smp_clut8_samples_o, top->smp_rgb565_samples_o);
  std::printf("      bands=%u busy=%u fill=%u rd=%u scan_st=%u outstanding=%u |",
              top->band_bands_o, top->tc_publishing_o, top->dbg_fill_band_o,
              top->dbg_rd_band_o, top->dbg_scan_st_o, top->dbg_outstanding_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const std::vector<uint8_t> page = glyphPageBytes();
  const uint32_t pageCrc = crcOf(page);

  // =========================================================================
  // 1. A COMMAND-FED GLYPH SPRITE, FROM AN ARENA BYTE TO A COMPOSITED PIXEL
  // =========================================================================
  // The whole chain in one case. Nothing is injected: the texels are bytes in
  // `gArena`, the page reaches the store because a PublishResource of kind 15
  // named it, and the sprite reaches the screen because a DrawSprite record
  // said where.
  {
    hardReset();
    stageArena(page);

    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, /*dst_slot=*/0));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_OK, "1: the publish packet is well formed",
          zhao_abi::ZH_ABI_OK, gLastErr);
    check(top->ex_twod_loads_issued_o == 1,
          "1: CMD.EXEC forked the kind-15 upload to TWOD.ASSET", 1,
          top->ex_twod_loads_issued_o);
    check(top->ex_uploads_issued_o == 0,
          "1: and MEM.UPLOAD saw none of it -- one head, two destinations", 0,
          top->ex_uploads_issued_o);
    check(top->ta_loads_done_o == 1, "1: the load completed with a clean CRC", 1,
          top->ta_loads_done_o);
    check(top->ta_crc_fails_o == 0, "1: no CRC failure", 0, top->ta_crc_fails_o);
    check(top->ta_words_written_o == 64,
          "1: sixty-four texel words reached the page store", 64,
          top->ta_words_written_o);
    check(top->ta_bursts_o == 2, "1: two 64-byte bursts", 2, top->ta_bursts_o);

    // Frame A: no descriptors. The BEFORE half of the pair.
    const Pass before = runFrame(kWorld);
    check(before.completed, "1: the empty frame completed", 1, before.completed ? 1 : 0);
    check(before.emitted == kW * kH, "1: every pixel came out once", kW * kH, before.emitted);
    int nonWorld = 0;
    for (int i = 0; i < kW * kH; ++i) if (before.rgb[i] != kWorld) ++nonWorld;
    check(nonWorld == 0, "1: with no HUD the world passes through untouched", 0, nonWorld);

    // Frame B: one 8x8 glyph at (16, 8), and nothing else changes.
    zhao::ZhaoFrameBuilder b2;
    b2.begin_frame(2, 0, 0, 0);
    b2.append_record(drawSpriteRecord(0x200, 16, 4, kGlyphW, kGlyphH,
                                      /*base=*/0, /*lstride=*/3, /*lheight=*/3,
                                      /*fmt=*/1, /*pal=*/0, /*blend=*/0, /*vm=*/1,
                                      0xFFFF, /*order=*/0, /*src_id=*/0));
    b2.end_frame(0);
    // A SECOND packet needs a second decoder, and the decoder is one-shot. The
    // chain is therefore rebuilt -- but the PAGE STORE is rebuilt with it, so
    // the asset is published again from the same arena. That is the honest
    // shape: in the console the page outlives the frame; here the bench pays
    // for the decoder's one-shot verdict by re-publishing, and re-publishing
    // exercises the same real path a second time.
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b3;
    b3.begin_frame(1, 0, 0, 0);
    b3.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                     pageCrc, kEpoch, 0));
    b3.append_record(drawSpriteRecord(0x200, 16, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                      1, 0, 0, 1, 0xFFFF, 0, 0));
    b3.end_frame(0);
    pushPacket(b3.seal(1, 1, 0));
    drainLoads(4000);
    check(top->ta_loads_done_o == 1, "1: the page loaded again", 1, top->ta_loads_done_o);
    check(top->tc_sprites_staged_o == 1, "1: the DrawSprite reached TWOD.CMD", 1,
          top->tc_sprites_staged_o);

    const Pass after = runFrame(kWorld);
    check(after.completed, "1: the HUD frame completed", 1, after.completed ? 1 : 0);
    check(top->tc_sprites_published_o == 1, "1: it was published into the sealed list", 1,
          top->tc_sprites_published_o);
    check(top->band_descriptors_o == 1, "1: and the band took it", 1, top->band_descriptors_o);
    check(top->band_sprites_admitted_o == 1, "1: R235 admitted it", 1,
          top->band_sprites_admitted_o);
    check(top->band_sprites_refused_budget_o == 0, "1: and refused nothing", 0,
          top->band_sprites_refused_budget_o);
    check(top->smp_bind_missing_o == 0,
          "1: the binding was programmed before the first sample", 0, top->smp_bind_missing_o);
    check(top->smp_page_oob_o == 0, "1: no sample left the page store", 0, top->smp_page_oob_o);
    check(top->band_desc_mid_sweep_o == 0,
          "1: THE SEALED LIST WAS WRITTEN BEFORE THE PASS -- the ruling's "
          "'neither a later packet nor the next frame may mutate the list being "
          "consumed', measured", 0, top->band_desc_mid_sweep_o);

    // THE SET OF CHANGED PIXELS IS EXACTLY THE SPRITE'S RECTANGLE.
    const DiffSet d = diffPasses(before, after);
    check(d.allSeen, "1: both passes covered the frame", 1, d.allSeen ? 1 : 0);
    check(d.at.size() == static_cast<size_t>(kGlyphW * kGlyphH),
          "1: exactly 64 pixels changed", kGlyphW * kGlyphH,
          static_cast<uint64_t>(d.at.size()));
    int wrongPlace = 0, wrongColour = 0;
    for (const auto& xy : d.at) {
      const int u = xy.first - 16, v = xy.second - 4;
      if (u < 0 || u >= kGlyphW || v < 0 || v >= kGlyphH) { ++wrongPlace; continue; }
      const size_t i = static_cast<size_t>(xy.second) * kW + xy.first;
      if (after.rgb[i] != glyphTexel(u, v)) ++wrongColour;
    }
    check(wrongPlace == 0, "1: every changed pixel is inside the sprite", 0, wrongPlace);
    if (wrongPlace || wrongColour) {
      std::printf("  DBG1 changed:");
      int n = 0;
      for (const auto& xy : d.at) {
        if (n++ >= 80) break;
        const size_t i = static_cast<size_t>(xy.second) * kW + xy.first;
        std::printf(" (%d,%d)=%04X", xy.first, xy.second, after.rgb[i]);
      }
      std::printf(" | underrun=%u written=%u admitted=%u refused=%u descr=%u|",
                  top->band_underrun_o, top->band_pixels_written_o,
                  top->band_sprites_admitted_o,
                  top->band_sprites_refused_budget_o, top->band_descriptors_o);
    }
    check(wrongColour == 0,
          "1: and carries the ARENA BYTE for its own (u, v) -- the row origin "
          "is right in every row, which a uniform sheet could not have shown",
          0, wrongColour);
    check(top->smp_rgb565_samples_o == 64, "1: sixty-four RGB565 samples", 64,
          top->smp_rgb565_samples_o);
  }

  // =========================================================================
  // 2. A TWO-PLAYER HUD, TEXT AS GLYPH SPRITES, OVERLAPPING AND ORDERED
  // =========================================================================
  // The ruling's list, in one frame: both HUD regions (the Duo seam is a ROW,
  // so `view_split_i` splits the screen and each sprite's `view_mask` claims a
  // side), text as several glyph sprites, and two sprites that OVERLAP with
  // different `order`.
  {
    hardReset();
    stageArena(page);

    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    // THREE glyphs per player, not four, and the reason is the BENCH's narrow
    // line rather than the design's. R233's leaky bucket drains LINE_W pixels
    // per scanned line; at the console's 384 a HUD of eight 8-wide sprites is
    // 17% of rate, and at this bench's 64 it is ONE HUNDRED PERCENT -- exactly
    // at rate, with no margin for the walker's per-descriptor overhead, so the
    // filler falls behind and `band_underrun_o` fires. Six sprites is 75%,
    // which is a HUD and not a stress test; the stress case is case 6.
    for (int g = 0; g < 3; ++g)
      b.append_record(drawSpriteRecord(0x300u + g, 2 + g * 9, 0, kGlyphW, kGlyphH,
                                       0, 3, 3, 1, 0, 0, /*vm=*/1, 0xFFFF,
                                       /*order=*/10, /*src_id=*/0));
    // Player 2's row, below the seam, claimed by the OTHER view mask.
    for (int g = 0; g < 3; ++g)
      b.append_record(drawSpriteRecord(0x310u + g, 2 + g * 9, kH / 2, kGlyphW, kGlyphH,
                                       0, 3, 3, 1, 0, 0, /*vm=*/2, 0xFFFF,
                                       /*order=*/10, /*src_id=*/0));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_OK, "2: the packet is well formed",
          zhao_abi::ZH_ABI_OK, gLastErr);
    check(top->tc_sprites_staged_o == 6, "2: six glyph sprites staged", 6,
          top->tc_sprites_staged_o);

    top->view_split_i = kH / 2;
    const Pass p = runFrame(kWorld);
    check(p.completed, "2: the two-player frame completed", 1, p.completed ? 1 : 0);
    check(top->tc_sprites_published_o == 6, "2: all six published", 6,
          top->tc_sprites_published_o);

    int hi = 0, lo = 0, stray = 0;
    for (int y = 0; y < kH; ++y)
      for (int x = 0; x < kW; ++x) {
        const size_t i = static_cast<size_t>(y) * kW + x;
        if (p.rgb[i] == kWorld) continue;
        if (y < kH / 2) ++hi; else ++lo;
        // Every non-world pixel must be inside SOME glyph rectangle.
        bool inside = false;
        for (int g = 0; g < 3 && !inside; ++g) {
          const int gx = 2 + g * 9;
          if (x >= gx && x < gx + kGlyphW &&
              ((y >= 0 && y < kGlyphH) ||
               (y >= kH / 2 && y < kH / 2 + kGlyphH))) inside = true;
        }
        if (!inside) ++stray;
      }
    check(stray == 0, "2: no HUD pixel landed outside a glyph", 0, stray);
    check(hi == 3 * kGlyphW * kGlyphH, "2: player one's row drew 192 pixels",
          3 * kGlyphW * kGlyphH, hi);
    check(lo == 3 * kGlyphW * kGlyphH, "2: player two's row drew 192 pixels",
          3 * kGlyphW * kGlyphH, lo);
    if (lo != 3 * kGlyphW * kGlyphH) dumpCounters("2a");
    check(top->band_order_inversion_o == 0,
          "2: the list reached the band in `order`", 0, top->band_order_inversion_o);
    // THE BAND FILTERS BY VIEW, NOT THE WALKER, and that is worth asserting
    // rather than assuming: `zhao_twod_band`'s scan skips a descriptor whose
    // `view_mask` does not meet the band's own `fill_view_c` and NEVER CHARGES
    // it, so `zhao_twod_sprite.skipped_view_o` stays at zero in this
    // composition. A bench that asserted the walker's counter would have been
    // asserting that the band does NOT do its job.
    check(top->sprite_skipped_view_o == 0,
          "2: the BAND filtered by view, so the walker never had to", 0,
          top->sprite_skipped_view_o);

    // OVERLAP AND ORDER, in a second chain: two sprites on the same rectangle
    // with different `order`, and the higher one must win every pixel.
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b2;
    b2.begin_frame(1, 0, 0, 0);
    b2.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                     pageCrc, kEpoch, 0));
    // The LOWER order first, the higher second: the band composites by LAST
    // WRITE, so a producer that reordered the list would put the wrong sprite
    // on top and every counter would still balance.
    b2.append_record(drawSpriteRecord(0x400, 20, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                      1, 0, 0, 1, 0xFFFF, /*order=*/1, /*src_id=*/0));
    b2.append_record(drawSpriteRecord(0x401, 20, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                      1, 0, 0, 1, 0xFFFF, /*order=*/9, /*src_id=*/1,
                                      /*u=*/0x00010000, /*v=*/0x00010000));
    b2.end_frame(0);
    pushPacket(b2.seal(1, 1, 0));
    drainLoads(4000);
    top->view_split_i = 0;
    const Pass ov = runFrame(kWorld);
    check(ov.completed, "2: the overlap frame completed", 1, ov.completed ? 1 : 0);
    check(top->band_order_inversion_o == 0, "2: no order inversion", 0,
          top->band_order_inversion_o);
    // The second sprite samples at (u+1, v+1), so the top-left pixel must carry
    // texel (1, 1) and not texel (0, 0).
    const size_t tl = static_cast<size_t>(4) * kW + 20;
    check(ov.rgb[tl] == glyphTexel(1, 1),
          "2: the HIGHER-order sprite is the one on top, texel for texel",
          glyphTexel(1, 1), ov.rgb[tl]);
  }

  // =========================================================================
  // 3. A LEGAL PLANE, COMMAND-FED, COMPOSITED OVER THE WORLD
  // =========================================================================
  // `SetPlane` with role ATMOSPHERE and blend ALPHA at full opacity: the sheet
  // replaces the world everywhere it covers. The plane's texels come from the
  // SAME arena read -- role 1 binds slot 1, so the same page is bound twice and
  // `bind_conflict_o` must stay silent because both name the same region.
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    b.append_record(setPlaneRecord(0x500, /*slot=*/0, /*role=*/1, /*blend=*/1,
                                   /*opacity=*/255, /*fmt=*/1, /*wrap=*/0, /*vm=*/1,
                                   // WIDTH AND HEIGHT ARE THE SCREEN'S, NOT THE SHEET'S,
                                   // and that is TWOD.PLANE's own domain rather than a
                                   // trick: its repeat is ONE CONDITIONAL CORRECTION, valid
                                   // only while the coordinate leaves the range by at most a
                                   // step, and `wrap_fail_o` counts the violation. An 8-wide
                                   // `width` over a 64-wide screen is outside that domain.
                                   // The 8x8 TILING is the SAMPLER's mask (lstride/lheight),
                                   // which is a power-of-two mask and has no such limit.
                                   /*pal=*/0, /*width=*/64, /*height=*/16,
                                   /*flags=*/1, /*base=*/0, /*lstride=*/3, /*lheight=*/3));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_OK, "3: the plane packet is well formed",
          zhao_abi::ZH_ABI_OK, gLastErr);
    check(top->tc_planes_staged_o == 1, "3: the SetPlane reached TWOD.CMD", 1,
          top->tc_planes_staged_o);

    // TWO FRAMES, AND THE REASON IS THE SAMPLER'S OWN DECLARED BEHAVIOUR:
    // `atm_en_o` is "latched once per frame from whether the PREVIOUS frame
    // produced any atmosphere at all ... One frame of latency, stated rather
    // than hidden, and the alternative -- a mid-frame enable -- would put a
    // seam across the picture." So a sheet asked for in exactly one frame never
    // appears, and a game must re-send SetPlane every frame -- which is what
    // `TWOD.PLANE.md` says to do anyway.
    const Pass warm = runFrame(kWorld);
    check(warm.completed, "3: the warm-up frame completed", 1, warm.completed ? 1 : 0);
    rearmDecoder();
    zhao::ZhaoFrameBuilder b2;
    b2.begin_frame(2, 0, 0, 0);
    b2.append_record(setPlaneRecord(0x502, 0, 1, 1, 255, 1, 0, 1, 0, 64, 16, 1, 0, 3, 3));
    b2.end_frame(0);
    pushPacket(b2.seal(1, 1, 0));
    const Pass p = runFrame(kWorld);
    check(p.completed, "3: the plane frame completed", 1, p.completed ? 1 : 0);
    check(top->tc_planes_published_o == 2, "3: a plane published in each frame", 2,
          top->tc_planes_published_o);
    check(top->tc_slots_auto_disabled_o == 2,
          "3: and the slot neither frame named was DISABLED, on the lawful "
          "enable path, once per frame", 2, top->tc_slots_auto_disabled_o);
    check(top->plane_disabled_o == 2, "3: TWOD.PLANE counted them as disables", 2,
          top->plane_disabled_o);
    check(top->plane_refused_role_o + top->plane_refused_blend_o == 0,
          "3: and as NEITHER refusal", 0,
          top->plane_refused_role_o + top->plane_refused_blend_o);
    check(top->plane_pixels_o > 0, "3: the plane produced pixels", 1,
          top->plane_pixels_o > 0 ? 1 : 0);
    check(top->tc_bind_conflict_o == 0,
          "3: binding the same region twice is NOT a conflict", 0,
          top->tc_bind_conflict_o);

    // The sheet is 8x8 repeated across the screen, so EVERY pixel changes and
    // each carries its own (x mod 8, y mod 8) texel.
    int wrong = 0, unchanged = 0;
    for (int y = 0; y < kH; ++y)
      for (int x = 0; x < kW; ++x) {
        const size_t i = static_cast<size_t>(y) * kW + x;
        if (p.rgb[i] == kWorld) ++unchanged;
        if (p.rgb[i] != glyphTexel(x & 7, y & 7)) ++wrong;
      }
    check(unchanged == 0, "3: the ATMOSPHERE sheet covers the whole frame", 0, unchanged);
    check(wrong == 0,
          "3: and every pixel is its own wrapped texel -- the affine walk, the "
          "repeat and the page read, end to end", 0, wrong);
    if (wrong) { dumpCounters("3");
      for (int ry = 0; ry < kH; ++ry) {
        int bad = 0;
        for (int x = 0; x < kW; ++x)
          if (p.rgb[static_cast<size_t>(ry) * kW + x] != glyphTexel(x & 7, ry & 7)) ++bad;
        std::printf(" r%d:%d", ry, bad);
      }
      std::printf("  DBG3 row0:");
      for (int x = 0; x < 12; ++x) std::printf(" %04X", p.rgb[x]);
      std::printf(" row1:");
      for (int x = 0; x < 12; ++x) std::printf(" %04X", p.rgb[kW + x]);
      std::printf(" expect:");
      for (int x = 0; x < 12; ++x) std::printf(" %04X", glyphTexel(x & 7, 0));
      std::printf(" |"); }
  }

  // =========================================================================
  // 4. THE DISABLED / EMPTY NEXT FRAME
  // =========================================================================
  // The ruling: "a disabled/empty next frame" and "an empty-frame path that
  // cannot retain old HUD contents." Frame one draws; frame two carries no
  // packet at all and must show the world.
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    b.append_record(drawSpriteRecord(0x600, 8, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 0));
    b.append_record(setPlaneRecord(0x601, 0, 1, 1, 255, 1, 0, 1, 0, 64, 16, 1, 0, 3, 3));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    const Pass warm = runFrame(kWorld);
    check(warm.completed, "4: the warm-up frame completed", 1, warm.completed ? 1 : 0);
    // The sheet arms on the second frame (see case 3), so the DRAWN frame
    // re-sends the same descriptors -- which is what a game does every frame.
    rearmDecoder();
    zhao::ZhaoFrameBuilder b2;
    b2.begin_frame(2, 0, 0, 0);
    b2.append_record(drawSpriteRecord(0x602, 8, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                      1, 0, 0, 1, 0xFFFF, 0, 0));
    b2.append_record(setPlaneRecord(0x603, 0, 1, 1, 255, 1, 0, 1, 0, 64, 16, 1, 0, 3, 3));
    b2.end_frame(0);
    pushPacket(b2.seal(1, 1, 0));
    const Pass f1 = runFrame(kWorld);
    check(f1.completed, "4: frame one completed", 1, f1.completed ? 1 : 0);
    int drew = 0;
    for (int i = 0; i < kW * kH; ++i) if (f1.rgb[i] != kWorld) ++drew;
    check(drew == kW * kH, "4: frame one drew the sheet over everything", kW * kH, drew);

    const uint32_t disabled_before = top->tc_slots_auto_disabled_o;
    const Pass f2 = runFrame(kWorld);
    check(f2.completed, "4: frame two completed", 1, f2.completed ? 1 : 0);
    int left = 0;
    for (int i = 0; i < kW * kH; ++i) if (f2.rgb[i] != kWorld) ++left;
    // NO HUD PIXEL SURVIVES -- the band's generation tag cleared the list --
    // and the plane survives only inside the PUBLISH WINDOW, which is a
    // declared bound and not a surprise. `zhao_twod_sampler` starts prefilling
    // atmosphere lines at the frame TICK, and `zhao_twod_cmd` publishes the
    // frame's plane pair over the next `2*MAX_DESC + 4` clocks; the pixels the
    // walk produces before the auto-disable lands still carry the PREVIOUS
    // frame's sheet. `TWOD.CMD.md` states the bound and this asserts it, on the
    // correct side: the leak is confined to the first row and to that window.
    int offRow = 0, pastWindow = 0;
    const int window = 2 * kMaxDesc + 4;
    for (int y = 0; y < kH; ++y)
      for (int x = 0; x < kW; ++x) {
        const size_t i = static_cast<size_t>(y) * kW + x;
        if (f2.rgb[i] == kWorld) continue;
        if (y != 0) ++offRow;
        if (x >= window) ++pastWindow;
      }
    check(offRow == 0,
          "4: NOTHING OF FRAME ONE SURVIVED past its first row -- the HUD not "
          "at all, and the plane only inside the publish window", 0, offRow);
    check(pastWindow == 0,
          "4: and nothing past the declared 2*MAX_DESC + 4 publish window", 0,
          pastWindow);
    std::printf("  [4] plane leak into the empty frame: %d pixels, all on row 0, "
                "window %d, screen %d pixels\n", left, window, kW * kH);
    check(top->tc_slots_auto_disabled_o - disabled_before == 2,
          "4: the empty frame published TWO disables", 2,
          top->tc_slots_auto_disabled_o - disabled_before);
    check(top->tc_frames_sealed_o == 3, "4: three frames sealed", 3,
          top->tc_frames_sealed_o);
  }

  // =========================================================================
  // 5. MALFORMED DESCRIPTORS -- REFUSED, COUNTED, AND THE FRAME COMPLETES
  // =========================================================================
  // Three kinds in one packet, on three different counters:
  //   * a reserved bit set, which TWOD.CMD refuses (unrepresentable);
  //   * a reserved ROLE, which TWOD.PLANE refuses (the value's owner);
  //   * a zero-width sprite, which TWOD.SPRITE refuses (likewise).
  // The ruling requires the last two be RETAINED, and this is what says they
  // were: the refusal moved on the CONSUMER's counter, not on the producer's.
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    // a: a sprite with a reserved flag bit -- TWOD.CMD's refusal.
    b.append_record(drawSpriteRecord(0x700, 4, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 0, 0, 0, /*flags=*/1));
    // b: a plane with a RESERVED ROLE -- TWOD.PLANE's refusal, not this one's.
    b.append_record(setPlaneRecord(0x701, 0, /*role=*/2, 1, 255, 1, 0, 1, 0, 64, 16, 1, 0, 3, 3));
    // c: a zero-width sprite -- TWOD.SPRITE's refusal.
    b.append_record(drawSpriteRecord(0x702, 20, 4, /*w=*/0, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 1));
    // d: one LEGAL sprite, so the frame is not empty and "the frame completes"
    //    means something.
    b.append_record(drawSpriteRecord(0x703, 40, 6, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 2));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_OK,
          "5: THE FRAME VALIDATOR ACCEPTS ALL OF IT -- a malformed DESCRIPTOR "
          "is not a malformed RECORD, which is why these fields are u8 and not "
          "enums", zhao_abi::ZH_ABI_OK, gLastErr);
    check(top->tc_sprite_refused_o == 1, "5: the reserved flag bit, refused by TWOD.CMD", 1,
          top->tc_sprite_refused_o);
    check(top->tc_plane_refused_o == 0,
          "5: and the reserved ROLE was NOT -- the producer does not judge a "
          "value the consumer owns", 0, top->tc_plane_refused_o);
    check(top->tc_sprites_staged_o == 2, "5: two sprites staged", 2, top->tc_sprites_staged_o);

    const Pass p = runFrame(kWorld);
    check(p.completed, "5: THE FRAME COMPLETED", 1, p.completed ? 1 : 0);
    check(top->plane_refused_role_o == 1,
          "5: TWOD.PLANE refused the reserved role, on its own counter", 1,
          top->plane_refused_role_o);
    // TWICE, not once, and that is the BAND's per-band re-walk rather than a
    // double count: the zero-width sprite spans two bands, so the band hands
    // the walker a clipped slice in each and the walker refuses each. The
    // refusal is still WHOLE -- no pixel of it is drawn in either band.
    check(top->sprite_refused_o == 2,
          "5: TWOD.SPRITE refused the zero-width sprite in each band it "
          "touched, on its own counter", 2, top->sprite_refused_o);

    int drew = 0, stray = 0;
    for (int y = 0; y < kH; ++y)
      for (int x = 0; x < kW; ++x) {
        const size_t i = static_cast<size_t>(y) * kW + x;
        if (p.rgb[i] == kWorld) continue;
        ++drew;
        if (x < 40 || x >= 40 + kGlyphW || y < 6 || y >= 6 + kGlyphH) ++stray;
      }
    check(drew == kGlyphW * kGlyphH, "5: exactly the ONE legal sprite drew",
          kGlyphW * kGlyphH, drew);
    if (drew != kGlyphW * kGlyphH) dumpCounters("5");
    check(stray == 0, "5: and nothing of the three refusals reached the screen", 0, stray);
  }

  // =========================================================================
  // 6. WHOLE-SPRITE OVERFLOW (R235) AND THE LIST BUDGET
  // =========================================================================
  // Two different overflows, deliberately separated because they are two laws:
  //   * R235's leaky bucket refuses a sprite WHOLE, before one pixel of it is
  //     rasterised, and counts it;
  //   * the frame's descriptor budget drops the TAIL of the list and counts it.
  // NEITHER faults the frame, which is the clause the ruling names.
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    // MAX_DESC + 3 sprites: the last three cannot enter the list.
    for (int i = 0; i < kMaxDesc + 3; ++i)
      // `base` VARIES, and it has to: the binding slot is 4 + src_id[1:0], so
      // four sprites share each slot -- and they only CONFLICT if they name
      // different regions. A bench that left every base at zero would assert a
      // conflict that never happened, which is the detector-that-fires-on-
      // everything failure read backwards.
      b.append_record(drawSpriteRecord(0x800u + i, (i % 8) * 8, (i / 8) * 8,
                                       /*w=*/4, kGlyphH,
                                       /*base=*/(i % 8) * 64, 3, 3,
                                       1, 0, 0, 1,
                                       0xFFFF, static_cast<uint8_t>(i),
                                       static_cast<uint16_t>(i)));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_OK, "6: the packet is well formed",
          zhao_abi::ZH_ABI_OK, gLastErr);
    check(top->tc_sprites_staged_o == static_cast<uint32_t>(kMaxDesc),
          "6: exactly MAX_DESC entered the list", kMaxDesc, top->tc_sprites_staged_o);
    check(top->tc_list_overflow_o == 3, "6: three dropped from the TAIL, and counted", 3,
          top->tc_list_overflow_o);

    const Pass p = runFrame(kWorld);
    check(p.completed, "6: THE FRAME COMPLETED -- a HUD overflow never faults one", 1,
          p.completed ? 1 : 0);
    check(top->band_desc_overflow_o == 0,
          "6: and the band saw no overflow of its own, because the producer had "
          "already capped the list at the same budget", 0, top->band_desc_overflow_o);
    check(top->tc_bind_conflict_o > 0,
          "6: sixteen sprites over four binding slots ALIAS, and the detector "
          "says so rather than the picture saying it", 1,
          top->tc_bind_conflict_o > 0 ? 1 : 0);
  }

  // =========================================================================
  // 7. EVERY ASSET REFUSAL CLAUSE, DRIVEN BY A REAL PublishResource
  // =========================================================================
  // Against `zref::twod::asset_verdict`, which is the ledger's declared
  // reference model for TWOD.ASSET. Each clause is checked on its OWN counter,
  // because a test that only asked "was it refused" could not see two clauses
  // swapping.
  {
    struct Case {
      const char* what;
      uint8_t  dst_slot;
      uint32_t len;
      uint32_t addr_off;
      uint16_t epoch;
    };
    const uint32_t good_len = 128;
    const Case cases[] = {
      {"dst_slot 8 (past the page map)",  8,  good_len, 0, kEpoch},
      {"dst_slot 20 (past the palettes)", 20, good_len, 0, kEpoch},
      {"length 0",                        0,  0,        0, kEpoch},
      {"length not a multiple of 64",     0,  96 + 8,   0, kEpoch},
      {"length past the slot",            0,  (kSlotWords + 32) * 2, 0, kEpoch},
      {"address not 64-byte aligned",     0,  good_len, 8, kEpoch},
      {"a closed epoch",                  0,  good_len, 0, static_cast<uint16_t>(kEpoch + 1)},
    };
    for (const Case& c : cases) {
      hardReset();
      stageArena(page);
      zhao::ZhaoFrameBuilder b;
      b.begin_frame(1, 0, 0, 0);
      b.append_record(publishTwodPage(0x900, kArenaBase + c.addr_off, c.len,
                                      pageCrc, c.epoch, c.dst_slot));
      b.end_frame(0);
      pushPacket(b.seal(1, 1, 0));
      drainLoads(3000);

      const zref::twod::AssetVerdict want =
          zref::twod::asset_verdict(c.dst_slot, c.len,
                                    static_cast<uint64_t>(kArenaBase) + c.addr_off,
                                    c.epoch, kEpoch, kPageWords, kPalSlots, kPageSlots);
      const uint32_t got_slot  = top->ta_slot_refused_o;
      const uint32_t got_len   = top->ta_len_refused_o;
      const uint32_t got_addr  = top->ta_addr_refused_o;
      const uint32_t got_epoch = top->ta_epoch_refused_o;
      const uint32_t got_done  = top->ta_loads_done_o;

      const std::string tag = std::string("7: ") + c.what;
      check(want != zref::twod::AssetVerdict::kAccept,
            (tag + ": the oracle refuses it too").c_str(), 1,
            want != zref::twod::AssetVerdict::kAccept ? 1 : 0);
      check(got_done == 0, (tag + ": nothing was published").c_str(), 0, got_done);
      check(got_slot  == (want == zref::twod::AssetVerdict::kSlotRefused  ? 1u : 0u),
            (tag + ": slot counter").c_str(),
            want == zref::twod::AssetVerdict::kSlotRefused ? 1 : 0, got_slot);
      check(got_len   == (want == zref::twod::AssetVerdict::kLenRefused   ? 1u : 0u),
            (tag + ": length counter").c_str(),
            want == zref::twod::AssetVerdict::kLenRefused ? 1 : 0, got_len);
      check(got_addr  == (want == zref::twod::AssetVerdict::kAddrRefused  ? 1u : 0u),
            (tag + ": address counter").c_str(),
            want == zref::twod::AssetVerdict::kAddrRefused ? 1 : 0, got_addr);
      check(got_epoch == (want == zref::twod::AssetVerdict::kEpochRefused ? 1u : 0u),
            (tag + ": epoch counter").c_str(),
            want == zref::twod::AssetVerdict::kEpochRefused ? 1 : 0, got_epoch);
      check(top->ta_bursts_o == 0, (tag + ": and no burst was issued").c_str(), 0,
            top->ta_bursts_o);
    }

    // AND THE POSITIVE HALF. A transfer EXACTLY filling its slot is legal, and
    // that boundary is where an off-by-one would put the next slot's first word
    // under this transfer's control.
    hardReset();
    std::vector<uint8_t> full(kSlotWords * 2, 0);
    for (size_t i = 0; i < full.size(); ++i) full[i] = static_cast<uint8_t>(i * 7 + 1);
    stageArena(full);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x910, kArenaBase,
                                    static_cast<uint32_t>(gArena.size()),
                                    crcOf(gArena), kEpoch, /*dst_slot=*/1));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(20000);
    check(top->ta_loads_done_o == 1,
          "7: a transfer EXACTLY filling its slot is ACCEPTED -- the boundary, "
          "from the legal side", 1, top->ta_loads_done_o);
    check(top->ta_len_refused_o == 0, "7: and nothing refused it", 0, top->ta_len_refused_o);
    check(top->ta_words_written_o == kSlotWords, "7: every word landed", kSlotWords,
          top->ta_words_written_o);
  }

  // =========================================================================
  // 8. A CORRUPT PAGE IS ZEROED, NOT PUBLISHED
  // =========================================================================
  // The CRC is only known at the last beat, so the words are written as they
  // land. A bad CRC is followed by a zeroing pass over exactly what was
  // written, and then the sprite drawn from that region is BLACK -- visibly
  // nothing -- rather than a corrupt page that looks like art (R221).
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x920, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc ^ 1u, kEpoch, 0));    // ONE BIT WRONG
    b.append_record(drawSpriteRecord(0x921, 8, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 0));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(8000);

    check(top->ta_crc_fails_o == 1, "8: the CRC failure is counted", 1, top->ta_crc_fails_o);
    check(top->ta_regions_zeroed_o == 1, "8: and the region was zeroed", 1,
          top->ta_regions_zeroed_o);
    check(top->ta_loads_done_o == 0, "8: nothing was published", 0, top->ta_loads_done_o);

    const Pass p = runFrame(kWorld);
    check(p.completed, "8: the frame still completed", 1, p.completed ? 1 : 0);
    int nonBlack = 0, drew = 0;
    for (int y = 4; y < 4 + kGlyphH; ++y)
      for (int x = 8; x < 8 + kGlyphW; ++x) {
        const size_t i = static_cast<size_t>(y) * kW + x;
        ++drew;
        if (p.rgb[i] != 0) ++nonBlack;
      }
    check(drew == kGlyphW * kGlyphH, "8: the sprite still drew its rectangle",
          kGlyphW * kGlyphH, drew);
    check(nonBlack == 0,
          "8: and every pixel of it is COLOUR 0 -- visibly nothing, not a "
          "corrupt page that looks like art", 0, nonBlack);
  }

  // =========================================================================
  // 9. AN ABANDONED PACKET REACHES NO FRAME
  // =========================================================================
  // The atomicity case, end to end: the same descriptors, a corrupted payload
  // CRC, and nothing on the screen.
  {
    hardReset();
    stageArena(page);
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0x100, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    b.append_record(drawSpriteRecord(0xA00, 8, 4, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 0));
    b.end_frame(0);
    std::vector<uint8_t> pkt = b.seal(1, 1, 0);
    // Break the payload CRC32C, which is the last four bytes of the packet.
    pkt[pkt.size() - 1] ^= 0xFFu;
    pushPacket(pkt);
    drainLoads(4000);

    check(gLastErr == zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC,
          "9: the decoder refused the packet", zhao_abi::ZH_ABI_BAD_PAYLOAD_CRC, gLastErr);
    check(top->tc_packets_abandoned_o == 1, "9: TWOD.CMD was told to roll back", 1,
          top->tc_packets_abandoned_o);
    check(top->tc_sprites_staged_o == 1,
          "9: the record HAD been staged -- which is what makes the rollback a "
          "rollback rather than a drop", 1, top->tc_sprites_staged_o);

    const Pass p = runFrame(kWorld);
    check(p.completed, "9: the frame completed", 1, p.completed ? 1 : 0);
    check(top->tc_sprites_published_o == 0, "9: and NOTHING was published", 0,
          top->tc_sprites_published_o);
    int drew = 0;
    for (int i = 0; i < kW * kH; ++i) if (p.rgb[i] != kWorld) ++drew;
    check(drew == 0, "9: the screen is the world, untouched", 0, drew);
  }

  // =========================================================================
  // 10. A CLUT8 SPRITE, WITH A COMMAND-FED PALETTE
  // =========================================================================
  // The other format, and the other destination in `spec/cartridge.md` 4f: a
  // palette published to slot 16 of the map and an 8-bit page published to a
  // page slot, both by PublishResource, both read out of the arena.
  {
    hardReset();
    // The page: 8x8 CLUT8 indices, two texels per 16-bit word, so a row of 8
    // texels is 4 words -> lstride = 2.
    std::vector<uint8_t> idx;
    for (int v = 0; v < 8; ++v)
      for (int u = 0; u < 8; ++u) idx.push_back(static_cast<uint8_t>(v * 8 + u));
    idx.resize(64, 0);
    std::vector<uint8_t> body = idx;
    body.resize(64, 0);                      // 64 bytes = one burst
    stageArena(body);
    const uint32_t bodyCrc = crcOf(gArena);

    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0xB00, kArenaBase,
                                    static_cast<uint32_t>(gArena.size()),
                                    bodyCrc, kEpoch, /*dst_slot=*/0));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(4000);
    check(top->ta_loads_done_o == 1, "10: the CLUT8 page loaded", 1, top->ta_loads_done_o);

    // The palette, in its own chain (the decoder is one-shot), then the sprite.
    hardReset();
    std::vector<uint8_t> pal;
    for (int e = 0; e < 256; ++e) {
      const uint16_t c = static_cast<uint16_t>(0x4000u | (e + 1));
      pal.push_back(static_cast<uint8_t>(c & 0xFF));
      pal.push_back(static_cast<uint8_t>(c >> 8));
    }
    // ONE arena holding the page and the palette back to back, so both loads
    // are real reads of real bytes at real addresses.
    std::vector<uint8_t> both = body;
    both.insert(both.end(), pal.begin(), pal.end());
    stageArena(both);
    const uint32_t pageCrc8 = zhao_abi::zhao_crc32c(0, gArena.data(), 64);
    const uint32_t palCrc   = zhao_abi::zhao_crc32c(0, gArena.data() + 64, 512);

    zhao::ZhaoFrameBuilder b2;
    b2.begin_frame(1, 0, 0, 0);
    b2.append_record(publishTwodPage(0xB10, kArenaBase, 64, pageCrc8, kEpoch, 0));
    b2.append_record(publishTwodPage(0xB11, kArenaBase + 64, 512, palCrc, kEpoch,
                                     /*dst_slot=*/16));
    b2.append_record(drawSpriteRecord(0xB12, 4, 4, 8, 8, /*base=*/0,
                                      /*lstride=*/2, /*lheight=*/3,
                                      /*fmt=*/0, /*pal=*/0, 0, 1, 0xFFFF, 0, 0));
    b2.end_frame(0);
    pushPacket(b2.seal(1, 1, 0));
    drainLoads(8000);

    check(top->ta_loads_done_o == 2, "10: both the page and the palette published", 2,
          top->ta_loads_done_o);
    check(top->ta_words_written_o == 32 + 256,
          "10: 32 page words and 256 palette entries", 32 + 256, top->ta_words_written_o);

    const Pass p = runFrame(kWorld);
    check(p.completed, "10: the CLUT8 frame completed", 1, p.completed ? 1 : 0);
    check(top->smp_clut8_samples_o == 64, "10: sixty-four CLUT8 samples", 64,
          top->smp_clut8_samples_o);
    check(top->smp_pal_refused_o == 0, "10: the palette slot was in range", 0,
          top->smp_pal_refused_o);
    int wrong = 0;
    for (int v = 0; v < 8; ++v)
      for (int u = 0; u < 8; ++u) {
        const size_t i = static_cast<size_t>(4 + v) * kW + (4 + u);
        const uint8_t index = static_cast<uint8_t>(v * 8 + u);
        const uint16_t want = static_cast<uint16_t>(0x4000u | (index + 1));
        if (p.rgb[i] != want) ++wrong;
      }
    check(wrong == 0,
          "10: and every pixel is its own PALETTE ENTRY, looked up from the "
          "index the page held -- two arena reads, one composited pixel", 0, wrong);
  }

  // =========================================================================
  // 11. THE BRIDGE REFUSES A BURST AND THE LOAD STILL COMPLETES
  // =========================================================================
  // `zhao_hps_arbiter_n` answers `err` with no grant on a collision. The loader
  // takes the request DOWN and re-offers; a held request the arbiter re-serves
  // would spin with every counter frozen.
  {
    hardReset();
    stageArena(page);
    gErrOnce = 3;
    gGrantDelay = 5;
    zhao::ZhaoFrameBuilder b;
    b.begin_frame(1, 0, 0, 0);
    b.append_record(publishTwodPage(0xC00, kArenaBase, static_cast<uint32_t>(gArena.size()),
                                    pageCrc, kEpoch, 0));
    b.append_record(drawSpriteRecord(0xC01, 0, 0, kGlyphW, kGlyphH, 0, 3, 3,
                                     1, 0, 0, 1, 0xFFFF, 0, 0));
    b.end_frame(0);
    pushPacket(b.seal(1, 1, 0));
    drainLoads(8000);

    check(top->ta_bridge_errs_o == 3, "11: three refusals counted", 3, top->ta_bridge_errs_o);
    check(top->ta_loads_done_o == 1, "11: and the load still completed", 1,
          top->ta_loads_done_o);
    const Pass p = runFrame(kWorld);
    int wrong = 0;
    for (int v = 0; v < kGlyphH; ++v)
      for (int u = 0; u < kGlyphW; ++u) {
        const size_t i = static_cast<size_t>(v) * kW + u;
        if (p.rgb[i] != glyphTexel(u, v)) ++wrong;
      }
    check(wrong == 0, "11: with the right bytes in the right places", 0, wrong);
    if (wrong) {
      std::printf("  DBG11 non-world pixels:");
      int n = 0;
      for (int y = 0; y < kH && n < 80; ++y)
        for (int x = 0; x < kW && n < 80; ++x) {
          const size_t i = static_cast<size_t>(y) * kW + x;
          if (p.rgb[i] != kWorld) { std::printf(" (%d,%d)=%04X", x, y, p.rgb[i]); ++n; }
        }
      std::printf(" | total shown %d, band_underrun=%u, pixels_written=%u, admitted=%u, descriptors=%u|", n,
                  top->band_underrun_o, top->band_pixels_written_o,
                  top->band_sprites_admitted_o, top->band_descriptors_o);
    }
  }

  std::printf(
      "  THE CHAIN RAN END TO END: a packet byte -> zhao_cmd_decoder +\n"
      "  zhao_cmd_exec -> zhao_twod_cmd's sealed list -> zhao_twod_plane /\n"
      "  zhao_twod_band -> zhao_twod_sprite -> zhao_twod_sampler ->\n"
      "  zhao_post_composite -> a composited pixel, with every texel read out\n"
      "  of an HPS arena by zhao_twod_asset. NO descriptor and NO texel was\n"
      "  written at a downstream boundary by this driver.\n");

  delete top;
  return zhao::report_and_exit("twod_cmd_chain_directed");
}
