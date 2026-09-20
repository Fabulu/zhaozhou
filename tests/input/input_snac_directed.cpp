// input_snac_directed.cpp -- INPUT.SNAC directed vectors (owner ruling R7).
//
// Law: spec/input_rules.md 1, 2.3, 4 and 7 / design/contracts/INPUT.SNAC.md.
//
// The six cases, and what each one is actually protecting:
//
//   1. TRANSPARENT WHEN IDLE. With nothing plugged in, the adapter is the
//      IDENTITY on the whole pad bus. This is the claim a reader checks first,
//      because the block sits in the path of a capability that already worked,
//      and "it is transparent" is exactly the kind of sentence that gets
//      asserted in a comment and never measured. The /ACK timeout counter is
//      also seen to FIRE here, on legal stimulus.
//   2. A DIGITAL PAD REACHES THE CANONICAL BUS. Slot 0 goes present with the
//      button word zref::SnacAdapter computes, sticks centred; slots 1-3 still
//      carry the host route untouched, in the same cycle.
//   3. THE AXIS LAW OVER ALL 256 CODES. `axisInverse(axis(a)) == a` for every
//      a, so the transform adds and removes nothing (input_rules.md 1's "the
//      exact raw sample travels to software unchanged"). Checked against the
//      oracle, not against a transcription of the formula.
//   4. AN ANALOG PAD'S FOUR AXES, through the RTL, against the oracle.
//   5. AN UNIMPLEMENTED MODE IS ABSENT, COUNTED, AND LOSES NOTHING. The slot
//      falls back to the host route rather than going dark -- a refusal that
//      removed function would be the failure this campaign forbids.
//   6. THE MERGE-PATH GAP COUNTER FIRES. input_rules.md 2.3's counter, on
//      legal stimulus (ticks faster than the bus), so it owes no mutant.
//
// Every decoded value is compared against `zref::SnacAdapter`; the button
// table and the axis law are NOT restated here.

#include <cstdio>
#include <cstring>

#include "Vzhao_input_snac.h"
#include "zhao_sim.hpp"
#include "zref/zref_input.hpp"

// The verilated instance is parameterised down for simulation speed:
// CLK_DIV=4, ACK_TIMEOUT=64, IDLE_GAP=4 (see tests/CMakeLists.txt). The
// DECODED VALUES do not depend on any of them, which is the property that
// makes a fast bench evidence about the shipping one.
static constexpr int kPorts = 2;

static Vzhao_input_snac top;

// ---------------------------------------------------------------------------
// A PS1 CONTROLLER, as a bench model
// ---------------------------------------------------------------------------
// It watches /ATT and CLK exactly as a pad does: presents its DAT bit while
// CLK is low, lets the host sample on the rising edge, and pulses /ACK after
// every byte except the last one it intends to send. It also CAPTURES the
// host's CMD bits, so the test can assert that the console actually sent 0x01
// then 0x42 rather than something that happened to elicit a reply.
class PadModel {
 public:
  bool attached = false;
  zref::SnacReply reply = zref::snacNoPad();

  void reset() {
    prev_clk_ = 1;
    prev_att_ = 1;
    byte_ = 0;
    bit_ = 0;
    ack_ctr_ = 0;
    dat_ = 1;
    ackn_ = 1;
    cmd_shift_ = 0;
    cmd_seen_n_ = 0;
    std::memset(cmd_seen_, 0, sizeof cmd_seen_);
  }

  uint8_t dat() const { return dat_; }
  uint8_t ackn() const { return ackn_; }
  int cmdSeenCount() const { return cmd_seen_n_; }
  uint8_t cmdSeen(int i) const { return cmd_seen_[i]; }

