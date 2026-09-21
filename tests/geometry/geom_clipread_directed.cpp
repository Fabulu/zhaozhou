// geom_clipread_directed.cpp -- the kind-8 BODY and kind-9 CLIP FRAME page
// reader against `zref::creature_page::body` and `zref::clip_page`, the two
// frozen layouts, and against the two committed goldens.
//
// The bench is BOTH ends of the block: the publication MEM.UPLOAD makes
// (`spec/memory_rules.md` 5f.1's row, whole), and the asset window it reads
// through. Every page under test is built by the REFERENCE, so the RTL is read
// against bytes nothing in the RTL wrote -- the only arrangement in which a
// layout disagreement can show up at all.
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. EVERY BONE, EVERY WORD, IN ORDER. The fill stream is recorded and
//      replayed against `zref::…::body::decode_body` and
//      `zref::clip_page::decode` field by field. A reader that mixed up the
//      two records in a line, or that transposed a quaternion's lanes, fails
//      here -- and the EXACT READ COUNT is asserted too, because re-reading a
//      line delivers the same bytes again and is invisible to any content
//      check.
//   2. FILL WHOLE, THEN ASK -- the block's whole reason for existing, and the
//      one the committed latefetch mutant states. The bench asserts that every
//      word of both stores is written BEFORE `src_req_o` rises and that NO
//      read is issued after it. A reader that fetched per bone behind the
//      decode passes every content check above and fails this one.
//   3. `bone_mismatch_o` DISCRIMINATES, with its negative control beside it
//      (R95). A 6-bone skeleton under an 8-bone clip page fires it; the
//      matched pair, through the identical request path, leaves it at zero.
//   4. A REFUSED PAGE CHANGES NOTHING. Bad magic, bad version, a body claiming
//      a rest rotation, a misaligned offset, an extent that does not reach,
//      more clips than CLIP_ROWS -- each refuses the page WHOLE, and the bench
//      requires the previous residency to still answer bit for bit afterwards.
//      "Fewer bones" is not the test; "the same bones" is.
//   5. A MISS IS A MISS. An unknown clip slot and a frame past its count are
//      each counted and raise NO handover; the caller degrades to bind pose
//      (ruling R229) rather than decoding whatever was in the store.
//   6. THE GOLDENS. `clip_page_v1.bin` and `ladder_page_body_v1.bin` are read
//      by the RTL, not merely re-derived by the reference.
//   7. EVERY COUNTER IS SEEN TO MOVE. A detector reading zero is a claim.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Vtb_geom_clipread.h"
#include "zhao_sim.hpp"
#include "zref/zref_clip_page.hpp"
#include "zref/zref_creature_page.hpp"

using zhao::check;
namespace cp = zref::creature_page;
namespace bd = zref::creature_page::body;
namespace clp = zref::clip_page;

namespace {

constexpr uint32_t kBase = 0x0100'0000u;
constexpr uint8_t kOtherKind = 11;  // MATERIAL_SET, published on the same wire

// One recorded fill beat, exactly as the store sees it.
struct Fill {
  bool sel;
  uint8_t bone;
  uint8_t word;
  uint64_t data;
};

struct Bench {
  Vtb_geom_clipread& d;
  std::vector<uint8_t> mem;  // the asset window, at kBase
  int reads = 0;             // requests accepted
  int deny_after = -1;       // deny the Nth request (0-based); -1 = never

  std::vector<Fill> fills;
  // THE LATE-FETCH LAW, recorded rather than argued. Both are reset per frame.
  int reads_after_req = 0;   // a read issued after `src_req_o` -- must be 0
  int fills_after_req = 0;   // a fill written after it -- must be 0
  bool seen_req = false;

  int beats_left = 0;
  uint32_t beat_addr = 0;

  explicit Bench(Vtb_geom_clipread& dut) : d(dut) {}

