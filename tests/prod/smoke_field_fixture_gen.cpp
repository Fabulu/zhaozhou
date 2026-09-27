// smoke_field_fixture_gen.cpp -- the console smoke bench's TERRAIN FIELD
// FIXTURE: a REAL Earth program, lowered by the REAL production library, into
// the two byte-streams the composed console needs to run one.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS A COMMITTED GENERATOR AND NOT BENCH CODE
// ---------------------------------------------------------------------------
// `reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt` section
// 13.7 commissions a positive composed smoke mode and fences it:
//
//   > A bring-up bench may act as HPS by submitting bytes produced by this real
//   > software path. It may NOT hand the patch a ready-made height result, an
//   > idealized field answer, or an untracked association assembled differently
//   > from production.
//
// So the bench may not hand-build uops. Everything below comes out of
// `zfield::decode` -> `zfield::plan` -> `zfield::prepare` ->
// `zfield::host_plan::lower`, the same four calls `tools/field/pack_field_host.cpp`
// makes, and the capsule is `serialize_program_image`'s own bytes.
// `zhao_console_core.sv` entry I34 names this file's absence as a blocker --
// "NO C++ HELPER TURNS A `HostPlan` INTO DOORBELL LOAD WORDS ... that
// translation is new code, and it belongs in a committed fixture generator
// beside `smoke_geom_fixture_gen.cpp`, not in the bench." This is that file.
//
// ---------------------------------------------------------------------------
// THE PROGRAM, AND WHY IT IS NO LONGER wave_pool
// ---------------------------------------------------------------------------
// `u_field_host` is composed at `.REGS (32)` (`zhao_console_core.sv:36563`).
// Entry I34's blocker list says `crater_ring` needs 36 physical registers and
// that the remedy is to widen REGS. MEASURED 2026-09-27, with the packer built
// from this tree, sweeping `--scalar-base` at `--registers 32`:
//
//     crater_ring   REFUSED at every scalar base (the entry is right)
//     impact_wave   LOWERS at scalar-base 7, 8, 9
//     wave_pool     LOWERS at scalar-base 8, 9, 11, 12, 13
//
// So REGS=32 does NOT have to move: the ceiling is a function of `scalar_base`,
// which is a LowerOptions field the entry's own remedy list omits.
//
// BUT wave_pool CANNOT SATISFY THE OWNER'S CLAUSE 3, and that is why this
// fixture now stages `scorch_wash` instead. wave_pool is a HEIGHT spell whose
// out-lane 2 is `b.ldc(MAT_SOIL)` -- the BARE id 1. `spec/qformats.md` sec 14.1
// makes out-lane 2 the 32-bit MATERIAL TOKEN, whose top byte must be 0xE1, so a
// bare id has tag 0x00 and `zmt_tag_ok` REFUSES it. Measured in this very
// console by packet NOPROG: `terrmat field_composed=0 token_refused=1024`, one
// refusal per compose-cache cell. The tag check was doing its job; the program
// had nothing legal to say. `crater_ring` emits bare ids too, so NO program in
// the corpus could write a material the fabric would accept.
//
// `scorch_wash` (compiler/src/field_ir/scorch_wash.ts, hash 0xFDB4FE21) is the
// first one that can. It is built from the same builder/alloc/serialize chain --
// the single sanctioned program source path -- declares the same FOUR canonical
// outputs so `required_mask` stays 0x0F (I34's four Earth channels), and writes
// out-lane 2 as a real v1 token.
//
// IT WRITES ZERO HEIGHT AND ZERO VELOCITY, WHICH IS THE EXPERIMENTAL DESIGN.
// `zhao_terrain_patch.sv:339` composes field height ADDITIVELY, so height 0
// leaves every lattice height bit-identical to the authored terrain. The covered
// run therefore keeps the plain run's geometry, fragment count and pixel count,
// and the ONLY thing that moves is the mosaic tile the texture island fetches.
// A program that both deformed AND painted would move that tile for two possible
// reasons -- the material landed, or the deformed ground sampled elsewhere -- and
// CLAUDE.md's rule is "compare like with like, or do not compare".
//
// NOTE WHAT THIS FIXTURE THEREFORE NO LONGER DEMONSTRATES, said out loud rather
// than left for someone to notice: the composed console's HEIGHT channel is no
// longer exercised with a nonzero write by this mode. That channel's evidence is
// `composepub_acceptance` case 2 at the leaf, and NOPROG's recorded console run
// of wave_pool (`fldearth runs=1089 noprog=0 faults=0`). Swapping the program is
// a deliberate trade of a channel this mode already proved for the one it never
// could.
//
// ---------------------------------------------------------------------------
// TWO STREAMS, BECAUSE THE CONSOLE HAS TWO DOORS AND THEY ARE NOT THE SAME DOOR
// ---------------------------------------------------------------------------
// This was the architectural finding of the packet and it is written down here
// because it is not obvious from any one file:
//
//   1. THE CAPSULE goes to `zhao_field_loader` over the REAL HPS bridge
//      (`hps_req_o`, arbiter client 5), as an FH2 INSTALL_CAPSULE. The loader
//      validates it, SEALs it, and PUBLISHES `pub_handle_o` / `pub_prog_hash_o`
//      / `pub_ready_o`. This is the ONLY thing that lets
//      `zhao_terrain_fieldlist:304` resolve a TerrainField record's `program`
//      handle -- it sweeps that publication port and matches on the handle.
//      Without the capsule, every record is `noprog`.
//
//   2. THE LOAD WORDS go to `zhao_field_host_v2` through the doorbell's op-0
//      LOAD arm. The loader owns the BACKING STORE's descriptor table (FH15);
//      it does NOT write the host's microcode -- it has no `ld_*` port. So the
//      executable must also arrive as load words, HEADER LAST, which is what
//      marks the slot runnable.
//
// Both streams are derived from ONE `HostPlan` here, so the capsule the loader
// seals and the microcode the host executes cannot describe different programs.
// That is the whole reason this is one generator and not two.
//
// ---------------------------------------------------------------------------
// FRESHNESS
// ---------------------------------------------------------------------------
// Writes tests/prod/smoke_field_fixture.svh, which the bench `include`s under
// `ZHAO_SMOKE_FIELD_ACTIVE`. The ctest `smoke_field_fixture_fresh` re-runs this
// with --check and fails if the committed header differs, so the capsule, the
// load words and the handle cannot drift apart: they are one file's output.
//
// ENFORCED-BY: tests/prod/run_console_core_smoke.ps1 -FieldActive
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_host_plan.hpp"
#include "zfield/zfield_plan.hpp"

