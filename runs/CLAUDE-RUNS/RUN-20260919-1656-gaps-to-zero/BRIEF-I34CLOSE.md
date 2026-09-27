# I34CLOSE — MEASURE FIRST, BUILD SECOND. Do not trust this entry's prose.

**Branch `gz/i34close`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**`I34` is ONE OF TWO REMAINING REGISTER ENTRIES.** Six packets have worked it.
**Every one of them took the entry's description of the remaining work at face
value, and every one of them found that description WRONG** — always in the
direction that made the work look smaller.

**So your first deliverable is not a build. It is an ENUMERATION.**

## READ HANDOVER 15.35 FIRST, IN FULL

It is the law this packet exists to apply. Entry `I55` understated its own
remaining work by **378 bits**, with the refuting arithmetic **one paragraph
above the false sentence, in the same entry**, for seven packets. *"A document
that refutes itself is not self-correcting."*

**`I34`'s prose is under exactly the same suspicion, and here is the specific
reason:**

* The register classifies `I34` as a **boundary tie-off**. It reaches that
  classification **by scanning the entry's PROSE**, whose header still reads
  `-- BOUNDARY.`
* **But the three ports that word refers to — `terr_pt_fld_valid_i`,
  `terr_pt_fld_ready_o`, `terr_pt_fld_height_i` — HAVE LEFT THIS MODULE'S EDGE**
  and are driven inside it by `zhao_field_earth_adapter` (`u_field_earth_adapter`),
  client 3 of the one `u_field_host`.

**I do not know whether there is substance behind that word or whether the text
is simply stale, and I am not going to guess a third time. Find out.**

## PHASE 1 — THE ENUMERATION (do this before touching any RTL)

**Produce a complete, measured list of everything `I34` still requires.** Not
what the entry says it requires. What the tree says.

The channel accounting as it stands — **verify each line, do not inherit it**:

| ordinal | channel | claimed status | your job |
|---:|---|---|---|
| 0 | `height_o` | real consumer, `u_terrain_patch`, live in `composepub_acceptance` case 2 | confirm it is LIVE, not merely connected |
| 1 | `velocity_o` | real consumer: veljoin → TERRAIN.VELOCITY → compcache §4.2 → spdesc → heighttap → `zhao_part_collide` | confirm the chain reaches a particle contact |
| 2 | `material_o` | encoding authored, `TERRAIN.MATJOIN` composed, owner's test met (`composepub_acceptance` 154/0), carried per-triangle to the mosaic (`tile[max/or]` `[255 255]` → `[6 7]`) | confirm end to end |
| 3 | `nav_cost_o` | **owner-ruled to SW.CPUCOLL**, `zref::nav::Service` built, `FIELD.WRITE.NAV` preserved, `efa_nav_cost` produced and classified | confirm the classification CITES its ruling |

**Then ask the question nobody has asked: WHAT ELSE?** The entry names a
`section 9.1 list intake` and a `field-height lane`. Enumerate every obligation
that entry text places on the console, and for each one state **MET / NOT MET /
STALE TEXT**, with the measurement beside it.

**Report the enumeration even if you then close the entry in the same packet.**
It is the artefact that stops the seventh packet repeating the sixth.

## PHASE 2 — THEN ONE OF TWO OUTCOMES

**A) The substance is genuinely complete and the word "BOUNDARY" is stale.**
Then close it — **but closing it means proving it, not editing it.**

**The owner's constraint is explicit and it governs this packet more than any
other:**

> *"Do not make the register reach zero through a dated stopgap, a renamed gap,
> or a computed-but-unread lane."*

So for **each** of the four channels you must show **one** of:

* a **demonstrated live path** — a value produced at one end and observed
  changing at the other, in a bench or the console smoke. **A port connection is
  not a path.** *"It is wired"* is the claim that has been wrong every time on
  this entry.
* or a **cited owner ruling** that places it elsewhere, with the replacement
  obligation **built** — which is nav's situation, and the citation must name the
  document, not paraphrase it.

**And re-run `completion_register.py` BARE.** If it reads **1**, say so with the
evidence. If your prose edit alone moved it, **that is the renamed gap and you
must revert it.**

