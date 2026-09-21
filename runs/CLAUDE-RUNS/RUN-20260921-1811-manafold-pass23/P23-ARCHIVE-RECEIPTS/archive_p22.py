#!/usr/bin/env python3
"""Archive Manafold pass 22: verify the 44 live files against the published
pass-22 receipt, copy them to immutable archive-p22-manafold-* names, re-hash
the copies, and write P22-ARCHIVE-SHA256.txt.

This is the pass-21 archive tool (itself V18's, via passes 19, 20, 21 and 22) repointed one generation later,
unchanged in shape on purpose -- the archive step is the one place in the pass
where "the same thing as last time" is the requirement rather than a compromise.
It refuses to write anything unless all 44 live files are exactly the published,
production-verified bytes, so an archive can never record a generation that was
not served.

⚠ THIS RUNS BEFORE THE PASS-23 ENCODE. The encode overwrites the live
renders/manafold-* names in place; once it has run, the pass-22 bytes exist
nowhere but production. The ordering is the whole safety property.
"""
import hashlib, shutil
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
CREATURE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\creature\Manafold")
PUBLIC = SITE / "public"
LIVE_RECEIPT = CREATURE / "P22-LIVE-MEDIA-SHA256.txt"
OUT = CREATURE / "P22-ARCHIVE-SHA256.txt"


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
        print(f"FAIL {bad} live files are not the published pass-22 bytes; nothing copied")
        return 1
    print(f"OK live files verified against the published pass-22 receipt {len(rows)}/{len(rows)}")

    # 2. copy to the immutable archive twins and re-hash the COPIES
    out_rows, total, ok = [], 0, 0
    for digest, size, source in rows:
        archive = source.replace("renders/manafold-", "renders/archive-p22-manafold-")
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
        "# Manafold pass 22 immutable archive \u2014 source/archive SHA-256 receipt",
        "# Source: production-verified Upheaval main ca51e26d023c113b37ec0625622dba4b5467eb06"
        " / Zhaozhou 0d796c4a582a9396ff8bce5b2a0486d97138d3cb,"
        " renderer MD5 8a0aa4da7f35a24c8415848268a5cf98, source zhaozhou 0a743562,"
        " bank manifest b188f2efa85649821bf5578bbfb1cea25511c214ea48040195d5a579b1777d6e",
        "# Every row also equals P22-LIVE-MEDIA-SHA256.txt (the production-verified pass-22 live receipt)",
        f"# files={ok} bytes={total}",
        "# sha256<TAB>bytes<TAB>source<TAB>archive",
    ]
    body = [f"{d}\t{s}\t{src}\t{a}" for d, s, src, a in sorted(out_rows, key=lambda r: r[2])]
    OUT.write_text("\n".join(hdr + body) + "\n", encoding="utf-8")
    print(f"OK wrote {OUT} ({ok} rows, {total} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
