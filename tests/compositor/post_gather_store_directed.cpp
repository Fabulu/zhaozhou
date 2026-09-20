// post_gather_store_directed.cpp -- does the plane put a tile's cells at the
// right SCREEN address, and do its two tripwires actually fire?
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS FOR
// ---------------------------------------------------------------------------
// POST.GATHER flushes sixteen cells per tile as a STREAM, indexed 0..15
// "within the tile". POST.COMPOSITE reads a plane by {view, cx, cy} --
// absolute, per view. Packet POSTMEAS found that neither the RTL nor the
// contract named the mechanism between them; this block is the mechanism, and
// this bench is what says it is right.
//
// ---------------------------------------------------------------------------
// THE CASE THAT MATTERS MOST IS CASE 3, AND IT IS NOT AN ADDRESS CASE
// ---------------------------------------------------------------------------
// POST.GATHER ping-pongs: when a tile's cells arrive here, that tile is
// already CLOSED and the next one may be streaming. Writing them at the
// CURRENT origin is `CLAUDE.md`'s metadata-swap defect exactly -- the right
// cells at the wrong address, one tile late, with every counter balancing
// because no counter looks at the field that moved. On screen it is every
// halo displaced by one tile, and on a frame with one bright tile it is
// nothing at all.
//
// So case 3 streams tile A, closes it, STARTS STREAMING TILE B, and only then
// flushes A's cells -- and requires them at A's address. An implementation
// with one origin register passes every other case in this file and fails
// that one.
//
// ---------------------------------------------------------------------------
// BOTH TRIPWIRES ARE FIRED HERE, DELIBERATELY
// ---------------------------------------------------------------------------
// `tests/prod/tb_zhao_console_core_smoke.sv` quotes `flush_overrun_o` and
// `rdw_collide_o` at ZERO in the composed console and $fatals if they are not.
// CLAUDE.md is explicit that this is only worth anything if the instruments
// have been seen to work: "a detector reading zero is a claim, and it is the
// claim to check hardest. Fire it deliberately on a fault it SHOULD catch
// before quoting its silence." Cases 6 and 7 are that firing.
//
// Both are reachable with LEGAL stimulus at this block's own ports -- a tile
// closing while a flush drains, and a read and a write naming one cell on one
// clock -- so no committed mutant is owed for either. `oob_writes_o` (case 5)
// and the `org_ok_i` rejection (case 8) are the same: reachable here, and
// structurally unreachable in the composed console, which is why the console
// asserts them at zero and this file makes them move.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_gather_store.h"

#include "zhao_sim.hpp"

namespace {

// The three modes, in CELLS. Duo STACKS -- zhao_post_lease reads one tall
// source of `frame_h << 1` per frame, so the canvas is 64 x 96 with view 1 in
// the lower half, not two views side by side.
struct Geom {
  const char* name;
  int w_cells;     // cells per canvas row
  int rows;        // cell rows in the whole canvas
  int view_rows;   // cell rows in ONE view
};
const Geom kZ60{"Z60", 96, 60, 60};
const Geom kStorm{"Storm", 80, 60, 60};
const Geom kDuo{"Duo", 64, 96, 48};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_gather_store top;

  auto idle = [&]() {
    top.org_valid_i = 0;
    top.org_close_i = 0;
    top.org_ok_i = 1;
    top.w_busy_i = 0;
    top.w_valid_i = 0;
    top.plane_commit_i = 0;
    top.gd_req_v_i = 0;
    top.gg_req_v_i = 0;
  };

  auto geom = [&](const Geom& g) {
    top.plane_w_cells_i = g.w_cells;
    top.plane_rows_i = g.rows;
    top.view_rows_i = g.view_rows;
  };

  // The two tripwires are fired in cases 6 and 7 and then RESET by case 8's
  // fresh start, so the summary at the bottom would print them as zero and
  // read exactly like a bench that never fired them. They are captured at the
  // moment they move instead. Evidence that has to be inferred from a bench's
  // structure is evidence nobody reads.
  uint32_t fired_overrun = 0, fired_rdw = 0, fired_oob = 0;

