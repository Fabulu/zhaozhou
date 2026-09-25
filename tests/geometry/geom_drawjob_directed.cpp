// geom_drawjob_directed.cpp -- zhao_geom_drawjob against zref::drawjob.
//
// The block turns a ratified DrawForm into GEOM.MESHFETCH jobs (owner ruling
// R29). What is differentiated here is every field of every job and every
// refusal ORDINAL -- a wrong reason is as wrong as a wrong verdict, because
// each reason is its own counter and its own diagnosis.
//
// The oracle is `zref::drawjob::expand`, which is the FREEZE of the layout;
// this file never restates the law, it asks the header for it. The guard and
// the memory are PLAYED (this block owns no arithmetic over their contents
// beyond the header decode), with the guard's two-cycle verdict modelled the
// way `zhao_mem_guard` actually answers -- ready and ok are never high in the
// same cycle, which is the fault D22 tread 10 found in two fetchers at once.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_drawjob.h"

#include "zhao_sim.hpp"
#include "zref/zref_drawjob.hpp"

namespace dj = zref::drawjob;

namespace {

constexpr uint32_t kPoolBase = 0x06A00000u;  // ZHAO_RENDER_ASSET_BASE
constexpr uint32_t kXforms = 256;            // the RTL's XFORMS default
int g_checks = 0, g_failed = 0;

void check(bool ok, const char* what, long long got, long long want) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL %-44s got %lld want %lld\n", what, got, want);
  }
}

bool guard_valid(const Vzhao_geom_drawjob& t) { return ((t.guard_req_o[3] >> 6) & 1u) != 0u; }
uint32_t guard_addr(const Vzhao_geom_drawjob& t) {
  // {valid, write, client[2:0], addr[26:0], len[6:0], be[63:0]}: addr sits just
  // above len, which starts at bit 64.
  const uint64_t hi = (static_cast<uint64_t>(t.guard_req_o[3]) << 32) | t.guard_req_o[2];
  return static_cast<uint32_t>((hi >> 7) & 0x07FFFFFFu);
}
int guard_len(const Vzhao_geom_drawjob& t) { return static_cast<int>(t.guard_req_o[2] & 0x7Fu); }

struct SeenJob {
  uint16_t instance_id;
  uint32_t desc_addr;
  uint8_t format;
  uint16_t generation;
  uint8_t active_mask;
  int32_t xform[12];
  uint32_t stream_base;
  uint32_t raster_state;
  uint32_t material_set;
  uint8_t semantic_weight;
  // The 24-bit MESH_STREAM form index, as the ACCEPTED job presents it (owner
  // ruling of 2026-09-21, section 3). Sampled on the handshake like every
  // other field here, which is the lifetime the ruling requires.
  uint32_t form_idx;
};

struct Run {
  int form_idx_offered_bare = 0;
  std::vector<SeenJob> jobs;
  bool saw_request = false;
  int request_len = -1;
  uint32_t request_addr = 0;
};

void step(Vzhao_geom_drawjob& t) {
  t.eval();
  zhao::tick(t);
}

void publish(Vzhao_geom_drawjob& t, uint8_t slot, uint32_t index, uint16_t gen, uint32_t base,
             uint32_t extent) {
  t.dir_we_i = 1;
  t.dir_entry_i = slot;
  t.dir_index_i = index;
  t.dir_generation_i = gen;
  t.dir_base_i = base;
  t.dir_extent_i = extent;
  step(t);
  t.dir_we_i = 0;
  step(t);
}

void palette_write(Vzhao_geom_drawjob& t, uint32_t node, const int32_t m[12]) {
  t.px_valid_i = 1;
  t.px_index_i = node;
  for (int k = 0; k < 12; ++k) t.px_m_i[k] = static_cast<uint32_t>(m[k]);
  step(t);
  t.px_valid_i = 0;
  step(t);
}

