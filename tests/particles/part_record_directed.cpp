// part_record_directed.cpp — is the particle128 record the record the spec
// says it is?
//
// ---------------------------------------------------------------------------
// WHY A CODEC NEEDS A TEST AT ALL
// ---------------------------------------------------------------------------
// It is wires. There is no arithmetic to get wrong except two derived values.
// What there is, is TWELVE FIELDS AT TWELVE OFFSETS, written twice -- once to
// unpack and once to pack -- and a layout that every other particle block will
// take on faith.
//
// A wrong offset is not caught by a round trip: pack and unpack can agree with
// each other perfectly while both disagree with the spec. So there are two
// separate obligations here and they need separate checks:
//
//   1. ROUND TRIP -- pack(unpack(r)) == r, which proves the two halves agree.
//   2. THE SPEC -- against `zref::part::particle_pack/unpack`, written from
//      qformats §10 rather than from this Verilog, which proves they agree
//      with the ruling.
//
// And one more that neither of those catches: FIELD ISOLATION. Change one
// field and exactly its own bits must move. Two fields that overlap by a bit
// still round-trip and still match a reference that shares the mistake.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "verilated.h"

#include "Vzhao_part_record.h"

#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_part_record top;

  // MASK EVERY FIELD TO ITS WIDTH. Verilator does not mask input ports -- it
  // trusts the driver -- so writing a sign-extended int32 like -5 (0xFFFFFFFB)
  // into an 18-bit input leaves fourteen garbage bits above the field, and the
  // pack concatenation picks them up.
  //
  // This cost a real debugging pass and produced exactly the symptom the header
  // of this file predicts: the ROUND TRIP passed while the comparison against
  // zref FAILED, because the block agreed with itself about garbage. The fault
  // was in the driver, not the block -- which is the other half of that lesson.
  auto set_fields = [&](const zref::part::Particle128& p) {
    top.pos_x_i = static_cast<uint32_t>(p.pos[0]) & 0x3FFFFu;
    top.pos_y_i = static_cast<uint32_t>(p.pos[1]) & 0x3FFFFu;
    top.pos_z_i = static_cast<uint32_t>(p.pos[2]) & 0x3FFFFu;
    top.vel_x_i = static_cast<uint32_t>(p.vel[0]) & 0x7FFu;
    top.vel_y_i = static_cast<uint32_t>(p.vel[1]) & 0x7FFu;
    top.vel_z_i = static_cast<uint32_t>(p.vel[2]) & 0x7FFu;
    top.age_i = p.age & 0x3FFu;
    top.species_i = p.species & 0x7Fu;
    top.size_i = p.size & 0x3Fu;
    top.spin_i = p.spin & 0x3Fu;
    top.flags_i = p.flags & 0xFu;
    top.variation_i = p.variation;
  };

  auto rec_out = [&]() {
    // rec_o is 128 bits: Verilator gives a WData array of four 32-bit words.
    uint64_t lo = (static_cast<uint64_t>(top.rec_o[1]) << 32) | top.rec_o[0];
    uint64_t hi = (static_cast<uint64_t>(top.rec_o[3]) << 32) | top.rec_o[2];
    return std::pair<uint64_t, uint64_t>(lo, hi);
  };

  auto set_rec = [&](uint64_t lo, uint64_t hi) {
    top.rec_i[0] = static_cast<uint32_t>(lo);
    top.rec_i[1] = static_cast<uint32_t>(lo >> 32);
    top.rec_i[2] = static_cast<uint32_t>(hi);
    top.rec_i[3] = static_cast<uint32_t>(hi >> 32);
  };

  // ---- 1: PACK MATCHES THE SPEC, not just itself --------------------------
  int pack_bad = 0, unpack_bad = 0, roundtrip_bad = 0;
  uint32_t s = 0x9A17E5u;
  for (int i = 0; i < 2000; ++i) {
    zref::part::Particle128 p{};
    p.pos[0] = zref::part::sign_extend(rnd(&s) & 0x3FFFFu, 18);
    p.pos[1] = zref::part::sign_extend(rnd(&s) & 0x3FFFFu, 18);
    p.pos[2] = zref::part::sign_extend(rnd(&s) & 0x3FFFFu, 18);
    p.vel[0] = zref::part::sign_extend(rnd(&s) & 0x7FFu, 11);
    p.vel[1] = zref::part::sign_extend(rnd(&s) & 0x7FFu, 11);
    p.vel[2] = zref::part::sign_extend(rnd(&s) & 0x7FFu, 11);
    p.age = static_cast<uint16_t>(rnd(&s) & 0x3FFu);
    p.species = static_cast<uint8_t>(rnd(&s) & 0x7Fu);
    p.size = static_cast<uint8_t>(rnd(&s) & 0x3Fu);
    p.spin = static_cast<uint8_t>(rnd(&s) & 0x3Fu);
    p.flags = static_cast<uint8_t>(rnd(&s) & 0xFu);
    p.variation = static_cast<uint8_t>(rnd(&s) & 0xFFu);

    uint64_t wlo = 0, whi = 0;
    zref::part::particle_pack(p, &wlo, &whi);

    set_fields(p);
    top.eval();
    const auto got = rec_out();
    if (got.first != wlo || got.second != whi) ++pack_bad;

    // and unpacking the SPEC's bytes gives the spec's fields back
    set_rec(wlo, whi);
    top.eval();
    // And SIGN-EXTEND the outputs, the mirror of masking the inputs: a signed
    // 18-bit output arrives as eighteen valid bits in a uint32, not as a
    // negative int32. Comparing it raw makes -5 read as 262,139.
    const auto sx18 = [](uint32_t v) { return zref::part::sign_extend(v, 18); };
    const auto sx11 = [](uint32_t v) { return zref::part::sign_extend(v, 11); };
    if (sx18(top.pos_x_o) != p.pos[0] || sx18(top.pos_y_o) != p.pos[1] ||
        sx18(top.pos_z_o) != p.pos[2] || sx11(top.vel_x_o) != p.vel[0] ||
        sx11(top.vel_y_o) != p.vel[1] || sx11(top.vel_z_o) != p.vel[2] || top.age_o != p.age ||
        top.species_o != p.species || top.size_o != p.size || top.spin_o != p.spin ||
        top.flags_o != p.flags || top.variation_o != p.variation)
      ++unpack_bad;

    // round trip through the block alone
    set_rec(got.first, got.second);
    top.eval();
    top.pos_x_i = top.pos_x_o & 0x3FFFFu;
    top.pos_y_i = top.pos_y_o & 0x3FFFFu;
    top.pos_z_i = top.pos_z_o & 0x3FFFFu;
    top.vel_x_i = top.vel_x_o & 0x7FFu;
    top.vel_y_i = top.vel_y_o & 0x7FFu;
    top.vel_z_i = top.vel_z_o & 0x7FFu;
    top.age_i = top.age_o;
    top.species_i = top.species_o;
    top.size_i = top.size_o;
    top.spin_i = top.spin_o;
    top.flags_i = top.flags_o;
    top.variation_i = top.variation_o;
    top.eval();
    const auto again = rec_out();
    if (again != got) ++roundtrip_bad;
  }

  zhao::check(pack_bad == 0,
              "the packed record is bit-identical to zref::part::particle_pack "
              "over 2,000 random particles",
              0, pack_bad);
  zhao::check(unpack_bad == 0, "and unpacking the SPEC's bytes returns the spec's fields", 0,
              unpack_bad);
  zhao::check(roundtrip_bad == 0, "and pack(unpack(r)) == r", 0, roundtrip_bad);

  // ---- 2: FIELD ISOLATION -------------------------------------------------
  // Round trip and reference agreement both survive two fields overlapping by
  // a bit. This does not: change one field, and exactly its own bits move.
  {
    struct F {
      const char* name;
      int off, width;
    };
    const F fields[] = {
        {"pos_x", 0, 18},  {"pos_y", 18, 18}, {"pos_z", 36, 18}, {"vel_x", 54, 11},
        {"vel_y", 65, 11}, {"vel_z", 76, 11}, {"age", 87, 10},   {"species", 97, 7},
        {"size", 104, 6},  {"spin", 110, 6},  {"flags", 116, 4}, {"variation", 120, 8},
    };
    zref::part::Particle128 zero{};
    uint64_t zlo = 0, zhi = 0;
    zref::part::particle_pack(zero, &zlo, &zhi);
    set_fields(zero);
    top.eval();
    const auto base = rec_out();

    int isolation_bad = 0;
    int total_bits = 0;
    for (const F& f : fields) {
      zref::part::Particle128 p{};
      // all ones in this field only
      const uint32_t all = (f.width >= 32) ? 0xFFFFFFFFu : ((1u << f.width) - 1u);
      if (f.off == 0)
        p.pos[0] = zref::part::sign_extend(all, 18);
      else if (f.off == 18)
        p.pos[1] = zref::part::sign_extend(all, 18);
      else if (f.off == 36)
        p.pos[2] = zref::part::sign_extend(all, 18);
      else if (f.off == 54)
        p.vel[0] = zref::part::sign_extend(all, 11);
      else if (f.off == 65)
        p.vel[1] = zref::part::sign_extend(all, 11);
      else if (f.off == 76)
        p.vel[2] = zref::part::sign_extend(all, 11);
      else if (f.off == 87)
        p.age = static_cast<uint16_t>(all);
      else if (f.off == 97)
        p.species = static_cast<uint8_t>(all);
      else if (f.off == 104)
        p.size = static_cast<uint8_t>(all);
      else if (f.off == 110)
        p.spin = static_cast<uint8_t>(all);
      else if (f.off == 116)
        p.flags = static_cast<uint8_t>(all);
      else
        p.variation = static_cast<uint8_t>(all);

      set_fields(p);
      top.eval();
      const auto got = rec_out();
      const uint64_t dlo = got.first ^ base.first;
      const uint64_t dhi = got.second ^ base.second;

      // exactly f.width bits changed, all inside [off, off+width)
      int moved = 0;
      for (int b = 0; b < 128; ++b) {
        const bool bit = (b < 64) ? ((dlo >> b) & 1u) : ((dhi >> (b - 64)) & 1u);
        if (!bit) continue;
        ++moved;
        if (b < f.off || b >= f.off + f.width) ++isolation_bad;
      }
      if (moved != f.width) ++isolation_bad;
      total_bits += f.width;
    }
    zhao::check(isolation_bad == 0,
                "each field owns exactly its own bits and no others -- two "
                "fields overlapping by a bit would still round-trip",
                0, isolation_bad);
    zhao::check(total_bits == 128,
                "and the twelve fields account for all 128 bits, with none "
                "spare and none shared",
                128, total_bits);
  }

  // ---- 3: the two derived values -----------------------------------------
  // radius = base * size / 16 with ONE round-half-up on the whole product.
  // The wrong forms -- rounding before the shift, or truncating -- agree with
  // this one at every even size, so the sweep covers all 64.
  {
    int radius_bad = 0, angle_bad = 0;
    const int32_t bases[] = {0, 1, 15, 16, 17, 255, 4096, 65535, 1'000'003};
    for (int32_t base : bases)
      for (int sz = 0; sz < 64; ++sz) {
        top.base_radius_i = base;
        top.size_i = static_cast<uint8_t>(sz);
        top.spin_i = static_cast<uint8_t>(sz);
        top.eval();
        if (static_cast<int32_t>(top.radius_o) !=
            zref::part::particle_radius(base, static_cast<uint8_t>(sz)))
          ++radius_bad;
        if (top.angle16_o != zref::part::particle_angle16(static_cast<uint8_t>(sz))) ++angle_bad;
      }
    zhao::check(radius_bad == 0,
                "radius = base * size / 16 with ONE round-half-up, over every "
                "size 0..63 and nine bases",
                0, radius_bad);
    zhao::check(angle_bad == 0, "angle16 = spin << 10, and the phase wraps mod 64", 0, angle_bad);
  }

  std::printf("  particle128 v%u: 2,000 records, 12 fields, 128 bits accounted\n",
              zref::part::kParticleFormatVersion);

  return zhao::report_and_exit("part_record_directed");
}
