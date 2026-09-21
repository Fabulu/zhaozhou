#!/usr/bin/env python3
"""Add the "Pass 22 - 2026-09-21" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 21" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p22 clips exactly once
each, with the item labels copied from the pass-21 collection so the same clip
carries the same name in every generation. The archive note goes SEVENTEEN ->
EIGHTEEN generations and gains a leading sentence for pass 22.

This is the pass-21 tool (via V18 and passes 19, 20, 21) repointed one
generation later, unchanged in shape on purpose.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 22 \u2014 2026-09-21"
LABEL = "Pass 22"

CAPTION = ("The complete Manafold pass-22 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-23 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou 0a743562, renderer MD5 8a0aa4da7f35a24c8415848268a5cf98, "
    "exact bank manifest "
    "b188f2efa85649821bf5578bbfb1cea25511c214ea48040195d5a579b1777d6e, "
    "production-verified Upheaval main ca51e26d / Zhaozhou 0d796c4a. This is "
    "the generation where the kneading beat pressed too lightly on five clips "
    "to see the lightning answer it \u2014 open Drift, Damage, Death-drop or "
    "Death-gutter beside the live clips and the strand here barely moves as the "
    "top nodule comes down, while the live pass presses deeper and the "
    "lightning plainly turns and re-forms. It is the generation that first "
    "shrank the green and blue mana dots with distance, and that keeps the "
    "pass-21 ball-and-stick antenna rig. It is provenance, not an alternate "
    "live presentation."
)

NEW_SENTENCE = (
    "Pass 22 is the complete 22-clip generation pass 23 replaced, preserved "
    "byte-for-byte before the pass-23 encode; each clip appears exactly once, "
    "and it is the one to open to see the kneading press at its shallow "
    "setting on Drift, Damage, Death-drop and Death-gutter, where the "
    "lightning's answer is too faint to read. "
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p21 = next(r for r in renders if r.get("label") == "Pass 21")
    labels = [it["label"] for it in p21["items"]]
    subjects = [it["src"].rsplit("archive-p21-manafold-", 1)[1][: -len(".webm")]
                for it in p21["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-21 collection has {len(labels)} items, expected 22")
        return 1
    if len(set(subjects)) != 22:
        print("FAIL the pass-21 collection does not declare 22 distinct clips")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p22-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p21)
    renders.insert(at, collection)

    note = manafold["archive_note"]
    if not note.startswith("SEVENTEEN generations, newest first. "):
        print("FAIL the archive note does not start as expected; not edited")
        return 1
    manafold["archive_note"] = (
        "EIGHTEEN generations, newest first. " + NEW_SENTENCE
        + note[len("SEVENTEEN generations, newest first. "):])

    # CRLF AND indent=2, because that is what the file already is. Writing LF
    # would make every line of a ~300 KB manifest a diff and bury the one change
    # this script actually makes.
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK inserted '{LABEL}' at index {at} with {len(collection['items'])} clips")
    print("OK archive note -> EIGHTEEN generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
