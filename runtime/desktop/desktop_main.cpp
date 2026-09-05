// desktop_main.cpp — the desktop console host.
// Rewritten 2026-09-05 (software lane). It was a 64-line stub.
//
// ---------------------------------------------------------------------------
// WHAT CHANGED AND WHY
// ---------------------------------------------------------------------------
// The previous file replayed sealed frame packets through the empty ZRef shell
// and exited 0. The roadmap's audit was blunt about it:
//
//   > zemu_main.cpp and desktop_main.cpp currently replay packets through the
//   > empty shell. They are not interactive game hosts or complete console
//   > emulators. ... somebody must turn those pieces into products.
//
// This is a HOST: it owns the fixed tick, drives a simulation through the
// shared console-runtime boundary, records the session, and can replay it and
// report the exact tick of any divergence.
//
//   host --ticks N [--seed S] [--record FILE]      run a match
//   host --replay FILE                             replay and verify one
//
// ---------------------------------------------------------------------------
// WHAT IS HONESTLY NOT HERE YET
// ---------------------------------------------------------------------------
// **Audio output.** None. The tone table exists in the reference; nothing here
// asks for it yet.
//
// **A window.** VIDEO IS RENDERED but not displayed: `submit` runs the frame
// through `zref::render::SoftwareRenderer` -- the same code the capture tools
// and the RTL differentials agree with, per the roadmap's instruction to reuse
// the exact machinery rather than add a renderer beside it -- and `--ppm`
// writes the canvas out to look at. Opening a window is presentation plumbing
// and changes nothing above the backend boundary.
//
// What is drawn is TWO MARKERS, not two wizards: DrawForm quads at the
// simulation's positions. That is the whole picture the frozen command set can
// express for a character today -- meshes need G2's resource formats and G3's
// geometry path. Stated here so the picture is not mistaken for more than it
// is.
//
// **Real controllers.** Input is scripted or idle. The `Backend::poll`
// boundary is where a real pad arrives, and nothing above it changes when one
// does -- which is the point of the boundary.
//
// **Cartridge loading.** `--cart` reads a file and publishes it as one
// MESH_STREAM resource so the lifecycle is exercised end to end; the real
// cartridge parse is G2's resource-format work, and pretending otherwise would
// invent a format before it is frozen.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zcon/frame_build.hpp"
#include "zcon/session_io.hpp"
#include "zcon/zcon.hpp"
#include "zgame/wizards.hpp"

#include "zref/zref_render.hpp"

namespace {

// C stdio rather than <fstream>, and the reason is measured rather than
// stylistic: on this toolchain (winlibs g++ 15.x, MinGW) an `std::ofstream`
// write faults at -O1 and works at -O0. It was bisected to exactly this
// 592 bytes..." printed, and the process died inside the stream write.
// stdio has no such problem and this file has no need of iostreams.
std::vector<uint8_t> read_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return {};
  std::vector<uint8_t> v;
  uint8_t buf[4096];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + n);
  std::fclose(f);
  return v;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& bytes) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const std::size_t n =
      bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
  const bool ok = (n == bytes.size());
  return (std::fclose(f) == 0) && ok;
}

// ---------------------------------------------------------------------------
// The desktop backend. Owns resource memory and accepts presentation work.
// ---------------------------------------------------------------------------
class DesktopBackend : public zcon::Backend {
 public:
  struct Resource {
    zcon::ResourceKind kind;
    std::vector<uint8_t> bytes;
    uint16_t generation;
  };

  zcon::Handle publish(zcon::ResourceKind kind, const uint8_t* bytes,
                       std::size_t len) override {
    Resource r;
    r.kind = kind;
    r.bytes.assign(bytes, bytes + len);
    // Generations start at 1 because 0 means "never published" -- a stale
    // handle must be detectable, not merely unlucky (D-3).
    r.generation = static_cast<uint16_t>(resources_.size() + 1);
    resources_.push_back(std::move(r));
    zcon::Handle h;
    h.index = static_cast<uint32_t>(resources_.size() - 1);
    h.generation = resources_.back().generation;
    h.kind = kind;
    return h;
  }

  zcon::InputSnapshot poll(uint32_t tick) override {
    if (tick < scripted_.size()) return scripted_[tick];
    zcon::InputSnapshot s;
    s.tick = tick;
    return s;
  }

