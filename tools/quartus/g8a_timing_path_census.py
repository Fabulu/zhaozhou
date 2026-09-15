#!/usr/bin/env python3
"""Build a reproducible census from a G8A TimeQuest setup-summary table."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Callable


SCHEMA_ID = "zhao.g8a.timing_path_census"
SCHEMA_VERSION = 1


def sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def parse_paths(raw: bytes) -> list[dict[str, object]]:
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError(f"setup summary is not UTF-8: {exc}") from exc
    rows: list[dict[str, object]] = []
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line.startswith(";"):
            continue
        fields = [field.strip() for field in line.split(";")]
        if len(fields) < 10:
            continue
        try:
            slack = float(fields[1])
            relationship = float(fields[6])
            skew = float(fields[7])
            data_delay = float(fields[8])
        except ValueError:
            continue
        if not fields[2] or not fields[3] or not fields[4] or not fields[5]:
            raise ValueError(f"path row {line_number} contains an empty identity/clock field")
        rows.append({
            "data_delay_ns": data_delay,
            "from": fields[2],
            "launch_clock": fields[4],
            "latch_clock": fields[5],
            "line": line_number,
            "relationship_ns": relationship,
            "skew_ns": skew,
            "slack_ns": slack,
            "to": fields[3],
        })
    if not rows:
        raise ValueError("setup summary produced zero timing paths")
    return rows


Rule = tuple[str, Callable[[str], bool]]


def contains(*needles: str) -> Callable[[str], bool]:
    return lambda value: all(needle in value for needle in needles)


LAUNCH_RULES: tuple[Rule, ...] = (
    ("bank-sres-write-enable", contains("zhao_texture_v3bank:g_sres", "PORT_B_WRITE_ENABLE_REG")),
    ("owner-mask-lifetime", contains("owner_mask_lifetime_fault_q")),
    ("uv-join-lifetime", contains("zhao_texture_uv_join_v2", "lifetime_fault")),
    ("combine-product", contains("zhao_texture_material_combine_v3", "|m_p")),
    ("fragment-expand", contains("zhao_texture_frag_expand_v2")),
    ("early-descriptor-ram", contains("zhao_texture_early_desc_v2", "PORT_B_WRITE_ENABLE_REG")),
    ("bilerp-dsp2", contains("zhao_texture_bilerp_lane_dsp2")),
    ("owner-control", contains("zhao_texture_v3own")),
    ("attribute-dsp3", contains("zhao_raster_attrgrad_dsp3")),
    ("tile-control", lambda value: value.startswith("zhao_raster_tile_pipe_v2:u_tile|")
     and value.count("|") == 1),
    ("physical-boundary", lambda value: value.startswith("zhao_raster_texture_v3_fit_top|")),
)

ENDPOINT_RULES: tuple[Rule, ...] = (
    ("material-combine", contains("zhao_texture_material_combine_v3")),
    ("owner-control", contains("zhao_texture_v3own")),
    ("aux-pipe", contains("zhao_texture_aux_pipe_v2")),
    ("uv-join", contains("zhao_texture_uv_join_v2")),
    ("bilerp-dsp2", contains("zhao_texture_bilerp_lane_dsp2")),
    ("attribute-dsp3", contains("zhao_raster_attrgrad_dsp3")),
    ("other-island", contains("zhao_texture_island_v3_top:u_texture_v3")),
    ("tile-control", lambda value: value.startswith("zhao_raster_tile_pipe_v2:u_tile|")
     and value.count("|") == 1),
    ("physical-boundary", lambda value: value.startswith("zhao_raster_texture_v3_fit_top|")),
)


def classify(value: str, rules: tuple[Rule, ...]) -> str:
    for name, predicate in rules:
        if predicate(value):
            return name
    return "other"


def family_summary(paths: list[dict[str, object]], key: str,
                   rules: tuple[Rule, ...]) -> list[dict[str, object]]:
    grouped: dict[str, list[dict[str, object]]] = {}
    for path in paths:
        family = classify(str(path[key]), rules)
        grouped.setdefault(family, []).append(path)
    result = []
    for family, members in grouped.items():
        worst = min(members, key=lambda row: float(row["slack_ns"]))
        result.append({
            "count": len(members),
            "family": family,
            "worst_data_delay_ns": worst["data_delay_ns"],
            "worst_from": worst["from"],
            "worst_slack_ns": worst["slack_ns"],
            "worst_to": worst["to"],
        })
    return sorted(result, key=lambda row: (float(row["worst_slack_ns"]), str(row["family"])))


def build_payload(raw: bytes, *, input_name: str, label: str | None,
                  source_commit: str | None) -> dict[str, object]:
    paths = parse_paths(raw)
    ordered = sorted(paths, key=lambda row: (float(row["slack_ns"]), int(row["line"])))
    negative = [row for row in ordered if float(row["slack_ns"]) < 0]
    return {
        "families": {
            "endpoint": family_summary(negative, "to", ENDPOINT_RULES),
            "launch": family_summary(negative, "from", LAUNCH_RULES),
        },
        "input": {
            "path": input_name.replace("\\", "/"),
            "sha256": sha256(raw),
        },
        "label": label,
        "paths": {
            "negative": len(negative),
            "nonnegative": len(paths) - len(negative),
            "summarized": len(paths),
            "top20": ordered[:20],
            "worst_data_delay_ns": ordered[0]["data_delay_ns"],
            "worst_slack_ns": ordered[0]["slack_ns"],
            "worst_skew_ns": ordered[0]["skew_ns"],
        },
        "schema_id": SCHEMA_ID,
        "schema_version": SCHEMA_VERSION,
        "source_commit": source_commit,
    }


def canonical(payload: dict[str, object]) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")


def self_test() -> None:
    sample = (
        "; Slack ; From Node ; To Node ; Launch Clock ; Latch Clock ; Relationship ; Clock Skew ; Data Delay ;\n"
        "; -0.500 ; top|owner_mask_lifetime_fault_q ; top|zhao_texture_v3own:u|live_q[0] ; clk ; clk ; 10.000 ; -0.200 ; 9.700 ;\n"
        "; 0.125 ; top|src ; top|dst ; clk ; clk ; 10.000 ; 0.100 ; 9.775 ;\n"
    ).encode("utf-8")
    payload = build_payload(sample, input_name="sample.rpt", label="test", source_commit=None)
    assert payload["paths"]["summarized"] == 2
    assert payload["paths"]["negative"] == 1
    assert payload["families"]["launch"][0]["family"] == "owner-mask-lifetime"
    assert payload["families"]["endpoint"][0]["family"] == "owner-control"
    broken = sample.replace(b"-0.500", b"not-a-number")
    parsed = parse_paths(broken)
    assert len(parsed) == 1 and parsed[0]["slack_ns"] == 0.125
    try:
        parse_paths(b"no rows\n")
    except ValueError as exc:
        assert "zero timing paths" in str(exc)
    else:
        raise AssertionError("empty timing census did not fail")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--label")
    parser.add_argument("--source-commit")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            print("g8a timing path census self-test: PASS")
            return 0
        if args.input is None or args.output is None:
            parser.error("--input and --output are required unless --self-test is used")
        raw = args.input.read_bytes()
        payload = build_payload(
            raw,
            input_name=str(args.input),
            label=args.label,
            source_commit=args.source_commit,
        )
        args.output.parent.mkdir(parents=False, exist_ok=True)
        args.output.write_bytes(canonical(payload))
        print(
            f"wrote {args.output}: {payload['paths']['summarized']} paths, "
            f"{payload['paths']['negative']} negative"
        )
        return 0
    except (OSError, ValueError) as exc:
        print(f"g8a timing path census failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
