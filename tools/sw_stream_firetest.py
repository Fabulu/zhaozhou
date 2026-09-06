#!/usr/bin/env python3
"""Fire-test for tests/terrain/sw_stream_directed.cpp.

CLAUDE.md, 2026-09-04: "A detector that has not been shown to FIRE has not been
tested. Break it on purpose, watch the alarm go off, put it back."

Each perturbation below breaks ONE law the SW.STREAM reference model claims to
enforce, rebuilds, runs the directed suite, and records the exact failure text.
The header is restored from a pristine copy afterwards -- and the restore is
verified rather than assumed, because a fire-test that leaves the tree broken is
worse than none.

Usage:  python tools/sw_stream_firetest.py [--out FILE]
"""

import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HDR = os.path.join(REPO, "reference", "include", "zref", "zref_sw_stream.hpp")
SRC = os.path.join(REPO, "tests", "terrain", "sw_stream_directed.cpp")

# One law each. The comment on the right of every pair is the property the
# suite is claiming to hold; if breaking it changes nothing, the suite is not
# testing that property.
PERTURBATIONS = [
    (
        "P1  budget ignored: T7's 32-page ceiling is not enforced at all",
        "      if (spent >= budget_) {",
        "      if (false) {",
    ),
    (
        "P2  view union lost: the second view's bit OVERWRITES the first",
        "    e.view_mask = static_cast<uint8_t>(e.view_mask | view_bit);",
        "    e.view_mask = view_bit;",
    ),
    (
        "P3  canonical order: T5's required-before-prefetch key removed",
        "  if (ar != br) return ar < br;",
        "  if (ar != br) return false;",
    ),
    (
        "P4  staging: a CRC-mismatched page is accepted into the sealed list",
        "    if (actual != s.declared_crc32c) {",
        "    if (false && actual != s.declared_crc32c) {",
    ),
    (
        "P5  the seal is not a one-way door: mutation after sealing allowed",
        "    if (l.sealed) {",
        "    if (false) {",
    ),
    (
        "P6  dirty eviction never reaches the F journal",
        "    journal_.write(island::Streamer::resource_index("
        "dir_.desc().island_id, ix, iz), f_sheet);",
        "    (void)f_sheet;",
    ),
    (
        "P7  budget counts LIST ENTRIES, not transfers: a resident page spends one",
        "        r.hps_page_addr = staged_addr(k);\n"
        "        ++f.already_resident;",
        "        r.hps_page_addr = staged_addr(k);\n"
        "        ++spent;\n"
        "        ++f.already_resident;",
    ),
    (
        "P8  bounds validated AFTER staging instead of before (T12 inverted)",
        "    if (!mem::upload_source_in_arena(cartridge_, cartridge_.base + "
        "s.cart_offset, kPageBytes)) {",
        "    if (false) {",
    ),
    (
        "P9  the gameplay-starvation flag is never raised (the unruled case "
        "goes quiet)",
        "            f.unruled_gameplay_starvation = true;",
        "            f.unruled_gameplay_starvation = false;",
    ),
    (
        "P10 prefetch is never deferred: over-budget prefetch is sealed anyway",
        "          ++f.prefetch_deferred;\n          if (L) ++L->prefetch_deferred;",
        "          ++f.prefetch_deferred;\n          if (L) ++L->prefetch_deferred;\n"
        "          kept.push_back(r);",
    ),
]


def gxx():
    p = shutil.which("g++")
    if not p:
        sys.exit("g++ not on PATH; source tools/env/zhao-env.ps1 first")
    return p


def build_and_run(exe, out):
    b = subprocess.run(
        [gxx(), "-std=c++17", "-O1", "-I" + os.path.join(REPO, "reference", "include"),
         "-I" + os.path.join(REPO, "runtime", "include"), SRC, "-o", exe],
        capture_output=True, text=True, cwd=REPO)
    if b.returncode != 0:
        out.write("BUILD FAILED (rc %d)\n%s\n\n" % (b.returncode, b.stderr[:1500]))
        return None
    return subprocess.run([exe], capture_output=True, text=True, cwd=REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(REPO, "build", "sw_stream_firetest.txt"))
    args = ap.parse_args()

    pristine = io.open(HDR, encoding="utf-8").read()
    tmpdir = tempfile.mkdtemp(prefix="swstream_fire_")
    exe = os.path.join(tmpdir, "pert.exe")
    out = io.open(args.out, "w", encoding="utf-8", newline="\n")

    out.write("SW.STREAM directed suite -- proof that it can fail\n")
    out.write("=" * 74 + "\n\n")

    # The unperturbed baseline, so "N checks, ok" is on the record next to the
    # perturbed runs rather than remembered.
    r = build_and_run(exe, out)
    out.write("=" * 74 + "\nBASELINE (unperturbed)\n" + "=" * 74 + "\n")
    if r is not None:
        out.write("exit %d\n" % r.returncode)
        for line in r.stdout.split("\n"):
            if line.strip():
                out.write("   | " + line.rstrip() + "\n")
    out.write("\n")

    failures_seen = 0
    try:
        for name, old, new in PERTURBATIONS:
            if old not in pristine:
                out.write("=" * 74 + "\n" + name + "\n" + "=" * 74 + "\n")
                out.write("ANCHOR NOT FOUND -- this perturbation is stale and "
                          "proves nothing. Fix it.\n\n")
                failures_seen += 1  # a stale fire-test is itself a defect
                continue
            io.open(HDR, "w", encoding="utf-8", newline="\n").write(
                pristine.replace(old, new, 1))
            r = build_and_run(exe, out)
            out.write("=" * 74 + "\n" + name + "\n" + "=" * 74 + "\n")
            if r is None:
                continue
            lines = r.stdout.split("\n")
            fired = [l for l in lines if l.startswith("FAIL:")]
            out.write("exit %d, %d checks fired\n" % (r.returncode, len(fired)))
            if r.returncode == 0:
                out.write("  *** NOTHING FIRED. The suite cannot see this "
                          "defect. ***\n")
            for l in fired:
                out.write("  " + l + "\n")
            for l in lines:
                if "checks," in l or "frames:" in l or "ledger:" in l:
                    out.write("   | " + l.strip() + "\n")
            out.write("\n")
    finally:
        io.open(HDR, "w", encoding="utf-8", newline="\n").write(pristine)
        restored = io.open(HDR, encoding="utf-8").read()
        out.write("=" * 74 + "\nRESTORE VERIFIED: %s\n" % (
            "byte-identical" if restored == pristine else "*** DIFFERS ***"))
        out.close()
        shutil.rmtree(tmpdir, ignore_errors=True)

    print(io.open(args.out, encoding="utf-8").read())
    return 0


if __name__ == "__main__":
    sys.exit(main())
