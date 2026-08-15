# Zhaozhou Performance Counters — Phase 2 (wave 2)

**Status:** ratified 2026-08-14 (plan W2.1, decision D9). Single law for the
counter catalog, counter ids, and the snapshot protocol (DEBUG.COUNTERS and
every counter-owning block). Where this file and any other text disagree,
this file wins for counters.

---

## 1. The catalog

The counter catalog lives in `design/blocks.yml` (`counter_catalog:`): the
charter §25 minimum set plus the declared platform extensions, in a FIXED
LIST ORDER. Adding a counter is a LEDGER EDIT (validator-gated), never an
RTL whim.

## 2. counter_id = catalog index (ledger rule V15)

- A counter's `counter_id` is its ZERO-BASED POSITION in the catalog list.
  Example: catalog index 0 = `frame_cycles`, 1 = `deadline_faults`,
  2 = `commands`, … (the generated order is the committed order).
- `counter_id` is u16 on every wire (the .zcap COUNTERS section's
  `counter_id` field, the RTL read-mux address, the debug command payload).
- **Totality (enforced):** every counter named by any block's `counters:`
  list maps to a catalog index (no free-floating names), and the catalog
  contains NO duplicates — duplicates would tear holes in the index space.
  The ledger validator rejects both (rule V15, `npm run ledger:check`).
- Index assignment is APPEND-ONLY: a new counter is appended to the catalog
  (taking the next free index); an existing index is never renumbered and
  never reused. A capture that cites counter ids pins their meaning via the
  ABI_INFO-committed blocks.yml of its commit.

## 3. Snapshot protocol (D9) — distributed counters, no global bus

There is NO global event bus. Each block owns its §25 counters locally as
plain registers in its own clock domain.

1. **Own:** a block increments its own counters on its own events, in its
   own domain. No interconnect.
2. **Latch:** the broadcast `frame_tick` (one-cycle pulse, emitted by
   VIDEO.FRAMECTL at each vblank, spec/video_rules.md §5) latches every
   block's counters into local SHADOW registers (same domain as the counter
   — the shadow set is stable for the whole frame). Domain crossings to the
   debug read side are per-block gray-coded toggles (SYS.CDC law).
3. **Read:** DEBUG.COUNTERS aggregates the shadow set through a small
   time-multiplexed read-mux window during VBLANK (the shadows are stable
   there by construction): one `(counter_id, u64 value)` pair per read
   beat, addressed by counter_id, ready/valid. Reading NEVER affects the
   live counters.
4. **Record:** a capture's COUNTERS section (capture_format.md §4.2) is
   `u32 count` + count × `{u16 counter_id; u16 rsv; u64 expected_value}` —
   the shadow values AT THE FRAME the section is attached to. Ordering is
   ascending counter_id.

## 4. Widths and overflow

- Counters are u64 in snapshots. Internal registers may be narrower (u32
  typical) but MUST saturate, never wrap, and a saturation is itself
  visible (the snapshot reads 0xFFFF_FFFF_FFFF_FFFF). No counter value is
  ever silently corrupted (charter §29-11: record, remain correct).
- `max_tile_list_depth` is a high-water mark, not a count: it latches the
  maximum observed value; the frame_tick snapshot latches the current
  high-water; software resets the high-water by reading (read-clear law:
  the read returns the value and re-arms the tracker from zero).

## 5. Phase-2 counter set (active in the console shell)

Only these catalog entries have live owners in wave 2 (all others exist in
the catalog with no owner yet — legal, they snapshot 0):

| Counter | Owner | Event |
|---|---|---|
| frame_cycles | VIDEO.FRAMECTL | one per displayed frame (tick count) |
| deadline_faults | CMD.SCHEDULER (+MEM.VRAM.ARBITER budget tests) | a displayed frame came from the repeat path |
| commands | CMD.DECODER | accepted command records |
| vram_bytes_by_client | MEM.VRAM.ARBITER | accepted payload bytes per client |
| hps_ddr_bytes_by_client | MEM.HPS.BRIDGE | burst payload bytes per client |
| scanout_starvation_cycles | VIDEO.SCANOUT | starved vid cycles at the serializer |
| audio_underruns | AUDIO.FIFO | underrun events (repeat law) |
| input_sequence_gaps | INPUT.SNAPSHOT | observed sequence gaps |
| rumble_frames_dropped | INPUT.RUMBLE | dropped rumble updates |

## 6. Test obligations (directed at W2.6)

- Directed: inject a known event count per block, force a frame_tick, read
  the shadow set through the read-mux, compare `zref::DebugCounters`.
- Golden: every wave-2 capture's COUNTERS section is byte-compared against
  the ZRef-composed expectation (the W2.7 demo asserts all of
  {deadline_faults, scanout_starvation_cycles, audio_underruns,
  input_sequence_gaps, rumble_frames_dropped} are ZERO for all 600 frames).
- Validator: `npm run ledger:check` green (V15 totality included).
