#!/usr/bin/env python3
"""bitident.py -- is creature 01 STILL BYTE-FOR-BYTE WHAT IT WAS?

Every edit to shared engine code (`creature_core.cpp`, `zref_creature.hpp`) or
to the reel has to re-prove that Zixxtrixx's approved, published bytes did not
move. That proof has been rebuilt by hand in at least four passes and thrown
away every time, so its numbers are unreproducible and each pass ends up
arguing with the last one's quoted figures. CLAUDE.md's ground-contact rule
says exactly what to do about that: COMMIT THE PROBE.

    python tools/reel/bitident.py --exe-a <base.exe> --exe-b <new.exe> \
        --work <scratch dir> [--ids <file>] [--source <zhao_reel.cpp>] \
        [--env K=V ...] [--jobs N] [--keep-frames]

WHAT IT DOES, AND WHY IT DOES IT THIS WAY
-----------------------------------------
* **Two independent metrics per subject**, because they fail differently: the
  reel's own printed `sequence_crc32c` (computed in memory, so it cannot see a
  corrupted file on disk) and a sha256 over every frame's raw bytes (which
  can). PASS-12-QA caught a planted one-byte flip on the hash leg while the
  CRC leg stayed correctly green -- that disagreement is the point of having
  two.
* **EMPTY IS NOT IDENTICAL.** Two subjects (`zixxtrixx-sweep`,
  `zixxtrixx-fall-baked`) sit behind `#ifdef`s that `build-direct.sh` defines
  in neither tree, so they render nothing from either binary. A sha256 of
  nothing equals a sha256 of nothing, and a harness that scores that
  "identical" is scoring its own silence. They are reported by name and kept
  out of the pass count.
* **A distinctness check**: N non-empty subjects must yield N distinct CRCs.
  If anything ever collapsed the CRC to a constant, every row would agree and
  the harness would report a perfect pass.
* **A per-frame identical count** as well as a verdict, so a partial
  difference cannot hide behind one boolean.
* Frames are deleted as soon as they are hashed. 129,000 orphaned `.rgb`
  intermediates once filled this machine's C: drive to zero bytes free
  (CLAUDE.md). Pass --keep-frames only when you want to look at pixels.

EXIT CODE: 0 when every non-empty subject matches on BOTH metrics, else 1.

Run it BOTH WAYS ROUND. Green against a default-off change proves the change
is inert; it does not prove the harness can see anything. Build a deliberately
mutated binary and this must go red, or the green meant nothing.
"""
import argparse
import concurrent.futures
import hashlib
import os
import re
import shutil
import subprocess
import sys

CRC_RE = re.compile(
    r"^(\S+): (\d+) frames, \d+ unique colours, sequence_crc32c=0x([0-9A-Fa-f]{8})", re.M)


