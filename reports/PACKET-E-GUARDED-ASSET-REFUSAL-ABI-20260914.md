# Packet E guarded asset/refusal ABI — 2026-09-14

**Status:** implemented and simulation-verified at excluded subsystem boundaries; not fitted, selected, shell-connected, or physical evidence.

Packet E closes the denial deadlock reserved by Packet B and builds the excluded ENGINE1 local mux needed by later shell-v2 composition. It does not spend global client 5, alter the protected shell, or start G8A.

## 1. Typed cache fill termination

`zhao_texture_cache_pipe_v2` retains its request/fill/data channels and adds a held eight-bit status beside each cache response, `frame_fault_clear_i`, clearable `fill_protocol_fault_o`, and reset-zero counters for cache accepted/completed, fill accepted/completed/refused, and accepted fill-data beats.

Status zero is the existing success path. A denied fill produces exactly `SOURCE_REFUSED=8'h01`, zero sample data, and the original accepted 18-bit route token.

### Identity and reservation

A miss captures `{route_token,tag,index,lane_mask}`. The C2 probe's response reservation remains owned by the blocking fill; only a younger squashed C1 probe is refunded. The fill reservation is never silently recreated:

* success transfers it to the first replay of the same queue head, which issues without reserving a second slot;
* refusal transfers it directly into one response-FIFO entry and skips only the denied request head.

The replay path carries an explicit prepaid-reservation bit through C1/C2. Successful prepaid replay does not increment `rs_resv`; hit retirement and refusal do not decrement the already-owned reservation.

```text
rs_resv == queued responses
         + ordinary C1 reservations
         + ordinary C2 reservations
         + blocking-fill reservation
         + prepaid reservation in exactly one of replay / C1 / C2
```

Every term has a different enable. Refusal cannot wait for an unreserved output slot, and success cannot double-reserve.

### Fill protocol

RAM writes use only `fill_data_accept`, never raw `fill_data_valid_i`. Data/refusal is legal only after `FI = fill_valid_o && fill_ready_i`.

* eight accepted data beats publish tag/valid, complete the fill, and replay the head;
* refusal wins over simultaneous data, invalidates partial content, emits one refused response, and completes the fill;
* data before FI, refusal before FI/no miss, simultaneous data/refusal, partial refusal, ninth/unsolicited data, and duplicate refusal independently set the clearable protocol fault;
* partial refusal still terminates once; malformed data never writes/counts;
* malformed set wins over same-edge clear.

The fault clears only on a quiet frame clear. Payload, valid data, work, reservations, and counters do not clear. Idle includes all queue, pipeline, fill, and prepaid state, not the fault level itself.

Counters are modulo 32-bit:

```text
CA    cache request accepted
CC    held cache response accepted downstream
FI    external fill request accepted
FTERM successful or refused fill terminal
FREF  refused terminal (subset of FTERM)
FB    accepted legal fill-data halfword
FOK   = FTERM - FREF
```

After legal drain: `CA=CC`, `FI=FTERM=FOK+FREF`, and `FB=8*FOK`. Existing `fills_o` retains its historical fill-allocation meaning and is not relabelled FI.

## 2. V3 top atomic migration

The production elaboration removes `unsupported_fill_refusal_pre_e` and every pre-E simulation-overlay lifetime operand from `zhao_texture_island_v3_top`. The committed historical mutant branch alone retains a private `sim_pre_e_fill_refusal_lifetime_q` positive control. `fill_refused_i` remains a physical quiet operand but is no longer reset-lifetime production state.

The cache's held `{data,status,token}` boundary is checked together. Sample index 3 retains reset-lifetime priority. Status zero follows the existing metadata/class path unchanged. Nonzero cache status launches no metadata, palette, direct-decode, or bilerp work and forms exactly:

```text
{original token, status=SOURCE_REFUSED, raw_index=0, alpha=255, RGB=FF00FF}
```

One fair held 2:1 merge per physical class combines its native terminal with refusals whose original token names that class. Only the selected producer sees ready; round-robin changes only on a contended acceptance. A CLUT/NEAR/BIL refusal remains in that physical class and is never relabelled ERR. The unchanged dispatcher/owner then commits the original sample handle/status and retires the fragment normally.

