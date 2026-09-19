"""Qwen relay: small, self-contained jobs for the local Qwen, with a ledger and
continuation handoffs so a fresh Qwen can pick up where the last one stopped.

Why this exists (owner, 2026-09-19): the local Qwen is free and fast but has a
~112k-token window that must also hold its reasoning. So work is cut into
CHUNKS, each chunk is one job file, and every answer ends with a CONTINUATION
block that the next job inherits instead of the whole history. Qwen may also
REVIEW another Qwen job (``--review Q007``): the reviewer sees the original
brief, the same inputs and the answer, and must confirm or refute each claim.

Nothing here is trusted by default. The coordinator records a VERDICT on every
answer (``verdict`` subcommand) after checking claims against source; the
ledger shows which answers were verified, partly right or rejected, which is
also Qwen's running calibration record.

Runs INSIDE the HomeAI WSL distro (it needs the broker secret); ``qwen.ps1``
is the Windows wrapper. Paths may be Windows (C:\\...) or /mnt/c/...

  qwen.ps1 run   <job.md>                  assemble, size-check, ask, record
  qwen.ps1 run   <job.md> --dry            assemble + size report only
  qwen.ps1 new   <dir> <slug> [--continue Qnnn] [--review Qnnn]
  qwen.ps1 verdict <dir> Qnnn <verified|partial|rejected> "<one line>"

Job file format (markdown):

  # Q007 short-title
  max_tokens: 12000            (optional; answer + reasoning budget)
  root: C:\\path\\to\\repo       (inputs are relative to this)
  continue: Q006               (optional; inherits Q006's CONTINUATION)
  review: Q005                 (optional; makes this a review of Q005)
  ## Brief
  ...what to do, facts the model cannot see...
  ## Questions
  1. ...
  ## Inputs
  tools/reel/manafold_clips.h:3204-3305
  tools/reel/manafold_art.h:2819-2873
  diff:tools/reel/manafold_probe.cpp      (git diff -U6 of that path)
"""
from __future__ import annotations

import datetime as _dt
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

# ---- budgets ---------------------------------------------------------------
# Context is ~112k tokens. Code tokenises at roughly 3.2 chars/token. Keep the
# prompt well under half the window so reasoning and the answer both fit.
CONTEXT_TOKENS = 112_000
PROMPT_TOKEN_CEILING = 60_000
CHARS_PER_TOKEN = 3.2
# Owner, 2026-09-19: the local Qwen is meant to run at its HIGHEST reasoning
# setting; quality drops fast below it. Empty/truncated answers are a BUDGET
# problem, never a reason to lower effort. So: effort xhigh, and max_tokens is
# sized to whatever the window leaves after the prompt (auto), minus a margin.
DEFAULT_EFFORT = "xhigh"
DEFAULT_MAX_TOKENS = "auto"
WINDOW_MARGIN_TOKENS = 4_000

PREAMBLE = """You are a careful senior engineer doing ONE bounded chunk of a larger task.
Rules:
- Answer ONLY from the material given. If something is not shown, write "not shown" instead of guessing.
- Cite exact file:line for every claim about code.
- Rate each finding: P1 (wrong result / a check that can pass while the property is false), P2 (fragile or misleading), P3 (nit).
- Do not inflate severity. Say "none found" when that is the truth.
- Reason as thoroughly as you need; then write the answer in full.
Put a section "## FINDINGS" FIRST, before any explanation: a table | # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |, or the single line "none found". The coordinator reads only that table plus the CONTINUATION, so it must stand alone.
Finish with this exact section, max 400 words, for whoever continues after you:
## CONTINUATION
- Done: <what this chunk established, as facts with file:line>
- Open: <what remains uncertain or unchecked>
- Next chunk: <the single most useful next job, and which inputs it needs>
"""

REVIEW_PREAMBLE = """You are REVIEWING another model's answer to the job below. It may be wrong.
Check ONLY the claims named under REVIEW FOCUS (or, if none are named, the answer's five most consequential claims).
For each: CONFIRMED / REFUTED / UNVERIFIABLE, one line of file:line evidence. Then at most three important things the answer MISSED.
Be adversarial but fair; do not invent faults. Do not re-derive the whole task.
"""


