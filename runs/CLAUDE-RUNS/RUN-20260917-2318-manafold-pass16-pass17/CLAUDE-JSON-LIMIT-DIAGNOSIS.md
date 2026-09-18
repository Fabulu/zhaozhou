# Claude Code JSON length failure — diagnosis and fix

**Date:** 2026-09-18
**Affected session:** `33af73ab-55ab-4714-988b-7c3762672a27`
**Observed error:** `API Error: 400 Invalid JSON: length limit exceeded`

## What failed

The failed session sequentially opened 27 Manafold every-frame PNG contact sheets. Opening them one at a time did not bound the next request: each `Read` result remained in conversation history.

Measured without reopening the images into a model context:

- persisted transcript: **34,058,648 bytes**;
- 27 image `Read` result records: **32,924,055 bytes**;
- embedded image strings alone: **16,449,844 characters**;
- individual embedded image strings: approximately **338–680 KiB**;
- last successful model request: approximately **183k input tokens**;
- following request and three automatic retries: identical HTTP 400 failure.

This is a serialized JSON/request-size failure, not exhaustion of the model's nominal 1M context. Retrying cannot heal it because every retry reconstructs the same oversized history.

## Why the earlier 150k settings edit did not take effect

`C:\Users\Fabs\.claude\settings.json` was valid and contained:

- `autoCompactEnabled: true`;
- `autoCompactWindow: 150000`;
- `precomputeCompactionEnabled: true`.

But the session was launched through `C:\programmieren\_devenv\ai.ps1`, whose GPT branch exported:

```powershell
$env:CLAUDE_CODE_AUTO_COMPACT_WINDOW = '872000'
```

The live process confirmed `CLAUDE_CODE_AUTO_COMPACT_WINDOW=872000`. The launcher environment overrode the user setting, so changing only `settings.json` could not alter that session's effective window.

## Permanent configuration decision

A global 150k window was rejected after review: it discards 85% of a nominal 1M context to mitigate a byte-volume problem, and still cannot guarantee that a few large images stay under the JSON request limit.

The two launchers and `settings.json` now agree on **800k**, with background summary precomputation enabled. The GPT backend's independent maximum input declaration remains **872k**, leaving 72k headroom while preserving most of the useful text context. Backups were made before editing and both PowerShell launchers plus the JSON were parsed successfully afterward.

The already-running session inherited 872k; process environments cannot be rewritten from a child shell. Future `ai gpt` launches receive 800k.

## Payload-aware review rule

Large image banks are reviewed in **fresh isolated GPT forks**, four full sheets per fork. Each fork writes text findings to disk and ends; the main thread receives only the small text verdict, never the image base64. Reduced composites are an alternative where they preserve enough visual detail. A low `--autocompact` value may be used for one deliberately image-heavy session, but not as the global context policy.

Never resume a session already poisoned by this 400 for more image review.
