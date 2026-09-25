#!/usr/bin/env python3
"""Add the "Pass 25 - 2026-09-22" archive generation to Manafold's manifest.

One new archive collection, inserted immediately BEFORE "Pass 24" so it is the
newest (Manafold's generation order is the array order -- it has no
archive_generation_order key), declaring the 22 archive-p25 clips exactly once
each, with the item labels copied from the pass-24 collection so the same clip
carries the same name in every generation. The archive note goes TWENTY ->
TWENTY-ONE generations and gains a leading sentence for pass 25.

This is the pass-25 tool (via V18 and passes 19-25) repointed one generation
later, unchanged in shape on purpose -- the archive step is the one place in the
pass where "the same thing as last time" is the requirement rather than a
compromise.

Idempotent: refuses to run twice.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

GENERATION = "Pass 25 \u2014 2026-09-22"
LABEL = "Pass 25"

CAPTION = ("The complete Manafold pass-25 presentation, all 22 clips and their "
           "posters, preserved byte-for-byte before pass-26 work overwrote the "
           "live files.")
NOTE = (
    "Source zhaozhou f66d107c, renderer MD5 a0c0c80a2e9b02dc852c25c1ff1aa530, "
    "exact bank manifest "
    "23cd615d25ceb27f29231c769f3e1a507e2d9cc2dcc74612da247b99fc3cef16, "
    "production-verified Upheaval main 7d2745a / Zhaozhou f66d107c. This is "
    "the last generation before Hasty was given its hurry \u2014 open Hasty "
    "here beside the live pass and the difference is the whole of pass 26: "
    "the creature and the ground slide across the frame together, because its "
    "traverse had been removed while the camera went on compensating for it, "
    "so nothing reads as speed. Its face is too small to act, too, which is "
    "why the expression could not be authored until the camera came in. "
    "Everything else on this page is byte-identical to the live pass. It is "
    "provenance, not an alternate live presentation."
)

NEW_SENTENCE = (
    "Pass 25 is the complete 22-clip generation pass 26 replaced, preserved "
    "byte-for-byte before the pass-26 encode; each clip appears exactly once, "
    "and it is the one to open to see Hasty before it was in a hurry \u2014 "
    "drifting across the frame with the ground, at a size that left its eyes "
    "unable to act. "
)

PREV_PREFIX = "TWENTY generations, newest first. "
NEW_PREFIX = "TWENTY-ONE generations, newest first. "


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    renders = manafold["renders"]

    if any(r.get("archive_generation") == GENERATION for r in renders):
        print(f"FAIL {GENERATION} is already in the manifest; nothing done")
        return 1

    p24 = next(r for r in renders if r.get("label") == "Pass 24")
    labels = [it["label"] for it in p24["items"]]
    subjects = [it["src"].rsplit("archive-p24-manafold-", 1)[1][: -len(".webm")]
                for it in p24["items"]]
    if len(labels) != 22:
        print(f"FAIL the pass-24 collection has {len(labels)} items, expected 22")
        return 1
    if len(set(subjects)) != 22:
        print("FAIL the pass-24 collection does not declare 22 distinct clips")
        return 1

    collection = {
        "label": LABEL,
        "group": "Archive",
        "archive": True,
        "archive_generation": GENERATION,
        "caption": CAPTION,
        "note": NOTE,
        "items": [{"src": f"renders/archive-p25-manafold-{s}.webm", "label": l}
                  for s, l in zip(subjects, labels)],
    }

    at = renders.index(p24)
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
    print("OK archive note -> TWENTY-ONE generations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