  // Presentation work is now RENDERED, not counted. The roadmap's instruction
  // was to turn the existing exact reference machinery into a reusable console
  // backend rather than adding another renderer beside it -- so this calls
  // zref::render::SoftwareRenderer, the same code the capture tools and the
  // RTL differentials already agree with.
  void submit(const std::vector<uint8_t>& commands) override {
    ++frames_;
    command_bytes_ += commands.size();
    if (commands.empty()) return;
    const zref::render::RenderResult r =
        renderer_.render_frame(commands, 0, canvas_, rres_);
    last_status_ = r.status;
    last_crc_ = r.canvas_crc32c;
    commands_executed_ += r.commands_executed;
    resource_misses_ += r.resource_misses;
  }

  zref::render::RenderResources& resources() { return rres_; }
  const zref::render::RenderCanvas& canvas() const { return canvas_; }
  uint8_t last_status() const { return last_status_; }
  uint32_t last_crc() const { return last_crc_; }
  uint32_t commands_executed() const { return commands_executed_; }
  uint32_t resource_misses() const { return resource_misses_; }
  zhao_abi::video_mode latched_mode() const { return renderer_.latched_mode(); }

  const char* name() const override { return "desktop"; }

  void set_script(std::vector<zcon::InputSnapshot> s) { scripted_ = std::move(s); }
  std::size_t resource_count() const { return resources_.size(); }
  uint64_t frames() const { return frames_; }
  uint64_t command_bytes() const { return command_bytes_; }

 private:
  std::vector<Resource> resources_;
  std::vector<zcon::InputSnapshot> scripted_;
  uint64_t frames_ = 0;
  uint64_t command_bytes_ = 0;
  zref::render::SoftwareRenderer renderer_;
  zref::render::RenderCanvas canvas_;
  zref::render::RenderResources rres_;  // NOT resources_: that is the
                                        // published-handle table above.
  uint8_t last_status_ = 0;
  uint32_t last_crc_ = 0;
  uint32_t commands_executed_ = 0;
  uint32_t resource_misses_ = 0;
};

// A deterministic demo script, so `--ticks N` alone produces a real match
// rather than two wizards standing still. Bolts fired with no aim travel
// toward the opponent.
//
// THE SHAPE OF THE MOTION IS DELIBERATE, and the earlier version was wrong in
// a way only the picture revealed. It drove `stick_lx` from `(t * 7) % 11 - 5`
// -- a fast sawtooth whose mean is very close to zero, so over 400 ticks each
// wizard ended within a tenth of a metre of where it started. Every check
// passed: the simulation advanced, hashes recorded, frames rendered, markers
// drawn. But at 6 px per metre the markers never left their pixel, and the
// canvas CRC alternated between exactly two values for every run length. A
// counter cannot see that; the rendered frame can.
//
// So the walk is now a slow TRIANGLE WAVE with a long period, on both axes and
// in opposite phase for the two pads. It traverses real ground, the two paths
// cross, and a bolt aimed at the other wizard has something to miss.
std::vector<zcon::InputSnapshot> demo_script(int n) {
  // A triangle wave in [-amp, +amp] with period `period` ticks. The
  // amplitudes below are sized against kMoveScale = 4 and the 32 m ground:
  // amp 28 over a 45-tick quarter sweeps about 10 m, so the wizards cross
  // real distance without spending the run pinned against the clamp -- the
  // first amplitudes tried (96) drove them into the walls and held them
  // there, which the frames showed and the position print confirmed.
  const auto tri = [](int t, int period, int amp) {
    const int p = ((t % period) + period) % period;
    const int half = period / 2;
    const int up = p < half ? p : period - p;      // 0..half
    return static_cast<int8_t>((up * 2 * amp) / half - amp);
  };

  std::vector<zcon::InputSnapshot> v;
  for (int t = 0; t < n; ++t) {
    zcon::InputSnapshot s;
    s.tick = static_cast<uint32_t>(t);
    s.pad[0].stick_lx = tri(t, 180, 28);
    s.pad[0].stick_ly = tri(t + 45, 240, 20);
    s.pad[0].buttons = static_cast<uint16_t>((t % 31 == 0) ? 1 : 0);
    s.pad[1].stick_lx = tri(t + 90, 180, 28);   // opposite phase
    s.pad[1].stick_ly = tri(t + 165, 240, 20);
    s.pad[1].buttons = static_cast<uint16_t>((t % 33 == 0) ? 1 : 0);
    v.push_back(s);
  }
  return v;
}