#include "zref/zref_fieldir.hpp"

#include "../../compiler/tests/generated/scorch_wash.hpp"

namespace hp_ns = zfield::host_plan;

namespace {

// ---------------------------------------------------------------------------
// THE KNOBS. Every one of these is a named editable constant, per CLAUDE.md's
// "never remove the owner's control in the name of fidelity": a value derived
// from a measurement is still a value somebody may need to change.
// ---------------------------------------------------------------------------

// The composed ceiling. `zhao_console_core.sv:36563` -- `.REGS (32)` on
// `u_field_host`. Lowering is checked AGAINST this rather than assuming the
// library default of 64, so a console that widened REGS and a fixture that did
// not cannot silently disagree.
constexpr int kRegisterCeiling = 32;

// MEASURED, NOT GUESSED, AND RE-MEASURED FOR scorch_wash.
//
// It was 12 for wave_pool. `scorch_wash` REFUSES at 12 -- and at every other
// scalar base -- with "BAD_IMAGE: an output ordinal whose source register lies
// outside the capture window", which is not a scalar-base problem at all. That
// cost a sweep to establish and the finding is worth the space:
//
// THERE IS A THIRD KNOB AND NOBODY WAS SETTING IT. `LowerOptions::out_base`
// defaults to 0, so the capture window is registers [0, out_lanes) = [0,7).
// wave_pool lowers at that default by luck: two of its four ordinals resolve to
// PREPARED_SCALARs (which are NOT window-checked, by construction) and its
// VECTOR_REG ordinals land at source registers 3, 4 and 6 -- inside [0,7) with
// one to spare. `scorch_wash`'s vector ordinals land at 5 and 7, and 7 is one
// past the window, so it refused for a reason that has nothing to do with
// register pressure.
//
// Entry I34's remedy list for this refusal class says "widen REGS". FIELDACTIVE
// then found `scalar_base`, which the list omits. `out_base` is a THIRD knob
// that neither names, and it is the one that mattered here. No REGS widening and
// no ALM purchase is owed.
//
// THE FULL (scalar_base, out_base) GRID at register_ceiling 32, measured with
// the library built from this tree -- 25 pairs lower AND pass the output
// contract, and the high-water is a function of scalar_base alone:
//
//     scalar_base 8  -> high_water 28    out_base 1..5
//     scalar_base 9  -> high_water 29    out_base 1..5
//     scalar_base 10 -> high_water 30    out_base 1..5
//     scalar_base 11 -> high_water 31    out_base 1..5
//     scalar_base 12 -> high_water 32    out_base 1..5
//     everything else refuses
//
// 8 is chosen because it is the minimum high-water, FOUR registers under the
// composed ceiling -- wave_pool ran at 31, one under. out_base 3 is chosen from
// the five that work because it puts the two vector ordinals at window
// positions 2 and 4 (mask 0x14), i.e. centred, so a future output landing one
// register either side still fits.
constexpr int kScalarBase = 8;

// The base of the CAPTURE WINDOW: physical register `out_base + k` is window
// position k. Must be set explicitly -- the library default of 0 is a window
// that this program's outputs fall outside of. See the grid above.
constexpr int kOutBase = 3;

// Earth's varying lanes: 0 = cx, 1 = cz. `reference/src/zrender/terrain.cpp`
// compose_lattice puts the vertex world x and z in in[0] and in[1]; everything
// above is uniform (age, phase, p0..p7). This is the E profile's own mask and
// is the same default `pack_field_host` carries.
constexpr uint32_t kVaryingMask = 0x3;

// The host slot the microcode is loaded into, and the object index the loader
// reserves for the FIRST install. They must be the same number: the fieldlist
// carries the loader's OBJECT INDEX through as the binding the host executes
// from. A first install into an empty catalogue reserves index 0.
constexpr int kSlot = 0;

// The staging window the bench serves and `fld_ldr_stage_base_i` already
// advertises (`tb_zhao_console_core_smoke.sv:2448`). Named here so the fixture
// and the bench cannot disagree about where the capsule is.
constexpr uint32_t kStageBase = 0x10000000u;

// THE LIVE RESOURCE EPOCH, and it is not decoration.
//
// `u_field_loader` is composed with `CHECK_EPOCH(1'b1)` and its `cfg_epoch_i`
// is the console's `fld_cfg_plan_base_i`, which the smoke bench drives to
// 0xF1E1D000 (`tb_zhao_console_core_smoke.sv:2416`). `zhao_field_loader:764`
// refuses any capsule whose header RESOURCE_EPOCH differs, with verdict
// V_BAD_META, BEFORE a slot is reserved -- "so a stale image cannot displace a
// live one on its way to being rejected".
//
// FOUND THE EXPENSIVE WAY, 2026-09-27: the first `-FieldActive` run left this
// at the library default of 0, the loader refused the capsule as stale, the
// fieldlist then had nothing to resolve the handle against, and the console
// reported ZERO Earth runs. Every visible symptom pointed downstream -- at the
// adapter, the join, the coverage test -- and the cause was one header word
// this generator never filled in. The bench ASSERTS the two agree
// (`SFF_EPOCH` against `fld_cfg_plan_base_i`) so the next person gets a named
// refusal instead of a silent zero.
constexpr uint32_t kResourceEpoch = 0xF1E1D000u;

// ---------------------------------------------------------------------------
// THE DOORBELL LOAD KINDS -- `zhao_field_host_v2`'s own, via
// `zhao_console_core.sv:16192`: "0 uop / 1 table entry / 2 header / 3 uniform"
// widened at packet C1 to eight: 4 OUTMAP, 5 ASSOC, 6 INITPROOF, 7 PREPARED.
// ---------------------------------------------------------------------------
constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;
// 3 UNIFORM -- the RING SERVICE's scalar-bank write (`fab_sb_*` ->
// `zhao_field_v3_sbank`, read only by `zhao_field_v3_ring_svc` at slots named
// in a RING instruction's immediate). Declared for completeness; this fixture
// emits none, because this program has no RING and because that bank is NOT
// the constant pool the microcode reads. See the block above `main()`.
constexpr uint8_t kLdUniform = 3;

constexpr uint8_t kLdOutMap = 4;
constexpr uint8_t kLdAssoc = 5;
constexpr uint8_t kLdInitProof = 6;
constexpr uint8_t kLdPrepared = 7;

// The association generation this fixture binds at. Nonzero on purpose: a
// generation of zero is indistinguishable from a register that was never
// written, and FH16 makes the generation the checked part of a binding.
constexpr uint8_t kAssocGen = 1;

// ---------------------------------------------------------------------------
// THE UNIFORMS, AND WHY THEY ARE NOT ZERO ANY MORE
// ---------------------------------------------------------------------------
// This generator used to hand `zfield::prepare` a vector of zeros and say so:
// "the PRELOAD VALUES here are placeholders ... a zero that looks like data is
// how a placeholder ships". With wave_pool that was harmless, because its
// material lane is a constant either way. With `scorch_wash` it is NOT: p2/p3
// are the falloff radii, and r_in == r_out == 0 is the degenerate footprint --
// the token stays legal (the smoothstep macro clamps) but the weight goes
// CONSTANT, and a constant out-lane 2 cannot show that the engine read the
// VARYING lanes at all. So these are real values.
//
// CENTRED ON THE ISLAND DATUM (0,0) rather than on a patch. The console places
// ONE of the geom fixture's terrain records and this file should not be the
// place that asserts which: a centre pinned to one patch would silently stop
// varying if the placement moved. A 160 m falloff from the datum means any 32 m
// patch within 160 m samples a monotone, non-constant slice of it.
// `compiler/tests/material_program.test.ts` sweeps this exact set over all
// three candidate patches and asserts more than one distinct weight in each
// (measured: 16, 19 and 10 distinct weights).
constexpr int32_t kFxOne = 65536;
constexpr int32_t kScorchAge = 0;
constexpr int32_t kScorchPhase = 0;
constexpr int32_t kScorchCentreX = 0;            // p0, island datum
constexpr int32_t kScorchCentreZ = 0;            // p1, island datum
constexpr int32_t kScorchRIn = 0;                // p2, no full-scorch core
constexpr int32_t kScorchROut = 160 * kFxOne;    // p3, clean ground 160 m out
constexpr int32_t kScorchNav = 2 * kFxOne;       // p4, nav surcharge at full scorch

// ---------------------------------------------------------------------------
// THE MATERIAL THE PROGRAM MUST EMIT -- A CHECKED MIRROR, NOT A DUPLICATE
// ---------------------------------------------------------------------------
// These four bytes are `scorch_wash.ts`'s MAT_SCORCH_A / MAT_SCORCH_B /
// SCORCH_WEIGHT_LO / +SCORCH_WEIGHT_SPAN. They are restated here because C++
// cannot import TypeScript -- and they are VERIFIED below by interpreting the
// program and decoding out-lane 2 with `zref::fieldir::material_token_decode`,
// the REFERENCE decoder. So if the TS side moves and this does not, the
// generator REFUSES rather than emitting a fixture the bench will then assert
// the wrong thing about. That is the difference between a mirror that is read
// and a mirror that is merely written.
//
// Why these values: the bench's authored layer-E plane spans matA in {1,2},
// matB in {5,6} and weight in [0x30,0xCF]
// (`tb_zhao_console_core_smoke.sv:3881-3886`). None of 0xD4, 0x1E or
// [0xD0,0xFF] is in that set, so the owner's clause 3 -- "a value that cannot
// equal the authored baseline by accident" -- holds from the two LAYOUTS rather
// than from an observation about one run.
constexpr uint8_t kScorchMatA = 0xD4;
constexpr uint8_t kScorchMatB = 0x1E;
constexpr uint8_t kScorchWeightLo = 0xD0;
constexpr uint8_t kScorchWeightHi = 0xFF;

// The candidate patch envelopes the verification sweeps, from
// `tb_zhao_console_core_smoke.sv`: record r sits at patch (r + SGF_TERR_IX0,
// r + SGF_TERR_IZ0) and spans 32 m per side at pitch_log2 0. SGF_TERR_IX0 = 0,
// SGF_TERR_IZ0 = 1, SGF_TERR_RECORDS = 3 in `smoke_geom_fixture.svh`.
constexpr int kPatchIx0 = 0;
constexpr int kPatchIz0 = 1;
constexpr int kPatchRecords = 3;
constexpr int kPatchEdgeM = 32;

struct LoadWord {
  uint8_t kind;
  uint32_t addr;
  uint64_t lo64;
  uint32_t hi32;
};

// THE v2 HEADER WORD, field order taken from `field_host_v2_directed.cpp`'s
// `header_word`, which is the only other place in the tree that knows it:
//   [13:8] out_base  [32 +: 7] WINDOW mask  [48 +: 8] ORDINAL mask
//   [56 +: 4] output_count  [60 +: 2] execution_form
// The two masks are DIFFERENT QUANTITIES of different widths -- R101's window
// mask is WINDOW-POSITION indexed and the required mask is ORDINAL indexed --
// and `HostPlan` keeps them in separate types precisely so they cannot be
// assigned to each other. They are read through those types here.
uint64_t header_word(uint8_t instr_count, uint8_t out_base, uint8_t win_mask, uint8_t ord_mask,
                     uint8_t out_count, uint8_t form) {
  uint64_t w = static_cast<uint64_t>(instr_count);
  w |= (static_cast<uint64_t>(out_base) & 0x3Full) << 8;
  w |= (static_cast<uint64_t>(win_mask) & 0x7Full) << 32;
  w |= (static_cast<uint64_t>(ord_mask) & 0xFFull) << 48;
  w |= (static_cast<uint64_t>(out_count) & 0x0Full) << 56;
  w |= (static_cast<uint64_t>(form) & 0x03ull) << 60;
  return w;
}

std::string hex32(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "32'h%08X", v);
  return b;
}

