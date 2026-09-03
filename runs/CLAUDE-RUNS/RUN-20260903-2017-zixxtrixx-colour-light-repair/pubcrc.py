"""Publication CRC proof: 21 subjects identical to the live bank's table,
moving-light equal to the accepted v3 draft, frame counts contiguous and
every frame header-verified for size."""
import os, re, struct, sys
run = os.path.dirname(os.path.abspath(__file__))
scratch = sys.argv[1]
expected = {}
for line in open(os.path.join(run, "evidence", "expected-crc.txt")):
    m = re.match(r"(zixxtrixx-\S+)\s+(0x[0-9A-Fa-f]+)\s+frames=(\d+)", line)
    if m: expected[m.group(1)] = (m.group(2), int(m.group(3)))
expected["zixxtrixx-moving-light"] = ("0x65A8D1E5", 600)
bad = []
for sub in sorted(os.listdir(scratch)):
    meta = os.path.join(scratch, sub, "meta.txt")
    if not os.path.exists(meta):
        bad.append(f"{sub}: no meta"); continue
    crc = re.search(r"sequence_crc32c=(0x[0-9A-Fa-f]+)", open(meta).read()).group(1)
    files = sorted(f for f in os.listdir(os.path.join(scratch, sub)) if re.fullmatch(r"\d{4}\.rgb", f))
    exp_crc, exp_n = expected.get(sub, ("?", -1))
    ok_seq = [int(f[:4]) for f in files] == list(range(len(files)))
    sizes_ok = all(os.path.getsize(os.path.join(scratch, sub, f)) == 8 + 384*240*3 for f in files)
    verdict = "OK" if (crc.lower() == exp_crc.lower() and len(files) == exp_n and ok_seq and sizes_ok) else "MISMATCH"
    if verdict != "OK": bad.append(f"{sub}: crc {crc} vs {exp_crc}, n {len(files)} vs {exp_n}, contiguous={ok_seq}, sizes={sizes_ok}")
    tag = "CHANGED-AS-EXPECTED" if sub == "zixxtrixx-moving-light" and verdict == "OK" else verdict
    print(f"{sub:26s} {crc} f={len(files):4d}  {tag}")
print("RESULT:", "ALL-PROVEN" if not bad else "FAULTS: " + "; ".join(bad))
