# Contract — TEXTURE.AUX.V2 (Owner-sealed restricted auxiliary source)

> Packet B successor to `TEXTURE.AUX`. Normative for
> `zhao_texture_aux_pipe_v2`; the unversioned contract and RTL remain an
> oracle-only compatibility boundary for the unchanged old island.

## Purpose and authority

AUX V2 maps one owner-sealed terrain world position into one resident Surface
Sheet READ and returns one typed owner result. It is a restricted byte-pair
source, not a second TMU: it has no format, mip, wrap, palette, filtering, or
material-combine mode.

This contract owns the V2 adapter, typed owner plane, credit, issue/return,
malformed-response disposition, accounting, and quiet laws. It normatively
imports only the Sheet store transaction from `SURFACE.SHEET.md`: READ opcode,
handle lookup, texel address, source echo, HIT/MISS meanings, and ready/valid
hold. `SURFACE.SHEET.md` continues to own the store and returned bytes.

The older `TEXTURE.AUX.md` and `zhao_texture_aux_pipe.sv` are not Packet-B
product authority. In particular, their narrow request/response seam and any
old-island AUX-as-sample-2 behavior cannot outvote this contract.

## Clock and reset

Single `gpu` clock. Reset is asynchronous active-low assert and synchronous
release. Reset clears every valid, occupancy, reserved credit, owed-response
bit, sticky fault, and counter. While reset is asserted, `job_ready_o` and
`issue_valid_o` are both low: a held input offer cannot create a phantom logical
issue before the sequential reset branch can accept it. Sheet RAM is not owned
or reset here.

`frame_fault_clear_i` clears only `frame_fault_o`; it does not clear counters,
credits, valid bits, payloads, or protocol state. Any fault on the same edge has
set priority. The island may pulse it only on its accepted full-quiet frame-clear
handshake.

## RTL interface

`zhao_texture_aux_pipe_v2` has `OWNERW=14` and a finite `CREDIT` parameter
(default 16). Its channels and observations are exact:

```text
logical job:
  job_valid_i / job_ready_o
  job_wx_i, job_wz_i
  job_env_x0_i, job_env_x1_i, job_env_z0_i, job_env_z1_i
  job_sheet_handle_i[31:0], job_owner_i[OWNERW-1:0]
  job_force_refuse_i
  frame_fault_clear_i

owner issue instrument:
  issue_valid_o, issue_owner_o[OWNERW-1:0]

Sheet READ request:
  req_valid_o / req_ready_i
  req_op_o[1:0], req_handle_o[31:0], req_texel_o[11:0], req_src_id_o[15:0]

Sheet response:
  pg_valid_i / pg_ready_o
  pg_op_i[1:0], pg_status_i[1:0], pg_tag_i[7:0]
  pg_strength_i[7:0], pg_src_id_i[15:0]

owner return:
  out_valid_o / out_ready_i
  out_owner_o[OWNERW-1:0], out_result_o[47:0]

observations:
  refuse_valid_o, sheet_rsp_owed_o, idle_o, frame_fault_o
  credit_in_use_o[$clog2(CREDIT+1)-1:0]
```

The exact 32-bit modulo counters are `accepted_o`, `sheet_reads_o`,
`local_refused_o`, `completed_o`, `degenerate_o`, `sheet_hits_o`,
`sheet_misses_o`, `sheet_rsp_wrong_op_o`, `sheet_rsp_wrong_status_o`,
`sheet_rsp_wrong_src_o`, `sheet_rsp_unsolicited_o`, and `credit_fault_o`.

## Owner-sealed request

One logical AUX job carries:

```text
owner_handle14 = {slot[5:0], generation[7:0]}
sheet_handle32 = {patch_index[23:0], sheet_generation[7:0]}
envelope       = {env_x0, env_x1, env_z0, env_z1}
point          = {wx, wz}
```

All six coordinate words are signed Q16.16. The context is the exact packed
`zhao_aux_surface_ctx_v2_t` from `zhao_render_texture_pkg.sv`:

