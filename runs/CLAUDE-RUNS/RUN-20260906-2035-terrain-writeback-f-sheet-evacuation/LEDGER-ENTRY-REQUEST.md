# Requested ledger entry — TERRAIN.WRITEBACK

`design/blocks.yml` is owned by another lane this session, so the entry is
requested here rather than written. Insert after `TERRAIN.PAGELOADER`.

```yaml
  - id: TERRAIN.WRITEBACK
    name: Dirty-page evacuation (F sheet to the journal)
    kind: rtl
    subsystem: terrain
    clock_domain: gpu
    purpose: "Copies exactly the 8,192-byte layer-F surface sheet out of one TERRAIN.PAGE_POOL slot into the HPS terrain journal on a dirty_F eviction, and holds the slot hostage until the journal acknowledges. Ruling T4: layers B and D are NEVER written back (the HPS keeps the canonical mirror current from the same deterministic commands); layer F has no canonical mirror and must go back behind an ACK barrier. Every wb_* it presents to TERRAIN.RESIDENCY is caused by a matched acknowledgement -- never a timeout, a byte count or a last bridge beat -- because a fabricated barrier release silently heals terrain a player broke. Closes step 6 of the world-layer build sequence and reports/Missingterrain's 'dirty-page writeback'."
    contract: design/contracts/TERRAIN.WRITEBACK.md
    phase: 6
    owner_issue: ZH-126
    inputs: [sheet_writeback_jobs, journal_acks]
    outputs: [writeback_acks, writeback_completions]
    upstream: [TERRAIN.RESIDENCY]
    downstream: [MEM.GUARD, MEM.HPS.BRIDGE, TERRAIN.RESIDENCY]
    backpressure: ready_valid
    latency: variable
    target_throughput: "one 8,192-byte sheet per transfer, up to ACK_SLOTS (4) awaiting acknowledgement; 38.3 % of a page's bytes"
    reference_model: zref::terrain::sheet_writeback
    tests:
      directed: tests/terrain/writeback_rtl_directed.cpp
      random: tests/terrain/writeback_rtl_directed.cpp
    counters: [writeback_sheets_written, writeback_sheets_refused, writeback_sheets_faulted, writeback_hdr_ident_fails, writeback_guard_denied, writeback_bridge_errs, writeback_acks_ok, writeback_acks_nak, writeback_acks_unmatched, writeback_acks_after_epoch, writeback_acks_overdue, writeback_seq_conflicts, writeback_wb_bytes, writeback_outstanding_hwm, writeback_ack_wait_max_cycles, writeback_jobs_stall_cycles]
    source_ids: true
    budget_group: geometry_mantle
    maturity: UNIT_VERIFIED
    maturity_log:
      - state: SPECIFIED
        date: "2026-09-06"
        commit: "TBD"
        evidence: design/contracts/TERRAIN.WRITEBACK.md
      - state: UNIT_VERIFIED
        date: "2026-09-06"
        commit: "TBD"
        evidence: tests/terrain/writeback_rtl_directed.cpp
    deferred: false
    cut_order: null
    superseded_by: null
    notes: >-
      Its job source will be TERRAIN.SEQ (step 5 of the world-layer build
      sequence), which does not exist yet, so that edge is absent from
      `upstream` rather than asserted against a block nobody has written.
      TERRAIN.RESIDENCY appears on BOTH sides deliberately: the claim's dirty
      victim is what starts a job, and the `wb_*` barrier release is what ends
      one.

      NOT YET INTEGRABLE, and for one precise reason. MEM.GUARD gives
      ZHAO_CLIENT_TERRAIN_BUILD a WRITE-ONLY window over TERRAIN.PAGE_POOL and
      this block READS that pool -- the guard's own comment names this block as
      the reason the read arm was withheld ("when the block that does it exists,
      it brings its own arm and its own proof"). The arm is one direction bit on
      the arm already there: a separate `terrain_rd_ok` over the same constant
      window for the same single client, so the two directions stay two
      theorems. `a1_terrain_owner` is unchanged; `a1_terrain_wo` becomes a
      direction statement; a read cannot alter a frame buffer, which is the same
      safety argument that carried GEOM.ASSET_POOL. It is NOT made here: the
      contract sets it out with the four narrower forms that were considered and
      why each is impossible (an address-to-slot decode by 21,376 on the verdict
      path) or forbidden (T3: "Client ID 5 ... do not spend it pre-emptively").
      The bench measures the refusal: the real zhao_mem_guard, watching, refuses
      all 130 requests and passes none, and those two counts INVERT when the arm
      lands -- which is the amendment's acceptance test.

      MEM.HPS.BRIDGE needs no RTL: it already carries writes
      (`req.write` + `wr_valid/wr_data/wr_last`). It needs one line of contract
      -- the F-sheet journal arena added to its granted-writes list, beside the
      FRAME_RING state word, the PCM read pointer and the trace extents.

      Two defects found in neighbouring blocks while building this one, reported
      rather than fixed: `zhao_hps_bridge.hps_bytes` is `[4:0][31:0]` and is
      indexed by a 3-bit client id, so client 6's bytes are silently unaccounted
      in `hps_ddr_bytes_by_client` for THIS block and for TERRAIN.PAGELOADER;
      and the bridge's write channel has no `wr_ready`, so a client that streams
      on its grant pulse loses beats before the bridge's internal `issued` --
      this block takes the acceptance level as a sideband `hps_wready_i` and
      asks for the bridge to expose the level it already computes.
```
