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
import time

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

# THE v2 CONE, which this file did not have and needed the moment v2 existed.
# zhao_field_v2_core.sv and zhao_field_v2_lanemux.sv are in the MUTANT list but
# were in NO cone, so every v2 mutant was linted against a composition that does
# not contain it: the lint passed by not looking. A mutant in a file the cone
# omits is a mutant with no preflight at all.
CONE_V2 = [
    'fpga/rtl/field/zhao_field_v2_core.sv',
    'fpga/rtl/field/zhao_field_v2_lanemux.sv',
    'fpga/rtl/field/zhao_field_curve.sv',
    'fpga/rtl/field/zhao_field_len.sv',
    'fpga/rtl/field/zhao_field_isqrt.sv',
    'fpga/rtl/field/zhao_field_ring.sv',
    'fpga/rtl/field/zhao_field_rcp.sv',
    'fpga/rtl/field/zhao_field_rcp_rom.sv',
    'fpga/rtl/field/zhao_field_noise.sv',
    'fpga/rtl/field/zhao_field_rot.sv',
    'fpga/rtl/field/zhao_field_sin.sv',
    'fpga/rtl/field/zhao_field_sin_rom.sv',
    'fpga/rtl/field/zhao_field_normalize.sv',
    'fpga/rtl/field/zhao_field_rcp24_rom.sv',
    'fpga/rtl/field/zhao_field_mul.sv',
]
V2_FILES = ('zhao_field_v2_core.sv', 'zhao_field_v2_lanemux.sv')


def main():
    vr = os.environ['VERILATOR_ROOT']
    exe = vr + '/bin/verilator_bin.exe'
    if not os.path.exists(exe):
        exe = 'verilator_bin'

    gold = {}
    for f in M.files():
        gold[f] = io.open(f, encoding='utf-8', newline='').read()

    def restore(gold):
        """Write every file back and PROVE it took.

        The old code wrote and walked on. That assumes a write is immediately
        visible to the next read, and on this platform it intermittently is not
        -- runs of this preflight failed on M80 once and on M84/M92 the next
        time, with anchors that were present in the file the whole while. The
        sweep already knew: its own restore() reads back, compares and retries
        up to ten times. This is that, and for the same reason.

        A restore that cannot be proven is fatal. Continuing would lint mutant
        k against mutant k-1's source and call the result evidence.
        """
        for _ in range(10):
            ok = True
            for f, raw in gold.items():
                io.open(f, 'w', encoding='utf-8', newline='').write(raw)
            for f, raw in gold.items():
                if io.open(f, encoding='utf-8', newline='').read() != raw:
                    ok = False
            if ok:
                return True
            time.sleep(1)
        return False

    # ---- THE BASELINE MUST LINT BEFORE ANY MUTANT IS JUDGED ---------------
    # `gold` is captured from the WORKING TREE, and a working tree is not
    # necessarily pristine: a sweep killed mid-iteration leaves a mutation in
    # it. On 2026-08-26 that happened, gold captured a mutated baseline, and
    # this file reported 32 of 111 mutants as broken -- every one of them a
    # signal orphaned by the STUCK mutation rather than by the mutant under
    # test. Thirty-two false failures are worse than none, because they read
    # like a bad mutant list and send you editing correct mutants.
    #
    # So: lint the baseline first. If the tree is dirty, say so and stop.
    for top, cone in (('zhao_field_seq', CONE), ('zhao_field_v2_core', CONE_V2)):
        rc = subprocess.run([exe, '--lint-only', '-Wall', '--top-module', top] + cone,
                            capture_output=True, text=True)
        if rc.returncode != 0:
            sys.stderr.write('ABORT: the BASELINE does not lint with top %s.\n' % top)
            sys.stderr.write('       Nothing below would mean anything. The likeliest\n')
            sys.stderr.write('       cause is a mutation stranded by a killed sweep:\n')
            sys.stderr.write('         git diff --stat fpga/rtl/field/\n')
            for l in (rc.stdout + rc.stderr).splitlines():
                if '%Error' in l or '%Warning' in l:
                    sys.stderr.write('       ' + l[:160] + '\n')
            return 3

    # ---- A SUBSET RUN LINTS ITS SUBSET ------------------------------------
    # Reading the same SWEEP_ONLY the sweep reads, so the two cannot disagree
    # about which mutants a run covers. Linting all 134 for a 3-mutant re-score
    # costs about ten minutes to check three things, and re-scoring after a fix
    # is the most frequent operation in this loop -- that is the cost that
    # compounds.
    #
    # The BASELINE check above is NOT subject to this: it runs every time,
    # because a dirty tree invalidates every mutant whether or not it was
    # selected.
    only = os.environ.get('SWEEP_ONLY', '').split()
    idx = [int(x) for x in only] if only else list(range(len(M.MUTS)))
    if only:
        sys.stderr.write('preflight: SUBSET, %d of %d mutants\n' % (len(idx), len(M.MUTS)))

    bad = []
    for k in idx:
        name, path, _, _ = M.MUTS[k]
        if not restore(gold):
            sys.stderr.write('ABORT: could not restore before %s\n' % name)
            return 2
        if not M.apply(k):
            bad.append((name, 'anchor'))
            continue
        if path.replace(chr(92), '/').split('/')[-1] in V2_FILES:
            top, cone = 'zhao_field_v2_core', CONE_V2
        else:
            top, cone = 'zhao_field_seq', CONE
        rc = subprocess.run([exe, '--lint-only', '-Wall', '--top-module', top] + cone,
                            capture_output=True, text=True)
        if rc.returncode != 0:
            lines = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if '%Error' in l or '%Warning' in l]
            bad.append((name, lines[0][:100] if lines else 'rc=%d' % rc.returncode))

    if not restore(gold):
        sys.stderr.write('ABORT: final restore could not be proven\n')
        return 2

    print('linted %d mutants across %d files, %d do not build'
          % (len(idx), len(gold), len(bad)))
    for n, why in bad:
        print('   %-56s %s' % (n, why))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
