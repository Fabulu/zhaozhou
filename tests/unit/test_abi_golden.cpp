// test_abi_golden.cpp — the C++ testee of the byte-identity matrix (plan W4
// gate (e), spec/capture_format.md 6): the generated zhao_abi.h must
// reproduce every committed golden byte-for-byte — per-command sample
// records, the minimal sealed frame, and (via the .zcap writer) the minimal
// .zcap. The TS testee runs in the compiler workspace against the same
// files; the SV testee is driven through the Verilated probe.

#include "zhao_abi.h"  // generated

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path golden_dir() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 4 && !fs::exists(dir / "spec" / "commands.zidl"); i++) {
    dir = dir.parent_path();
  }
  return dir / "tests" / "abi" / "golden";
}

std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  using namespace zhao_abi;
  const auto dir = golden_dir();

  struct Entry {
    const char* snake;
    uint16_t opcode;
  };
  const Entry cmds[] = {
    {"nop", ZHAO_OP_NOP},
    {"begin_frame", ZHAO_OP_BEGIN_FRAME},
    {"end_frame", ZHAO_OP_END_FRAME},
    {"set_view", ZHAO_OP_SET_VIEW},
    {"set_presentation_contract", ZHAO_OP_SET_PRESENTATION_CONTRACT},
    {"terrain_field", ZHAO_OP_TERRAIN_FIELD},
    {"surface_stamp", ZHAO_OP_SURFACE_STAMP},
    {"draw_form", ZHAO_OP_DRAW_FORM},
    {"draw_population", ZHAO_OP_DRAW_POPULATION},
    {"draw_procedural", ZHAO_OP_DRAW_PROCEDURAL},
    {"emit_audio_event", ZHAO_OP_EMIT_AUDIO_EVENT},
    {"debug_bootstrap", ZHAO_OP_DEBUG_BOOTSTRAP},
    {"draw_sky", ZHAO_OP_DRAW_SKY},
    {"debug_frame_blit", ZHAO_OP_DEBUG_FRAME_BLIT},
    {"debug_rumble", ZHAO_OP_DEBUG_RUMBLE},
  };

  // ---- per-command goldens: pack(sample) == committed bytes -----------------
  for (const auto& e : cmds) {
    const auto want = read_file(dir / ("cmd_" + std::string(e.snake) + ".bin"));
    zhao::check(!want.empty(), (std::string("golden present: ") + e.snake).c_str(), 1, want.size());

    std::vector<uint8_t> got;
    switch (e.opcode) {
      case ZHAO_OP_NOP: zhao_pack_nop(zhao_sample_nop(), got); break;
      case ZHAO_OP_BEGIN_FRAME: zhao_pack_begin_frame(zhao_sample_begin_frame(), got); break;
      case ZHAO_OP_END_FRAME: zhao_pack_end_frame(zhao_sample_end_frame(), got); break;
      case ZHAO_OP_SET_VIEW: zhao_pack_set_view(zhao_sample_set_view(), got); break;
      case ZHAO_OP_SET_PRESENTATION_CONTRACT:
        zhao_pack_set_presentation_contract(zhao_sample_set_presentation_contract(), got); break;
      case ZHAO_OP_TERRAIN_FIELD: zhao_pack_terrain_field(zhao_sample_terrain_field(), got); break;
      case ZHAO_OP_SURFACE_STAMP: zhao_pack_surface_stamp(zhao_sample_surface_stamp(), got); break;
      case ZHAO_OP_DRAW_FORM: zhao_pack_draw_form(zhao_sample_draw_form(), got); break;
      case ZHAO_OP_DRAW_POPULATION: zhao_pack_draw_population(zhao_sample_draw_population(), got); break;
      case ZHAO_OP_DRAW_PROCEDURAL: zhao_pack_draw_procedural(zhao_sample_draw_procedural(), got); break;
      case ZHAO_OP_EMIT_AUDIO_EVENT: zhao_pack_emit_audio_event(zhao_sample_emit_audio_event(), got); break;
      case ZHAO_OP_DEBUG_BOOTSTRAP: zhao_pack_debug_bootstrap(zhao_sample_debug_bootstrap(), got); break;
      case ZHAO_OP_DRAW_SKY: zhao_pack_draw_sky(zhao_sample_draw_sky(), got); break;
      case ZHAO_OP_DEBUG_FRAME_BLIT: zhao_pack_debug_frame_blit(zhao_sample_debug_frame_blit(), got); break;
      case ZHAO_OP_DEBUG_RUMBLE: zhao_pack_debug_rumble(zhao_sample_debug_rumble(), got); break;
      default: break;
    }

    bool equal = got.size() == want.size();
    for (size_t i = 0; equal && i < want.size(); i++) equal = got[i] == want[i];
    zhao::check(equal, (std::string("byte-identity cmd_") + e.snake + ".bin").c_str(),
                want.size(), equal ? want.size() : got.size());
  }

  // ---- round-trip: pack -> unpack -> pack is stable ---------------------------
  {
    std::vector<uint8_t> a, b;
    zhao_pack_set_view(zhao_sample_set_view(), a);
    ZhReader r(a.data(), a.size());
    ZhRecordSetView decoded{};
    const bool ok = zhao_unpack_set_view(r, decoded);
    zhao::check(ok, "unpack set_view", 1, ok ? 1 : 0);
    zhao_pack_set_view(decoded, b);
    bool equal = a == b;
    zhao::check(equal, "pack(unpack(x)) == x (set_view)", 1, equal ? 1 : 0);
  }

  // ---- minimal frame golden: C++ builder == committed bytes --------------------
  {
    // canonical minimal frame = the three sample records verbatim (exactly
    // what the generator sealed into the golden)
    std::vector<uint8_t> rec;
    zhao::ZhaoFrameBuilder fb;
    zhao_pack_begin_frame(zhao_sample_begin_frame(), rec);
    fb.append_record(rec);
    rec.clear();
    zhao_pack_nop(zhao_sample_nop(), rec);
    fb.append_record(rec);
    rec.clear();
    zhao_pack_end_frame(zhao_sample_end_frame(), rec);
    fb.append_record(rec);
    const auto pkt = fb.seal(1, 0, 1, 0, 0);
    const auto want = read_file(dir / "frame_minimal.bin");
    bool equal = pkt == want;
    zhao::check(equal, "byte-identity frame_minimal.bin", want.size(),
                equal ? want.size() : pkt.size());

    const auto v = zhao::zhao_frame_validate(pkt);
    zhao::check(v.error == ZH_ABI_OK, "frame_minimal validates", ZH_ABI_OK, v.error);
    zhao::check(v.commands_consumed == 3, "frame_minimal commands", 3, v.commands_consumed);
    zhao::check(v.bytes_consumed == want.size(), "frame_minimal bytes_consumed",
                want.size(), v.bytes_consumed);

    if (!equal) {
      zhao::save_failing_vector("abi_frame_minimal_mismatch", pkt,
                                "byte-identical to tests/abi/golden/frame_minimal.bin",
                                std::to_string(pkt.size()) + " bytes (mismatch)");
    }
  }

  // ---- minimal .zcap golden: parses, verifies, embeds frame_minimal ----------
  {
    const auto frame = read_file(dir / "frame_minimal.bin");
    zhao::ZhaoZcapReader reader((dir / "zcap_minimal.zcap").string());
    zhao::check(reader.open() == zhao::ZhaoZcapError::kOk, "golden zcap opens clean",
                0, int(reader.open()));
    zhao::check(reader.sections().size() == 3, "golden zcap sections", 3,
                reader.sections().size());
    std::vector<uint8_t> body;
    const auto* fp = reader.find(zhao::ZhaoZcapSection::ZHAO_ZCAP_FRAME_PACKET);
    zhao::check(fp != nullptr, "golden zcap has FRAME_PACKET", 1, fp != nullptr ? 1 : 0);
    if (fp != nullptr && reader.read_body(*fp, body)) {
      bool equal = body == frame;
      zhao::check(equal, "golden zcap embeds frame_minimal.bin", 1, equal ? 1 : 0);
    } else {
      zhao::check(false, "golden zcap FRAME_PACKET body verifies", 1, 0);
    }
  }

  return zhao::report_and_exit("test_abi_golden");
}
