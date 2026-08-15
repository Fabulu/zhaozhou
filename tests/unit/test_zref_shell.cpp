// test_zref_shell.cpp — the W6 ZRef shell unit test (plan W6, charter §23
// Phase-1 gate item "one empty frame replays through ZRef", session layer).
//
//   1. the committed golden frame_minimal.bin replays GREEN through the
//      shell: ZH_ABI_OK / ZHAO_COMPL_DONE with the exact §3.3 counters;
//   2. corrupt variants return the right fail-safe error codes (spec
//      capture_format.md §3.2): magic -> BAD_MAGIC (36 bytes consumed),
//      header CRC -> BAD_HEADER_CRC, payload byte -> BAD_PAYLOAD_CRC
//      (whole packet consumed);
//   3. SetView + SetPresentationContract (implemented, semantically no-op
//      in Phase 1) execute as counted no-ops — spec §3.3;
//   4. session accumulation: accepted + rejected frames sum across submits;
//      reset() clears the session;
//   5. a reserved command (DrawSky 0x0310 since the wave-3 D7 promotions
//      implemented DrawForm; DrawForm 0x0300 was the wave-2 vehicle)
//      validates structurally but the executor refuses it with the exact
//      code ZH_ABI_UNIMPLEMENTED_COMMAND (review m2, RUN-20260814-1912).

#include "zhao_abi.h"  // generated
#include "zref/zref.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using namespace zhao_abi;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

fs::path repo_root() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 4 && !fs::exists(dir / "spec" / "commands.zidl"); i++) {
    dir = dir.parent_path();
  }
  return dir;
}

