// zhao_terrain_fieldlist.sv -- TERRAIN.FIELDLIST: the SEALED per-FRAME
// TerrainField association list, and its per-PATCH replay.
//
// NEW 2026-09-22 (packet FIELDARM), against `zhao_console_core` entry I34 and
// owner directive `reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt`
// sections 13.2 (responsibilities 1 and 3 -- "the bounded 16-entry field intake
// in command order" and "field-to-association mapping for its accepted list"),
// 13.6 ("seal its accepted command-ordered association list") and 20.8
// ("Commit G -- Earth production path and one reducer").
//
// ===========================================================================
// WHY THIS BLOCK EXISTS, AND IT IS NOT THE REASON THE ENTRY GIVES
// ===========================================================================
// Entry I34 and packet FIELDLANE both record that the tempting composition is
// ONE WIRE -- join `zhao_cmd_exec`'s `tfld_*` arm to `zhao_terrain_patch`'s
// section 9.1 `fld_add_*` intake inside the console -- and both refuse it by
// CITATION: directive 13.2 moves the intake to a not-yet-built
// `zhao_terrain_patch_v2`, and 20.8 forbids closing I34 by wiring only height.
// Those citations are correct and they are not the first reason.
//
// THE FIRST REASON IS A CADENCE MISMATCH AND IT IS MEASURABLE IN THIS TREE.
// It was on nobody's list.
//
//   * `zhao_cmd_exec` emits TerrainField 0x0200 records ONCE PER COMMAND
//     PACKET. `tfld_valid_o` is `(tq_rp != tq_cp)`: the staged records become
//     visible at packet COMMIT and drain exactly once, contiguously, and are
//     never re-offered.
//   * `zhao_terrain_patch`'s `list_clear_i` is driven in the console by
//     `tce_job_take` = `tps_j_valid && tps_j_ready` -- ONE PULSE PER PATCH
//     JOB -- and it sets `n_fields <= 0`. The block's own contract says so:
//     "Asserted once per patch per frame, BEFORE the first record."
//
// One command packet is one FRAME. One frame has MANY patch jobs. So a direct
// join fills the section 9.1 list for the FIRST patch of the frame and every
// subsequent patch composes against an EMPTY list -- silently, with
// `fields_active_o` reading 0, `terrain_samples_evaluated` reading correctly,
// and every counter on both blocks balancing perfectly. That is the console's
// own "a detector wired to two operands that move together cannot fire" shape:
// nothing in either block looks at the field that moved.
//
// This is a HARDWARE fact about two composed blocks. It holds whether or not
// 13.2 is ever written, it would still hold if `zhao_terrain_patch_v2` existed
// tomorrow, and it names what the missing producer must DO rather than which
// file it must live in: SEAL the frame's list, and REPLAY it per patch.
//
// ===========================================================================
// WHAT THIS BLOCK IS, AND WHAT IT DELIBERATELY IS NOT
// ===========================================================================
// IT IS the frame-scoped half of directive 13.2's first responsibility, built
// once so that `zhao_terrain_patch_v2` INSTANTIATES it rather than growing a
// third copy of the same list. It owns:
//
//   (1) the bounded MAX_FIELDS-entry append in COMMAND ORDER, with the
//       terrain_rules section 9.1 law: append, reject the TAIL, NEVER evict,
//       and count the rejection;
//   (2) the SEAL, so a patch never composes against a half-delivered list;
//   (3) the {handle32 -> canonical program hash} resolution, against
//       `zhao_field_loader`'s publication port -- the producer entry I34's S2
//       records as having landed and which NOTHING had yet read;
//   (4) the per-patch REPLAY into a section 9.1 intake, with a real stall so
//       the vertex lane cannot start before the list is present.
//
// IT IS NOT the Earth evaluation and it does not pretend to be. The uniforms
// TerrainField carries -- `start_tick`, `duration_ticks` and p0..p7 -- are
// NOT stored here and NOT consumed here, because their only reader is the
// EARTH stream adapter (I34 build item (c)) and that block does not exist. A
// table with an address port nobody drives is a gap, and a stored word nobody
// reads is dead logic; both are worse than an honestly unconnected output on
// the producer. So this block closes the LIST half of I34 and leaves the
// HEIGHT half open, which is exactly what 20.8 permits and what it forbids
// pretending otherwise about.
//
// ===========================================================================
// THE HASH, AND WHY IT IS RESOLVED RATHER THAN FORWARDED
// ===========================================================================
// `zhao_cmd_exec`'s own header is emphatic and right: TerrainField carries a
// `handle32[program]`, FIELD.PROGCACHE's directory key is a CONTENT hash, and
// "wiring this output into a port called `hash` would put a handle in a trace
// field that says hash, which is a lie that costs nothing today and an
// afternoon later". This block therefore does not forward the handle. It
// SWEEPS `zhao_field_loader`'s publication port -- `pub_sel_o` walks
// 0..OBJECTS-1, one object per clock, and the FIRST object that is READY and
// whose `pub_handle_i` equals the record's handle supplies `pub_prog_hash_i`
// -- and stores THAT.
//
// The sweep is affordable by construction: OBJECTS clocks per record, at most
// MAX_FIELDS records per frame, so at the composed 8 and 16 the whole frame's
// resolution is 128 clocks against a 10,416-clock association allowance.
//
// A HANDLE THAT RESOLVES TO NOTHING STILL BECOMES AN ENTRY, and the reason is
// stated rather than assumed: the ASSOCIATION is real -- a command named a
// footprint and a program -- and the footprint is what `zhao_terrain_patch`
// keys on (its chosen law 2). What is absent is RESIDENCY, which is section
// 9's `PROGRAM_NOT_RESIDENT` and is the Earth adapter's verdict to reach, not
// this block's. The entry is appended with `hash = 0` and `unresolved_o`
// counts it. `fld_add_hash_i` is trace-only in the consumer
// (`zhao_terrain_patch.sv` chosen law 4's neighbourhood says so in as many
// words), so a zero there misleads nothing today; the adapter that later cares
// will key on the DIRECTORY, which is the authority on residency, and not on
// this trace field.
//
// ===========================================================================
// THE SEAL IS THE PRODUCER'S FACT, NOT A TIMING GUESS
// ===========================================================================
// `cmd_last_i` rides the record and says "this is the last of the commit's
// set". It is `zhao_cmd_exec`'s `tfld_set_last_o`, added by the same packet,
// because that block is the ONLY party that knows where its queue's commit
// pointer is. The alternative -- watch `cmd_valid_i` fall and call the gap a
// boundary -- is a cross-module timing assumption of exactly the kind this
// repository has paid for before, and it would seal a list in the middle of a
// set the instant the consumer backpressured.
//
// THE EMPTY CASE IS THE COMMON CASE AND IT MUST COST NOTHING. At reset the
// list is SEALED and holds zero records, so a console that never receives a
// TerrainField replays nothing, stalls for two clocks per patch job, and
// composes exactly as it does today. A design in which "no field programs"
// deadlocked the terrain chain would be caught by no gate this repository
// owns, because every existing smoke form is that case.
//
// NO DEADLOCK, STATED AS A CLOSED ARGUMENT RATHER THAN A HOPE. Intake is
// refused only while a replay is RUNNING (`rp_run`), never while one is
// merely pending; a pending replay is released by the SEAL, and the seal
// arrives on an intake this block is still accepting. So "waiting for the
// seal" and "refusing the record that carries it" are never both true.
//
// Conservative SystemVerilog subset only (charter section 2). Quartus 17.0:
// every elaboration check is inside `initial begin ... end` (R212), because a
// bare module-scope `if` is rejected with "syntax error near text: `if`" and
// `verilator --lint-only` does not run `initial` blocks at all.