| bits | field |
|---|---|
| `[31:0]` | `wx` |
| `[63:32]` | `wz` |
| `[95:64]` | `sheet_handle` |
| `[127:96]` | `env_x0` |
| `[159:128]` | `env_x1` |
| `[191:160]` | `env_z0` |
| `[223:192]` | `env_z1` |

The complete record is captured under the texture owner's admission identity
and reaches AUX only through that identity. No current terrain pin, current
active sheet, late envelope register, rolling pointer, or raw numeric slice may
replace a field. New RTL accesses the package struct fields or named offsets.

The low context word is world X and the next is world Z. The V2 instantiation
therefore connects `wx` to the X request and `wz` to the Z request; transposing
them is a contract failure.

## World-to-sheet mapping

For each axis, with signed 32-bit `w`, `e0`, and `e1`:

```text
if e1 <= e0: degenerate
otherwise:   q = trunc_toward_zero(((s64(w) - s64(e0)) * 64) /
                                   (u64(e1) - u64(e0)))
             texel = clamp(q, 0, 63)
```

The complete clamped `{ru,du,rv,dv,side_index}` divider-input bundle is
registered as one identity before the unchanged six-stage divider. This adds one
caller clock without changing divider latency or II=1. The side-table entry is
written on that clamp edge and read seven clocks later; no numerator,
denominator, clamp result, or side index may be retimed alone.

The multiply precedes the divide and the intermediate is widened so every
32-bit input is defined without overflow. There is no half-texel bias and no
rounding-to-nearest. U comes from X, V comes from Z, and the Sheet texel is
`{v[5:0],u[5:0]}`.

A degenerate envelope emits no Sheet request. It is still one accepted logical
AUX issue and receives one reserved local terminal refusal, no earlier than the
cycle after issue.

## Surface Sheet master

Every nondegenerate job offers exactly this request:

```text
req_op[1:0]      = 2'd1                 // READ
req_handle[31:0] = sealed sheet_handle32
req_texel[11:0]  = {sheet_v[5:0], sheet_u[5:0]}
req_src_id[15:0] = {2'b00, owner_handle14}
```

Request valid and the complete payload hold while valid and not ready. An
accepted request appends its immutable owner identity to the in-order issued
identity FIFO. A request may be offered only after capacity has been reserved
for both that FIFO entry and its eventual terminal return.

The complete Sheet response channel is
`{pg_op[1:0],pg_status[1:0],pg_tag[7:0],pg_strength[7:0],pg_src_id[15:0]}` with
ready/valid. The response is consumed only on its handshake.

`sheet_rsp_owed_o` is mandatory, resets low, and is exactly
`issued_identity_fifo_occupancy != 0`. It rises when an accepted READ creates
an entry and remains high while any accepted READ lacks its one consumed
terminal response. Consuming the final Sheet response may clear it even while
the resulting AUX return is held; `idle_o` must remain low until that return is
accepted.

## Sheet response validation and terminal disposition

A response for the FIFO head is successful only when all are true:

```text
pg_op     == 2'd1
pg_src_id == {2'b00, head.owner_handle14}
pg_status == HIT
```

Disposition is exact:

* HIT produces `{status=0, tag=pg_tag, strength=pg_strength}`.
* MISS is the legal negative READ response. It produces
  `{status=SOURCE_REFUSED, tag=0, strength=0}` and raises the sticky frame
  fault.
* ALLOCATED or OVERFLOW on a READ, a wrong opcode, or a wrong source is a Sheet
  protocol fault. It consumes the owed FIFO head and produces exactly one
  `SOURCE_REFUSED` terminal result for that head, never success for the token
  claimed by the malformed response.
* A response with no issued FIFO head, including a duplicate after the head was
  consumed, enters an always-draining protocol-fault sink. It increments
  `sheet_rsp_unsolicited`, invents no owner, and commits no completion.

Malformed traffic cannot leave an accepted owner parked. A later response
cannot complete a head already consumed by an earlier malformed disposition.
A stale or missing sealed handle is decided by the Sheet store and returns
MISS; comparing only the 24-bit patch index is forbidden.

## Typed owner issue and return

