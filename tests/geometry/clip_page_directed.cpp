// clip_page_directed.cpp -- the CLIP_BANK page (cartridge kind 9), against the
// packer's golden and against its own refusals.
//
// Owner ruling R90's recommendation item 1 asked for "the body section AND A
// MINIMAL KIND-9 FRAME WITH A ZREF MODEL". The body half landed on 2026-09-21
// with `geom_bonesrc_directed` as its evidence; this is the kind-9 half's.
//
// WHAT ACTUALLY DISCRIMINATES, named up front:
//
//   1. THE MODEL REPRODUCES THE PACKER'S GOLDEN BYTE FOR BYTE. Two files state
//      this layout -- `reference/include/zref/zref_clip_page.hpp` and
//      `tools/pack/mkclipbank.py` -- and two statements of one layout drift
//      unless an ARTEFACT pins them. A layout edit that misses either goes red
//      here. This is `geom_ladderbank_directed`'s discipline, one page kind
//      over, and it is copied deliberately.
//   2. THE GOLDEN ROUND-TRIPS THROUGH `decode`, FIELD FOR FIELD. A builder and
//      a decoder that agree with each other and not with the bytes is the
//      cancelling-errors shape; comparing the decode against the FIXTURE the
//      packer was given, rather than against `build`'s own output, is what
//      separates them.
//   3. EVERY VERDICT IS SEEN TO FIRE. A `Verdict` enum with nine members and a
//      test that only ever reaches `kOk` is nine claims. Each refusal is
//      reached here by corrupting exactly one field of a page that was
//      otherwise legal, so a verdict that fires for the wrong reason is a
//      failure rather than a pass.
//   4. THE FRAME ADDRESS IS READ OFF THE PAGE, NOT RECOMPUTED. An RTL reader's
//      address chain is `frame_off + stride * f`, and `frame_offset` is the one
//      statement of it. Case 5 differences that against a byte-level search for
//      each frame's own root displacement -- so the arithmetic is checked
//      against where the data ACTUALLY IS rather than against itself.
//   5. THE FRAME BYTES ARE creature_rules 2.1's, UNMOVED. The container spaces
//      frames onto MEM.GUARD's 64-byte grid and that spacing is the only thing
//      R90's lift authorised inventing. Case 6 asserts the root displacement
//      still occupies bytes 0..11 of its slot and that every quaternion is one
//      aligned 64-bit word whose lanes are w,x,y,z -- i.e. that the frozen half
//      is where 2.1 says it is.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zhao_sim.hpp"
#include "zref/zref_clip_page.hpp"

namespace cl = zref::clip_page;

static void check(bool cond, const char* what) {
  zhao::check(cond, what, 1u, cond ? 1u : 0u);
}

namespace {

// `tools/pack/mkclipbank.py`'s GOLDEN_CLIPS, restated here ONLY as the input
// the packer was given. Every byte compared below comes from the file.
constexpr int kBones = 6;

cl::Quat16 bone_quat(int b, int f) {
  cl::Quat16 q;
  q.w = static_cast<int16_t>(16384 - 300 * b + 11 * f);
  q.x = static_cast<int16_t>(1500 * (b + 1) + 11 * f);
  q.y = static_cast<int16_t>(700 * b + 11 * f);
  q.z = static_cast<int16_t>(-400 * b + 11 * f);
  return q;
}

cl::Frame golden_frame(int f) {
  cl::Frame fr;
  fr.root_dx = 4096 * f;
  fr.root_dy = -2048 * f;
  fr.root_dz = 1024 * f;
  for (int b = 0; b < kBones; ++b) fr.bones.push_back(bone_quat(b, f));
  return fr;
}

std::vector<cl::Clip> golden_clips() {
  cl::Clip a;
  a.slot_id = 7;
  a.frames = {golden_frame(0), golden_frame(1), golden_frame(2)};
  cl::Clip b;
  b.slot_id = 0;
  b.frames = {golden_frame(3)};
  b.event_count = 2;
  b.event_bytes = {0x01, 0x00, 0x2A, 0x00, 0x02, 0x00, 0x5B, 0x00};
  return {a, b};
}

// mkclipbank.py's GOLDEN_OWNER: the form this bank animates, supplied by the
// asset definition and never inferred (owner ruling 2026-09-21, section 2).
constexpr uint32_t kGoldenOwner = 0x000101u;

std::vector<uint8_t> read_golden() {
  std::vector<uint8_t> out;
  const char* path = ZHAO_SOURCE_DIR "/tests/golden/creature_clip/clip_page_v2.bin";
  std::FILE* f = std::fopen(path, "rb");
  check(f != nullptr, "the committed clip-page golden opens");
  if (!f) return out;
  uint8_t buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
    out.insert(out.end(), buf, buf + n);
  std::fclose(f);
  return out;
}

// Decode a page and report only its verdict. Used by every refusal case, so a
// verdict is always reached through the same door the reader uses.
cl::Verdict verdict_of(const std::vector<uint8_t>& p, int rows = cl::kMaxClips) {
  std::vector<cl::Clip> out;
  return cl::decode(p.data(), p.size(), rows, out);
}

const char* name_of(cl::Verdict v) {
  switch (v) {
    case cl::Verdict::kOk: return "kOk";
    case cl::Verdict::kBadMagic: return "kBadMagic";
    case cl::Verdict::kBadVersion: return "kBadVersion";
    case cl::Verdict::kTruncated: return "kTruncated";
    case cl::Verdict::kBadBoneCount: return "kBadBoneCount";
    case cl::Verdict::kBadClipCount: return "kBadClipCount";
    case cl::Verdict::kBadFrameCount: return "kBadFrameCount";
    case cl::Verdict::kMisaligned: return "kMisaligned";
    case cl::Verdict::kDuplicateSlot: return "kDuplicateSlot";
    case cl::Verdict::kReservedNz: return "kReservedNz";
    case cl::Verdict::kBadOwner: return "kBadOwner";
  }
  return "?";
}

}  // namespace