int usage() {
  std::printf(
      "zhao-desktop -- the desktop console host\n"
      "  --ticks N [--seed S] [--cart FILE] [--record FILE]   run a match\n"
      "  --replay FILE                                        verify a recording\n"
      "\n"
      "  --ppm FILE                                           write the canvas\n"
      "\n"
      "Video RENDERS through the reference software renderer (--ppm to look at\n"
      "it); audio and a window are not wired. See the header comment.\n");
  return 2;
}

int do_replay(const std::string& path) {
  const std::vector<uint8_t> bytes = read_file(path);
  if (bytes.empty()) {
    std::printf("replay: cannot read %s\n", path.c_str());
    return 1;
  }
  zcon::Recording rec;
  if (!zcon::deserialize(bytes, &rec)) {
    // Refusing a malformed recording matters: a half-parsed one would replay
    // the part it managed to read and present as a desync.
    std::printf("replay: %s is not a valid recording (magic/version/truncation)\n",
                path.c_str());
    return 1;
  }

  DesktopBackend be;
  zgame::Wizards truth;
  zcon::Session s(&truth, &be);
  s.start(rec.seed);
  const int diverged = s.replay_and_compare(rec.inputs, rec.hashes);
  if (diverged < 0) {
    std::printf("replay: %s OK -- %zu ticks, identical hash stream\n", path.c_str(),
                rec.inputs.size());
    return 0;
  }
  std::printf("replay: %s DIVERGED at hash index %d of %zu\n", path.c_str(),
              diverged, rec.hashes.size());
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  int ticks = 0;
  uint64_t seed = 0x5A5A5A5A;
  std::string cart, record_path, replay_path, ppm_path;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](std::string* dst) {
      if (i + 1 < argc) *dst = argv[++i];
    };
    if (a == "--ticks" && i + 1 < argc) {
      ticks = std::atoi(argv[++i]);
    } else if (a == "--seed" && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    } else if (a == "--cart") {
      next(&cart);
    } else if (a == "--record") {
      next(&record_path);
    } else if (a == "--replay") {
      next(&replay_path);
    } else if (a == "--ppm") {
      next(&ppm_path);
    } else {
      return usage();
    }
  }

  if (!replay_path.empty()) return do_replay(replay_path);
  if (ticks <= 0) return usage();

  DesktopBackend be;
  be.set_script(demo_script(ticks));
  std::vector<uint8_t> frame_bytes;

  if (!cart.empty()) {
    const std::vector<uint8_t> bytes = read_file(cart);
    if (bytes.empty()) {
      std::printf("cart: cannot read %s\n", cart.c_str());
      return 1;
    }
    const zcon::Handle h =
        be.publish(zcon::ResourceKind::kMeshStream, bytes.data(), bytes.size());
    std::printf("cart: %s -> handle{index=%u generation=%u} (%zu bytes)\n",
                cart.c_str(), h.index, h.generation, bytes.size());
  }

  // ---- resources ---------------------------------------------------------
  // A wizard is an 8x8 marker (FormPattern) at a world position
  // (FormTransform) -- the marker-law lineage `spec/commands.zidl` DrawForm
  // documents and `sprites.cpp` implements. The HOST builds these from game
  // state; the simulation never sees them, which is the boundary the software
  // lane exists to keep.
  //
  // THE TABLE IS KEYED BY THE WIRE HANDLE, not by a bare index. handle32 is
  // {index:24, generation:8}, so a form published at index 1 generation 1 is
  // 0x101 on the wire. Keying by the index alone missed every lookup and the
  // host reported 400 rendered frames over a blank canvas -- caught only
  // because the summary counts painted bytes.
  zcon::Handle form_h[2], xform_h[2], mat_h[2];
  for (int p = 0; p < 2; ++p) {
    zref::render::FormPattern pat;
    for (int i = 0; i < 64; ++i) {
      pat.rgb[i * 3 + 0] = static_cast<uint8_t>(p == 0 ? 230 : 60);
      pat.rgb[i * 3 + 1] = 60;
      pat.rgb[i * 3 + 2] = static_cast<uint8_t>(p == 0 ? 60 : 230);
      pat.mask[i] = 1;
    }
    form_h[p] = {static_cast<uint32_t>(p * 3 + 0), 1, zcon::ResourceKind::kMeshStream};
    xform_h[p] = {static_cast<uint32_t>(p * 3 + 1), 1, zcon::ResourceKind::kMeshStream};
    mat_h[p] = {static_cast<uint32_t>(p * 3 + 2), 1, zcon::ResourceKind::kMaterialSet};
    be.resources().forms.emplace_back(zcon::detail::handle32(form_h[p]), pat);
    be.resources().materials.emplace_back(zcon::detail::handle32(mat_h[p]),
                                          zref::render::Material{200, 200, 200});
  }

  zgame::Wizards truth;
  zcon::Session s(&truth, &be);
  s.start(seed);

  // The view. This is `ortho_topdown` from tests/render/render_helpers.hpp,
  // written out rather than reached for, because the test helper is a test
  // helper: screen x comes from world X and screen y from world Z, w = 1.
  // Scale 2048 maps world +-32 m across NDC, so the 32 m ground fills half the
  // 384-px view and both wizards stay well inside the wall clamp.
  // DUO: TWO VIEWS, ONE PER WIZARD. The roadmap's first playable slice names
  // "two wizards, two controllers, two views", and two views is the one of the
  // three the console's own video law has an opinion about: Duo stores its two
  // 256x192 canvases as CONTIGUOUS PACKED BLOCKS, not interleaved
  // (video_rules.md §3.1), and the mode LATCHES EFFECTIVE NEXT FRAME (§1.1).
  //
  // Both views use the same top-down projection here. They are not yet
  // per-wizard cameras -- that needs the view matrix to follow a wizard, which
  // is a gameplay-to-camera binding this host does not own yet. What IS proven
  // is that the frame carries two views, the renderer honours the viewport
  // mask, and the Duo canvas is the packed layout the scanout expects.
  zcon::ViewSpec view0, view1;
  const int32_t kScale = 2048;
  for (zcon::ViewSpec* v : {&view0, &view1}) {
    v->m[0] = kScale;    // ndc.x <- world.x
    v->m[6] = kScale;    // ndc.y <- world.z  (top-down)
    v->m[10] = 1 << 16;
    v->m[15] = 1 << 16;  // w = 1: orthographic, no divide
  }
  view0.view_id = 0;
  view0.viewport_id = 0;
  view1.view_id = 1;
  view1.viewport_id = 1;

  // Game units are fx8.8 on a 0..32 m ground (kOne = 256); world is fx16, and
  // the field is centred so 16 m sits at the view origin.
  const int32_t kHalfField = 16 * zgame::kOne;
  const auto to_world = [](int32_t game_fx8) { return game_fx8 << 8; };

  for (int t = 0; t < ticks; ++t) {
    // Transforms come from the CURRENT state, rebuilt every tick.
    be.resources().transforms.clear();
    for (int p = 0; p < 2; ++p) {
      zref::render::FormTransform tr;
      tr.x = to_world(truth.wizard(p).x - kHalfField);
      tr.y = 0;
      tr.z = to_world(truth.wizard(p).y - kHalfField);
      tr.size = 8 << 16;  // screen-space half-extent, with flags b1 below
      be.resources().transforms.emplace_back(zcon::detail::handle32(xform_h[p]), tr);
    }

    zcon::FramePlan plan;
    plan.frame_id = static_cast<uint32_t>(t + 1);
    plan.sequence = static_cast<uint32_t>(t + 1);
    plan.resource_epoch = 1;
    // The mode is set on the FIRST frame only. It latches for the next frame,
    // so re-asserting it every frame would be harmless and would also hide a
    // host that never chose one -- see the note on `has_mode`.
    plan.has_mode = (t == 0);
    plan.mode = 2;  // zhao_abi::VIDEO_DUO
    plan.views.push_back(view0);
    plan.views.push_back(view1);
    for (int p = 0; p < 2; ++p) {
      if (!truth.wizard(p).alive) continue;  // the dead are not drawn
      zcon::DrawItem d;
      d.form = form_h[p];
      d.material_set = mat_h[p];
      d.transform = xform_h[p];
      // BOTH views draw BOTH wizards. A mask of 0x1 for player 0 and 0x2 for
      // player 1 would look like split-screen and would in fact be each player
      // seeing only themselves -- the opponent would vanish, which is a
      // gameplay change smuggled in as a rendering choice.
      d.viewport_mask = 0x3;
      d.flags = 0x2;          // b1 = screen-space size (marker law)
      plan.draws.push_back(d);
    }
    frame_bytes = zcon::build_frame(plan);
    s.tick_with(frame_bytes);
  }

  std::printf(
      "host: %d ticks, seed 0x%llX, backend '%s'\n"
      "      wizards: p0 hp=%d deaths=%u at (%d,%d) | p1 hp=%d deaths=%u at "
      "(%d,%d)\n"
      "      frames submitted %llu, command bytes %llu, resources %zu\n"
      "      RENDERED: last status %u, commands executed %u, resource "
      "misses %u, canvas crc 0x%08X\n"
      "      final state hash 0x%016llX\n",
      ticks, static_cast<unsigned long long>(seed), be.name(),
      truth.wizard(0).health, truth.wizard(0).deaths, truth.wizard(0).x,
      truth.wizard(0).y, truth.wizard(1).health, truth.wizard(1).deaths,
      truth.wizard(1).x, truth.wizard(1).y, static_cast<unsigned long long>(be.frames()),
      static_cast<unsigned long long>(be.command_bytes()), be.resource_count(),
      be.last_status(), be.commands_executed(), be.resource_misses(),
      be.last_crc(), static_cast<unsigned long long>(truth.hash()));

  // ANTI-VACUITY. A host that reports 400 rendered frames while every canvas
  // is still the clear colour has proved nothing, and that exact failure has
  // already been caught twice in this tree -- once in the wizards replay that
  // killed nobody, once in a shell test that compared two blank framebuffers
  // and would have called them equal. So count the canvas bytes that are not
  // zero and say so. A zero here is a FINDING, not a formatting detail.
  {
    std::size_t painted = 0;
    for (uint8_t b : be.canvas().slot[0])
      if (b != 0) ++painted;
    std::printf("      canvas: %zu of %zu bytes non-zero%s\n", painted,
                be.canvas().slot[0].size(),
                painted ? "" : "  <-- NOTHING WAS DRAWN");
  }

  // Write the canvas out so it can be LOOKED AT. A byte count says something
  // was painted; it cannot say the markers are in the right places, the right
  // way up, or the right colours -- and this tree has a standing rule that
  // measurement never substitutes for looking. PPM because it needs no
  // library and every viewer opens it.
  if (!ppm_path.empty()) {
    const zhao_abi::video_mode m = be.latched_mode();
    const uint32_t w = zref::render::canvas_width(m);
    const uint32_t h = zref::render::canvas_height(m);
    const uint8_t* px = be.canvas().slot[0].data();
    std::vector<uint8_t> out;
    char hdr[64];
    const int n = std::snprintf(hdr, sizeof hdr, "P6\n%u %u\n255\n", w, h);
    out.insert(out.end(), hdr, hdr + n);
    for (uint32_t i = 0; i < w * h; ++i) {
      // RGB565 little-endian halfwords, expanded by bit replication so full
      // scale stays full scale (5-bit 31 -> 255, not 248).
      const uint16_t v =
          static_cast<uint16_t>(px[i * 2] | (px[i * 2 + 1] << 8));
      const uint8_t r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
      out.push_back(static_cast<uint8_t>((r5 << 3) | (r5 >> 2)));
      out.push_back(static_cast<uint8_t>((g6 << 2) | (g6 >> 4)));
      out.push_back(static_cast<uint8_t>((b5 << 3) | (b5 >> 2)));
    }
    std::printf("      ppm: %s (%ux%u) %s\n", ppm_path.c_str(), w, h,
                write_file(ppm_path, out) ? "written" : "FAILED");
  }
  std::fflush(stdout);
  // Flush before the long tail of work. Buffered output lost in a crash makes a
  // late fault look like an early one, which cost real time here: the record
  // path faulted AFTER this summary and presented as "no output at all".
  std::fflush(stdout);

  // Self-verify before writing: a recording that does not replay in the process
  // that produced it will certainly not replay anywhere else, and finding that
  // out at write time is free.
  const int diverged = s.replay_and_compare(s.inputs(), s.hashes());
  if (diverged >= 0) {
    std::printf("host: INTERNAL -- session does not replay, diverges at %d\n",
                diverged);
    return 1;
  }

  if (!record_path.empty()) {
    const zcon::Recording rec = zcon::record_of(s, be.name());
    const std::vector<uint8_t> blob = zcon::serialize(rec);
    if (!write_file(record_path, blob)) {
      std::printf("host: cannot write %s\n", record_path.c_str());
      return 1;
    }
    std::printf("host: recorded %zu ticks to %s\n", rec.inputs.size(),
                record_path.c_str());
  }
  return 0;
}
