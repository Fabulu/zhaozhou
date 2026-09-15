# G8A CRC-serial timing recovery — measured state and next batch

Date: 2026-09-14

## Evidence status

This report distinguishes two clean, physical-top-port subsystem fits. Neither is
a shell, terrain, board, or whole-console measurement.

| row | source | ALMs | registers | RAM blocks | DSP | Fmax | setup WNS | setup TNS | gate |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| `@g8a` | `c88e2b31` | 13,478 | 21,527 | 71 | 49 | 65.96 MHz | -5.160 ns | -4388.685 ns | timing fail |
| `@g8a-crcserial` | `a03ebe5f` | 13,285 | 21,630 | 71 | 49 | 80.61 MHz | -2.406 ns | -3386.650 ns | timing fail |
| `@g8a-timing1` | `8908bc6f` | 12,867 | 21,371 | 71 | 49 | 82.33 MHz | -2.146 ns | -1484.079 ns | timing fail |

The canonical receipts are:

- `reports/synthesis/zhao_g8a_raster_texture.json`
- `reports/synthesis/zhao_g8a_raster_texture_crcserial.json`
- `reports/synthesis/zhao_g8a_raster_texture_timing1.json`

All three prove 18 physical and zero virtual top pins, exact hierarchy, one V3 owner,
zero TEXJOIN, mapped `MIGRATION_SHADOWS=0`, no mapped shadow state, required RAM
witnesses, clean source, and seed 1. All three fail only the predeclared 100 MHz rule.

## What the CRC repair proved

Serializing the binding-page CRC from ten byte steps per selector to one shared
byte step removed the former `u_binding|crc_q[8] -> cfg_*` critical family.
Measured deltas are:

- 193 fewer ALMs;
- 103 more registers;
- unchanged 49 DSPs and 71 RAM blocks;
- +14.65 MHz Fmax;
- +2.754 ns WNS and +1002.035 ns TNS.

The functional change is only loader latency: one preload plus 2,560 bytes,
exactly 2,561 clocks. G8A activity remains identical at jobs=3, fills=1,
fill beats=8, framebuffer beats=512, tiles=2, signatures=256; total harness time
moves from 5,504 to 7,808 clocks. The resolver/composed/mutant matrix, Packet-E
matrix, V3 interface oracle, and G8A pre-fit matrix are green.

## Current timing boundary

The retained `@g8a-crcserial` setup reports contain 2,000 summarized paths:
17 touch a boundary and 1,983 are internal-to-internal. The boundary therefore
does not excuse the result.

The worst ten paths all launch at lane 0
`zhao_raster_attrgrad_v2|st_r.S_GRAD_REQ` and end at
`zhao_raster_attrdiv_v2|dividend_r[88:97]`. `path_anatomy.py` measures a
12.084 ns data path through wide rounding/add/magnitude formation.

The next measured families are close enough that an attribute-only refit would
be wasteful:

1. V3 owner COMBINE selection through generation validation/reservation and
   `zhao_texture_v3rq` count/pop logic, about -2.33 ns;
2. AUX clamp/comparison into `zhao_texture_aux_div6`, about -2.29 ns;
3. material product/round/saturate/mux into completion RAM write, about -2.24 ns;
4. reciprocal normalization priority/shift paths, about -2.01 ns.

## ALM posture

The entity table reports ALUT attribution, not ALM attribution; it cannot be
summed into a saving. It does identify where ALM-oriented review belongs. Largest
self-ALUT rows are:

| entity | self ALUTs | self registers | DSP |
|---|---:|---:|---:|
| `zhao_texture_v3own` | 2,833 | 2,333 | 0 |
| `zhao_texture_island_v3_top` | 2,385 | 5,964 | 23 |
| `zhao_raster_edgewalk` | 1,288 | 550 | 2 |
| `zhao_texture_aux_div6` | 756 | 752 | 0 |
| `zhao_raster_perspuv_pairpipe_v2` | 750 | 778 | 6 |
| `zhao_texture_aux_pipe_v2` | 691 | 760 | 0 |
| `zhao_texture_material_combine_v3` | 637 | 607 | 2 |
| each `zhao_raster_attrdiv_v2` | 553–560 | 256 | 0 |
| `zhao_raster_rcp24_v4` | 558 | 428 | 3 |

This makes V3 owner restructuring the primary ALM opportunity inside the measured
subsystem, but its identity/quiet/credit laws make it the highest-risk change.
No ALUT row is promoted as an ALM saving before a new connected fit.

## One batched recovery packet

Read-only Astra xhigh consultation recommended six candidate cuts. They are
recommendations, not implementation authority, and each must survive independent
RTL review and focused tests:

