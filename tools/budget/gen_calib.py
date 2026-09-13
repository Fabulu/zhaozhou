#!/usr/bin/env python3
"""gen_calib.py -- generate the Quartus calibration microbenches.

WHY
===
`design/budgets/dsp.md` was corrected on 2026-08-23 because its rule was
half wrong.  The measured half stands -- Quartus 17.0.2 Lite packs nothing, so
DSP count tracks OPERATOR count -- but the qualifier "whatever the operand
widths" does not survive `reports/QUARTUS_GOTCHAS.md` 5, which records the
SAME `zhao_geom_lod` source costing **28 DSPs at 72-bit operands and 18 at
64-bit**.  Width changes the cost, discontinuously.

So the corrected rule says operator count is a **LOWER BOUND**, and leaves the
audit with no way to turn "three signed 33x32 products, input and output
registered" into a number.  This generates the modules that answer that
question on **this exact tool and this exact device**, rather than from a
datasheet or from memory.

WHAT IT GENERATES
=================
multiply grid
  widths 8/9/16/18/19/24/26/27/28/31/32/33/40/48/64, signed and unsigned, at 1
  and 4 operators, input+output registered.  The 1-vs-4 pair is the PACKING
  test: if four operators cost four times one, Lite packs nothing at that
  width, which is what the corrected rule claims and what this either confirms
  or refutes.  The 26/27/28 cluster locates the cliff -- the design-derived
  curve in reports/BUDGET_HEATMAP.md shows 21- and 23-bit products taking ONE
  DSP block in the tool's `Independent 27x27` mode and 32-bit products taking
  THREE, so the interesting boundary is between 27 and 32, not at 18.

register grid
  widths 18/32/64 signed, one operator, combinational vs input-registered vs
  input+output-registered.  A DSP block's own input and output registers are
  free only if the RTL puts registers where the block expects them; this says
  whether that is happening.

widening-idiom control
  `{{32{a[31]}}, a} * {{32{b[31]}}, b}` -- the SystemVerilog idiom for a
  widening signed product -- against a plain `signed 32 x signed 32`.  This
  exists because the first draft of `scan_rtl.py` flagged the idiom as
  extension slack, and the claim that Quartus folds it away needs a measurement
  rather than an inference from one module's total.

RAM grid
  sync vs async read, reset vs no reset, four depth/width shapes, one and two
  ports, plus byte enables.  `blockMemoryBits > 0` is the only evidence an
  array became a memory; `zhao_field_seq` reports zero while spending 8,901
  ALMs, and `Allow Any RAM Size For Recognition` is DISABLED in this project's
  settings, so inference cannot be assumed anywhere.

Each module has REGISTERED PORTS wherever the point is not about registering,
so the fitter sees a real boundary rather than several hundred virtual pins
feeding raw combinational logic.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import secrets
import shutil
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTDIR = os.path.join(REPO, "build-budget", "calib")

# 26/27/28 were added after the design-derived cost curve showed the boundary
# is NOT at 18. zhao_geom_setup's 21-bit and zhao_raster_edgewalk's 23-bit
# products each take ONE DSP block and the tool's own decomposition calls them
# `Independent 27x27` -- so Cyclone V has a 27x27 mode and the interesting
# cliff is between 27 and 32, not between 18 and 19. Sampling only 24 and 31
# would have bracketed it without locating it.
WIDTHS = [8, 9, 16, 18, 19, 24, 26, 27, 28, 31, 32, 33, 40, 48, 64]
REG_WIDTHS = [18, 32, 64]


# ---------------------------------------------------------------------------
# DUAL-18 PHYSICAL-PACK DISCRIMINATOR
#
# These are isolated Quartus MapOnly projects rather than ordinary one-source
# calibration points: three revisions need the explicit wrapper and exactly one
# synthesis macro, while the inferred contrast must include neither.  Emitting
# each revision's effective source/macro receipt here keeps that distinction
# machine-checkable without changing the shared Quartus runner.
# ---------------------------------------------------------------------------
DUAL18_DEVICE = "5CSEBA6U23I7"
DUAL18_VENDOR_MACRO = "ZHAO_DUAL18_CYCLONEV=1"
DUAL18_TOP_SOURCE = "tests/rtl/dual18_physical_pack_discriminator.sv"
DUAL18_WRAPPER_SOURCE = "fpga/rtl/common/zhao_dual18_mul.sv"
DUAL18_MUTANT_SOURCE = "tests/mutants/dual18_two_primitives_mutant.sv"

DUAL18_REVISIONS = [
    {
        "revision": "dual18_inferred_pair",
        "top": "dual18_inferred_pair",
        "sources": [DUAL18_TOP_SOURCE],
        "macros": [],
        "expectedDspBlocks": [1, 2],
        "note": "inferred contrast: two is expected; one is useful good news, not failure",
    },
    {
        "revision": "dual18_explicit_pair",
        "top": "dual18_explicit_pair",
        "sources": [DUAL18_WRAPPER_SOURCE, DUAL18_TOP_SOURCE],
        "macros": [DUAL18_VENDOR_MACRO],
        "expectedDspBlocks": [1],
    },
    {
        "revision": "dual18_s32x18_exact",
        "top": "dual18_s32x18_exact",
        "sources": [DUAL18_WRAPPER_SOURCE, DUAL18_TOP_SOURCE],
        "macros": [DUAL18_VENDOR_MACRO],
        "expectedDspBlocks": [1],
    },
    {
        "revision": "dual18_two_primitives_mutant",
        "top": "dual18_two_primitives_mutant",
        "sources": [DUAL18_WRAPPER_SOURCE, DUAL18_MUTANT_SOURCE],
        "macros": [DUAL18_VENDOR_MACRO],
        "expectedDspBlocks": [2],
        "positiveControl": "the one-DSP map detector must reject this revision",
    },
]


def _sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


DUAL18_VENDOR_EVIDENCE_SPECS = [
    {
        "kind": "atom-declaration",
        "path": r"C:\intelFPGA_lite\17.0\quartus\eda\sim_lib\cyclonev_atoms.v",
        "requiredPatterns": [
            r"(?m)^module\s+cyclonev_mac\s*\(",
            r'(?m)^parameter\s+operation_mode\s*=\s*"[^"]+"\s*;',
            r"(?m)^parameter\s+signed_max\s*=.*$",
            r"(?m)^parameter\s+signed_may\s*=.*$",
            r"(?m)^parameter\s+signed_mbx\s*=.*$",
            r"(?m)^parameter\s+signed_mby\s*=.*$",
            r"(?m)^output\s*\[result_a_width-1\s*:\s*0\]\s*resulta\s*;",
            r"(?m)^output\s*\[result_b_width-1\s*:\s*0\]\s*resultb\s*;",
        ],
    },
    {
        "kind": "xml-metadata",
        "path": r"C:\intelFPGA_lite\17.0\quartus\libraries\megafunctions\xml_info\cyclonev_mac_info.xml",
        "requiredPatterns": [
            r'(?m)^.*<PARAMETER NAME="OPERATION_MODE"[^\n]*m18x18_full[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="RESULT_A_WIDTH"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="RESULT_B_WIDTH"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MAX"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MAY"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MBX"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MBY"[^\n]*$',
            r'(?m)^.*<PORT NAME="ax"[^\n]*$',
            r'(?m)^.*<PORT NAME="ay"[^\n]*$',
            r'(?m)^.*<PORT NAME="bx"[^\n]*$',
            r'(?m)^.*<PORT NAME="by"[^\n]*$',
            r'(?m)^.*<PORT NAME="resulta"[^\n]*$',
            r'(?m)^.*<PORT NAME="resultb"[^\n]*$',
        ],
    },
]


def _dual18_vendor_evidence():
    """Installed Quartus-17 interface/metadata provenance for the candidate.

    A missing installation does not prevent generation on CI or a review host;
    it is recorded as unavailable and remains a named gate.  On the calibration
    machine, hashes bind the receipt to the exact canonical declaration/XML and
    every required excerpt is positively matched before being retained.
    """
    evidence = []
    for item in DUAL18_VENDOR_EVIDENCE_SPECS:
        rec = {"kind": item["kind"], "path": item["path"]}
        if os.path.isfile(item["path"]):
            with open(item["path"], "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            search_text = text
            if item["kind"] == "atom-declaration":
                scope = re.search(
                    r"(?ms)^module\s+cyclonev_mac\s*\(.*?^endmodule\s*//cyclonev_mac\s*$",
                    text,
                )
                if not scope:
                    raise RuntimeError("installed atom declaration has no complete cyclonev_mac module")
                search_text = scope.group(0)
            excerpts = []
            for pattern in item["requiredPatterns"]:
                match = re.search(pattern, search_text)
                if not match:
                    raise RuntimeError("installed %s misses required pattern %r" %
                                       (item["kind"], pattern))
                excerpts.append(match.group(0).strip())
            rec["available"] = True
            rec["sha256"] = _sha256_file(item["path"])
            rec["matchedExcerpts"] = excerpts
        else:
            rec["available"] = False
            rec["requiredPatterns"] = list(item["requiredPatterns"])
        evidence.append(rec)
    return evidence


def emit_dual18_map_revisions(outdir):
    """Emit four fresh, content-addressed independent MapOnly projects."""
    root = os.path.join(outdir, "dual18")
    anchor_path = os.path.join(outdir, "dual18_invocation_anchor.json")
    # The anchor is an orchestration-owned trust root outside the replaceable
    # candidate tree.  Remove it first so failed regeneration cannot leave an
    # old anchor beside a partly new dual18 directory.
    if os.path.isfile(anchor_path):
        os.remove(anchor_path)
    # Regeneration is a new run boundary.  Keeping an old output_files tree here
    # would let yesterday's report sit beside today's receipt and look current.
    # This directory contains only these generated calibration artifacts.
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    emitted = []
    vendor_evidence = _dual18_vendor_evidence()

    for spec in DUAL18_REVISIONS:
        base_revision = spec["revision"]
        source_rows = []
        for rel in spec["sources"]:
            absolute = os.path.abspath(os.path.join(REPO, rel))
            if not os.path.isfile(absolute):
                raise FileNotFoundError("dual18 calibration source is missing: %s" % absolute)
            source_rows.append({
                "path": rel,
                "absolutePath": absolute.replace(os.sep, "/"),
                "sha256": _sha256_file(absolute),
            })

        source_set = hashlib.sha256()
        for row in source_rows:
            source_set.update(row["path"].encode("utf-8"))
            source_set.update(b"\0")
            source_set.update(row["sha256"].encode("ascii"))
            source_set.update(b"\n")
        source_set_sha256 = source_set.hexdigest()

        # QSF bytes do not contain the revision name, so their hash can safely
        # participate in the content witness without a circular dependency.
        qsf_lines = [
            "# GENERATED by tools/budget/gen_calib.py -- dual18 MapOnly discriminator.",
            '# Device and tool premise: Quartus Prime Lite 17.0.2 / Cyclone V.',
            'set_global_assignment -name FAMILY "Cyclone V"',
            "set_global_assignment -name DEVICE %s" % DUAL18_DEVICE,
            "set_global_assignment -name TOP_LEVEL_ENTITY %s" % spec["top"],
            "set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files",
            "set_global_assignment -name NUM_PARALLEL_PROCESSORS 4",
            "set_global_assignment -name SEED 1",
            'set_global_assignment -name OPTIMIZATION_MODE "BALANCED"',
        ]
        for row in source_rows:
            qsf_lines.append('set_global_assignment -name SYSTEMVERILOG_FILE "%s"' %
                             row["absolutePath"])
        for macro in spec["macros"]:
            qsf_lines.append('set_global_assignment -name VERILOG_MACRO "%s"' % macro)
        qsf_lines.append("set_instance_assignment -name VIRTUAL_PIN ON -to *")
        qsf_text = "\n".join(qsf_lines) + "\n"
        qsf_sha256 = hashlib.sha256(qsf_text.encode("ascii")).hexdigest()

        witness_inputs = {
            "schemaVersion": 1,
            "gate": "dual18_physical_pack_discriminator",
            "tool": {"name": "Quartus Prime Lite", "version": "17.0.2", "build": 602},
            "device": DUAL18_DEVICE,
            "baseRevision": base_revision,
            "top": spec["top"],
            "sources": [{"path": row["path"], "sha256": row["sha256"]}
                        for row in source_rows],
            "macros": list(spec["macros"]),
            "sourceSetSha256": source_set_sha256,
            "qsfSha256": qsf_sha256,
            # Canonical Quartus interface provenance affects the candidate just
            # as much as RTL/QSF bytes do.  Binding the complete normalized
            # records here prevents arbitrary files or hand-written excerpts
            # from being substituted without changing the report revision.
            "vendorInterfaceEvidence": vendor_evidence,
        }
        witness_bytes = json.dumps(
            witness_inputs, sort_keys=True, separators=(",", ":"), ensure_ascii=True
        ).encode("ascii")
        content_witness = hashlib.sha256(witness_bytes).hexdigest()
        # Quartus emits Revision Name verbatim in Analysis & Synthesis Summary.
        # This makes raw evidence carry the source/QSF witness using a field the
        # tool genuinely owns; no invented report hash field is involved.
        revision = "%s_%s" % (base_revision, content_witness[:16])

        revision_dir = os.path.join(root, base_revision)
        os.makedirs(revision_dir)
        output_dir = os.path.join(revision_dir, "output_files")
        os.makedirs(output_dir)
        if os.listdir(output_dir):
            raise RuntimeError("dual18 output directory was not empty at preparation")

        qpf_path = os.path.join(revision_dir, revision + ".qpf")
        with open(qpf_path, "w", encoding="ascii", newline="\n") as fh:
            fh.write('QUARTUS_VERSION = "17.0"\n')
            fh.write('PROJECT_REVISION = "%s"\n' % revision)
        qsf_path = os.path.join(revision_dir, revision + ".qsf")
        with open(qsf_path, "w", encoding="ascii", newline="\n") as fh:
            fh.write(qsf_text)
        if _sha256_file(qsf_path) != qsf_sha256:
            raise RuntimeError("written dual18 QSF differs from witnessed bytes")

        prepared_unix_ns = time.time_ns()
        prepared_local = datetime.datetime.fromtimestamp(
            prepared_unix_ns / 1_000_000_000
        ).astimezone()
        preparation = {
            "schemaVersion": 1,
            "gate": "dual18_physical_pack_discriminator",
            "baseRevision": base_revision,
            "revision": revision,
            "contentWitness": content_witness,
            "preparedAtLocal": prepared_local.isoformat(timespec="microseconds"),
            "preparedAtUnixNs": prepared_unix_ns,
            "outputDirectory": "output_files",
            "outputDirectoryWasEmpty": True,
            "expectedMapReport": "output_files/%s.map.rpt" % revision,
            "expectedMapSummary": "output_files/%s.map.summary" % revision,
        }
        preparation_path = os.path.join(revision_dir, "run_preparation.json")
        with open(preparation_path, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(preparation, fh, indent=2)
            fh.write("\n")

        receipt = {
            "schemaVersion": 1,
            "gate": "dual18_physical_pack_discriminator",
            "stage": "map-only",
            "tool": {"name": "Quartus Prime Lite", "version": "17.0.2", "build": 602},
            "device": DUAL18_DEVICE,
            "baseRevision": base_revision,
            "revision": revision,
            "top": spec["top"],
            "sources": source_rows,
            "macros": list(spec["macros"]),
            "sourceSetSha256": source_set_sha256,
            "qpfFile": os.path.basename(qpf_path),
            "qsfFile": os.path.basename(qsf_path),
            "qsfSha256": qsf_sha256,
            "contentWitness": content_witness,
            "witnessInputs": witness_inputs,
            "runPreparation": {
                "file": os.path.basename(preparation_path),
                "sha256": _sha256_file(preparation_path),
            },
            "vendorInterfaceEvidence": vendor_evidence,
            "expectedDspBlocks": spec["expectedDspBlocks"],
        }
        if "note" in spec:
            receipt["note"] = spec["note"]
        if "positiveControl" in spec:
            receipt["positiveControl"] = spec["positiveControl"]

        receipt_path = os.path.join(revision_dir, "effective_config.json")
        with open(receipt_path, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(receipt, fh, indent=2)
            fh.write("\n")

        emitted.append({
            "baseRevision": base_revision,
            "revision": revision,
            "contentWitness": content_witness,
            "top": spec["top"],
            "projectFile": os.path.relpath(qpf_path, REPO).replace(os.sep, "/"),
            "projectFileSha256": _sha256_file(qpf_path),
            "settingsFile": os.path.relpath(qsf_path, REPO).replace(os.sep, "/"),
            "settingsFileSha256": qsf_sha256,
            "sourceSetSha256": source_set_sha256,
            "runPreparation": os.path.relpath(preparation_path, REPO).replace(os.sep, "/"),
            "runPreparationSha256": _sha256_file(preparation_path),
            "effectiveConfig": os.path.relpath(receipt_path, REPO).replace(os.sep, "/"),
            "effectiveConfigSha256": _sha256_file(receipt_path),
        })

    manifest = {
        "schemaVersion": 1,
        "gate": "dual18_physical_pack_discriminator",
        "stage": "map-only",
        "device": DUAL18_DEVICE,
        "revisions": emitted,
    }
    manifest_path = os.path.join(root, "dual18_manifest.json")
    with open(manifest_path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")

    # One-way trust chain only: manifest -> external anchor.  The anchor cannot
    # appear in the manifest it hashes without circular self-hashing.  A caller
    # must retain and explicitly supply this path plus its hash/nonce/manifest
    # hash to check_dual18_map.py; rediscovering those values from candidate
    # evidence would recreate the co-moving replay defect.
    anchor_unix_ns = time.time_ns()
    anchor_local = datetime.datetime.fromtimestamp(
        anchor_unix_ns / 1_000_000_000
    ).astimezone()
    anchor = {
        "schemaVersion": 1,
        "gate": "dual18_physical_pack_discriminator",
        "trustBoundary": "orchestration-supplied-outside-candidate-tree",
        "createdAtLocal": anchor_local.isoformat(timespec="microseconds"),
        "createdAtUnixNs": anchor_unix_ns,
        "invocationNonce": secrets.token_hex(32),
        "evidenceDirectory": "dual18",
        "manifestPath": "dual18/dual18_manifest.json",
        "manifestSha256": _sha256_file(manifest_path),
    }
    with open(anchor_path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(anchor, fh, indent=2)
        fh.write("\n")
    return emitted


# ---------------------------------------------------------------------------
# ASYMMETRIC OPERANDS. Added 2026-08-24 because the whole 110-DSP narrowing
# thesis rests on an assumption this grid never tested.
#
# All 106 existing multiply points are SYMMETRIC (w x w). The band table they
# produced -- 8..27 -> 1 DSP, 28..33 -> 3 -- was then used to argue that
# narrowing a COORDINATE to 27 bits recovers ~110 DSPs. But the other operand
# is a matrix coefficient that stays 32-bit, and `zhao_project_core` already
# contains 32x27 products that map at 3 DSPs each (11 x 3 = 33, exactly the
# measured total). So narrowing one side may recover NOTHING.
#
# Two independent reviews reached that objection separately and neither could
# settle it, because the measurement does not exist. This is that measurement:
# does the WIDE operand set the band, or the pair?
#
# The grid is chosen to answer specific live questions rather than to be
# uniform: 32x27 and 33x27 are the projector and normals shapes as they stand;
# 27x27 and 28x28 are the known controls that must reproduce 1 and 3; 23x11 is
# GEOM.BINNER's real operand pair, which is currently multiplied out of 36-bit
# registers.
# ---------------------------------------------------------------------------
def mul_asym_module(name, wa, wb, signed):
    """Input+output registered, one operator, INDEPENDENT operand widths."""
    s = "signed " if signed else ""
    rw = wa + wb
    L = []
    L.append("// GENERATED by tools/budget/gen_calib.py -- asymmetric calibration.")
    L.append("// %s: %s%dx%d, input+output registered." % (
        name, "signed " if signed else "unsigned ", wa, wb))
    L.append("module %s (" % name)
    L.append("    input  logic clk,")
    L.append("    input  logic rst_n,")
    L.append("    input  logic %s[%d:0] a_i," % (s, wa - 1))
    L.append("    input  logic %s[%d:0] b_i," % (s, wb - 1))
    L.append("    output logic %s[%d:0] y_o" % (s, rw - 1))
    L.append(");")
    L.append("  logic %s[%d:0] a_r;" % (s, wa - 1))
    L.append("  logic %s[%d:0] b_r;" % (s, wb - 1))
    L.append("  logic %s[%d:0] p_r;" % (s, rw - 1))
    L.append("  always_ff @(posedge clk or negedge rst_n) begin")
    L.append("    if (!rst_n) begin")
    L.append("      a_r <= '0; b_r <= '0; p_r <= '0;")
    L.append("    end else begin")
    L.append("      a_r <= a_i; b_r <= b_i;")
    L.append("      p_r <= %s'(a_r) * %s'(b_r);" % (rw, rw))
    L.append("    end")
    L.append("  end")
    L.append("  assign y_o = p_r;")
    L.append("endmodule")
    return chr(10).join(L) + chr(10)


# (wide, narrow) pairs. Controls first so a broken grid is obvious immediately.
ASYM_PAIRS = [
    (27, 27),  # control: must reproduce 1 DSP
    (28, 28),  # control: must reproduce 3 DSPs
    (32, 32),  # control: must reproduce 3 DSPs
    (32, 27),  # THE question -- the projector's existing shape
    (33, 27),  # terrain_normals as it stands
    (32, 24),
    (32, 18),
    (27, 24),
    (27, 18),
    (24, 24),
    (24, 18),
    (23, 11),  # GEOM.BINNER's real operand pair
    # ---- LOCATING THE 32xN BOUNDARY, added 2026-09-09 ----------------------
    # The grid above jumps 32x18 (2 DSP) straight to 32x24 (3 DSP), so five
    # widths between them have never been measured. That gap is load-bearing:
    #
    #   32x27 = 3, 32x24 = 3, 32x18 = 2
    #
    # means narrowing the projector's MATRIX operand to 27 or 24 buys NOTHING,
    # and only 18 moves the cost -- which is the opposite of what "narrow it a
    # bit" suggests, and is why the whole lever needs a table instead of a guess.
    #
    # 18 bits signed is +-2.0 in Q16.16, and a perspective coefficient
    # cot(fov/2)/aspect passes 2.0 at roughly a 53 degree vertical field of view.
    # So an 18-bit matrix operand caps the longest usable lens. If the real
    # boundary is 22 rather than 18, coefficients reach +-32.0 and the cap stops
    # mattering -- the difference between a live constraint on the camera and a
    # formality.
    #
    # Descending so the first result is the most valuable: if 23 already costs 3,
    # the answer is bounded from above immediately.
    (32, 23),
    (32, 22),
    (32, 21),
    (32, 20),
    (32, 19),
]


def mul_module(name, width, signed, nops, style):
    """style: 'comb' | 'inreg' | 'ioreg'."""
    s = "signed " if signed else ""
    rw = 2 * width
    L = []
    L.append("// GENERATED by tools/budget/gen_calib.py -- calibration microbench.")
    L.append("// %s: %d operator(s), %s%d-bit operands, %s." % (
        name, nops, "signed " if signed else "unsigned ", width, style))
    L.append("module %s (" % name)
    L.append("    input  logic clk,")
    L.append("    input  logic rst_n,")
    for i in range(nops):
        L.append("    input  logic %s[%d:0] a%d_i," % (s, width - 1, i))
        L.append("    input  logic %s[%d:0] b%d_i," % (s, width - 1, i))
    L.append("    output logic %s[%d:0] y_o" % (s, rw - 1))
    L.append(");")
    if style == "comb":
        # Operands straight from the pins; result straight to the pin.
        terms = " + ".join("(a%d_i * b%d_i)" % (i, i) for i in range(nops))
        L.append("  assign y_o = %s;" % terms)
    else:
        for i in range(nops):
            L.append("  logic %s[%d:0] a%d_r, b%d_r;" % (s, width - 1, i, i))
        L.append("  logic %s[%d:0] p_c;" % (s, rw - 1))
        terms = " + ".join("(a%d_r * b%d_r)" % (i, i) for i in range(nops))
        L.append("  always_comb p_c = %s;" % terms)
        if style == "ioreg":
            L.append("  logic %s[%d:0] p_r;" % (s, rw - 1))
        L.append("  always_ff @(posedge clk or negedge rst_n) begin")
        L.append("    if (!rst_n) begin")
        for i in range(nops):
            L.append("      a%d_r <= '0; b%d_r <= '0;" % (i, i))
        if style == "ioreg":
            L.append("      p_r <= '0;")
        L.append("    end else begin")
        for i in range(nops):
            L.append("      a%d_r <= a%d_i; b%d_r <= b%d_i;" % (i, i, i, i))
        if style == "ioreg":
            L.append("      p_r <= p_c;")
        L.append("    end")
        L.append("  end")
        L.append("  assign y_o = %s;" % ("p_r" if style == "ioreg" else "p_c"))
    L.append("endmodule")
    return "\n".join(L) + "\n"


def widening_idiom_module(name, explicit):
    """The widening-signed-multiply control pair.

    explicit=True writes the SystemVerilog idiom with the sign extension
    spelled out, which is what zhao_geom_project's mul32 does nine times.
    explicit=False writes the plain signed product and lets the language widen
    it. If the two map to the same DSP count, the idiom is free and
    scan_rtl.py is right not to flag it.
    """
    L = ["// GENERATED by tools/budget/gen_calib.py -- widening-idiom control.",
         "module %s (" % name,
         "    input  logic clk,",
         "    input  logic rst_n,",
         "    input  logic signed [31:0] a_i,",
         "    input  logic signed [31:0] b_i,",
         "    output logic signed [63:0] y_o",
         ");",
         "  logic signed [31:0] a_r, b_r;",
         "  logic signed [63:0] p_r;"]
    if explicit:
        L.append("  logic signed [63:0] p_c;")
        L.append("  always_comb p_c = $signed({{32{a_r[31]}}, a_r}) * $signed({{32{b_r[31]}}, b_r});")
    else:
        L.append("  logic signed [63:0] p_c;")
        L.append("  always_comb p_c = 64'(a_r) * 64'(b_r);")
    L += ["  always_ff @(posedge clk or negedge rst_n) begin",
          "    if (!rst_n) begin a_r <= '0; b_r <= '0; p_r <= '0; end",
          "    else begin a_r <= a_i; b_r <= b_i; p_r <= p_c; end",
          "  end",
          "  assign y_o = p_r;",
          "endmodule"]
    return "\n".join(L) + "\n"


def ram_rdw_module(name, depth, width, shared, read_enable):
    """The two axes the original RAM grid never varied, and which SURFACE.SHEET
    turns out to depend on.

    Every `sync` point in the grid above writes in one `always_ff` and reads in
    ANOTHER, with NO read enable:

        always_ff @(posedge clk) if (we_i) mem[waddr_i] <= wdata_i;
        always_ff @(posedge clk) rdata_o <= mem[raddr_i];

    Real blocks do not look like that. `zhao_surface_sheet` puts both accesses
    in ONE process, which is what makes read-during-write return the OLD word
    (its contract's C5 states that semantic rather than leaving it to the
    synthesiser), and it gates the read with an enable so a stalled response
    does not lose its word. Neither variation was measured, so "it infers" was
    not actually known for the shape that matters.

      shared=True   read and write in one `always_ff` -> read-during-write
                    returns the PRE-write word (Intel's own single-clock simple
                    dual-port template).
      shared=False  read in its own `always_ff`.
      read_enable   gate the read with `if (re_i)`.

    Depth/width default to 8192x8 at the call site: that is one byte plane of
    SURFACE.SHEET at `Slots = 2`, i.e. the array this run proposes to build.
    """
    aw = max(1, (depth - 1).bit_length())
    L = ["// GENERATED by tools/budget/gen_calib.py -- RAM read-during-write template.",
         "// %s: %dx%d, sync read, no reset, 1 port, %s process, read enable %s." % (
             name, depth, width,
             "SHARED read/write" if shared else "separate read",
             "YES" if read_enable else "no"),
         "module %s (" % name,
         "    input  logic clk,",
         "    input  logic we_i,"]
    if read_enable:
        L.append("    input  logic re_i,")
    L += ["    input  logic [%d:0] waddr_i," % (aw - 1),
          "    input  logic [%d:0] wdata_i," % (width - 1),
          "    input  logic [%d:0] raddr_i," % (aw - 1),
          "    output logic [%d:0] rdata_o" % (width - 1),
          ");",
          "  logic [%d:0] mem [0:%d];" % (width - 1, depth - 1)]
    rd = "    %srdata_o <= mem[raddr_i];" % ("if (re_i) " if read_enable else "")
    wr = "    if (we_i) mem[waddr_i] <= wdata_i;"
    if shared:
        L += ["  always_ff @(posedge clk) begin", rd, wr, "  end"]
    else:
        L += ["  always_ff @(posedge clk) begin", wr, "  end",
              "  always_ff @(posedge clk) begin", rd, "  end"]
    L.append("endmodule")
    return "\n".join(L) + "\n"


def ram_module(name, depth, width, read_style, reset, ports, byteen):
    """read_style: 'sync' | 'async'.  ports: 1 | 2."""
    aw = max(1, (depth - 1).bit_length())
    L = ["// GENERATED by tools/budget/gen_calib.py -- RAM template microbench.",
         "// %s: %dx%d, %s read, %s, %d port(s)%s." % (
             name, depth, width, read_style,
             "reset-touched" if reset else "no reset", ports,
             ", byte enables" if byteen else "")]
    L.append("module %s (" % name)
    L.append("    input  logic clk,")
    L.append("    input  logic rst_n,")
    L.append("    input  logic we_i,")
    if byteen:
        L.append("    input  logic [%d:0] be_i," % (width // 8 - 1))
    L.append("    input  logic [%d:0] waddr_i," % (aw - 1))
    L.append("    input  logic [%d:0] wdata_i," % (width - 1))
    L.append("    input  logic [%d:0] raddr_i," % (aw - 1))
    L.append("    output logic [%d:0] rdata_o%s" % (width - 1, "," if ports == 2 else ""))
    if ports == 2:
        L.append("    input  logic [%d:0] raddr2_i," % (aw - 1))
        L.append("    output logic [%d:0] rdata2_o" % (width - 1))
    L.append(");")
    L.append("  logic [%d:0] mem [0:%d];" % (width - 1, depth - 1))
    if reset:
        L.append("  integer i;")
        L.append("  always_ff @(posedge clk or negedge rst_n) begin")
        L.append("    if (!rst_n) begin")
        L.append("      for (i = 0; i <= %d; i = i + 1) mem[i] <= '0;" % (depth - 1))
        L.append("    end else if (we_i) begin")
        if byteen:
            for b in range(width // 8):
                L.append("      if (be_i[%d]) mem[waddr_i][%d:%d] <= wdata_i[%d:%d];"
                         % (b, b * 8 + 7, b * 8, b * 8 + 7, b * 8))
        else:
            L.append("      mem[waddr_i] <= wdata_i;")
        L.append("    end")
        L.append("  end")
    else:
        L.append("  always_ff @(posedge clk) begin")
        L.append("    if (we_i) begin")
        if byteen:
            for b in range(width // 8):
                L.append("      if (be_i[%d]) mem[waddr_i][%d:%d] <= wdata_i[%d:%d];"
                         % (b, b * 8 + 7, b * 8, b * 8 + 7, b * 8))
        else:
            L.append("      mem[waddr_i] <= wdata_i;")
        L.append("    end")
        L.append("  end")
    if read_style == "async":
        L.append("  assign rdata_o = mem[raddr_i];")
        if ports == 2:
            L.append("  assign rdata2_o = mem[raddr2_i];")
    else:
        L.append("  always_ff @(posedge clk) rdata_o <= mem[raddr_i];")
        if ports == 2:
            L.append("  always_ff @(posedge clk) rdata2_o <= mem[raddr2_i];")
    L.append("endmodule")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--outdir", default=OUTDIR)
    ap.add_argument(
        "--dual18-only",
        action="store_true",
        help="emit only the four isolated dual18 MapOnly revisions",
    )
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    dual18_revisions = emit_dual18_map_revisions(args.outdir)
    anchor_path = os.path.join(args.outdir, "dual18_invocation_anchor.json")
    with open(anchor_path, "r", encoding="utf-8") as fh:
        anchor = json.load(fh)
    print(
        "DUAL18_INVOCATION_ANCHOR path=%s sha256=%s nonce=%s manifestSha256=%s"
        % (
            os.path.abspath(anchor_path).replace(os.sep, "/"),
            _sha256_file(anchor_path),
            anchor["invocationNonce"],
            anchor["manifestSha256"],
        )
    )
    if args.dual18_only:
        print("generated %d isolated dual18 MapOnly revision(s) into %s" %
              (len(dual18_revisions), os.path.join(args.outdir, "dual18")))
        return

    points = []

    def emit(name, src, **meta):
        path = os.path.join(args.outdir, name + ".sv")
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(src)
        rec = {"module": name, "file": os.path.relpath(path, REPO).replace(os.sep, "/")}
        rec.update(meta)
        points.append(rec)

    # ---- multiply grid: width x signedness x {1,4} operators -------------
    for w in WIDTHS:
        for signed in (False, True):
            for nops in (1, 4):
                nm = "calib_mul_%s%d_n%d_ioreg" % ("s" if signed else "u", w, nops)
                emit(nm, mul_module(nm, w, signed, nops, "ioreg"),
                     family="multiply", width=w, signed=signed, operators=nops,
                     style="ioreg")

    # ---- asymmetric grid: does the WIDE operand set the band? ------------
    for (wa, wb) in ASYM_PAIRS:
        nm = "calib_mulasym_s%dx%d_ioreg" % (wa, wb)
        emit(nm, mul_asym_module(nm, wa, wb, True),
             family="multiply_asym", width=wa, widthB=wb, signed=True,
             operators=1, style="ioreg")

    # ---- registering grid -----------------------------------------------
    for w in REG_WIDTHS:
        for style in ("comb", "inreg"):
            nm = "calib_mul_s%d_n1_%s" % (w, style)
            emit(nm, mul_module(nm, w, True, 1, style),
                 family="multiply", width=w, signed=True, operators=1, style=style)

    # ---- widening-idiom control pair ------------------------------------
    emit("calib_widen_explicit", widening_idiom_module("calib_widen_explicit", True),
         family="widening", width=32, signed=True, operators=1, style="ioreg",
         note="sign extension spelled out, as zhao_geom_project's mul32 writes it")
    emit("calib_widen_implicit", widening_idiom_module("calib_widen_implicit", False),
         family="widening", width=32, signed=True, operators=1, style="ioreg",
         note="plain widened signed product")

    # ---- RAM grid --------------------------------------------------------
    shapes = [(64, 32), (256, 16), (1024, 32), (2048, 18)]
    for (d, w) in shapes:
        for read_style in ("sync", "async"):
            for reset in (False, True):
                for ports in (1, 2):
                    nm = "calib_ram_%dx%d_%s_%s_p%d" % (
                        d, w, read_style, "rst" if reset else "norst", ports)
                    emit(nm, ram_module(nm, d, w, read_style, reset, ports, False),
                         family="ram", depth=d, width=w, readStyle=read_style,
                         reset=reset, ports=ports, byteEnables=False,
                         expectedBits=d * w)
    for (d, w) in [(1024, 32), (2048, 32)]:
        nm = "calib_ram_%dx%d_sync_norst_p1_be" % (d, w)
        emit(nm, ram_module(nm, d, w, "sync", False, 1, True),
             family="ram", depth=d, width=w, readStyle="sync", reset=False,
             ports=1, byteEnables=True, expectedBits=d * w)

    # ---- read-during-write / read-enable grid ----------------------------
    # RUN-20260824-0317. The `ram` grid above reads in a SEPARATE always_ff
    # with NO read enable, so it does not actually cover the template real
    # blocks use. SURFACE.SHEET shares one process (that is what makes
    # read-during-write return the OLD word -- its contract states the
    # semantic) and gates the read with an enable. Two axes, four points, at
    # 8192x8: one byte plane of SURFACE.SHEET at Slots = 2.
    for shared in (True, False):
        for re_ in (True, False):
            nm = "calib_ram_8192x8_%s_%s" % (
                "shared" if shared else "split", "re" if re_ else "nore")
            emit(nm, ram_rdw_module(nm, 8192, 8, shared, re_),
                 family="ram_rdw", depth=8192, width=8, readStyle="sync",
                 reset=False, ports=1, byteEnables=False,
                 sharedProcess=shared, readEnable=re_, expectedBits=8192 * 8)

    man = os.path.join(args.outdir, "manifest.json")
    with open(man, "w", encoding="utf-8", newline="\n") as fh:
        json.dump({"schemaVersion": 1, "generator": "tools/budget/gen_calib.py",
                   "points": points, "dual18MapRevisions": dual18_revisions}, fh, indent=1)
        fh.write("\n")
    print("generated %d microbench module(s) into %s" % (len(points), args.outdir))
    fams = {}
    for p in points:
        fams[p["family"]] = fams.get(p["family"], 0) + 1
    for k, v in sorted(fams.items()):
        print("  %-10s %d" % (k, v))


if __name__ == "__main__":
    main()
