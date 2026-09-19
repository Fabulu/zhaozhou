// raster_state_directed.cpp -- the raster_state u32 layout, owner ruling R28,
// against its model `zref::raster_state` AND against its consumer.
//
//   1  every DrawForm.flags value's cull field lands at raster_state[1:0];
//      the material's bits [31:2] are carried unchanged, its [1:0] never read.
//   2  cull mode 3 is REFUSED (ok = false, word 0), never aliased to a mode.
//   3  the draw's OTHER flag bits (billboard, screen-space size, reserved) do
//      not leak into the word.
//   4  THE CONSUMER'S ENCODING: zhao_geom_clip.sv's own CULL_NEG / CULL_POS
//      localparams are read back from the RTL source and must equal the
//      model's. A differential against the file, not a shared constant -- if
//      the clip's encoding moves, this fails instead of agreeing with itself.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "zref/zref_raster_state.hpp"

namespace fs = std::filesystem;
namespace rs = zref::raster_state;

namespace {

int g_checks = 0, g_fail = 0;
void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

fs::path repo_root() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 6 && !fs::exists(dir / "spec" / "commands.zidl"); ++i) dir = dir.parent_path();
  return dir;
}

}  // namespace

int main() {
  // ---- 1 + 3: every draw-flags value, a material with every bit set ---------
  int bad = 0, refused = 0;
  for (uint32_t f = 0; f < 0x10000u; ++f) {
    bool ok = false;
    const uint32_t w = rs::compose(static_cast<uint16_t>(f), 0xFFFF'FFFFu, &ok);
    const uint32_t cull = (f >> 2) & 3u;
    if (cull == 3u) {
      if (ok || w != 0u) ++bad;
      ++refused;
      continue;
    }
    if (!ok || w != (0xFFFF'FFFCu | cull) || rs::cull_mode(w) != cull) ++bad;
  }
  ck(bad == 0, "1/3: raster_state = material[31:2] | DrawForm.flags[3:2], for all 65536 flag words");
  ck(refused == 0x4000, "2: exactly the flag words naming cull 3 are refused");
  {
    bool ok = true;
    ck(rs::compose(0x0000u, 0x0000'0003u, &ok) == 0u && ok,
       "1: the material's bits [1:0] are never read -- the cull mode is the draw's");
    ck(rs::compose(0x0003u, 0u, &ok) == 0u && ok,
       "3: billboard / screen-space flag bits do not leak into the word (a v1 draw reads NONE)");
    ck(rs::compose(0x0008u, 0x0000'1230u, &ok) == (0x0000'1230u | rs::kCullPos) && ok,
       "1: flags[3:2] = 2 -> POS, material bits carried");
  }

  // ---- 4: the consumer's encoding, read from the RTL ------------------------
  {
    std::ifstream in(repo_root() / "fpga" / "rtl" / "geometry" / "zhao_geom_clip.sv");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string src = ss.str();
    ck(!src.empty(), "4: zhao_geom_clip.sv was read");
    const std::regex re_neg(R"(localparam\s+logic\s*\[1:0\]\s*CULL_NEG\s*=\s*2'd(\d))");
    const std::regex re_pos(R"(localparam\s+logic\s*\[1:0\]\s*CULL_POS\s*=\s*2'd(\d))");
    std::smatch m;
    const bool has_neg = std::regex_search(src, m, re_neg);
    const int neg = has_neg ? std::stoi(m[1]) : -1;
    const bool has_pos = std::regex_search(src, m, re_pos);
    const int pos = has_pos ? std::stoi(m[1]) : -1;
    ck(has_neg && has_pos, "4: the clip's CULL_NEG / CULL_POS localparams were found");
    ck(neg == rs::kCullNeg, "4: the model's NEG is the clip's CULL_NEG");
    ck(pos == rs::kCullPos, "4: the model's POS is the clip's CULL_POS");
  }

  std::printf("raster_state_directed: %d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