  // Called once per simulated clock cycle, BEFORE the posedge, with the bus
  // pins the DUT is currently driving.
  void step(uint8_t att_n, uint8_t clk, uint8_t cmd) {
    if (ack_ctr_ > 0) {
      --ack_ctr_;
      ackn_ = (ack_ctr_ > 0) ? 0 : 1;
    }

    if (att_n && !prev_att_) {  // /ATT released: the poll is over
      byte_ = 0;
      bit_ = 0;
      dat_ = 1;
      ackn_ = 1;
      ack_ctr_ = 0;
      cmd_shift_ = 0;
    }
    if (!att_n && prev_att_) {  // /ATT asserted: a poll begins
      byte_ = 0;
      bit_ = 0;
      cmd_shift_ = 0;
      dat_ = attached ? bitOf(replyByte(0), 0) : 1;
    }

    if (!att_n) {
      if (!clk && prev_clk_) {  // falling edge: present this bit
        dat_ = attached ? bitOf(replyByte(byte_), bit_) : 1;
      }
      if (clk && !prev_clk_) {  // rising edge: the host samples; so do we
        cmd_shift_ |= static_cast<uint8_t>((cmd & 1) << bit_);
        if (bit_ == 7) {
          if (cmd_seen_n_ < 16) cmd_seen_[cmd_seen_n_++] = cmd_shift_;
          cmd_shift_ = 0;
          // /ACK after every byte except the last one this pad sends
          if (attached && byte_ + 1 < nBytes()) {
            ack_ctr_ = 4;
            ackn_ = 0;
          }
          ++byte_;
          bit_ = 0;
          dat_ = attached ? bitOf(replyByte(byte_), 0) : 1;
        } else {
          ++bit_;
        }
      }
    }

    prev_clk_ = clk;
    prev_att_ = att_n;
  }

 private:
  int nBytes() const { return zref::SnacAdapter::replyBytes(reply.mode); }

  uint8_t replyByte(int i) const {
    switch (i) {
      case 0: return 0xFF;  // the idle byte while the host addresses the pad
      case 1: return reply.mode;
      case 2: return reply.ready;
      case 3: return reply.btn_lo;
      case 4: return reply.btn_hi;
      case 5: return reply.rjx;
      case 6: return reply.rjy;
      case 7: return reply.ljx;
      case 8: return reply.ljy;
      default: return 0xFF;
    }
  }
  static uint8_t bitOf(uint8_t b, int i) { return static_cast<uint8_t>((b >> i) & 1); }

  uint8_t prev_clk_ = 1, prev_att_ = 1, dat_ = 1, ackn_ = 1, cmd_shift_ = 0;
  int byte_ = 0, bit_ = 0, ack_ctr_ = 0, cmd_seen_n_ = 0;
  uint8_t cmd_seen_[16] = {0};
};

static PadModel pads[kPorts];

// ---------------------------------------------------------------------------
// bench plumbing
// ---------------------------------------------------------------------------
static uint64_t g_frame_id = 0;

static void applyPadPins() {
  uint32_t dat = 0, ackn = 0;
  for (int p = 0; p < kPorts; ++p) {
    dat |= static_cast<uint32_t>(pads[p].dat()) << p;
    ackn |= static_cast<uint32_t>(pads[p].ackn()) << p;
  }
  top.snac_dat_i = dat;
  top.snac_ack_n_i = ackn;
}

// The frame boundary is a BARE PULSE on this block (it stamps no frame_id),
// so there is no packed `zhao_frame_tick_t` word to reproduce here.
static void cycle(bool tick = false) {
  for (int p = 0; p < kPorts; ++p)
    pads[p].step(static_cast<uint8_t>((top.snac_att_n_o >> p) & 1),
                 static_cast<uint8_t>(top.snac_clk_o & 1),
                 static_cast<uint8_t>(top.snac_cmd_o & 1));
  applyPadPins();
  top.frame_tick_i = tick ? 1 : 0;
  if (tick) ++g_frame_id;
  top.clk = 0;
  top.eval();
  top.clk = 1;
  top.eval();
  top.frame_tick_i = 0;
  top.eval();
}

