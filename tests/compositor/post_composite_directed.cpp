// post_composite_directed.cpp -- does the compositor obey the FROZEN ORDER,
// and can this bench actually see the order it claims to check?
//
// ===========================================================================
// THIS IS NOT A DIFFERENTIAL TEST. SAYING SO IS THE FIRST CHECK.
// ===========================================================================
// `design/blocks.yml` declares POST.COMPOSITE's reference_model as
// `zref::PostComposite`, and that symbol has never been written --
// `tools/budget/refmodel_liveness.py` lists it as one of eleven PHANTOM
// citations and states the consequence itself: "These blocks have NO
// differential test available. A directed test for one of them checks
// self-consistency against contract prose, not agreement with a ratified
// model."
//
// So: every "matches the expectation" check below compares the RTL against
// `pc::model_pixel`, which is the contract's prose transcribed into C++ by the
// same hand that wrote the RTL. That pair can be wrong together. It is
// SELF-CONSISTENCY plus HAND-COMPUTED EXPECTATION, and nothing here should be
// quoted as agreement with a ratified oracle.
//
// What carries real weight is the STRUCTURAL half, which does not depend on
// the model being right:
//
//   * every mandatory stage is shown to do positive work, AND muting it is
//     shown to change the picture (completion plan 11.5);
//   * the bench is shown able to distinguish grade-after-bloom from
//     grade-before-bloom BEFORE it is used to assert which one ships;
//   * world, glow and ink are shown to move by the SAME displacement;
//   * a displacement off the edge is shown to clamp and specifically NOT to
//     wrap to the far side;
//   * a full-intensity flash is shown not to wash out the ink line;
//   * backpressure is shown to cost cycles and never pixels.
//
// `ring_hazard_o` is asserted ZERO here and that is a claim, not a result. Its
// positive control is tests/mutants/zhao_post_composite_ring_hazard_mutant.sv,
// whose driver passes only when the counter FIRES.
// ===========================================================================
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_composite.h"

#include "post_composite_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

using pc::CH;
using pc::CW;
using pc::H;
using pc::W;

