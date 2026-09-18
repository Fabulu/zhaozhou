"""capability_gap -- reconcile the 38-capability completion seed against the tree.

The owner's completion plan says a capability row *"cannot be declared absent
only by a filename heuristic"* and *"cannot be called implemented only because a
similarly named historical block exists"*. This does the one mechanical half of
that honestly and refuses to do the other half at all:

  * it checks whether every file a capability CITES actually exists;
  * it reports the citation, so a human can judge whether that file is really
    the capability.

It does NOT decide that a capability is complete. A path existing is not a
producer, a consumer and evidence, which is what the plan requires.

Reads LOW when broken -- an empty seed or a path-normalisation slip would report
"nothing missing" -- so it refuses to run unless a known-present file resolves
and a known-absent one does not.
"""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SEED = (ROOT / "reports" / "true-console-completion-package" / "zhaozhou_true_console"
        / "examples" / "completion_seed.json")

CANARY_PRESENT = "design/blocks.yml"
CANARY_ABSENT = "fpga/rtl/does_not_exist_zzz.sv"


def load():
    return json.loads(SEED.read_text(encoding="utf-8", errors="replace"))


def resolves(rel: str) -> bool:
    return (ROOT / rel).exists()


def main(argv: list[str]) -> int:
    if not resolves(CANARY_PRESENT) or resolves(CANARY_ABSENT):
        raise SystemExit("SELF-TEST FAILED: path resolution is broken; every "
                         "'present' below would be meaningless")

    seed = load()
    caps = seed["capabilities"]
    print("seed pinned at %s" % seed.get("inspected_head", "?"))
    print("%d capabilities\n" % len(caps))

    by_state: dict[str, int] = {}
    no_impl, dangling, ok = [], [], []

    for c in caps:
        by_state[c.get("state", "?")] = by_state.get(c.get("state", "?"), 0) + 1
        impls = c.get("implementation") or []
        if not impls:
            no_impl.append(c)
            continue
        missing = [p for p in impls if not resolves(p)]
        (dangling if missing else ok).append((c, missing))

    print("states:", ", ".join("%s=%d" % kv for kv in sorted(by_state.items())))
    print("\ncites nothing at all            : %d" % len(no_impl))
    print("cites a path that does NOT exist: %d" % len(dangling))
    print("every cited path exists         : %d" % len(ok))

    if no_impl:
        print("\n=== NO IMPLEMENTATION CITED ===")
        for c in no_impl:
            print("  %-5s %-8s %s" % (c["id"], c.get("state", ""), c["title"]))

    if dangling:
        print("\n=== CITES A MISSING FILE ===")
        for c, missing in dangling:
            print("  %-5s %-8s %s" % (c["id"], c.get("state", ""), c["title"]))
            for m in missing:
                print("          MISSING: %s" % m)

    print("\nA path existing is NOT completion. The plan requires a real producer,")
    print("a real consumer and evidence for every row; this tool checks none of")
    print("those three. It only removes the rows that cannot possibly be done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
