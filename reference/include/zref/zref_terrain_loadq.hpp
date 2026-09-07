// zref_terrain_loadq.hpp -- TERRAIN.LOADQ, the load queue between the terrain
// sequencer and the page loader.
//
// Law: design/contracts/TERRAIN.LOADQ.md
//      reports/OWNER-RULINGS-BUILDABILITY-20260902.md  T4, T6, T7
//
// ===========================================================================
// WHAT THIS MODEL OWNS, AND THE MUCH LARGER PART IT DOES NOT
// ===========================================================================
// The block is a FIFO. That is not a disclaimer, it is the specification: the
// only reason a queue rather than a scheduler sits between TERRAIN.SEQ and
// TERRAIN.PAGELOADER is that T4's writeback-before-load barrier lives in the
// sequencer and cannot be seen from here. A structure that could reorder a
// load ahead of the writeback guarding it would break a law it does not know
// about. So ORDER is the property, and order is what this model exists to
// pin.
//
// Three laws, and they are all of them:
//
//   ORDER         jobs leave in the order they arrived, always, with no
//                 merging, no priority and no dropping.
//   CAPACITY      at most DEPTH jobs live in the store and one more waits at
//                 the output port, so DEPTH + 1 are held. T7 permits 32 pages
//                 per frame and the depth is 32 for exactly that reason -- see
//                 the measurement in the RTL header, and the paragraph it
//                 replaced.
//   CONSERVATION  accepted == issued + drained + held. A job is never
//                 invented and never quietly lost.
//
// IT DOES NOT MODEL TIME. The RTL serialises each job into eight 40-bit words
// so that thirty-two of them fit in one M10K instead of 7,776 flops, which
// costs eight cycles in and nine out. That is a storage decision, not a
// behavioural one: no observer outside the block can tell it from a wide
// register file except by counting cycles, and the loader spends 6,726 cycles
// per page. A model that mirrored the serialiser would be a second copy of an
// implementation detail, and its first divergence would be a test failing for
// a reason that is not a fault.
//
// So the bench drives both and compares WHAT came out and IN WHAT ORDER, never
// WHEN. `held()` is the model's answer to `inflight_o` and covers everything
// the block has taken and not yet handed on, wherever it physically sits.

#ifndef ZREF_TERRAIN_LOADQ_HPP
#define ZREF_TERRAIN_LOADQ_HPP

#include <cstdint>
#include <deque>

namespace zref {
namespace terrain {

// The job, field for field as the seam carries it. Not a redefinition of the
// patch record: this is what TERRAIN.SEQ hands over after it has decided a
// load is due, and the fields are exactly the ten wires on `ld_*`.
struct LoadJob {
  uint32_t slot = 0;
  uint8_t  gen = 0;
  uint32_t epoch = 0;
  uint32_t island = 0;
  int16_t  ix = 0;
  int16_t  iz = 0;
  uint64_t hps_addr = 0;
  uint32_t expect_crc = 0;
  uint32_t src_id = 0;

  bool operator==(const LoadJob& o) const {
    return slot == o.slot && gen == o.gen && epoch == o.epoch &&
           island == o.island && ix == o.ix && iz == o.iz &&
           hps_addr == o.hps_addr && expect_crc == o.expect_crc &&
           src_id == o.src_id;
  }
  bool operator!=(const LoadJob& o) const { return !(*this == o); }
};

class LoadQueue {
 public:
  explicit LoadQueue(unsigned depth = 32) : depth_(depth) {}

  // CAPACITY IS DEPTH + 1, AND THE ONE IS NOT A FUDGE. `depth_` is the size of
  // the M10K store -- the number T7's 32-page budget speaks to and the only one
  // worth tuning. The block additionally presents one job at its output port,
  // deserialised and waiting for the loader to take it, and that job has left
  // the store. So the number it can be HOLDING is one more than the number it
  // can STORE.
  //
  // This was found by the bench rather than reasoned out: the model capped at
  // 32 while the RTL accepted a 33rd, and every job after that compared one
  // position out of step. A model that quietly used depth+1 with no explanation
  // would have hidden a genuine seam fact behind an off-by-one that looked like
  // arithmetic.
  //
  // The RTL additionally refuses while its write serialiser is busy, which this
  // model does not represent -- see the header. A bench must therefore drive
  // the RTL's `j_ready_o` and offer the model the same job on the cycles the
  // RTL actually accepts, not offer both blind.
  bool can_accept() const { return q_.size() < depth_ + 1u; }

  // What the store alone can hold: DEPTH. `capacity()` is what the block can
  // hold: DEPTH + 1.
  unsigned capacity() const { return depth_ + 1u; }

  // Returns false if it could not take the job. A refusal is COUNTED because
  // the sequencer is told by ready and should not be offering; a non-zero
  // refusal count means a caller ignored the handshake, not that the queue is
  // too small.
  bool push(const LoadJob& j) {
    if (!can_accept()) {
      ++refused_;
      return false;
    }
    q_.push_back(j);
    ++accepted_;
    if (q_.size() > high_water_) high_water_ = static_cast<unsigned>(q_.size());
    return true;
  }

  bool has_job() const { return !q_.empty(); }

  // The front job, and it leaves. FIFO with no exceptions: this is the whole
  // ordering law in one line.
  LoadJob pop() {
    LoadJob j = q_.front();
    q_.pop_front();
    ++issued_;
    return j;
  }

  const LoadJob& front() const { return q_.front(); }

  // T6's frame fault abandons the rest of the list, and queued jobs belong to
  // that list. Whether a fault SHOULD abandon them is an owner ruling nobody
  // has made -- this is the mechanism, and the count is what makes an emptied
  // queue distinguishable from one that was never filled.
  unsigned drain() {
    const unsigned n = static_cast<unsigned>(q_.size());
    q_.clear();
    drained_ += n;
    return n;
  }

  unsigned held() const { return static_cast<unsigned>(q_.size()); }
  unsigned depth() const { return depth_; }
  uint32_t accepted() const { return accepted_; }
  uint32_t issued() const { return issued_; }
  uint32_t drained() const { return drained_; }
  uint32_t refused() const { return refused_; }
  unsigned high_water() const { return high_water_; }

  // accepted == issued + drained + held, always. Stated as a function rather
  // than left to each bench so that "nothing was lost" is one claim with one
  // implementation.
  bool conserved() const {
    return accepted_ == issued_ + drained_ + static_cast<uint32_t>(q_.size());
  }

 private:
  unsigned depth_;
  std::deque<LoadJob> q_;
  uint32_t accepted_ = 0;
  uint32_t issued_ = 0;
  uint32_t drained_ = 0;
  uint32_t refused_ = 0;
  unsigned high_water_ = 0;
};

}  // namespace terrain
}  // namespace zref

#endif  // ZREF_TERRAIN_LOADQ_HPP
