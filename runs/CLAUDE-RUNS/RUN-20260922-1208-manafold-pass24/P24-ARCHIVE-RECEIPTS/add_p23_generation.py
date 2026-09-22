#!/usr/bin/env python3
"""Add the "Pass 23 - 2026-09-21" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 22" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p23 clips exactly once
each, with the item labels copied from the pass-22 collection so the same clip
carries the same name in every generation. The archive note goes EIGHTEEN ->
NINETEEN generations and gains a leading sentence for pass 23.

This is the pass-22 tool (via V18 and passes 19-23) repointed one generation
later, unchanged in shape on purpose.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 23 \u2014 2026-09-21"
LABEL = "Pass 23"

CAPTION = ("The complete Manafold pass-23 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-24 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou d6532cc1, renderer MD5 3e9104bae2777a3160da9bbe37bd0b9c, "
    "exact bank manifest "
    "2f3cfb8175f4e4cad80ced5351591ca1fdaa6fc5ee3fa768f4449059ecdffb18, "
    "production-verified Upheaval main e965dc5c / Zhaozhou 1d449717. This is "
    "the last generation where the lightning still passed straight through the "
    "antenna rods \u2014 open Crackle or Hover beside the live clips and the "
    "blue figure here lies across the pink bands, where the live pass holds it "
    "clear of them. It is also the generation before the eyes picked up their "
    "small ambient drift on the ordinary clips, and the one that deepened the "
    "kneading press on Drift, Damage, Death-drop and Death-gutter. It is "
    "provenance, not an alternate live presentation."
)

NEW_SENTENCE = (
    "Pass 23 is the complete 22-clip generation pass 24 replaced, preserved "
    "byte-for-byte before the pass-24 encode; each clip appears exactly once, "
    "and it is the one to open to see the lightning still crossing the antenna "
    "rods on Crackle and Hover, and the eyes still holding one fixed attitude "
    "on the ordinary clips. "
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p22 = next(r for r in renders if r.get("label") == "Pass 22")
    labels = [it["label"] for it in p22["items"]]
    subjects = [it["src"].rsplit("archive-p22-manafold-", 1)[1][: -len(".webm")]
                for it in p22["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-22 collection has {len(labels)} items, expected 22")
        return 1
    if len(set(subjects)) != 22:
        print("FAIL the pass-22 collection does not declare 22 distinct clips")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p23-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p22)
    renders.insert(at, collection)

    note = manafold["archive_note"]
    if not note.startswith("EIGHTEEN generations, newest first. "):
        print("FAIL the archive note does not start as expected; not edited")
        return 1
    manafold["archive_note"] = (
        "NINETEEN generations, newest first. " + NEW_SENTENCE
        + note[len("EIGHTEEN generations, newest first. "):])

    # CRLF AND indent=2, because that is what the file already is. Writing LF
    # would make every line of a ~300 KB manifest a diff and bury the one change
    # this script actually makes.
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK inserted '{LABEL}' at index {at} with {len(collection['items'])} clips")
    print("OK archive note -> NINETEEN generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