/** Drive one DrawForm to completion. `header` may be null: the guard then
 *  refuses the read, which is the kFetchDenied path. */
Run drive(Vzhao_geom_drawjob& t, const dj::DrawForm& d, const uint8_t* header) {
  Run r;
  t.d_valid_i = 1;
  t.d_form_i = d.form;
  t.d_material_set_i = d.material_set;
  t.d_transform_i = d.transform;
  t.d_viewport_mask_i = d.viewport_mask;
  t.d_semantic_weight_i = d.semantic_weight;
  t.d_flags_i = d.flags;
  t.d_src_id_i = d.src_id;
  t.j_ready_i = 1;
  t.eval();
  // The accept happens on the edge where d_ready_o is high. Bounded: an
  // unbounded wait turns a stuck DUT into a test that never returns.
  for (int w = 0; w < 64 && !t.d_ready_o; ++w) step(t);
  if (!t.d_ready_o) {
    std::printf("FAIL driver: the block never offered d_ready_o\n");
    ++g_failed;
    return r;
  }
  step(t);
  t.d_valid_i = 0;

  // THE RUN ENDS WHEN THE BLOCK IS IDLE AGAIN, not after a few quiet cycles.
  // The first version of this driver stopped after eight cycles with no job,
  // which is FEWER than a header fetch takes -- so it walked away mid-fetch and
  // the next draw waited on `d_ready_o` for ever. A test that leaves the DUT
  // mid-transaction reports a hang, not a failure, which is the most expensive
  // kind of wrong.
  bool accepted = false, granted = false, verdict_done = false, finished = false;
  int beat = 0;
  for (int c = 0; c < 400 && !finished; ++c) {
    // {ready, ok, violation}. A DENIAL is accepted the same way a pass is --
    // `ready` is a level meaning "the forwarding stage is free" -- and only the
    // VERDICT cycle differs. Modelling a denial as "never ready" (which the
    // first version did) does not test the refusal at all: it wedges the block
    // in S_REQ, which reads as a hang rather than as the counter not moving.
    if (guard_valid(t) && !accepted) {
      r.saw_request = true;
      r.request_len = guard_len(t);
      r.request_addr = guard_addr(t);
      t.guard_rsp_i = 0b100;  // ready: the accept
    } else if (accepted && !verdict_done) {
      t.guard_rsp_i = header ? 0b010 : 0b001;  // ok, or violation
    } else {
      t.guard_rsp_i = 0;
    }

    t.beat_valid_i = 0;
    t.beat_last_i = 0;
    t.crc_ok_i = 0;
    if (granted && beat < 8) {
      uint64_t w = 0;
      for (int k = 0; k < 8; ++k) w |= static_cast<uint64_t>(header[beat * 8 + k]) << (8 * k);
      t.beat_valid_i = 1;
      t.beat_data_i = w;
      t.beat_last_i = (beat == 7) ? 1 : 0;
      // The walker's verdict, PLAYED exactly as `zhao_geom_desc_crc` presents
      // it: a combinational pulse on the LAST beat, high only when the fold
      // over bytes 0..59 matches the word at 60. Driving it high for the whole
      // burst would make the CRC refusal untestable.
      const uint32_t stamped =
          static_cast<uint32_t>(header[60]) | (static_cast<uint32_t>(header[61]) << 8) |
          (static_cast<uint32_t>(header[62]) << 16) | (static_cast<uint32_t>(header[63]) << 24);
      const bool crc_good = zhao_abi::zhao_crc32c(0, header, dj::kHdrCrcCovered) == stamped;
      t.crc_ok_i = (beat == 7 && crc_good) ? 1 : 0;
    }

    t.eval();

    if (accepted && !verdict_done) {
      verdict_done = true;
      granted = (header != nullptr);
    }
    if (guard_valid(t) && !accepted) accepted = true;
    if (t.beat_valid_i) ++beat;

    if (t.j_valid_o && t.j_ready_i) {
      SeenJob j{};
      j.instance_id = static_cast<uint16_t>(t.j_instance_id_o);
      j.desc_addr = t.j_desc_addr_o;
      j.format = static_cast<uint8_t>(t.j_format_o);
      j.generation = static_cast<uint16_t>(t.j_generation_o);
      j.active_mask = static_cast<uint8_t>(t.j_active_mask_o);
      for (int k = 0; k < 12; ++k) j.xform[k] = static_cast<int32_t>(t.j_xform_o[k]);
      j.stream_base = t.j_stream_base_o;
      // {weight[71:64], material_set[63:32], raster[31:0]}
      j.raster_state = t.j_side_o[0];
      j.material_set = t.j_side_o[1];
      j.semantic_weight = static_cast<uint8_t>(t.j_side_o[2] & 0xFFu);
      j.form_idx = t.j_form_idx_o;
      r.jobs.push_back(j);
    }
    // `d_ready_o` is high only in S_IDLE, and the block cannot be back there
    // until this draw has emitted or refused everything it is going to.
    // NOT OFFERED BARE. `d_ready_o` is `(st_q == S_IDLE)`, and in S_IDLE the
    // block's `form_idx_q` still holds the PREVIOUS draw -- so a consumer that
    // read the port there would associate this draw's pages with the last
    // draw's form. Recorded every cycle the job is not being offered.
    if (!t.j_valid_o && t.j_form_idx_o != 0) ++r.form_idx_offered_bare;
    if (t.d_ready_o && c > 0) finished = true;
    zhao::tick(t);
  }
  if (!finished) {
    std::printf("FAIL driver: the block never returned to idle\n");
    ++g_failed;
  }
  t.beat_valid_i = 0;
  t.guard_rsp_i = 0;
  return r;
}

