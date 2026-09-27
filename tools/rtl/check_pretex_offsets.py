#!/usr/bin/env python3
"""Hold the PRETEX record's layout together across the language boundary.

WHY THIS EXISTS.  On 2026-09-26 commit `ceba0bfe` (NORMALMAP) grew the texture
request by one bit at its top.  It moved the TEXREQ, EZPAY, EARLYZ_KEY and
CONTINUATION constants and left the PRETEX ones behind, so
`PRETEX_EARLYZ_PAYLOAD_HI` said 409 while `PRETEX_EARLYZ_KEY_LO` said 411 and
BIT 410 BELONGED TO NO FIELD.  Six required Packet-D tests read four fields one
bit low for a week, and a registered ctest (`render_texture_packet_a`) failed at
verilation the whole time.

Nothing caught it, for two reasons worth stating separately:

  * the package's `PRETEX_OFFSET_CONTRACT_OK` compares every constant TO ITS OWN
    LITERAL.  It asserts 410 == 410.  It is CLAUDE.md's "detector wired to two
    operands that move together", with the two operands being one operand twice.
    That half is repaired IN the package by `PRETEX_FIELDS_TILE_OK` and
    `PRETEX_CROSS_RECORD_OK`, which relate the constants to each other.
  * the consumer that actually broke was a C++ test holding FOURTEEN BARE
    LITERALS, in another language, which no SystemVerilog check can reach.  That
    is this file's half.  A literal in a test cannot go stale loudly.

WHAT IT CHECKS
  1. the package's PRETEX fields TILE the record: no hole, no overlap, nothing
     past the end, and they reach the declared width exactly;
  2. the C++ `ZHAO_PRETEX_OFFSETS` block agrees with the package, field by name;
  3. PRETEX agrees with the sub-records it is assembled from (TEXREQ, EZPAY and
     the Early-Z key lifted to its base).

`--selftest` mutates in-memory copies and requires every check to REPORT the
breach.  A detector that has not been shown to fire has not been tested, and the
detector this file replaces is a live example of precisely that.

Exit 0 when the layout is coherent, 1 otherwise.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PKG = REPO / "fpga" / "rtl" / "common" / "zhao_render_texture_pkg.sv"
CPP = REPO / "tests" / "geometry" / "geom_bin_pipe_v2_directed.cpp"

# The record's fields in ascending offset order.  This order IS the claim being
# checked; a field missing from it would let its neighbours abut and the tiling
# check would pass over a real hole, so the census below refuses a table that
# does not account for every PRETEX field constant the package declares.
FIELD_ORDER = [
    "PALETTE_GENERATION", "PALETTE_SLOT", "RESPONSE_CLASS", "BASE_ALPHA",
    "BASE_RGB", "AUX_SURFACE_CTX", "AUX_REQUIRED", "RECIPE_WEIGHT",
    "MATERIAL_RECIPE", "LOD_Q4_4", "BASE_BINDING", "SAMPLE_COUNT",
    "V_OVER_W", "U_OVER_W", "DETAIL_REQUIRED", "STENCIL_REFERENCE",
    "EFFECT_TAG", "VERTEX_ALPHA", "VERTEX_RGB", "SOURCE_ID",
    "FRAGMENT_STATE", "INVW24", "IN_TILE_ADDR",
]

# Group spans, which are NOT fields: they bracket runs of the fields above.
GROUP_SPANS = ["EARLYZ_PAYLOAD", "EARLYZ_KEY", "CONTINUATION", "TEXTURE_REQUEST"]

# C++ constant name -> package field name.  Every entry must resolve, and every
# constant found in the C++ block must appear here, or the mapping is stale.
CPP_TO_FIELD = {
    "kBaseRgb": "BASE_RGB",
    "kMaterialRecipe": "MATERIAL_RECIPE",
    "kBaseBinding": "BASE_BINDING",
    "kSampleCount": "SAMPLE_COUNT",
    "kVOverW": "V_OVER_W",
    "kUOverW": "U_OVER_W",
    "kStencilReference": "STENCIL_REFERENCE",
    "kEffectTag": "EFFECT_TAG",
    "kVertexAlpha": "VERTEX_ALPHA",
    "kVertexRgb": "VERTEX_RGB",
    "kSourceId": "SOURCE_ID",
    "kFragmentState": "FRAGMENT_STATE",
    "kInvw24": "INVW24",
    "kInTileAddr": "IN_TILE_ADDR",
}

_LOCALPARAM = re.compile(
    r"^\s*localparam\s+int\s+unsigned\s+([A-Z0-9_]+)\s*=\s*(\d+)\s*;", re.MULTILINE)

_CPP_BLOCK = re.compile(
    r"//\s*----\s*ZHAO_PRETEX_OFFSETS\s*-*\s*\n(.*?)//\s*----\s*end ZHAO_PRETEX_OFFSETS",
    re.DOTALL)
_CPP_CONST = re.compile(r"^\s*constexpr\s+int\s+(k[A-Za-z0-9_]+)\s*=\s*(\d+)\s*;",
                        re.MULTILINE)

# The broken-instrument law: a parser that silently matches nothing reports no
# problems.  Assert at import that each pattern still matches a known example.
assert _LOCALPARAM.search("  localparam int unsigned PRETEX_SOURCE_ID_LO = 411;")
assert _CPP_CONST.search("constexpr int kSourceId = 411;")
assert _CPP_BLOCK.search(
    "// ---- ZHAO_PRETEX_OFFSETS ----\nconstexpr int kA = 1;\n"
    "// ---- end ZHAO_PRETEX_OFFSETS ----")


def parse_package(text: str) -> dict[str, int]:
    values = {name: int(value) for name, value in _LOCALPARAM.findall(text)}
    if "RASTER_PRETEX_W" not in values:
        raise SystemExit("check_pretex_offsets: RASTER_PRETEX_W not found in the "
                         "package -- the localparam pattern has gone stale")
    return values


def parse_cpp(text: str) -> dict[str, int]:
    found = _CPP_BLOCK.search(text)
    if not found:
        raise SystemExit("check_pretex_offsets: the ZHAO_PRETEX_OFFSETS block is "
                         "missing from %s -- if the offsets went back to bare "
                         "literals, this gate is blind and that is the defect" % CPP)
    constants = {n: int(v) for n, v in _CPP_CONST.findall(found.group(1))}
    if not constants:
        raise SystemExit("check_pretex_offsets: the ZHAO_PRETEX_OFFSETS block "
                         "parsed to zero constants -- precision at zero is a tell")
    return constants


def check(pkg: dict[str, int], cpp: dict[str, int]) -> list[str]:
    """Return a list of breaches.  Empty means the layout is coherent."""
    bad: list[str] = []
    width = pkg["RASTER_PRETEX_W"]

    # ---- census: the field table must account for every declared PRETEX field.
    declared = set()
    for name in pkg:
        if name.startswith("PRETEX_") and name.endswith("_LO"):
            stem = name[len("PRETEX_"):-len("_LO")]
            if stem not in GROUP_SPANS:
                declared.add(stem)
    missing = declared - set(FIELD_ORDER)
    if missing:
        bad.append("PRETEX field(s) %s are declared in the package but absent "
                   "from this tool's FIELD_ORDER, so the tiling check would pass "
                   "over them" % sorted(missing))
    absent = set(FIELD_ORDER) - declared
    if absent:
        bad.append("FIELD_ORDER names %s which the package does not declare"
                   % sorted(absent))

    # ---- 1. the fields tile the record ------------------------------------
    prev_hi = -1
    for stem in FIELD_ORDER:
        lo_key, hi_key = "PRETEX_%s_LO" % stem, "PRETEX_%s_HI" % stem
        if lo_key not in pkg or hi_key not in pkg:
            continue
        lo, hi = pkg[lo_key], pkg[hi_key]
        if lo != prev_hi + 1:
            bad.append("PRETEX_%s starts at %d but the previous field ends at %d "
                       "-- %s of %d bit(s)"
                       % (stem, lo, prev_hi,
                          "a HOLE" if lo > prev_hi + 1 else "an OVERLAP",
                          abs(lo - prev_hi - 1)))
        if hi < lo:
            bad.append("PRETEX_%s is empty or inverted: [%d:%d]" % (stem, hi, lo))
        prev_hi = hi
    if prev_hi != width - 1:
        bad.append("the PRETEX fields end at bit %d but the record is %d bits "
                   "wide, so bit(s) %d..%d belong to no field"
                   % (prev_hi, width, prev_hi + 1, width - 1))

    # ---- 2. the C++ consumer agrees, field by name -------------------------
    unmapped = set(cpp) - set(CPP_TO_FIELD)
    if unmapped:
        bad.append("the C++ offset block declares %s which this tool cannot map "
                   "to a package field" % sorted(unmapped))
    for const, stem in CPP_TO_FIELD.items():
        if const not in cpp:
            bad.append("the C++ offset block is missing %s" % const)
            continue
        lo_key = "PRETEX_%s_LO" % stem
        if lo_key in pkg and cpp[const] != pkg[lo_key]:
            bad.append("%s = %d in %s but %s = %d in the package"
                       % (const, cpp[const], CPP.name, lo_key, pkg[lo_key]))

    # ---- 3. PRETEX agrees with the records it is assembled from ------------
    for stem in FIELD_ORDER[:15]:  # the texture request's own fields
        for side in ("LO", "HI"):
            a, b = "PRETEX_%s_%s" % (stem, side), "TEXREQ_%s_%s" % (stem, side)
            if a in pkg and b in pkg and pkg[a] != pkg[b]:
                bad.append("%s = %d but %s = %d" % (a, pkg[a], b, pkg[b]))
    for stem in ["DETAIL_REQUIRED", "STENCIL_REFERENCE", "EFFECT_TAG",
                 "VERTEX_ALPHA", "VERTEX_RGB", "TEXTURE_REQUEST"]:
        for side in ("LO", "HI"):
            a, b = "PRETEX_%s_%s" % (stem, side), "EZPAY_%s_%s" % (stem, side)
            if a in pkg and b in pkg and pkg[a] != pkg[b]:
                bad.append("%s = %d but %s = %d" % (a, pkg[a], b, pkg[b]))
    key_lo = pkg.get("PRETEX_EARLYZ_KEY_LO")
    if key_lo is not None:
        for stem in ["SOURCE_ID", "FRAGMENT_STATE", "INVW24", "IN_TILE_ADDR"]:
            for side in ("LO", "HI"):
                a, b = "PRETEX_%s_%s" % (stem, side), "EARLYZ_%s_%s" % (stem, side)
                if a in pkg and b in pkg and pkg[a] != key_lo + pkg[b]:
                    bad.append("%s = %d but PRETEX_EARLYZ_KEY_LO + %s = %d"
                               % (a, pkg[a], b, key_lo + pkg[b]))
    return bad


# ---- the positive control ---------------------------------------------------
# Each case perturbs ONE quantity and names the check it must wake.  They are
# separate cases because a single mutation that trips three checks would let two
# of them be dead without anyone noticing.
SELFTEST_CASES = [
    ("a field displaced by one bit (the fault ceba0bfe actually created)",
     lambda p, c: (p.update({"PRETEX_SOURCE_ID_LO": p["PRETEX_SOURCE_ID_LO"] + 1}), None)[1]),
    ("a one-bit hole between two fields",
     lambda p, c: (p.update({"PRETEX_VERTEX_RGB_HI": p["PRETEX_VERTEX_RGB_HI"] - 1}), None)[1]),
    ("an overlap between two fields",
     lambda p, c: (p.update({"PRETEX_INVW24_LO": p["PRETEX_INVW24_LO"] - 1}), None)[1]),
    ("the top field one bit short of the record",
     lambda p, c: (p.update({"PRETEX_IN_TILE_ADDR_HI": p["PRETEX_IN_TILE_ADDR_HI"] - 1}), None)[1]),
    ("the C++ consumer left behind while the package moves",
     lambda p, c: (c.update({"kSourceId": c["kSourceId"] - 1}), None)[1]),
    ("a sub-record moved without the whole record (TEXREQ)",
     lambda p, c: (p.update({"TEXREQ_BASE_RGB_LO": p["TEXREQ_BASE_RGB_LO"] + 1}), None)[1]),
    ("a sub-record moved without the whole record (EZPAY)",
     lambda p, c: (p.update({"EZPAY_EFFECT_TAG_HI": p["EZPAY_EFFECT_TAG_HI"] + 1}), None)[1]),
    ("the Early-Z key moved without the whole record",
     lambda p, c: (p.update({"EARLYZ_INVW24_LO": p["EARLYZ_INVW24_LO"] + 1}), None)[1]),
    ("a field dropped from this tool's own table",
     lambda p, c: FIELD_ORDER.remove("EFFECT_TAG")),
]


def selftest(pkg: dict[str, int], cpp: dict[str, int]) -> int:
    clean = check(dict(pkg), dict(cpp))
    if clean:
        print("SELFTEST ABORTED: the tree is already breaching, so a mutation "
              "proving nothing would look like a pass. Breaches:")
        for line in clean:
            print("  %s" % line)
        return 1
    print("selftest: the unmutated tree is clean, so every red below is the "
          "mutation and not the tree")
    failures = 0
    original_order = list(FIELD_ORDER)
    for label, mutate in SELFTEST_CASES:
        p, c = dict(pkg), dict(cpp)
        del FIELD_ORDER[:]
        FIELD_ORDER.extend(original_order)
        mutate(p, c)
        found = check(p, c)
        if found:
            print("  FIRES  %-62s -> %s" % (label, found[0]))
        else:
            print("  SILENT %-62s <- THE CHECK IS BLIND TO THIS" % label)
            failures += 1
    del FIELD_ORDER[:]
    FIELD_ORDER.extend(original_order)
    if failures:
        print("check_pretex_offsets SELFTEST FAILED: %d of %d mutations went "
              "undetected" % (failures, len(SELFTEST_CASES)))
        return 1
    print("check_pretex_offsets SELFTEST OK: %d of %d mutations detected"
          % (len(SELFTEST_CASES), len(SELFTEST_CASES)))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest", action="store_true",
                    help="perturb the layout in memory and require every check "
                         "to report the breach")
    args = ap.parse_args()

    pkg = parse_package(PKG.read_text(encoding="utf-8"))
    cpp = parse_cpp(CPP.read_text(encoding="utf-8"))

    if args.selftest:
        return selftest(pkg, cpp)

    bad = check(pkg, cpp)
    if bad:
        print("check_pretex_offsets: %d BREACH(ES)" % len(bad))
        for line in bad:
            print("  %s" % line)
        return 1
    print("check_pretex_offsets OK: %d fields tile %d bits, %d C++ offsets agree, "
          "sub-records consistent"
          % (len(FIELD_ORDER), pkg["RASTER_PRETEX_W"], len(CPP_TO_FIELD)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