module zhao_terrain_fieldlist #(
    // terrain_rules section 9.1's MAX_PATCH_FIELDS, frozen 2026-08-16. Named
    // rather than literal because `zhao_terrain_patch` carries the same
    // number and two hard-coded 16s are one knob with two halves.
    parameter int unsigned MAX_FIELDS = 16,
    // `zhao_field_loader`'s OBJECTS. The sweep's length, and nothing else.
    parameter int unsigned OBJECTS    = 8,
    parameter int unsigned OBJW       = 3
) (
    input var logic clk,
    input var logic rst_n,

    // ---- CMD.EXEC's TerrainField 0x0200 arm --------------------------------
    // The footprint is `rectfx` fx16 raw, CLOSED interval, exactly as the
    // consumer's section 9.1 test wants it. Nothing is converted here.
    input  var logic               cmd_valid_i,
    output var logic               cmd_ready_o,
    input  var logic signed [31:0] cmd_x0_i,
    input  var logic signed [31:0] cmd_z0_i,
    input  var logic signed [31:0] cmd_x1_i,
    input  var logic signed [31:0] cmd_z1_i,
    input  var logic        [31:0] cmd_handle_i,   // handle32[program]
    input  var logic        [15:0] cmd_cmd_i,      // record source_id, low 16
    input  var logic               cmd_last_i,     // last record of this commit

    // ---- FIELD.LOADER's publication port (the {handle -> hash} authority) --
    output var logic [OBJW-1:0]    pub_sel_o,
    input  var logic [OBJECTS-1:0] pub_ready_i,
    input  var logic [31:0]        pub_handle_i,
    input  var logic [31:0]        pub_prog_hash_i,

    // ---- per-patch replay into the section 9.1 intake ----------------------
    // `patch_open_i` is the consumer's OWN `list_clear_i` pulse, so the clear
    // and the refill cannot disagree about which patch they belong to.
    input  var logic               patch_open_i,
    output var logic               patch_stall_o,  // hold the vertex lane

    output var logic               add_valid_o,
    input  var logic               add_ready_i,
    output var logic signed [31:0] add_x0_o,
    output var logic signed [31:0] add_z0_o,
    output var logic signed [31:0] add_x1_o,
    output var logic signed [31:0] add_z1_o,
    output var logic        [31:0] add_hash_o,
    output var logic        [15:0] add_cmd_o,
    // ADDED 2026-09-23 (EARTHADAPT), and it is the SAME SWEEP's other half.
    // This block already walks FIELD.LOADER's publication objects to turn a
    // handle into a canonical program hash, and it already knows WHICH object
    // matched -- it simply threw the index away. `zhao_field_earth_adapter`
    // needs exactly that index, because a BIND reply's `fh2_resp_slot_o` IS
    // the object index and is what an adapter puts on `req_slot_o` (the same
    // value `zhao_geom_warp` resolves its own `d_slot_i` from).
    //
    // IT IS NOT A SECOND SWEEPER, and that is the point of carrying it here.
    // `pub_sel_o` has one master in this console; a consumer that resolved its
    // own slot would need a second, which is the two-client share entry I34
    // lists as build item (d) and which nobody has had to build.
    //
    // `add_resident_o` is the RESOLUTION, not the residency of the program in
    // the field fabric. It says a ready publication object claims this handle.
    // Whether that object's header is loaded is `zhao_field_host_v2`'s own
    // `!hdr_loaded[slot]` test, and the two are deliberately separate: this
    // block can see the binding and cannot see the fabric.
    output var logic        [OBJW-1:0] add_obj_o,
    output var logic                   add_resident_o,

    // ---- evidence ----------------------------------------------------------
    // Every one of these is asserted silent and then FIRED by
    // `tests/terrain/terrain_fieldlist_directed.cpp` (R95): a counter whose
    // zero nobody has seen move is a claim, not a measurement.
    output var logic [31:0] records_sealed_o,   // appended into a frame list
    output var logic [31:0] tail_rejected_o,    // section 9.1 law 2, > MAX_FIELDS
    output var logic [31:0] unresolved_o,       // handle published by no ready object
    output var logic [31:0] replays_o,          // patch lists filled
    output var logic [31:0] entries_replayed_o, // records handed to the consumer
    output var logic [31:0] open_at_patch_o,    // a patch job met an UNSEALED list
    output var logic [ 4:0] records_o,          // 0..MAX_FIELDS, the live count
    output var logic        sealed_o,
    output var logic        idle_o
);

  initial begin
    if (MAX_FIELDS < 1 || MAX_FIELDS > 31)
      $fatal(1, "zhao_terrain_fieldlist: MAX_FIELDS must be 1..31 (records_o is 5 bits)");
    if (OBJECTS < 1)
      $fatal(1, "zhao_terrain_fieldlist: OBJECTS must be nonzero");
    if ((32'd1 << OBJW) < OBJECTS)
      $fatal(1, "zhao_terrain_fieldlist: OBJW too narrow for OBJECTS");
  end

  localparam int unsigned IDXW = 5;                 // records_o's width
  localparam int unsigned AW   = (MAX_FIELDS <= 1) ? 1 : $clog2(MAX_FIELDS);

  // ---- the sealed list -----------------------------------------------------
  // One entry is {x0, z0, x1, z1, hash, cmd} = 176 bits. Written one per
  // clock, read one per clock, so a synthesiser is free to infer distributed
  // memory. HAND COUNT, IN THE UNFLATTERING DIRECTION (R236): if it does NOT
  // infer and lands in flops, MAX_FIELDS * 176 = 2,816 registers plus a
  // 16-to-1 176-bit read mux -- order 1,400 ALM total. That is the number to
  // quote, not the MLAB one. It is the SAME storage `zhao_terrain_patch`
  // already holds for one patch, which is the honest comparison: this console
  // now holds a frame's list and a patch's list, and directive 13.2's
  // `zhao_terrain_patch_v2` is where the two become one.
  logic signed [31:0] e_x0  [0:MAX_FIELDS-1];
  logic signed [31:0] e_z0  [0:MAX_FIELDS-1];
  logic signed [31:0] e_x1  [0:MAX_FIELDS-1];
  logic signed [31:0] e_z1  [0:MAX_FIELDS-1];
  logic        [31:0] e_hash[0:MAX_FIELDS-1];
  logic        [15:0] e_cmd [0:MAX_FIELDS-1];
  logic [OBJW-1:0]    e_obj [0:MAX_FIELDS-1];
  logic               e_res [0:MAX_FIELDS-1];

  logic [IDXW-1:0] n_rec;     // entries in the current list, 0..MAX_FIELDS
  logic            list_open; // records are still arriving for this commit

  // ---- the intake sequencer ------------------------------------------------
  localparam logic S_TAKE  = 1'b0;   // offering `cmd_ready_o`
  localparam logic S_SWEEP = 1'b1;   // walking the loader's publication objects

  logic            in_st;
  logic [OBJW-1:0] sw_sel;
  logic [IDXW-1:0] sw_cnt;      // objects examined so far
  logic            sw_hit;
  logic [31:0]     sw_hash;
  logic [OBJW-1:0] sw_obj;      // the object index the hit was found at
  logic            sw_reject;   // the held record is beyond MAX_FIELDS
  // THE HELD RECORD'S `cmd_last_i`, AND IT IS A REGISTER FOR A MEASURED
  // REASON. The first version of this block cleared `list_open` at the TAKE.
  // That is eight clocks -- the whole publication sweep -- BEFORE the record
  // reaches the array, so `sealed_o` went high while the set's last entry was
  // still in flight and a pending replay could start against a list one short.
  // `terrain_fieldlist_directed` caught it on every case at once: three
  // records offered, two replayed. The seal is a statement about the STORED
  // list, so it is made where the store happens.
  logic            sw_last;

  logic signed [31:0] h_x0, h_z0, h_x1, h_z1;
  logic        [31:0] h_handle;
  logic        [15:0] h_cmd;

  // ---- the replay sequencer ------------------------------------------------
  logic            rp_pending;  // a patch job is waiting for the seal
  logic            rp_run;
  logic [IDXW-1:0] rp_idx;
  logic [IDXW-1:0] rp_n;

  assign pub_sel_o     = sw_sel;
  // Refused only while a replay is RUNNING -- see the no-deadlock argument in
  // the header. A pending replay must keep taking records, because the record
  // it is waiting for is the one that seals the list.
  assign cmd_ready_o   = (in_st == S_TAKE) && !rp_run;
  assign records_o     = n_rec;
  assign sealed_o      = ~list_open;
  assign patch_stall_o = rp_pending || rp_run;
  assign idle_o        = (in_st == S_TAKE) && !rp_pending && !rp_run;

  // The replay presentation. Combinational off the array, and HELD until
  // `add_ready_i` takes it -- the consumer's `fld_add_ready_o` is a constant
  // 1'b1 today, but a lane that only worked against a constant ready is a lane
  // that breaks the day it stops being one.
  wire [AW-1:0] rp_a = rp_idx[AW-1:0];

  assign add_valid_o = rp_run && (rp_idx < rp_n);
  assign add_x0_o    = e_x0  [rp_a];
  assign add_z0_o    = e_z0  [rp_a];
  assign add_x1_o    = e_x1  [rp_a];
  assign add_z1_o    = e_z1  [rp_a];
  assign add_hash_o  = e_hash[rp_a];
  assign add_cmd_o   = e_cmd [rp_a];
  assign add_obj_o   = e_obj [rp_a];
  assign add_resident_o = e_res[rp_a];

  // The object the sweep is currently looking at agrees with the record.
  wire sw_match = pub_ready_i[sw_sel] && (pub_handle_i == h_handle);
  // The resolved hash at the end of the sweep: an earlier hit, else this last
  // object, else nothing resolved.
  wire [31:0] sw_resolved   = sw_hit ? sw_hash : (sw_match ? pub_prog_hash_i : 32'd0);
  wire        sw_unresolved = !sw_hit && !sw_match;
  // The OBJECT the resolved hash came from, by the same three-way rule and in
  // the same expression shape, so the two can never disagree about which
  // object answered. An unresolved record stores object 0 with `e_res` low:
  // the index is meaningless and the bit is what says so.
  wire [OBJW-1:0] sw_resolved_obj = sw_hit ? sw_obj : sw_sel;

  // A record arriving on a SEALED list opens a new one: the previous frame's
  // list is finished with, and section 9.1's bound is per list.
  wire             take_now     = cmd_valid_i && cmd_ready_o;
  wire             reopening    = take_now && !list_open;
  wire [IDXW-1:0]  eff_n        = reopening ? {IDXW{1'b0}} : n_rec;
  // Widened explicitly rather than cast: Quartus 17.0 and Verilator agree on
  // a concatenation, and a size cast is one of the forms R212 records as a
  // lint-clean/synthesis-reject trap.
  wire             would_reject = ({27'd0, eff_n} >= MAX_FIELDS);

  wire [AW-1:0] wr_a = n_rec[AW-1:0];

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (i = 0; i < MAX_FIELDS; i = i + 1) begin
        e_x0[i]   <= 32'sd0;
        e_z0[i]   <= 32'sd0;
        e_x1[i]   <= 32'sd0;
        e_z1[i]   <= 32'sd0;
        e_hash[i] <= 32'd0;
        e_cmd[i]  <= 16'd0;
        e_obj[i]  <= {OBJW{1'b0}};
        e_res[i]  <= 1'b0;
      end
      n_rec              <= 5'd0;
      list_open          <= 1'b0;
      in_st              <= S_TAKE;
      sw_sel             <= {OBJW{1'b0}};
      sw_cnt             <= 5'd0;
      sw_hit             <= 1'b0;
      sw_hash            <= 32'd0;
      sw_obj             <= {OBJW{1'b0}};
      sw_reject          <= 1'b0;
      sw_last            <= 1'b0;
      h_x0               <= 32'sd0;
      h_z0               <= 32'sd0;
      h_x1               <= 32'sd0;
      h_z1               <= 32'sd0;
      h_handle           <= 32'd0;
      h_cmd              <= 16'd0;
      rp_pending         <= 1'b0;
      rp_run             <= 1'b0;
      rp_idx             <= 5'd0;
      rp_n               <= 5'd0;
      records_sealed_o   <= 32'd0;
      tail_rejected_o    <= 32'd0;
      unresolved_o       <= 32'd0;
      replays_o          <= 32'd0;
      entries_replayed_o <= 32'd0;
      open_at_patch_o    <= 32'd0;
    end else begin

      // ---- intake ---------------------------------------------------------
      case (in_st)
        S_TAKE: begin
          if (take_now) begin
            // Open a new list on the first record after a seal. The clear is
            // the SAME cycle as the take, so a set of one record leaves a
            // list of exactly one.
            if (reopening) n_rec <= 5'd0;
            list_open <= 1'b1;
            sw_last   <= cmd_last_i;

            h_x0      <= cmd_x0_i;
            h_z0      <= cmd_z0_i;
            h_x1      <= cmd_x1_i;
            h_z1      <= cmd_z1_i;
            h_handle  <= cmd_handle_i;
            h_cmd     <= cmd_cmd_i;
            sw_reject <= would_reject;

            // Section 9.1 law 2: append in command order, reject the TAIL,
            // NEVER evict. A rejected record is still WALKED -- it costs the
            // sweep it does not need -- deliberately: a reject that skipped
            // the sweep would make the intake's timing depend on the list's
            // fullness, and the first thing that would hide is a resolution
            // failure on exactly the records a full frame drops.
            sw_sel  <= {OBJW{1'b0}};
            sw_cnt  <= 5'd0;
            sw_hit  <= 1'b0;
            sw_hash <= 32'd0;
            sw_obj  <= {OBJW{1'b0}};
            in_st   <= S_SWEEP;
          end
        end

        S_SWEEP: begin
          if (!sw_hit && sw_match) begin
            sw_hit  <= 1'b1;
            sw_hash <= pub_prog_hash_i;
            sw_obj  <= sw_sel;
          end
          if (({27'd0, sw_cnt} + 32'd1) >= OBJECTS) begin
            // The sweep is over. Commit or reject.
            if (sw_reject) begin
              tail_rejected_o <= tail_rejected_o + 32'd1;
            end else begin
              e_x0  [wr_a]     <= h_x0;
              e_z0  [wr_a]     <= h_z0;
              e_x1  [wr_a]     <= h_x1;
              e_z1  [wr_a]     <= h_z1;
              e_hash[wr_a]     <= sw_resolved;
              e_cmd [wr_a]     <= h_cmd;
              e_obj [wr_a]     <= sw_resolved_obj;
              e_res [wr_a]     <= !sw_unresolved;
              n_rec            <= n_rec + 5'd1;
              records_sealed_o <= records_sealed_o + 32'd1;
              if (sw_unresolved) unresolved_o <= unresolved_o + 32'd1;
            end
            // THE SEAL LANDS HERE, with the record, and not at the take.
            if (sw_last) list_open <= 1'b0;
            in_st <= S_TAKE;
          end else begin
            sw_sel <= sw_sel + {{(OBJW-1){1'b0}}, 1'b1};
            sw_cnt <= sw_cnt + 5'd1;
          end
        end

        default: in_st <= S_TAKE;
      endcase

      // ---- replay ---------------------------------------------------------
      // A patch job ALWAYS raises `rp_pending`, even for an empty list, so the
      // stall is one shape and not two. An empty list then runs for one clock
      // and retires, which is the console's behaviour today.
      if (patch_open_i) begin
        rp_pending <= 1'b1;
        if (list_open) open_at_patch_o <= open_at_patch_o + 32'd1;
      end

      if (rp_run) begin
        if (add_valid_o && add_ready_i) begin
          rp_idx             <= rp_idx + 5'd1;
          entries_replayed_o <= entries_replayed_o + 32'd1;
        end
        if (!add_valid_o || (add_ready_i && ((rp_idx + 5'd1) >= rp_n))) begin
          rp_run    <= 1'b0;
          replays_o <= replays_o + 32'd1;
        end
      end else if (rp_pending && !list_open && !patch_open_i) begin
        // Start only once the list is SEALED. `patch_open_i` is excluded so a
        // job taken in the same cycle as the seal still gets a clean start
        // edge rather than a replay that began before the consumer's
        // `list_clear_i` had emptied it.
        rp_pending <= 1'b0;
        rp_run     <= 1'b1;
        rp_idx     <= 5'd0;
        rp_n       <= n_rec;
      end
    end
  end

endmodule
