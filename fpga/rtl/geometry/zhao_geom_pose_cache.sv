// zhao_geom_pose_cache.sv — the pose cache's tags, LRU and counters.
//
// GEOM.POSE's cache half (design/contracts/GEOM.POSE.md). The decode half is
// `zhao_geom_pose_decode`; this block decides WHETHER a decode is needed, WHERE
// its result belongs, and keeps the four counters the contract exposes.
//
// Reference: `zref::creature::PoseBank::acquire` and `::begin_frame`
// (reference/src/zcreature/creature_core.cpp:166).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DELIBERATELY DOES NOT OWN
// ---------------------------------------------------------------------------
// **The palettes themselves.** 128 tuples x 32 bones x 12 elements x 32 bits is
// 1.5 Mbit — about 28% of the device's entire 553-block M10K budget for one
// cache. That is a resource decision with alternatives (fewer tuples, palettes
// in SDRAM at a bandwidth cost, or a narrower matrix format), and burying it
// inside this module would settle it silently.
//
// So this block emits a VERDICT and a slot index, and the caller owns the
// store. `resp_kind_o` says what happened; on MISS_INSERT the caller decodes
// into `resp_slot_o`, on CLAMPED it decodes into its scratch and does not
// insert, on HIT it reads `resp_slot_o`, and on BAD_ID it uses the identity
// bind pose. The sizing argument belongs in design/budgets/, with numbers.
//
// ---------------------------------------------------------------------------
// THE LAW, and the two parts of it that are not obvious
// ---------------------------------------------------------------------------
// On acquire(type, clip_slot, frame):
//
//   1. If the caller reports the request is not resolvable — no clip with that
//      slot id, or frame >= frame_count — then bad_ids++ and BAD_ID, with NO
//      cache state touched at all. Bad ids never evict anything.
//   2. Otherwise scan for a valid slot matching (type, clip, frame). On a match:
//      hits++, mark it referenced-this-frame, restamp its LRU, HIT.
//   3. Otherwise misses++, and choose a victim:
//        - the FIRST invalid slot, stopping the search there; otherwise
//        - among slots NOT referenced this frame, the smallest LRU stamp.
//   4. If every slot was referenced this frame, there is no victim: that is a
//      content-tier violation. clamped_inserts++ and CLAMPED — decode without
//      inserting. Deterministic, and never a stall or a wrong palette.
//
// **The first-invalid rule really is a break.** The reference's victim loop
// tracks the best LRU as it goes, but the moment it meets an invalid slot it
// takes it and stops — even though a valid slot earlier in the scan may already
// have looked like a better victim. Matching the counters means matching that,
// so this block breaks too.
//
// **A referenced-this-frame slot is never evicted.** That is what makes a
// palette safe to hand out and still be there when the next instance of the
// same type asks for it in the same frame — the type-grouped army economy the
// whole cache exists for. It is also why the clamp in rule 4 has to exist.
//
// ---------------------------------------------------------------------------
// WHY A SEQUENTIAL SCAN IS THE RIGHT SHAPE HERE
// ---------------------------------------------------------------------------
// A fully-associative 128-entry tag compare in parallel is 128 comparators and
// puts the tags in flip-flops: about 6,300 registers. Scanned instead, the tags
// live in ONE M10K and the cost is up to 128 cycles for a hit and up to 256 for
// a miss.
//
// That is affordable because of what the numbers are: a miss already costs
// about 1,600 cycles in the decoder, so the scan is under a sixth of it. And a
// hit at 128 cycles, for a few hundred instances a frame, is a low-single-digit
// percentage of a 60 Hz frame. The cache is per-INSTANCE-per-frame work, not
// per-vertex work, and this block is sized for that.
//
// The LRU stamp is 48 bits. 32 would wrap after about six days of continuous
// play and silently invert the eviction order; 48 outlasts the hardware.
module zhao_geom_pose_cache #(
    parameter int TUPLES = 128,

    // THE KEY'S FORM FIELD, IN BITS. Owner ruling of 2026-09-21 (kind-8 /
    // kind-9 ownership), section 4a:
    //
    //     "zhao_geom_pose_cache.acq_type_i and its stored type tag are
    //      currently 16 bits; form indices are 24 bits. A direct connection
    //      using the low 16 bits aliases forms such as 0x000100 and 0x010100.
    //      ... production must use 24 bits and the high-bit-alias negative
    //      test must run. Do not invent a compact mapping merely to retain the
    //      old width."
    //
    // 24 IS THE PRODUCTION VALUE AND THE DEFAULT. The parameter exists so a
    // legacy bench can be configured at 16 without a second module, which the
    // ruling permits in terms -- it does NOT exist so production can be
    // narrowed, and the elaboration guard below refuses anything outside
    // 16..24. `spec/memory_rules.md` 5f.1's key is {index:24}; a cache keyed on
    // less of it answers one form's question with another form's palette, and
    // every counter in this block reads correct while it does.
    parameter int TYPE_W = 24
) (
    input  logic clk,
    input  logic rst_n,

    // ---- frame boundary ----------------------------------------------------
    // Clears every referenced-this-frame mark. One cycle: the marks are a flat
    // register vector precisely so this is not a 128-cycle walk.
    input  logic begin_frame_i,

    // ---- acquire -----------------------------------------------------------
    input  logic        acq_valid_i,
    output logic        acq_ready_o,
    input  logic [TYPE_W-1:0] acq_type_i,
    input  logic [15:0] acq_clip_i,
    input  logic [15:0] acq_frame_i,
    // THE HALF-KEY PHASE. Added 2026-09-03: the reference
    // `zref::creature::PoseBank::acquire(type, slot, frame, sub)` has always
    // carried it and this cache did not, so with baked 60 Hz presentation data
    // a key and its midpoint had the SAME {type, clip, frame} and aliased --
    // the cache returned the wrong palette and nothing reported an error. The
    // animation ruling of 2026-09-03 permits baked 60 Hz for any creature,
    // which turned a latent mismatch into a live defect.
    input  logic [7:0]  acq_sub_i,
    // THE CLIP-BANK GENERATION. Owner ruling D-3, 2026-09-03:
    //
    //     cache tag = physical line tag + residency generation
    //
    // and the ruling names this cache specifically -- it "must also
    // distinguish the clip-bank generation in addition to
    // {type, clip, frame, sub}".
    //
    // Why: animation banks live in HPS DDR and are uploaded into reusable
    // local-SDRAM slots. A physical slot stopped being a sufficient identity
    // the moment slots became reusable, so a pose cached from generation N and
    // read after generation N+1 was published is a pose from bytes that are no
    // longer there. It decodes, it looks like animation, and nothing reports
    // an error -- the same silent-wrong-answer shape as the `sub` aliasing
    // fixed earlier today, one level up.
    input  logic [15:0] acq_gen_i,

    // ---- ASSET LIFETIME: the two resource identities, kept apart ------------
    // Owner ruling of 2026-09-21, section 4b:
    //
    //     "Owner equality proves the intended form, not which version of its
    //      skeleton and animation produced a cached palette. ... Cache validity
    //      must distinguish the accepted body and clip resource identities and
    //      generations, including sub-phase. ... NEVER COMPARE
    //      body_generation == clip_generation: they belong to independent
    //      publications. Likewise, two different clip resources may have equal
    //      local generation numbers. A cache hit must not confuse them."
    //
    // So four fields, not two, and no comparison between them anywhere in this
    // file: each is matched against ITS OWN stored copy. `acq_gen_i` above
    // stays the CLIP BANK's generation (owner decision D-3) and gains
    // `acq_clip_idx_i` beside it, because two different clip resources may
    // carry the same local generation number -- the generation alone does not
    // identify the resource.
    //
    // WHY A BODY CHANGE MUST MISS. A palette is decoded from a skeleton AND a
    // clip. Republish the BODY alone -- a re-rigged creature, same form, same
    // clips, same frame, same sub-phase -- and every field the cache held
    // before this packet still matches. The stale palette hits, it decodes, it
    // looks like animation, and nothing reports an error.
    //
    // These are `zhao_geom_clipread`'s `res_body_index_o` / `res_body_gen_o`
    // and `res_clip_index_o`, which that block already carries through from
    // MEM.UPLOAD's publication. They are RESOURCE identity; `acq_type_i` above
    // is FORM identity. Two different numbers about one creature, and the
    // ruling's section 1 keeps them apart deliberately.
    input  logic [23:0] acq_body_idx_i,
    input  logic [15:0] acq_body_gen_i,
    input  logic [23:0] acq_clip_idx_i,

    // The caller resolves the clip table and the frame bound; this block does
    // not own the clip bank. Low means "no such clip slot, or frame past the
    // end", which is rule 1.
    input  logic        acq_resolvable_i,

    // ---- verdict -----------------------------------------------------------
    output logic       resp_valid_o,
    input  logic       resp_ready_i,
    output logic [1:0] resp_kind_o,
    output logic [$clog2(TUPLES)-1:0] resp_slot_o,

    // ---- counters, the four the contract exposes ---------------------------
    output logic [31:0] hits_o,
    output logic [31:0] misses_o,
    output logic [31:0] bad_ids_o,
    output logic [31:0] clamped_inserts_o,
    output logic [31:0] resident_o
);

  localparam int IDXW = $clog2(TUPLES);
  localparam int LRUW = 48;

  // Quartus 17.0 rejects a bare module-scope `if`, so the guard lives inside
  // `initial begin ... end` (CLAUDE.md, 2026-09-08). `--lint-only` does not run
  // it, so a clean lint is not evidence about it.
  initial begin
    if (TYPE_W < 16 || TYPE_W > 24)
      $fatal(1, "zhao_geom_pose_cache: TYPE_W is %0d; memory_rules 5f.1's form index is 24 bits and production must carry all of them",
             TYPE_W);
  end

  // Tag layout, named once so the slices below are readable, and DERIVED from
  // the field widths rather than written as literals -- TYPE_W moves and a
  // hand-written 16 would silently overlap the next field.
  localparam int TYPE_LO  = 0;
  localparam int CLIP_LO  = TYPE_LO  + TYPE_W;
  localparam int FRAME_LO = CLIP_LO  + 16;
  localparam int SUB_LO   = FRAME_LO + 16;
  localparam int GEN_LO   = SUB_LO   + 8;    // the CLIP BANK's generation (D-3)
  localparam int CIDX_LO  = GEN_LO   + 16;   // the clip resource's 5f.1 index
  localparam int BGEN_LO  = CIDX_LO  + 24;   // the BODY's own generation
  localparam int BIDX_LO  = BGEN_LO  + 16;   // the body resource's 5f.1 index
  localparam int LRU_LO   = BIDX_LO  + 24;

  localparam logic [1:0] RESP_HIT         = 2'd0;
  localparam logic [1:0] RESP_MISS_INSERT = 2'd1;
  localparam logic [1:0] RESP_CLAMPED     = 2'd2;
  localparam logic [1:0] RESP_BAD_ID      = 2'd3;

  // ---- tag store ----------------------------------------------------------
  // {lru, body_idx, body_gen, clip_idx, clip_gen, sub, frame, clip, type}.
  // `valid` and `this_frame` stay as register vectors because both are needed
  // combinationally during the scan and `this_frame` must clear for every slot
  // in a single cycle at the frame boundary.
  //
  // COST, RECORDED. At TUPLES=128 and TYPE_W=24 the tag is 192 bits against the
  // pre-ruling 120: +72 bits x 128 = 9,216 logical bits, of which 1,024 are
  // section 4a's eight extra form bits and 8,192 are section 4b's two resource
  // indices and the body generation. That is a LOGICAL bit count, not an M10K
  // or ALM figure -- physical RAM packing, the wider comparator and the four
  // extra input ports are counted by a fit and by nothing else.
  localparam int TAGW = LRU_LO + LRUW;

  logic [TAGW-1:0] tags [0:TUPLES-1];
  logic [IDXW-1:0] tag_raddr, tag_waddr;
  logic [TAGW-1:0] tag_rdata, tag_wdata;
  logic            tag_we;

  // M10K rules: no initializer, no reset branch on the array, registered read
  // inside the clocked process only.
  always_ff @(posedge clk) begin
    if (tag_we) tags[tag_waddr] <= tag_wdata;
    tag_rdata <= tags[tag_raddr];
  end


  logic [TUPLES-1:0] valid_q;
  logic [TUPLES-1:0] this_frame_q;

  typedef enum logic [2:0] {
    S_IDLE,
    S_HITSCAN,
    S_VICSCAN,
    S_COMMIT,
    S_RESP
  } state_e;

  state_e state;

  // The scan is one pipeline stage deep: cycle N issues the read for slot `i`,
  // cycle N+1 evaluates it as slot `j`. `j_live` says whether `j`/`tag_rdata`
  // hold anything yet, which is false only on the first cycle of a pass.
  logic [IDXW-1:0] i;          // next slot to issue a read for
  logic [IDXW-1:0] j;          // the slot tag_rdata belongs to
  logic            j_live;
  logic            j_last;     // j is the final slot: this pass ends here
  logic [LRUW-1:0] lru_ctr;

  logic [TYPE_W-1:0] q_type;
  logic [15:0] q_clip, q_frame;
  logic [7:0]  q_sub;
  logic [15:0] q_gen;
  logic [23:0] q_bidx, q_cidx;
  logic [15:0] q_bgen;

  // The stored tag, in ONE place, so the hit comparison and the two writes
  // cannot drift apart. They did not before this packet either, but there were
  // five fields; there are now nine.
  logic [TAGW-1:0] tag_new;
  assign tag_new = {(lru_ctr + 1'b1), q_bidx, q_bgen, q_cidx, q_gen, q_sub,
                    q_frame, q_clip, q_type};

  logic            have_inv;
  logic [IDXW-1:0] inv_idx;
  logic            have_best;
  logic [IDXW-1:0] best_idx;
  logic [LRUW-1:0] best_lru;

  // An invalid slot beats the best LRU, per the break in the reference's loop.
  logic [IDXW-1:0] victim;
  assign victim = have_inv ? inv_idx : best_idx;

  logic [1:0]      kind_q;
  logic [IDXW-1:0] slot_q;

  assign acq_ready_o = (state == S_IDLE) && (!resp_valid_o || resp_ready_i);
  assign resp_kind_o = kind_q;
  assign resp_slot_o = slot_q;

  // The scan reads slot `i`; the value read lands next cycle and is evaluated
  // as slot `j`. One pipeline stage, so a scan of N slots takes N+1 cycles.
  assign tag_raddr = i;
  assign j_last = j_live && (j == IDXW'(TUPLES - 1));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      valid_q <= '0;
      this_frame_q <= '0;
      lru_ctr <= '0;
      i <= '0;
      j <= '0;
      j_live <= 1'b0;
      have_inv <= 1'b0;
      have_best <= 1'b0;
      inv_idx <= '0;
      best_idx <= '0;
      best_lru <= '0;
      q_type <= '0; q_clip <= '0; q_frame <= '0; q_sub <= '0;
      q_gen <= '0;
      q_bidx <= '0; q_bgen <= '0; q_cidx <= '0;
      kind_q <= RESP_HIT;
      slot_q <= '0;
      resp_valid_o <= 1'b0;
      hits_o <= '0;
      misses_o <= '0;
      bad_ids_o <= '0;
      clamped_inserts_o <= '0;
      resident_o <= '0;
      tag_we <= 1'b0;
      tag_waddr <= '0;
      tag_wdata <= '0;
    end else begin
      tag_we <= 1'b0;

      // The frame boundary is independent of the scan: a decode in flight keeps
      // its own marks correct because a slot is marked when it is chosen, not
      // when its palette lands.
      if (begin_frame_i) this_frame_q <= '0;

      if (resp_valid_o && resp_ready_i) resp_valid_o <= 1'b0;

      unique case (state)
        S_IDLE: begin
          if (acq_valid_i && acq_ready_o) begin
            q_type <= acq_type_i;
            q_clip <= acq_clip_i;
            q_frame <= acq_frame_i;
            q_sub   <= acq_sub_i;
            q_gen   <= acq_gen_i;
            q_bidx  <= acq_body_idx_i;
            q_bgen  <= acq_body_gen_i;
            q_cidx  <= acq_clip_idx_i;
            if (!acq_resolvable_i) begin
              // Rule 1: no cache state is touched. A bad id must not be able to
              // evict a live palette.
              bad_ids_o <= bad_ids_o + 32'd1;
              kind_q <= RESP_BAD_ID;
              slot_q <= '0;
              resp_valid_o <= 1'b1;
            end else begin
              i <= '0;
              j <= '0;
              j_live <= 1'b0;
                      have_inv <= 1'b0;
              have_best <= 1'b0;
              best_lru <= '0;
              state <= S_HITSCAN;
            end
          end
        end

        // Pass 1: the first valid slot whose tag matches. The reference returns
        // on first match, so this stops there too.
        S_HITSCAN: begin
          i <= i + IDXW'(1);
          j <= i;
          j_live <= 1'b1;

          // `sub` is part of the KEY, not payload: a key and its 60 Hz
          // midpoint differ in nothing else.
          // EVERY FIELD, EACH AGAINST ITS OWN STORED COPY. No field is
          // compared against another field: `q_bgen` is matched to the stored
          // BODY generation and `q_gen` to the stored CLIP generation, never
          // to each other, because they belong to independent publications
          // (section 4b). And `q_cidx`/`q_bidx` are here beside them because
          // two different resources may carry equal local generation numbers.
          if (j_live && valid_q[j] && tag_rdata[TYPE_LO+:TYPE_W] == q_type &&
              tag_rdata[CLIP_LO+:16] == q_clip &&
              tag_rdata[FRAME_LO+:16] == q_frame &&
              tag_rdata[SUB_LO+:8] == q_sub &&
              tag_rdata[GEN_LO+:16] == q_gen &&
              tag_rdata[CIDX_LO+:24] == q_cidx &&
              tag_rdata[BGEN_LO+:16] == q_bgen &&
              tag_rdata[BIDX_LO+:24] == q_bidx) begin
            hits_o <= hits_o + 32'd1;
            this_frame_q[j] <= 1'b1;
            lru_ctr <= lru_ctr + 1'b1;
            tag_we <= 1'b1;
            tag_waddr <= j;
            tag_wdata <= tag_new;
            kind_q <= RESP_HIT;
            slot_q <= j;
            state <= S_RESP;
          end else if (j_last) begin
            // Every slot scanned, no match.
            misses_o <= misses_o + 32'd1;
            i <= '0;
            j <= '0;
            j_live <= 1'b0;
            state <= S_VICSCAN;
          end
        end

        // Pass 2: the FIRST invalid slot wins outright and stops the search --
        // even if an earlier valid slot already looked like a better victim.
        // The reference breaks there, and matching the counters means matching
        // that. Otherwise: the smallest LRU among slots not referenced this
        // frame.
        S_VICSCAN: begin
          i <= i + IDXW'(1);
          j <= i;
          j_live <= 1'b1;

          if (j_live && !valid_q[j]) begin
            have_inv <= 1'b1;
            inv_idx <= j;
            state <= S_COMMIT;
          end else begin
            if (j_live && !this_frame_q[j] && (!have_best || tag_rdata[LRU_LO+:LRUW] < best_lru)) begin
              have_best <= 1'b1;
              best_idx <= j;
              best_lru <= tag_rdata[LRU_LO+:LRUW];
            end
            if (j_last) state <= S_COMMIT;
          end
        end

        S_COMMIT: begin
          if (!have_inv && !have_best) begin
            // Rule 4: every slot is referenced this frame. Decode without
            // inserting, and say so.
            clamped_inserts_o <= clamped_inserts_o + 32'd1;
            kind_q <= RESP_CLAMPED;
            slot_q <= '0;
          end else begin
            if (!valid_q[victim]) resident_o <= resident_o + 32'd1;
            valid_q[victim] <= 1'b1;
            this_frame_q[victim] <= 1'b1;
            lru_ctr <= lru_ctr + 1'b1;
            tag_we <= 1'b1;
            tag_waddr <= victim;
            tag_wdata <= tag_new;
            kind_q <= RESP_MISS_INSERT;
            slot_q <= victim;
          end
          state <= S_RESP;
        end

        S_RESP: begin
          if (!resp_valid_o) begin
            resp_valid_o <= 1'b1;
          end else if (resp_ready_i) begin
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_pose_cache
