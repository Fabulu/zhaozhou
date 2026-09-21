#!/usr/bin/env python3
"""Add the "Pass 20 - 2026-09-20" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 19" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p20 clips exactly once
each, with the item labels copied from the pass-19 collection so the same clip
carries the same name in every generation. The archive note goes FIFTEEN ->
SIXTEEN generations and gains a leading sentence for pass 20.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 20 \u2014 2026-09-20"
LABEL = "Pass 20"

CAPTION = ("The complete Manafold pass-20 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-21 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou e0447b1f, renderer MD5 e95faca916627d1bddb02892c5eb67e1, "
    "exact bank manifest "
    "a40b41549383246d7c9580c768c936f8919eb810dce7c0ecae24e3cdb1313b15, "
    "production-verified deploy 0cb48546. This is the generation whose antenna "
    "still bent in the middle of every run \u2014 the extra hinges the owner "
    "asked about \u2014 and whose rear end flicked about because the band into "
    "the body was bowed. Open it beside the live clips to see the difference: "
    "pass 20 has no straight runs and no visible joints, while pass 21 is four "
    "straight rods meeting at four round knuckles. Its kneading press is the "
    "same gesture the live pass keeps. It is provenance, not an alternate live "
    "presentation."
)

NEW_SENTENCE = (
    "Pass 20 is the complete 22-clip generation pass 21 replaced, preserved "
    "byte-for-byte before the pass-21 encode; each clip appears exactly once, "
    "and it is the one to open to see the antenna bending in the middle of its "
    "runs instead of at the balls, and the rear end flicking. "
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p19 = next(r for r in renders if r.get("label") == "Pass 19")
    labels = [it["label"] for it in p19["items"]]
    subjects = [it["src"].rsplit("archive-p19-manafold-", 1)[1][: -len(".webm")]
                for it in p19["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-19 collection has {len(labels)} items, expected 22")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p20-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p19)
    renders.insert(at, collection)

    note = manafold["archive_note"]
    if not note.startswith("FIFTEEN generations, newest first. "):
        print("FAIL the archive note does not start as expected; not edited")
        return 1
    manafold["archive_note"] = (
        "SIXTEEN generations, newest first. " + NEW_SENTENCE
        + note[len("FIFTEEN generations, newest first. "):])

    # ⚠ CRLF AND indent=2, because that is what the file already is. Writing LF
    # would make every line of a 291 KB manifest a diff and bury the one change
    # this script actually makes -- the same "the real edit is unreviewable"
    # failure the NUL byte caused in manafold_clips.h this pass.
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK inserted '{LABEL}' at index {at} with {len(collection['items'])} clips")
    print(f"OK archive note -> SIXTEEN generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
