# Q011 answer — register-reads-zero

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 97613111cc5448aea9ef830227274daa
- when: 2026-09-20T07:40:22  seconds: 569  finish: stop  status: ok
- usage: {"completion_tokens": 33978, "completion_tokens_details": {"reasoning_tokens": 28438}, "prompt_tokens": 29427, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 63405}
- inputs: [{"input": "tools/budget/completion_register.py:1-1392", "sha256": "62955feb5249e973", "chars": 83850}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P1 | completion_register.py:414-415 | If `design/blocks.yml` is missing or moved, `ledger_blocks()` returns `[]`, so every capability gap is dropped and the total can be 0. | `if not BLOCKS.exists(): return []` |
| 2 | P1 | completion_register.py:1051-1057 | A deferred/hardware-blocked block is excused if its note merely contains `ruling`, `spec`, `charter`, `plan`, `SS\d`, or `§\d`; the citation is not checked to exist or apply. | `has_cite = bool(re.search(...))` then `deferred_ok.append(...); continue` |
| 3 | P1 | completion_register.py:1009-1012 | A `superseded_by:` note excuses a block if any cited `tests/` path is a substring of `CMakeLists.txt`, even if unrelated to the capability or present only in a comment. | `built = [p for p in tests if p[len("tests/"):] in cmake_text]` |
| 4 | P1 | completion_register.py:927-932,1058-1064 | An `_ALIAS` entry or `implementation:` field can name any existing connected module and clear the capability with no validation that the module is the correct one. | `resolve_module` returns alias/stem; `elif mod in closure and mod in live: connected.append(...)` |
| 5 | P1 | completion_register.py:270-304 | If the `zhao_console_core` target has no recognized source line, `started` stays false and the closure walk can absorb later targets’ source lines, shrinking the disconnected count. | `started` is set only at line 281, but breaks at 302/304 are guarded by `if started` |
| 6 | P1 | completion_register.py:72,82-95,126-130 | If the `INCOMPLETE -- TIED OFF` marker is present but no `_TIEOFF` entries are parsed—e.g. reformatted ids, empty block, or marker after the real list—`tieoffs()` returns `[]` with no failure. | `start = text.find(...)`, loop appends nothing, `missed` only scans for `_TIEOFF` matches |
| 7 | P1 | completion_register.py:777-781,1142-1144,1301-1316,1388 | `superseded_in_closure()` is only printed, omitted from `mandatory_gaps`, absent from `--json`, and does not affect exit code, so the campaign can stop while old-version modules are composed. | total formula lacks `sup`; `--json` returns at 1303 before printing it; return is based only on `rep["mandatory_gaps"]` |
| 8 | P2 | completion_register.py:83,127-130 | The parser matches indented entry heads via `line.strip()`, but the truncation guard only sees column-zero `//` lines, so an indented entry after an early stop is not reported. | `m = _TIEOFF.match(line.strip())` vs `if ln.startswith("//")` in `missed` |
| 9 | P2 | completion_register.py:64,213,220 | Any entry head containing `NOT a tie-off` is classified `resolved-in-composer` and not counted, even if the phrase is not a deliberate declaration. | `kind = ... if _NOT_A_TIEOFF.search(t["head"])` and `mandatory_gap = t["kind"] != "resolved-in-composer"` |
| 10 | P2 | completion_register.py:420,450 | A block whose `kind` token is not exactly `rtl`—e.g. quoted or differently cased—is silently omitted from the capability total. | `return [b for b in out if b["kind"] == "rtl"]` |
| 11 | P2 | completion_register.py:934-936,1063-1064 | Convention resolution can name a stale prototype file; if that module is connected, the capability reads present even though the current implementation is absent. | `cand = "zhao_" + block_id...` then connected if `mod in closure and mod in live` |
| 12 | P2 | completion_register.py:1001 | The “cites a ruling” requirement is satisfied by any bare `R<number>` in the note, not a meaningful ruling citation. | `re.search(r"\bruling\s+[A-Z]+\d+\b|\bR\d+\b", note)` |
| 13 | P2 | completion_register.py:813-898,1065-1071 | `board_addition()` trusts the external `module_graph` oracle; if that graph over-reports board reach, capabilities count connected. | `decl, inst = build(...)` then `elif mod in board: connected.append(...)` |
| 14 | P2 | completion_register.py:1170-1233 | The self-test only proves `tieoffs()` on a 3-entry synthetic file and one blank-line guard; it does not exercise empty parse, ledger excuses, closure absorption, aliases, or real header drops. | sample checks ids/kinds/truncation only; no `disconnected()` or real-file coverage |
| 15 | P3 | completion_register.py:394-397 | `has_test_target()` is unused in this file and uses a substring test, so it is misleading dead code rather than a live check. | no call in the shown code; `return module in CMAKE.read_text(...)` |
| 16 | P3 | completion_register.py:330-333 | An unreadable closure source file is skipped in `instantiated_in()`, shrinking `live` and making the count too high rather than too low. | `except OSError: continue` |
| 17 | P3 | completion_register.py:243-244 | Missing `fit_targets.yml` returns an empty closure, making connected capabilities read as gaps; direction is too high. | `if not TARGETS.exists(): return set()` |
| 18 | P3 | completion_register.py:869-871 | `ImportError` from `module_graph` caches an empty board addition, making board-connected capabilities read as gaps; direction is too high. | `except ImportError: _BOARD_CACHE["d"] = set()` |
| 19 | P3 | completion_register.py:387-388 | `closure_paths()` repeats the same next-target break already present at lines 371-372. | identical `if started and re.match(r"\s*-\s*top:", line): break` |
| 20 | P3 | completion_register.py:230-231 | Naive `//` and `/* */` stripping can be fooled by string contents; effect is uncertain and not a primary low path. | `re.sub(r"//[^\n]*", "", src)` |
| 21 | P3 | completion_register.py:71,245,331,397,416,1032 | `errors="replace"` can corrupt non-UTF-8 text, allowing a corrupted entry line to fail regex matching and be dropped. | repeated `read_text(..., errors="replace")` |

## ANALYSIS

### 1. Silent drops and direction

The printed number is computed at `completion_register.py:1142-1144` as:

```python
total = (len(gaps) + len(dis["built_not_connected"])
         + len(dis["unbuilt"]) + len(dis["uncited_excuse"])
         + len(dis["unresolvable"]))
```

where `gaps` comes from `tieoffs()` at `completion_register.py:1133`, and `dis` comes from `disconnected()` at `completion_register.py:1134`.

The main **too-low** silent drops are:

1. **Missing capability ledger**  
   `ledger_blocks()` returns `[]` when `BLOCKS` does not exist, `completion_register.py:414-415`.  
   Input: `design/blocks.yml` missing, renamed, or on a different path.  
   Effect: all capability gaps disappear. If tie-off gaps are also zero, the total is zero.

2. **Deferred/blocked excuse by keyword only**  
   At `completion_register.py:1051-1057`, a block with `deferred: true` or `blocked_on: hardware` is excluded from the total if the note contains one of:

   ```text
   ruling | SS\d | §\d | spec | charter | plan
   ```

   The citation is not checked for existence, relevance, currency, or ownership.  
   Input: `deferred: true # per plan`.  
   Effect: a real mandatory block is excused and not counted. Direction: too low.

3. **Superseded excuse by weak citation check**  
   `superseded_verdict()` is at `completion_register.py:982-1013`. It does check that cited files exist, `completion_register.py:1006-1008`, but it does not check that the cited test is related to the capability, that the replacement still implements the capability, or that the ruling is still true.  
   It also checks CMake build status by substring:

   ```python
   built = [p for p in tests if p[len("tests/"):] in cmake_text]
   ```

   at `completion_register.py:1010`. A filename in a comment or an unrelated built test can satisfy the condition.  
   Input: `superseded_by: "ruling R5: tests/foo/old_test.cpp"` where `old_test.cpp` exists and appears in `CMakeLists.txt` but does not test the replacement.  
   Effect: capability is excused. Direction: too low.

4. **Alias/implementation trusted without validation**  
   `resolve_module()` returns `_ALIAS[block_id]` immediately if present, `completion_register.py:927-928`. It also accepts an `implementation:` path if the file exists, `completion_register.py:929-932`.  
   In `disconnected()`, if the resolved module is in the closure and live, the capability is connected, `completion_register.py:1063-1064`.  
   Input: an alias or implementation field points at a connected module that is not actually the capability’s current implementation.  
   Effect: capability reads present. Direction: too low.

5. **Closure walk can absorb another target**  
   `console_closure()` only breaks at the next target after `started` is true, `completion_register.py:302-304`. `started` is set only when a source line beginning with `- fpga/rtl/` or `- fpga\rtl\` is seen, `completion_register.py:272-281`.  
   Input: the `zhao_console_core` target has no recognized source line, or its sources use a different path prefix, and a later target has recognized source lines.  
   Effect: the closure grows with another target’s sources. More modules appear composed/connected. Direction: too low.  
   The cross-check against `closure_paths()` at `completion_register.py:1119-1132` will not catch this if both walkers make the same mistake.

6. **Tie-off header can parse zero entries silently**  
   `tieoffs()` exits only if the marker is absent, `completion_register.py:72-79`. If the marker is present but no line matches `_TIEOFF`, `out` remains empty.  
   The truncation guard at `completion_register.py:126-130` only looks for later `_TIEOFF` ids. If the entries were reformatted so they no longer match `^//\s*(I\d+)\.\s+`, nothing is detected.  
   Input:
   ```text
   // INCOMPLETE -- TIED OFF
   //  GAP-1. something
   ```
   or an empty block.  
   Effect: tie-off gap count is zero. Direction: too low.

7. **Superseded composed modules are not in the stop condition**  
   `superseded_in_closure()` is computed in `main()` at `completion_register.py:1307`, printed at `completion_register.py:1308-1317`, but not included in `mandatory_gaps`.  
   `--json` returns before printing it, `completion_register.py:1301-1303`. The exit code is based only on `rep["mandatory_gaps"]`, `completion_register.py:1388`.  
   Effect: the register can print zero and start the expensive run while composed modules are superseded by newer modules on disk. Direction: too low for the campaign’s real stop condition.

### Other too-low paths

- **Indented truncation**: the parser matches indented entry heads because it uses `line.strip()` at `completion_register.py:83`, but the missed-entry guard only checks `ln.startswith("//")` at `completion_register.py:127-130`. An indented entry after an early break can be silently dropped.
- **Head phrase exclusion**: any head containing `NOT a tie-off` is excluded, `completion_register.py:213,220`. The head is intended to be a declaration, but the regex does not verify intent.
- **Exact `kind: rtl` filter**: blocks whose kind token is not exactly `rtl` are dropped, `completion_register.py:450`.
- **Convention resolution to stale prototype**: if the constructed module name exists and is connected, the capability reads present, `completion_register.py:934-936,1063-1064`.
- **Board trust**: `board_addition()` relies on `module_graph.build`; the internals of that graph builder are not shown. If it over-reports reachability, capabilities become connected, `completion_register.py:813-898,1065-1071`.

### Safe/over-counting skips

Some silent skips read **high**, not low:

- `TARGETS` missing returns empty closure, `completion_register.py:243-244`.
- Unreadable source files are skipped in `instantiated_in()`, `completion_register.py:330-333`.
- `module_graph` import failure returns empty board addition, `completion_register.py:869-871`.
- `successor_in()` only accepts suffix-version successors, `completion_register.py:955-973`; missing an infix successor would over-count, not under-count.
- Missing CMake makes superseded notes fail, because no test can be “built”, `completion_register.py:1032,1010-1012`.

### Genuinely defensive parts

- Duplicate tie-off ids are a hard failure, `completion_register.py:161-174`.
- `NOT a tie-off` in an entry body is a hard failure, `completion_register.py:199-207`.
- The column-zero truncation guard catches the historically observed blank-line drop for well-formed column-zero entries, `completion_register.py:126-144`.
- Stale `None` aliases are hard failures, `completion_register.py:901-923,1109-1117`.
- The two closure walkers are cross-checked, `completion_register.py:1119-1132`, though they can fail together.
- A board that no longer contains the core grants no board membership, `completion_register.py:896-897`.
- Uncited flags are counted as gaps, `completion_register.py:1057`.
- Unresolvable capabilities are counted as gaps, `completion_register.py:1061-1062,1143`.

### 2. Zero path specifically

For the printed total to be 0:

1. `tieoffs()` must produce no mandatory gaps:
   - marker present but no parsed entries, `completion_register.py:82-95`;
   - parsed entries all have `NOT a tie-off` in head, `completion_register.py:213,220`;
   - real entries are truncated and not detected because they are indented or non-`I\d+`, `completion_register.py:83,127-130`;
   - marker anchors after the real entries, `completion_register.py:72`.

2. `disconnected()` must contribute nothing:
   - `ledger_blocks()` returns empty because `BLOCKS` is missing or no block has exact `kind: rtl`, `completion_register.py:414-415,450`;
   - every block is excused by `superseded_verdict()`, `completion_register.py:1034-1038`;
   - every block is excused by deferred/blocked keyword, `completion_register.py:1053-1056`;
   - every block resolves to a module that is in closure/live, in board, or has a versioned successor, `completion_register.py:1058-1073`.

3. No hard failure fires:
   - missing core, missing marker, duplicate ids, body `NOT a tie-off`, stale `None` alias, and closure mismatch would all stop the run, `completion_register.py:69-70,73-79,161-174,199-207,1109-1117,1122-1132`.

So zero can occur from an empty ledger, an empty/misformatted tie-off block, weak excuses, wrong aliases, inflated closure, or trusted board graph—not only from “no gaps”.

### 3. Self-test

The self-test at `completion_register.py:1170-1233` only tests `tieoffs()` on a synthetic file:

- three entries, `completion_register.py:1172-1178`;
- expected ids/kinds, `completion_register.py:1189-1196`;
- mandatory count, `completion_register.py:1197-1199`;
- one blank-line truncation guard, `completion_register.py:1205-1220`.

It does **not** test:

- marker present but zero entries;
- entries with non-`I\d+` ids;
- indented entries after truncation;
- duplicate ids;
- `NOT a tie-off` in body;
- prose lines that accidentally match `_TIEOFF`;
- `ledger_blocks()` parsing;
- deferred/blocked excuses;
- `superseded_verdict()` semantic weaknesses;
- closure absorption;
- real `zhao_console_core.sv` header;
- the full `audit()` total.

The separate `_superseded_self_test()` at `completion_register.py:1236-1261` tests some negative `superseded_verdict()` cases, but not an unrelated built test, stale ruling, or semantic replacement. The board self-test at `completion_register.py:1264-1295` tests a toy graph, not the real `module_graph` builder.

Therefore the self-test can pass while the real parse silently drops entries or while the full register prints zero for the wrong reasons.

### 4. Excuses

- **Deferred/blocked**: the citation is required to be non-empty in a weak sense, not checked. It only needs a keyword, `completion_register.py:1052`. A stale note can excuse a real gap.
- **Superseded**: the citation is checked more: ruling token, cited files exist, at least one `tests/` path appears in CMake. But relevance and current truth are not checked, `completion_register.py:1001-1012`.
- **Alias**: non-`None` aliases are trusted. Only `None` aliases are re-checked by `_stale_none_aliases()`, `completion_register.py:901-923`. A non-`None` alias can point at a connected module and clear the capability.
- **Implementation field**: an existing file path is trusted, `completion_register.py:929-932`. No check that the file declares the expected module or that the module is the current implementation.

### 5. Asymmetry ranking

Too-low first:

1. Missing `blocks.yml`, `completion_register.py:414-415`.
2. Deferred/blocked keyword excuse, `completion_register.py:1051-1057`.
3. Superseded weak citation, `completion_register.py:1009-1012`.
4. Alias/implementation to connected module, `completion_register.py:927-932,1058-1064`.
5. Closure absorption, `completion_register.py:270-304`.
6. Empty/misformatted tie-off parse, `completion_register.py:72,82-95,126-130`.
7. Superseded composed modules not fatal, `completion_register.py:777-781,1142-1144,1388`.
8. Indented truncation gap, `completion_register.py:83,127-130`.
9. Head phrase exclusion, `completion_register.py:213,220`.
10. Exact `kind: rtl` filter, `completion_register.py:450`.
11. Convention to stale prototype, `completion_register.py:934-936,1063-1064`.

Over-counting or neutral nits: missing targets, unreadable sources, board import failure, dead `has_test_target()`, repeated break, naive comment stripping.

## CONTINUATION
- Done: Established that the printed total is `completion_register.py:1374-1377`, computed from `completion_register.py:1142-1144`. Found low-count paths: missing `blocks.yml` at `completion_register.py:414-415`; deferred/blocked keyword excuse at `completion_register.py:1051-1057`; superseded citation substring/unrelated-test weakness at `completion_register.py:1009-1012`; alias/implementation trust at `completion_register.py:927-932,1058-1064`; closure absorption when no recognized source line starts the walk at `completion_register.py:270-304`; empty/misformatted tie-off parse at `completion_register.py:72,82-95,126-130`; and `superseded_in_closure()` not affecting exit code or JSON at `completion_register.py:777-781,1142-1144,1301-1316,1388`. Also established self-test is narrow: `completion_register.py:1170-1233` only tests synthetic `tieoffs()` and one truncation case.
- Open: Actual `zhao_console_core.sv`, `design/blocks.yml`, `design/fit_targets.yml`, `tests/CMakeLists.txt`, and `module_graph.py` are not shown. Need verify which low paths are live: whether `blocks.yml` exists, whether any deferred note uses only a keyword, whether any `superseded_by:` note cites an unrelated built test, whether any alias points to a connected stale module, whether the core fit target has recognized source lines, and what `module_graph.build()` actually returns.
- Next chunk: Build minimal fixtures that make `main()` print zero through each low path. Start with: (a) missing `blocks.yml` plus an `INCOMPLETE -- TIED OFF` marker with no `I#` entries; (b) one block with `deferred: true # per plan`; (c) one block with `superseded_by:` citing an unrelated existing built test; (d) a fit-target where the core target has no `- fpga/rtl/` source line before a later target’s sources. Inputs needed: `completion_register.py`, a scratch ROOT layout with `fpga/rtl/prod/zhao_console_core.sv`, `design/fit_targets.yml`, `tests/CMakeLists.txt`, and optionally `design/blocks.yml`.

## Coordinator verdict

**verified** — The most valuable job so far. Twenty-one findings against the instrument the whole campaign is steered by; I verified the seven P1s in source and fixed four, each of which was a genuine path to a SMALLER number. (1) A missing design/blocks.yml returned [] -- deleting EVERY capability gap. tieoffs() has refused a missing core since it was written; the other root had no such guard. Now refuses. (2) The 'INCOMPLETE -- TIED OFF' block parsing to ZERO entries returned a clean [] -- the existing truncation guard compares parsed against what appears LATER, so it is structurally blind to 'nothing matched anywhere'. Now refuses. (3) console_closure()/closure_paths() guarded the next-target break on started, so a core target whose source lines stopped being recognised ran to EOF absorbing every later target's sources -- closure membership is one of the two tests for 'connected'. Break is now unguarded and an empty closure is a hard failure. (4) THE BIG ONE: superseded_in_closure() was computed in main(), printed inside exclamation marks, and then DROPPED -- absent from --json, from the report dict, and from the exit code. The campaign could have reached '0 gaps, exit 0' with an old version composed, and the honest fit would then have measured a machine nobody ships. That is the owner's one capitalised rule. It is now in the report, in the JSON, and in a single _done() that both return paths share. All four guards are FIRED by a new _blind_root_self_test, and I ran a negative control on that self-test itself: reverting the blocks.yml guard makes it report 'a missing design/blocks.yml did not refuse'. Register unchanged at 27 and superseded is empty, so nothing was masking anything today -- the value is entirely in what could not have been seen later. Not fixed, recorded: the keyword/substring weakness of the deferral and superseded citations (F2/F3/F12) is real but wants a design decision about what a citation must prove, not a patch.