**B) Something is genuinely missing.** Then **enumerate all of it before building
any of it** — that is the whole point of Phase 1 — and build what fits in one
packet, declaring the rest. **A packet that enumerates honestly and closes
nothing is a good packet.** A packet that closes the entry on a stale sentence is
the worst outcome available here, because it would make the register lie.

## The fences

* **STAY OUT OF GEOM.** `SWAPCLOSE` owns `zhao_geom_*`, the console's geometry
  plumbing, `TriangleDescriptor` and `ProjectedVertex`. **You own TERRAIN and
  FIELD.** `fpga/rtl/prod/zhao_console_core.sv` is shared — **stage the HUNK, not
  the file.**
* **Do not touch nav** beyond confirming its classification cites its ruling.
  `FIELD.WRITE.NAV` is preserved and `efa_nav_cost` stays produced.
* **DO NOT REGRESS VELOCITY** — `terrain_veljoin_directed` 19/0 **on the repaired
  test**; it used to print a hardcoded `0 failures` and could not go red.
  `part_terrain_tap_directed` 1297/0. `composepub_acceptance` 154/0.
* **Do not raise `FAB_LANES`, `FAB_GROUP_PTS` or `FRONT_PTS`** — priced at
  **+11,979 ALUTs and +9 DSP** and refused.
* **Do not build a `zhao_terrain_patch_v2`.** Its four-channel commission was
  retired on measurement; what survived is a **performance** commission, and
  owner authorization 4 says **separate functional completion from performance
  qualification.** `I34` is a functional entry.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — **146,414 registers
  against 1,010**. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.** Remedy: one flat array at a module's own scope (`zhao_dc_sdp_ram`).
  **Rule 6 is not complete.**
* **No console or full-device fit.** Leaf `-MapOnly` only, device named,
  `rtlCleanAtHead` read before quoting.

## Evidence bar

* **The enumeration**, with a measurement against every line.
* **If you close it: a demonstrated live path per channel**, or a cited ruling
  with its replacement built. **Never a port-connectivity argument.**
* **The register measured BARE** before and after, and **stated whether prose or
  substance moved it.**
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  This entry has produced, in one week: a test printing a **hardcoded
  `0 failures`**; an anti-vacuity check passing on a **constant `0xFF`** because
  the smoke never authored the page; a counter (`mat_cells`) **counting cells,
  not values**; and a console assertion whose own comment admitted **it could not
  fail**.
* **Do not write a test that asserts the bug.**

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **RUN GATE 31** (`check_console_closure_lint.py`) if you touch any port. The
  last three merges landed 1, then 31, then 0 PINMISSING because packets' gate
  lists omitted it. **An unconnected output is `.port_o ()`, never an omission.**
* **A new core port costs FOUR things**: the port, **BOTH** `.*` wrapper mutants
  (R220 — fix the wrapper, never the module), the smoke bench's wire, and a
  **reader**.
* **`UNUSEDSIGNAL` is waived across whole directories** — a dead wire raises
  nothing; count readers by hand.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **Other repositories' builds and suites run on this machine.** Classify by
  command line **and parent PID**; kill nothing you did not start. **Read CPU as
  a RATE.** And note the console smoke **verilates into a TEMP directory**, so a
  process filter on your worktree path will not find your own build — that
  mistake is mine, made twice today.

## Deliverable

1. **THE ENUMERATION** — every obligation `I34` places on the console, each
   marked MET / NOT MET / STALE TEXT with its measurement.
2. **The register before and after, measured BARE**, and whether `I34` closed.
3. **If closed: the four demonstrated paths or cited rulings**, one per channel.
4. **Explicitly: did substance close it, or prose?** If prose alone, revert.
5. **Velocity still reaching its consumer**, on the repaired test.
6. **Every claim in this brief or the entry you found FALSE.** Six packets, six
   wrong descriptions. Assume this brief is wrong somewhere and find it.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **Anything belonging to `SWAPCLOSE`** — report it, do not touch it.
9. Branch and commit hash. **Push `gz/i34close` only.** Never `--force`.
