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


def _load_reference_text() -> list[tuple[pathlib.Path, str]]:
    out = []
    for p in REFDIR.rglob("*"):
        if p.suffix in (".h", ".hpp", ".cpp", ".cc") and "build" not in p.parts:
            try:
                out.append((p, p.read_text(encoding="utf-8", errors="replace")))
            except OSError:
                pass
    return out


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