  uint64_t word(uint32_t addr) const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      const size_t o = static_cast<size_t>(addr - kBase) + static_cast<size_t>(i);
      const uint8_t b = o < mem.size() ? mem[o] : 0xEEu;
      v |= static_cast<uint64_t>(b) << (8 * i);
    }
    return v;
  }

  void idle_inputs() {
    d.pub_valid_i = 0;
    d.pub_tag_i = 0;
    d.pub_index_i = 0;
    d.pub_generation_i = 0;
    d.pub_base_i = 0;
    d.pub_extent_i = 0;
    d.p_valid_i = 0;
    d.p_clip_id_i = 0;
    d.p_frame_no_i = 0;
  }

  void cycle() {
    if (pub_pending) {
      d.pub_valid_i = 1;
      d.pub_tag_i = pub_kind;
      d.pub_index_i = pub_index;
      d.pub_generation_i = pub_gen;
      d.pub_base_i = pub_base;
      d.pub_extent_i = pub_extent;
      pub_pending = false;
    }
    if (req_pending) {
      d.p_valid_i = 1;
      d.p_clip_id_i = req_clip;
      d.p_frame_no_i = req_frame;
      req_pending = false;
    }
    d.rsp_ready_i = 0;
    d.rsp_ok_i = 0;
    d.rsp_violation_i = 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;
    const bool req = (d.req_valid_o != 0);
    if (req && beats_left == 0) {
      const bool deny = (deny_after >= 0 && reads == deny_after);
      d.rsp_ready_i = 1;
      if (deny) {
        d.rsp_violation_i = 1;
      } else {
        d.rsp_ok_i = 1;
        // THE SHAPE RULE, CHECKED RATHER THAN ASSUMED. MEM.GUARD refuses a
        // request whose byte mask does not match its length, so a reader that
        // asked for 64 bytes with a partial mask would be denied in silicon
        // while passing every content check here.
        check(d.req_len_o == 64, "the read asks for a whole 64-byte line", 64, d.req_len_o);
        check(d.req_be_o == 0xFFFFFFFFFFFFFFFFull, "with the mask its length requires", 1,
              d.req_be_o == 0xFFFFFFFFFFFFFFFFull);
        check(d.req_write_o == 0, "and it is a READ", 0, d.req_write_o);
        check((d.req_addr_o & 63u) == 0, "on the 64-byte grid", 0, d.req_addr_o & 63u);
        beat_addr = d.req_addr_o;
        beats_left = 8;
      }
      ++reads;
      if (seen_req) ++reads_after_req;
    } else if (beats_left > 0) {
      d.beat_valid_i = 1;
      d.beat_data_i = word(beat_addr);
      beat_addr += 8;
      --beats_left;
    }

    d.eval();
    // The fill is combinational off the state the clock is about to leave, so
    // it is sampled HERE, before the edge, exactly as the store latches it.
    if (d.fill_we_o) {
      fills.push_back(Fill{d.fill_sel_o != 0, static_cast<uint8_t>(d.fill_bone_o),
                           static_cast<uint8_t>(d.fill_word_o), d.fill_data_o});
      if (seen_req) ++fills_after_req;
    }
    if (d.src_req_o) seen_req = true;
    zhao::tick(d);
    idle_inputs();
    d.eval();
  }

  // A publication and a request are ONE CYCLE OF THE SAME BENCH, driven
  // through `cycle()` rather than beside it. Ticking the DUT outside `cycle()`
  // leaves `beat_valid_i` asserted from the previous cycle and injects a
  // DUPLICATE BEAT into any read in flight -- which is not a thing MEM.GUARD
  // can do, and which made case 9 (a publication arriving mid-walk) measure the
  // bench instead of the block. Found by an assertion that was too weak:
  // "clips_o > 0" passed while the running walk was being corrupted.
  bool pub_pending = false;
  uint8_t pub_kind = 0;
  uint32_t pub_index = 0, pub_base = 0, pub_extent = 0;
  uint16_t pub_gen = 0;
  bool req_pending = false;
  uint16_t req_clip = 0, req_frame = 0;

  void publish(uint8_t kind, uint32_t index, uint16_t gen, uint32_t base, uint32_t extent) {
    pub_pending = true;
    pub_kind = kind;
    pub_index = index;
    pub_gen = gen;
    pub_base = base;
    pub_extent = extent;
    cycle();
  }

  void request(uint16_t clip_id, uint16_t frame_no) {
    fills.clear();
    reads_after_req = 0;
    fills_after_req = 0;
    seen_req = false;
    req_pending = true;
    req_clip = clip_id;
    req_frame = frame_no;
    cycle();
  }

  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) cycle();
  }

  // Load a page and let the walk finish, recording its fills from scratch.
  void load(uint8_t kind, const std::vector<uint8_t>& page, uint32_t index, uint16_t gen) {
    mem = page;
    fills.clear();
    reads = 0;
    publish(kind, index, gen, kBase, static_cast<uint32_t>(page.size()));
    run(600);
  }
};