def ids_from_source(source, prefix):
    """Enumerate DISPATCHABLE subject ids from the reel source.

    Deliberately not a hardcoded list: the list one committed script carried
    had drifted to 16 while 71 ids actually existed, and a harness that checks
    a stale subset reports a pass it did not earn.
    """
    with open(source, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    found = re.findall(r'wanted\("(' + re.escape(prefix) + r'[a-z0-9-]*)"\)', text)
    return sorted(set(found))


def hash_dir(d):
    """(sha256 over every frame's bytes, frame count, per-frame digests)."""
    if not os.path.isdir(d):
        return None, 0, []
    names = sorted(n for n in os.listdir(d) if n.endswith(".rgb"))
    h = hashlib.sha256()
    per = []
    for n in names:
        with open(os.path.join(d, n), "rb") as fh:
            b = fh.read()
        h.update(b)
        per.append(hashlib.sha256(b).hexdigest())
    return h.hexdigest(), len(names), per


def run_one(exe, out_root, subject, env):
    os.makedirs(out_root, exist_ok=True)
    e = dict(os.environ)
    e.update(env)
    p = subprocess.run([exe, out_root, subject], env=e, capture_output=True, text=True)
    crc = None
    for m in CRC_RE.finditer(p.stdout or ""):
        if m.group(1) == subject:
            crc = m.group(3).upper()
    return p.returncode, crc


def subject_job(args, subject):
    row = {"subject": subject}
    dirs = []
    try:
        for tag, exe in (("a", args.exe_a), ("b", args.exe_b)):
            root = os.path.join(args.work, tag, subject.replace("/", "_"))
            shutil.rmtree(root, ignore_errors=True)
            rc, crc = run_one(exe, root, subject, args.env_map)
            frames_dir = os.path.join(root, subject)
            if not os.path.isdir(frames_dir):
                frames_dir = root
            digest, n, per = hash_dir(frames_dir)
            row[tag] = {"rc": rc, "crc": crc, "sha": digest, "n": n, "per": per}
            dirs.append(root)
    finally:
        if not args.keep_frames:
            for d in dirs:
                shutil.rmtree(d, ignore_errors=True)
    a, b = row["a"], row["b"]
    if a["n"] == 0 and b["n"] == 0:
        row["verdict"] = "EMPTY"
    elif a["n"] != b["n"]:
        row["verdict"] = "DIFFERS"
    else:
        same_frames = sum(1 for x, y in zip(a["per"], b["per"]) if x == y)
        row["same_frames"] = same_frames
        crc_ok = a["crc"] is not None and a["crc"] == b["crc"]
        sha_ok = a["sha"] == b["sha"]
        row["verdict"] = "IDENTICAL" if (crc_ok and sha_ok) else "DIFFERS"
        row["crc_ok"], row["sha_ok"] = crc_ok, sha_ok
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe-a", required=True)
    ap.add_argument("--exe-b", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--ids", help="file of subject ids, one per line")
    ap.add_argument("--source", help="reel source to enumerate ids from")
    ap.add_argument("--prefix", default="zixxtrixx-")
    ap.add_argument("--env", action="append", default=[], metavar="K=V")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--keep-frames", action="store_true")
    ap.add_argument("--label", default="")
    args = ap.parse_args()

    args.env_map = dict(kv.split("=", 1) for kv in args.env)
    if args.ids:
        with open(args.ids, encoding="utf-8") as fh:
            subjects = [l.strip() for l in fh if l.strip() and not l.startswith("#")]
    elif args.source:
        subjects = ids_from_source(args.source, args.prefix)
    else:
        ap.error("one of --ids / --source is required")
    if not subjects:
        print("bitident: NO SUBJECTS -- refusing to report a pass over nothing")
        return 1

    os.makedirs(args.work, exist_ok=True)
    print("bitident %s" % (args.label or ""))
    print("  A   %s" % args.exe_a)
    print("  B   %s" % args.exe_b)
    print("  env %s" % (args.env_map or "(inherited only)"))
    print("  %d subjects" % len(subjects))
    print("")

    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for row in ex.map(lambda s: subject_job(args, s), subjects):
            rows.append(row)
            if row["verdict"] == "EMPTY":
                print("  %-38s  ---- EMPTY (absent from BOTH binaries; not a pass)"
                      % row["subject"])
            else:
                print("  %-38s  %5d frames  %5d identical  %s"
                      % (row["subject"], row["a"]["n"], row.get("same_frames", 0),
                         row["verdict"]))
            sys.stdout.flush()

    ident = [r for r in rows if r["verdict"] == "IDENTICAL"]
    diff = [r for r in rows if r["verdict"] == "DIFFERS"]
    empty = [r for r in rows if r["verdict"] == "EMPTY"]
    frames = sum(r["a"]["n"] for r in ident)
    same = sum(r.get("same_frames", 0) for r in ident)

    crcs = [r["a"]["crc"] for r in rows if r["verdict"] != "EMPTY" and r["a"]["crc"]]
    distinct = len(set(crcs))

    print("")
    print("  IDENTICAL %d   DIFFERS %d   EMPTY %d   of %d"
          % (len(ident), len(diff), len(empty), len(rows)))
    print("  frames %d/%d identical across the matching subjects" % (same, frames))
    print("  distinct CRCs %d of %d non-empty subjects%s"
          % (distinct, len(crcs),
             "  <-- COLLAPSED, the metric is not discriminating"
             if distinct < len(crcs) else ""))
    for r in empty:
        print("  EMPTY, named not counted: %s" % r["subject"])
    for r in diff:
        print("  DIFFERS: %-34s crc %s vs %s  frames %d vs %d"
              % (r["subject"], r["a"]["crc"], r["b"]["crc"], r["a"]["n"], r["b"]["n"]))
    return 1 if diff else 0


if __name__ == "__main__":
    sys.exit(main())