namespace {

template <typename Top>
pc::Result run(Top* t, const pc::Frame& f, const pc::Cfg& c, uint32_t seed = 0) {
  pc::reset_dut(t);
  pc::load_pv_table(t, c);
  return pc::run_frame(t, f, c, seed);
}

// A configuration in which EVERY mandatory stage does something, so that muting
// any one of them has somewhere to show up.
pc::Cfg rich_cfg() {
  pc::Cfg c;
  c.atm_en = true; c.atm_valid = true; c.atm_add = false;
  c.atm_rgb = pc::pack565(40, 90, 200); c.atm_opacity = 110;
  c.bloom_gain = 200;
  c.grade_valid = true;
  pc::set_halving_grade(&c);
  c.flash_rgb = 0xFFFF; c.flash_amt = 70;
  c.ink_rgb = 0x001F;
  for (int y = 4; y < 9; ++y)
    for (int x = 10; x < 20; ++x) c.hud[(y * W) + x] = 1;
  c.hud_rgb = 0x07E0;
  return c;
}

pc::Frame rich_frame() {
  pc::Frame f = pc::base_frame();
  for (int cy = 0; cy < CH; ++cy)
    for (int cx = 0; cx < CW; ++cx) {
      const int i = (cy * CW) + cx;
      f.dx[i] = static_cast<int8_t>(((cx + cy) % 5) - 2);
      f.dy[i] = static_cast<int8_t>((cy % 3) - 1);
      if (((cx + cy) & 3) == 0) f.glow[i] = pc::pack565(200, 120, 60);
      if (cx == 7 && cy == 3) f.ink[i] = 1;
    }
  // two cells that ask for more than the frame can serve, so the edge clamp has
  // something legitimate to count
  f.dx[0] = -8;  f.dy[0] = -4;
  f.dx[CW - 1] = 8;
  f.dy[((CH - 1) * CW) + CW - 1] = 4;
  return f;
}

int frames_differ(const pc::Result& a, const pc::Result& b) {
  int n = 0;
  for (int i = 0; i < pc::NPIX; ++i)
    if (a.rgb[i] != b.rgb[i]) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_post_composite;

  // =======================================================================
  // 1 -- THE BASELINE PASS: every pixel exactly once, and the work census
  // =======================================================================
  {
    const pc::Frame f = pc::base_frame();
    pc::Cfg c;                       // every effect off, no grading
    const pc::Result r = run(top, f, c);

    int unseen = 0;
    for (int i = 0; i < pc::NPIX; ++i) if (!r.seen[i]) ++unseen;
    zhao::check(unseen == 0,
                "a pass emits every pixel of the frame -- a compositor that "
                "drops the tail of the drain looks perfect on every pixel it "
                "did emit",
                0, unseen);
    zhao::check(r.duplicates == 0,
                "and emits each exactly once -- a test that checks WHAT came "
                "out cannot see HOW MANY TIMES the machine did it",
                0, r.duplicates);
    zhao::check(r.last_pulses == 1, "exactly one o_last_o in a pass", 1, r.last_pulses);
    zhao::check(r.passes == 1u, "passes_completed_o moves by exactly one", 1,
                static_cast<int>(r.passes));

    // With no effects at all the output is the source, bit for bit: the
    // RGB565 -> 8 bit -> RGB565 round trip must invent nothing.
    int bad = 0;
    for (int i = 0; i < pc::NPIX; ++i) if (r.rgb[i] != f.src[i]) ++bad;
    zhao::check(bad == 0,
                "with every effect muted the frame passes through UNCHANGED -- "
                "the unpack/repack round trip cannot invent a colour that was "
                "not in the frame",
                0, bad);

    // The completion plan's counting law: these are three separate axes and a
    // single work-item total would hide that a pixel costs two plane reads.
    zhao::check(r.line_fill == static_cast<uint32_t>(pc::NPIX),
                "line_fill_writes_o is ONE source fetch per pixel -- the "
                "ruling's 'one source framebuffer pixel is fetched into the "
                "line system once'",
                pc::NPIX, static_cast<int>(r.line_fill));
    zhao::check(r.out_writes == static_cast<uint32_t>(pc::NPIX),
                "output_writes_o counts accepted beats, one per pixel",
                pc::NPIX, static_cast<int>(r.out_writes));
    zhao::check(r.plane_reads == static_cast<uint32_t>(2 * pc::NPIX),
                "plane_reads_o is TWO per pixel, counted separately from the "
                "line fill: displacement at the undisplaced pixel, glow+ink at "
                "the displaced one. A single work-item total would say one",
                2 * pc::NPIX, static_cast<int>(r.plane_reads));
    zhao::check(r.hazard == 0u,
                "ring_hazard_o is zero -- a CLAIM, whose positive control is "
                "tests/mutants/zhao_post_composite_ring_hazard_mutant.sv",
                0, static_cast<int>(r.hazard));
  }

  // =======================================================================
  // 2 -- STAGE LIVENESS: every mandatory stage does positive work, and
  //      muting it changes the result (completion plan 11.5)
  // =======================================================================
  {
    const pc::Frame f = rich_frame();
    const pc::Cfg   base = rich_cfg();
    const pc::Result all = run(top, f, base);

    zhao::check(pc::diff_count(all, f, base) == 0,
                "the fully-loaded frame matches the hand-computed expectation "
                "(SELF-CONSISTENCY, not a differential -- zref::PostComposite "
                "does not exist)",
                0, pc::diff_count(all, f, base));

    struct Mute { const char* name; pc::Cfg cfg; pc::Frame frm; };
    std::vector<Mute> mutes;

    { pc::Frame g = f; for (auto& v : g.dx) v = 0; for (auto& v : g.dy) v = 0;
      mutes.push_back({"displacement", base, g}); }
    { pc::Cfg g = base; g.atm_en = false;      mutes.push_back({"atmosphere", g, f}); }
    { pc::Cfg g = base; g.bloom_gain = 0;      mutes.push_back({"bloom", g, f}); }
    { pc::Cfg g = base; g.grade_valid = false; mutes.push_back({"curves+matrix", g, f}); }
    { pc::Cfg g = base; g.flash_amt = 0;       mutes.push_back({"flash", g, f}); }
    { pc::Frame g = f; for (auto& v : g.ink) v = 0;
      mutes.push_back({"exterior ink", base, g}); }
    { pc::Cfg g = base; for (auto& v : g.hud) v = 0; mutes.push_back({"HUD", g, f}); }
    { pc::Cfg g = base; g.gg_present = false;  mutes.push_back({"glow/ink plane", g, f}); }

    for (const auto& m : mutes) {
      const pc::Result r = run(top, m.frm, m.cfg);
      const int d = frames_differ(all, r);
      zhao::check(d > 0,
                  "muting a mandatory stage CHANGES the picture -- a stage that "
                  "can be switched off with no visible effect is a stage that "
                  "was never running",
                  1, d > 0 ? 1 : 0);
      if (d == 0) std::printf("    ^^ the stage that did nothing was: %s\n", m.name);
    }

    zhao::check(all.bloom_cells > 0u,
                "bloom_cells_contributing_o moves when glow cells actually "
                "contribute",
                1, all.bloom_cells > 0u ? 1 : 0);
    zhao::check(all.edge_clamps > 0u,
                "displacement_edge_clamps_o moves when a cell asks to sample "
                "off the frame",
                1, all.edge_clamps > 0u ? 1 : 0);
    zhao::check(all.hazard == 0u, "and no ring hazard on a fully-loaded frame",
                0, static_cast<int>(all.hazard));
  }

  // =======================================================================
  // 3 -- THE ORDER IS ASSERTED, AND THE BENCH IS SHOWN ABLE TO SEE IT
  // =======================================================================
  // The contract: "a frame with both grading and bloom, compared against a
  // reference that applies them in the opposite order, must DIFFER -- proving
  // the test can actually see the ordering it claims to check."
  {
    pc::Frame f = pc::base_frame();
    for (int i = 0; i < CW * CH; ++i) f.glow[i] = pc::pack565(180, 180, 180);
    pc::Cfg c;
    c.bloom_gain = 255;
    c.grade_valid = true;
    pc::set_halving_grade(&c);
    const pc::Result r = run(top, f, c);

    const int d_ship  = pc::diff_count(r, f, c, false);   // bloom then grade
    const int d_swap  = pc::diff_count(r, f, c, true);    // grade then bloom

    zhao::check(d_swap > 0,
                "the OPPOSITE order produces a different picture -- which is "
                "what proves this check can see the ordering at all. Without "
                "this the next assertion would pass for a block that ignored "
                "order entirely",
                1, d_swap > 0 ? 1 : 0);
    zhao::check(d_ship == 0,
                "and the RTL applies BLOOM THEN GRADE, the frozen visible "
                "order -- grading before bloom and grading after bloom are "
                "different pictures",
                0, d_ship);
  }

  // =======================================================================
  // 4 -- COHERENT SAMPLING: world, glow and ink move TOGETHER
  // =======================================================================
  // "An outline that stayed put while the creature bent would read as a
  // printing error." This is the case that would look like one.
  {
    const int SX = 5, SY = 2;
    pc::Frame f0 = pc::base_frame();          // no displacement
    pc::Frame fd = pc::base_frame();          // uniform displacement
    for (int i = 0; i < CW * CH; ++i) { fd.dx[i] = SX; fd.dy[i] = SY; }
    // a glowing, outlined creature in two adjacent cells
    for (pc::Frame* p : {&f0, &fd}) {
      p->ink[(3 * CW) + 6]  = 1;
      p->glow[(3 * CW) + 7] = pc::pack565(255, 255, 255);
    }
    pc::Cfg c;
    c.bloom_gain = 255;
    c.ink_rgb = 0x001F;

    const pc::Result r0 = run(top, f0, c);
    const pc::Result rd = run(top, fd, c);

    zhao::check(pc::diff_count(rd, fd, c) == 0,
                "the refracted frame matches the hand-computed expectation",
                0, pc::diff_count(rd, fd, c));

    // The structural half, which owes nothing to the model: every one of the
    // three displaced quantities must be the undisplaced frame read at
    // (x+SX, y+SY). Checking them TOGETHER is the point -- each one alone
    // would pass for an implementation that used a different coordinate for
    // the outline.
    int world_bad = 0, ink_bad = 0, glow_bad = 0;
    for (int y = 0; y + SY < H; ++y)
      for (int x = 0; x + SX < W; ++x) {
        const uint16_t got  = rd.rgb[(y * W) + x];
        const uint16_t want = r0.rgb[((y + SY) * W) + (x + SX)];
        const bool got_ink  = (got == c.ink_rgb);
        const bool want_ink = (want == c.ink_rgb);
        if (got_ink != want_ink) ++ink_bad;
        else if (!got_ink && got != want) {
          // a brightened (bloomed) pixel and a plain one are both covered by
          // the same equality; split them only for the diagnostic
          if (want == 0xFFFF || got == 0xFFFF) ++glow_bad; else ++world_bad;
        }
      }
    zhao::check(ink_bad == 0,
                "the exterior INK lands exactly where the displaced world came "
                "from -- the outline follows the creature, which is the case "
                "that would look like a printing error if it broke",
                0, ink_bad);
    zhao::check(glow_bad == 0,
                "and the GLOW does too, through the same displaced coordinate",
                0, glow_bad);
    zhao::check(world_bad == 0,
                "and so does the world colour -- one displacement field, "
                "sampled once, for all three",
                0, world_bad);
  }

  // =======================================================================
  // 5 -- A FULL FLASH DOES NOT WASH OUT THE INK
  // =======================================================================
  {
    pc::Frame f = pc::base_frame();
    for (int cx = 0; cx < CW; ++cx) f.ink[(2 * CW) + cx] = 1;
    pc::Cfg c;
    c.flash_rgb = 0xFFFF;
    c.flash_amt = 255;
    c.ink_rgb   = 0x001F;
    const pc::Result r = run(top, f, c);

    int ink_lost = 0, flash_weak = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        const bool inked = (f.ink[((y >> 2) * CW) + (x >> 2)] != 0);
        const uint16_t px = r.rgb[(y * W) + x];
        if (inked && px != c.ink_rgb) ++ink_lost;
        if (!inked && px == f.src[(y * W) + x]) ++flash_weak;
      }
    zhao::check(ink_lost == 0,
                "a FULL-INTENSITY flash frame still shows the line exactly -- "
                "ink is applied after the flash, per the ruling, and changing "
                "that is an explicit artistic mode rather than an accident of "
                "stage order",
                0, ink_lost);
    zhao::check(flash_weak == 0,
                "and the flash really was full intensity: no un-inked pixel "
                "came through unchanged. Without this the check above would "
                "pass for a flash that did nothing",
                0, flash_weak);
  }