// A small, ordered skeleton: every field distinctive, so a transposition shows.
std::vector<bd::BoneRecord> skeleton(int n) {
  std::vector<bd::BoneRecord> v;
  for (int i = 0; i < n; ++i) {
    bd::BoneRecord r;
    r.parent = static_cast<uint8_t>(i == 0 ? 0 : i - 1);
    r.tx = 0x0001'0000 + i * 0x0000'1111;
    r.ty = -(0x0000'2000 + i * 0x0000'0303);
    r.tz = 0x0004'0000 - i * 0x0000'0707;
    v.push_back(r);
  }
  return v;
}

std::vector<cp::Record> one_ladder_row() {
  cp::Record r;
  r.form_index = 0x000123u;
  r.bound_radius = 0x0002'0000;
  r.micro_error = 0x0000'0400;
  r.splat_error = 0x0001'0000;
  r.glint_error = 0x0002'0000;
  return std::vector<cp::Record>{r};
}

clp::Clip make_clip(uint16_t slot, int frames, int bones, int seed) {
  clp::Clip c;
  c.slot_id = slot;
  for (int f = 0; f < frames; ++f) {
    clp::Frame fr;
    fr.root_dx = 0x0000'1000 * (f + 1) + seed;
    fr.root_dy = -0x0000'0800 * (f + 1) - seed;
    fr.root_dz = 0x0000'2000 + f * 7 + seed;
    for (int b = 0; b < bones; ++b) {
      clp::Quat16 q;
      // Distinct in every lane, so a lane swap is visible; the values are not
      // required to be unit here -- this block moves bytes, it does not decode.
      q.w = static_cast<int16_t>(16000 - 13 * b - 3 * f - seed);
      q.x = static_cast<int16_t>(100 + b);
      q.y = static_cast<int16_t>(-200 - b - f);
      q.z = static_cast<int16_t>(300 + 2 * b + f);
      fr.bones.push_back(q);
    }
    c.frames.push_back(fr);
  }
  return c;
}

