"""EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.

The sweep's own guards DISCARD a mutant that fails to build, which is correct
but late: it turns a broken mutation into a discard rather than into evidence,
and a run that discards a third of its mutants has not tested what it claims to.
Worse, before guard 5 existed a mutant that failed to COMPILE left the previous
binary in place and was scored as CAUGHT -- the most flattering possible way to
be wrong.

Three ways a Field mutant can be malformed and look fine in a diff, all of them
already found in this repository:

  * `W'sd0` where W is a parameter is a syntax error;
  * an always-false comparison fails -Wall;
  * a mutation that leaves a signal unread trips UNUSEDSIGNAL under -Wall.

The cone is linted with `zhao_field_seq` as top, because that is the composition
every mutated file actually lives in: a mutation inside `zhao_field_exec_shared`
or `zhao_field_mul` cannot be linted from the op unit alone.
"""

import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_field_dsp_mutants as M   # noqa: E402

CONE = [
    'fpga/rtl/field/zhao_field_seq.sv',
    'fpga/rtl/field/zhao_field_exec_shared.sv',
    'fpga/rtl/field/zhao_field_mul.sv',
    'fpga/rtl/field/zhao_field_alu.sv',
    'fpga/rtl/field/zhao_field_rcp.sv',
    'fpga/rtl/field/zhao_field_rcp_rom.sv',
    'fpga/rtl/field/zhao_field_sin.sv',
    'fpga/rtl/field/zhao_field_sin_rom.sv',
    'fpga/rtl/field/zhao_field_len.sv',
    'fpga/rtl/field/zhao_field_isqrt.sv',
    'fpga/rtl/field/zhao_field_normalize.sv',
    'fpga/rtl/field/zhao_field_rcp24_rom.sv',
    'fpga/rtl/field/zhao_field_noise.sv',
    'fpga/rtl/field/zhao_field_rot.sv',
    'fpga/rtl/field/zhao_field_ring.sv',
    'fpga/rtl/field/zhao_field_curve.sv',
]


def main():
    vr = os.environ['VERILATOR_ROOT']
    exe = vr + '/bin/verilator_bin.exe'
    if not os.path.exists(exe):
        exe = 'verilator_bin'

    gold = {}
    for f in M.files():
        gold[f] = io.open(f, encoding='utf-8', newline='').read()

    bad = []
    for k in range(len(M.MUTS)):
        name, path, _, _ = M.MUTS[k]
        for f, raw in gold.items():
            io.open(f, 'w', encoding='utf-8', newline='').write(raw)
        if not M.apply(k):
            bad.append((name, 'anchor'))
            continue
        rc = subprocess.run([exe, '--lint-only', '-Wall', '--top-module', 'zhao_field_seq'] + CONE,
                            capture_output=True, text=True)
        if rc.returncode != 0:
            lines = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if '%Error' in l or '%Warning' in l]
            bad.append((name, lines[0][:100] if lines else 'rc=%d' % rc.returncode))

    for f, raw in gold.items():
        io.open(f, 'w', encoding='utf-8', newline='').write(raw)

    print('linted %d mutants across %d files, %d do not build'
          % (len(M.MUTS), len(gold), len(bad)))
    for n, why in bad:
        print('   %-56s %s' % (n, why))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