std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const std::vector<uint8_t> golden =
      read_file(repo_root() / "tests" / "abi" / "golden" / "frame_minimal.bin");
  check(golden.size() == 120, "golden frame_minimal.bin is the 120-byte packet");

  // -- 1. golden replay (BeginFrame + Nop + EndFrame) ----------------------
  {
    zhao::ZhaoZrefShell shell;
    const zhao::ZhaoExecutionResult r = shell.submit(golden);
    check(r.status == ZH_ABI_OK, "golden replays with ZH_ABI_OK");
    check(r.completion_flags == ZHAO_COMPL_DONE, "golden completes DONE");
    check(r.counters.commands_total == 3, "golden counts 3 commands");
    check(r.counters.begin_frames == 1, "golden counts 1 begin_frames");
    check(r.counters.end_frames == 1, "golden counts 1 end_frames");
    check(r.counters.nops == 1, "golden counts 1 nop");
    check(r.counters.frames_accepted == 1 && r.counters.frames_rejected == 0,
          "golden frame accepted");
    check(r.counters.bytes_consumed == 120, "golden consumes the whole packet");
    check(shell.session().frames_submitted == 1 && shell.session().frames_accepted == 1 &&
              shell.session().commands_total == 3,
          "session counters mirror the single green frame");
  }

  // -- 2. corrupt variants -> exact fail-safe error codes (spec 3.2) -------
  {
    zhao::ZhaoZrefShell shell;

    std::vector<uint8_t> bad_magic = golden;
    bad_magic[0] ^= 0xFF;  // ZPK1 magic destroyed (header-level abort)
    const zhao::ZhaoExecutionResult r1 = shell.submit(bad_magic);
    check(r1.status == ZH_ABI_BAD_MAGIC, "corrupt magic -> ZH_ABI_BAD_MAGIC");
    check(r1.completion_flags == ZHAO_COMPL_ERR, "corrupt magic completes ERR");
    check(r1.counters.bytes_consumed == 36,
          "magic abort consumes exactly 36 bytes (spec 3.2 header-level rule)");

    std::vector<uint8_t> bad_hcrc = golden;
    bad_hcrc[ZHAO_OFF_HEADER_CRC] ^= 0x01;  // header CRC word
    const zhao::ZhaoExecutionResult r2 = shell.submit(bad_hcrc);
    check(r2.status == ZH_ABI_BAD_HEADER_CRC, "corrupt header CRC -> ZH_ABI_BAD_HEADER_CRC");
    check(r2.counters.bytes_consumed == 36,
          "header-level abort consumes exactly 36 bytes (spec 3.2)");

    std::vector<uint8_t> bad_pcrc = golden;
    bad_pcrc[ZHAO_FRAME_HEADER_BYTES + 4] ^= 0x80;  // inside the command stream
    const zhao::ZhaoExecutionResult r3 = shell.submit(bad_pcrc);
    check(r3.status == ZH_ABI_BAD_PAYLOAD_CRC, "corrupt payload -> ZH_ABI_BAD_PAYLOAD_CRC");
    check(r3.counters.bytes_consumed == 120,
          "payload verdict consumes the whole packet (spec 3.2)");

    check(shell.session().frames_submitted == 3 && shell.session().frames_rejected == 3 &&
              shell.session().frames_accepted == 0 && shell.session().commands_total == 0,
          "three corrupt frames rejected, none executed");

    shell.reset();
    check(shell.session().frames_submitted == 0 && shell.last_status() == ZH_ABI_OK,
          "reset clears session and last-frame latch");
  }

  // -- 3. SetView/SetPresentationContract: implemented, counted no-ops -----
  {
    std::vector<uint8_t> rec;
    zhao::ZhaoFrameBuilder fb;
    zhao_pack_set_view(zhao_sample_set_view(), rec);
    fb.append_record(rec);
    rec.clear();
    zhao_pack_set_presentation_contract(zhao_sample_set_presentation_contract(), rec);
    fb.append_record(rec);
    const std::vector<uint8_t> pkt = fb.seal(/*frame_id=*/7, /*sequence=*/0, /*resource_epoch=*/1);

    zhao::ZhaoZrefShell shell;
    const zhao::ZhaoExecutionResult r = shell.submit(pkt);
    check(r.status == ZH_ABI_OK, "SetView+SetPresentationContract frame executes OK (spec 3.3)");
    check(r.completion_flags == ZHAO_COMPL_DONE, "presentation frame completes DONE");
    check(r.counters.commands_total == 2 && r.counters.begin_frames == 0 &&
              r.counters.end_frames == 0 && r.counters.nops == 0,
          "implemented-but-semantically-empty commands still count");
  }

  // -- 3b/m2: a RESERVED command validates structurally but is refused at ---
  // -- execution time with the exact executor-level code (spec 3.2/3.3). ----
  // -- Test vehicle is DrawSky 0x0310 (reserved until wave 8): the wave-3 ---
  // -- D7 promotions moved DrawForm and the other Phase-3 commands into ------
  // -- the implemented set (spec/commands.zidl, run RUN-20260815-0544). ------
  {
    std::vector<uint8_t> rec;
    zhao::ZhaoFrameBuilder fb;
    zhao_pack_draw_sky(zhao_sample_draw_sky(), rec);  // 0x0310, reserved
    fb.append_record(rec);
    const std::vector<uint8_t> pkt = fb.seal(/*frame_id=*/8, /*sequence=*/0, /*resource_epoch=*/1);

    zhao::ZhaoZrefShell shell;
    const zhao::ZhaoExecutionResult r = shell.submit(pkt);
    check(r.status == ZH_ABI_UNIMPLEMENTED_COMMAND,
          "reserved DrawSky executes -> ZH_ABI_UNIMPLEMENTED_COMMAND (14)");
    check(r.completion_flags == ZHAO_COMPL_ERR, "unimplemented command completes ERR");
    check(r.counters.frames_rejected == 1 && r.counters.frames_accepted == 0,
          "unimplemented command rejects the frame");
    check(r.counters.commands_total == 0, "nothing counts as executed before the refusal");
    check(r.counters.bytes_consumed == pkt.size(),
          "the full validated packet is consumed before execution");
  }

  if (failures != 0) {
    std::fprintf(stderr, "test_zref_shell: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("test_zref_shell: all green\n");
  return 0;
}
