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
// THE PROGRAM, AND THE MEASUREMENT THAT CHOSE IT
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
// So REGS=32 does NOT have to move, and no new program has to be authored: the
// ceiling is a function of `scalar_base`, which is a LowerOptions field the
// entry's own remedy list omits. `wave_pool` at scalar-base 12 lowers with
// `register_high_water = 31` -- one register under the composed ceiling.
//
// WAVE_POOL IS A REAL SHIPPED SPELL (`spells/membrane.form`), not a fixture
// program written to pass. It declares FOUR canonical outputs and its required
// mask is 0x0F -- lanes 0..3, which are exactly I34's four Earth channels:
// height, velocity, material and nav. That is the reason it is the right
// program for this mode and not merely a program that fits.
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

#include "../../compiler/tests/generated/wave_pool.hpp"

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

// Measured, not guessed -- see the sweep in the header comment. 12 is chosen
// over 8 because it leaves the most room below the uniform region for the
// vector writes while still landing the high-water at 31.
constexpr int kScalarBase = 12;

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

// ---------------------------------------------------------------------------
// THE DOORBELL LOAD KINDS -- `zhao_field_host_v2`'s own, via
// `zhao_console_core.sv:16192`: "0 uop / 1 table entry / 2 header / 3 uniform"
// widened at packet C1 to eight: 4 OUTMAP, 5 ASSOC, 6 INITPROOF, 7 PREPARED.
// ---------------------------------------------------------------------------
constexpr uint8_t kLdUop = 0;
constexpr uint8_t kLdHeader = 2;
constexpr uint8_t kLdOutMap = 4;
constexpr uint8_t kLdAssoc = 5;
constexpr uint8_t kLdInitProof = 6;
constexpr uint8_t kLdPrepared = 7;

// The association generation this fixture binds at. Nonzero on purpose: a
// generation of zero is indistinguishable from a register that was never
// written, and FH16 makes the generation the checked part of a binding.
constexpr uint8_t kAssocGen = 1;

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
  const uint8_t* prog_bytes = zfield_gen::wave_pool::kProgramBytes.data();
  const size_t prog_len = zfield_gen::wave_pool::kProgramBytesLen;

  zfield::DecodeResult dr = zfield::decode(prog_bytes, prog_len);
  if (dr.error != zfield::DecodeError::kOk) {
    std::printf("smoke_field_fixture_gen: REFUSED at decode: %s (%s)\n",
                zfield::decodeErrorName(dr.error), dr.detail.c_str());
    return 1;
  }

  const zfield::Fplan fp = zfield::plan(dr.prog, kVaryingMask);

  // The association's uniform values. A packer run from a bare .zprog has none
  // and `pack_field_host` says so out loud; this fixture is in the same
  // position. The PRELOAD VALUES here are placeholders, the maps and the proof
  // are not -- they depend on WHICH registers are written, never on what is in
  // them. Said out loud for the same reason the packer says it: a zero that
  // looks like data is how a placeholder ships.
  std::vector<int32_t> zero_in(dr.prog.in_lanes.size(), 0);
  const zfield::Prepared prep =
      zfield::prepare(fp, dr.prog, zero_in.data(), zero_in.size());

  hp_ns::LowerOptions opt;
  opt.scalar_base = kScalarBase;
  opt.register_ceiling = kRegisterCeiling;
  opt.canonical_program_handle32 = fp.canonical_hash;
  opt.source_id32 = dr.prog.source_id;

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

  // PRELOAD rows become PREPARED scalars. `addr` is the prepared index; the
  // VALID bit is driven independently of the value so a genuine zero and a
  // withheld value do not look alike.
  for (size_t i = 0; i < hp.preload.size(); ++i) {
    uint64_t w = (uint64_t)(uint32_t)hp.preload[i].value;
    w |= (1ull << 32);                              // VALID
    w |= ((uint64_t)kAssocGen) << 40;               // generation
    lw.push_back({kLdPrepared, (uint32_t)hp.preload[i].physical_register, w, 0});
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
  line("// A REAL Earth program (spells/membrane.form -> wave_pool), lowered by the");
  line("// REAL production library at the console's OWN composed register ceiling.");
  line("// Nothing below is hand-built: directive 13.7 forbids the bench inventing");
  line("// an association, and every byte here is serialize_program_image's or a");
  line("// HostPlan field's.");
  o += "\n";

  char buf[256];
  std::snprintf(buf, sizeof buf, "// lowered: scalar_base=%d register_high_water=%d ceiling=%d",
                hp.scalar_base, hp.register_high_water, kRegisterCeiling);
  line(buf);
  std::snprintf(buf, sizeof buf,
                "// outputs=%d required_mask(ORDINAL)=0x%02X window_mask(POSITION)=0x%02X",
                hp.output_count, ord_mask, win_mask);
  line(buf);
  std::snprintf(buf, sizeof buf, "// uops=%u preload_rows=%u image_bytes=%u",
                (unsigned)hp.uops.size(), (unsigned)hp.preload.size(), (unsigned)image.size());
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
  std::snprintf(buf, sizeof buf, "localparam int unsigned SFF_SLOT = %d;", kSlot);
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
