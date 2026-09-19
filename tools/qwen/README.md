# Qwen relay

The local Qwen is free, fast and has a ~112k-token window that must also hold
its own reasoning. Use it **aggressively, in chunks**; never trust it unchecked.
One Qwen job at a time (two starve each other on the GPU).

## The loop

1. **Chunk.** One job = one bounded question over a few hundred lines of named
   input (`file:lo-hi` or `diff:path`). `run --dry` prints the size; the tool
   refuses anything over ~60k prompt tokens. Three to six numbered questions
   per job; more than that and the answer truncates.
2. **Run.** `qwen.ps1 run <job.md>` assembles the prompt from source *at run
   time* (so the job file stays tiny), asks the broker directly and writes
   `Qnnn-*.answer.md` with model, dispatch, token usage and input hashes.
3. **Continue.** Every task answer ends with a `## CONTINUATION` block (done /
   open / next chunk, ≤400 words). `new <dir> <slug> --continue Qnnn` makes a
   job that inherits only that block, not the history, so a fresh Qwen picks up
   where the last stopped without the context the last one burned.
4. **Qwen reviews Qwen.** `new <dir> <slug> --review Qnnn` builds a review job
   that sees the original brief, the same inputs and the answer, and must mark
   every claim CONFIRMED / REFUTED / UNVERIFIABLE and list what was missed.
5. **Coordinator spot checks (Stichproben) — mandatory.** Treat Qwen as a very
   zealous intern. After the review, the coordinator personally checks against
   source: every P1, every claim that would change code or a gate, anything
   the reviewer and the author disagree on, plus at least two random other
   claims. Then records the result:
   `verdict <dir> Qnnn <verified|partial|rejected> "spot 5/5: ..."`
   The ledger is the calibration record: what Qwen gets right, what it misses,
   and whether the Qwen reviewer caught it.

## Budgets learned (2026-09-19)

- 1,500 max_tokens truncated a short answer; 6,000 came back **empty** (the
  reasoning ate it). A six-question design job used 9.6k reasoning tokens and
  truncated at 14k. Default is 12k; design jobs 16k+; fewer questions beats
  more tokens.
- ~4.6k-token prompts answer in ~2–3 minutes.

## Files

- `qwen_job.py` runs inside the HomeAI WSL distro (it needs the broker
  secret); `qwen.ps1` is the Windows wrapper.
- Jobs, answers and `QWEN-LEDGER.md` live in the run folder's `qwen/` directory.
