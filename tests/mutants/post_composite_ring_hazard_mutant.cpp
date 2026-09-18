// post_composite_ring_hazard_mutant.cpp -- the INVERTED-POLARITY driver for
// zhao_post_composite's ring hazard detector.
//
// ---------------------------------------------------------------------------
// THIS TEST PASSES WHEN THE COUNTER FIRES
// ---------------------------------------------------------------------------
// `ring_hazard_o` is asserted ZERO by post_composite_directed, on every frame
// it runs. That is a claim about an instrument, and a detector that has not
// been shown to FIRE has not been tested. No legal stimulus can move it,
// because the state it watches for is unreachable while the output lag is
// correct -- so the only demonstration is to break the lag, and the break is a
// COMMITTED WRAPPER rather than a temporary edit to production RTL.
//
// tests/mutants/zhao_post_composite_ring_hazard_mutant.sv instantiates the real
// `zhao_post_composite` with LAG_PX = 0 and changes nothing else. At LAG_PX = 0
// the write pointer leads the ring read by two columns instead of eleven, so a
// sample at dx = +8, dy = +4 reaches past it and the counter must move.
//
// It is evidence about the instrument, not about the design. It is also the
// only place the "small output-line delay queue" is shown to be load-bearing
// rather than decorative.
//
// NOTE ON WHAT IS ASSERTED. This does NOT assert the bug -- there is no bug to
// assert. It asserts that a DELIBERATELY MISCONFIGURED instance trips the
// detector, and separately that the shipping configuration's behaviour is
// unaffected, which is post_composite_directed's job. Those are two different
// claims and they live in two different files on purpose.
//
// No simulation assertion had to be disabled: the module contains none.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_post_composite_ring_hazard_mutant.h"

#include "post_composite_dev.hpp"
#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_post_composite_ring_hazard_mutant;

  // A uniform displacement at the full R5 bound in BOTH axes: +8 horizontally
  // is the reach the nine-pixel lead exists to cover, and +4 vertically puts
  // the sample on the line the write pointer is currently filling.
  pc::Frame f = pc::base_frame();
  for (int i = 0; i < pc::CW * pc::CH; ++i) { f.dx[i] = 8; f.dy[i] = 4; }
  pc::Cfg c;

  pc::reset_dut(top);
  pc::load_pv_table(top, c);
  const pc::Result r = pc::run_frame(top, f, c);

  std::printf("  mutant (LAG_PX = 0): ring_hazard_o = %u over %d pixels\n",
              r.hazard, r.emitted);

  // INVERTED POLARITY. Green here means the detector works.
  zhao::check(r.hazard > 0u,
              "with the output delay queue removed (LAG_PX = 0), a displaced "
              "sample reaches past the ring write pointer and ring_hazard_o "
              "FIRES -- which is what makes the zero it reads in production a "
              "result rather than a hope",
              1, r.hazard > 0u ? 1 : 0);

  const int rc = zhao::report_and_exit("post_composite_ring_hazard_mutant");
  delete top;
  zhao::exit_hard(rc);
}
