#!/usr/bin/env python3
"""Add the "Pass 24 - 2026-09-22" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 23" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p24 clips exactly once
each, with the item labels copied from the pass-23 collection so the same clip
carries the same name in every generation. The archive note goes NINETEEN ->
TWENTY generations and gains a leading sentence for pass 24.

This is the pass-24 tool (via V18 and passes 19-24) repointed one generation
later, unchanged in shape on purpose -- the archive step is the one place in the
pass where "the same thing as last time" is the requirement rather than a
compromise.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 24 \u2014 2026-09-22"
LABEL = "Pass 24"

CAPTION = ("The complete Manafold pass-24 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-25 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou 8e5a3ee3, renderer MD5 e90af7c043cd9670054eeb256abd5e95, "
    "exact bank manifest "
    "f9128e0821004dc49b19a280651db864e4556947b20d9267fbb84d5b9e58703b, "
    "production-verified Upheaval main 8e45d38b / Zhaozhou 09108284. This is "
    "the generation where bolt avoidance was an EXPERIMENT on three clips "
    "only \u2014 Crackle and Hover carry it, Inspect carries the rival "
    "depth-splitting approach, and the other nineteen clips still draw the "
    "lightning straight through the antenna rods. Open Drift or Death-drop "
    "here beside the live pass to see the difference the rollout made. It is "
    "also the generation before the eyes' ambient acting was strengthened, and "
    "the last one where Hover's rear jitters from frame to frame. It is "
    "provenance, not an alternate live presentation."
)

NEW_SENTENCE = (
    "Pass 24 is the complete 22-clip generation pass 25 replaced, preserved "
    "byte-for-byte before the pass-25 encode; each clip appears exactly once, "
    "and it is the one to open to see bolt avoidance still confined to three "
    "clips, the lightning still crossing the rods on the other nineteen, and "
    "Hover's back ball still fidgeting. "
)

PREV_PREFIX = "NINETEEN generations, newest first. "
NEW_PREFIX = "TWENTY generations, newest first. "


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p23 = next(r for r in renders if r.get("label") == "Pass 23")
    labels = [it["label"] for it in p23["items"]]
    subjects = [it["src"].rsplit("archive-p23-manafold-", 1)[1][: -len(".webm")]
                for it in p23["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-23 collection has {len(labels)} items, expected 22")
        return 1
    if len(set(subjects)) != 22:
        print("FAIL the pass-23 collection does not declare 22 distinct clips")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p24-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p23)
    renders.insert(at, collection)

    note = manafold["archive_note"]
    if not note.startswith(PREV_PREFIX):
        print("FAIL the archive note does not start as expected; not edited")
        return 1
    manafold["archive_note"] = (
        NEW_PREFIX + NEW_SENTENCE + note[len(PREV_PREFIX):])

    # CRLF AND indent=2, because that is what the file already is. Writing LF
    # would make every line of a ~300 KB manifest a diff and bury the one change
    # this script actually makes.
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK inserted '{LABEL}' at index {at} with {len(collection['items'])} clips")
    print("OK archive note -> TWENTY generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