`aux_required` is an independent required-source bit. Acceptance of the
logical AUX job and notification of the texture owner are the same event:
`issue_valid_o == job_valid_i && job_ready_o`, and `issue_owner_o` is the
accepted `job_owner_i`. No terminal AUX return may be presented on that edge;
the earliest local or Sheet terminal return is cycle N+1 after a cycle-N
logical issue.

The returned owner plane is exactly 48 bits:

| bits | value |
|---|---|
| `[47:40]` | status; bit 0 is `SOURCE_REFUSED`, bits 7:1 zero |
| `[39:32]` | Sheet tag on HIT, otherwise zero |
| `[31:24]` | Sheet strength on HIT, otherwise zero |
| `[23:0]` | zero |

The result carries the exact owner handle of the accepted logical job. It holds
unchanged while return valid and owner ready is low. The owner commits the AUX
plane only after the matching logical issue and full-identity return.
Wrong-generation, unrequested, pre-issue, stale, or duplicate owner returns
remain owner protocol faults: they do not satisfy the required bit or count a
completion.

A malformed material descriptor sets `job_force_refuse_i` on the logical AUX
job. That job is accepted and issued under its owner but bypasses Sheet access,
then receives the reserved local refusal no earlier than the next cycle. A
degenerate envelope follows the same local-disposition path. Each is a local
refusal, sets `frame_fault_o`, and increments its exact counter where one exists.
A Sheet MISS is not a local refusal: it creates one Sheet acceptance and one
refused completion.

`frame_fault_o` is set by every accepted forced or degenerate local refusal,
Sheet MISS, wrong Sheet opcode, illegal READ status, wrong echoed source,
unsolicited/duplicate Sheet response, issued/reserved-credit fault, and any
nonzero terminal AUX status. It remains set until reset or
`frame_fault_clear_i`; clear never hides a same-edge fault. HIT with status zero
is the only nonfaulting terminal disposition.

## Credit and backpressure

AUX-job ready may be high only when one credit reserves the complete future
path: arithmetic state, held Sheet offer, issued-identity FIFO entry,
response/terminal reservation, local-refusal entry, and held owner return.
The credit is released only when the owner accepts the terminal return.

Every packet holds field-for-field under backpressure. No valid is a function
of its own ready. Independent stalls on logical input, Sheet request, Sheet
response, and owner return may change latency but cannot change identity,
coordinates, handle, texel, status, tag, strength, count, or order.

`credit_fault_o` increments once for each cycle in which any reserved-capacity
invariant is violated: total credit, offer FIFO, issued-identity FIFO, or return
FIFO exceeds `CREDIT`; a fixed-latency divider result has no reserved offer
room; an owed Sheet response has no reserved return room; or a local return is
created without a live lifetime credit. Any such event sets `frame_fault_o`.
The counter is not cleared by `frame_fault_clear_i`.

Those states are unreachable under legal production guards. Each independent
predicate therefore requires a renamed committed mutant whose inverse-polarity
driver disables a pre-empting simulation assertion in the mutant only and
requires `credit_fault_o` to move. An assertion abort by itself is corroboration,
not evidence that the synthesizable shipped detector can fire.

## Accounting

All counters are 32-bit reset-zero modulo counters and increment on their named
handshake or terminal event, never on valid alone. The island-level names map to
ports as follows:

```text
AJ_required       sum of admitted aux_required bits (island admission counter)
AJ_accepted       accepted_o: logical AUX job handshakes
AJ_sheet_accept   sheet_reads_o: Surface Sheet READ request handshakes
AJ_local_refused  local_refused_o: jobs terminated before a Sheet request
AJ_completed      completed_o: typed returns accepted by the owner
OWN_AUX_COMMIT    the owner's independent AUX-commit instrument
```

`degenerate_o`, `sheet_hits_o`, `sheet_misses_o`,
`sheet_rsp_wrong_op_o`, `sheet_rsp_wrong_status_o`,
`sheet_rsp_wrong_src_o`, and `sheet_rsp_unsolicited_o` count their exact named
dispositions. `refuse_valid_o` is the currently presented typed refusal, not a
counter.

After complete drain and within a window shorter than counter wrap:

```text
AJ_required  == AJ_accepted
AJ_accepted  == AJ_sheet_accept + AJ_local_refused
AJ_completed == AJ_accepted
AJ_completed == OWN_AUX_COMMIT
```

