#!/usr/bin/env python3
"""Check proposal arithmetic. This is NOT a simulator or feasibility certificate.

No third-party dependencies. Python >= 3.10.
Exit codes: 0 = requested arithmetic/report/self-tests completed; 2 = invalid
inputs or allocation exceeds a declared ceiling; 3 = feasibility was requested
but cannot be certified by this planning-only model.
"""
from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any


class InputError(ValueError):
    """Invalid or inconsistent planning inputs."""


def integer(value: Any, label: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise InputError(f"{label}: expected integer >= {minimum}, got {value!r}")
    return value


def ceil_div(n: int, d: int) -> int:
    if n < 0 or d <= 0:
        raise InputError("ceil_div requires nonnegative demand and positive capacity")
    return (n + d - 1) // d


def totals(config: dict[str, Any]) -> dict[str, int]:
    fields = ("alm", "dsp", "m10k", "advisory_registers")
    result = {key: 0 for key in fields}
    rows = config.get("allocations")
    if not isinstance(rows, list) or not rows:
        raise InputError("allocations must be a nonempty list")
    owners: set[str] = set()
    for row in rows:
        name = row.get("owner")
        if not isinstance(name, str) or not name or name in owners:
            raise InputError(f"missing or duplicate allocation owner: {name!r}")
        owners.add(name)
        for field in fields:
            result[field] += integer(row.get(field), f"{name}.{field}")
    for field in ("alm", "dsp", "m10k"):
        capacity = integer(config["device_working_ceilings"][field], field, 1)
        if result[field] > capacity:
            raise InputError(f"{field} allocation {result[field]} exceeds {capacity}")
    return result


def validate(config: dict[str, Any]) -> None:
    if config.get("schema_version") != 1:
        raise InputError("unsupported schema_version")
    if config.get("evidence_class") != "ALLOCATION_AND_ILLUSTRATIVE_LOWER_BOUNDS_ONLY":
        raise InputError("this tool accepts planning inputs only, never fit evidence")
    if config.get("feasibility_status") != "UNPROVEN":
        raise InputError("a planning-only tool cannot accept a feasibility claim")
    if config.get("v2_measurements") != []:
        raise InputError("attach real measurements to a proper evidence ledger, not this toy model")
    totals(config)
    frame = config["frame"]
    integer(frame["fps"], "fps", 1)
    den = integer(frame["reserve_denominator"], "reserve_denominator", 1)
    num = integer(frame["reserve_numerator"], "reserve_numerator")
    if num >= den:
        raise InputError("reserve must be between 0 and 1")
    clocks = frame["clock_hz_sensitivity"]
    if not isinstance(clocks, list) or not clocks:
        raise InputError("clock sensitivity must be a nonempty list")
    for frequency in clocks:
        integer(frequency, "clock_hz", 1)
    g = config["geometry_example"]
    for k in ("skinned_vertices", "products_per_blended_vertex", "terrain_patches",
              "lattice_vertices_per_patch", "assumed_products_per_projection"):
        integer(g[k], k, 1)
    for lane in g["product_lanes"]:
        integer(lane, "product_lanes", 1)
    f = config["field_synthetic_example"]
    for k in ("patches", "points_per_patch", "changing_fields", "assumed_uops_per_point_field"):
        integer(f[k], k, 1)
    for lane in f["vector_lanes"]:
        integer(lane, "vector_lanes", 1)
    for case in config["sampler_examples"]:
        integer(case["logical_samples"], "logical_samples", 1)
        integer(case["palette_reads"], "palette_reads")
        integer(case["filter_channel_jobs"], "filter_channel_jobs")
    m = config["terrain_backing_example"]
    for k in ("patches", "vertices_per_patch", "bytes_per_vertex", "planes"):
        integer(m[k], k, 1)


def usable_cycles(config: dict[str, Any], hz: int) -> tuple[int, int]:
    frame = config["frame"]
    raw = hz // frame["fps"]
    usable = raw * (frame["reserve_denominator"] - frame["reserve_numerator"]) // frame["reserve_denominator"]
    return raw, usable


def geometry_products(config: dict[str, Any]) -> int:
    g = config["geometry_example"]
    return (g["skinned_vertices"] * g["products_per_blended_vertex"]
            + g["terrain_patches"] * g["lattice_vertices_per_patch"]
            * g["assumed_products_per_projection"])


def report(config: dict[str, Any]) -> str:
    validate(config)
    total = totals(config)
    cap = config["device_working_ceilings"]
    lines = ["# Zhaozhou V2 R0 — planning arithmetic", "",
             "**Feasibility: UNPROVEN. No V2 fit, schedule, simulation or board result is represented here.**", "",
             "A lower bound above a budget rejects that illustrative configuration. A lower bound below it does not certify it.", "",
             "## Whole-bitstream allocation", "",
             "| Resource | Allocated | Working ceiling | Nominal remainder |",
             "|---|---:|---:|---:|"]
    for key in ("alm", "dsp", "m10k"):
        lines.append(f"| {key} | {total[key]:,} | {cap[key]:,} | {cap[key]-total[key]:,} |")
    lines += ["", f"Advisory registers: {total['advisory_registers']:,}; not an independent packing/fit certificate.", "",
              "## Clock sensitivity", "", "20% reserve is applied conservatively after flooring raw cycles.", "",
              "| Compute MHz | Raw cycles/frame | Usable cycles/frame |",
              "|---|---:|---:|"]
    for hz in config["frame"]["clock_hz_sensitivity"]:
        raw, usable = usable_cycles(config, hz)
        lines.append(f"| {hz/1_000_000:g} | {raw:,} | {usable:,} |")
    target = config["frame"]["target_hz_assumption"]
    _, budget = usable_cycles(config, target)
    products = geometry_products(config)
    lines += ["", "## Illustrative joint geometry demand", "",
              config["geometry_example"]["limitations"], "",
              f"Counted full-width products: **{products:,}**. Target comparison: {budget:,} usable clocks.", "",
              "| Perfect product lanes | Arithmetic-only cycles | Compared with reserve |",
              "|---|---:|---|"]
    for lanes in config["geometry_example"]["product_lanes"]:
        cycles = ceil_div(products, lanes)
        status = "EXCEEDS: inadequate even before omitted work" if cycles > budget else "BELOW: feasibility still unproven"
        lines.append(f"| {lanes} | {cycles:,} | {status} |")
    f = config["field_synthetic_example"]
    point_fields = f["patches"] * f["points_per_patch"] * f["changing_fields"]
    uops = point_fields * f["assumed_uops_per_point_field"]
    lines += ["", "## Synthetic all-changing Field conjunction", "", f["limitations"], "",
              f"Point-field evaluations: **{point_fields:,}**; assumed lane-micro-ops: **{uops:,}**.", "",
              "| Perfect vector lanes | Issue-only clocks | Ratio to usable target clocks |",
              "|---|---:|---:|"]
    for lanes in f["vector_lanes"]:
        cycles = ceil_div(uops, lanes)
        lines.append(f"| {lanes} | {cycles:,} | {cycles/budget:.2f}x |")
    lines += ["", "## Sampler examples", "",
              "The idealized floor assumes one logical cache request, palette read, filter-channel job and output slot per clock, on independent resources. It excludes misses, dependencies, ports needed within a bilinear cache request, queues and drain.", "",
              "| Profile | Ideal resource floor | Qualification |", "|---|---:|---|"]
    for case in config["sampler_examples"]:
        floor = max(case["logical_samples"], case["palette_reads"], case["filter_channel_jobs"])
        lines.append(f"| {case['name']} | {floor:,} | {case['qualification']} |")
    m = config["terrain_backing_example"]
    plane = m["patches"] * m["vertices_per_patch"] * m["bytes_per_vertex"]
    all_planes = plane * m["planes"]
    all_m10k_bytes = cap["m10k"] * 10240 // 8
    lines += ["", "## Terrain backing capacity", "",
              f"One example plane: **{plane:,} bytes**. {m['planes']} planes: **{all_planes:,} bytes**.", "",
              f"All {cap['m10k']} M10Ks hold only **{all_m10k_bytes:,} raw bytes**, before allocating any other state, width/port fragmentation or replicas. External backing plus a small working set is necessary for this example.", "",
              "## What remains missing", ""]
    lines += [f"- {item}" for item in config["unresolved"]]
    lines += ["", "**Conclusion: allocation arithmetic checked; architectural feasibility remains UNPROVEN.**", ""]
    return "\n".join(lines)


def self_test(config: dict[str, Any]) -> int:
    validate(config)
    checks = 0
    assert totals(config) == {"alm": 35000, "dsp": 96, "m10k": 455, "advisory_registers": 110000}; checks += 1
    assert usable_cycles(config, 100_000_000) == (1666666, 1333332); checks += 1
    assert usable_cycles(config, 80_000_000) == (1333333, 1066666); checks += 1
    assert geometry_products(config) == 4669056; checks += 1
    assert ceil_div(4669056, 3) == 1556352; checks += 1
    assert ceil_div(4669056, 6) == 778176; checks += 1
    assert 256 * 1089 * 16 * 22 == 98131968; checks += 1
    assert 256 * 1089 * 2 * 2 > 553 * 10240 // 8; checks += 1
    mutations = [
        ("over-budget allocation", lambda c: c["allocations"][0].update(alm=50000)),
        ("fake feasibility claim", lambda c: c.update(feasibility_status="PROVEN")),
        ("unrecognized evidence", lambda c: c.update(v2_measurements=[{"fake": True}])),
        ("negative allocation", lambda c: c["allocations"][0].update(m10k=-1)),
        ("invalid reserve", lambda c: c["frame"].update(reserve_numerator=5)),
        ("boolean resource value", lambda c: c["allocations"][0].update(dsp=True)),
        ("zero capacity lane", lambda c: c["geometry_example"].update(product_lanes=[0])),
    ]
    for name, mutate in mutations:
        damaged = copy.deepcopy(config)
        mutate(damaged)
        try:
            validate(damaged)
        except (InputError, KeyError, TypeError):
            checks += 1
        else:
            raise AssertionError(f"negative control failed to reject: {name}")
    assert "Feasibility: UNPROVEN" in report(config); checks += 1
    return checks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=Path(__file__).with_name("planning_inputs.json"))
    parser.add_argument("--report", type=Path, help="write a Markdown arithmetic report")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--require-feasibility", action="store_true")
    args = parser.parse_args()
    try:
        config = json.loads(args.inputs.read_text(encoding="utf-8"))
        validate(config)
        if args.self_test:
            print(f"PASS: {self_test(config)} arithmetic/schema/negative-control checks. Not hardware tests.")
        if args.report:
            args.report.write_text(report(config), encoding="utf-8")
            print(f"Wrote {args.report}")
        if not args.self_test and not args.report:
            print(report(config))
        if args.require_feasibility:
            print("UNPROVEN: no V2 hardware or complete schedulability evidence; certification refused.", file=sys.stderr)
            return 3
        return 0
    except (OSError, ValueError, KeyError, TypeError, AssertionError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