Cache protocol fault joins the clearable child-fault set. The public V3 port list, output meanings, quiet operand count, and explicit production `MIGRATION_SHADOWS=0` remain unchanged. The exact 26-source interface artifact is regenerated because top/cache hashes change; the generated production top must remain byte-identical. Packet-B fit receipts become stale and are not reused.

A retained historical test-only pre-E latch control proves the superseded lifetime behavior without restoring that state in production elaboration.

## 3. Excluded ENGINE1 local mux

`zhao_render_asset_mux` is immediately `excluded:not-yet-adopted` and is not placed in the protected shell. It arbitrates the already-muxed geometry guard request against one texture line fill.

### Ports

* geometry: typed guard request/response and 64-bit beat/last output;
* texture: valid/ready 32-bit fill address and 16-bit data/refusal output;
* sole downstream typed guard request/response;
* raw ENGINE1 return `{valid,data16,last}`;
* frame clear, quiet, protocol fault, and typed accept/verdict/refusal/contention/raw/protocol counters.

`raw_last` is mandatory for independently detecting short fill, surplus fill, and completion without a start. The protected shell/controller do not expose it today, so Packet E makes no shell-integration claim. Later shell-v2 must derive it from an independently tracked request-retirement fact; silence cannot mean completion.

### Request/FSM law

Texture translates to read-only ENGINE1, 16-byte aligned, length 16, low sixteen byte enables set. Geometry preserves legal 32/64-byte shape while forcing read-only ENGINE1. A malformed local request is presented as existing client NONE so the real guard denies it; no local owner becomes a global client ID.

The FSM is:

```text
M_IDLE: capture one selected whole local request
M_OFFER: hold selected request/owner/fields until guard_accept; only winner ready
M_VERDICT: guard valid low; accept exactly one later ok or violation
M_DATA: retain owner/length; route exact halfwords; release only on raw logical last
M_DRAIN_BAD: discard malformed texture surplus until raw_last
M_STRUCTURAL: reset-only geometry barrier; no quiet or invented terminal
```

Round-robin advances on guard acceptance, including a request that is later denied. On denial geometry receives its typed violation or texture receives one refusal. Both verdict bits produce one denial disposition plus protocol fault. Texture data passes raw 16-bit words directly; geometry packs four little-endian halfwords per 64-bit beat and marks last after four beats for 32 bytes or eight for 64.

Protocol detectors cover verdict without acceptance, double verdict, second accept while waiting, stalled-source change/disappearance, raw data before OK/after denial/no owner, early or missing last, ninth/surplus texture data, wrong geometry pack length, and routing against captured owner. Short texture return suppresses its early-last data and emits one refusal. Malformed approved geometry return is structural fault, never an invented empty beat.

## 4. Canonical region and guard

`zhao_pkg.sv` adds canonical `ZHAO_RENDER_ASSET_BASE=0x06A00000` and `SPAN=0x01600000`; geometry names remain aliases. MEM.GUARD uses only canonical names and preserves exactly the same half-open read-only ENGINE1 window, 1..64-byte shape, default deny, and unspent client 5.

Contracts/spec/reference gain canonical aliases and correct the stale request law: acceptance is `req.valid && rsp.ready`; the master drops valid immediately; one registered OK or violation follows. Ready during a denial verdict is not another permission.

Formal retains all existing region/map theorems and adds explicit render-asset ownership, non-vacuous aligned ENGINE1 16/32/64-byte covers, no-forward-client-5, and accepted-client-5 denial cover. The arbiter, geometry adapter, SDR controller, protected shell, Packet D, and client enum remain byte-identical.

## 5. Verification and packet boundary

Required controls include cache token/status hold, success/refusal reservation conservation, head skip and younger replay, partial invalidation, all malformed fill cases, four route classes and native/refusal contention, mux round-robin and 16/32/64 traffic, exact guard accept-versus-verdict, raw packing/last faults, MEM.GUARD boundaries/clients/client5, and committed selector mutants for replayed denial, owner drift, texture-through-64 packing, beat7/beat9, and denial silence.

Packet E updates cache/top tests, interface artifact, guard/reference/formal/contracts, CMake, excluded accounting, and static closure controls atomically. It adds no fit target or production source, no shell connection, no client5, no Packet-H lease logic, and no G8A claim.