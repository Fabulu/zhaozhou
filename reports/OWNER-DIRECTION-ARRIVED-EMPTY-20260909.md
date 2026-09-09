# Two owner commits announced rearchitecture direction and carried no content

2026-09-09. Recorded here rather than only in a run log, because a run folder is
orphaned by the next pass and this needs to survive until the direction actually
lands.

| commit | time | subject | files | tree vs parent |
|---|---|---|---|---|
| `fa87d888` | 12:33:27 | *Agent please read - new rearchitecture goals* | none | **identical** |
| `6cfb9704` | 12:39:22 | *Agent please read - New rearchitecture help* | none | **identical** |

Both are single-parent commits on `zixxtrixx-v8-closeout`, each sitting directly
on top of one of this session's pushes (`6e189134` and `3b2615b5`). Both have a
subject line and **no message body**. Both have a tree byte-identical to their
parent, so they are genuinely empty commits rather than something my reading
missed.

## What was ruled out before concluding that

* **Not a merge commit.** A merge shows no files under `--name-status` by
  default, which would have produced exactly this appearance. Checked: parent
  count is 1 for both.
* **Not the message body.** `git log -1 --format=%B` returns the subject alone.
* **Not another branch.** `fa87d888` is the newest commit anywhere in the
  repository; nothing else has landed on any remote ref since 10:30, and
  `origin/main`'s tip is `51d30033` from 10:20.
* **Not `.gitignore`.** No pattern in it would swallow a normally-named
  document — the closest are `*.config` and `*.exe`. A `.md` or `.txt` under
  `reports/` or the repo root is not ignored.

## Why this matters more than a missing file

`CLAUDE.md` records that owner direction *"was posted four times because it kept
not reaching the working agent... and five passes solved the wrong problem in the
meantime."* This is that failure one step earlier: the instruction was not
mis-delivered, it was announced and never sent.

**Nothing was guessed at.** "New rearchitecture goals" could redirect the whole
programme, and acting on an assumption about unknown goals is the one case where
proceeding would make the work useless if the assumption were wrong. The session
continued on already-authorised work instead.

## What is needed

The goals as a file (or in the commit body). A likely cause worth checking on the
sending side: `git commit -a` stages modifications to **tracked** files only and
silently skips a brand-new untracked file, so a freshly written document plus
`commit -am` stages nothing — and an empty commit then requires `--allow-empty`
or a wrapper that passes it.

## What was NOT blocked by this

Everything already authorised continued and is pushed: both map-lane syntax
blockers repaired, the whole-tree map lane verified restored (213 files, 0
errors), and the RAM-inference probe advanced to v3. See
`RAM-INFERENCE-PROBE-AND-A-DEAD-MAP-LANE-20260909.md`.