static void run(int n, int tick_every = 0) {
  for (int i = 0; i < n; ++i) cycle(tick_every > 0 && ((i + 1) % tick_every == 0));
}

// Run until `n` more polls have COMPLETED, or the cap expires. Waiting on the
// counter rather than on a cycle count is what makes a stimulus change
// certainly visible: the first completed poll after a reply changes may have
// been in flight when it changed, so callers ask for two.
static bool waitPolls(int n, int cap = 20000) {
  const uint64_t target = static_cast<uint64_t>(top.snac_polls_o) + static_cast<uint64_t>(n);
  for (int i = 0; i < cap; ++i) {
    cycle(false);
    if (static_cast<uint64_t>(top.snac_polls_o) >= target) return true;
  }
  return false;
}

static void driveHost(int slot, const zref::PadRawState& s) {
  const uint32_t mask = ~(1u << slot);
  top.host_pad_present_i = (top.host_pad_present_i & mask) | (s.present ? (1u << slot) : 0u);
  top.host_pad_buttons_i[slot] = s.buttons;
  top.host_pad_lx_i[slot] = static_cast<uint16_t>(s.lx);
  top.host_pad_ly_i[slot] = static_cast<uint16_t>(s.ly);
  top.host_pad_rx_i[slot] = static_cast<uint16_t>(s.rx);
  top.host_pad_ry_i[slot] = static_cast<uint16_t>(s.ry);
}

static zref::PadRawState readOut(int slot) {
  zref::PadRawState s;
  s.present = ((top.pad_present_o >> slot) & 1) != 0;
  s.buttons = top.pad_buttons_o[slot];
  s.lx = static_cast<int16_t>(top.pad_lx_o[slot]);
  s.ly = static_cast<int16_t>(top.pad_ly_o[slot]);
  s.rx = static_cast<int16_t>(top.pad_rx_o[slot]);
  s.ry = static_cast<int16_t>(top.pad_ry_o[slot]);
  return s;
}

static bool same(const zref::PadRawState& a, const zref::PadRawState& b) {
  return a.present == b.present && a.buttons == b.buttons && a.lx == b.lx && a.ly == b.ly &&
         a.rx == b.rx && a.ry == b.ry;
}

static void resetDut() {
  for (int p = 0; p < kPorts; ++p) {
    pads[p].reset();
    pads[p].attached = false;
    pads[p].reply = zref::snacNoPad();
  }
  top.rst_n = 0;
  top.host_pad_present_i = 0;
  for (int i = 0; i < 4; ++i) {
    top.host_pad_buttons_i[i] = 0;
    top.host_pad_lx_i[i] = 0;
    top.host_pad_ly_i[i] = 0;
    top.host_pad_rx_i[i] = 0;
    top.host_pad_ry_i[i] = 0;
  }
  applyPadPins();
  top.frame_tick_i = 0;
  for (int i = 0; i < 6; ++i) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
  }
  top.rst_n = 1;
  top.eval();
}

// A host route with distinctive values per slot, so a pass-through that
// silently zeroes or swaps a slot cannot look like a pass-through.
static zref::PadRawState hostFixture(int slot) {
  zref::PadRawState s;
  s.present = true;
  s.buttons = 0x0000'1111u << slot;
  s.lx = static_cast<int16_t>(0x0100 + slot);
  s.ly = static_cast<int16_t>(-0x0200 - slot);
  s.rx = static_cast<int16_t>(0x0300 + slot);
  s.ry = static_cast<int16_t>(-0x0400 - slot);
  return s;
}

