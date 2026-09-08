# Task Log: RUN-20260908-1943 - [Describe objective here]

**Created:** 2026-09-08 19:43 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-1943-manafold-p13-implb-performance/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 19:43 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-1943
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## IMPL-B, Manafold pass 13 -- performance lane (R3, R4, R5, R2 opt-ins)

Lane: `C:\programmieren\zencrifice\manafold-p13-b\{zhaozhou,Upheaval}`.
Items: R3 (taunt3), R4 (blown), R5 (death-gutter root), the four
`wrap_root_delta` opt-ins, and the owner's 39-degree eye ruling relayed
mid-run.

### Where I was, written down before reading anything that would redirect me
* Baseline built at `bb-base`, gates at `bb-gates-base`, before-bank in
  `out-before/` (8 subjects). Verified the before-bank still reproduces
  byte-identically after rebasing onto R6-bis, so the pair is honest.
* Edits landed in `1e9aa9a3`. Gates all re-run against my own build.
* Outstanding at that point: iteration-2 render (`out-a2`), the wrapseam
  numbers for the four opt-in clips, lasso's tail, and the findings doc.

### Build/render discipline
* Every build `--clean` (gotcha 19: build-direct.sh has no header dependency
  tracking and all of this is header work). Exit code read from the build,
  never from a pipeline.
* A foreground render was killed by a tool timeout at 10 minutes and **the
  render process survived the shell** (CLAUDE.md: stopping an agent does not
  stop its background work). Found it alive in `tasklist`, killed it, and
  confirmed no `zhao-reel-cel` was running before the next link -- gotcha 13
  is a link failure that reports success. Every render after that ran through
  the harness's own background mechanism.
* Frames read only through `tools/reel/rgbframe.py` / `plates.py`.

### An instrument that lied to me, recorded because it will lie again
The Bash tool's quoted heredoc **strips one level of backslash** before Python
sees it, so a patch script containing `\n` writes a REAL NEWLINE into C++
source instead of the two characters `\n`. It presented as a string-match
assertion failing on text that was visibly identical in the file. Use `\\n`
in a heredoc to land a literal `\n`, or write the script to a file first.
