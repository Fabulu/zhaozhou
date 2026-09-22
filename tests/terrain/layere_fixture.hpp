// layere_fixture.hpp -- the PLAYED layer-E plane, in ONE place (tests only).
//
// Ruling R13 made the per-cell base material {matA, matB, weight} a
// per-TRIANGLE quantity read at tessellation. Several benches now have to (a)
// answer TERRAIN.TESS's `mat_*` read and (b) predict what should come out the
// far end of the pipe -- and those are the two halves of the same fixture.
//
// THEY LIVE HERE BECAUSE TWO COPIES OF A FIXTURE DRIFT, and a fixture that
// drifts TOWARD the RTL is the one defect a self-consistent test cannot see:
// the model and the expectation agree with each other and with nothing else,
// and the bench goes green while the block reads the wrong cell. One
// definition, every consumer.
//
// THREE DIFFERENT FUNCTIONS OF THE SAME CELL, with odd coefficients, so:
//   * adjacent cells differ in EVERY field -- an off-by-one cell is visible;
//   * the three fields are never equal -- a swapped matA/weight is visible;
//   * a subpatch spans many distinct triples -- "the block emitted one
//     material for the whole job" cannot pass.
// A constant, or one function used three times, hides all three.

#pragma once

#include <cstdint>

namespace tess_test {

inline uint8_t mat_a_at(int ci, int cj) { return static_cast<uint8_t>(5 * ci + 11 * cj + 0x21); }
inline uint8_t mat_b_at(int ci, int cj) { return static_cast<uint8_t>(13 * ci + 3 * cj + 0x8E); }
inline uint8_t weight_at(int ci, int cj) { return static_cast<uint8_t>(7 * ci + 9 * cj + 0x40); }

}  // namespace tess_test