1. **ATTRDIV request preparation:** use the existing wide dividend register to
   separate signed rounding from magnitude formation; avoid a second wide bank.
   Consider registering the existing row-seed product before the row divide.
2. **V3 COMBINE validation:** remove the generation-table lookup from the
   enqueue-to-request-queue feedback path, without weakening full-handle identity,
   simultaneous validation publication, lifetime fault, or reservation laws.
3. **V3 retirement heads:** replace the wide RAM-output/copy feedback path with
   bounded elastic heads only if total OUTQD=4 ownership and quiet remain exact.
4. **AUX clamp:** register clamped numerators/denominators and side index before
   `aux_div6`; preserve II=1 and include the new valid in idle.
5. **Material finish:** register the narrow finished results/control before RAM
   writeback, keeping exactly two multiplier sites and moving phase-completion
   accounting to the actual write edge.
6. **RCP normalization:** balance/register leading-zero normalization before the
   shared shifter while retaining independent accepted-minus-retired accounting.

The owner changes are not assumed safe merely because they target the largest
logic row. Simpler cuts must not hide the owner path behind another 12 ns family,
and owner cleanup must not turn wide memories back into flip-flops.

## Batch implementation progress

The first four low-coupling cuts are now implemented in the working batch:

- **Attributes:** ATTRDIV reuses its 98-bit dividend as the rounded-number
  preparation register, then forms magnitude in `D_PREP`; no second wide bank.
  ATTRGRAD captures the existing global-to-tile product at gradient return.
  Radix-2/radix-4 plus both mutants pass 4/4 with exact normal latency 100/51,
  exceptional latency 2, and busy deltas. A focused independent review found
  the reuse/sign/bias/state law clean.
- **AUX:** the complete clamp/divider bundle and side index are registered
  together. Production, random, credit, seven unreachable-state mutants, and
  the unchanged six-cycle DIV6 leaf pass 11/11. All eight production-shaped
  mutant copies carry the same seven-clock side-table transport. A focused
  review also exposed and closed a pre-existing reset leak: logical ready/issue
  are now low throughout asserted reset, and a held offer completes exactly once
  only after release. The direct suite passes 428 checks.
- **Material:** a 49-bit finished row plus context/phase/final is registered
  before RAM writeback. Production copy/read-late and all eleven arithmetic/
  identity/cadence mutants pass 13/13; phase-drop remains WB-aligned.
- **RCP:** a five-decision balanced 24-bit leading-zero tree captures exponent
  and zero under the same acceptance enable as denominator/context/token. The
  old 24-step priority loop is absent, latency and ticket transport are
  unchanged, and predecessor/successor observation parity passes. The new
  direct zref/token/hold/occupancy oracle and reversed-LZC committed mutant pass
  2/2; the healthy run retires 241/241 tokens in 977 clocks with zero arithmetic,
  exponent, or zero-class mismatches.
- **V3 owner feedback:** COMBINE ready now consumes only the resettable per-slot
  joined-validation pending bit; the generation RAM remains on the registered
  material-read path and still gates usability and the assertion witness. The
  reservation full-to-free law is expressed as exact Boolean range cases rather
  than subtracting `cmb_fire_c` through a carry chain. An exhaustive default-width
  oracle finds zero differences across all 256 reservation/fire states, including
  unsigned underflow and corrupt out-of-range states. Owner legacy/read-late,
  three top profiles, nine top mutants, and registered source controls are green;
  a final logic-only Luna review returned CLEAN with its explicit
  `unrecognized_model` route warning.

The focused local Qwen xhigh review completed on actual profile
`qwen38-quasar-dflash2-k8v4-112k` with no repository changes. It independently
selected exactly the two implemented owner cuts: pending-only validation ready
while retaining the downstream generation witness, and the Boolean reservation
law. It rejected a per-ticket permission cache as stale-sample risk and deferred
elastic retirement heads because their ALM direction is unproved and their
identity coupling is high. It also confirmed `zhao_texture_v3rq` and the
`oq_ctx` retirement RAM should remain unchanged for this fit. Its additional
owner/queue/legacy checklist passes 13/13; obsolete pre-Packet-B
`island_v3_fault_directed.cpp` remains intentionally unregistered and is not
misrepresented as current execution evidence.

## Current pre-fit regression

A fresh short-path native build compiled every timing-affected executable from
current sources. It passes the complete Packet-B boundary **74/74**, combined
Packet-C/D/E/F boundary **47/47** (4/4, 13/13, 26/26, 4/4), focused timing/V3
matrix **42/42**, interface oracle **113/113**, production accounting **51/51**,
and unaffected Packet G **22/22**. The Qwen-requested owner/queue/legacy
compatibility selection is separately **13/13**. The first focused run's sole
`Not Run` was a
missing clean-build DIV6 executable; it was built and the entire 42-test selection
was rerun green rather than promoting the partial run.

