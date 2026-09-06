#!/usr/bin/env python3
"""Prove every check in raster_rcp24_v3_directed can actually FAIL.

CLAUDE.md: "A detector that has not been shown to FIRE has not been tested.
Break it on purpose, watch the alarm go off, put it back."

Each mutation below is a single exact string replacement in one RTL file. The
script applies it, rebuilds ONLY the one test target, runs it, records the exact
failure text, and restores the file byte-for-byte from a saved copy -- restored
from the saved bytes rather than by reversing the replacement, so a mutation that
matched twice cannot leave residue behind.

Usage:  python fire_tests.py [only_id]
"""
import subprocess, sys, os, json, shutil, io

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
MUL = 'fpga/rtl/raster/zhao_raster_rcp24_mul.sv'
V3 = 'fpga/rtl/raster/zhao_raster_rcp24_v3.sv'

MUTATIONS = [
    # F1 IS THE ONE THAT MATTERS. Deleting the high-word correction is exactly
    # the failure a denominator-driven pair test cannot see, because the
    # reciprocal's own domain never reaches w > 2^31.
    ('F1-drop-negative-correction', MUL,
     '      c_hi_q  <= hi_corrected_c;',
     '      c_hi_q  <= h_high_q;'),
    # F2: the round-half-up carry out of bit 29, which S10.6 proves is the only
    # carry that can survive.
    ('F2-drop-rounding-carry', MUL,
     "      x_next_o <= {c_hi_q[29:0], c_low_q[31:30]} + {31'd0, c_low_q[29]};",
     '      x_next_o <= {c_hi_q[29:0], c_low_q[31:30]};'),
    # F3: the MW extraction shifted by one place.
    ('F3-mw-shift-off-by-one', MUL,
     '      w_next_o <= {c_hi_q[23:0], c_low_q[31:24]};',
     '      w_next_o <= {c_hi_q[22:0], c_low_q[31:23]};'),
    # F4: the carry the tiled recombination hands from the low half to the high.
    ('F4-drop-low-to-high-carry', MUL,
     "  assign high_sum_c = {1'b0, l_p11_q} + {16'd0, l_crosshi_q} + {32'd0, l_carry_q};",
     "  assign high_sum_c = {1'b0, l_p11_q} + {16'd0, l_crosshi_q};"),
    # F5: S10.2's "Phase MW0 ignores old scratch and initializes the whole
    # scratch row through its ordinary writeback" -- made to read stale scratch.
    ('F5-mw0-reads-stale-scratch', V3,
     '        s2_x_q   <= (s1_ph_q == PH_MW0) ? p_x0_q[s1_ctx_q] : s_x_q[s1_ctx_q];',
     '        s2_x_q   <= s_x_q[s1_ctx_q];'),
    # F7: set the 33rd bit of the high sum WITHOUT disturbing bits [31:0], so
    # the product and both extractions stay correct and the ONLY thing that can
    # notice is the a_high_carry_never_set assertion. If that assertion were
    # decorative, this mutant would pass every one of the 56 checks.
    ('F7-force-high-carry-bit', MUL,
     "  assign high_sum_c = {1'b0, l_p11_q} + {16'd0, l_crosshi_q} + {32'd0, l_carry_q};",
     "  assign high_sum_c = {1'b0, l_p11_q} + {16'd0, l_crosshi_q} + {32'd0, l_carry_q} + 33'h1_0000_0000;"),
    # F6: skip the second Newton MW, which must break BOTH the results and the
    # four-launches-per-reciprocal counter contract.
    ('F6-skip-mw1-phase', V3,
     '      PH_MX0:  cont_din = {PH_MW1, w_ctx_c};',
     '      PH_MX0:  cont_din = {PH_MX1, w_ctx_c};'),
]


# The test links against the dsstuff mingw64 g++, whose runtime DLLs are not on
# the default PATH. Without this the mutant exits 0xC0000139 (entrypoint not
# found) and would be scored BUILD-FAILED -- a mutant that never ran, recorded
# as evidence.
#
# FORWARD SLASHES ON PURPOSE. The first version of this line was written through
# a shell heredoc and its bin path arrived in the file as a literal 0x08 byte --
# the exact accident CLAUDE.md records, a word-boundary escape that a shell
# heredoc turned into a literal backspace character. Windows accepts a forward
# slash in a PATH entry, so there is nothing left here for a heredoc to eat.
os.environ['PATH'] = 'C:/Programmieren/dsstuff/mingw64/bin' + os.pathsep + os.environ.get('PATH', '')


def run(cmd, cwd=REPO):
    p = subprocess.run(cmd, cwd=cwd, shell=True, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or '')


HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = 'powershell -NoProfile -ExecutionPolicy Bypass -File "%s"' % os.path.join(HERE, 'rebuild.ps1')
EXE = os.path.join(REPO, 'build-rcp24v3-quick', 'test_rcp24v3.exe')


def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    results = []
    for mid, relpath, old, new in MUTATIONS:
        if only and only != mid:
            continue
        path = os.path.join(REPO, relpath)
        saved = path + '.firebak'
        shutil.copyfile(path, saved)
        try:
            s = io.open(path, encoding='utf-8').read()
            if s.count(old) != 1:
                results.append({'id': mid, 'status': 'ANCHOR-MISS',
                                'occurrences': s.count(old)})
                continue
            io.open(path, 'w', encoding='utf-8', newline='\n').write(s.replace(old, new))
            rc, out = run(BUILD)
            if rc != 0:
                # A mutant that does not compile is NOT a caught mutant.
                results.append({'id': mid, 'status': 'BUILD-FAILED',
                                'text': out[-1500:]})
                continue
            rc, out = run('"%s"' % EXE)
            fails = [l for l in out.splitlines()
                     if 'FAIL' in l or 'MISMATCH' in l.upper()
                     or 'Assertion failed' in l or 'rcp24_mul:' in l]
            results.append({'id': mid, 'status': 'FIRED' if rc != 0 else 'SILENT',
                            'exit': rc, 'failures': fails[:12],
                            'failure_count': len(fails)})
        finally:
            shutil.copyfile(saved, path)
            os.remove(saved)
    # Restore the binary to the unmutated source so a later run is not stale.
    run(BUILD)
    print(json.dumps(results, indent=1))


if __name__ == '__main__':
    main()
