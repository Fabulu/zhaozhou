# Contract — INPUT.SNAC (SNAC adapter)

> Ledger: `design/blocks.yml` · owner ZH-017 · phase 2 · RTL
> `fpga/rtl/input/zhao_input_snac.sv`

**AUTHORED 2026-09-20 under owner ruling R7**
(`reports/OWNER-RULINGS-20260919-EVENING.md`): *"INPUT.SNAC, GEOM.WARP,
POST.ECHO — Build all three (owner, explicit). They stay mandatory; the
2026-09-18 revocation stands."*

Every section of this file read **"Deliberately unwritten"** until that
ruling, and the reason given was right at the time: *"Specifying clocks,
packets, throughput and test plans for a block nobody is building would make
the design look decided when it is not — the same error as building it."* R7
decided it. The sections below are filled from the law that already existed
(`spec/input_rules.md` §1, §2.3 and §4) plus the PS1 bus, which is the one
thing genuinely new here and now lives in `spec/input_rules.md` §7.

## Purpose and exclusions

A PS1/SNAC peripheral adapter that produces the canonical pad state and merges
it into the pad snapshot path.

The 2026-08-31 §6.6 ruling's sentence survives R7 and is the whole shape of
this block: *"It must emit the same canonical PadFrame and may not create a
second input semantics."*

**Excluded, and each for a reason rather than for scope:**

* **the PadFrame itself** — INPUT.SNAPSHOT's, unchanged. This block emits the
  RAW pad state that enters it, on the same wires the existing route uses.
* **the sequence law, the atomic latch and the absent-pad law** —
  `input_rules.md` §2, unchanged and untouched. They apply to the merged bus
  because it is the same bus.
* **the button table** — `input_rules.md` §4, frozen. §7.3 is a mapping ONTO
  it, not a second table.
* **all policy** — no deadzone, no calibration, no remapping, no filtering.
  §1 forbids all four in hardware and this block adds none.
* **rumble** — INPUT.RUMBLE's, and this block has no path to the pad's motors.
* **anything the pad's config mode (`0x53`) can do.** Answered as an
  unimplemented mode: counted, absent, and the slot falls back.

## Clock and reset semantics

Single fabric-clock (`gpu_clk`) domain. `rst_n` is asynchronous, active low,
and its released state is the whole machine: every slot absent, every field
zero, every counter zero, the engine in `SN_IDLE` on port 0.

`DAT` and `/ACK` arrive from a cable and are asynchronous at any bus rate.
Each crosses through a **2-flop synchronizer**, and the engine reads ONLY the
synchronized copy — this is the `async_bridge: true` of the ledger row.

`frame_tick` is the broadcast `zhao_frame_tick_t` pulse, the same one
INPUT.SNAPSHOT latches on. Only `.pulse` is consumed; this block stamps no
`frame_id`.

## Input and output packet layouts

**In, from the board:** `snac_att_n_o[PORTS]`, `snac_clk_o`, `snac_cmd_o`
driven; `snac_dat_i[PORTS]`, `snac_ack_n_i[PORTS]` sampled. These are pins of
the part, the same class of edge as `pad_buttons_i` and `hps_req_*` — see
`zhao_console_core.sv`'s HOST.REGWIN composition for why that class is not a
console boundary entry.

**In, from the existing route:** `host_pad_present_i[3:0]` plus
`host_pad_{buttons,lx,ly,rx,ry}_i[0:3]` — the canonical raw pad state.

**Out:** `pad_present_o[3:0]` plus `pad_{buttons,lx,ly,rx,ry}_o[0:3]`, the
same shape, merged per §7.4. This IS the ledger's `outputs: [pad_pins]`: the
adapter's declared output is the pad bus, not a side channel somebody else has
to merge.

The wire encoding of one poll is `spec/input_rules.md` §7.2; the button and
axis mappings are §7.3.

## Backpressure rules

**None, in either direction, and deliberately.**

