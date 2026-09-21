"""Transcribe lane records that live only in commit messages into FINDINGS files.

The harness refuses a lane-written `.md`, so several packets landed their record
as a commit whose MESSAGE is the document. That is a real record and a bad home:
`git log --grep` finds it only if you already know it exists, and this campaign's
own law is that a thing nobody reads back was never delivered.

The coordinator CAN write the file, so it does. The commit message is reproduced
verbatim -- this is a transcription, not a summary, and rewriting a lane's words
into my own is how a finding becomes my recollection of a finding.
"""
import io
import os
import subprocess
import sys

REPO = r"C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912"
RUN = os.path.join(REPO, "runs", "CLAUDE-RUNS",
                   "RUN-20260919-1656-gaps-to-zero")

# lane -> the commits whose messages ARE its record, oldest first.
LANES = {
    "terracomp":  ["89a9ce32"],
    "terrassem":  ["c60f454d"],
    "forgecomp":  ["6072d3bf"],
    "warpbuild":  ["20261b1b"],
    "cfgarm":     ["f056e586", "5db18f12", "926df70f", "e1b4cbd7", "e0ab6551"],
    "sheetseam":  ["778e4846"],
    "projout":    ["935245ab", "890449d2"],
    "fieldlane":  ["48c46633", "07055e85", "1b4280e6"],
    "posepage":   ["3086be18"],
    "gouraudlook": ["6ff6d2d7"],
    "posecmd":    ["db2bd937"],
    "forgeprim":  ["0d33c434"],
    "shadowsub":  ["89cccdf7", "9555a86d"],
    "deltalaw":   ["7b7e3e48"],
}

HEADER = """<!-- TRANSCRIBED FROM COMMIT MESSAGES, %s.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
%s -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- %s

"""


def body(sha):
    out = subprocess.run(["git", "-C", REPO, "log", "-1", "--format=%B", sha],
                         capture_output=True, text=True, encoding="utf-8",
                         errors="replace")
    if out.returncode != 0:
        raise SystemExit("cannot read %s: %s" % (sha, out.stderr))
    return out.stdout.rstrip("\n")


def subject(sha):
    out = subprocess.run(["git", "-C", REPO, "log", "-1", "--format=%s", sha],
                         capture_output=True, text=True, encoding="utf-8",
                         errors="replace")
    return out.stdout.strip()


def main():
    wrote = 0
    for lane, shas in sorted(LANES.items()):
        path = os.path.join(RUN, "FINDINGS-%s.md" % lane)
        if os.path.exists(path):
            print("SKIP  %-12s already present" % lane)
            continue
        parts = [HEADER % ("2026-09-21", ", ".join(shas), lane.upper())]
        for sha in shas:
            parts.append("## `%s` -- %s\n" % (sha, subject(sha)))
            parts.append("```\n%s\n```\n" % body(sha))
        text = "\n".join(parts)
        with io.open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        print("WROTE %-12s %d commit(s), %d bytes" % (lane, len(shas),
                                                      len(text)))
        wrote += 1
    print("\n%d file(s) written." % wrote)
    return 0


if __name__ == "__main__":
    sys.exit(main())
