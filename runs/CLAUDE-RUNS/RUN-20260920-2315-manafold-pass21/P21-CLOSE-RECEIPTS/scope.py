#!/usr/bin/env python3
"""Pass-20 scope attribution.

Three complete 22-subject banks from the SAME hashed binary and the same
production environment:

  shipping  -- what ships
  pass19    -- KNEAD_DIP_PM=0 REAR_BOW=legacy FOLD_DIP_PM=0
               (ALL THREE pass-20 mechanisms off: must reproduce PASS 19 exactly)
  rearonly  -- KNEAD_DIP_PM=0 FOLD_DIP_PM=0 (shipping bow, no beat, no reaction)
  dipoff    -- KNEAD_DIP_PM=0 only (shipping bow, no beat, reaction LIVE:
               isolates the reaction firing on a clip's own ambient sag)

The comparison is on the per-subject ORDERED FRAME BYTES SHA-256, which is the
same quantity pass 19 recorded, so `pass19` is checked against pass 19's own
committed table rather than against a re-render of it -- the pass-19 raw root
was deleted after its production verification, exactly as the process says.

  pass19 == the pass-19 table  ->  every byte that differs from pass 19 is
                                   produced by the three pass-20 mechanisms.
  rearonly vs pass19           ->  isolates the REAR BOW repair.
  shipping vs rearonly         ->  isolates the KNEAD BEAT and its reaction.
  dipoff vs rearonly           ->  the reaction firing on a clip's OWN ambient
                                   sag, with no knead beat present at all.

⚠ The two-switch leg (`dipoff` without FOLD_DIP_PM=0) is what the pass used as
its "pass-19 identity" all the way through, sampled on three clips. It is NOT
pass 19 on Blown, Fall and Trick. See scope-finding.md.
"""
import hashlib
import json
import sys
from pathlib import Path

BASE = Path(r"C:\programmieren\zencrifice\manafold-p16")
HERE = Path(__file__).resolve().parent
ROOTS = {
    "shipping": BASE / "p20-final-reel-22",
    "pass19": BASE / "p20-scope-pass19",
    "rearonly": BASE / "p20-scope-rearonly",
    "dipoff": BASE / "p20-scope-dipoff",
}


def subject_digest(d: Path):
    h = hashlib.sha256()
    n = 0
    for f in sorted(d.glob("*.rgb")):
        h.update(f.read_bytes())
        n += 1
    return h.hexdigest(), n


def crc_of(d: Path):
    for line in (d / "meta.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("sequence_crc32c="):
            return line.split("=", 1)[1].strip().upper().replace("0X", "0x")
    raise ValueError(d)


def main():
    recorded = {}
    for raw in (HERE / "pass19-recorded-bank.tsv").read_text().splitlines():
        s, n, c, h = raw.split("\t")
        recorded[s] = (int(n), c, h)

    banks = {}
    for name, root in ROOTS.items():
        if not root.is_dir():
            print(f"FAIL missing bank root {root}")
            return 1
        banks[name] = {p.name: (subject_digest(p), crc_of(p))
                       for p in sorted(root.iterdir()) if p.is_dir()}

    subjects = sorted(recorded)
    if any(sorted(b) != subjects for b in banks.values()):
        print("FAIL the three banks do not hold the same 22 subjects")
        return 1

    rows, ident, rear, knead, amb = [], 0, 0, 0, 0
    for s in subjects:
        p19n, p19c, p19h = recorded[s]
        (sh_h, sh_n), sh_c = banks["shipping"][s]
        (l_h, l_n), l_c = banks["pass19"][s]
        (d_h, d_n), d_c = banks["rearonly"][s]
        (a_h, _a_n), a_c = banks["dipoff"][s]
        same_as_p19 = (l_h == p19h and l_c == p19c and l_n == p19n)
        rear_changed = d_h != l_h
        knead_changed = sh_h != d_h
        ident += same_as_p19
        rear += rear_changed
        knead += knead_changed
        ambient = a_h != d_h
        amb += ambient
        rows.append(dict(subject=s, frames=sh_n, pass19_recorded_crc=p19c,
                         pass19_bank_crc=l_c, reproduces_pass19=same_as_p19,
                         rearonly_crc=d_c, twoswitch_crc=a_c, shipping_crc=sh_c,
                         rear_changed=rear_changed, knead_changed=knead_changed,
                         ambient_reaction=ambient))

    out = HERE / "scope-attribution.txt"
    with out.open("w", encoding="utf-8") as f:
        f.write("# Pass-20 scope attribution, all three banks from ONE binary "
                "(MD5 e95faca916627d1bddb02892c5eb67e1), production env\n")
        f.write("# pass19 = all THREE switches off; rearonly = shipping bow, no beat, "
                "no reaction; reaction-only = KNEAD_DIP_PM=0 only, i.e. shipping "
                "bow, no beat, reaction LIVE\n")
        f.write("# comparison is the per-subject ordered-frame-bytes SHA-256\n")
        f.write("subject\tframes\tp19-recorded\tp19-bank\treproduces-p19\trearonly\tshipping"
                "\trear-changed(rearonly!=p19)\tknead-changed(shipping!=rearonly)"
                "\treaction-only\tambient-reaction(reaction-only!=rearonly)\n")
        for r in rows:
            f.write(f"{r['subject']}\t{r['frames']}\t{r['pass19_recorded_crc']}\t"
                    f"{r['pass19_bank_crc']}\t{'SAME' if r['reproduces_pass19'] else 'DIFFERS'}\t"
                    f"{r['rearonly_crc']}\t{r['shipping_crc']}\t"
                    f"{'yes' if r['rear_changed'] else 'no'}\t"
                    f"{'yes' if r['knead_changed'] else 'no'}\t"
                    f"{r['twoswitch_crc']}\t"
                    f"{'yes' if r['ambient_reaction'] else 'no'}\n")
        f.write(f"# reproduces pass 19: {ident}/22; rear changes bytes: {rear}/22; "
                f"knead changes bytes: {knead}/22; reaction fires on ambient sag "
                f"with NO beat: {amb}/22\n")
    (HERE / "scope-attribution.json").write_text(json.dumps(rows, indent=2) + "\n")
    print(f"reproduces pass 19 {ident}/22; rear changes {rear}/22; "
          f"knead changes {knead}/22; ambient reaction {amb}/22")
    print(f"wrote {out}")
    return 0 if ident == 22 else 1


if __name__ == "__main__":
    raise SystemExit(main())