int main() {
  // =========================================================== case 1 ======
  // TRANSPARENT WHEN IDLE: nothing plugged in, the whole pad bus passes
  // through bit for bit, and the /ACK timeout counter is seen to move.
  resetDut();
  for (int i = 0; i < 4; ++i) driveHost(i, hostFixture(i));
  top.eval();
  run(4000, 200);

  zhao::check(top.snac_present_o == 0, "case1: no pad attached -> snac_present_o == 0", 0,
              top.snac_present_o);
  for (int i = 0; i < 4; ++i) {
    const zref::PadRawState got = readOut(i);
    const zref::PadRawState want = hostFixture(i);
    char what[128];
    std::snprintf(what, sizeof what, "case1: slot %d passes the host route through unchanged", i);
    zhao::check(same(got, want), what, 1, same(got, want) ? 1 : 0);
  }
  zhao::check(top.snac_timeouts_o > 0,
              "case1: the /ACK timeout counter FIRES on an empty port (legal stimulus)", 1,
              top.snac_timeouts_o > 0 ? 1 : 0);
  zhao::check(top.snac_polls_o == 0, "case1: no poll completed", 0,
              static_cast<uint64_t>(top.snac_polls_o));
  zhao::check(top.snac_bad_header_o == 0,
              "case1: an EMPTY port is not a bad header (the counter stays useful)", 0,
              static_cast<uint64_t>(top.snac_bad_header_o));
  zhao::check(top.input_snac_input_sequence_gaps_o == 0,
              "case1: no gaps while no slot is present", 0,
              static_cast<uint64_t>(top.input_snac_input_sequence_gaps_o));

  // =========================================================== case 2 ======
  // A DIGITAL PAD on port 0. START + CROSS + LEFT held, everything else up.
  resetDut();
  for (int i = 0; i < 4; ++i) driveHost(i, hostFixture(i));
  top.eval();
  {
    zref::SnacReply r = zref::snacNoPad();
    r.mode = zref::SnacAdapter::kModeDigital;
    r.ready = zref::SnacAdapter::kReadyByte;
    r.btn_lo = static_cast<uint8_t>(~((1u << 3) | (1u << 7)));  // START, LEFT
    r.btn_hi = static_cast<uint8_t>(~(1u << 6));                // CROSS
    pads[0].attached = true;
    pads[0].reply = r;

    run(4000, 0);

    const zref::PadRawState want = zref::SnacAdapter::decode(r);
    const zref::PadRawState got = readOut(0);
    zhao::check(((top.snac_present_o >> 0) & 1) == 1, "case2: slot 0 present from SNAC", 1,
                (top.snac_present_o >> 0) & 1);
    zhao::check(got.present, "case2: merged slot 0 present", 1, got.present ? 1 : 0);
    zhao::check(got.buttons == want.buttons,
                "case2: buttons match zref::SnacAdapter (canonical table 4)", want.buttons,
                got.buttons);
    zhao::check(got.lx == 0 && got.ly == 0 && got.rx == 0 && got.ry == 0,

                "case2: a digital pad reports centred sticks", 0,
                got.lx | got.ly | got.rx | got.ry);
    zhao::check(top.snac_polls_o > 0, "case2: polls completed", 1,
                top.snac_polls_o > 0 ? 1 : 0);
    // the other three slots are untouched IN THE SAME CYCLE
    for (int i = 1; i < 4; ++i) {
      const bool ok = same(readOut(i), hostFixture(i));
      char what[128];
      std::snprintf(what, sizeof what, "case2: slot %d still carries the host route", i);
      zhao::check(ok, what, 1, ok ? 1 : 0);
    }
    // the console really did speak the protocol
    zhao::check(pads[0].cmdSeenCount() >= 2, "case2: the host sent at least two command bytes", 1,
                pads[0].cmdSeenCount() >= 2 ? 1 : 0);
    zhao::check(pads[0].cmdSeen(0) == 0x01, "case2: byte 0 of the poll is 0x01 (address)", 0x01,
                pads[0].cmdSeen(0));
    zhao::check(pads[0].cmdSeen(1) == 0x42, "case2: byte 1 of the poll is 0x42 (read)", 0x42,
                pads[0].cmdSeen(1));
  }

  // ========================================================== case 2b ======
  // EVERY BIT OF THE BUTTON TABLE, one at a time.
  //
  // This case exists because the first version of case 2 held three buttons
  // and PASSED against a planted fault that swapped `up` for `right` -- both
  // were released in that fixture, so the swap was invisible. A table mapping
  // checked on three of its sixteen entries is not a checked table, and a test
  // that cannot fail on a wrong mapping is the reassuring instrument this
  // repo's CLAUDE.md is mostly about. Walking a single ACTIVE-LOW zero through
  // both PS1 bytes makes every one of the sixteen the only bit that differs,
  // so any permutation of the map fails at the bit it moved.
  {
    int mismatches = 0;
    int last_want = -1, last_got = -1, last_k = -1;
    for (int k = 0; k < 16; ++k) {
      zref::SnacReply r = zref::snacNoPad();
      r.mode = zref::SnacAdapter::kModeDigital;
      r.ready = zref::SnacAdapter::kReadyByte;
      r.btn_lo = (k < 8) ? static_cast<uint8_t>(~(1u << k)) : 0xFF;
      r.btn_hi = (k >= 8) ? static_cast<uint8_t>(~(1u << (k - 8))) : 0xFF;
      pads[0].reply = r;
      if (!waitPolls(2)) {
        zhao::check(false, "case2b: a poll completed for every pattern", 1, 0);
        break;
      }
      const uint32_t want = zref::SnacAdapter::decode(r).buttons;
      const uint32_t got = readOut(0).buttons;
      if (got != want) {
        ++mismatches;
        last_want = static_cast<int>(want);
        last_got = static_cast<int>(got);
        last_k = k;
      }
    }
    char what[160];
    std::snprintf(what, sizeof what,
                  "case2b: all 16 canonical buttons map bit-exactly (last bad PS1 bit %d)", last_k);
    zhao::check(mismatches == 0, what, last_want < 0 ? 0 : last_want, last_got < 0 ? 0 : last_got);
    zhao::check(mismatches == 0, "case2b: zero button-map mismatches across the walk", 0,
                mismatches);
  }

  // =========================================================== case 3 ======
  // THE AXIS LAW over all 256 codes: lossless and invertible, plus the three
  // anchors the spec names.
  {
    int bad = 0;
    for (int a = 0; a < 256; ++a) {
      const int16_t v = zref::SnacAdapter::axis(static_cast<uint8_t>(a));
      if (zref::SnacAdapter::axisInverse(v) != static_cast<uint8_t>(a)) ++bad;
    }
    zhao::check(bad == 0, "case3: the axis transform is invertible over all 256 codes", 0, bad);
    zhao::check(zref::SnacAdapter::axis(0x80) == 0, "case3: 0x80 is exactly centre", 0,
                zref::SnacAdapter::axis(0x80));
    zhao::check(zref::SnacAdapter::axis(0x00) == -32768, "case3: 0x00 is full negative", -32768,
                zref::SnacAdapter::axis(0x00));
    zhao::check(zref::SnacAdapter::axis(0xFF) == 32512, "case3: 0xFF is full positive", 32512,
                zref::SnacAdapter::axis(0xFF));
    int nonmono = 0;
    for (int a = 1; a < 256; ++a)
      if (!(zref::SnacAdapter::axis(static_cast<uint8_t>(a)) >
            zref::SnacAdapter::axis(static_cast<uint8_t>(a - 1))))
        ++nonmono;
    zhao::check(nonmono == 0, "case3: the axis transform is strictly monotonic", 0, nonmono);
  }

  // =========================================================== case 4 ======
  // AN ANALOG PAD's four axes, through the RTL, against the oracle. Four
  // distinct codes so a swapped pair cannot pass.
  resetDut();
  top.eval();
  {
    zref::SnacReply r = zref::snacNoPad();
    r.mode = zref::SnacAdapter::kModeAnalog;
    r.ready = zref::SnacAdapter::kReadyByte;
    r.btn_lo = 0xFF;
    r.btn_hi = 0xFF;
    r.ljx = 0x20;
    r.ljy = 0xC0;
    r.rjx = 0xF0;
    r.rjy = 0x00;
    pads[0].attached = true;
    pads[0].reply = r;

    run(6000, 0);

    const zref::PadRawState want = zref::SnacAdapter::decode(r);
    const zref::PadRawState got = readOut(0);
    zhao::check(got.present, "case4: analog pad present", 1, got.present ? 1 : 0);
    zhao::check(got.buttons == 0, "case4: nothing held -> buttons 0", 0, got.buttons);
    zhao::check(got.lx == want.lx, "case4: lx matches zref::SnacAdapter", want.lx, got.lx);
    zhao::check(got.ly == want.ly, "case4: ly matches zref::SnacAdapter", want.ly, got.ly);
    zhao::check(got.rx == want.rx, "case4: rx matches zref::SnacAdapter", want.rx, got.rx);
    zhao::check(got.ry == want.ry, "case4: ry matches zref::SnacAdapter", want.ry, got.ry);
  }

  // ========================================================== case 4b ======
  // THE AXIS LAW THROUGH THE RTL, swept. Case 3 proves the oracle's transform
  // is lossless; this proves the RTL implements THAT transform and not a
  // neighbouring one, at codes spread across the range including both rails
  // and the centre. Each of the four axes gets a different code in every pass,
  // so a crossed pair fails as well as a wrong shift.
  {
    static const uint8_t codes[8] = {0x00, 0x01, 0x40, 0x7F, 0x80, 0x81, 0xC0, 0xFF};
    int mismatches = 0;
    for (int i = 0; i < 8; ++i) {
      zref::SnacReply r = zref::snacNoPad();
      r.mode = zref::SnacAdapter::kModeAnalog;
      r.ready = zref::SnacAdapter::kReadyByte;
      r.btn_lo = 0xFF;
      r.btn_hi = 0xFF;
      r.ljx = codes[i];
      r.ljy = codes[(i + 1) & 7];
      r.rjx = codes[(i + 2) & 7];
      r.rjy = codes[(i + 3) & 7];
      pads[0].reply = r;
      if (!waitPolls(2)) {
        zhao::check(false, "case4b: a poll completed for every code set", 1, 0);
        break;
      }
      const zref::PadRawState want = zref::SnacAdapter::decode(r);
      const zref::PadRawState got = readOut(0);
      if (got.lx != want.lx || got.ly != want.ly || got.rx != want.rx || got.ry != want.ry)
        ++mismatches;
    }
    zhao::check(mismatches == 0,
                "case4b: all four axes match the oracle across the swept codes", 0, mismatches);
  }

  // =========================================================== case 5 ======
  // AN UNIMPLEMENTED MODE is absent, COUNTED, and loses nothing: the slot
  // falls back to the host route rather than going dark.
  resetDut();
  for (int i = 0; i < 4; ++i) driveHost(i, hostFixture(i));
  top.eval();
  {
    zref::SnacReply r = zref::snacNoPad();
    r.mode = 0x53;  // config mode: a real PS1 reply this block does not implement
    r.ready = zref::SnacAdapter::kReadyByte;
    pads[0].attached = true;
    pads[0].reply = r;

    run(6000, 0);

    zhao::check(top.snac_bad_header_o > 0,
                "case5: an unimplemented mode FIRES snac_bad_header_o", 1,
                top.snac_bad_header_o > 0 ? 1 : 0);
    zhao::check(((top.snac_present_o >> 0) & 1) == 0, "case5: the slot is not claimed by SNAC", 0,
                (top.snac_present_o >> 0) & 1);
    const bool ok = same(readOut(0), hostFixture(0));
    zhao::check(ok, "case5: the slot FALLS BACK to the host route (no function removed)", 1,
                ok ? 1 : 0);
    zhao::check(zref::SnacAdapter::decode(r).present == false,
                "case5: the oracle agrees the reply is not a pad", 0, 0);
  }

  // =========================================================== case 6 ======
  // THE MERGE-PATH GAP COUNTER FIRES (input_rules.md 2.3), on legal stimulus:
  // frame ticks arriving faster than the bus can complete a poll means frames
  // reuse a sample, and that is precisely what the counter is for.
  resetDut();
  top.eval();
  {
    zref::SnacReply r = zref::snacNoPad();
    r.mode = zref::SnacAdapter::kModeDigital;
    r.ready = zref::SnacAdapter::kReadyByte;
    r.btn_lo = 0xFF;
    r.btn_hi = 0xFF;
    pads[0].attached = true;
    pads[0].reply = r;

    // settle: let the slot become present with a slow tick first
    run(3000, 0);
    zhao::check(((top.snac_present_o >> 0) & 1) == 1, "case6: slot 0 present before the squeeze",
                1, (top.snac_present_o >> 0) & 1);
    const uint64_t gaps_before = top.input_snac_input_sequence_gaps_o;

    // now tick far faster than a poll round can complete
    run(2000, 8);

    zhao::check(top.input_snac_input_sequence_gaps_o > gaps_before,
                "case6: input_snac_input_sequence_gaps FIRES when ticks outrun the bus", 1,
                top.input_snac_input_sequence_gaps_o > gaps_before ? 1 : 0);
  }

  // =========================================================== case 7 ======
  // THE ROUND-ROBIN ACTUALLY REACHES PORT 1.
  //
  // Nothing above could tell the difference between an engine that walks the
  // connectors and one permanently parked on port 0: every earlier case drives
  // port 0, and case 1's timeout counter fires whether one port or two are
  // being polled. An engine stuck on port 0 would have passed all six. So this
  // case attaches a pad to port 1 ONLY -- a different button held, so the
  // answer cannot be port 0's leaking across -- and requires slot 1 to go
  // present while slot 0 falls back to the host route.
  resetDut();
  for (int i = 0; i < 4; ++i) driveHost(i, hostFixture(i));
  top.eval();
  {
    zref::SnacReply r = zref::snacNoPad();
    r.mode = zref::SnacAdapter::kModeDigital;
    r.ready = zref::SnacAdapter::kReadyByte;
    r.btn_lo = static_cast<uint8_t>(~(1u << 0));  // SELECT: canonical bit 14
    r.btn_hi = 0xFF;
    pads[1].attached = true;
    pads[1].reply = r;

    if (!waitPolls(2)) zhao::check(false, "case7: a poll completed on port 1", 1, 0);

    const zref::PadRawState want = zref::SnacAdapter::decode(r);
    const zref::PadRawState got = readOut(1);
    zhao::check(((top.snac_present_o >> 1) & 1) == 1,
                "case7: the round-robin REACHES port 1 (slot 1 present)", 1,
                (top.snac_present_o >> 1) & 1);
    zhao::check(got.buttons == want.buttons, "case7: port 1 decodes to slot 1's buttons",
                want.buttons, got.buttons);
    zhao::check(((top.snac_present_o >> 0) & 1) == 0,
                "case7: slot 0 is NOT claimed by a pad on port 1", 0,
                (top.snac_present_o >> 0) & 1);
    const bool ok0 = same(readOut(0), hostFixture(0));
    zhao::check(ok0, "case7: slot 0 still carries the host route", 1, ok0 ? 1 : 0);
    zhao::check(pads[1].cmdSeen(1) == 0x42, "case7: the poll on port 1 was a real read", 0x42,
                pads[1].cmdSeen(1));
    zhao::check(pads[0].cmdSeenCount() > 0,
                "case7: port 0 is still being polled too (the walk did not park on 1)", 1,
                pads[0].cmdSeenCount() > 0 ? 1 : 0);
  }

  return zhao::report_and_exit("input_snac_directed");
}
