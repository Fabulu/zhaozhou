# LANESCOST — price `FAB_LANES` 1 vs 4, then compose I34's front if it is affordable

**Branch `gz/lanescost`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I34` is ONE OF TWO REMAINING REGISTER ENTRIES.** `I13` closed and
`zhao_terrain_normalmap` came off the disconnected list — **`BUILT BUT NOT
CONNECTED: 0`.** Only `I34` and `I55` are left.

## The one thing standing between I34 and its composition

**GATHERFRONT built the gathering front and refused to compose it, for a reason
that is one measurement wide:**

> *"It needs `FAB_LANES`→4 = four datapath replicas on a device already over on
> ALMs, and **this packet has no area number**. Owed first: one leaf map at
> `FAB_LANES` 1 vs 4."*

**That map is your packet.** It is small, it is decisive, and nothing else on
this entry can move until it exists.

## What is already built and measured — do not re-derive it

**The gathering front is BUILT and in production RTL at its scalar default.** It
is **`FRONT_PTS`, a parameter on `zhao_field_host_v2` — NOT a new block.** The
fabric could always do it: `zhao_field_v3_exec.sv:82` makes LANES independent
datapath replicas sharing one instruction stream, so lane *p* can hold point
*p*. The front was writing `{FAB_LANES{cur_in[lane_sel]}}` — **one point
replicated** — and reading lane 0 back. **Writing point p into lane p is the
whole gather: one expression.**

**Measured, 31 checks / 0 failures, at the console's parameters:**

| | clk/group | per association | vs 6,000 |
|---|---:|---:|---:|
| scalar front, as composed | 248.00 | 74,507 | 12.42× |
| **gathering front** | **62.00** | **19,265** | **3.21×** |
| **+ `INIT_PROOF`** | **30.00** | **9,761** | **1.63×** |
| ceiling | 17.30 | 6,000 | 1.00× |

**Exactly ÷4.** And `FRONT_PTS=1` is proven **bit-identical** — the census still
reports 248.00 / 74,507 / 12.42× to the digit.

**The discriminator is committed and SEEN TO FAIL.** Four points in must give
four *different* answers — the only check a replicating front fails, since
cadence, run count, `StOk`, overlap and "the output is 42" all pass on one.
Production answers **`42 43 44 45`**; the mutant answers **`42 42 42 42`**, 64
collapsed / 0 distinct, **while asserting it ran**, so the collapse cannot be a
refusal wearing a control's clothes.

## YOUR JOB

1. **One leaf map of `zhao_field_host_v2` at `FAB_LANES` 1, and one at 4**, on
   the **shipping part `5CSEBA6U23I7`**, both `rtlCleanAtHead` true, device
   named on both rows.
2. **Report the ALUT, register, memory and DSP delta.** That is the number
   GATHERFRONT did not have.
3. **THEN decide, and say which you did:**
   * **Affordable** → compose it. Raise `FAB_LANES` to 4, keep the front, and
     re-measure the clocks at the composed configuration.
   * **Not affordable** → **refuse with the number**, and say what the design
     would have to give up. **That is a full deliverable**, and on a console
     measured at **350% ALUTs** it is the likelier answer.

**Do not compose first and measure after.** The whole point of this packet is
that the previous one correctly declined to spend area it had not priced.

## THE SECOND FABRIC PARAMETER NOBODY NAMED

**The bill is TWO parameters, not one.** GATHERFRONT found it by an elaboration
`$fatal`: **`FAB_GROUP_PTS` must be a multiple of `FAB_LANES`**
(`zhao_field_v3_dispatch.sv:252`). It is named in **no** document. Setting
`-GFAB_GROUP_PTS=1` beside `-GFAB_LANES=4` aborts at elaboration — **and then
presents as *alive at zero CPU*, because the VlThreadPool teardown deadlocks.**
So if a run of yours hangs with no output, suspect this before you suspect a
wedge.

## What you may NOT conclude

* **The ÷2 term is NOT a free multiplier.** On this host **a run's identity IS
  its program slot** (`store[ctx*PLAN+pc]`, `fab_start_ctx = cur_slot`), so two
  groups of one program cannot be in flight in one context. It needs a
  redesigned two-deep front **and** a second resident slot out of eight.
  GATHERFRONT's ≈5,600 figure is **arithmetic, declared as such**, and it is
  under 6,000 *only just* and *only for that census's two-uop program*.
* **`INIT_PROOF` had nothing to land** — `hdr_ipok`, the FH08 fast path, the
  doorbell's 3-bit kind and `verify_init_proof` all exist and connect. **Kind
  6's producer is SW.STREAM, outside this console.** Do not "build" it.

## The fences

* **DO NOT REGRESS VELOCITY.** `terrain_veljoin_directed` 19/0,
  `part_terrain_tap_directed` 1297/0.
* **Do not compose a prefix.** If the front goes in composed, its consumer
  chain goes with it.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — a fourth Quartus
  17.0.2 inference killer measured this week; it cost `zhao_geom_arenabin`
  **146,414 registers against 1,010** for the identical circuit. Generate-**IF**
  infers; module scope infers; **the LOOP is the killer.** `rule 6` catches it
  now, and the checker was **100% false alarms and 100% miss** on that file
  before, so **its silence is not a verdict.**
* **Do NOT start a console or full-device fit.** Leaf `-MapOnly` is exactly what
  this packet is for. **Every map must name its `-Device`**, rows on different
  devices must never be differenced, and **read `rtlCleanAtHead` before quoting
  ANY row.**

## Evidence bar

* **Two rows, same block, same device, differing in one parameter.** That is a
  like-for-like; anything else is not.
* **If you compose: the clocks re-measured at the composed configuration**, not
  inherited from the census.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Seven packets this week found their own controls vacuous — including one that
  measured **refusals** and called it a 62× speed-up, caught only by the status
  check.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — and note
  **`| tail` reports TAIL's exit code**, which two packets hit this week. That
  applies to **every gate**, not just builds.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.**
* **`run_block_fit.ps1` writes `-Device` UNVALIDATED** — a packet produced a row
  whose device was a file path. **Check the row you just wrote.**
* **Splitting `-ExtraSources` through `powershell -File` breaks it** — splat.
* **Do not edit RTL while a `cmake --preset` is verilating it** — GATHERFRONT
  did and had to redo work.
* **`git show` hands back INDEX content at LF while the working copy is CRLF**,
  so a diff of a correct file can claim thousands of changed lines. GATHERFRONT
  hit this at 1,690 lines; a CRLF write-back turned `check_eol_worktree` red on
  the merge today.
* **Check your commit did not stale a mutant copy** — GATHERFRONT's first one
  staled two, and it correctly diagnosed them as ITS OWN by checking the base
  was green rather than calling them inherited.
* **Three concurrent smoke forms is this box's ceiling.**
* **One `ctest` at a time per build tree**, and other lanes' trees are live —
  classify by command line and parent PID, touch none.

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **The two rows**, device named, with the ALUT/register/memory/DSP delta.
3. **Composed or refused — and the number that decided it.**
4. **If composed: the clocks at the composed configuration**, against ≤ 6,000.
5. **What happened to velocity's chain.**
6. **Every claim in this brief or the record you found FALSE.** Every packet
   this week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/lanescost` only.** Never `--force`.