Unsolicited responses and malformed owner returns increment only their typed
protocol-fault counters. They do not increment `AJ_completed`. A local refusal
counts once as accepted and once as completed, not as a phantom Sheet request.
A cycle which also commits a TMU return increments the independent TMU and AUX
owner counters once each and the retained combined owner counter by two.

## Structural quiet

`idle_o` is high if and only if all state owned by the instance is empty:

* no A0 arithmetic, registered clamp bundle, or divider work;
* no held logical job or Sheet request offer;
* no issued-identity FIFO entry and `sheet_rsp_owed_o == 0`;
* no reserved credit, response disposition, or local refusal;
* no typed owner return held;
* no accepted job awaiting any of those states.

The island maps its `q_aux_idle` alias only from this port and maps
`q_sheet_rsp_owed` only from `sheet_rsp_owed_o`. It separately includes the
logical AUX-job valid, Sheet request valid, external Sheet response valid,
`refuse_valid_o`, and AUX owner-return valid in the literal structural-quiet
equation. Hierarchical reads of private FIFO pointers or occupancy are
forbidden.

## No AUX-as-sample-2 law

AUX never occupies, supplies, aliases, or substitutes for TMU sample 2. No
material recipe 0 through 7 consumes AUX tag or strength as an RGB or alpha
sample operand. The required-source mask remains
`{aux_required,sample2_required,sample1_required,sample0_required}`. AUX status
participates in final status only when AUX is required; tag and strength are
reserved for a later visible terrain-effect composition and Packet B does not
claim that effect is connected.

At sample count zero, AUX may still be independently required. The owner cannot
combine until that required AUX terminal result commits. With AUX absent,
PASSTHRU count zero returns the admitted base color under the material contract.

## Scalar reference function

`zref::aux::AuxSource` in `reference/include/zref/zref_aux.hpp` is the scalar
world-to-texel and Sheet byte-pair authority. It is a view onto the executed
`zref::render::sample_sheet` mapping, not a second arithmetic law. The V2 random
differential must compare request U/V and HIT/MISS data against it while an
independent scoreboard owns owner/Sheet ready-valid timing, typed status,
credits, and malformed-response disposition.

The unchanged old-island tests remain evidence for their old RTL only. They do
not establish V2 issue identity, owed responses, local refusal, or frame-fault
semantics.

## Required verification

The Packet-B gate must demonstrate, with independent scoreboards and every
counter checked:

1. hand-computed mapping anchors, both clamp rails, constructed boundaries, the
   widest signed domain, and every degenerate-envelope form;
2. interleaved owners naming sheet A/envelope A, sheet B/envelope B, then A,
   with independent stalls on every channel;
3. full-handle generation: stale generation and same-index/new-generation both
   refuse;
4. HIT, MISS, wrong opcode, ALLOCATED, OVERFLOW, wrong source, duplicate, and
   response-without-request dispositions;
5. issue at N and earliest local refusal at N+1;
6. Sheet request, response, and owner-return payload hold;
7. exact `sheet_rsp_owed_o` rise/hold/fall and `idle_o` remaining low through a
   held terminal return;
8. frame clear affects only the sticky summary, idle clear works, and a
   simultaneous unsolicited response/fault wins over clear;
9. post-drain AJ/owner equalities, including simultaneous valid TMU and AUX
   commits and separate sample-only/AUX-only controls;
10. no AUX field influencing sample-2 material arithmetic.

Every detector must have a positive control. The committed slot-swap mutant
must pair descriptor data from the currently offered owner with the prior owner
token and be caught by an independently captured admission record. Separate
committed controls must make wrong-op, wrong-status, wrong-source, unsolicited,
and no-AUX-as-sample-2 detectors fire. Any structurally unreachable fault needs
a renamed committed mutant rather than a temporary edit to production RTL.

## Evidence boundary

Packet B is a functional, contract, and structural-observability packet. It is
not a connected raster/texture fit and banks no ALM, DSP, M10K, throughput, or
Fmax saving. Physical claims wait for the pre-named G8A subsystem fit after
Packets A through E pass their simulation and mutation gates.
