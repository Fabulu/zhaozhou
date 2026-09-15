#!/usr/bin/env python3
"""Build and validate Packet-F's exact G8A fit receipt from retained evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
from typing import Any


REPO = Path(__file__).resolve().parents[2]
BLOCK_REPORT = REPO / "reports/synthesis/zhao_block_fit.json"
BLOCKPATHS = REPO / "reports/synthesis/blockpaths"
FIT_MANIFEST = (
    REPO / "reports/characterization/g8a_raster_texture_single_owner_characterization"
    / "c88e2b31-20260914T165040Z-attempt2/fit.manifest.json"
)
RECEIPT = REPO / "reports/synthesis/zhao_g8a_raster_texture.json"
MODULE = "zhao_raster_texture_v3_fit_top"
ROW_NAME = MODULE + "@g8a"
REQUIRED_RAW_SUFFIXES = (
    "sources.sha256",
    "qsf",
    "sdc",
    "map.rpt",
    "map.summary",
    "fit.summary",
    "fit.rpt",
    "sta.rpt",
    "setup.rpt",
    "hold.rpt",
    "setup.summary.rpt",
)
REQUIRED_ENTITY_COUNTS = {
    "zhao_raster_texture_v3_fit_top": 1,
    "zhao_raster_tile_pipe_v2": 1,
    "zhao_raster_texture_stage_v3": 1,
    "zhao_texture_island_v3_top": 1,
    "zhao_texture_v3own": 1,
}
DSP_RESCUE_ENTITY_COUNTS = {
    "zhao_raster_attrgrad_dsp3": 3,
    "zhao_attr_mul72x13_dsp3": 3,
    "zhao_mul27_exact": 9,
    "zhao_texture_bilerp_lane_dsp2": 4,
    "zhao_dual18_mul": 4,
}
DSP_RESCUE_FORBIDDEN_ENTITIES = (
    "zhao_raster_attrgrad_v2",
    "zhao_texture_bilerp_lane_v2",
)
DSP_RESCUE_TOP_PARAMETERS = {
    "ATTR_DSP3": "1'b1",
    "BILERP_DSP2": "1'b1",
}
DSP_RESCUE_VENDOR_MACRO = "ZHAO_DUAL18_CYCLONEV=1"
FORBIDDEN_ENTITY_FRAGMENTS = ("texjoin",)
SHADOW_STATE_PATTERNS = (
    r"g_migration_shadows",
    r"shadow_metadata_m[^\r\n]*(?:~reg|~mem)",
    r'(?:Inferred RAM node|RAM logic) "[^"\r\n]*shadow_metadata_m',
    r"meta_shadow_(?:mismatch|reads)_o\[\d+\].*~reg",
    r"meta_(?:align|bil)_(?:err|chk|first_q|first_t|first_tok)_o\[\d+\].*~reg",
)


class ReceiptError(RuntimeError):
    pass


def sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def object_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ReceiptError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"),
                           object_pairs_hook=object_no_duplicates)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ReceiptError(f"cannot read strict JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ReceiptError(f"JSON root is not an object: {path}")
    return value


def exact_keys(value: Any, expected: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != expected:
        actual = set(value) if isinstance(value, dict) else type(value).__name__
        raise ReceiptError(f"{label} keys differ: expected={sorted(expected)} actual={actual}")
    return value


def manifest_source_digest(manifest: dict[str, Any]) -> tuple[str, str, int]:
    rows = manifest.get("source_closure")
    if not isinstance(rows, list) or not rows:
        raise ReceiptError("fit manifest source_closure is absent/empty")
    entries: list[tuple[str, str]] = []
    seen: set[str] = set()
    for ordinal, row in enumerate(rows):
        row = exact_keys(row, {"ordinal", "path", "sha256"},
                         f"source_closure[{ordinal}]")
        if row["ordinal"] != ordinal or not isinstance(row["path"], str):
            raise ReceiptError("fit manifest source order is malformed")
        digest = row["sha256"]
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ReceiptError("fit manifest source hash is malformed")
        leaf = Path(row["path"]).name
        if leaf in seen:
            raise ReceiptError(f"flat block-fit snapshot has duplicate basename: {leaf}")
        seen.add(leaf)
        entries.append((leaf, digest))
    entries.sort(key=lambda entry: entry[0].casefold())
    lines = [f"{digest.upper()}  {leaf}" for leaf, digest in entries]
    text = "\n".join(lines)
    return sha256_bytes(text.encode("utf-8")), text, len(lines)


def dsp_rescue_profile(manifest: dict[str, Any]) -> bool:
    parameters = manifest.get("fit_top_parameters")
    if parameters is None:
        return False
    if parameters != DSP_RESCUE_TOP_PARAMETERS:
        raise ReceiptError(
            "fit manifest DSP-rescue parameters differ: "
            f"expected={DSP_RESCUE_TOP_PARAMETERS} actual={parameters}"
        )
    return True


def parse_entity_rows(map_text: str) -> list[dict[str, Any]]:
    marker = "Analysis & Synthesis Resource Utilization by Entity"
    starts = [match.start() for match in re.finditer(re.escape(marker), map_text)]
    if not starts:
        raise ReceiptError("map report has no entity utilization table")
    section = map_text[starts[-1]:]
    end = section.find("Analysis & Synthesis RAM Summary")
    if end >= 0:
        section = section[:end]
    rows: list[dict[str, Any]] = []
    resource = re.compile(r"^(\d[\d,]*)\s*\((\d[\d,]*)\)$")
    for line in section.splitlines():
        if not line.startswith(";"):
            continue
        fields = [field.strip() for field in line.split(";")]
        if len(fields) < 11:
            continue
        alut = resource.fullmatch(fields[2])
        registers = resource.fullmatch(fields[3])
        if alut is None or registers is None:
            continue
        def number(text: str) -> int:
            match = re.search(r"\d[\d,]*", text)
            return int(match.group(0).replace(",", "")) if match else 0
        rows.append({
            "aluts": number(alut.group(1)),
            "aluts_self": number(alut.group(2)),
            "dsp": number(fields[5]),
            "entity": fields[9],
            "hierarchy": fields[8],
            "memory_bits": number(fields[4]),
            "pins": number(fields[6]),
            "registers": number(registers.group(1)),
            "registers_self": number(registers.group(2)),
            "virtual_pins": number(fields[7]),
        })
    if not rows:
        raise ReceiptError("entity utilization parser produced zero rows")
    return rows


def parse_sta_summary(sta_text: str, section_name: str) -> dict[str, float]:
    header = re.compile(
        rf"(?m)^;\s*Slow 1100mV [^;\r\n]+ Model "
        rf"{re.escape(section_name)}\s*;"
    )
    matches = list(header.finditer(sta_text))
    if len(matches) != 2:
        raise ReceiptError(
            f"expected two slow-corner {section_name} tables, got {len(matches)}"
        )
    # Match the same first slow corner used by run_block_fit's first Fmax row.
    table = sta_text[matches[0].start():matches[0].start() + 2000]
    row = re.search(
        r"(?m)^;\s*[^;]+;\s*(-?[0-9]+\.[0-9]+)\s*;"
        r"\s*(-?[0-9]+\.[0-9]+)\s*;",
        table,
    )
    if row is None:
        raise ReceiptError(f"{section_name} table has no clock/slack/TNS row")
    return {"slack_ns": float(row.group(1)), "tns_ns": float(row.group(2))}


def parse_v3_parameters(map_text: str, names: tuple[str, ...]) -> dict[str, dict[str, str]]:
    header = re.compile(
        r"(?m)^;\s*Parameter Settings for User Entity Instance:\s*"
        r"([^\r\n;]*zhao_texture_island_v3_top:u_texture_v3)\s*;\s*$"
    )
    matches = list(header.finditer(map_text))
    if len(matches) != 1:
        raise ReceiptError(f"expected one mapped V3 parameter section, got {len(matches)}")
    section = map_text[matches[0].start():matches[0].start() + 7000]
    result: dict[str, dict[str, str]] = {}
    for name in names:
        value_match = re.search(
            rf";\s*{re.escape(name)}\s*;\s*([^;]+?)\s*;", section
        )
        if value_match is None:
            raise ReceiptError(f"mapped V3 section lacks {name}")
        result[name] = {
            "instance": matches[0].group(1).strip(),
            "value": value_match.group(1).strip(),
        }
    return result


def parse_v3_parameter(map_text: str) -> dict[str, str]:
    parameter = parse_v3_parameters(map_text, ("MIGRATION_SHADOWS",))["MIGRATION_SHADOWS"]
    value = parameter["value"]
    compact = value.replace("_", "")
    if not (compact == "0" or re.fullmatch(r"0+", compact)):
        raise ReceiptError(f"mapped V3 MIGRATION_SHADOWS is not zero: {value!r}")
    return parameter


def parse_v3_bilerp_parameter(map_text: str) -> dict[str, str]:
    parameter = parse_v3_parameters(map_text, ("BILERP_DSP2",))["BILERP_DSP2"]
    value = parameter["value"]
    compact = value.replace("_", "")
    if not (compact == "1" or re.fullmatch(r"0*1", compact)):
        raise ReceiptError(f"mapped V3 BILERP_DSP2 is not one: {value!r}")
    return parameter


def parse_uninferred_ram(map_text: str) -> list[dict[str, str]]:
    return [
        {"name": name, "reason": reason.strip()}
        for name, reason in re.findall(
            r'RAM logic "([^"]+)" is uninferred due to ([A-Za-z ]+)', map_text
        )
    ]


def validate_hierarchy(entities: list[dict[str, Any]], map_text: str,
                       *, dsp_rescue_selected: bool = False) -> dict[str, Any]:
    counts: dict[str, int] = {}
    for row in entities:
        counts[row["entity"]] = counts.get(row["entity"], 0) + 1
    expected_counts = dict(REQUIRED_ENTITY_COUNTS)
    if dsp_rescue_selected:
        expected_counts.update(DSP_RESCUE_ENTITY_COUNTS)
    for entity, expected in expected_counts.items():
        if counts.get(entity, 0) != expected:
            raise ReceiptError(
                f"mapped entity count differs for {entity}: {counts.get(entity, 0)} != {expected}"
            )
    if dsp_rescue_selected:
        surviving = {name: counts.get(name, 0)
                     for name in DSP_RESCUE_FORBIDDEN_ENTITIES
                     if counts.get(name, 0) != 0}
        if surviving:
            raise ReceiptError(
                f"baseline multiplier engines survived selected DSP rescue: {surviving}"
            )
    for fragment in FORBIDDEN_ENTITY_FRAGMENTS:
        bad = sorted(name for name in counts if fragment.lower() in name.lower())
        if bad:
            raise ReceiptError(f"forbidden mapped entity fragment {fragment!r}: {bad}")
    shadow_hits = []
    for pattern in SHADOW_STATE_PATTERNS:
        shadow_hits.extend(match.group(0) for match in re.finditer(pattern, map_text, re.I))
    if shadow_hits:
        raise ReceiptError(f"mapped shadow state survived: {shadow_hits[:8]}")
    result = {
        "entity_counts": {name: counts.get(name, 0) for name in expected_counts},
        "shadow_state_matches": shadow_hits,
        "texjoin_count": sum(count for name, count in counts.items()
                             if "texjoin" in name.lower()),
    }
    if dsp_rescue_selected:
        result["baseline_engine_counts"] = {
            name: counts.get(name, 0) for name in DSP_RESCUE_FORBIDDEN_ENTITIES
        }
    return result


def validate_ram(map_text: str) -> dict[str, Any]:
    lower = map_text.lower()
    tile_context = r"zhao_raster_tilestore:u_tilestore[^\r\n]*"
    ram0 = re.search(tile_context + r"ram0", lower) is not None
    ram1 = re.search(tile_context + r"ram1", lower) is not None
    uninferred = parse_uninferred_ram(map_text)
    critical_words = ("context", "owner", "response", "material", "data_r", "tag_r")
    critical_async = [row for row in uninferred
                      if "asynchronous" in row["reason"].lower()
                      and any(word in row["name"].lower() for word in critical_words)]
    return {
        "critical_async_uninferred": critical_async,
        "pass": ram0 and ram1 and not critical_async,
        "tilestore_ram0_present": ram0,
        "tilestore_ram1_present": ram1,
        "uninferred": uninferred,
    }


def validate_fit_configuration(qsf_text: str, sdc_text: str,
                               manifest: dict[str, Any]) -> dict[str, Any]:
    rescue_selected = dsp_rescue_profile(manifest)
    source_paths = re.findall(
        r'^set_global_assignment -name SYSTEMVERILOG_FILE "([^"]+)"\s*$',
        qsf_text,
        re.MULTILINE,
    )
    normalized_sources = [path.replace("\\", "/") for path in source_paths]
    source_names = [Path(path).name for path in normalized_sources]
    expected_names = [Path(row["path"]).name for row in manifest["source_closure"]]
    source_parents = {path.rsplit("/", 1)[0] for path in normalized_sources
                      if "/" in path}
    if (source_names != expected_names or len(source_names) != len(set(source_names))):
        raise ReceiptError("retained G8A QSF source pool is not the exact ordered manifest")
    if (len(source_parents) != 1 or
            not next(iter(source_parents)).lower().endswith("/src") or
            any(not re.match(r"^[A-Za-z]:/", path) or "/../" in f"/{path}/"
                for path in normalized_sources)):
        raise ReceiptError("retained G8A QSF sources are not one absolute snapshot src directory")
    required_once = [
        "set_global_assignment -name DEVICE 5CSEBA6U23I7",
        f"set_global_assignment -name TOP_LEVEL_ENTITY {MODULE}",
        "set_global_assignment -name SDC_FILE blockfit.sdc",
        "set_global_assignment -name SEED 1",
        "# Physical top ports retained by run_block_fit.ps1 -PhysicalPins.",
        'set_global_assignment -name VERILOG_MACRO "SYNTHESIS=1"',
    ]
    if rescue_selected:
        required_once.extend((
            "set_parameter -name ATTR_DSP3 1",
            "set_parameter -name BILERP_DSP2 1",
            'set_global_assignment -name VERILOG_MACRO "ZHAO_DUAL18_CYCLONEV=1"',
        ))
    qsf_lines = qsf_text.splitlines()
    for marker in required_once:
        if qsf_lines.count(marker) != 1:
            raise ReceiptError(f"retained G8A QSF active marker is not exact: {marker}")

    def active_assignment_values(name: str) -> list[str]:
        return re.findall(
            rf"(?m)^\s*set_global_assignment\s+-name\s+{re.escape(name)}\s+(.+?)\s*$",
            qsf_text,
        )

    for name, expected in (
        ("DEVICE", "5CSEBA6U23I7"),
        ("TOP_LEVEL_ENTITY", MODULE),
        ("SDC_FILE", "blockfit.sdc"),
        ("SEED", "1"),
    ):
        if active_assignment_values(name) != [expected]:
            raise ReceiptError(
                f"retained G8A QSF active {name} assignment differs or is duplicated"
            )
    if "set_instance_assignment -name VIRTUAL_PIN" in qsf_text:
        raise ReceiptError("retained G8A QSF contains a virtual-pin assignment")
    clock = "create_clock -name clk        -period 10.000 [get_ports {clk}]"
    if sdc_text.count(clock) != 1:
        raise ReceiptError("retained G8A SDC lacks the exact 100-MHz clk constraint")

    result = {
        "physical_pin_comment": True,
        "source_count": len(source_names),
        "source_order": source_names,
        "top": MODULE,
        "virtual_pin_assignments": 0,
    }
    if rescue_selected:
        parameter_rows = re.findall(
            r"(?m)^set_parameter -name ([A-Za-z_][A-Za-z0-9_]*) ([^\s]+)\s*$",
            qsf_text,
        )
        expected_parameter_rows = [(name, "1") for name in DSP_RESCUE_TOP_PARAMETERS]
        if parameter_rows != expected_parameter_rows:
            raise ReceiptError(
                "retained G8A QSF top parameters differ: "
                f"expected={expected_parameter_rows} actual={parameter_rows}"
            )
        macros = active_assignment_values("VERILOG_MACRO")
        expected_macros = [
            '"QUARTUS_SYNTHESIS=1"',
            '"SYNTHESIS=1"',
            f'"{DSP_RESCUE_VENDOR_MACRO}"',
        ]
        if macros != expected_macros or "ZHAO_DUAL18_BEHAVIORAL" in qsf_text:
            raise ReceiptError(
                "retained G8A QSF macro closure is not exact vendor-only DSP rescue"
            )
        result.update({
            "dsp_rescue_selected": True,
            "top_parameters": dict(parameter_rows),
            "verilog_macros": [value.strip('"') for value in macros],
        })
    return result


def raw_paths() -> dict[str, Path]:
    return {suffix: BLOCKPATHS / f"{ROW_NAME}.{suffix}"
            for suffix in REQUIRED_RAW_SUFFIXES}


def select_row(report: dict[str, Any]) -> dict[str, Any]:
    rows = report.get("blocks")
    if not isinstance(rows, list):
        raise ReceiptError("block-fit report has no blocks array")
    matches = [row for row in rows
               if isinstance(row, dict) and row.get("module") == ROW_NAME]
    if len(matches) != 1:
        raise ReceiptError(f"expected one G8A block-fit row, got {len(matches)}")
    return matches[0]


def build_receipt(*, require_current_head: bool) -> dict[str, Any]:
    report = load_json(BLOCK_REPORT)
    manifest = load_json(FIT_MANIFEST)
    row = select_row(report)
    expected_digest, expected_manifest_text, source_count = manifest_source_digest(manifest)
    rescue_selected = dsp_rescue_profile(manifest)
    for key in ("sourceCommit", "sourceDigest", "status", "ioMode"):
        if not isinstance(row.get(key), str):
            raise ReceiptError(f"G8A row lacks string {key}")
    if not re.fullmatch(r"[0-9a-f]{40}", row["sourceCommit"]):
        raise ReceiptError("G8A sourceCommit is malformed")
    if require_current_head:
        import subprocess
        head = subprocess.check_output(
            ["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True
        ).strip()
        if row["sourceCommit"] != head:
            raise ReceiptError(f"G8A row measured {row['sourceCommit']}, current HEAD is {head}")
    if row.get("treeCleanAtHead") is not True or row.get("rtlCleanAtHead") is not True:
        raise ReceiptError("G8A row was not measured from a fully clean tree")
    if row["ioMode"] != "physical-top-ports":
        raise ReceiptError(f"G8A row has wrong I/O mode: {row['ioMode']}")
    if row.get("sourcesHashed") != source_count or row["sourceDigest"] != expected_digest:
        raise ReceiptError("G8A row source count/digest differs from generated manifest")
    if rescue_selected:
        expected_top_parameters = " ".join(
            f"{name}=1" for name in DSP_RESCUE_TOP_PARAMETERS
        )
        if row.get("topParameters") != expected_top_parameters:
            raise ReceiptError(
                "G8A row top-parameter provenance differs from selected DSP rescue"
            )
        if row.get("verilogMacros") != [DSP_RESCUE_VENDOR_MACRO]:
            raise ReceiptError(
                "G8A row Verilog-macro provenance differs from vendor DSP rescue"
            )

    paths = raw_paths()
    for label, path in paths.items():
        if not path.is_file():
            raise ReceiptError(f"G8A retained raw artifact missing: {label}: {path}")
    source_file_text = paths["sources.sha256"].read_text(encoding="utf-8-sig").rstrip("\r\n")
    if source_file_text != expected_manifest_text:
        raise ReceiptError("retained block-fit source manifest differs from generated authority")
    qsf_text = paths["qsf"].read_text(encoding="utf-8-sig")
    sdc_text = paths["sdc"].read_text(encoding="utf-8-sig")
    fit_configuration = validate_fit_configuration(qsf_text, sdc_text, manifest)

    map_text = paths["map.rpt"].read_text(encoding="utf-8", errors="replace")
    sta_text = paths["sta.rpt"].read_text(encoding="utf-8", errors="replace")
    entities = parse_entity_rows(map_text)
    hierarchy = validate_hierarchy(
        entities, map_text, dsp_rescue_selected=rescue_selected)
    parameters = {"MIGRATION_SHADOWS": parse_v3_parameter(map_text)}
    if rescue_selected:
        parameters["BILERP_DSP2"] = parse_v3_bilerp_parameter(map_text)
        parameters["ATTR_DSP3"] = {
            "instance": MODULE,
            "value": fit_configuration["top_parameters"]["ATTR_DSP3"],
        }
    ram = validate_ram(map_text)
    setup = parse_sta_summary(sta_text, "Setup Summary")
    hold = parse_sta_summary(sta_text, "Hold Summary")

    numeric = {}
    for key in (
        "alms", "registers", "blockMemoryBits", "ramBlocks", "dspBlocks",
        "virtualPins", "fitterSeed", "fmaxMhz",
    ):
        value = row.get(key)
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            raise ReceiptError(f"G8A row lacks numeric {key}")
        numeric[key] = value
    retained_timing = {
        "setupSlackNs": setup["slack_ns"],
        "setupTnsNs": setup["tns_ns"],
        "holdSlackNs": hold["slack_ns"],
        "holdTnsNs": hold["tns_ns"],
    }
    for key, value in retained_timing.items():
        row_value = row.get(key)
        if row_value is not None and row_value != value:
            raise ReceiptError(
                f"G8A row {key} differs from retained STA report: "
                f"{row_value} != {value}"
            )
        numeric[key] = value
    if numeric["virtualPins"] != 0:
        raise ReceiptError(f"G8A mapped {numeric['virtualPins']} virtual pins")
    if numeric["fitterSeed"] != 1:
        raise ReceiptError(f"G8A seed is not the pinned seed 1: {numeric['fitterSeed']}")

    timing_pass = numeric["fmaxMhz"] >= 100
    resource_pass = numeric["alms"] <= 29999 and numeric["dspBlocks"] <= 84
    dsp_rescue_target_pass = (not rescue_selected or numeric["dspBlocks"] <= 30)
    structure_pass = hierarchy["texjoin_count"] == 0 and ram["pass"]
    fit_complete = row["status"] in {"ok", "failed:structure"}
    gate_pass = (fit_complete and timing_pass and resource_pass and structure_pass
                 and dsp_rescue_target_pass and row["status"] == "ok")
    gate = {
        "fit_complete": fit_complete,
        "pass": gate_pass,
        "ram_inference_pass": ram["pass"],
        "resource_pass": resource_pass,
        "structure_pass": structure_pass,
        "timing_100mhz_pass": timing_pass,
    }
    if rescue_selected:
        gate["dsp_rescue_target_pass"] = dsp_rescue_target_pass

    raw = {
        label: {
            "path": str(path.relative_to(REPO)).replace(os.sep, "/"),
            "sha256": sha256_file(path),
        }
        for label, path in paths.items()
    }
    top_entity = next(row_entity for row_entity in entities
                      if row_entity["entity"] == MODULE)
    if top_entity["pins"] != 18 or top_entity["virtual_pins"] != 0:
        raise ReceiptError(
            f"G8A top boundary is not 18 physical/0 virtual pins: {top_entity}"
        )
    tile_entity = next(row_entity for row_entity in entities
                       if row_entity["entity"] == "zhao_raster_tile_pipe_v2")
    return {
        "device": report.get("device"),
        "entity_attribution": {
            "all_rows": entities,
            "tile": tile_entity,
            "top": top_entity,
        },
        "fit_configuration": fit_configuration,
        "gate": gate,
        "hierarchy": hierarchy,
        "io_mode": row["ioMode"],
        "limitations": manifest.get("limitations"),
        "module": MODULE,
        "parameters": parameters,
        "ram_witness": ram,
        "raw_artifacts": raw,
        "resources": numeric,
        "row_status": row["status"],
        "rule_violations": row.get("ruleViolations", []),
        "schema_id": "zhao.g8a.fit_receipt",
        "schema_version": 2 if rescue_selected else 1,
        "source": {
            "commit": row["sourceCommit"],
            "digest": row["sourceDigest"],
            "fit_manifest_path": str(FIT_MANIFEST.relative_to(REPO)).replace(os.sep, "/"),
            "fit_manifest_sha256": sha256_file(FIT_MANIFEST),
            "rtl_clean_at_head": row["rtlCleanAtHead"],
            "sources_hashed": row["sourcesHashed"],
            "tree_clean_at_head": row["treeCleanAtHead"],
        },
        "tool": report.get("tool"),
    }


def validate_receipt(payload: dict[str, Any]) -> None:
    exact_keys(payload, {
        "device", "entity_attribution", "fit_configuration", "gate", "hierarchy", "io_mode",
        "limitations", "module", "parameters", "ram_witness", "raw_artifacts",
        "resources", "row_status", "rule_violations", "schema_id",
        "schema_version", "source", "tool",
    }, "receipt")
    if (payload["schema_id"] != "zhao.g8a.fit_receipt" or
            payload["schema_version"] not in {1, 2}):
        raise ReceiptError("G8A receipt schema identity differs")
    rescue_selected = payload["schema_version"] == 2
    if payload["module"] != MODULE or payload["io_mode"] != "physical-top-ports":
        raise ReceiptError("G8A receipt module/I/O mode differs")
    fit_keys = {
        "physical_pin_comment", "source_count", "source_order", "top",
        "virtual_pin_assignments",
    }
    if rescue_selected:
        fit_keys.update({
            "dsp_rescue_selected", "top_parameters", "verilog_macros",
        })
    fit_configuration = exact_keys(
        payload["fit_configuration"], fit_keys, "receipt.fit_configuration")
    expected_source_count = 48 if rescue_selected else 43
    if (fit_configuration["source_count"] != expected_source_count or
            fit_configuration["virtual_pin_assignments"] != 0 or
            fit_configuration["top"] != MODULE):
        raise ReceiptError("G8A receipt fit configuration differs")
    if rescue_selected:
        expected_parameters = {name: "1" for name in DSP_RESCUE_TOP_PARAMETERS}
        if (fit_configuration["dsp_rescue_selected"] is not True or
                fit_configuration["top_parameters"] != expected_parameters or
                fit_configuration["verilog_macros"] != [
                    "QUARTUS_SYNTHESIS=1", "SYNTHESIS=1",
                    DSP_RESCUE_VENDOR_MACRO,
                ]):
            raise ReceiptError("G8A receipt does not bind the exact DSP-rescue fit selection")
    source = exact_keys(payload["source"], {
        "commit", "digest", "fit_manifest_path", "fit_manifest_sha256",
        "rtl_clean_at_head", "sources_hashed", "tree_clean_at_head",
    }, "receipt.source")
    if source["tree_clean_at_head"] is not True or source["rtl_clean_at_head"] is not True:
        raise ReceiptError("G8A receipt admits dirty source")
    if source["sources_hashed"] != expected_source_count:
        raise ReceiptError("G8A receipt source count disagrees with its profile")
    resources = payload["resources"]
    if resources.get("virtualPins") != 0:
        raise ReceiptError("G8A receipt admits virtual pins")
    parameters = payload["parameters"]
    expected_parameter_keys = {"MIGRATION_SHADOWS"}
    if rescue_selected:
        expected_parameter_keys.update(DSP_RESCUE_TOP_PARAMETERS)
    if not isinstance(parameters, dict) or set(parameters) != expected_parameter_keys:
        raise ReceiptError("G8A receipt parameter inventory differs")
    migration = parameters.get("MIGRATION_SHADOWS")
    if (not isinstance(migration, dict) or
            not re.fullmatch(r"0+", migration.get("value", "").replace("_", ""))):
        raise ReceiptError("G8A receipt does not prove mapped MIGRATION_SHADOWS=0")
    if rescue_selected:
        for name in DSP_RESCUE_TOP_PARAMETERS:
            parameter = parameters.get(name)
            if (not isinstance(parameter, dict) or
                    not re.fullmatch(r"0*1", parameter.get("value", "").replace("_", ""))):
                raise ReceiptError(f"G8A receipt does not prove selected {name}=1")

    hierarchy = payload["hierarchy"]
    if hierarchy.get("texjoin_count") != 0 or hierarchy.get("shadow_state_matches") != []:
        raise ReceiptError("G8A receipt admits TEXJOIN or shadow state")
    expected_entities = dict(REQUIRED_ENTITY_COUNTS)
    if rescue_selected:
        expected_entities.update(DSP_RESCUE_ENTITY_COUNTS)
        if hierarchy.get("baseline_engine_counts") != {
                name: 0 for name in DSP_RESCUE_FORBIDDEN_ENTITIES}:
            raise ReceiptError("G8A receipt retains a baseline ATTR/BIL engine")
    for entity, expected in expected_entities.items():
        if hierarchy.get("entity_counts", {}).get(entity) != expected:
            raise ReceiptError(f"G8A receipt hierarchy count differs for {entity}")

    gate_keys = {
        "fit_complete", "pass", "ram_inference_pass", "resource_pass",
        "structure_pass", "timing_100mhz_pass",
    }
    if rescue_selected:
        gate_keys.add("dsp_rescue_target_pass")
    gate = exact_keys(payload["gate"], gate_keys, "receipt.gate")
    if not isinstance(gate.get("pass"), bool):
        raise ReceiptError("G8A receipt gate.pass is not Boolean")
    if rescue_selected:
        dsp_blocks = resources.get("dspBlocks")
        expected_dsp_pass = (isinstance(dsp_blocks, (int, float)) and
                             not isinstance(dsp_blocks, bool) and dsp_blocks <= 30)
        if gate["dsp_rescue_target_pass"] is not expected_dsp_pass:
            raise ReceiptError("G8A receipt DSP-rescue target gate is inconsistent")


def write_atomic(path: Path, raw: bytes) -> None:
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=path.name + ".", suffix=".tmp",
            dir=path.parent, delete=False,
        ) as stream:
            temporary = stream.name
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass


def canonical(payload: dict[str, Any]) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--repair-retained", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        expected = build_receipt(require_current_head=args.write)
        validate_receipt(expected)
        raw = canonical(expected)
        if args.write or args.repair_retained:
            if args.repair_retained:
                if not RECEIPT.is_file():
                    raise ReceiptError("retained-repair requires an existing receipt")
                previous = load_json(RECEIPT)
                previous_source = previous.get("source")
                if (not isinstance(previous_source, dict) or
                        previous_source.get("commit") != expected["source"]["commit"]):
                    raise ReceiptError(
                        "retained-repair source commit differs from existing receipt"
                    )
            write_atomic(RECEIPT, raw)
            mode_name = "retained-repair" if args.repair_retained else "write"
            print(
                f"wrote {RECEIPT.relative_to(REPO)} mode={mode_name} "
                f"gate_pass={expected['gate']['pass']}"
            )
            return 0 if expected["gate"]["pass"] else 2
        if not RECEIPT.is_file() or RECEIPT.read_bytes() != raw:
            raise ReceiptError("committed G8A receipt is absent or stale")
        actual = load_json(RECEIPT)
        validate_receipt(actual)
        print(f"G8A receipt fresh: gate_pass={actual['gate']['pass']}")
        return 0 if actual["gate"]["pass"] else 2
    except (OSError, ReceiptError) as exc:
        print(f"G8A receipt failed: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