  auto reset = [&](const Geom& g) {
    idle();
    geom(g);
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // Tell the store a tile is streaming at this origin (in cells).
  auto stream_tile = [&](int cx, int cy, bool ok = true) {
    top.org_valid_i = 1;
    top.org_cx_i = cx;
    top.org_cy_i = cy;
    top.org_ok_i = ok ? 1 : 0;
    zhao::tick(top);
    top.org_valid_i = 0;
  };

  auto close_tile = [&]() {
    top.org_close_i = 1;
    zhao::tick(top);
    top.org_close_i = 0;
  };

  // One flushed cell. `index` is POST.GATHER's own {cy[1:0], cx[1:0]}.
  auto write_cell = [&](int index, uint16_t glow, int dx, int dy, bool ink) {
    top.w_valid_i = 1;
    top.w_index_i = index;
    top.w_glow_i = glow;
    top.w_dx_i = dx;
    top.w_dy_i = dy;
    top.w_ink_i = ink ? 1 : 0;
    zhao::tick(top);
    top.w_valid_i = 0;
  };

  auto commit = [&]() {
    top.plane_commit_i = 1;
    zhao::tick(top);
    top.plane_commit_i = 0;
  };

  // Address out in cycle N, data in cycle N+1 -- the compositor's contract.
  struct Answer {
    bool present;
    uint16_t glow;
    int dx, dy;
    bool ink;
  };
  auto read_cell = [&](bool view, int cx, int cy) {
    top.gd_req_v_i = 1;
    top.gd_view_i = view ? 1 : 0;
    top.gd_cx_i = cx;
    top.gd_cy_i = cy;
    top.gg_req_v_i = 1;
    top.gg_view_i = view ? 1 : 0;
    top.gg_cx_i = cx;
    top.gg_cy_i = cy;
    zhao::tick(top);
    top.gd_req_v_i = 0;
    top.gg_req_v_i = 0;
    Answer a;
    a.present = (top.gd_present_o != 0);
    a.glow = top.gg_glow_o;
    a.dx = static_cast<int8_t>(top.gd_dx_o);
    a.dy = static_cast<int8_t>(top.gd_dy_o);
    a.ink = (top.gg_ink_o != 0);
    return a;
  };

  // ======================================================================
  // 1. AN EMPTY PLANE ANSWERS NOT PRESENT, NOT ZERO
  // ======================================================================
  // W10: an absent output must not look like a zero result. The compositor
  // COUNTS the difference (`post_plane_missing_o`), so answering zero on a
  // plane that has never been gathered would make the first frame's absent
  // bloom indistinguishable from a frame with no lights in it.
  {
    reset(kZ60);
    const Answer a = read_cell(false, 10, 10);
    zhao::check(!a.present,
                "a plane that has never been gathered answers NOT PRESENT -- "
                "it does not answer zero, which W10 says is a different thing",
                0, a.present ? 1 : 0);
    zhao::check(top.gd_miss_o == 1 && top.gg_miss_o == 1,
                "and the miss is COUNTED on both clients", 1, top.gd_miss_o);
  }

  // ======================================================================
  // 2. ONE TILE, ALL SIXTEEN CELLS, AT THE RIGHT SCREEN ADDRESS
  // ======================================================================
  // The tile at cell origin (8, 12) covers cells (8..11, 12..15). Each cell
  // carries a value derived from its index, so a transposed or rotated
  // mapping fails rather than passing on a uniform fill.
  {
    reset(kZ60);
    stream_tile(8, 12);
    close_tile();
    for (int i = 0; i < 16; ++i)
      write_cell(i, static_cast<uint16_t>(0x1000 + i), i - 8, (i % 9) - 4,
                 (i & 1) != 0);
    commit();

    int wrong = 0;
    int first_bad = -1;
    for (int i = 0; i < 16; ++i) {
      const int cx = 8 + (i & 3);
      const int cy = 12 + ((i >> 2) & 3);
      const Answer a = read_cell(false, cx, cy);
      const bool ok = a.present && a.glow == (0x1000 + i) && a.dx == (i - 8) &&
                      a.dy == ((i % 9) - 4) && a.ink == ((i & 1) != 0);
      if (!ok) {
        if (first_bad < 0) first_bad = i;
        ++wrong;
      }
    }
    zhao::check(wrong == 0,
                "all sixteen cells of one tile land at origin + "
                "{index[3:2], index[1:0]} -- the index is {cy, cx} in the "
                "tile, and a transposed map fails this rather than passing "
                "on a uniform fill",
                0, wrong);
    if (wrong) std::printf("  first wrong cell index %d\n", first_bad);
    zhao::check(top.cells_written_o == 16 && top.oob_writes_o == 0,
                "sixteen cells accepted, none out of bounds", 16,
                top.cells_written_o);
  }

  // ======================================================================
  // 3. THE ORIGIN PIPELINE -- the case a one-register store gets wrong
  // ======================================================================
  // Tile A streams, closes, and tile B STARTS STREAMING before A's cells are
  // flushed. That is what POST.GATHER's ping-pong produces on every tile but
  // the first, and it is the metadata-swap shape: an implementation that
  // wrote at the CURRENT origin would put A's light at B's address and pass
  // every other case in this file.
  {
    reset(kZ60);
    stream_tile(4, 4);        // tile A
    close_tile();             // A is now the flushing tile
    stream_tile(40, 40);      // tile B begins streaming -- the trap
    write_cell(0, 0xBEEF, 3, -2, true);   // ...and A's cells arrive NOW
    commit();

    const Answer at_a = read_cell(false, 4, 4);
    const Answer at_b = read_cell(false, 40, 40);
    zhao::check(at_a.present && at_a.glow == 0xBEEF && at_a.dx == 3 &&
                    at_a.dy == -2 && at_a.ink,
                "a tile's cells land at the origin of the tile that CLOSED, "
                "not the one now streaming -- the metadata-swap case, which a "
                "single origin register fails and nothing else here catches",
                0xBEEF, at_a.glow);
    zhao::check(at_b.glow == 0,
                "and nothing was written at the streaming tile's origin",
                0, at_b.glow);
  }

  // ======================================================================
  // 4. DUO STACKS, and the view offset is on the READ side only
  // ======================================================================
  // Getting this backwards puts player two's bloom on player one's screen.
  // The write side is in CANVAS coordinates and carries no view; the read
  // side is VIEW-LOCAL, so view 1 reads canvas row cy + view_rows.
  {
    reset(kDuo);
    // Write a marker in view 0's territory (canvas row 4) and a different one
    // in view 1's (canvas row 48 + 4 = 52).
    stream_tile(0, 4);
    close_tile();
    write_cell(0, 0x0AA0, 0, 0, false);
    stream_tile(0, 52);
    close_tile();
    write_cell(0, 0x0BB0, 0, 0, false);
    commit();

    const Answer v0 = read_cell(false, 0, 4);
    const Answer v1 = read_cell(true, 0, 4);   // view 1, view-local row 4
    zhao::check(v0.present && v0.glow == 0x0AA0,
                "Duo view 0 reads canvas row cy with no offset", 0x0AA0,
                v0.glow);
    zhao::check(v1.present && v1.glow == 0x0BB0,
                "Duo view 1's view-local row 4 reads CANVAS row 52 -- the "
                "views stack, because zhao_post_lease reads one tall source "
                "of frame_h << 1 per frame",
                0x0BB0, v1.glow);
    zhao::check(v0.glow != v1.glow,
                "and the two views are distinct cells, so a refraction can "
                "never sample across the split", 1,
                (v0.glow != v1.glow) ? 1 : 0);
  }

  // ======================================================================
  // 5. oob_writes_o FIRES -- a cell outside the plane is refused and counted
  // ======================================================================
  {
    reset(kStorm);   // 80 cells wide
    zhao::check(top.oob_writes_o == 0, "oob_writes_o starts at zero", 0,
                top.oob_writes_o);
    stream_tile(78, 4);   // cells 78..81, and 80/81 are off the right edge
    close_tile();
    for (int i = 0; i < 16; ++i) write_cell(i, 0x7777, 0, 0, false);
    commit();
    zhao::check(top.oob_writes_o == 8,
                "the eight cells past the right edge are REFUSED and COUNTED, "
                "not wrapped onto the next row -- a wrap would put the right "
                "of the screen's bloom on the left of the row below",
                8, top.oob_writes_o);
    zhao::check(top.cells_written_o == 8, "and the eight on the plane landed",
                8, top.cells_written_o);
    fired_oob = top.oob_writes_o;
  }

  // ======================================================================
  // 6. flush_overrun_o FIRES -- the positive control for the smoke's zero
  // ======================================================================
  // `w_busy_i` is POST.GATHER's `flush_busy_o`, a sixteen-clock walk.
  // `org_close_i` comes from the RASTER's 256-pixel tile cadence. Nothing
  // clocks both, so this detector can see a timing fault and not only a value
  // fault -- which is the distinction CLAUDE.md's metadata chapter turns on.
  {
    reset(kZ60);
    zhao::check(top.flush_overrun_o == 0, "flush_overrun_o starts at zero", 0,
                top.flush_overrun_o);
    stream_tile(4, 4);
    close_tile();
    // A flush is now draining...
    top.w_busy_i = 1;
    write_cell(0, 0x1111, 0, 0, false);
    // ...and a tile closes underneath it. This is the fault.
    close_tile();
    top.w_busy_i = 0;
    zhao::check(top.flush_overrun_o == 1,
                "flush_overrun_o FIRES when a tile closes while a flush is "
                "still draining -- the composed console quotes this counter "
                "at zero, and this is the firing that makes the zero worth "
                "quoting",
                1, top.flush_overrun_o);
    fired_overrun = top.flush_overrun_o;
  }

  // ======================================================================
  // 7. rdw_collide_o FIRES -- the instrument for "one plane is enough"
  // ======================================================================
  // The single-plane decision rests on the raster and post phases never
  // overlapping (the shell's `rpx_ready` is `!post_phase_w && ...`). That is
  // a CLAIM, and this counter is what checks it rather than a comment
  // asserting it. A second plane would be 27 M10K spent on a hazard that
  // cannot occur -- "cannot" being the word worth an instrument.
  {
    reset(kZ60);
    zhao::check(top.rdw_collide_o == 0, "rdw_collide_o starts at zero", 0,
                top.rdw_collide_o);
    stream_tile(4, 4);
    close_tile();
    // Write cell index 0 -- canvas cell (4, 4) -- and read the SAME cell on
    // the same clock.
    top.gd_req_v_i = 1;
    top.gd_view_i = 0;
    top.gd_cx_i = 4;
    top.gd_cy_i = 4;
    top.gg_req_v_i = 1;
    top.gg_view_i = 0;
    top.gg_cx_i = 4;
    top.gg_cy_i = 4;
    write_cell(0, 0x2222, 0, 0, false);
    top.gd_req_v_i = 0;
    top.gg_req_v_i = 0;
    zhao::check(top.rdw_collide_o == 1,
                "rdw_collide_o FIRES when a read and a write name one cell on "
                "one clock -- the phase interlock breaking is what this reads, "
                "and the console asserts it at zero",
                1, top.rdw_collide_o);
    zhao::check(top.rdw_collide_o == 1,
                "ONE count per colliding clock, not one per client: what it "
                "reports is 'the phases overlapped', which is one event "
                "however many ports saw it",
                1, top.rdw_collide_o);
    fired_rdw = top.rdw_collide_o;
  }

  // ======================================================================
  // 8. org_ok_i IS A FLAG AND IT IS HONOURED (R197)
  // ======================================================================
  // `fb_x_o`/`fb_y_o` are SIGNED, so a tile the binner places off the left or
  // top edge arrives negative and narrowing it into seven bits aliases it
  // back INTO the plane. R197: "a sentinel value is the defect. A flag is the
  // fix." The flag travels through the SAME two-deep pipeline as the
  // coordinates, because it describes the tile being flushed.
  {
    reset(kZ60);
    stream_tile(9, 9, /*ok=*/false);   // an off-canvas tile
    close_tile();
    stream_tile(20, 20, /*ok=*/true);  // a legal tile begins streaming
    for (int i = 0; i < 16; ++i) write_cell(i, 0x3333, 0, 0, false);
    commit();
    zhao::check(top.cells_written_o == 0 && top.oob_writes_o == 16,
                "an off-canvas tile's whole flush is refused and counted -- "
                "and the flag followed the CLOSED tile, not the one now "
                "streaming, which is the same two-deep pipeline as the "
                "coordinates",
                16, top.oob_writes_o);
    const Answer a = read_cell(false, 20, 20);
    zhao::check(a.glow == 0,
                "nothing landed at the streaming tile's origin either", 0,
                a.glow);
  }

  // ======================================================================
  // 9. THE PLANE'S LIFETIME -- a write invalidates, a commit validates
  // ======================================================================
  // This is where the composed console's first version was WRONG. The store
  // used to take a `plane_open_i` from the frame tick; the post pass runs for
  // most of a frame, so the tick fired inside it and cleared the plane under
  // the compositor's own read -- 582,252 misses against 188,056 hits on a
  // plane that was complete the whole time. The plane opens when something
  // OVERWRITES it, which is an event on this block's own write port.
  {
    reset(kZ60);
    stream_tile(0, 0);
    close_tile();
    write_cell(0, 0x4444, 0, 0, false);
    commit();
    zhao::check(read_cell(false, 0, 0).present,
                "after a commit the plane answers PRESENT", 1, 1);

    // A new frame's first cell arrives: the plane is being rebuilt.
    stream_tile(0, 0);
    close_tile();
    write_cell(1, 0x5555, 0, 0, false);
    zhao::check(!read_cell(false, 0, 0).present,
                "the FIRST cell of the next frame invalidates the plane -- a "
                "plane being overwritten has not got a complete frame in it, "
                "and there is no second event that could disagree with this "
                "one",
                0, 0);
    zhao::check(top.plane_commits_o == 1, "one commit counted", 1,
                top.plane_commits_o);
  }

  // THE COUNTERS BELOW ARE NOT THIS BLOCK'S CURRENT STATE, and printing that
  // would be actively misleading: cases 6, 7 and 5 fire the three tripwires
  // and cases 8 and 9 reset the block, so a live read prints ZERO on all
  // three and reads exactly like a bench that never fired them. Captured at
  // the moment they moved instead.
  std::printf(
      "  POSITIVE CONTROLS FIRED: flush_overrun=%u rdw_collide=%u "
      "oob_writes=%u -- the composed console asserts all three at ZERO, and "
      "these are the firings that make those zeros evidence rather than\n"
      "  silence\n",
      fired_overrun, fired_rdw, fired_oob);

  return zhao::report_and_exit("post_gather_store_directed");
}
