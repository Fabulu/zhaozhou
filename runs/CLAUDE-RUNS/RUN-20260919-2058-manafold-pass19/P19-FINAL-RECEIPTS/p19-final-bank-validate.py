from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

REPO = Path(r"C:\programmieren\zencrifice\manafold-p16\zhaozhou")
sys.path.insert(0, str(REPO / "tools" / "reel"))
from rgbframe import load  # noqa: E402

EXPECTED = {
    "manafold-blown": 292,
    "manafold-channel": 420,
    "manafold-crackle": 600,
    "manafold-curious": 180,
    "manafold-damage": 464,
    "manafold-death-drop": 450,
    "manafold-death-gutter": 590,
    "manafold-drift": 300,
    "manafold-fall": 340,
    "manafold-flight": 352,
    "manafold-hasty": 240,
    "manafold-hit": 140,
    "manafold-hover": 600,
    "manafold-inspect": 600,
    "manafold-lasso": 336,
    "manafold-pirouette": 240,
    "manafold-rest": 400,
    "manafold-startle": 160,
    "manafold-taunt": 280,
    "manafold-taunt2": 240,
    "manafold-taunt3": 368,
    "manafold-trick": 400,
}


def parse_meta(path: Path) -> tuple[str, int, str, list[int]]:
    text = path.read_text(encoding="utf-8")
    subject_match = re.search(r"^subject=(.+)$", text, re.MULTILINE)
    frames_match = re.search(r"^frames=(\d+)\b", text, re.MULTILINE)
    sequence_match = re.search(r"^sequence_crc32c=(0x[0-9A-Fa-f]{8})$", text, re.MULTILINE)
    if not subject_match or not frames_match or not sequence_match:
        raise ValueError(f"{path}: missing subject/frames/sequence_crc32c")
    receipt_indices = [
        int(m.group(1))
        for m in re.finditer(r"^frame_(\d{4})_crc32c=0x[0-9A-Fa-f]{8}$", text, re.MULTILINE)
    ]
    return (
        subject_match.group(1),
        int(frames_match.group(1)),
        sequence_match.group(1).upper().replace("0X", "0x"),
        receipt_indices,
    )


def validate(root: Path) -> dict:
    if not root.is_dir():
        raise ValueError(f"missing bank root: {root}")
    actual_dirs = {p.name for p in root.iterdir() if p.is_dir()}
    expected_dirs = set(EXPECTED)
    if actual_dirs != expected_dirs:
        raise ValueError(
            f"subject set mismatch: missing={sorted(expected_dirs - actual_dirs)} "
            f"extra={sorted(actual_dirs - expected_dirs)}"
        )

    rows = []
    total_frames = 0
    total_bytes = 0
    for subject in sorted(EXPECTED):
        expected_count = EXPECTED[subject]
        directory = root / subject
        meta_subject, meta_count, sequence_crc, receipt_indices = parse_meta(directory / "meta.txt")
        if meta_subject != subject:
            raise ValueError(f"{subject}: metadata subject={meta_subject!r}")
        if meta_count != expected_count:
            raise ValueError(f"{subject}: metadata frames={meta_count}, expected={expected_count}")
        expected_indices = list(range(expected_count))
        if receipt_indices != expected_indices:
            raise ValueError(
                f"{subject}: receipt indices are not exactly contiguous 0..{expected_count - 1}"
            )

        frames = sorted(directory.glob("*.rgb"))
        names = [p.name for p in frames]
        expected_names = [f"{i:04d}.rgb" for i in expected_indices]
        if names != expected_names:
            raise ValueError(f"{subject}: RGB names are not exactly contiguous zero-based filenames")

        sequence_sha = hashlib.sha256()
        subject_bytes = 0
        for frame in frames:
            load(frame)
            raw = frame.read_bytes()
            sequence_sha.update(raw)
            subject_bytes += len(raw)
        total_frames += len(frames)
        total_bytes += subject_bytes
        rows.append(
            {
                "subject": subject,
                "frames": len(frames),
                "sequence_crc32c": sequence_crc,
                "sequence_sha256": sequence_sha.hexdigest(),
                "bytes": subject_bytes,
                "newest_frame_mtime_ns": max(p.stat().st_mtime_ns for p in frames),
            }
        )

    if total_frames != 7992:
        raise ValueError(f"bank has {total_frames} frames, expected 7992")

    manifest_text = "".join(
        f"{row['subject']}\t{row['frames']}\t{row['sequence_crc32c']}\t"
        f"{row['sequence_sha256']}\t{row['bytes']}\n"
        for row in rows
    )
    manifest_sha = hashlib.sha256(manifest_text.encode("utf-8")).hexdigest()
    result = {
        "root": str(root),
        "subjects": len(rows),
        "frames": total_frames,
        "bytes": total_bytes,
        "manifest_sha256": manifest_sha,
        "manifest_text": manifest_text,
        "rows": rows,
    }
    return result


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: p19-final-bank-validate.py BANK_ROOT OUT_JSON")
    root = Path(sys.argv[1])
    out = Path(sys.argv[2])
    result = validate(root)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        f"PASS subjects={result['subjects']} frames={result['frames']} "
        f"bytes={result['bytes']} manifest_sha256={result['manifest_sha256']}"
    )


if __name__ == "__main__":
    main()
