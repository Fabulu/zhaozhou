# Task Log: RUN-20260908-0957 - [Describe objective here]

**Created:** 2026-09-08 09:57 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0957-manafold-p12-final-publish/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 09:57 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0957
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

---

## 09:40 — the fix pass landed; this run RENDERS AND PUBLISHES it

Lane: `manafold-p12-fix/` (reused as the publish lane rather than cloning a
sixth — it was already at the tip and clean, and the disk has been at zero once
this week).

Starting SHAs, both at `origin/main` with nothing unpushed:

    zhaozhou   0ecf69b6
    Upheaval   e3bae84

**What this bank contains that the live page does not.** The fix pass closed
five of the reviewer's items, and four of them were the same fault: the knob
every previous pass had turned was **not connected to the thing being judged**
(gotcha §18, four times in one pass).

* the corpse stops standing up — `hold_last`, never set on any Manafold clip,
  while Zixxtrixx has had it since run 0326
* one mana light RATIO for the bank — each lamp had been rounding independently
  and `hit` collapsed all four into one light
* the free-floating orbs — `kWanderCount` authored "of kMoteCount", subtracted
  as an absolute, so the garnish cut **promoted** the drifters 15% → 54%
* `inspect` no longer duplicates `hover` — 0 of 600 frames identical
* **the eye travel driver, recovered.** It was stranded on the branch of an
  agent a rate limit killed; only the channel itself had reached main, so five
  owner paragraphs (D9 §6, §6.1, §6.2, §12.2, §12.3) were undelivered.

### 09:41 — build: `--clean`, and the reason is now gotcha 19

    tools/reel/build-direct.sh --output out/p12final --clean cel
    md5 5ed169464ea58ff67e64ec6814c7ece7   (recorded BEFORE the render)

`--clean` is not caution. `build-direct.sh` compares timestamps on the `.cpp`
only and **has no header dependency tracking**, and every constant in this pass
lives in a header. Written up as `09-ENGINE-GOTCHAS.md` §19 before the render
started, because it is the stale-binary trap reached by the road CLAUDE.md
currently recommends as the *safe* one.

### 09:43 — render: 28 subjects, ONE binary invocation

22 last time, 28 now: `taunt3`, `lasso`, `blown`, `flight` and the two deaths
have all been added since. The list is taken from `creatures.json`'s live
declarations, not from memory — twice this week finished work was one file away
from being invisible, and both times the file was that one.

### NEXT STEPS — written down BEFORE the render lands
1. encode, **with no `-SkipMediaCheck`**
2. LOOK at the eye travel and the two deaths specifically
3. assemble, deploy `-Branch main`, verify from production
4. then items 5, 6, 7, 9, 10 and C3, which the fix pass named rather than omitted

⚠ **`-SkipMediaCheck` is not available to this run.** Using it twice last night
was judged UNSOUND by QA and it was: the flag is overloaded and also disables
`checkfresh.py`, so **six live mana clips shipped from the previous generation**
— the exact fault `checkfresh.py` was written to catch, defeated by the flag
that was supposed to only skip decodability.

### 09:50 — lane sweep, while the render runs
Audit clean: **no unpushed commits anywhere**, 16 lanes scanned. The p12-publish
run log was committed and rebased onto main first (45 lines that existed in one
place). Deleted `manafold-p11-L`, `-p12-pub`, `-p12-qa`, `-p12-review`, `-p12-w3`.
The review lane's 8 orphaned commits were preserved to
`origin/archive/p12-review-runlog` before it went.
