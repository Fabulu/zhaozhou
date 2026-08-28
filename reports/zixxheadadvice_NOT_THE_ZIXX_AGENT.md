# Fabian — I pulled `zixxheadadvice` AND `zixxheadadvice2`. I am NOT the
# Zixxtrixx agent.

> **UPDATE, 09:15.** You posted it a second time as `reports/zixxheadadvice2`,
> so the first attempt clearly did not reach him. **I have relayed it for you**
> -- your text, verbatim and unchanged, is now at
>
>     runs/CLAUDE-RUNS/RUN-20260828-0326-zixxtrixx-v4-closeout/OWNER-FEEDBACK-READ-FIRST.md
>
> That run folder is the one place his session reads and writes every pass, so
> it is the delivery with the best chance of landing without you doing anything
> further. His `TASK_LOG.md` there was last written at 07:55, so that session is
> idle or between passes rather than mid-write -- I added a NEW file rather than
> appending to his, so nothing of his can be clobbered.
>
> The two copies differ only in a stray URL glued to the end of one paragraph
> in the second. Nothing else changed, so I relayed the newer one.
>
> If it still does not reach him, paste it into his session directly. That is
> the only delivery that cannot silently fail.

You asked for exactly this note, so here it is.

**I am the hardware session.** I am working in `fpga/rtl/field/` on the Field IR
engine — the multiplier bank, the executor, the NOISE2/RIDGE unit, the long-op
dispatcher and the writeback arbiter. Nothing to do with the creature.

**I have not acted on the advice, and I have not touched anything it names.**
No model, animation, texture, atlas, silhouette or clip. The file is untouched
where you put it.

## What this means for getting it to him

You are right about the mechanism: the note landed in a commit the creature
session had already moved past, so pulling it into my session does not deliver
it to his. He will only see it if he pulls after this point, and nothing makes
him do that.

Fastest ways to be sure it reaches him, in the order I would try them:

1. **Paste it straight into his session.** It is 71 lines; that is the only
   delivery that cannot silently fail.
2. **Re-commit it**, as you suggested — he picks it up on his next pull, if he
   pulls.
3. Put it where his workflow already reads: his run folder is
   `runs/CLAUDE-RUNS/RUN-20260828-0326-zixxtrixx-v4-closeout/`, and its
   `TASK_LOG.md` is a file that session writes to every pass, so it is the one
   place it is guaranteed to look.

## One thing I will say, because it is not creature-specific

The part of your note that generalises past Zixxtrixx:

> Ground contact, overlap allowance, deterministic clips, and a measured snout
> vector can all pass while the animal still looks completely fucking wrong.

That is the same failure I have been finding all day on the hardware side, in a
different costume. Three of my own "this matters" claims turned out to be
decoration this morning — provably unable to affect any output — and my tests
passed 274 checks over a dispatcher whose release timing was never checked at
all. Green gates measuring the wrong thing is not a creature problem.

It is already in `CLAUDE.md` as *"component checks passing is not likeness
evidence"*. Your note is that rule catching the same session twice, and it is
worth him reading it that way rather than as a list of things to change.

---

*Written by the hardware session, 2026-08-28. Delete this file once the advice
has reached the Zixxtrixx agent — it exists only to tell you the wrong agent
picked it up.*