The pad bus has no flow control: a PS1 controller answers at the rate the host
clocks it or it does not answer at all, and the `/ACK` timeout is the only
recourse. Downstream, INPUT.SNAPSHOT samples a level at `frame_tick` and has
no ready — adding a handshake here would invent one for a consumer that does
not want it.

What replaces backpressure is the GAP COUNTER (§7.5): when the bus cannot keep
up with the display, nothing stalls and nothing is dropped silently — the
reuse is COUNTED. A rate mismatch that produced no evidence would be the
silent-drop shape this repo has a chapter about.

## Memory ownership

None. Per-slot state is six registers and a 16-bit poll sequence, ×4 slots.
No RAM, no M10K, no external storage.

## Q formats and rounding

One conversion, and it rounds nothing: `canonical_i16 = {ps1_u8, 8'h00} ^
16'h8000`. §7.3 gives it in full with its inverse. Buttons are a bit
permutation with an inversion (PS1 is active low) and have no numeric format.

## Latency (fixed or variable)

**The merge is COMBINATIONAL** — a slot's output is its decoded state or the
host route's in the same cycle, with no stage. That is what keeps the adapter
the identity function when idle rather than the identity delayed by a clock.

**The poll is VARIABLE and long**, in bus time: one poll is 5 bytes (digital)
or 9 (analog), each 8 bits × 2 half-bits at `CLK_DIV` fabric cycles, plus an
`/ACK` wait per byte and `IDLE_GAP` between polls. At the defaults
(`CLK_DIV=200`) a digital poll is ≈16,000 fabric cycles and an analog one
≈29,000.

The ledger's `latency: fixed:1` describes the MERGE, which is the path a
consumer sees. The poll's latency is the bus's and is stated here rather than
folded into a number that would then be wrong.

## Target throughput

**At least one poll per port per frame** — the ledger's `1 poll per frame`.

Measured against the law rather than asserted: two ports at the default
divider cost ≈57,600 fabric cycles for a full analog round, against a Z60
frame of 251,520 (`spec/video_rules.md` §2). The round therefore completes
about four times per frame, and Duo's 318,592-cycle frame has more margin
still.

**When it is NOT met, §7.5's counter fires** rather than the shortfall being
invisible. That is the acceptance criterion: not "it is fast enough" but "if
it stops being fast enough, something says so."

## Overflow and malformed-input behaviour

There is no queue and therefore no overflow. Malformed input has four shapes,
and each has a defined, counted, non-hanging response:

| what | response | counter |
|---|---|---|
| `/ACK` never arrives | poll abandoned, slot ABSENT | `snac_timeouts_o` |
| mode byte `0xFF` (empty bus) | slot ABSENT, **not** an error | none, on purpose |
| mode byte unimplemented (e.g. `0x53`) | slot ABSENT, host route passes through | `snac_bad_header_o` |
| byte 2 is not `0x5A` | slot ABSENT, host route passes through | `snac_bad_header_o` |

**An absent slot falls back to the incoming route; it never goes dark.** A
refusal that removed function would be the failure the completion campaign
forbids, so the fallback is asserted (`input_snac_directed` case 5) rather
than assumed.

Nothing here can hang: every wait is bounded by `ACK_TIMEOUT`.

## Counters and traces

| port | meaning |
|---|---|
| `input_snac_input_sequence_gaps_o` | `input_rules.md` §2.3's merge-path gap, §7.5 |
| `snac_polls_o` | polls that completed and decoded a present pad |
| `snac_timeouts_o` | polls abandoned waiting for `/ACK` (the empty-port signal) |
| `snac_bad_header_o` | a pad answered with a mode or ready byte not implemented |
| `snac_overrides_o` | ticks where SNAC drove a slot the host route also claimed |
| `snac_present_o` | which slots SNAC is currently driving (a level, not a count) |

All four u64 counters saturate (`spec/counters.md` §4).

