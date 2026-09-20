#!/usr/bin/env python3
"""Archive Manafold pass 19: verify the 44 live files against the published
pass-19 receipt, copy them to immutable archive-p19-manafold-* names, re-hash
the copies, and write P19-ARCHIVE-SHA256.txt.

This is the pass-19 tool (V18's) repointed one generation later. It refuses to
write anything unless all 44 live files are exactly the published bytes.
"""
import hashlib, shutil, sys
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
CREATURE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\creature\Manafold")
PUBLIC = SITE / "public"
LIVE_RECEIPT = CREATURE / "P19-LIVE-MEDIA-SHA256.txt"
OUT = CREATURE / "P19-ARCHIVE-SHA256.txt"


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def main():
    rows = []
    for raw in LIVE_RECEIPT.read_text(encoding="utf-8").splitlines():
        if not raw.strip() or raw.startswith("#"):
            continue
        digest, size, source = raw.split()
        rows.append((digest, int(size), source))
    if len(rows) != 44:
        print(f"FAIL live receipt has {len(rows)} rows, expected 44")
        return 1

    # 1. every live file is exactly the published bytes
    bad = 0
    for digest, size, source in rows:
        p = PUBLIC / source
        if not p.is_file():
            print(f"FAIL live file absent {source}"); bad += 1; continue
        if p.stat().st_size != size or sha256(p) != digest:
            print(f"FAIL live file differs from the published receipt {source}"); bad += 1
    if bad:
        print(f"FAIL {bad} live files are not the published pass-19 bytes; nothing copied")
        return 1
    print(f"OK live files verified against the published pass-19 receipt {len(rows)}/{len(rows)}")

    # 2. copy to the immutable archive twins and re-hash the COPIES
    out_rows, total, ok = [], 0, 0
    for digest, size, source in rows:
        archive = source.replace("renders/manafold-", "renders/archive-p19-manafold-")
        src, dst = PUBLIC / source, PUBLIC / archive
        shutil.copy2(src, dst)
        adigest, asize = sha256(dst), dst.stat().st_size
        if adigest != digest or asize != size:
            print(f"FAIL archive copy differs {archive}")
            return 1
        out_rows.append((digest, size, source, archive))
        total += size; ok += 1
    print(f"OK archive copies re-hashed {ok}/{len(rows)}, {total} bytes")

    hdr = [
        "# Manafold pass 19 immutable archive \u2014 source/archive SHA-256 receipt",
        "# Source: production-verified Upheaval main f9e98bae (deploy 452e401c) / Zhaozhou 10877707,"
        " renderer MD5 776d55758933d5284147b360ff2eda62,"
        " bank manifest f7edc1fbe25016970d6adc830fbea925954fb7d7282297478375c0690f036ed5",
        "# Every row also equals P19-LIVE-MEDIA-SHA256.txt (the production-verified pass-19 live receipt)",
        f"# files={ok} bytes={total}",
        "# sha256<TAB>bytes<TAB>source<TAB>archive",
    ]
    body = [f"{d}\t{s}\t{src}\t{a}" for d, s, src, a in sorted(out_rows, key=lambda r: r[2])]
    OUT.write_text("\n".join(hdr + body) + "\n", encoding="utf-8")
    print(f"OK wrote {OUT} ({ok} rows, {total} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