// The recorded body fill stream, rebuilt into records and compared field by
// field against what the reference reads out of the same bytes.
void check_body_fills(Bench& b, const std::vector<bd::BoneRecord>& want, const char* what) {
  char nm[160];
  std::vector<Fill> body;
  for (const Fill& f : b.fills)
    if (!f.sel) body.push_back(f);
  std::snprintf(nm, sizeof nm, "%s: exactly 4 words per bone, %zu bones", what, want.size());
  check(body.size() == want.size() * 4, nm, want.size() * 4, body.size());
  if (body.size() != want.size() * 4) return;
  for (size_t i = 0; i < want.size(); ++i) {
    // The four words, in order, addressed to this bone.
    bool addressed = true;
    for (int w = 0; w < 4; ++w) {
      const Fill& f = body[i * 4 + static_cast<size_t>(w)];
      addressed = addressed && f.bone == i && f.word == static_cast<uint8_t>(w);
    }
    std::snprintf(nm, sizeof nm, "%s: bone %zu is written as {bone=%zu, word=0..3}", what, i, i);
    check(addressed, nm, 1, addressed);
    if (!addressed) continue;
    const uint64_t w0 = body[i * 4 + 0].data;
    const uint64_t w1 = body[i * 4 + 1].data;
    const uint64_t w2 = body[i * 4 + 2].data;
    const uint64_t w3 = body[i * 4 + 3].data;
    const uint8_t parent = static_cast<uint8_t>(w0 & 0xFFu);
    const int32_t tx = static_cast<int32_t>(static_cast<uint32_t>(w0 >> 32));
    const int32_t ty = static_cast<int32_t>(static_cast<uint32_t>(w1 & 0xFFFF'FFFFu));
    const int32_t tz = static_cast<int32_t>(static_cast<uint32_t>(w1 >> 32));
    const int32_t itx = static_cast<int32_t>(static_cast<uint32_t>(w2 & 0xFFFF'FFFFu));
    const int32_t ity = static_cast<int32_t>(static_cast<uint32_t>(w2 >> 32));
    const int32_t itz = static_cast<int32_t>(static_cast<uint32_t>(w3 & 0xFFFF'FFFFu));
    std::snprintf(nm, sizeof nm, "%s: bone %zu parent", what, i);
    check(parent == want[i].parent, nm, want[i].parent, parent);
    std::snprintf(nm, sizeof nm, "%s: bone %zu rest translation", what, i);
    check(tx == want[i].tx && ty == want[i].ty && tz == want[i].tz, nm, 1,
          tx == want[i].tx && ty == want[i].ty && tz == want[i].tz);
    std::snprintf(nm, sizeof nm, "%s: bone %zu BAKED inv_rest", what, i);
    check(itx == want[i].inv_tx && ity == want[i].inv_ty && itz == want[i].inv_tz, nm, 1,
          itx == want[i].inv_tx && ity == want[i].inv_ty && itz == want[i].inv_tz);
  }
}

void check_quat_fills(Bench& b, const clp::Frame& want, const char* what) {
  char nm[160];
  std::vector<Fill> q;
  for (const Fill& f : b.fills)
    if (f.sel) q.push_back(f);
  std::snprintf(nm, sizeof nm, "%s: exactly one word per bone, %zu bones", what,
                want.bones.size());
  check(q.size() == want.bones.size(), nm, want.bones.size(), q.size());
  if (q.size() != want.bones.size()) return;
  for (size_t i = 0; i < want.bones.size(); ++i) {
    const int16_t w = static_cast<int16_t>(q[i].data & 0xFFFFu);
    const int16_t x = static_cast<int16_t>((q[i].data >> 16) & 0xFFFFu);
    const int16_t y = static_cast<int16_t>((q[i].data >> 32) & 0xFFFFu);
    const int16_t z = static_cast<int16_t>((q[i].data >> 48) & 0xFFFFu);
    std::snprintf(nm, sizeof nm, "%s: bone %zu is addressed as bone %zu", what, i, i);
    check(q[i].bone == i, nm, i, q[i].bone);
    // `qformats` 7.6 amendment C1: four s16 lanes in w,x,y,z order, which is
    // byte for byte `zhao_geom_bonesrc`'s 64-bit quat word.
    std::snprintf(nm, sizeof nm, "%s: bone %zu quat16 lanes {w,x,y,z}", what, i);
    check(w == want.bones[i].w && x == want.bones[i].x && y == want.bones[i].y &&
              z == want.bones[i].z,
          nm, 1,
          w == want.bones[i].w && x == want.bones[i].x && y == want.bones[i].y &&
              z == want.bones[i].z);
  }
}

std::vector<uint8_t> read_golden(const char* path) {
  std::vector<uint8_t> v;
  FILE* f = std::fopen(path, "rb");
  check(f != nullptr, "the committed golden opens", 1, f != nullptr);
  if (!f) return v;
  uint8_t buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + n);
  std::fclose(f);
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_clipread;  // heap + exit_hard: see zhao_sim.hpp
  Bench b(*top);
  b.idle_inputs();
  top->rsp_ready_i = 0;
  top->rsp_ok_i = 0;
  top->rsp_violation_i = 0;
  top->beat_valid_i = 0;
  top->src_ready_i = 1;
  top->rst_n = 0;
  top->eval();
  for (int i = 0; i < 3; ++i) zhao::tick(*top);
  top->rst_n = 1;
  top->eval();

  const int kBones = 6;
  const std::vector<bd::BoneRecord> bones_in = skeleton(kBones);
  std::vector<bd::BoneRecord> baked;
  const bool baked_ok = bd::bake_body(bones_in, baked);
  check(baked_ok, "the reference bakes the skeleton", 1, baked_ok);
  const std::vector<uint8_t> page8 = cp::build_with_body(one_ladder_row(), bones_in);
  check(!page8.empty(), "the reference builds a kind-8 page WITH a body", 1, !page8.empty());

  std::vector<clp::Clip> clips;
  clips.push_back(make_clip(3, 4, kBones, 0));
  clips.push_back(make_clip(9, 2, kBones, 5));
  const std::vector<uint8_t> page9 = clp::build(clips, kBones);
  check(!page9.empty(), "the reference builds a kind-9 clip page", 1, !page9.empty());

  // ---- 0. an empty reader answers nothing, and says so --------------------
  {
    b.mem = page9;
    b.request(3, 0);
    b.run(60);
    check(top->not_resident_o == 1, "a request before either page is resident is REFUSED", 1,
          top->not_resident_o);
    check(top->frames_o == 0, "and hands nothing over", 0, top->frames_o);
    check(b.fills.empty(), "and writes not one word into the store", 0, b.fills.size());
  }

  // ---- 1. the kind-8 body adopts whole, bone for bone ---------------------
  {
    b.load(cp::kPageKind, page8, 0x00ABCDu, 0x1234);
    check(top->bodies_o == 1, "the kind-8 body is adopted", 1, top->bodies_o);
    check(top->res_body_index_o == 0x00ABCDu, "and carries 5f.1's handle INDEX through",
          0x00ABCDu, top->res_body_index_o);
    check(top->res_body_gen_o == 0x1234, "and MEM.UPLOAD's 16-bit generation", 0x1234,
          top->res_body_gen_o);
    check_body_fills(b, baked, "kind-8 body");
    // THE READ COUNT. page header + body header + ceil(6/2) record lines = 5.
    check(b.reads == 5, "exactly five 64-byte reads: page header, body header, 3 record lines",
          5, b.reads);
  }

  // ---- 2. the kind-9 directory adopts whole -------------------------------
  {
    b.load(clp::kPageKind, page9, 0x00BEEFu, 0x5678);
    check(top->clips_o == 1, "the kind-9 directory is adopted", 1, top->clips_o);
    check(top->res_clip_index_o == 0x00BEEFu, "and carries its own handle index", 0x00BEEFu,
          top->res_clip_index_o);
    check(b.fills.empty(), "a DIRECTORY load writes nothing into the bone store", 0,
          b.fills.size());
    // page header + one directory line (two 32-byte rows share it) = 2.
    check(b.reads == 2, "exactly two reads: the header and one directory line", 2, b.reads);
  }

  // ---- 3. a frame, every quaternion, and the root -------------------------
  // The two stores now hold one creature. This is the case the whole block is
  // for, and the sub-case that discriminates is 3b.
  {
    b.mem = page9;
    b.reads = 0;
    b.request(3, 2);
    b.run(400);
    check(top->frames_o == 1, "the frame is fetched and handed over", 1, top->frames_o);
    check(top->src_bone_count_o == kBones, "with the skeleton's own bone count", kBones,
          top->src_bone_count_o);
    const clp::Frame& want = clips[0].frames[2];
    check(static_cast<int32_t>(top->root_dx_o) == want.root_dx &&
              static_cast<int32_t>(top->root_dy_o) == want.root_dy &&
              static_cast<int32_t>(top->root_dz_o) == want.root_dz,
          "the frame's root displacement reaches the decoder", 1,
          static_cast<int32_t>(top->root_dx_o) == want.root_dx &&
              static_cast<int32_t>(top->root_dy_o) == want.root_dy &&
              static_cast<int32_t>(top->root_dz_o) == want.root_dz);
    check_quat_fills(b, want, "clip 3 frame 2");
    // frame header + ceil(6*8/64) = 1 quat line.
    check(b.reads == 2, "exactly two reads for a 6-bone frame", 2, b.reads);

    // ---- 3b. FILL WHOLE, THEN ASK -- the latefetch mutant's own law -------
    check(b.reads_after_req == 0,
          "NO read is issued after src_req_o -- the mutant's late-fetch law", 0,
          b.reads_after_req);
    check(b.fills_after_req == 0, "and no word is written after it either", 0,
          b.fills_after_req);
    check(b.seen_req, "...and the handover did happen, so the two above are not vacuous", 1,
          b.seen_req);
  }

  // ---- 4. a DIFFERENT frame of a DIFFERENT clip ---------------------------
  // A reader that computed `frame_off + stride*f` wrongly answers frame 0 of
  // every clip and passes case 3.
  {
    b.mem = page9;
    b.reads = 0;
    b.request(9, 1);
    b.run(400);
    check(top->frames_o == 2, "a second frame, from the second clip slot", 2, top->frames_o);
    const clp::Frame& want = clips[1].frames[1];
    check(static_cast<int32_t>(top->root_dx_o) == want.root_dx,
          "slot 9 frame 1's own root, not slot 3's", want.root_dx,
          static_cast<int32_t>(top->root_dx_o));
    check_quat_fills(b, want, "clip 9 frame 1");
  }

  // ---- 5. a miss is a miss, and so is a frame past the end ---------------
  {
    const uint32_t frames_before = top->frames_o;
    b.mem = page9;
    b.request(77, 0);
    b.run(60);
    check(top->clip_miss_o == 1, "an unknown slot is counted", 1, top->clip_miss_o);
    b.request(3, 4);  // the clip has four frames, 0..3
    b.run(60);
    check(top->frame_oob_o == 1, "a frame at the count is counted", 1, top->frame_oob_o);
    check(top->frames_o == frames_before, "and neither hands anything over", frames_before,
          top->frames_o);
  }

  // ---- 6. bone_mismatch_o, WITH ITS NEGATIVE CONTROL (R95) ---------------
  // The detector must DISCRIMINATE, not merely move. Both halves run the same
  // request path; only the two bone counts differ.
  {
    check(top->bone_mismatch_o == 0,
          "NEGATIVE CONTROL: a matched skeleton and clip page leave it at ZERO", 0,
          top->bone_mismatch_o);
    std::vector<clp::Clip> wide;
    wide.push_back(make_clip(3, 2, 8, 1));
    const std::vector<uint8_t> page9w = clp::build(wide, 8);
    b.load(clp::kPageKind, page9w, 0x00C0DEu, 1);
    check(top->clips_o == 2, "an 8-bone clip page adopts legally on its own terms", 2,
          top->clips_o);
    const uint32_t frames_before = top->frames_o;
    b.request(3, 0);
    b.run(200);
    check(top->bone_mismatch_o == 1,
          "a 6-bone skeleton under an 8-bone clip page FIRES the mismatch", 1,
          top->bone_mismatch_o);
    check(top->frames_o == frames_before,
          "and the frame is refused -- never decoded against the wrong skeleton",
          frames_before, top->frames_o);
    // Put the matched page back for what follows.
    b.load(clp::kPageKind, page9, 0x00BEEFu, 0x5678);
  }

  // ---- 7. a refused page changes NOTHING ---------------------------------
  // Each refusal is followed by the same frame request that worked in case 3,
  // and the answer must be identical, bit for bit.
  {
    auto refetch_still_works = [&](const char* what) {
      b.mem = page9;
      b.request(3, 2);
      b.run(400);
      const clp::Frame& want = clips[0].frames[2];
      char nm[160];
      std::snprintf(nm, sizeof nm, "%s: the RESIDENT frame still reads back whole", what);
      check(static_cast<int32_t>(top->root_dx_o) == want.root_dx, nm, want.root_dx,
            static_cast<int32_t>(top->root_dx_o));
      check_quat_fills(b, want, what);
    };

    // 7a. wrong magic on the kind-9 header.
    {
      std::vector<uint8_t> p = page9;
      p[0] ^= 0xFFu;
      const uint32_t before = top->bad_magic_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->bad_magic_o == before + 1, "a wrong magic refuses the page", before + 1,
            top->bad_magic_o);
      refetch_still_works("after a bad magic");
    }

    // 7b. wrong version.
    {
      std::vector<uint8_t> p = page9;
      p[clp::kOffVersion] = 9;
      const uint32_t before = top->bad_magic_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->bad_magic_o == before + 1, "a wrong version refuses it too", before + 1,
            top->bad_magic_o);
      refetch_still_works("after a bad version");
    }

    // 7c. a frames_off off the 64-byte grid.
    {
      std::vector<uint8_t> p = page9;
      clp::put_u32(p.data() + clp::kOffFramesOff,
                   clp::get_u32(p.data() + clp::kOffFramesOff) + 8u);
      const uint32_t before = top->misaligned_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->misaligned_o == before + 1, "a frames_off off the read grid is MISALIGNED",
            before + 1, top->misaligned_o);
      refetch_still_works("after a misaligned frames_off");
    }

    // 7d. a bone count past the ceiling.
    {
      std::vector<uint8_t> p = page9;
      p[clp::kOffBoneCount] = 33;
      const uint32_t before = top->bad_bone_count_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->bad_bone_count_o == before + 1, "33 bones is past creature_rules 1.2",
            before + 1, top->bad_bone_count_o);
      refetch_still_works("after a bad bone count");
    }

    // 7e. more clips than CLIP_ROWS.
    {
      std::vector<clp::Clip> many;
      for (int i = 0; i < 20; ++i) many.push_back(make_clip(static_cast<uint16_t>(i), 1, kBones, i));
      const std::vector<uint8_t> p = clp::build(many, kBones);
      const uint32_t before = top->overflow_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->overflow_o == before + 1, "20 clips against CLIP_ROWS=16 OVERFLOWS",
            before + 1, top->overflow_o);
      refetch_still_works("after an overflow");
    }

    // 7f. a reserved header field that is not zero.
    {
      std::vector<uint8_t> p = page9;
      p[clp::kOffHdrRsv0] = 1;
      const uint32_t before = top->reserved_nz_o;
      b.load(clp::kPageKind, p, 1, 1);
      check(top->reserved_nz_o == before + 1, "a non-zero reserved byte refuses the page",
            before + 1, top->reserved_nz_o);
      refetch_still_works("after a reserved-field refusal");
    }

    // 7g. a kind-8 body claiming a rest ROTATION this build cannot decode.
    {
      std::vector<uint8_t> p = page8;
      const uint32_t body_off = cp::get_u32(p.data() + 8);
      p[body_off + 7] = 0x03u;  // flags: RIGID_REST plus an unknown bit
      const uint32_t before = top->not_rigid_o;
      const uint32_t bodies_before = top->bodies_o;
      b.load(cp::kPageKind, p, 1, 1);
      check(top->not_rigid_o == before + 1,
            "a body header whose flags are not exactly RIGID_REST is refused", before + 1,
            top->not_rigid_o);
      check(top->bodies_o == bodies_before, "and is NOT adopted", bodies_before,
            top->bodies_o);
    }

    // 7h. a truncated extent -- the page is shorter than it declares.
    {
      const uint32_t before = top->truncated_o;
      b.mem = page9;
      b.fills.clear();
      b.reads = 0;
      b.publish(clp::kPageKind, 1, 1, kBase, 64);  // header only
      b.run(200);
      check(top->truncated_o == before + 1, "an extent that does not reach the directory",
            before + 1, top->truncated_o);
      refetch_still_works("after a truncation");
    }
  }

  // ---- 8. a denied read abandons the page; another kind is ignored --------
  {
    const uint32_t before = top->denied_o;
    b.deny_after = 1;  // deny the SECOND read: the header has already landed
    b.load(clp::kPageKind, page9, 1, 1);
    b.deny_after = -1;
    check(top->denied_o == before + 1, "a MEM.GUARD violation abandons the walk", before + 1,
          top->denied_o);

    const uint32_t clips_before = top->clips_o;
    const uint32_t bodies_before = top->bodies_o;
    b.load(kOtherKind, page9, 1, 1);
    check(top->clips_o == clips_before && top->bodies_o == bodies_before,
          "a MATERIAL_SET publication on the same wire is ignored entirely", 1,
          top->clips_o == clips_before && top->bodies_o == bodies_before);
    check(b.reads == 0, "and issues no read at all", 0, b.reads);
  }

  // ---- 9. a publication ARRIVING DURING a walk is dropped, not queued -----
  {
    const uint32_t before = top->pages_dropped_o;
    const uint32_t clips_before = top->clips_o;
    b.mem = page9;
    b.reads = 0;
    b.publish(clp::kPageKind, 1, 1, kBase, static_cast<uint32_t>(page9.size()));
    b.cycle();
    b.cycle();
    b.publish(clp::kPageKind, 2, 2, kBase, static_cast<uint32_t>(page9.size()));
    b.run(400);
    check(top->pages_dropped_o == before + 1, "a publication while busy is DROPPED and counted",
          before + 1, top->pages_dropped_o);
    // ...and the FIRST walk still finishes. "Dropped" must mean the arriving
    // page is ignored, never that the running one is abandoned -- a reader
    // that dropped both would pass the counter check above.
    check(top->clips_o == clips_before + 1, "and the walk that was running still adopts",
          clips_before + 1, top->clips_o);
    check(top->res_clip_index_o == 1, "under the FIRST publication's handle index, not the "
          "dropped one's", 1, top->res_clip_index_o);
  }

  // ---- 10. the two committed GOLDENS, read by the RTL --------------------
  {
    // 10a. the kind-9 clip bank.
    const std::vector<uint8_t> g9 =
        read_golden(ZHAO_SOURCE_DIR "/tests/golden/creature_clip/clip_page_v1.bin");
    check(g9.size() == 704, "clip_page_v1.bin is the 704 bytes POSEPAGE froze", 704, g9.size());
    std::vector<clp::Clip> gclips;
    int gbones = 0;
    const clp::Verdict v = clp::decode(g9.data(), g9.size(), 64, gclips, &gbones);
    check(v == clp::Verdict::kOk, "the reference decodes it", 0, static_cast<int>(v));

    // 10b. the kind-8 body golden, whose skeleton it shares.
    const std::vector<uint8_t> gbody =
        read_golden(ZHAO_SOURCE_DIR "/tests/golden/creature_ladder/ladder_page_body_v1.bin");
    std::vector<bd::BoneRecord> gbone_recs;
    const uint32_t gbody_off = cp::get_u32(gbody.data() + 8);
    check(gbody_off != 0, "the golden ladder page declares a body_off", 1, gbody_off != 0);
    const bd::BodyVerdict bv =
        bd::decode_body(gbody.data() + gbody_off, gbody.size() - gbody_off, gbone_recs);
    check(bv == bd::BodyVerdict::kOk, "the reference decodes the golden body", 0,
          static_cast<int>(bv));
    check(static_cast<int>(gbone_recs.size()) == gbones,
          "THE TWO GOLDENS DESCRIBE ONE CREATURE: the bone counts agree", gbones,
          gbone_recs.size());

    // 10c. and the RTL reads BOTH FILES.
    b.load(cp::kPageKind, gbody, 0x000001u, 1);
    check_body_fills(b, gbone_recs, "golden kind-8 body");
    const uint32_t bodies_now = top->bodies_o;
    const uint32_t clips_now = top->clips_o;
    b.load(clp::kPageKind, g9, 0x000002u, 2);
    check(top->clips_o == clips_now + 1, "the golden clip bank adopts", clips_now + 1,
          top->clips_o);
    b.mem = g9;
    b.request(gclips[0].slot_id, static_cast<uint16_t>(gclips[0].frames.size() - 1));
    b.run(400);
    check_quat_fills(b, gclips[0].frames.back(), "golden clip, last frame");
    check(top->bodies_o == bodies_now, "and the golden body is still the resident skeleton",
          bodies_now, top->bodies_o);
    check(top->bone_mismatch_o == 1,
          "NEGATIVE CONTROL, second form: two goldens of one creature do NOT fire the "
          "mismatch",
          1, top->bone_mismatch_o);
  }

  // ---- 11. every counter was seen to move --------------------------------
  const bool all_fired =
      top->bodies_o > 0 && top->clips_o > 0 && top->frames_o > 0 && top->pages_dropped_o > 0 &&
      top->bad_magic_o > 0 && top->truncated_o > 0 && top->misaligned_o > 0 &&
      top->bad_bone_count_o > 0 && top->bone_mismatch_o > 0 && top->overflow_o > 0 &&
      top->not_rigid_o > 0 && top->reserved_nz_o > 0 && top->denied_o > 0 &&
      top->clip_miss_o > 0 && top->frame_oob_o > 0 && top->not_resident_o > 0;
  check(all_fired, "every counter this block owns was FIRED by stimulus", 1, all_fired);

  std::printf(
      "[geom_clipread_directed] bodies=%u clips=%u frames=%u dropped=%u bad_magic=%u "
      "truncated=%u misaligned=%u bad_bones=%u mismatch=%u overflow=%u not_rigid=%u "
      "rsv_nz=%u denied=%u clip_miss=%u frame_oob=%u not_resident=%u\n",
      top->bodies_o, top->clips_o, top->frames_o, top->pages_dropped_o, top->bad_magic_o,
      top->truncated_o, top->misaligned_o, top->bad_bone_count_o, top->bone_mismatch_o,
      top->overflow_o, top->not_rigid_o, top->reserved_nz_o, top->denied_o, top->clip_miss_o,
      top->frame_oob_o, top->not_resident_o);
  top->final();
  return zhao::report_and_exit("geom_clipread_directed");
}