The next-fit evidence lane was also hardened before use. `@g8a-timing1` requires
local/remote branch equality, exact immutable post-CRC baseline bytes, a changed
clean HEAD, no prior row/receipt/fit manifest, and exact seed-1 physical mode. It
snapshots the generated 43-source manifest in memory before Quartus, verifies it
after the fit, atomically retains those exact bytes, and derives the receipt only
from that retained manifest. The QSF parser now requires one absolute snapshot
`src` parent and exact active DEVICE/TOP/SDC/SEED assignments, so comment-spoofed,
duplicate, or alternate-parent controls reject. The generic runner already copies
and hashes the actual snapshot before map, after map, and after the complete run;
live-tree edits cannot reach Quartus. Both receipt tools are independently hashed
before/after the fit, then opened under Windows read-sharing locks and rehashed
under lock through Python import; a focused Luna re-review of that final mutation
window returned CLEAN (with the route's explicit `unrecognized_model` warning).

The first fresh-clone launch from `00bf552d` correctly stopped before Quartus:
11 G8A sources had acquired `eol=lf` attributes after their existing working
copies were already CRLF, so the generated manifest had hashed clean-filtered
live bytes that did not match the committed LF blobs. The generator now requires
strict UTF-8 with no CR byte for every generator/template/source input, with
executed CRLF/bare-CR/invalid-UTF8 controls. The 11 local working copies were
restored from their exact HEAD blobs, the wrapper/manifest were regenerated, and
a future clone must reproduce `--check` before any Quartus process can launch.
The failed `00bf552d` attempt contains no map, resource, or timing evidence.

A replacement clone at `0c65c43c` passed generator freshness, then stopped before
Quartus when the static archive gate exposed the same historical checkout trap in
seven attempt-1 raw artifacts. Their metadata correctly names the original CRLF
bytes, but Git had normalized their blobs before the later directory-wide binary
attribute was committed. The seven authoritative working-copy bytes match every
existing `ATTEMPT.json` digest and are now restaged under the active binary rule,
so future clones retain those exact bytes. No generated manifest, source, Quartus
stage, resource, or timing claim came from either failed preflight.

## `@g8a-timing1` result and next batch

The predeclared fit completed from clean exact source `8908bc6f`, source digest
`3f739c36...ca707`, seed 1, 18 physical/zero virtual pins. Structure, RAM, and
resource gates pass; timing remains red:

- **12,867 ALMs**, down 418 from post-CRC and 611 from the original baseline;
- **21,371 registers**, down 259 from post-CRC;
- unchanged **92,964 memory bits / 71 RAM blocks / 49 DSPs**;
- **82.33 MHz**, setup WNS **-2.146 ns**, TNS **-1484.079 ns**;
- hold **+0.262 ns / 0 TNS**.

The first batch therefore improved Fmax only 1.72 MHz, but cut 418 ALMs and more
than halved remaining TNS. The former ATTRGRAD/ATTRDIV, AUXDIV, material finish,
RCP priority-normalization, and owner COMBINE-generation/subtract families are
absent from the worst band, so those cuts did what they were intended to do.
They did not close Packet F.

The new overall worst path is characterization-only:
`signature_misr_q[24] -> fit_signature_o[0]` through the four-byte XOR at
-2.146 ns. Registering the eight signature outputs removes that physical-output
path without changing activity or the subsystem.

The real internal gate is now the Qwen-deferred retirement trigger. Worst
internal setup is -1.593 ns / 86.26 MHz from the `oq_ctx_q` M10K output family
through texture-stage sequence/fault/ready logic; path anatomy measures 9.031 ns
of data path, and **1,245 of 1,939** summarized internal paths launch from that
family. The measured trigger Qwen specified has therefore fired. The next batch
may add a bounded registered/elastic retirement head while preserving total
`OUTQD=4`, full owner/context/result identity, output hold, ordered release,
quiet, reset, RAM inference, and exact credit. It must not copy the 4x224-bit RAM
payload into an additional wide queue or change the owner lifecycle.

Before another fit, require back-to-back retirement, held-head refill, reset with
all retirement ownership positions, wrap/output-hold, exact admission/emission
order/context, new head-bit positive controls, complete Packet-B/C/D/E/F/G
regressions, and fresh interface/G8A manifests. Only one next subsystem fit may
measure the registered boundary plus retirement-head batch. Its unchanged gate
is Fmax >=100 MHz, setup WNS >=0, setup TNS=0, passing hold, DSP <=49, retained
RAM/hierarchy/shadow evidence, and explicit ALM/register deltas. Packet H may
continue as excluded development but cannot be promoted while Packet F remains
red.
