"""refmodel_liveness -- which reference_model symbols does the ledger PROMISE
that the reference oracle does not contain?

`design/blocks.yml` names a `reference_model:` for 95 blocks. That is a claim.
Eleven of them name a symbol that
exists nowhere in `reference/`, which means those blocks have **no differential
test available** -- their directed tests can only check self-consistency against
the contract's prose, which is a weaker instrument than every finished block in
this repo enjoys, and it reads in the flattering direction.

Two refinements that the raw idea does not have and needs:

* an `rtl::`-prefixed model names a MODULE, not an oracle symbol. Those resolve
  against `fpga/rtl/`, and counting them as missing produces two false alarms.
* this tool reads LOW when broken -- a regex that matches nothing reports "all
  present" -- so it REFUSES TO RUN unless it can first resolve a symbol known to
  exist. See CLAUDE.md, "A broken instrument lies in ONE direction".

Companion to `uncashed_cheques.py` (built, installed nowhere) and
`closure_liveness.py` (declared, never elaborated). Same genre: a written-down
intention that nothing reads back.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BLOCKS = ROOT / "design" / "blocks.yml"
REFDIR = ROOT / "reference"
RTLDIR = ROOT / "fpga" / "rtl"

# A symbol that MUST resolve. If it does not, the search is broken and every
# "present" below is worthless.
CANARY = "zref::render::project_vertex"

# A symbol that must NOT resolve from prose alone. The CANARY above proves the
# search is not wholly broken; it CANNOT prove the search distinguishes code
# from a comment, because it resolves either way. See `_prose_self_test`.
PROSE_CANARY = "zref::ThisSymbolExistsOnlyInThisComment"


def strip_cxx_comments(text: str) -> str:
    """Remove `//` and `/* */` comments. CODE ONLY IS EVIDENCE OF A DEFINITION.

    ADDED 2026-09-20, owner ruling R129, after this exact defect was found for
    the THIRD time in a third unrelated tool. The first was
    `uncashed_cheques.py` CHECK 5; the second was
    `tests/render/test_render_texture_packet_e.py:730`, which asserted a formal
    assertion R32 had DELETED and passed because the token survived in a comment
    in the file it grades -- it was grading prose.

    This tool had it too, and it is the worst place for it: `resolve()` searched
    `p.read_text()` raw, so **a comment saying "zref::X was removed, do not
    resurrect it" made zref::X resolve as PRESENT.** The obituary resolves the
    corpse, and the better the comment explaining the removal, the more reliably
    this auditor reported the thing alive.

    Demonstrated before repairing, per CLAUDE.md's broken-instrument law: a blob
    containing the name only inside a `//` comment returned a hit, while the
    same name absent entirely returned none. The CANARY self-test above passed
    throughout -- it proves the search finds what exists, never that it rejects
    prose, which is precisely the case that matters here.

    Not a parser and not meant to be: a name in a string literal or in dead
    `#if 0` code still resolves. It removes the one failure mode that THE ACT OF
    DOCUMENTING A REMOVAL creates.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j            # keep the newline
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            out.append(" ")                  # separator, never a join
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _load_reference_text() -> list[tuple[pathlib.Path, str]]:
    out = []
    for p in REFDIR.rglob("*"):
        if p.suffix in (".h", ".hpp", ".cpp", ".cc") and "build" not in p.parts:
            try:
                out.append((p, strip_cxx_comments(
                    p.read_text(encoding="utf-8", errors="replace"))))
            except OSError:
                pass
    return out


def _prose_self_test() -> None:
    """The half the CANARY cannot cover: prose must NOT resolve.

    Four cases, and the two middle ones were wrong before the strip.
    """
    import pathlib as _pl
    def hits(body):
        return resolve(PROSE_CANARY, [(_pl.Path("t.hpp"), strip_cxx_comments(body))])
    leaf = PROSE_CANARY.split("::")[-1]
    # 1. Absent entirely -> no hit. (Always worked.)
    assert not hits("int unrelated;\n"), "resolves a symbol that is not there"
    # 2. Line comment only -> no hit. (This was the blindness.)
    assert not hits("// %s was removed in R32; do not resurrect it\n" % PROSE_CANARY), \
        "resolves a symbol that exists only in a // comment"
    # 3. Block comment only -> no hit.
    assert not hits("/* replaces the %s phantom */\n" % PROSE_CANARY), \
        "resolves a symbol that exists only in a /* */ comment"
    # 4. Actually declared -> MUST hit. Without this a checker that rejected
    #    everything would pass the three above.
    assert hits("struct %s { int x; };\n" % leaf), \
        "fails to resolve a symbol that is genuinely declared"
    # 5. The block strip must not weld two identifiers into one.
    assert "AB" not in strip_cxx_comments("A/* c */B"), \
        "strip_cxx_comments joined tokens across a comment"


def resolve(sym: str, blob: list[tuple[pathlib.Path, str]]) -> list[pathlib.Path]:
    """Where is this reference_model defined? [] means nowhere."""
    sym = sym.strip().strip("\"'")
    if sym.startswith("rtl::"):
        name = sym[len("rtl::"):]
        return list(RTLDIR.rglob(name + ".sv"))
    leaf = sym.split("::")[-1]
    if not leaf:
        return []
    pat = re.compile(r"\b" + re.escape(leaf) + r"\b")
    return [p for p, t in blob if pat.search(t)]


def declared() -> dict[str, list[str]]:
    """block id -> the reference_model symbols it declares."""
    text = BLOCKS.read_text(encoding="utf-8", errors="replace")
    out: dict[str, list[str]] = {}
    cur = None
    for line in text.splitlines():
        m = re.match(r"\s*-\s*id:\s*(\S+)", line)
        if m:
            cur = m.group(1)
        m = re.match(r"\s*reference_model:\s*(.+)", line)
        if m and cur:
            out.setdefault(cur, []).append(m.group(1).strip().strip("\"'"))
    return out


def audit() -> tuple[list[tuple[str, str]], list[tuple[str, str]]]:
    blob = _load_reference_text()
    _prose_self_test()
    if not resolve(CANARY, blob):
        raise SystemExit(
            f"SELF-TEST FAILED: cannot resolve {CANARY}, which exists. "
            f"The search is broken; scanned {len(blob)} files. "
            f"Every 'present' this tool could print would be a false negative."
        )
    present: list[tuple[str, str]] = []
    missing: list[tuple[str, str]] = []
    for bid, syms in sorted(declared().items()):
        for s in syms:
            (present if resolve(s, blob) else missing).append((bid, s))
    return present, missing


def main(argv: list[str]) -> int:
    gate = "--gate" in argv
    present, missing = audit()
    print(f"{len(present) + len(missing)} reference_model declarations; "
          f"self-test OK ({CANARY} resolves)")
    print(f"  resolve:     {len(present)}")
    print(f"  UNRESOLVED:  {len(missing)}")
    if missing:
        print("\nDeclared in design/blocks.yml, absent from the oracle:")
        for bid, s in missing:
            print(f"  {bid:<24} declares {s}")
        print("\nThese blocks have NO differential test available. A directed "
              "test for one of them checks self-consistency against contract "
              "prose, not agreement with a ratified model.")
    # Not a gate by default: an unbuilt block legitimately has no oracle yet.
    # --gate is for the day the last one is built.
    return 1 if (gate and missing) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))