int main() {
  const std::vector<cl::Clip> fixture = golden_clips();
  const std::vector<uint8_t> golden = read_golden();
  check(!golden.empty(), "the golden is not empty");
  if (golden.empty()) return zhao::report_and_exit("clip_page_directed");

  // ---- 1. the model reproduces the packer, byte for byte -------------------
  const std::vector<uint8_t> built = cl::build(fixture, kBones, kGoldenOwner);
  check(built.size() == golden.size(), "zref::clip_page::build matches the golden's length");
  check(built == golden,
        "zref::clip_page::build reproduces tools/pack/mkclipbank.py's golden byte for byte");
  if (built != golden) {
    for (size_t i = 0; i < built.size() && i < golden.size(); ++i)
      if (built[i] != golden[i]) {
        std::printf("[clip_page_directed] first difference at byte %zu: %02X vs %02X\n",
                    i, built[i], golden[i]);
        break;
      }
  }

  // ---- 2. the golden round-trips against the FIXTURE ----------------------
  std::vector<cl::Clip> back;
  int bone_count = 0;
  const cl::Verdict v =
      cl::decode(golden.data(), golden.size(), cl::kMaxClips, back, &bone_count);
  check(v == cl::Verdict::kOk, "the golden decodes kOk");
  check(bone_count == kBones, "the golden's bone_count reads back as 6");
  check(back.size() == fixture.size(), "the golden carries both clips");
  bool fields_match = back.size() == fixture.size();
  for (size_t i = 0; fields_match && i < back.size(); ++i) {
    fields_match = back[i].slot_id == fixture[i].slot_id &&
                   back[i].event_count == fixture[i].event_count &&
                   back[i].frames.size() == fixture[i].frames.size() &&
                   back[i].event_bytes == fixture[i].event_bytes;
    for (size_t f = 0; fields_match && f < back[i].frames.size(); ++f) {
      const cl::Frame& g = back[i].frames[f];
      const cl::Frame& w = fixture[i].frames[f];
      fields_match = g.root_dx == w.root_dx && g.root_dy == w.root_dy &&
                     g.root_dz == w.root_dz && g.bones.size() == w.bones.size();
      for (size_t b = 0; fields_match && b < g.bones.size(); ++b)
        fields_match = g.bones[b].w == w.bones[b].w && g.bones[b].x == w.bones[b].x &&
                       g.bones[b].y == w.bones[b].y && g.bones[b].z == w.bones[b].z;
    }
  }
  check(fields_match,
        "every clip, frame, root and quat16 lane decodes back to the packer's fixture");

  // ---- 3. the lookup is a lookup ------------------------------------------
  // Slot 0 is the SECOND row on purpose (see the packer): a reader that
  // answers row zero, or that assumes slot == row, is caught here.
  const cl::Clip* hit = nullptr;
  check(cl::find_clip(back, 0, &hit) && hit->frames.size() == 1,
        "slot 0 resolves to the SECOND directory row, not to row zero");
  check(cl::find_clip(back, 7, &hit) && hit->frames.size() == 3,
        "slot 7 resolves to the first row");
  check(!cl::find_clip(back, 9, &hit), "a slot with no row MISSES");

  // ---- 4. the stride arithmetic is the frozen frame, spaced ---------------
  check(cl::frame_bytes(32) == 268,
        "creature_rules 2.1's frame is 268 bytes at 32 bones, unchanged");
  check(cl::frame_bytes(kBones) == 12 + 8 * kBones,
        "a 6-bone frame is 12 B of root plus 6 x 8 B of quat16");
  check(cl::frame_stride(kBones) == 128, "a 6-bone frame occupies two 64-byte lines");
  check(cl::frame_stride(32) == 320, "a 32-bone frame occupies five 64-byte lines");
  check(cl::frame_stride(8) == 128, "the stride is 64-byte shaped at every width");

  // ---- 5. the address chain, differenced against where the bytes ARE ------
  // An RTL reader computes `frame_off + stride * f`. Rather than recompute
  // that here -- which would compare the model with itself -- each frame is
  // located by searching the page for its own root displacement, which is
  // unique per frame because the packer's fixture walks the root.
  bool addr_ok = true;
  for (size_t row = 0; row < back.size(); ++row) {
    for (size_t f = 0; f < back[row].frames.size(); ++f) {
      size_t off = 0;
      if (!cl::frame_offset(golden.data(), golden.size(), static_cast<int>(row), f, &off)) {
        addr_ok = false;
        break;
      }
      const cl::Frame& fr = back[row].frames[f];
      uint8_t want[12];
      cl::put_i32(want + 0, fr.root_dx);
      cl::put_i32(want + 4, fr.root_dy);
      cl::put_i32(want + 8, fr.root_dz);
      // The root must be AT that offset...
      if (off + 12 > golden.size() || std::memcmp(golden.data() + off, want, 12) != 0) {
        addr_ok = false;
        break;
      }
      // ...and nowhere else, or "found it" would mean nothing. Frame 0 of the
      // first clip is all zeroes and is skipped: a zero root is legitimately
      // ambiguous against the page's padding, which is itself worth saying
      // out loud rather than papering over.
      if (fr.root_dx != 0 || fr.root_dy != 0 || fr.root_dz != 0) {
        int found = 0;
        for (size_t i = 0; i + 12 <= golden.size(); i += 4)
          if (std::memcmp(golden.data() + i, want, 12) == 0) ++found;
        if (found != 1) addr_ok = false;
      }
      if (!addr_ok) break;
    }
    if (!addr_ok) break;
  }
  check(addr_ok,
        "frame_off + stride*f lands on each frame's own root displacement, and on "
        "nothing else in the page");

  // ---- 6. the frozen half is where creature_rules 2.1 says it is ----------
  {
    size_t off = 0;
    check(cl::frame_offset(golden.data(), golden.size(), 0, 1, &off),
          "clip row 0 frame 1 has an offset");
    check(off % 64 == 0, "a frame begins on a 64-byte line boundary");
    // bytes 0..11 are the root; 12..63 are the declared hole; 64.. are the
    // quaternions, one aligned 64-bit word per bone, lanes w,x,y,z.
    bool quats_ok = (off + cl::frame_stride(kBones)) <= golden.size();
    for (int b = 0; quats_ok && b < kBones; ++b) {
      const uint8_t* q = golden.data() + off + cl::kFrameHeaderBytes + 8 * b;
      const cl::Quat16 want = bone_quat(b, 1);
      quats_ok = cl::get_i16(q + 0) == want.w && cl::get_i16(q + 2) == want.x &&
                 cl::get_i16(q + 4) == want.y && cl::get_i16(q + 6) == want.z;
    }
    check(quats_ok,
          "every bone's quat16 is ONE 8-byte-aligned word at slot+64, lanes w,x,y,z "
          "(qformats 7.6 C1) -- the frozen bytes, only spaced");
    bool hole_zero = true;
    for (size_t i = cl::kRootBytes; i < cl::kFrameHeaderBytes; ++i)
      if (golden[off + i] != 0u) hole_zero = false;
    check(hole_zero, "the frame header's declared hole is zero, not silently used");
  }

  // ---- 7. every verdict fires, one corrupted field at a time --------------
  int fired = 0;
  const auto expect = [&](std::vector<uint8_t> p, cl::Verdict want, const char* what) {
    const cl::Verdict got = verdict_of(p);
    const bool ok = got == want;
    zhao::check(ok, what, static_cast<uint32_t>(want), static_cast<uint32_t>(got));
    if (ok) ++fired;
    else std::printf("[clip_page_directed] %s: wanted %s, got %s\n", what,
                     name_of(want), name_of(got));
  };

  expect(golden, cl::Verdict::kOk, "the untouched golden is kOk");

  {
    std::vector<uint8_t> p = golden;
    p[0] ^= 0xFFu;
    expect(p, cl::Verdict::kBadMagic, "a wrong magic is kBadMagic");
  }
  {
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kOffVersion,
                static_cast<uint16_t>(cl::kVersion + 1));
    expect(p, cl::Verdict::kBadVersion, "a future version is kBadVersion");
  }
  {
    // AND THE PAST ONE. v1 carried no owner, so it is refused rather than read
    // with a zero owner -- owner ruling of 2026-09-21, section 2.
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kOffVersion, cl::kVersionV1);
    expect(p, cl::Verdict::kBadVersion, "an UNOWNED v1 page is kBadVersion too");
  }
  {
    std::vector<uint8_t> p = golden;
    p.resize(cl::kHeaderBytes - 1);
    expect(p, cl::Verdict::kTruncated, "a page shorter than its header is kTruncated");
  }
  {
    std::vector<uint8_t> p = golden;
    p.resize(cl::kHeaderBytes + 8);
    expect(p, cl::Verdict::kTruncated, "a page whose directory runs past it is kTruncated");
  }
  {
    std::vector<uint8_t> p = golden;
    p[cl::kOffBoneCount] = 0;
    expect(p, cl::Verdict::kBadBoneCount, "a zero bone_count is kBadBoneCount");
  }
  {
    std::vector<uint8_t> p = golden;
    p[cl::kOffBoneCount] = cl::kMaxBones + 1;
    expect(p, cl::Verdict::kBadBoneCount,
           "a bone_count past creature_rules 1.2's ceiling is kBadBoneCount");
  }
  {
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kOffClipCount, 0);
    expect(p, cl::Verdict::kBadClipCount, "a zero clip_count is kBadClipCount");
  }
  {
    // The bank's rows, not the page's own field: a page declaring more clips
    // than the reader holds is refused WHOLE rather than truncated.
    std::vector<cl::Clip> out;
    const cl::Verdict got = cl::decode(golden.data(), golden.size(), 1, out);
    zhao::check(got == cl::Verdict::kBadClipCount,
                "a page with more clips than the reader's rows is refused whole",
                static_cast<uint32_t>(cl::Verdict::kBadClipCount),
                static_cast<uint32_t>(got));
    if (got == cl::Verdict::kBadClipCount) ++fired;
    check(out.empty(), "a refused page leaves the reader's bank EMPTY");
  }
  {
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kHeaderBytes + cl::kOffFrameCount, 0);
    expect(p, cl::Verdict::kBadFrameCount, "a clip declaring zero frames is kBadFrameCount");
  }
  {
    std::vector<uint8_t> p = golden;
    const uint32_t off = cl::get_u32(p.data() + cl::kHeaderBytes + cl::kOffFrameOff);
    cl::put_u32(p.data() + cl::kHeaderBytes + cl::kOffFrameOff, off + 4);
    expect(p, cl::Verdict::kMisaligned, "a frame off the 64-byte read grid is kMisaligned");
  }
  {
    std::vector<uint8_t> p = golden;
    cl::put_u32(p.data() + cl::kOffDirOff, 128);
    expect(p, cl::Verdict::kMisaligned, "a directory anywhere but 64 is kMisaligned");
  }
  {
    // Two rows for one slot. The reader answers the FIRST match, so a page
    // whose meaning depends on row order is refused rather than served.
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kHeaderBytes + cl::kClipBytes + cl::kOffSlotId, 7);
    expect(p, cl::Verdict::kDuplicateSlot, "two rows for one slot_id is kDuplicateSlot");
  }
  {
    std::vector<uint8_t> p = golden;
    p[cl::kOffHdrRsv0] = 1;
    expect(p, cl::Verdict::kReservedNz, "a non-zero header reserved byte is kReservedNz");
  }
  {
    std::vector<uint8_t> p = golden;
    cl::put_u16(p.data() + cl::kHeaderBytes + cl::kOffSlotId + 6, 1);
    expect(p, cl::Verdict::kReservedNz,
           "a non-zero directory reserved u16 is kReservedNz");
  }
  {
    // The frame header's declared hole. A packer that starts using it without
    // this build knowing is a format change and must be refused, not ignored.
    std::vector<uint8_t> p = golden;
    size_t off = 0;
    cl::frame_offset(p.data(), p.size(), 0, 0, &off);
    p[off + cl::kRootBytes] = 0xA5u;
    expect(p, cl::Verdict::kReservedNz,
           "a byte written into the frame header's declared hole is kReservedNz");
  }

  // ---- 8. the builder refuses what the decoder refuses --------------------
  // A packer that emits a page its own reader rejects is the two-owners shape.
  {
    std::vector<cl::Clip> bad = fixture;
    bad[1].slot_id = 7;
    check(cl::build(bad, kBones, kGoldenOwner).empty(),
          "build REFUSES a duplicate slot_id");
    check(cl::build({}, kBones, kGoldenOwner).empty(),
          "build REFUSES a bank with no clips");
    check(cl::build(fixture, 0, kGoldenOwner).empty(), "build REFUSES a zero bone_count");
    check(cl::build(fixture, cl::kMaxBones + 1, kGoldenOwner).empty(),
          "build REFUSES a bone_count past the ceiling");
    check(cl::build(fixture, kBones, kGoldenOwner, 1).empty(),
          "build REFUSES a bank past the reader's rows");
    std::vector<cl::Clip> narrow = fixture;
    narrow[0].frames[0].bones.pop_back();
    check(cl::build(narrow, kBones, kGoldenOwner).empty(),
          "build REFUSES a frame that is not the bank's width");
  }

  // ---- OWNERSHIP, owner ruling of 2026-09-21 section 2 --------------------
  {
    check(cl::kVersion == 2, "the CLIP_BANK page is at version 2");
    check(cl::kOffOwnerForm == 20, "its owner word is at bytes 20..23");
    check(cl::kHeaderBytes == 64,
          "and the header is STILL 64 bytes -- the word went into existing padding");
    check(cl::page_owner_word(golden.data(), golden.size()) == kGoldenOwner,
          "the golden's owner word reads back as the form the packer was GIVEN");

    // The owner is the only difference between two otherwise identical banks.
    const std::vector<uint8_t> other = cl::build(fixture, kBones, 0x010101u);
    check(other.size() == golden.size(),
          "a bank for a DIFFERENT form is byte-for-byte the same size");
    size_t diffs = 0;
    for (size_t i = 0; i < other.size() && i < golden.size(); ++i)
      if (other[i] != golden[i]) ++diffs;
    check(diffs > 0 && diffs <= 4,
          "and differs ONLY inside the owner word -- which is exactly why nothing "
          "before this version could tell the two apart");

    check(cl::build(fixture, kBones, 0x01000101u).empty(),
          "build REFUSES an owner setting bits 31:24");
    std::vector<uint8_t> high = golden;
    high[cl::kOffOwnerForm + 3] = 0x01u;
    check(verdict_of(high) == cl::Verdict::kBadOwner,
          "decode REFUSES a stored owner word with a non-zero high byte");

    // A v1 page is identifiable and refused: the ruling's "unowned v1
    // CLIP_BANK data is not accepted on the new posed-render path".
    std::vector<uint8_t> v1;
    {
      const char* path = ZHAO_SOURCE_DIR "/tests/golden/creature_clip/clip_page_v1.bin";
      std::FILE* f = std::fopen(path, "rb");
      check(f != nullptr, "the historical v1 golden is still committed");
      if (f) {
        uint8_t buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
          v1.insert(v1.end(), buf, buf + n);
        std::fclose(f);
      }
    }
    if (!v1.empty()) {
      check(cl::page_version(v1.data(), v1.size()) == cl::kVersionV1,
            "and still declares version 1 -- it stays IDENTIFIABLE");
      check(verdict_of(v1) == cl::Verdict::kBadVersion,
            "and decode refuses it: an unowned bank cannot be upgraded by a reader, "
            "because nothing in its bytes names a form");
    }

    // Index zero is a form, not a sentinel.
    const std::vector<uint8_t> zero = cl::build(fixture, kBones, 0u);
    uint32_t zowner = 0xFFFFFFFFu;
    std::vector<cl::Clip> zback;
    check(cl::decode(zero.data(), zero.size(), cl::kMaxClips, zback, nullptr, &zowner) ==
              cl::Verdict::kOk,
          "a bank owned by form ZERO is an ordinary bank");
    check(zowner == 0u, "and decodes with owner 0 rather than 'absent'");
  }

  std::printf("[clip_page_directed] golden %zu bytes, %zu clips, %d bones, stride %zu, "
              "%d verdicts fired\n",
              golden.size(), back.size(), bone_count, cl::frame_stride(kBones), fired);
  return zhao::report_and_exit("clip_page_directed");
}
