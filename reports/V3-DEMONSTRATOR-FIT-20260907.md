# The V3 completion-bank demonstrator, fitted

`zhao_texture_v3own@v3-full`, 8,979 s, digest `1627c71acdf4` — byte-identical to
the map-only row and to the bytes the 467-check adversarial bench measured, so
synthesis, tests and fit refer to the same code (§26.2's acceptance condition).

## The numbers, and the one that is a breach

| | measured | rule | |
|---|---|---|---|
| ALM | **5,678** | ≤ 1,800 (§21.6) | **BREACH, 3.15×** |
| fmax | 75.79 MHz | — | |
| registers | 4,864 | — | |
| DSP | 0 | ≤ 0 | pass |
| M10K | **17** | ≥ 9 | pass |
| memory bits | **20,640** | ≥ 19,584 | pass |
| virtual pins | 952 | — | |

## The structural claim holds. The area claim does not.

**Memory bits came out at 20,640 — the map-only figure to the bit**, and 17
M10Ks were inferred against nine declared banks. The floor rules were written to
catch a bank COLLAPSING into fabric, which would show as bits going *down*;
nothing collapsed. Registers rose 4,220 → 4,864 through placement, which is
ordinary.

So the thing the experiment was built to test is answered YES: payload lives in
memory, the write-enable cone is gone structurally, and 4,864 registers at
64-owner capacity stands against `zhao_texture_fragrob`'s **9,431 with zero
M10K** at the same depth.

**And it costs 5,678 ALMs where §21.6 budgets 1,800.** That rule was set at the
budget deliberately, and this report exists because it fired. §21.1 is explicit
about what happens next: *"A design exceeding them is reported as a failed
allocation, not quietly accepted because it is smaller than 16,192 ALMs."* It is
smaller than 16,192. It is still a failed allocation.

## fmax is internal, and the limit is the ready queue

| origin | count | worst | implied |
|---|---|---|---|
| starts at a PIN | 3,959 | −2.601 ns | 79.36 MHz |
| **starts INSIDE** | 97 | **−3.194 ns** | **75.79 MHz** |

Deleting the fit boundary would buy about 3.6 MHz and no more — the design's own
logic sets the number. The three worst internal paths all begin in the same
place:

    zhao_texture_v3rq:u_rq_tmu |wp_q[0] -> adm_accept_o
    zhao_texture_v3rq:u_rq_init|wp_q[0] -> ev_quiet_o
    zhao_texture_v3rq:u_rq_tmu |wp_q[0] -> Mux2~4_OTERM3179

**The ready queue's write pointer**, not the banks and not the completion
pipeline. `zhao_texture_v3rq` is the block that carries two head registers per
queue precisely because a synchronous body has no combinationally visible head;
that head is now on the critical path into `adm_accept_o`.

## What to do with this, honestly

The ALM figure is the finding, and it is not obviously fatal: the lane could not
attribute 6,532 ALUTs at map time and believed it to be the 64-way scoreboard
decode, unproven. Three things follow, in order:

1. **Attribute the 5,678 before changing anything.** A 3.15× breach that nobody
   has localised is a number, not a diagnosis, and this repository's own law is
   that the comfortable explanation arrives first and explains almost all of it.
2. **The ready queue is the timing limit and may also be the area one** — three
   of three worst paths and a structure that needs two registers per queue to
   hide a synchronous read.
3. **Do not compose it into the island on these numbers.** §26.1 forbids that
   until the shared record and credit contracts are fixed, and 5,678 ALMs for
   the owner/completion machinery alone would consume most of a budget the whole
   island has to fit inside.

## What this does not establish

The write-enable cone is gone *structurally* — every payload write enable is a
bare flop output, and Quartus's own "Registers Removed" merged `c3a_we_q` with
`c3a_v_q`, seeing registers rather than logic folded into a memory enable. It is
not measured in the timing report: no path in the top 97 ends in a bank write
enable, which is consistent with the cone being gone but is not the same as
proving it.