  // =======================================================================
  // 6 -- OFF THE EDGE: CLAMP, NEVER WRAP
  // =======================================================================
  // "Wrapping would sample the opposite side of the screen, which is a
  // spectacular and very confusing artefact."
  {
    pc::Frame f = pc::base_frame();
    for (int i = 0; i < CW * CH; ++i) { f.dx[i] = -8; f.dy[i] = -4; }
    pc::Cfg c;
    const pc::Result r = run(top, f, c);

    int not_clamped = 0, wrapped = 0;
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 8; ++x) {
        const uint16_t got = r.rgb[(y * W) + x];
        if (got != f.src[0]) ++not_clamped;                    // clamps to (0,0)
        if (got == f.src[((H + y - 4) * W) + (W + x - 8)]) ++wrapped;
      }
    zhao::check(not_clamped == 0,
                "a displacement off the top-left clamps to the edge texel",
                0, not_clamped);
    zhao::check(wrapped == 0,
                "and specifically does NOT wrap to the opposite side of the "
                "screen",
                0, wrapped);
    zhao::check(r.edge_clamps > 0u, "and every clamp is counted",
                1, r.edge_clamps > 0u ? 1 : 0);

    // The clamp is applied ONCE, to the already-combined field: the count is
    // exactly the number of pixels whose sample left the frame, not three
    // times that for the three contributions POST.GATHER summed.
    int expect = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if ((x - 8) < 0 || (y - 4) < 0) ++expect;
    zhao::check(r.edge_clamps == static_cast<uint32_t>(expect),
                "and it is counted ONCE per pixel, not once per contribution -- "
                "the three effects combined BEFORE sampling and are clamped "
                "once, which is what stops them tearing the image",
                expect, static_cast<int>(r.edge_clamps));
  }

  // =======================================================================
  // 7 -- DUO: no bleed between views, STRUCTURALLY
  // =======================================================================
  // The Duo plane is 128 x 48 = 6,144 cells addressed as TWO 64 x 48 views --
  // a quarter of the RENDERED area, not of the 512 x 240 displayed canvas whose
  // extra 48 rows are black border. This block composites ONE VIEW PER PASS, so
  // every x it forms is view-local and the ordinary clamp IS the view clamp.
  //
  // The point of that is not the 1,536 cells it saves -- the owner has ruled
  // memory affordable and saving cells is not worth selling. The point is that
  // "a refraction cannot reach the other player's screen" stops being a
  // comparator that has to be right. So the check below is about the ADDRESS
  // SPACE, not about a pixel: no address the block ever forms names the other
  // view, because the other view is not in the range.
  {
    pc::Frame f = pc::base_frame();
    for (int i = 0; i < CW * CH; ++i) { f.dx[i] = 8; f.dy[i] = 4; }
    pc::Cfg c;
    c.view_sel = true;              // composite the RIGHT view this pass
    const pc::Result r = run(top, f, c);

    zhao::check(r.view_bad == 0,
                "every plane address this pass forms names THIS view -- the "
                "bank is a pass property and is never computed per sample, "
                "which is what makes the no-bleed property structural",
                0, r.view_bad);
    zhao::check(r.max_gd_cx < CW && r.max_gg_cx < CW,
                "and no cell index reaches the other view's half: with one "
                "128-wide plane these could have run to 2*CW-1, and here that "
                "address does not exist",
                1, (r.max_gd_cx < CW && r.max_gg_cx < CW) ? 1 : 0);
    zhao::check(r.max_gd_cy < CH && r.max_gg_cy < CH,
                "nor past the view's last quarter-row -- the rendered area is "
                "what is addressed, not the canvas whose extra rows are black "
                "border. This check FAILED on first run: the front pointer "
                "walks one row past the last line during the drain, and the "
                "port was presenting a cell that does not exist",
                1, (r.max_gd_cy < CH && r.max_gg_cy < CH) ? 1 : 0);
    zhao::check(r.parked_bad == 0,
                "and an INVALID request parks its address at zero rather than "
                "leaving the last one on the port -- which is what makes the "
                "range claim above true for every cycle and not merely for the "
                "cycles anyone looked at",
                0, r.parked_bad);

    int crossed = 0;
    for (int y = 0; y < H; ++y)
      for (int x = W - 8; x < W; ++x)
        if (r.rgb[(y * W) + x] != f.src[(((y + 4 > H - 1) ? H - 1 : y + 4) * W) + (W - 1)])
          ++crossed;
    zhao::check(crossed == 0,
                "and a sample displaced past the view's right edge clamps to "
                "the view's own last column, never to a neighbour's first",
                0, crossed);
    zhao::check(pc::diff_count(r, f, c) == 0,
                "and the whole view matches the hand-computed expectation",
                0, pc::diff_count(r, f, c));
  }

  // =======================================================================
  // 7b -- THE EXACT PRODUCT-VECTOR TABLE EQUALS THE NINE-MULTIPLIER PATH
  // =======================================================================
  // Owner plan 11.2 asks for "actual generated-table equivalence", and the
  // ruling of 2026-09-18 raised the M10K ceiling so the table could ship. This
  // is that equivalence, and it is TOTAL rather than sampled: the grading
  // stage's whole input space is (r5, g6, b5) = 32 x 64 x 32 = 65,536
  // combinations, and every one is checked against the nine-multiplier
  // reference, for several matrices.
  //
  // The algebra is trivially an identity -- that is not what this catches. What
  // it catches is a PACKING, SIGN-EXTENSION or TRANSPOSE error in the
  // GENERATOR: indexing the matrix `m[col*3 + o]` instead of `m[o*3 + col]`
  // produces an entirely plausible picture with the channels cross-mixed. Fired
  // deliberately: that one mutation gives 187,564 mismatches of 786,432.
  //
  // WHAT IT CANNOT SEE, found by fire-testing it and worth writing down rather
  // than quietly fixing: swapping the first two arguments of
  // `grade_channel_table` changes NOTHING, because the three lanes are summed
  // and addition commutes. The first fire test injected exactly that and the
  // check stayed green -- which is not a fault in the check, because it is not
  // a fault at all. The lesson is that "it catches a transpose" had to be
  // narrowed to "it catches a transpose IN THE GENERATOR", and the difference
  // was only visible because the mutation was actually run.
  {
    struct Set { const char* name; int16_t m[9]; int16_t bias[3]; };
    const Set sets[] = {
      {"identity",      {16384, 0, 0, 0, 16384, 0, 0, 0, 16384},          {0, 0, 0}},
      {"warm rotate",   {17000, -2200, 800, 1500, 15000, -900, -600, 2400, 16900},
                                                                          {4, -3, 7}},
      {"heavy negative",{-32768, 32767, -16384, 32767, -32768, 16384, -16384, 16384, -32768},
                                                                          {-255, 255, 0}},
      {"extreme bias",  {16384, 0, 0, 0, 16384, 0, 0, 0, 16384},          {255, -256, 128}},
    };

    int total = 0, bad = 0, saturated_low = 0, saturated_high = 0;
    for (const auto& s : sets) {
      // a curve that is not the identity, so a transpose cannot hide
      uint8_t cr[32], cg[64], cb[32];
      for (int i = 0; i < 32; ++i) cr[i] = static_cast<uint8_t>((i * 7) & 0xFF);
      for (int i = 0; i < 64; ++i) cg[i] = static_cast<uint8_t>((i * 3) + 11);
      for (int i = 0; i < 32; ++i) cb[i] = static_cast<uint8_t>(255 - (i * 8));

      for (int i = 0; i < 32; ++i) {
        int32_t pr[3];
        zref::post::grade_product_vector(s.m, 0, cr[i], pr);
        for (int j = 0; j < 64; ++j) {
          int32_t pg[3];
          zref::post::grade_product_vector(s.m, 1, cg[j], pg);
          for (int k = 0; k < 32; ++k) {
            int32_t pb[3];
            zref::post::grade_product_vector(s.m, 2, cb[k], pb);
            for (int row = 0; row < 3; ++row) {
              const uint8_t want =
                  zref::post::grade_channel_mul(s.m, row, cr[i], cg[j], cb[k], s.bias[row]);
              const uint8_t got =
                  zref::post::grade_channel_table(pr, pg, pb, row, s.bias[row]);
              if (want != got) ++bad;
              if (want == 0) ++saturated_low;
              if (want == 255) ++saturated_high;
              ++total;
            }
          }
        }
      }
      // the width claim, checked rather than asserted in prose: every product
      // this set can produce must fit the signed 24-bit field the RTL declares
      for (int col = 0; col < 3; ++col)
        for (int v = 0; v <= 255; ++v) {
          int32_t p[3];
          zref::post::grade_product_vector(s.m, col, static_cast<uint8_t>(v), p);
          for (int o = 0; o < 3; ++o)
            if (p[o] < -8388608 || p[o] > 8388607) ++bad;
        }
    }

    zhao::check(bad == 0,
                "the generated product-vector table equals the nine-multiplier "
                "path EXACTLY, over the grading stage's entire input space at "
                "four matrices -- total, not sampled, because a sign-extension "
                "fault only bites on negative products",
                0, bad);
    zhao::check(total == 4 * 32 * 64 * 32 * 3,
                "and the whole space really was walked",
                4 * 32 * 64 * 32 * 3, total);
    zhao::check(saturated_low > 0 && saturated_high > 0,
                "and the space reached BOTH saturation rails, so the check is "
                "not passing merely because nothing interesting happened",
                1, (saturated_low > 0 && saturated_high > 0) ? 1 : 0);
  }

  // =======================================================================
  // 8 -- THE GRADING PATH IS EXACT ON AN IDENTITY TABLE, IN THE RTL
  // =======================================================================
  // 7b proved the table equals the multiply path in the MODEL. This proves the
  // RTL's three 72-bit reads, sign extensions, sums and finalize do not drift:
  // an identity curve with a 1.0 matrix and zero bias must leave the frame
  // bit-identical, through the whole loaded-table path.
  {
    const pc::Frame f = pc::base_frame();
    pc::Cfg off;
    pc::Cfg on;
    on.grade_valid = true;
    pc::set_identity_grade(&on);

    const pc::Result r_off = run(top, f, off);
    const pc::Result r_on  = run(top, f, on);

    zhao::check(frames_differ(r_off, r_on) == 0,
                "an identity curve with a 1.0 matrix and zero bias is EXACTLY "
                "a no-op through round-half-up and saturate -- the arithmetic "
                "does not drift on the way through",
                0, frames_differ(r_off, r_on));
    zhao::check(r_on.grade_missing == 0u,
                "and grading_table_missing_o stays at zero while a table is "
                "present",
                0, static_cast<int>(r_on.grade_missing));
    zhao::check(r_off.grade_missing == 1u,
                "while a pass composited with no grading table counts exactly "
                "one -- pass through ungraded, because a wrong grade is worse "
                "than no grade",
                1, static_cast<int>(r_off.grade_missing));
  }

  // =======================================================================
  // 9 -- AN ABSENT PLANE IS TREATED AS ZERO AND COUNTED
  // =======================================================================
  {
    pc::Frame f = rich_frame();
    pc::Cfg c;
    c.gd_present = false;
    c.gg_present = false;
    c.atm_en = true;
    c.atm_valid = false;          // enabled but never delivered
    const pc::Result r = run(top, f, c);

    zhao::check(pc::diff_count(r, f, c) == 0,
                "with all three planes absent the frame still composites, "
                "treating each as zero",
                0, pc::diff_count(r, f, c));
    zhao::check(r.plane_missing == static_cast<uint32_t>(3 * pc::NPIX),
                "and each absence is counted on its own axis -- three planes "
                "times every pixel. One nonblocking increment per cycle would "
                "have reported a third of this",
                3 * pc::NPIX, static_cast<int>(r.plane_missing));
    zhao::check(r.edge_clamps == 0u,
                "an absent displacement plane displaces nothing, so nothing "
                "clamps",
                0, static_cast<int>(r.edge_clamps));
  }

  // =======================================================================
  // 10 -- BACKPRESSURE COSTS CYCLES, NEVER PIXELS
  // =======================================================================
  {
    const pc::Frame f = rich_frame();
    const pc::Cfg   c = rich_cfg();
    const pc::Result clean   = run(top, f, c, 0);
    const pc::Result stalled = run(top, f, c, 0xC0FFEEu);

    zhao::check(frames_differ(clean, stalled) == 0,
                "a stalling consumer changes nothing about the picture -- and "
                "the line ring's write is held with it, because the two "
                "pointers must stay the designed distance apart",
                0, frames_differ(clean, stalled));
    zhao::check(stalled.hazard == 0u,
                "and a stall never lets a write reach a line that is still "
                "sampleable",
                0, static_cast<int>(stalled.hazard));
    zhao::check(stalled.out_writes == static_cast<uint32_t>(pc::NPIX),
                "and every pixel is still written exactly once",
                pc::NPIX, static_cast<int>(stalled.out_writes));
  }

  // =======================================================================
  // 11 -- THE POST.ECHO SEAM IS OPEN AND IS POST-INK, PRE-HUD
  // =======================================================================
  // POST.ECHO is DEFERRED and nothing of it is implemented. What its contract
  // asks of this block is that the post-ink/pre-HUD image stay AVAILABLE. This
  // checks the tap is that image and not the final one.
  {
    pc::Frame f = pc::base_frame();
    for (int cx = 0; cx < CW; ++cx) f.ink[(1 * CW) + cx] = 1;
    pc::Cfg c;
    c.ink_rgb = 0x001F;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < 8; ++x) c.hud[(y * W) + x] = 1;
    c.hud_rgb = 0x07E0;
    const pc::Result r = run(top, f, c);

    int tap_bad = 0, hud_bad = 0, indistinguishable = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        const int i = (y * W) + x;
        if (r.echo[i] != pc::model_echo(f, c, x, y)) ++tap_bad;
        if (r.rgb[i]  != pc::model_pixel(f, c, x, y)) ++hud_bad;
        if (x < 8 && r.echo[i] == r.rgb[i]) ++indistinguishable;
      }
    zhao::check(tap_bad == 0,
                "the echo tap carries the post-ink, PRE-HUD image -- the seam "
                "POST.ECHO's contract asks be kept, as a tap point and not a "
                "buffer",
                0, tap_bad);
    zhao::check(hud_bad == 0, "and the main output carries the HUD on top of it",
                0, hud_bad);
    zhao::check(indistinguishable == 0,
                "and under the HUD the two differ everywhere, so the tap "
                "cannot be the final image wearing a second name",
                0, indistinguishable);
  }

  std::printf(
      "  counters on the last pass: edge clamps, bloom cells, passes, "
      "grade missing, plane missing, line fill, out writes, plane reads, "
      "ring hazards\n");

  const int rc = zhao::report_and_exit("post_composite_directed");
  delete top;
  zhao::exit_hard(rc);
}