def winpath(p: str) -> Path:
    p = p.strip().strip('"')
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", p)
    if m:
        return Path(f"/mnt/{m.group(1).lower()}/" + m.group(2).replace("\\", "/"))
    return Path(p)


def parse_job(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    job: dict = {"path": path, "raw": text, "sections": {}}
    head = text.split("\n## ", 1)[0]
    m = re.search(r"^#\s+(Q\d+)\s*(.*)$", head, re.M)
    job["id"], job["title"] = (m.group(1), m.group(2).strip()) if m else (path.stem, "")
    for key in ("max_tokens", "root", "continue", "review", "effort"):
        mm = re.search(rf"^{key}:\s*(.+)$", head, re.M)
        job[key] = mm.group(1).strip() if mm else None
    for sec in re.split(r"\n(?=## )", text)[1:]:
        name, _, body = sec.partition("\n")
        job["sections"][name[3:].strip().lower()] = body.strip()
    return job


def read_input(spec: str, root: Path) -> tuple[str, str]:
    """Return (label, text-with-line-numbers) for one input spec."""
    spec = spec.strip().lstrip("-").strip()
    if not spec or spec.startswith("#"):
        return "", ""
    if spec.startswith("diff:"):
        rel = spec[5:].strip()
        out = subprocess.run(["git", "-C", str(root), "diff", "-U6", "--", rel],
                             capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
        return f"git diff -U6 -- {rel}", out or "(no diff)"
    if spec.startswith("show:"):
        # show:<commit>:<path> -- one file's diff inside a committed change
        _, sha, rel = spec.split(":", 2)
        out = subprocess.run(["git", "-C", str(root), "show", "-U6", "--format=%h %s", sha, "--", rel.strip()],
                             capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
        return f"git show {sha} -- {rel.strip()}", out or "(no diff)"
    m = re.match(r"^(.*?):(\d+)-(\d+)$", spec)
    rel, lo, hi = (m.group(1), int(m.group(2)), int(m.group(3))) if m else (spec, 1, 10**9)
    f = root / rel if not re.match(r"^[A-Za-z]:|^/", rel) else winpath(rel)
    lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
    hi = min(hi, len(lines))
    body = "\n".join(f"{i:5d}| {lines[i - 1]}" for i in range(lo, hi + 1))
    return f"{rel}:{lo}-{hi}", body


def continuation_of(answer_text: str) -> str:
    m = re.search(r"^## CONTINUATION\s*$(.*)", answer_text, re.M | re.S)
    return m.group(1).strip() if m else "(previous answer had no CONTINUATION block)"


def answer_path(job_path: Path) -> Path:
    return job_path.with_name(job_path.name.replace(".job.md", ".answer.md"))


def find_job(dirpath: Path, qid: str) -> Path:
    hits = sorted(dirpath.glob(f"{qid}-*.job.md"))
    if not hits:
        sys.exit(f"no job {qid} in {dirpath}")
    return hits[0]


def assemble(job: dict) -> tuple[str, list[dict]]:
    root = winpath(job["root"]) if job["root"] else job["path"].parent
    parts, receipts = [], []
    review_of = job.get("review")
    if review_of:
        src = parse_job(find_job(job["path"].parent, review_of))
        ans = answer_path(src["path"]).read_text(encoding="utf-8")
        parts.append(REVIEW_PREAMBLE + "\n" + PREAMBLE)
        parts.append(f"# ORIGINAL JOB {review_of}\n## Brief\n{src['sections'].get('brief', '')}\n"
                     f"## Questions\n{src['sections'].get('questions', '')}")
        inputs = src["sections"].get("inputs", "") + "\n" + job["sections"].get("inputs", "")
        src_root = winpath(src["root"]) if src["root"] else root
        root = src_root
        body = re.sub(r"^## CONTINUATION.*", "", ans.split("\n## Answer\n", 1)[-1], flags=re.S | re.M)
        parts.append(f"# ANSWER UNDER REVIEW ({review_of})\n{body.strip()}")
        if job["sections"].get("brief"):
            parts.append(f"# REVIEW FOCUS\n{job['sections']['brief']}")
    else:
        parts.append(PREAMBLE)
        if job.get("continue"):
            prev = answer_path(find_job(job["path"].parent, job["continue"])).read_text(encoding="utf-8")
            parts.append(f"# CARRIED FORWARD FROM {job['continue']}\n{continuation_of(prev)}")
        parts.append(f"# TASK {job['id']}: {job['title']}\n## Brief\n{job['sections'].get('brief', '')}")
        parts.append(f"## Questions\n{job['sections'].get('questions', '')}")
        inputs = job["sections"].get("inputs", "")
    seen = set()
    for spec in inputs.splitlines():
        if spec.strip() in seen:
            continue
        seen.add(spec.strip())
        label, text = read_input(spec, root)
        if not label:
            continue
        receipts.append({"input": label, "sha256": hashlib.sha256(text.encode()).hexdigest()[:16],
                         "chars": len(text)})
        parts.append(f"# INPUT {label}\n```\n{text}\n```")
    return "\n\n".join(parts), receipts


def ask(prompt: str, max_tokens: int, effort: str) -> dict:
    import httpx
    from homeai.common import config
    key = config.read_secret("broker_api_key", create=False)
    with httpx.Client(base_url=f"http://{config.BROKER_HOST}:{config.BROKER_PORT}",
                      timeout=httpx.Timeout(None, connect=10.0),
                      headers={"Authorization": f"Bearer {key}"}) as c:
        r = c.post("/v1/chat/completions",
                   json={"max_tokens": max_tokens, "reasoning_effort": effort,
                         "messages": [{"role": "user", "content": prompt}]},
                   headers={"X-HomeAI-Subject": "owner:claude-qwen-relay"})
        r.raise_for_status()
        return r.json()


def ledger_append(dirpath: Path, row: str) -> None:
    led = dirpath / "QWEN-LEDGER.md"
    if not led.exists():
        led.write_text("# Qwen ledger\n\nOne row per job. Verdict is the coordinator's check against source.\n\n"
                       "| Job | Kind | Title | Prompt tok (est) | Reasoning/answer tok | Seconds | Status | Verdict |\n"
                       "|---|---|---|---:|---:|---:|---|---|\n", encoding="utf-8")
    with led.open("a", encoding="utf-8") as f:
        f.write(row + "\n")


def cmd_run(job_path: Path, dry: bool) -> None:
    job = parse_job(job_path)
    prompt, receipts = assemble(job)
    est = int(len(prompt) / CHARS_PER_TOKEN)
    mt = job["max_tokens"] or DEFAULT_MAX_TOKENS
    room = CONTEXT_TOKENS - est - WINDOW_MARGIN_TOKENS
    max_tokens = room if str(mt).strip() == "auto" else min(int(mt), room)
    print(f"{job['id']}: prompt {len(prompt)} chars ~{est} tokens; max_tokens {max_tokens}; "
          f"window {CONTEXT_TOKENS}")
    for r in receipts:
        print(f"  input {r['input']}  {r['chars']} chars  sha {r['sha256']}")
    if est > PROMPT_TOKEN_CEILING or est + max_tokens > CONTEXT_TOKENS:
        sys.exit(f"TOO BIG: split this chunk (ceiling {PROMPT_TOKEN_CEILING} prompt tokens, "
                 f"{CONTEXT_TOKENS} total). Nothing was sent.")
    if dry:
        (job_path.with_name(job_path.name.replace(".job.md", ".prompt.txt"))).write_text(prompt, encoding="utf-8")
        return
    t0 = _dt.datetime.now()
    effort = job["effort"] or DEFAULT_EFFORT
    data = ask(prompt, max_tokens, effort)
    secs = (_dt.datetime.now() - t0).total_seconds()
    meta, usage = data.get("homeai", {}), data.get("usage", {})
    choice = data["choices"][0]
    answer = (choice["message"].get("content") or "").strip()
    finish = choice.get("finish_reason")
    reasoning = (usage.get("completion_tokens_details") or {}).get("reasoning_tokens")
    status = "EMPTY" if not answer else ("TRUNCATED" if finish == "length" else "ok")
    if status == "ok" and "## CONTINUATION" not in answer and not job.get("review"):
        status = "no-continuation"
    out = answer_path(job_path)
    out.write_text(
        f"# {job['id']} answer — {job['title']}\n\n"
        f"- model: {meta.get('profile_id')}  dispatch: {meta.get('dispatch_id')}\n"
        f"- when: {t0.isoformat(timespec='seconds')}  seconds: {secs:.0f}  finish: {finish}  status: {status}\n"
        f"- usage: {json.dumps(usage)}\n"
        f"- inputs: {json.dumps(receipts)}\n"
        f"- kind: {'review of ' + job['review'] if job.get('review') else 'task'}"
        f"{'  continues ' + job['continue'] if job.get('continue') else ''}\n\n"
        f"## Answer\n\n{answer}\n", encoding="utf-8")
    kind = f"review {job['review']}" if job.get("review") else (f"cont {job['continue']}" if job.get("continue") else "task")
    ledger_append(job_path.parent,
                  f"| {job['id']} | {kind} | {job['title']} | {est} | "
                  f"{reasoning}/{usage.get('completion_tokens')} | {secs:.0f} | {status} | pending |")
    if status in ("EMPTY", "TRUNCATED"):
        print("  -> the window ran out: split the chunk (narrower inputs / fewer questions). NEVER lower effort (owner).")
    print(f"{job['id']}: {status}; {usage.get('completion_tokens')} completion tokens "
          f"({reasoning} reasoning) in {secs:.0f}s -> {out}")


def cmd_new(dirpath: Path, slug: str, cont: str | None, review: str | None) -> None:
    dirpath.mkdir(parents=True, exist_ok=True)
    nums = [int(m.group(1)) for p in dirpath.glob("Q*-*.job.md") if (m := re.match(r"Q(\d+)-", p.name))]
    qid = f"Q{(max(nums) + 1) if nums else 1:03d}"
    p = dirpath / f"{qid}-{slug}.job.md"
    extra = (f"continue: {cont}\n" if cont else "") + (f"review: {review}\n" if review else "")
    p.write_text(f"# {qid} {slug}\nmax_tokens: {DEFAULT_MAX_TOKENS}\nroot: \n{extra}\n## Brief\n\n"
                 f"## Questions\n1. \n\n## Inputs\n", encoding="utf-8")
    print(p)


def cmd_brief(answer: Path) -> None:
    """Print only the FINDINGS table and CONTINUATION -- what the coordinator reads."""
    t = answer.read_text(encoding="utf-8")
    head = t.split("## Answer", 1)[0]
    f = re.search(r"^## FINDINGS\s*$(.*?)(?=^## |\Z)", t, re.M | re.S)
    c = re.search(r"^## CONTINUATION\s*$(.*?)(?=^## Coordinator|\Z)", t, re.M | re.S)
    print(head.strip().splitlines()[0])
    print("## FINDINGS\n" + (f.group(1).strip() if f else "(no FINDINGS section -- read the full answer)"))
    if c:
        print("## CONTINUATION\n" + c.group(1).strip())


def cmd_verdict(dirpath: Path, qid: str, verdict: str, note: str) -> None:
    led = dirpath / "QWEN-LEDGER.md"
    rows = led.read_text(encoding="utf-8").splitlines()
    for i, r in enumerate(rows):
        if r.startswith(f"| {qid} |"):
            cells = r.split("|")
            cells[-2] = f" {verdict}: {note} "
            rows[i] = "|".join(cells)
    led.write_text("\n".join(rows) + "\n", encoding="utf-8")
    ans = answer_path(find_job(dirpath, qid))
    with ans.open("a", encoding="utf-8") as f:
        f.write(f"\n## Coordinator verdict\n\n**{verdict}** — {note}\n")
    print(f"{qid}: {verdict}")


def main(argv: list[str]) -> None:
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd = argv[1]
    if cmd == "run":
        cmd_run(winpath(argv[2]), "--dry" in argv)
    elif cmd == "new":
        cont = argv[argv.index("--continue") + 1] if "--continue" in argv else None
        rev = argv[argv.index("--review") + 1] if "--review" in argv else None
        cmd_new(winpath(argv[2]), argv[3], cont, rev)
    elif cmd == "brief":
        cmd_brief(winpath(argv[2]))
    elif cmd == "verdict":
        cmd_verdict(winpath(argv[2]), argv[3], argv[4], argv[5] if len(argv) > 5 else "")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
