// pack_field_host.cpp -- the ZFH2 host-image PACKER (owner directive
// reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt section
// 5.2 "and a tool: tools/field/pack_field_host.cpp"; section 11.5 "The packer
// emits a machine-readable manifest and golden capsule bytes").
//
// It lowers a validated .zprog onto the current v3 fabric through the ONE
// library (zfield/zfield_host_plan.hpp) that the benchmark also uses, and
// writes the capsule the hardware loader test must receive over the modelled
// bridge -- rather than a parallel handwritten sequence of raw loader writes
// assumed to be equivalent.
//
// THE REFUSAL THAT MATTERS (R111, and FT030). A descriptor that declares
// outputs while its required_mask is zero is REFUSED here. R111 records why
// this is not a theoretical guard: nothing in the tree emits a header word at
// all, so mask == 0 is the ONLY case that occurs today, and "a plan writer who
// omits the mask silently restores the [R101] defect and passes every gate".
// The packer is the plan writer. This is where that stops.
//
// Usage:
//   pack_field_host <in.zprog> [-o <out.zfh>] [--varying-mask N]
//                   [--out-base N] [--out-lanes N] [--scalar-base N]
//                   [--registers N] [--contract] [--manifest <path>]
//                   [--force-empty-mask]
//
// --force-empty-mask exists ONLY to drive FT030's control: it clears the
// required mask after lowering so the refusal can be SEEN TO FIRE. It cannot
// produce an image -- the packer refuses before writing a byte.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_host_plan.hpp"
#include "zfield/zfield_plan.hpp"

namespace {

bool read_file(const char* path, std::vector<uint8_t>* out) {
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
  const size_t got = n > 0 ? std::fread(out->data(), 1, (size_t)n, f) : 0;
  std::fclose(f);
  return got == (size_t)n;
}

bool write_file(const char* path, const void* p, size_t n) {
  std::FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const size_t put = n > 0 ? std::fwrite(p, 1, n, f) : 0;
  std::fclose(f);
  return put == n;
}

}  // namespace

int main(int argc, char** argv) {
  const char* in_path = nullptr;
  const char* out_path = nullptr;
  const char* manifest_path = nullptr;
  uint32_t varying_mask = 0x3;  // Earth: lanes 0 and 1 (x, z) vary
  bool force_empty_mask = false;
  zfield::host_plan::LowerOptions opt;

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    auto val = [&](int& i_) -> const char* { return (i_ + 1 < argc) ? argv[++i_] : nullptr; };
    if (std::strcmp(a, "-o") == 0) {
      out_path = val(i);
    } else if (std::strcmp(a, "--manifest") == 0) {
      manifest_path = val(i);
    } else if (std::strcmp(a, "--varying-mask") == 0) {
      const char* v = val(i);
      if (v) varying_mask = (uint32_t)std::strtoul(v, nullptr, 0);
    } else if (std::strcmp(a, "--out-base") == 0) {
      const char* v = val(i);
      if (v) opt.out_base = (int)std::strtol(v, nullptr, 0);
    } else if (std::strcmp(a, "--out-lanes") == 0) {
      const char* v = val(i);
      if (v) opt.out_lanes = (int)std::strtol(v, nullptr, 0);
    } else if (std::strcmp(a, "--scalar-base") == 0) {
      const char* v = val(i);
      if (v) opt.scalar_base = (int)std::strtol(v, nullptr, 0);
    } else if (std::strcmp(a, "--registers") == 0) {
      const char* v = val(i);
      if (v) opt.register_ceiling = (int)std::strtol(v, nullptr, 0);
    } else if (std::strcmp(a, "--contract") == 0) {
      opt.contract = true;
    } else if (std::strcmp(a, "--force-empty-mask") == 0) {
      force_empty_mask = true;
    } else if (a[0] != '-') {
      in_path = a;
    } else {
      std::printf("pack_field_host: unknown option %s\n", a);
      return 2;
    }
  }

  if (!in_path) {
    std::printf(
        "usage: pack_field_host <in.zprog> [-o out.zfh] [--manifest p]\n"
        "       [--varying-mask N] [--out-base N] [--out-lanes N]\n"
        "       [--scalar-base N] [--registers N] [--contract]\n"
        "       [--force-empty-mask]   (FT030 control: refuses, never writes)\n");
    return 2;
  }

  std::vector<uint8_t> bytes;
  if (!read_file(in_path, &bytes)) {
    std::printf("pack_field_host: cannot read %s\n", in_path);
    return 2;
  }

  // The canonical decoder runs its FULL validator on every load; nothing here
  // second-guesses it or works around a refusal.
  zfield::DecodeResult dr = zfield::decode(bytes.data(), bytes.size());
  if (dr.error != zfield::DecodeError::kOk) {
    std::printf("pack_field_host: REFUSED at decode: %s (%s)\n",
                zfield::decodeErrorName(dr.error), dr.detail.c_str());
    return 1;
  }

  const zfield::Fplan fp = zfield::plan(dr.prog, varying_mask);
  // Uniform inputs need values to prepare; a packer run from a bare .zprog has
  // none, so the association's canonical inputs are zero here and the PRELOAD
  // VALUES are placeholders. The maps, the masks and the proof do not depend on
  // them -- they depend on WHICH registers are written -- and the association
  // builder supplies the real values at seal (directive 6.3). Said out loud
  // because a zero that looks like data is how a placeholder ships.
  std::vector<int32_t> zero_in(dr.prog.in_lanes.size(), 0);
  const zfield::Prepared prep =
      zfield::prepare(fp, dr.prog, zero_in.data(), zero_in.size());

  opt.canonical_program_handle32 = fp.canonical_hash;
  opt.source_id32 = dr.prog.source_id;

  zfield::host_plan::HostPlan hp;
  std::string refusal;
  if (!zfield::host_plan::lower(fp, dr.prog, &prep, opt, &hp, &refusal)) {
    std::printf("pack_field_host: REFUSED at lowering: %s\n", refusal.c_str());
    return 1;
  }

  if (force_empty_mask) {
    // FT030's control. Clearing the mask is exactly the mistake R111 describes
    // -- a plan writer that omits it -- and the packer must not be able to ship
    // the result. See the re-verification below.
    hp.required_mask = zfield::host_plan::RequiredMask(0);
  }

  // The strict output contract is re-checked HERE, on the descriptor about to
  // be written, and not merely inside lower(). A check that only runs on the
  // path that constructed the value cannot see a value that was edited after.
  if (!zfield::host_plan::verify_output_contract(hp, &refusal)) {
    std::printf("pack_field_host: REFUSED at the output contract: %s\n", refusal.c_str());
    return 1;
  }

  std::vector<uint8_t> image;
  if (!zfield::host_plan::serialize_program_image(hp, bytes.data(), bytes.size(), &image,
                                                  &refusal)) {
    std::printf("pack_field_host: REFUSED at serialisation: %s\n", refusal.c_str());
    return 1;
  }

  const std::string man = zfield::host_plan::manifest(hp, image);
  std::printf("%s", man.c_str());

  if (out_path && !write_file(out_path, image.data(), image.size())) {
    std::printf("pack_field_host: cannot write %s\n", out_path);
    return 2;
  }
  if (manifest_path && !write_file(manifest_path, man.data(), man.size())) {
    std::printf("pack_field_host: cannot write %s\n", manifest_path);
    return 2;
  }
  return 0;
}