void compare(const char* name, const Run& r, const dj::Outcome& want) {
  char buf[128];
  std::snprintf(buf, sizeof buf, "%s: job count", name);
  check(r.jobs.size() == want.jobs.size(), buf, static_cast<long long>(r.jobs.size()),
        static_cast<long long>(want.jobs.size()));
  const size_t n = r.jobs.size() < want.jobs.size() ? r.jobs.size() : want.jobs.size();
  for (size_t i = 0; i < n; ++i) {
    const SeenJob& g = r.jobs[i];
    const dj::Job& w = want.jobs[i];
    std::snprintf(buf, sizeof buf, "%s[%zu]: desc_addr", name, i);
    check(g.desc_addr == (w.desc_addr & 0x07FFFFFFu), buf, g.desc_addr, w.desc_addr);
    std::snprintf(buf, sizeof buf, "%s[%zu]: format", name, i);
    check(g.format == w.format, buf, g.format, w.format);
    std::snprintf(buf, sizeof buf, "%s[%zu]: generation", name, i);
    check(g.generation == w.generation, buf, g.generation, w.generation);
    std::snprintf(buf, sizeof buf, "%s[%zu]: active_mask", name, i);
    check(g.active_mask == w.active_mask, buf, g.active_mask, w.active_mask);
    std::snprintf(buf, sizeof buf, "%s[%zu]: instance_id", name, i);
    check(g.instance_id == w.instance_id, buf, g.instance_id, w.instance_id);
    std::snprintf(buf, sizeof buf, "%s[%zu]: stream_base", name, i);
    check(g.stream_base == w.stream_base, buf, g.stream_base, w.stream_base);
    std::snprintf(buf, sizeof buf, "%s[%zu]: raster_state", name, i);
    check(g.raster_state == w.raster_state, buf, g.raster_state, w.raster_state);
    std::snprintf(buf, sizeof buf, "%s[%zu]: material_set", name, i);
    check(g.material_set == w.material_set, buf, g.material_set, w.material_set);
    std::snprintf(buf, sizeof buf, "%s[%zu]: semantic_weight", name, i);
    check(g.semantic_weight == w.semantic_weight, buf, g.semantic_weight, w.semantic_weight);
    for (int k = 0; k < 12; ++k) {
      std::snprintf(buf, sizeof buf, "%s[%zu]: xform[%d]", name, i, k);
      check(g.xform[k] == w.xform[k], buf, g.xform[k], w.xform[k]);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_drawjob top;

  top.d_valid_i = 0;
  top.j_ready_i = 1;
  top.dir_we_i = 0;
  top.px_valid_i = 0;
  top.beat_valid_i = 0;
  top.crc_ok_i = 0;
  top.guard_rsp_i = 0;
  top.client_i = 3;  // ZHAO_CLIENT_ENGINE1
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) step(top);
  top.rst_n = 1;
  for (int i = 0; i < 4; ++i) step(top);

  // ---- the resident MESH_STREAM page, and the oracle's copy of it ----------
  const uint32_t kIndex = 0x00ABCD, kBase = kPoolBase + 0x4000, kExtent = 0x2000;
  const uint16_t kGen = 0x0102;
  const uint32_t kDescOff = 64;
  std::vector<dj::DirRow> dir;
  dir.push_back(dj::DirRow{true, kIndex, kGen, kBase, kExtent});
  publish(top, 1, kIndex, kGen, kBase, kExtent);

  dj::XformPalette pal(kXforms);
  int32_t m[12];
  for (int k = 0; k < 12; ++k) m[k] = 0x11110000 + k * 7;
  const uint32_t kNode = 5;
  palette_write(top, kNode, m);
  pal.write(kNode, m);

  uint8_t hdr[dj::kHeaderBytes];
  dj::make_header(hdr, 1, 3, 0x0707, kDescOff);

  dj::DrawForm d{};
  d.form = (kIndex << 8) | (kGen & 0xFF);
  d.material_set = 0xCAFEBA02;
  d.transform = (kNode << 8) | 0x55;  // the generation byte is not read (see the RTL)
  d.viewport_mask = 0x3;
  d.semantic_weight = 0x5A;
  d.flags = (1u << 2);  // cull mode NEG
  d.src_id = 0x1234;

  // ---- A: three meshlets, every field ------------------------------------
  {
    Run r = drive(top, d, hdr);
    dj::Outcome w = dj::expand(d, dir, pal, hdr, kPoolBase);
    compare("A three meshlets", r, w);
    check(r.request_len == 64, "A: the header request is 64 bytes", r.request_len, 64);
    check(r.request_addr == (kBase & 0x07FFFFFFu), "A: the header is read at the page base",
          r.request_addr, kBase & 0x07FFFFFFu);
    check(top.draws_o == 1, "A: draws counted", top.draws_o, 1);
    check(top.jobs_o == 3, "A: jobs counted", top.jobs_o, 3);
    // The addresses STEP by the frozen descriptor stride, and that is the
    // property a single-meshlet fixture cannot see.
    check(r.jobs.size() == 3 && r.jobs[1].desc_addr - r.jobs[0].desc_addr == 64,
          "A: desc_addr steps by 64", r.jobs.size() == 3 ? r.jobs[1].desc_addr - r.jobs[0].desc_addr : -1,
          64);

    // ---- the ACCEPTED JOB'S FORM INDEX -- owner ruling 2026-09-21 §3 ------
    // EXPOSED, NOT INVENTED. `zhao_geom_drawjob` has always held
    // `form_idx_q = d_form_i[31:8]`; this packet routes it out of the block so
    // the pose request can carry the DRAW'S own form rather than re-reading an
    // unrelated live register later. The consumer,
    // `zhao_geom_clipread.p_form_idx_i`, lands in the same commit.
    bool form_on_every_job = !r.jobs.empty();
    for (const SeenJob& j : r.jobs) form_on_every_job = form_on_every_job && j.form_idx == kIndex;
    check(form_on_every_job,
          "A: every emitted job carries the draw's 24-bit form index (d_form_i[31:8])",
          form_on_every_job, 1);
    check(r.form_idx_offered_bare == 0,
          "A: and it is NEVER offered while the job is not valid -- in S_IDLE the "
          "register still holds the PREVIOUS draw", 0, r.form_idx_offered_bare);
  }

  // ---- the refusals, each against the oracle's ordinal --------------------
  struct Case {
    const char* name;
    dj::DrawForm draw;
    bool use_header;
    uint8_t hdr[dj::kHeaderBytes];
    int ordinal;
  };
  std::vector<Case> cases;
  auto mk = [&](const char* name, int ordinal) {
    Case c{};
    c.name = name;
    c.draw = d;
    c.use_header = true;
    std::memcpy(c.hdr, hdr, sizeof hdr);
    c.ordinal = ordinal;
    return c;
  };

  {  // 0 cull reserved
    Case c = mk("B cull reserved", 0);
    c.draw.flags = (3u << 2);
    cases.push_back(c);
  }
  {  // 1 not resident
    Case c = mk("C not resident", 1);
    c.draw.form = (0x00BEEFu << 8) | (kGen & 0xFF);
    cases.push_back(c);
  }
  {  // 2 stale handle
    Case c = mk("D stale", 2);
    c.draw.form = (kIndex << 8) | 0xEE;
    cases.push_back(c);
  }
  {  // 3 transform row never written
    Case c = mk("E xform missing", 3);
    c.draw.transform = (77u << 8);
    cases.push_back(c);
  }
  {  // 3 transform index past the palette
    Case c = mk("E2 xform out of range", 3);
    c.draw.transform = (0x001000u << 8);
    cases.push_back(c);
  }
  {  // 4 the guard refuses the header read
    Case c = mk("F fetch denied", 4);
    c.use_header = false;
    cases.push_back(c);
  }
  {  // 5 a stream format this reader does not speak
    Case c = mk("G header format", 5);
    dj::make_header(c.hdr, 2, 3, 0x0707, kDescOff);
    cases.push_back(c);
  }
  {  // 6 one bit flipped after the CRC was stamped
    Case c = mk("H header crc", 6);
    c.hdr[2] ^= 0x01;
    cases.push_back(c);
  }
  {  // 7 a reserved byte used by an older writer
    Case c = mk("I header reserved", 7);
    c.hdr[40] = 0x01;
    dj::make_header(c.hdr, 1, 3, 0x0707, kDescOff);
    c.hdr[40] = 0x01;
    // restamp so the CRC is right and RESERVED is the reason, not the CRC
    const uint32_t crc = zhao_abi::zhao_crc32c(0, c.hdr, dj::kHdrCrcCovered);
    for (int k = 0; k < 4; ++k) c.hdr[dj::kHdrCrcOff + k] = static_cast<uint8_t>(crc >> (8 * k));
    cases.push_back(c);
  }
  {  // 8 a descriptor table that ends past the page
    Case c = mk("J table past extent", 8);
    dj::make_header(c.hdr, 1, 1024, 0x0707, kDescOff);
    cases.push_back(c);
  }
  {  // 8 a misaligned descriptor table
    Case c = mk("J2 desc_offset misaligned", 8);
    dj::make_header(c.hdr, 1, 2, 0x0707, 96);
    cases.push_back(c);
  }
  {  // 8 a table that would start inside the header
    Case c = mk("J3 desc_offset inside header", 8);
    dj::make_header(c.hdr, 1, 2, 0x0707, 0);
    cases.push_back(c);
  }

  for (const Case& c : cases) {
    uint32_t before[9];
    for (int i = 0; i < 9; ++i) before[i] = top.refused_o[i];
    Run r = drive(top, c.draw, c.use_header ? c.hdr : nullptr);
    dj::Outcome w = dj::expand(c.draw, dir, pal, c.use_header ? c.hdr : nullptr, kPoolBase);
    char buf[128];
    std::snprintf(buf, sizeof buf, "%s: emits no job", c.name);
    check(r.jobs.empty(), buf, static_cast<long long>(r.jobs.size()), 0);
    std::snprintf(buf, sizeof buf, "%s: the oracle refuses too", c.name);
    check(w.refusal != dj::Refusal::kNone, buf, static_cast<int>(w.refusal), c.ordinal);
    std::snprintf(buf, sizeof buf, "%s: oracle ordinal", c.name);
    check(static_cast<int>(w.refusal) == c.ordinal, buf, static_cast<int>(w.refusal), c.ordinal);
    for (int i = 0; i < 9; ++i) {
      const uint32_t moved = top.refused_o[i] - before[i];
      std::snprintf(buf, sizeof buf, "%s: refused_o[%d]", c.name, i);
      check(moved == (i == c.ordinal ? 1u : 0u), buf, moved, i == c.ordinal ? 1 : 0);
    }
  }

  // ---- K: a legal stream of no meshlets is not a refusal ------------------
  {
    uint8_t h0[dj::kHeaderBytes];
    dj::make_header(h0, 1, 0, 0x0707, kDescOff);
    const uint32_t empty_before = top.empty_o;
    Run r = drive(top, d, h0);
    dj::Outcome w = dj::expand(d, dir, pal, h0, kPoolBase);
    check(r.jobs.empty() && w.jobs.empty(), "K empty stream: no jobs",
          static_cast<long long>(r.jobs.size()), 0);
    check(w.refusal == dj::Refusal::kNone, "K empty stream: not a refusal",
          static_cast<int>(w.refusal), 255);
    check(top.empty_o - empty_before == 1, "K empty stream: counted as empty",
          top.empty_o - empty_before, 1);
  }

  // ---- L: a zero viewport mask reads NOTHING ------------------------------
  {
    dj::DrawForm dm = d;
    dm.viewport_mask = 0;
    const uint32_t masked_before = top.masked_o;
    Run r = drive(top, dm, hdr);
    dj::Outcome w = dj::expand(dm, dir, pal, hdr, kPoolBase);
    check(r.jobs.empty(), "L masked: no jobs", static_cast<long long>(r.jobs.size()), 0);
    check(w.masked, "L masked: the oracle agrees", w.masked ? 1 : 0, 1);
    check(!r.saw_request, "L masked: no memory was read", r.saw_request ? 1 : 0, 0);
    check(top.masked_o - masked_before == 1, "L masked: counted", top.masked_o - masked_before, 1);
  }

  // ---- M: a Loom node no draw can name is COUNTED, never wrapped ---------
  {
    const uint32_t dropped_before = top.pal_dropped_o;
    const uint32_t writes_before = top.pal_writes_o;
    int32_t junk[12];
    for (int k = 0; k < 12; ++k) junk[k] = 0x7F7F0000 + k;
    palette_write(top, 1000, junk);  // >= XFORMS at the default 256
    check(top.pal_dropped_o - dropped_before == 1, "M: the dropped node is counted",
          top.pal_dropped_o - dropped_before, 1);
    check(top.pal_writes_o - writes_before == 0, "M: and is not written",
          top.pal_writes_o - writes_before, 0);
    // The row a wrapped index would have hit (1000 mod 256 = 232) must still
    // be unwritten, so a draw naming it is REFUSED rather than drawn wrong.
    dj::DrawForm dw2 = d;
    dw2.transform = (232u << 8);
    const uint32_t before3 = top.refused_o[3];
    Run r = drive(top, dw2, hdr);
    check(r.jobs.empty() && (top.refused_o[3] - before3 == 1),
          "M: the row a wrap would have hit is still unwritten", top.refused_o[3] - before3, 1);
  }

  // ---- N: the SECOND draw after a refusal still works ---------------------
  // A machine that leaves state behind after a refusal produces a right answer
  // once and a wrong one after every fault, which no single-case suite sees.
  {
    Run r = drive(top, d, hdr);
    dj::Outcome w = dj::expand(d, dir, pal, hdr, kPoolBase);
    compare("N after refusals", r, w);
  }

  // ---- P: the palette across its FULL 256 range, and the boundary at 256 --
  //
  // Added 2026-09-26 with the M10K conversion (zhao_geom_drawjob.sv:229). Until
  // then `pal_q` was 98,304 flip-flops with a 256:1 x 384-bit read mux; it is
  // now a 256x384 Simple Dual Port altsyncram. A register file and a memory
  // differ exactly where this case looks -- whether EVERY row is addressable
  // and holds its own contents, rather than the handful any previous case
  // touched. Cases A-N between them exercised two rows out of 256.
  //
  // This asserts the CORRECT behaviour and contains no positive control. The
  // instrument evidence that the array really became memory is the quartus_map
  // RAM Summary, not this test: a directed test cannot tell M10K from flops,
  // and must not be quoted as though it could.
  {
    const uint32_t writes_before_p = top.pal_writes_o;

    // Every row gets a pattern that NAMES ITS OWN INDEX, so a row returning a
    // neighbour's contents fails instead of coincidentally matching. A sweep
    // of identical rows would pass against almost any addressing defect.
    for (uint32_t n = 0; n < kXforms; ++n) {
      int32_t mm[12];
      for (int k = 0; k < 12; ++k)
        mm[k] = static_cast<int32_t>(0xA0000000u + (n << 8) + static_cast<uint32_t>(k));
      palette_write(top, n, mm);
      pal.write(n, mm);
    }
    check(top.pal_writes_o - writes_before_p == kXforms,
          "P: all 256 rows were written", top.pal_writes_o - writes_before_p, kXforms);

    // Read back across the range, both ends included.
    const uint32_t probe_rows[] = {0u, 1u, 127u, 254u, kXforms - 1u};
    for (uint32_t n : probe_rows) {
      dj::DrawForm dp = d;
      dp.transform = (n << 8);
      Run rp = drive(top, dp, hdr);
      dj::Outcome wp = dj::expand(dp, dir, pal, hdr, kPoolBase);
      char nm[48];
      std::snprintf(nm, sizeof nm, "P row %u", n);
      compare(nm, rp, wp);
    }

    // THE BOUNDARY, AT EXACTLY XFORMS. Case M already covers index 1000, but
    // 1000 mod 256 is 232 -- an ordinary row. 256 mod 256 is ZERO, so a wrap
    // at exactly the tier would overwrite ROW 0: the most damaging wrap
    // available, and the one the refusal law exists to prevent. It is also the
    // classic off-by-one, where a `<=` in place of a `<` would let exactly this
    // one index through and nothing else.
    const uint32_t dropped_before_b = top.pal_dropped_o;
    const uint32_t writes_before_b = top.pal_writes_o;
    int32_t poison[12];
    for (int k = 0; k < 12; ++k) poison[k] = static_cast<int32_t>(0xDEAD0000u + k);
    palette_write(top, kXforms, poison);
    check(top.pal_dropped_o - dropped_before_b == 1,
          "P: index 256 is REFUSED and counted", top.pal_dropped_o - dropped_before_b, 1);
    check(top.pal_writes_o - writes_before_b == 0,
          "P: index 256 writes nothing", top.pal_writes_o - writes_before_b, 0);

    // Row 0 must still hold what the sweep put there. This passes BECAUSE the
    // refusal holds; it fails if the index is ever wrapped.
    dj::DrawForm d0 = d;
    d0.transform = 0u;
    Run r0 = drive(top, d0, hdr);
    dj::Outcome w0 = dj::expand(d0, dir, pal, hdr, kPoolBase);
    compare("P row 0 survives the boundary write", r0, w0);
  }

  std::printf("geom_drawjob_directed: %d checks, %d failed\n", g_checks, g_failed);
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
  return 0;
}
