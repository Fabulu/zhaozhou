// zref_video.cpp — ZRef oracle implementation for the W2.2 VIDEO subsystem.
// See zref_video.hpp for the law citations. Every mirror field below names
// its RTL counterpart in fpga/rtl/video/*.sv; when this file and the RTL
// disagree, one of them violates spec/video_rules.md and the differential
// fails with the exact cycle — fix the law breaker, never the test.

#include "zref/zref_video.hpp"

#include "zref/zref_render.hpp"  // canvas_bytes / displayed_crc32c (the one
                                 // C++ definition of both laws, charter 29-6)

#include <cstring>

namespace zref {

// ------------------------------------------------------------ constants ---

const VidTiming& vid_timing(uint32_t mode) {
  static const VidTiming kTable[3] = {
      // Z60: 384x240, H 480, 251,520 gpu cycles/frame (spec §2)
      {384, 8, 48, 40, 480, 240, 4, 4, 14, 262, 251520},
      // Storm: 320x240, H 416, 217,984
      {320, 8, 48, 40, 416, 240, 4, 4, 14, 262, 217984},
      // Duo: 512x240 (2 x 256x192), H 608, 318,592
      {512, 8, 48, 40, 608, 240, 4, 4, 14, 262, 318592},
  };
  return kTable[mode & 3u];
}

uint32_t canvas_bytes(uint32_t mode) {
  // Occupancy, not allocation (spec/video_rules.md §1): Z60 184,320 /
  // Storm 153,600 / Duo 196,608 (= 0x30000 packed view blocks, §3.1).
  // The branch's original returned the 0x3C000 Duo ALLOCATION — written
  // before the allocation/occupancy split was ratified — and would have
  // silently disagreed with zref::render::canvas_bytes after the merge.
  // One definition per language (charter 29-6): delegate.
  return render::canvas_bytes(static_cast<zhao_abi::video_mode>(mode));
}

uint32_t active_width(uint32_t mode) { return vid_timing(mode).h_active; }

bool duo_border_line(uint32_t display_y) {
  return display_y < 24 || display_y >= 216;  // spec §3.1 (48 border rows)
}

uint32_t duo_source_row(uint32_t display_y) { return display_y - 24; }

uint32_t frame_pixel_crc(uint32_t mode, const std::vector<uint8_t>& canvas) {
  // Thin vector adapter over the ONE displayed-stream composer (charter
  // 29-6): zref::render::displayed_crc32c implements the spec §4 law (for
  // Duo it needs the full 0x30000 stored occupancy behind the pointer).
  return render::displayed_crc32c(static_cast<zhao_abi::video_mode>(mode), canvas.data());
}

// ------------------------------------------------------------- VideoMode ---

void VideoMode::reset() {
  // contract VIDEO.MODE: raster pre-active at the start of BOTH back
  // porches; mode register = VIDEO_Z60 (spec §1.1 reset value).
  const VidTiming& t = vid_timing(0);
  x_ = t.h_active + t.h_front + t.h_sync;  // start of H back porch
  y_ = t.v_total - t.v_back;               // start of V back porch
  mode_cur_ = 0;
  mode_pend_ = 0;
  we_valid_ = false;
  we_value_ = 0;
}

void VideoMode::mode_we(uint32_t value, bool valid) {
  we_value_ = value;
  we_valid_ = valid;
}

void VideoMode::step() {
  const VidTiming& t = vid_timing(mode_cur_);
  const bool line_wrap = (x_ == t.h_total - 1);
  const bool frame_wrap = line_wrap && (y_ == t.v_total - 1);

  x_ = line_wrap ? 0 : x_ + 1;
  y_ = line_wrap ? (frame_wrap ? 0 : y_ + 1) : y_;
  if (frame_wrap) {
    mode_cur_ = mode_pend_;  // latch law (spec §1.1)
  }
  if (we_valid_ && we_value_ <= 2) {
    mode_pend_ = we_value_;  // last valid wins; rogues hold
  }
  we_valid_ = false;
}

RasterView VideoMode::view() const {
  const VidTiming& t = vid_timing(mode_cur_);
  RasterView v;
  v.x = x_;
  v.y = y_;
  v.hsync = (x_ >= t.h_active + t.h_front) && (x_ < t.h_active + t.h_front + t.h_sync);
  v.vsync = (y_ >= t.v_active + t.v_front) && (y_ < t.v_active + t.v_front + t.v_sync);
  v.hblank = x_ >= t.h_active;
  v.vblank = y_ >= t.v_active;
  v.frame_start = (x_ == 0) && (y_ == 0);
  v.frame_end = (x_ == 0) && (y_ == t.v_active);
  v.vswap_dec = (x_ == 0) && (y_ == t.v_active + t.v_front);
  v.mode = mode_cur_;
  v.mode_next = mode_pend_;
  return v;
}

// -------------------------------------------------------------- FrameCtl ---

namespace {
constexpr uint64_t kSat64 = 0xFFFFFFFFFFFFFFFFull;
}

void FrameCtl::reset() {
  committed_v_ = false;
  committed_slot_ = 0;
  deadline_left_ = 0;
  frame_id_q_ = 0;
  frame_cycles_q_ = 0;
  deadline_faults_q_ = 0;
  tick_tog_ = false;
  cur_slot_ = 0;
  cur_repeated_ = false;
  frame_tick_q_ = false;
  frame_repeated_q_ = false;
  frame_id_out_ = 0;
  deadline_margin_q_ = 0;
}

void FrameCtl::step(const FrameCtlIn& in) {
  const uint32_t load =
      (in.deadline_cycles != 0) ? in.deadline_cycles : vid_timing(in.mode).frame_gpu_cycles;

  // pre-edge samples (RTL NBA reads)
  const uint32_t dl_old = deadline_left_;
  const bool cv_old = committed_v_;
  const uint32_t cs_old = committed_slot_;
  const uint32_t fid_old = frame_id_q_;

  // deadline window: reload at frame_start, else decrement by 2 saturating
  if (in.frame_start) {
    deadline_left_ = load;
  } else if (dl_old >= 2) {
    deadline_left_ = dl_old - 2;
  } else {
    deadline_left_ = 0;
  }

  // commit: an uncommitted READY slot while the window is open
  const bool has_ready = (in.slot_ready & 3u) != 0;
  const uint32_t pick = (in.slot_ready & 1u) ? 0u : 1u;  // lowest index wins
  if (!cv_old && has_ready && dl_old != 0) {
    committed_v_ = true;
    committed_slot_ = pick;
  }

  // the decision (vswap_dec) — the RTL dec branch is written last, so it
  // overrides the commit latch of the same edge
  frame_tick_q_ = false;
  frame_repeated_q_ = false;
  if (in.vswap_dec) {
    if (cv_old) {
      cur_slot_ = cs_old;
      cur_repeated_ = false;
    } else {
      cur_repeated_ = true;  // repeat: fail-safe direction
      deadline_faults_q_ = (deadline_faults_q_ == kSat64) ? kSat64 : deadline_faults_q_ + 1;
    }
    committed_v_ = false;
    frame_id_q_ = fid_old + 1;
    frame_cycles_q_ = (frame_cycles_q_ == kSat64) ? frame_cycles_q_ : frame_cycles_q_ + 1;
    tick_tog_ = !tick_tog_;
    deadline_margin_q_ = dl_old;
    frame_tick_q_ = true;
    frame_repeated_q_ = !cv_old;
    frame_id_out_ = fid_old + 1;
  }
}

FrameCtlOut FrameCtl::out_comb(const FrameCtlIn& in) const {
  FrameCtlOut o;
  o.swap_req = in.vswap_dec && committed_v_;  // combinational at the dec
  o.swap_slot = committed_slot_;
  o.frame_tick = frame_tick_q_;
  o.frame_repeated = frame_repeated_q_;
  o.frame_id = frame_id_out_;
  o.frame_cycles = frame_cycles_q_;
  o.deadline_faults = deadline_faults_q_;
  o.deadline_margin = deadline_margin_q_;
  return o;
}

// ------------------------------------------------------------ ScalerFeed ---

void ScalerFeed::reset() {
  stage1_ = PxStream{};
  stage2_ = PxStream{};
  violation_q_ = false;
}

void ScalerFeed::step(const PxStream& in, bool out_ready) {
  if (in.valid && (in.hblank || in.vblank)) {
    violation_q_ = true;  // sticky protocol check
  }
  if (out_ready) {
    const PxStream s1_old = stage1_;  // pre-edge sample (RTL NBA)
    stage1_ = in;
    stage2_ = s1_old;
  }  // else: freeze — both stages hold (the stall propagates upstream)
}

// --------------------------------------------------------- VramResponder ---

void VramResponder::reset() {
  vram_.assign(0x78000, 0);  // both slots, max-canvas sized (spec §3)
  busy_ = false;
  beat_countdown_ = 0;
  beats_left_ = 0;
  burst_base_ = 0;
}

void VramResponder::set_canvas(uint32_t slot, const std::vector<uint8_t>& bytes) {
  const size_t base = (size_t)slot * 0x3C000u;
  const size_t n = bytes.size() <= 0x3C000u ? bytes.size() : 0x3C000u;
  if (n) std::memcpy(vram_.data() + base, bytes.data(), n);
}

VramResponder::Out VramResponder::step(bool req_valid, uint32_t req_addr, uint32_t req_len) {
  Out o;
  o.ok = true;

  // beats: one per gpu cycle once the latency has elapsed
  if (busy_ && beat_countdown_ > 0) {
    --beat_countdown_;
  }
  if (busy_ && beat_countdown_ == 0 && beats_left_ > 0) {
    o.beat_valid = true;
    const size_t served = 8u - beats_left_;
    // bus address -> internal 2-slot layout (slot 1 sits in DRAM bank 1 on
    // the bus since the W2.7 bank split; internally the slots stay packed)
    const uint32_t kSlot1 = 0x02000000u;
    uint32_t base_int = burst_base_;
    if (burst_base_ >= kSlot1) base_int = burst_base_ - kSlot1 + 0x3C000u;
    const size_t off = (size_t)base_int + served * 8u;
    uint64_t w = 0;
    for (uint32_t b = 0; b < 8; ++b) {
      const size_t ib = off + b;
      const uint8_t byte = (ib < vram_.size()) ? vram_[ib] : 0u;  // defensive: the differential
      w |= (uint64_t)byte << (8 * b);                             // catches address bugs anyway
    }
    o.beat_data = w;
    --beats_left_;
    if (beats_left_ == 0) busy_ = false;
  }

  // admission: accept when idle and service enabled
  if (!busy_ && service_) {
    o.ready = req_valid;
    if (req_valid) {
      busy_ = true;
      burst_base_ = req_addr;
      beat_countdown_ = latency_;
      beats_left_ = req_len / 8u;  // 64-bit beats of the 64-B request
    }
  }
  return o;
}

// ---------------------------------------------------------------- Scanout --

namespace {
// fetch FSM states (zhao_scanout_fetch.sv)
constexpr uint32_t F_ARM = 0, F_WAIT = 1, F_REQ = 2, F_BEATS = 3, F_PARK = 4;
constexpr uint32_t LB_EMPTY = 0, LB_FILLING = 1, LB_FULL = 2;

struct SegGeom {
  uint32_t addr;  // byte address of the segment start
  uint32_t reqs;  // 64-B requests in the segment
  bool real;      // the line needs fetched data
  bool last;      // no further real line this frame
  uint32_t next;  // next display line to fill
};

SegGeom seg_geometry(uint32_t mode, uint32_t slot, uint32_t line, uint32_t seg) {
  SegGeom g{0, 0, true, false, 0};
  const uint32_t base = slot ? 0x02000000u : 0u;  // bank split (zhao_pkg)
  switch (mode) {
    case 0:
      g.addr = base + line * 768u;
      g.reqs = 12;
      g.last = line == 239;
      g.next = line + 1;
      break;
    case 1:
      g.addr = base + line * 640u;
      g.reqs = 10;
      g.last = line == 239;
      g.next = line + 1;
      break;
    default:
      if (line < 24 || line >= 216) {
        g.real = false;
        g.next = 24;
      } else {
        g.addr = base + (line - 24) * 512u + (seg ? 0x18000u : 0u);
        g.reqs = 8;
        g.last = line == 215;
        g.next = line + 1;
      }
      break;
  }
  return g;
}
}  // namespace

void Scanout::reset() {
  gpu_steps_ = 0;
  vid_steps_ = 0;

  mode_.reset();
  raster_ = mode_.view();
  framectl_.reset();
  fctl_in_ = FrameCtlIn{};
  fctl_out_reg_ = FrameCtlOut{};

  display_slot_ = 0;
  swap_ack_ = false;
  dec_tog_ = false;
  fs_tog_ = false;
  dec_s1_ = dec_s2_ = dec_s2q_ = dec_sync_ = false;
  fs_s1_ = fs_s2_ = fs_s2q_ = fs_sync_ = false;
  slot_s1_ = slot_s2_ = 0;
  mnext_s1_ = mnext_s2_ = mode_s1_ = mode_s2_ = 0;

  f_state_ = F_ARM;
  fetch_mode_ = 0;
  fetch_slot_ = 0;
  fetch_line_ = 0;
  seg_idx_ = 0;
  req_idx_ = 0;
  beat_cnt_ = 0;
  fill_words_ = 0;
  fill_line_buf_ = 0;

  bstate_[0] = bstate_[1] = LB_EMPTY;
  full_toggle_[0] = full_toggle_[1] = false;
  cons_s1_[0] = cons_s1_[1] = false;
  cons_s2_[0] = cons_s2_[1] = false;
  cons_s2q_[0] = cons_s2q_[1] = false;
  consumed_toggle_[0] = consumed_toggle_[1] = false;
  full_s1_[0] = full_s1_[1] = false;
  full_s2_[0] = full_s2_[1] = false;
  last_seen_[0] = last_seen_[1] = false;
  for (auto& row : mem_)
    for (auto& w : row) w = 0;  // canonical black until first fill

  display_buf_ = false;
  line_fresh_ = false;
  last_px_ = 0;
  starve_q_ = 0;
  consume_start_[0] = consume_start_[1] = false;
  consume_done_[0] = consume_done_[1] = false;

  scaler_.reset();
  ser_px_ = decode_px_();  // the serializer level of the reset cycle

  f_tog_s1_ = f_tog_s2_ = f_tog_s2q_ = false;
  f_slot_s1_ = f_slot_s2_ = 0;
  gpu_tick_q_ = false;
  gpu_tick_frame_id_q_ = 0;
  gpu_tick_repeated_q_ = false;
  gpu_complete_slot_q_ = 0;
}

void Scanout::step(const VideoSysIn& in) {
  ++gpu_steps_;
  const bool vid_edge = (gpu_steps_ & 1ull) == 1ull;

  // ---- snapshot of the crossing sources (RTL samples pre-edge values) ----
  const bool s_dec_tog = dec_tog_;
  const bool s_fs_tog = fs_tog_;
  const uint32_t s_slot = display_slot_;
  const uint32_t s_mnext = mode_.mode_pend();
  const uint32_t s_mode = mode_.mode_cur();
  const bool s_full_tog0 = full_toggle_[0], s_full_tog1 = full_toggle_[1];
  const bool s_cons_tog0 = consumed_toggle_[0], s_cons_tog1 = consumed_toggle_[1];
  const bool s_ftog = framectl_.tick_tog();
  const uint32_t s_fslot = framectl_.display_slot();
  const bool s_frep = framectl_.cur_repeated();
  const uint32_t s_ffid = framectl_.frame_id();

  if (vid_edge) {
    vid_step_(in, s_full_tog0, s_full_tog1);
    ++vid_steps_;
  }
  gpu_step_(in, s_dec_tog, s_fs_tog, s_slot, s_mnext, s_mode, s_cons_tog0, s_cons_tog1, s_ftog,
            s_fslot, s_frep, s_ffid);
}

// -------------------------------------------------------------- vid side --
void Scanout::vid_step_(const VideoSysIn& in, bool s_full_tog0, bool s_full_tog1) {
  // raster decodes of the CURRENT cycle (pre-edge mode registers)
  const RasterView cur = raster_;

  // FRAMECTL stimulus + combinational swap command during this cycle
  fctl_in_.vswap_dec = cur.vswap_dec;
  fctl_in_.frame_start = cur.frame_start;
  fctl_in_.mode = cur.mode;
  fctl_in_.slot_ready = in.slot_ready;
  fctl_in_.deadline_cycles = in.deadline_cycles;
  const FrameCtlOut fco = framectl_.out_comb(fctl_in_);
  const bool swap_req = fco.swap_req;
  const uint32_t swap_slot = fco.swap_slot;

  // ---- SCALER steps on the stream level DURING this cycle ----
  scaler_.step(ser_px_, in.px_out_ready);

  // ---- LINEBUF vid side: full-sync chain (pre-edge s_full_tog) + consume
  // pulses (levels of THIS cycle = registered at the previous edge) ----
  const bool buf_fresh0 = full_s2_[0] != last_seen_[0];
  const bool buf_fresh1 = full_s2_[1] != last_seen_[1];

  const bool full_s1_o0 = full_s1_[0], full_s1_o1 = full_s1_[1];
  full_s1_[0] = s_full_tog0;
  full_s1_[1] = s_full_tog1;
  full_s2_[0] = full_s1_o0;
  full_s2_[1] = full_s1_o1;
  if (consume_start_[0]) last_seen_[0] = full_s2_[0];
  if (consume_start_[1]) last_seen_[1] = full_s2_[1];
  if (consume_done_[0]) consumed_toggle_[0] = !consumed_toggle_[0];
  if (consume_done_[1]) consumed_toggle_[1] = !consumed_toggle_[1];

  // ---- SERIALIZER (zhao_scanout_serializer.sv; pre-edge regs) ----
  const VidTiming& t = vid_timing(cur.mode);
  const bool line_active = cur.y < t.v_active;
  const bool line_real = (cur.mode != 2) ? line_active : (cur.y >= 24 && cur.y < 216);
  const bool line_start = (cur.x == 0) && line_active;
  const bool line_end = (cur.x == t.h_total - 1) && line_active;
  const bool px_valid = line_active && !cur.hblank;

  // ---- line boundary law (zhao_scanout_serializer.sv): the decision for
  // the NEXT line happens at the edge ending the CURRENT line's last cycle
  const bool line_last = (cur.x == t.h_total - 1);
  const bool last_of_frame = line_last && (cur.y == t.v_total - 1);
  const uint32_t y_next = last_of_frame ? 0u : cur.y + 1u;
  // at the wrap edge the next frame runs under the PENDING mode (the latch
  // fires at this edge — mirrors zhao_scanout_serializer.sv)
  const uint32_t mode_of_next = last_of_frame ? mode_.mode_pend() : cur.mode;
  const bool next_active = y_next < t.v_active;
  const bool next_real = (mode_of_next != 2) ? next_active : (y_next >= 24 && y_next < 216);
  const uint32_t next_buf =
      last_of_frame ? 0u : (line_real ? (display_buf_ ? 0u : 1u) : (display_buf_ ? 1u : 0u));
  const bool next_fresh = next_buf ? buf_fresh1 : buf_fresh0;

  // pixel lane from the CURRENT buffer content (reads only touch FULL
  // buffers — the never-torn law, spec §4)
  const uint64_t word = mem_[display_buf_ ? 1 : 0][(cur.x >> 2) & 127u];
  uint32_t px_buf;
  switch (cur.x & 3u) {
    case 0:
      px_buf = (uint32_t)(word & 0xFFFFull);
      break;
    case 1:
      px_buf = (uint32_t)((word >> 16) & 0xFFFFull);
      break;
    case 2:
      px_buf = (uint32_t)((word >> 32) & 0xFFFFull);
      break;
    default:
      px_buf = (uint32_t)((word >> 48) & 0xFFFFull);
      break;
  }

  if (px_valid && line_real && !line_fresh_) {
    starve_q_ = (starve_q_ == kSat64) ? starve_q_ : starve_q_ + 1;
  }
  if (px_valid && line_real && line_fresh_) {
    last_px_ = px_buf;
  }
  consume_start_[0] = consume_start_[1] = false;
  consume_done_[0] = consume_done_[1] = false;
  if (line_last) {
    if (line_real) {  // close the current real line (credit back)
      const uint32_t done_buf = display_buf_ ? 1u : 0u;
      display_buf_ = next_buf != 0u;
      if (line_fresh_) consume_done_[done_buf] = true;
    }
    if (last_of_frame) {
      display_buf_ = false;  // re-anchor at the wrap, even from vblank
    }
    if (next_real) {  // open the next real line (take freshness NOW)
      line_fresh_ = next_fresh;
      if (next_fresh) consume_start_[next_buf] = true;
    } else {
      line_fresh_ = false;  // border/vblank next
    }
  }

  // ---- SCANOUT vid: swap execution + boundary toggles ----
  swap_ack_ = swap_req;
  if (swap_req) display_slot_ = swap_slot;
  if (cur.vswap_dec) dec_tog_ = !dec_tog_;
  if (cur.frame_start) fs_tog_ = !fs_tog_;

  // ---- FRAMECTL decision ----
  framectl_.step(fctl_in_);
  fctl_out_reg_ = framectl_.out_comb(fctl_in_);

  // ---- VIDEO.MODE steps last (the raster view for the NEXT cycle) ----
  mode_.mode_we(in.mode_in, in.mode_we);
  mode_.step();
  raster_ = mode_.view();

  // serializer stream decode of the NEW cycle
  ser_px_ = decode_px_();
}

PxStream Scanout::decode_px_() const {
  // combinational serializer output of the post-edge state
  const RasterView& cur = raster_;
  const VidTiming& t = vid_timing(cur.mode);
  const bool line_active = cur.y < t.v_active;
  const bool line_real = (cur.mode != 2) ? line_active : (cur.y >= 24 && cur.y < 216);
  PxStream p;
  p.valid = line_active && !cur.hblank;
  p.rgb565 = !line_active  ? 0u
             : !line_real  ? 0u  // border colour 16'h0000 (spec §3.1)
             : line_fresh_ ? lane_pixel_(cur.x)
                           : last_px_;
  p.x = cur.x & 0x3FFu;  // zhao_px_stream_t port widths
  p.y = cur.y & 0xFFu;
  p.hsync = cur.hsync;
  p.vsync = cur.vsync;
  p.hblank = cur.hblank;
  p.vblank = cur.vblank;
  return p;
}

uint32_t Scanout::lane_pixel_(uint32_t x) const {
  const uint64_t word = mem_[display_buf_ ? 1 : 0][(x >> 2) & 127u];
  switch (x & 3u) {
    case 0:
      return (uint32_t)(word & 0xFFFFull);
    case 1:
      return (uint32_t)((word >> 16) & 0xFFFFull);
    case 2:
      return (uint32_t)((word >> 32) & 0xFFFFull);
    default:
      return (uint32_t)((word >> 48) & 0xFFFFull);
  }
}

// -------------------------------------------------------------- gpu side --
void Scanout::gpu_step_(const VideoSysIn& in, bool s_dec_tog, bool s_fs_tog, uint32_t s_slot,
                        uint32_t s_mnext, uint32_t s_mode, bool s_cons_tog0, bool s_cons_tog1,
                        bool s_ftog, uint32_t s_fslot, bool s_frep, uint32_t s_ffid) {
  // ---- FETCH combinational levels of THIS cycle (pre-edge regs) ----
  const SegGeom g = seg_geometry(fetch_mode_, fetch_slot_, fetch_line_, seg_idx_);
  const bool fill_we = (f_state_ == F_BEATS) && in.beat_valid;
  const uint32_t fill_buf = fill_line_buf_ ? 1u : 0u;
  const bool fill_we_ok = (bstate_[fill_buf] == LB_EMPTY) || (bstate_[fill_buf] == LB_FILLING);
  const bool fill_done_ok = (bstate_[fill_buf] == LB_FILLING);
  // completion/abort are combinational levels of the completing cycle
  // (they carry the CURRENT fill buffer — see zhao_scanout_fetch.sv)
  const bool beat_eob = (f_state_ == F_BEATS) && in.beat_valid && beat_cnt_ == 7;
  const bool seg_complete = beat_eob && (req_idx_ == g.reqs - 1);
  const bool line_complete = seg_complete && ((fetch_mode_ != 2) || (seg_idx_ == 1));
  const bool flush_now = dec_sync_ || (fs_sync_ && (mode_s2_ != fetch_mode_));
  const bool violation_now = (f_state_ == F_REQ) && in.guard_ready && in.guard_violation;
  const bool fill_done_lvl = line_complete;
  // frame flush discards BOTH buffers (per-frame re-alignment law); a
  // violation retries the line's own buffer
  const uint32_t abort_mask = flush_now ? 3u : violation_now ? (fill_line_buf_ ? 2u : 1u) : 0u;

  // ---- LINEBUF gpu next-state (from the levels above) ----
  uint32_t bstate_n[2] = {bstate_[0], bstate_[1]};
  bool full_tog_n[2] = {full_toggle_[0], full_toggle_[1]};
  if (fill_we && fill_we_ok) {
    mem_[fill_buf][fill_words_ & 127u] = in.beat_data;
  }
  for (uint32_t i = 0; i < 2; ++i) {
    const bool sel = (fill_buf == i);
    if ((abort_mask >> i) & 1u) {
      bstate_n[i] = LB_EMPTY;            // discard; a discarded
      if (bstate_[i] == LB_FULL) {       // FULL fill un-does its
        full_tog_n[i] = !full_tog_n[i];  // completion toggle
      }
    } else if (fill_done_lvl && sel && fill_done_ok) {
      bstate_n[i] = LB_FULL;
      full_tog_n[i] = !full_tog_n[i];
    } else if (fill_we && sel && fill_we_ok && bstate_[i] == LB_EMPTY) {
      bstate_n[i] = LB_FILLING;
    } else if (cons_s2_[i] != cons_s2q_[i]) {
      bstate_n[i] = LB_EMPTY;  // display credit
    }
  }

  // ---- FETCH next-state (reads dec_sync_ / fs_sync_ PRE-edge) ----
  uint32_t f_state_n = f_state_;
  uint32_t fetch_mode_n = fetch_mode_, fetch_slot_n = fetch_slot_, fetch_line_n = fetch_line_;
  uint32_t seg_idx_n = seg_idx_, req_idx_n = req_idx_, beat_cnt_n = beat_cnt_,
           fill_words_n = fill_words_;
  uint32_t fill_line_buf_n = fill_line_buf_;
  const bool buf_empty_now0 = bstate_[0] == LB_EMPTY;
  const bool buf_empty_now1 = bstate_[1] == LB_EMPTY;

  if (dec_sync_) {
    f_state_n = F_ARM;
    fetch_mode_n = mnext_s2_;
    fetch_slot_n = slot_s2_;
    fetch_line_n = 0;
    seg_idx_n = 0;
    req_idx_n = 0;
    beat_cnt_n = 0;
    fill_words_n = 0;
    fill_line_buf_n = 0;
  } else if (fs_sync_ && (mode_s2_ != fetch_mode_)) {
    f_state_n = F_ARM;
    fetch_mode_n = mode_s2_;
    fetch_line_n = 0;
    seg_idx_n = 0;
    req_idx_n = 0;
    beat_cnt_n = 0;
    fill_words_n = 0;
    fill_line_buf_n = 0;
  } else {
    switch (f_state_) {
      case F_ARM:
        fill_words_n = 0;
        seg_idx_n = 0;
        req_idx_n = 0;
        if (g.real) {
          f_state_n = F_WAIT;
        } else {
          fetch_line_n = g.next;
          f_state_n = F_ARM;
        }
        break;

      case F_WAIT:
        if (fill_line_buf_ ? buf_empty_now1 : buf_empty_now0) {
          f_state_n = F_REQ;
        }
        break;

      case F_REQ:
        if (in.guard_ready) {
          if (in.guard_violation) {
            f_state_n = F_ARM;
            seg_idx_n = 0;
            req_idx_n = 0;
            fill_words_n = 0;
          } else {
            f_state_n = F_BEATS;
            beat_cnt_n = 0;
          }
        }
        break;

      case F_BEATS:
        if (in.beat_valid) {
          fill_words_n = fill_words_ + 1;
          if (beat_cnt_ == 7) {
            beat_cnt_n = 0;
            if (req_idx_ == g.reqs - 1) {
              if ((fetch_mode_ != 2) || (seg_idx_ == 1)) {
                fill_line_buf_n = fill_line_buf_ ? 0u : 1u;
                req_idx_n = 0;
                seg_idx_n = 0;
                fetch_line_n = g.next;
                f_state_n = g.last ? F_PARK : F_ARM;
              } else {
                seg_idx_n = 1;
                req_idx_n = 0;
                f_state_n = F_REQ;
              }
            } else {
              req_idx_n = req_idx_ + 1;
              f_state_n = F_REQ;
            }
          } else {
            beat_cnt_n = beat_cnt_ + 1;
          }
        }
        break;

      case F_PARK:
      default:
        f_state_n = F_PARK;
        break;
    }
  }

  // ---- crossing chains (toggle+2FF+edge; data 2FF — pre-edge snapshots) --
  const bool dec_sync_n = (dec_s2_ != dec_s2q_);
  const bool fs_sync_n = (fs_s2_ != fs_s2q_);
  const bool f_pulse_n = (f_tog_s2_ != f_tog_s2q_);

  // ---- commit (all chains shift PRE-EDGE values — RTL NBA semantics) ----
  const bool dec_s1_o = dec_s1_, dec_s2_o = dec_s2_, dec_s2q_o = dec_s2q_;
  const bool fs_s1_o = fs_s1_, fs_s2_o = fs_s2_, fs_s2q_o = fs_s2q_;
  const uint32_t slot_s1_o = slot_s1_, mnext_s1_o = mnext_s1_, mode_s1_o = mode_s1_;
  dec_s1_ = s_dec_tog;
  dec_s2_ = dec_s1_o;
  dec_s2q_ = dec_s2_o;
  dec_sync_ = dec_sync_n;
  fs_s1_ = s_fs_tog;
  fs_s2_ = fs_s1_o;
  fs_s2q_ = fs_s2_o;
  fs_sync_ = fs_sync_n;
  slot_s1_ = s_slot;
  slot_s2_ = slot_s1_o;
  mnext_s1_ = s_mnext;
  mnext_s2_ = mnext_s1_o;
  mode_s1_ = s_mode;
  mode_s2_ = mode_s1_o;

  bstate_[0] = bstate_n[0];
  bstate_[1] = bstate_n[1];
  full_toggle_[0] = full_tog_n[0];
  full_toggle_[1] = full_tog_n[1];
  const bool cons_s1_o0 = cons_s1_[0], cons_s2_o0 = cons_s2_[0];
  const bool cons_s1_o1 = cons_s1_[1], cons_s2_o1 = cons_s2_[1];
  cons_s1_[0] = s_cons_tog0;
  cons_s1_[1] = s_cons_tog1;
  cons_s2_[0] = cons_s1_o0;
  cons_s2_[1] = cons_s1_o1;
  cons_s2q_[0] = cons_s2_o0;
  cons_s2q_[1] = cons_s2_o1;

  f_state_ = f_state_n;
  fetch_mode_ = fetch_mode_n;
  fetch_slot_ = fetch_slot_n;
  fetch_line_ = fetch_line_n;
  seg_idx_ = seg_idx_n;
  req_idx_ = req_idx_n;
  beat_cnt_ = beat_cnt_n;
  fill_words_ = fill_words_n;
  fill_line_buf_ = fill_line_buf_n;

  // FRAMECTL gpu broadcast (data captured on the pulse, pre-edge values)
  const uint32_t f_slot_s2_pre = f_slot_s1_;
  const bool f_tog_s1_o = f_tog_s1_, f_tog_s2_o = f_tog_s2_;
  const uint32_t f_slot_s1_o = f_slot_s1_;
  f_tog_s1_ = s_ftog;
  f_tog_s2_ = f_tog_s1_o;
  f_tog_s2q_ = f_tog_s2_o;
  f_slot_s1_ = s_fslot;
  f_slot_s2_ = f_slot_s1_o;
  gpu_tick_q_ = f_pulse_n;
  if (f_pulse_n) {
    gpu_tick_frame_id_q_ = s_ffid;
    gpu_tick_repeated_q_ = s_frep;
    gpu_complete_slot_q_ = f_slot_s2_pre;
  }
}

// ------------------------------------------------------------- observables -
VideoSysOut Scanout::out() const {
  VideoSysOut o;
  o.raster = raster_;

  // fetch request (combinational from the gpu-domain FSM state)
  const SegGeom g = seg_geometry(fetch_mode_, fetch_slot_, fetch_line_, seg_idx_);
  o.req_valid = (f_state_ == F_REQ);
  o.req_write = false;
  o.req_addr = (f_state_ == F_REQ) ? g.addr + req_idx_ * 64u : 0u;
  o.req_len = 64;

  o.px = scaler_.out();
  o.scaler_violation = scaler_.never_active();

  o.frame_tick = fctl_out_reg_.frame_tick;
  o.frame_repeated = fctl_out_reg_.frame_repeated;
  o.frame_id = fctl_out_reg_.frame_id;
  o.deadline_faults = fctl_out_reg_.deadline_faults;
  o.frame_cycles = fctl_out_reg_.frame_cycles;
  o.deadline_margin = fctl_out_reg_.deadline_margin;
  // combinational swap command of the CURRENT cycle (decode the CURRENT
  // raster against the CURRENT commit state — never the stale edge inputs)
  FrameCtlIn cin{};
  cin.vswap_dec = raster_.vswap_dec;
  o.swap_req = framectl_.out_comb(cin).swap_req;
  o.swap_slot = framectl_.out_comb(cin).swap_slot;
  o.swap_ack = swap_ack_;

  o.gpu_tick = gpu_tick_q_;
  o.gpu_tick_frame_id = gpu_tick_frame_id_q_;
  o.gpu_tick_repeated = gpu_tick_repeated_q_;
  o.gpu_complete_slot = gpu_complete_slot_q_;

  o.starvation = starve_q_;
  return o;
}

}  // namespace zref