**Every one is seen to FIRE on legal stimulus** in `input_snac_directed`:
timeouts in case 1, polls in case 2, bad header in case 5, the gap in case 6.
`snac_overrides_o` fires whenever both routes populate one slot, which case 2
sets up. No mutant is owed, because none of the six is unreachable — the
`wq_overflow_o` situation `CLAUDE.md` describes does not arise here.

## Scalar reference function

`zref::SnacAdapter` in `reference/include/zref/zref_input.hpp`.

It is the DECODE ONLY, deliberately: the serial engine's timing belongs to the
RTL, but every VALUE is a pure function of the nine reply bytes, and that is
the part a second implementation gets wrong. It exposes `axis`, its inverse
`axisInverse`, `buttons`, `decode`, `merge` and `replyBytes`.

`reports/PHANTOM_REFERENCES.md` listed `zref::SnacAdapter` as a declared
reference that did not exist. It exists now and that row can close.

## Directed tests

`tests/input/input_snac_directed.cpp` — 40 checks, built and run.

1. transparent when idle: the identity on all four slots, `/ACK` timeout fires
2. a digital pad reaches the canonical bus; the other three slots are untouched
   in the same cycle; the host really sent `0x01` then `0x42`
3. **2b: every one of §4's sixteen buttons, walked individually**
4. the axis law over all 256 codes: invertible, monotonic, three anchors
5. an analog pad's four axes through the RTL, and 4b sweeps eight code sets
6. an unimplemented mode: counted, absent, and FALLS BACK
7. the merge-path gap counter fires when ticks outrun the bus

**Case 2b exists because case 2 was not enough, and that is worth recording.**
The first version held three buttons and PASSED against a planted fault that
swapped `up` for `right` — both were released in that fixture, so the swap was
invisible. A sixteen-entry table checked on three entries is not a checked
table. Both faults (the button swap and a wrong axis shift) were planted again
after 2b and 4b landed, and both were caught.

The bench parameterises the block down (`CLK_DIV=4`, `ACK_TIMEOUT=64`,
`IDLE_GAP=4`) for speed. No decoded value depends on any of the three, which
is what makes the fast bench evidence about the shipping configuration; `PORTS`
stays at its production default so the round-robin under test is the real one.

## Randomized differential tests

Not built. The directed suite already sweeps the two mappings EXHAUSTIVELY —
all 16 button bits and all 256 axis codes — so a random differential over the
same two functions would add no coverage of the decode. What it would cover is
BUS TIMING interleavings (an `/ACK` that arrives late, a pad that stops
mid-poll, `/ATT` contention), and that is a bench-model question rather than a
decode question. It is recorded here as the next thing this block wants rather
than claimed as present.

## Formal properties

None. The one property worth proving — the merge is the identity when no slot
is present — is a combinational statement over 6×4 wires and is checked
directly in case 1 against a fixture whose per-slot values are distinct.

## Synthesis / resource ceiling

One serial engine time-multiplexed across `PORTS`, plus 4 slots × (1 + 32 +
64 + 16 + 16) bits of state and six 64-bit counters. No DSP, no M10K.

**UNMEASURED**: nothing in this run has been fitted, by design (the fit is at
completion). The estimate is small and it is an estimate.

A real PS1 bus polls one port at a time — `/ATT` selects exactly one — so a
per-port engine would spend silicon doing something the protocol forbids doing
in parallel. The time-multiplex is the protocol's shape, not a compromise.

## Integration capture cases

None owed. This block changes no wire format, no command and no capture
section: the PadFrame a `.zcap` CONTROLLER_SNAPSHOT records is INPUT.SNAPSHOT's
and is byte-identical whichever route filled it. That is the mechanical content
of "may not create a second input semantics."

## Notes

Composed in `zhao_console_core` between the console's incoming pad ports and
`zhao_shell_top_v2`'s `zhao_input_snapshot`. The core is the composer, so the
merge lives in THIS block and not in the composer — a composer containing a
pad mux would be inventing an input law in the one file whose discipline is
that it invents none.
