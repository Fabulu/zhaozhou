#!/usr/bin/env python3
"""Add the "Pass 21 - 2026-09-21" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 20" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p21 clips exactly once
each, with the item labels copied from the pass-20 collection so the same clip
carries the same name in every generation. The archive note goes SIXTEEN ->
SEVENTEEN generations and gains a leading sentence for pass 21.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 21 \u2014 2026-09-21"
LABEL = "Pass 21"

CAPTION = ("The complete Manafold pass-21 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-22 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou bd29d4f9, renderer MD5 fe1bab84..., exact bank manifest "
    "639c370cf32cb777e63896af064fc56a0d48f01edf354f2ee15d68e1ea395633, "
    "production-verified Upheaval main 76c6a3fb / Zhaozhou 806aecf7. This is "
    "the generation where the green and blue mana dots stayed the same size on "
    "screen however far away the creature was \u2014 open Drift or Hasty beside "
    "the live clips and the dots here are the fat pale balls that crowd the "
    "antenna and hide the green pocket, while the live pass shrinks them with "
    "distance. It is also the generation whose lightning held its shape through "
    "the kneading press instead of turning and re-forming. Its ball-and-stick "
    "antenna rig is the same one the live pass keeps. It is provenance, not an "
    "alternate live presentation."
)

NEW_SENTENCE = (
    "Pass 21 is the complete 22-clip generation pass 22 replaced, preserved "
    "byte-for-byte before the pass-22 encode; each clip appears exactly once, "
    "and it is the one to open to see the mana dots at their close-up size on "
    "a distant creature, and the lightning not reacting to the knead. "
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p20 = next(r for r in renders if r.get("label") == "Pass 20")
    labels = [it["label"] for it in p20["items"]]
    subjects = [it["src"].rsplit("archive-p20-manafold-", 1)[1][: -len(".webm")]
                for it in p20["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-20 collection has {len(labels)} items, expected 22")
        return 1
    if len(set(subjects)) != 22:
        print("FAIL the pass-20 collection does not declare 22 distinct clips")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p21-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p20)
    renders.insert(at, collection)

    note = manafold["archive_note"]
    if not note.startswith("SIXTEEN generations, newest first. "):
        print("FAIL the archive note does not start as expected; not edited")
        return 1
    manafold["archive_note"] = (
        "SEVENTEEN generations, newest first. " + NEW_SENTENCE
        + note[len("SIXTEEN generations, newest first. "):])

    # CRLF AND indent=2, because that is what the file already is. Writing LF
    # would make every line of a ~300 KB manifest a diff and bury the one change
    # this script actually makes.
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK inserted '{LABEL}' at index {at} with {len(collection['items'])} clips")
    print(f"OK archive note -> SEVENTEEN generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