std::string hex64(uint64_t v) {
  char b[24];
  std::snprintf(b, sizeof b, "64'h%016llX", (unsigned long long)v);
  return b;
}

bool read_file(const char* path, std::string* out) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0) {
    std::fclose(f);
    return false;
  }
  out->resize((size_t)n);
  if (n > 0 && std::fread(&(*out)[0], 1, (size_t)n, f) != (size_t)n) {
    std::fclose(f);
    return false;
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  bool check = false;
  const char* path = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--check") == 0) {
      check = true;
    } else {
      path = argv[i];
    }
  }
  if (!path) {
    std::printf("usage: smoke_field_fixture_gen [--check] <out.svh>\n");
    return 2;
  }

  // ---- 1. THE CANONICAL PROGRAM, THROUGH THE CANONICAL DECODER -------------
  // Nothing here second-guesses the validator. A program this refuses is a
  // program the console may not run either.
  const uint8_t* prog_bytes = zfield_gen::scorch_wash::kProgramBytes.data();
  const size_t prog_len = zfield_gen::scorch_wash::kProgramBytesLen;

  zfield::DecodeResult dr = zfield::decode(prog_bytes, prog_len);
  if (dr.error != zfield::DecodeError::kOk) {
    std::printf("smoke_field_fixture_gen: REFUSED at decode: %s (%s)\n",
                zfield::decodeErrorName(dr.error), dr.detail.c_str());
    return 1;
  }

  const zfield::Fplan fp = zfield::plan(dr.prog, kVaryingMask);

  // ---- 1a. THE ASSOCIATION'S UNIFORM VALUES, WHICH ARE REAL NOW ------------
  // These used to be a vector of zeros with a comment admitting it. See the
  // knob block above for why that is no longer acceptable: with `scorch_wash`
  // the radii ARE uniforms, and zero radii make out-lane 2 constant.
  //
  // Lane order is the earth profile's own: 0 x, 1 z (both VARYING, overwritten
  // per lattice vertex, so their preload is irrelevant and left zero), then
  // 2 age, 3 phase, 4..11 p0..p7.
  std::vector<int32_t> uni_in(dr.prog.in_lanes.size(), 0);
  if (uni_in.size() < 12) {
    std::printf("smoke_field_fixture_gen: program declares %u input lanes, earth owes 12\n",
                (unsigned)uni_in.size());
    return 1;
  }
  uni_in[2] = kScorchAge;
  uni_in[3] = kScorchPhase;
  uni_in[4] = kScorchCentreX;
  uni_in[5] = kScorchCentreZ;
  uni_in[6] = kScorchRIn;
  uni_in[7] = kScorchROut;
  uni_in[8] = kScorchNav;
  const zfield::Prepared prep =
      zfield::prepare(fp, dr.prog, uni_in.data(), uni_in.size());

  // ---- 1b. OUT-LANE 2 IS A LEGAL TOKEN, CHECKED BY THE REFERENCE DECODER ---
  // The owner's bar: "a real production Field program is installed and
  // executed" and "its material write produces a value that cannot equal the
  // authored baseline by accident". The second half is a property of the
  // PROGRAM, so it is checked HERE, before a capsule is built -- a fixture that
  // cannot possibly satisfy clause 3 should never reach the bench.
  //
  // This is also the cross-language pin on the TypeScript token packing. The
  // decode below is `zref::fieldir::material_token_decode`, the C++ view of
  // sec 14.1; nothing here trusts `material_token.ts`.
  {
    int distinct_weights = 0;
    bool w_seen[256] = {false};
    uint8_t w_lo = 0xFF, w_hi = 0x00;
    long long checked = 0;

    for (int r = 0; r < kPatchRecords; ++r) {
      const int ix = r + kPatchIx0;
      const int iz = r + kPatchIz0;
      for (int vi = 0; vi <= kPatchEdgeM; ++vi) {
        for (int vj = 0; vj <= kPatchEdgeM; ++vj) {
          std::vector<int32_t> in = uni_in;
          in[0] = (int32_t)((ix * kPatchEdgeM + vi) * kFxOne);
          in[1] = (int32_t)((iz * kPatchEdgeM + vj) * kFxOne);
          int32_t out[8] = {0};
          const size_t n_out = dr.prog.out_lanes.size() <= 8 ? dr.prog.out_lanes.size() : 8;
          zfield::interpret(dr.prog, in.data(), in.size(), out, n_out);
          ++checked;

          const uint32_t tok = (uint32_t)out[2];
          zref::fieldir::MaterialState ms;
          if (!zref::fieldir::material_token_decode(tok, &ms)) {
            std::printf("smoke_field_fixture_gen: out-lane 2 at patch (%d,%d) vertex (%d,%d) "
                        "is NOT a v1 material token: 0x%08X (tag 0x%02X, want 0xE1). "
                        "The staged program cannot satisfy I34 clause 3.\n",
                        ix, iz, vi, vj, tok, (unsigned)(tok >> 24));
            return 1;
          }
          if (ms.mat_a != kScorchMatA || ms.mat_b != kScorchMatB) {
            std::printf("smoke_field_fixture_gen: out-lane 2 at patch (%d,%d) vertex (%d,%d) "
                        "decodes to matA=0x%02X matB=0x%02X, this file declares 0x%02X/0x%02X. "
                        "The TypeScript program and this mirror have diverged.\n",
                        ix, iz, vi, vj, ms.mat_a, ms.mat_b, kScorchMatA, kScorchMatB);
            return 1;
          }
          if (ms.weight < kScorchWeightLo || ms.weight > kScorchWeightHi) {
            std::printf("smoke_field_fixture_gen: out-lane 2 weight 0x%02X at patch (%d,%d) "
                        "vertex (%d,%d) is outside the declared band [0x%02X,0x%02X], which is "
                        "what clause 3's non-coincidence rests on.\n",
                        ms.weight, ix, iz, vi, vj, kScorchWeightLo, kScorchWeightHi);
            return 1;
          }
          // height and velocity must be EXACTLY zero, or the covered run stops
          // being the geometry-invariant experiment the header describes.
          if (out[0] != 0 || out[1] != 0) {
            std::printf("smoke_field_fixture_gen: patch (%d,%d) vertex (%d,%d) writes "
                        "height=%d velocity=%d; this fixture's whole argument is that both "
                        "are zero so the covered run keeps the authored geometry.\n",
                        ix, iz, vi, vj, out[0], out[1]);
            return 1;
          }
          if (!w_seen[ms.weight]) { w_seen[ms.weight] = true; ++distinct_weights; }
          if (ms.weight < w_lo) w_lo = ms.weight;
          if (ms.weight > w_hi) w_hi = ms.weight;
        }
      }
    }

    // ANTI-VACUITY FOR THIS CHECK ITSELF. Everything above passes on a program
    // whose out-lane 2 is a single constant, and a constant cannot demonstrate
    // that the engine consumed the varying lanes. This is the assertion that
    // says the field is a FIELD.
    if (distinct_weights < 2) {
      std::printf("smoke_field_fixture_gen: out-lane 2 is CONSTANT across %lld sampled "
                  "vertices (weight 0x%02X). The uniforms make the falloff degenerate, so "
                  "the token cannot show the engine read x/z. Adjust kScorchROut.\n",
                  checked, w_lo);
      return 1;
    }
    std::printf("smoke_field_fixture_gen: out-lane 2 verified over %lld vertices -- "
                "every one a v1 token {0x%02X,0x%02X,w}, w in [0x%02X,0x%02X], "
                "%d distinct weights, height=velocity=0 throughout\n",
                checked, kScorchMatA, kScorchMatB, w_lo, w_hi, distinct_weights);
  }

  hp_ns::LowerOptions opt;
  opt.scalar_base = kScalarBase;
  opt.out_base = kOutBase;
  opt.register_ceiling = kRegisterCeiling;
  opt.canonical_program_handle32 = fp.canonical_hash;
  opt.source_id32 = dr.prog.source_id;
  opt.resource_epoch = kResourceEpoch;

  hp_ns::HostPlan hp;
  std::string refusal;
  if (!hp_ns::lower(fp, dr.prog, &prep, opt, &hp, &refusal)) {
    std::printf("smoke_field_fixture_gen: REFUSED at lowering: %s\n", refusal.c_str());
    return 1;
  }

  // THE CEILING IS RE-CHECKED HERE, on the plan about to be emitted, and not
  // merely inside lower(). `pack_field_host` does exactly this and gives the
  // reason: a check that only runs on the path that constructed the value
  // cannot see a value that was edited after. If REGS ever moves in the core
  // and not here, this is what says so.
  if (hp.register_high_water > kRegisterCeiling) {
    std::printf("smoke_field_fixture_gen: high-water %d exceeds the composed ceiling %d\n",
                hp.register_high_water, kRegisterCeiling);
    return 1;
  }
  if (!hp_ns::verify_output_contract(hp, &refusal)) {
    std::printf("smoke_field_fixture_gen: REFUSED at the output contract: %s\n", refusal.c_str());
    return 1;
  }

  // ---- 2. THE CAPSULE, for the loader, over the real bridge ----------------
  std::vector<uint8_t> image;
  if (!hp_ns::serialize_program_image(hp, prog_bytes, prog_len, &image, &refusal)) {
    std::printf("smoke_field_fixture_gen: REFUSED at serialisation: %s\n", refusal.c_str());
    return 1;
  }

  // ---- 3. THE LOAD WORDS, for the host, through the doorbell ---------------
  // Order is LAW, not preference: the HEADER is what marks a slot runnable, so
  // it goes LAST and a partially written program can never execute
  // (`zhao_console_core.sv:16194`). Everything else precedes it.
  std::vector<LoadWord> lw;

  for (size_t i = 0; i < hp.uops.size(); ++i)
    lw.push_back({kLdUop, (uint32_t)i, hp.uops[i].word(), 0});

  // =========================================================================
  // THE CONSTANT POOL, AND HOW IT IS NOW DELIVERED.
  // Found by MATFIELD 2026-09-27; repaired by LASTGAP the same day.
  // =========================================================================
  // THE DEFECT. `lower()` placed every uniform-only value -- every literal AND
  // every result of the uniform instruction block -- in the "broadcast uniform
  // region" at physical register `scalar_base + s`, and the physical uops
  // SOURCED those registers. `zhao_field_host_v2` never writes them: its
  // register-file preload port (`:1250-1256`) writes
  //
  //     fab_pre_we  = (state == E_ZERO) || ((state == E_WRITE) &&
  //                                         (lane_i < IN_LANES));
  //
  // zeros on the slow path and the point's own IN_LANES on the fast one, at
  // register == LANE INDEX. There is no association-broadcast phase and no
  // doorbell kind that would feed one. So every program executed on zeros,
  // retired cleanly, and NOTHING IN THE TREE COULD SEE IT -- every counter is
  // true and not one looks at a value.
  //
  // MEASURED at the settings this console composes: `scorch_wash` at
  // scalar_base 8 put its material token base 0xE1D41ED0 at REGISTER 27, and
  // the host writes registers 0..14.
  //
  // THE REPAIR, and it cost ZERO SILICON (decision record
  // reports/DECISION-20260927-I34-CONSTANT-POOL.md). `lower()` now emits every
  // scalar slot a VECTOR uop reads as an `LDC dst=scalar_base+s,
  // imm=prep->scalar[s]` at the head of the physical stream
  // (`LowerOptions::materialize_scalars`, ON by default). `LDC` is not new
  // hardware: `zhao_field_alu.sv:307` already reads
  // `OP_LDC: result_o = $signed(imm_i)`, and the doorbell's `LdUop` arm already
  // carries the 32-bit immediate at `ld_data_i[63:32]`. Nothing under `fpga/`
  // changed.
  //
  // IT TOOK A TAG TO MAKE THE DEFECT VISIBLE. A height that is silently zero
  // looks like a field that ran; a MATERIAL that is silently wrong is REFUSED
  // and counted, because `spec/qformats.md` sec 14.1 gives the token a version
  // byte for exactly this purpose. `terrmat field_composed=0
  // token_refused=1024` was that byte doing its job.
  //
  // THE `LdUniform` NON-REPAIR, kept here because it is the attractive wrong
  // answer and somebody will reach for it again. `LdUniform` writes `fab_sb_*`
  // -> `zhao_field_v3_sbank`, which LOOKS like "the register file the microcode
  // reads". IT IS NOT. That bank is instantiated in `zhao_field_v3_svcpath` and
  // its read address is driven solely by `zhao_field_v3_ring_svc`'s `f_slot_r`,
  // loaded from a RING instruction's IMMEDIATE (`ring_svc:404-407`). It is the
  // RING service's four-operand uniform file, not a general constant pool, and
  // this program contains no RING. MATFIELD built it, disproved it by reading
  // `ring_svc`, and measured `token_refused` unmoved at 1024 with it in place.
  // NO `LdUniform` WORD IS EMITTED.

  // ---- THE CONSTANT POOL ARRIVED: A STRUCTURAL CHECK, NOT A COUNT ----------
  // The owner's clause 3 is about a VALUE, and this repository has twice
  // shipped a check that passed on a constant. So this does not assert
  // `materialized_scalars != 0` -- a count is not a value, and a count is
  // exactly what stayed true and uninformative while the defect shipped.
  //
  // It walks the NON-LDC uops, collects every register they read that lies in
  // the scalar region, and requires each one to be written by an EARLIER LDC
  // carrying `prep.scalar[]`'s own number. That is the property the machine
  // needs, stated over the EMITTED STREAM rather than over the option that
  // produced it, so it fails if the option is off, if the ordering is wrong, or
  // if a single slot is missed.
  {
    std::vector<int> ldc_at((size_t)(hp.scalar_base + (int)fp.n_scalar), -1);
    int checked_reads = 0;
    for (size_t ui = 0; ui < hp.uops.size(); ++ui) {
      const hp_ns::Mapped& m = hp.uops[ui];
      if (m.op == zfield::OP_LDC) {
        const int sidx = m.dst - hp.scalar_base;
        if (sidx < 0 || sidx >= (int)fp.n_scalar) continue;
        if ((uint32_t)prep.scalar[(size_t)sidx] != m.imm) {
          std::printf("smoke_field_fixture_gen: LDC at uop %u loads 0x%08X into r%d but "
                      "prepared slot %d holds 0x%08X\n",
                      (unsigned)ui, m.imm, m.dst, sidx, (uint32_t)prep.scalar[(size_t)sidx]);
          return 1;
        }
        ldc_at[(size_t)m.dst] = (int)ui;
        continue;
      }
      for (int g = 0; g < m.n_groups && g < 3; ++g) {
        const int start = (g == 0) ? m.a : (g == 1) ? m.b : m.c;
        for (int k = 0; k < m.group_width[g]; ++k) {
          const int r = start + k;
          if (r < hp.scalar_base || r >= hp.scalar_base + (int)fp.n_scalar) continue;
          if (ldc_at[(size_t)r] < 0 || ldc_at[(size_t)r] >= (int)ui) {
            std::printf("smoke_field_fixture_gen: uop %u reads r%d, which is in the scalar "
                        "region and is NOT loaded by an earlier LDC. The constant pool does "
                        "not reach the execution register file.\n",
                        (unsigned)ui, r);
            return 1;
          }
          checked_reads++;
        }
      }
    }
    if (checked_reads == 0) {
      std::printf("smoke_field_fixture_gen: NO uop reads the scalar region at all, so this "
                  "check proved nothing about the constant pool. The program or its "
                  "lowering changed shape -- do not read this as a pass.\n");
      return 1;
    }
    // And the material token's own base must be one of the numbers delivered.
    // This is the byte clause 3 turns on, so it is named rather than left to
    // the general walk above.
    bool token_base_seen = false;
    for (size_t ui = 0; ui < hp.uops.size(); ++ui)
      if (hp.uops[ui].op == zfield::OP_LDC &&
          (hp.uops[ui].imm & 0xFF000000u) == 0xE1000000u)
        token_base_seen = true;
    if (!token_base_seen) {
      std::printf("smoke_field_fixture_gen: no LDC carries a v1 material token base (tag "
                  "0xE1). out-lane 2 cannot become a legal token in the console.\n");
      return 1;
    }
    std::printf("smoke_field_fixture_gen: constant pool delivered -- %d LDC uops, %d scalar "
                "reads each proven to follow its own LDC, material token base present\n",
                hp.materialized_scalars, checked_reads);
  }

  // THE PREPARED FILE, indexed by the SCALAR INDEX `s`, which is what
  //     `OutputSource::source_index` carries for a PREPARED_SCALAR ordinal
  //     (`zfield_host_plan.cpp`, `o.source_index = t.idx` on the scalar arm).
  //     STILL NEEDED, AND NOT SUPERSEDED BY THE LDCs: an ordinal whose
  //     OUTPUT_MAP row says PREPARED_SCALAR is seeded from `prep_value[]` at
  //     point start (`zhao_field_host_v2.sv:1669`) and never touches the
  //     register file at all. This program's height and velocity ordinals are
  //     both prepared scalars, so these rows are what makes them arrive.
  //     The VALID bit is driven independently of the value so a genuine zero
  //     and a withheld value do not look alike, and the generation must match
  //     the association's or the seed is refused (:1309-1321).
  for (size_t i = 0; i < hp.preload.size(); ++i) {
    const uint32_t val = (uint32_t)hp.preload[i].value;
    uint64_t w = (uint64_t)val;
    w |= (1ull << 32);                 // VALID
    w |= ((uint64_t)kAssocGen) << 40;  // generation
    lw.push_back({kLdPrepared, (uint32_t)i, w, 0});
  }

  // OUTPUT_MAP: one row per canonical ordinal. [15:0] source_index [16] kind.
  for (size_t i = 0; i < hp.output_map.size(); ++i) {
    const hp_ns::OutputSource& os = hp.output_map[i];
    uint64_t w = (uint64_t)(uint16_t)os.source_index;
    w |= ((uint64_t)(os.source_kind & 1)) << 16;
    lw.push_back({kLdOutMap, (uint32_t)os.ordinal, w, 0});
  }

  lw.push_back({kLdAssoc, 0, (uint64_t)kAssocGen, 0});
  lw.push_back({kLdInitProof, 0, 1ull, 0});

  // EVERY PREPARED_SCALAR ORDINAL MUST HAVE A ROW WE ACTUALLY POSTED. The
  // repair above emits one prepared word per preload row, indexed by the row's
  // scalar index; an ordinal naming an index outside that range would be
  // unseeded and the point would refuse for a reason pointing at the RTL. This
  // is cheap and it is the check whose absence let the original defect ship.
  for (size_t i = 0; i < hp.output_map.size(); ++i) {
    const hp_ns::OutputSource& o = hp.output_map[i];
    if (o.source_kind != zfield::host_image::ZFH_SOURCE_KIND_PREPARED_SCALAR) continue;
    if (o.source_index < 0 || (size_t)o.source_index >= hp.preload.size()) {
      std::printf("smoke_field_fixture_gen: ordinal %d is a PREPARED_SCALAR at index %d and "
                  "only %u prepared rows are posted -- it would never be seeded\n",
                  o.ordinal, o.source_index, (unsigned)hp.preload.size());
      return 1;
    }
  }

  const uint8_t win_mask = (uint8_t)hp.window_mask().bits();
  const uint8_t ord_mask = (uint8_t)hp.required_mask.bits();
  lw.push_back({kLdHeader, 0,
                header_word((uint8_t)hp.uops.size(), (uint8_t)hp.out_base, win_mask, ord_mask,
                            (uint8_t)hp.output_count, (uint8_t)hp.execution_form),
                0});

  // ---- 4. EMIT -------------------------------------------------------------
  std::string o;
  auto line = [&](const std::string& s) { o += s; o += "\n"; };

  line("// GENERATED by tests/prod/smoke_field_fixture_gen.cpp -- DO NOT EDIT.");
  line("// Regenerate: build `smoke_field_fixture_gen` and run it with the path of");
  line("// this file. The ctest `smoke_field_fixture_fresh` fails if it is stale.");
  line("//");
  line("// A REAL Earth program -- compiler/src/field_ir/scorch_wash.ts, the first in");
  line("// this tree whose out-lane 2 is a v1 MATERIAL TOKEN (spec/qformats.md 14.1) --");
  line("// lowered by the REAL production library at the console's OWN composed ceiling.");
  line("// It writes ZERO height and velocity on purpose: field height composes");
  line("// additively (zhao_terrain_patch.sv:339), so the covered run keeps the authored");
  line("// geometry exactly and the only thing that moves is the material.");
  line("// Nothing below is hand-built: directive 13.7 forbids the bench inventing");
  line("// an association, and every byte here is serialize_program_image's or a");
  line("// HostPlan field's.");
  o += "\n";

  char buf[256];
  std::snprintf(buf, sizeof buf,
                "// lowered: scalar_base=%d out_base=%d register_high_water=%d ceiling=%d",
                hp.scalar_base, hp.out_base, hp.register_high_water, kRegisterCeiling);
  line(buf);
  std::snprintf(buf, sizeof buf,
                "// outputs=%d required_mask(ORDINAL)=0x%02X window_mask(POSITION)=0x%02X",
                hp.output_count, ord_mask, win_mask);
  line(buf);
  // THIS LINE USED TO SAY the preload rows are "each posted TWICE: kind 3
  // UNIFORM to the scalar bank AND kind 7 PREPARED to the prepared file". They
  // are not and never were after MATFIELD removed the `LdUniform` post in the
  // same commit that wrote the sentence -- exactly ONE word is emitted per
  // preload row, kind 7. Corrected 2026-09-27 (LASTGAP); a comment that
  // overstates what is delivered is the same hazard as a counter that does.
  std::snprintf(buf, sizeof buf,
                "// uops=%u (of which %d are constant-pool LDC) preload_rows=%u "
                "(one kind-7 PREPARED word each) image_bytes=%u",
                (unsigned)hp.uops.size(), hp.materialized_scalars,
                (unsigned)hp.preload.size(), (unsigned)image.size());
  line(buf);
  o += "\n";
  // THE TWO COUNTS THE BENCH ASSERTS. They are emitted rather than restated so
  // the generator and the bench cannot hold two opinions about one number.
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_N_UOPS = %u;",
                (unsigned)hp.uops.size());
  line(buf);
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_N_LDC = %d;",
                hp.materialized_scalars);
  line(buf);
  o += "\n";

  // THE IDENTITY THE TERRAINFIELD RECORD MUST NAME. `zhao_field_loader` reads
  // CANONICAL_PROGRAM_HANDLE32 out of the capsule header at install and
  // publishes it as `pub_handle_o`; `zhao_terrain_fieldlist` matches the
  // record's `program` against exactly that. So these two constants are the
  // join between the command stream and the program store, and they come from
  // the same HostPlan that produced the capsule.
  line("// The capsule's own published identity. The TerrainField record's");
  line("// `program` handle MUST equal SFF_HANDLE or the fieldlist reports noprog.");
  line("localparam logic [31:0] SFF_HANDLE = " + hex32(hp.canonical_program_handle32) + ";");
  line("localparam logic [31:0] SFF_HASH   = " + hex32(hp.canonical_hash) + ";");
  line("localparam logic [31:0] SFF_STAGE_BASE = " + hex32(kStageBase) + ";");
  line("// The loader composes CHECK_EPOCH=1 and refuses a capsule stamped for a");
  line("// different resource epoch as STALE (V_BAD_META). The bench asserts this");
  line("// equals its own `fld_cfg_plan_base_i`, so a change to either is a named");
  line("// refusal rather than a silent zero-runs result.");
  line("localparam logic [31:0] SFF_EPOCH = " + hex32(hp.resource_epoch) + ";");
  line("");
  // THE HASH THE INSTALL POST MUST CARRY, and it is NOT the program hash.
  //
  // `zhao_field_loader:968` compares the CRC it folded over the RECEIVED BYTES
  // against `fh2_hash_i`, and refuses with V_BAD_CRC on a mismatch. That fold
  // masks bytes 12..15 to zero "exactly as the packer's own rule requires", so
  // the value it arrives at is the image header's own BODY_CRC32C field --
  // which is read straight out of the serialised image here rather than
  // recomputed, because recomputing it would be a SECOND implementation of the
  // packer's rule and the two could disagree silently.
  //
  // Posting SFF_HASH here instead looks completely reasonable (it is "the
  // program's hash", the doorbell field is called `hash`, and the fieldlist
  // matches on a hash) and is wrong. The two constants are emitted with
  // different names for that reason.
  {
    uint32_t body_crc = 0;
    for (int b = 0; b < 4; ++b)
      body_crc |= ((uint32_t)image[(size_t)(12 + b)]) << (8 * b);
    line("// The INSTALL post's `hash` operand: the image's BODY_CRC32C (header");
    line("// offset 12), NOT the canonical program hash. zhao_field_loader:968");
    line("// folds the received bytes with 12..15 masked and compares to this.");
    line("localparam logic [31:0] SFF_BODY_CRC = " + hex32(body_crc) + ";");
  }
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_SLOT = %d;", kSlot);
  line(buf);
  o += "\n";

  // ---- the material the field will compose, for the bench to assert on ------
  // Emitted rather than restated in the bench, so the program, this generator
  // and the bench cannot hold three opinions about one value. Each was verified
  // above by decoding the program's own out-lane 2 with the reference decoder
  // over every candidate patch lattice.
  line("// THE FIELD'S MATERIAL. Verified in the generator by interpreting the");
  line("// program and decoding out-lane 2 with zref::fieldir::material_token_decode");
  line("// at every vertex of every candidate patch lattice -- not asserted here.");
  line("// The mosaic picks between SFF_MAT_A and SFF_MAT_B, so a terrain fill in the");
  line("// covered run must land in one of those tiles and NOT in an authored one.");
  std::snprintf(buf, sizeof buf, "localparam logic [7:0] SFF_MAT_A = 8'h%02X;", kScorchMatA);
  line(buf);
  std::snprintf(buf, sizeof buf, "localparam logic [7:0] SFF_MAT_B = 8'h%02X;", kScorchMatB);
  line(buf);
  std::snprintf(buf, sizeof buf, "localparam logic [7:0] SFF_MAT_W_LO = 8'h%02X;",
                kScorchWeightLo);
  line(buf);
  std::snprintf(buf, sizeof buf, "localparam logic [7:0] SFF_MAT_W_HI = 8'h%02X;",
                kScorchWeightHi);
  line(buf);
  o += "\n";

  // ---- the capsule, as the 64-bit beats the HPS bridge serves --------------
  const size_t words = (image.size() + 7) / 8;
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_CAP_BYTES = %u;",
                (unsigned)image.size());
  line(buf);
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_CAP_WORDS = %u;", (unsigned)words);
  line(buf);
  {
    std::string s = "localparam logic [63:0] SFF_CAP [0:" + std::to_string(words - 1) + "] = '{";
    for (size_t w = 0; w < words; ++w) {
      uint64_t v = 0;
      for (int b = 0; b < 8; ++b) {
        const size_t idx = w * 8 + (size_t)b;
        if (idx < image.size()) v |= ((uint64_t)image[idx]) << (8 * b);
      }
      if (w) s += ", ";
      if ((w % 4) == 0) s += "\n  ";
      s += hex64(v);
    }
    s += "};";
    line(s);
  }
  o += "\n";

  // ---- the doorbell load words --------------------------------------------
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_N_LOAD = %u;", (unsigned)lw.size());
  line(buf);
  {
    std::string k = "localparam logic [2:0] SFF_LD_KIND [0:" + std::to_string(lw.size() - 1) + "] = '{";
    std::string a = "localparam logic [7:0] SFF_LD_ADDR [0:" + std::to_string(lw.size() - 1) + "] = '{";
    std::string d = "localparam logic [95:0] SFF_LD_DATA [0:" + std::to_string(lw.size() - 1) + "] = '{";
    for (size_t i = 0; i < lw.size(); ++i) {
      if (i) { k += ", "; a += ", "; d += ", "; }
      if ((i % 8) == 0) { k += "\n  "; a += "\n  "; }
      if ((i % 3) == 0) d += "\n  ";
      char t[64];
      std::snprintf(t, sizeof t, "3'd%u", (unsigned)lw[i].kind);
      k += t;
      std::snprintf(t, sizeof t, "8'd%u", (unsigned)lw[i].addr);
      a += t;
      char t2[64];
      std::snprintf(t2, sizeof t2, "96'h%08X%016llX", lw[i].hi32, (unsigned long long)lw[i].lo64);
      d += t2;
    }
    k += "};"; a += "};"; d += "};";
    line(k); line(a); line(d);
  }

  if (check) {
    std::string have;
    if (!read_file(path, &have)) {
      std::printf("smoke_field_fixture_gen: --check cannot read %s\n", path);
      return 1;
    }
    // Normalise line endings on BOTH sides before comparing. CLAUDE.md's
    // own chapter: "a line-ending difference in a correct file reads as a
    // content mismatch", and it cost a day once already.
    std::string a, b;
    for (char c : have) if (c != '\r') a += c;
    for (char c : o) if (c != '\r') b += c;
    if (a != b) {
      std::printf("smoke_field_fixture_gen: %s is STALE -- regenerate it\n", path);
      return 1;
    }
    std::printf("smoke_field_fixture_gen: %s is fresh (%u load words, %u capsule bytes)\n", path,
                (unsigned)lw.size(), (unsigned)image.size());
    return 0;
  }

  std::FILE* f = std::fopen(path, "wb");
  if (!f) {
    std::printf("smoke_field_fixture_gen: cannot write %s\n", path);
    return 2;
  }
  std::fwrite(o.data(), 1, o.size(), f);
  std::fclose(f);
  std::printf("smoke_field_fixture_gen: wrote %s (%u load words, %u capsule bytes)\n", path,
              (unsigned)lw.size(), (unsigned)image.size());
  return 0;
}
