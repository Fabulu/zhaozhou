# FINDINGS -- geomseam lane (gz/geomseam)

*The harness blocks subagents from writing report files, so this packet put its
report in a COMMIT MESSAGE. Transcribed here verbatim by the coordinator, because
files under `fpga/rtl/` cite this path and a citation to a file that does not
exist is an uncashed cheque one level down. Source commit: `442cdf95`.*

---

GEOMSEAM: R90 amended -- the kind-8 lift is right and it is not the whole
obstacle, and the half it misses is priced in ALMs

R90 says of I29's second partial lift: "the mechanism already exists and was
built for this". The PAGE mechanism does. **The bytes and the silicon do not**,
and the second is the expensive omission. Both findings searched, not inherited.

(a) NOTHING ANYWHERE DEFINES THE BYTE LAYOUT of a bone hierarchy or a clip
frame. `zref_creature.hpp` holds C++ structs whose members are `std::vector`
(no wire layout at all); `spec/creature_rules.md:58-60` holds a size in prose
("8 B/bone/frame, 32 bones => <= 268 B/frame") with no field order and no
alignment; `zref_creature_page.hpp:23-28` explicitly disclaims freezing them;
and `zref_creature.hpp:43-46` calls the quaternion lane format "PROPOSED, NOT
FROZEN". So the lift does not UNBLOCK a layout -- somebody must AUTHOR one, and
that authoring is an ABI freeze, not a packaging step.

(b) THE PRODUCER IS A ~17.6 kbit ASYNCHRONOUS-READ STORE, AND NOBODY HAS PRICED
IT. `zhao_geom_pose_decode.sv:89-92` makes the source fetch COMBINATIONAL BY
CONTRACT -- "the caller must present the data for `bone_idx_o` in the SAME
cycle" -- and `:146` is `assign bone_idx_o = b[4:0]`, pure combinational, with
the data required stable for every cycle the block sits in S_FETCH, not just
one. Per bone that is 5 + 3x32 + 4x16 + 12x32 = ~550 bits; at MAX_BONES = 32
the caller owes ~17.6 kbit that a synchronous M10K cannot serve. On a device
already measured at ~113% of its ALM ceiling that is an owner-visible resource
decision -- exactly the shape `zhao_geom_pose_cache`'s own header refuses to
bury ("burying it inside this module would settle it silently").

WHAT IS NOT THE OBSTACLE, confirmed so nobody builds one: the page path.
`zhao_geom_ladderbank.sv:81` already carries `PAGE_KIND = 8'd8` and walks a
creature-form page as 64-byte lines. I verified it is NOT instantiated in
`fpga/rtl/prod/` -- two hits in `zhao_console_core.sv`, both comments (:3351,
:3705). Composing a reader adds NO console-core boundary port: the publication
nets and the ENGINE1 window are already internal through `u_geom_mem_adapter`,
whose five requesters A-E are full, so a sixth is an in-core adapter edit.

RECOMMENDATION, recorded on R90's row: grant the lift, and schedule I29 as its
own packet with four named items -- author and freeze the body section and a
minimal kind-9 frame with a zref model; emit a non-zero `body_off` from
`tools/pack/mkcreatureladder.py`; compose the page reader as the sixth adapter
requester; and PRICE THE ASYNC-READ PALETTE SOURCE IN ALMS BEFORE BUILDING IT.
If it is not affordable, what has to move is the DECODER's combinational
contract -- a larger decision than the freeze, and not one to discover halfway
through a packet.

AND TWO OWNER DECISIONS, appended as their own section:

  D-GEOMSEAM-A  R90 is under-priced and the pricing may invert it (above).
  D-GEOMSEAM-B  A THIRD OWNER ON CLIENT A RE-AUTHORS A RATIFIED LAW, and it is
                NOT a "+1 bit" widening. Verified first-hand:
                `zhao_console_core.sv:3966` sets GEOM_PAY_A_W = 16 and the
                elaboration guard at :6348-6350 proves the rider is
                ARENA_W 3 + INDEX_W 12 = 15; `zhao_part_project.sv:356` sets
                TAG_BIT = PAY_W - 1 and :508's `res_is_part_c` is a ONE-BIT
                TWO-OWNER discriminator. A third owner makes that bit unable to
                name the owner at all -- it becomes a two-bit field and
                `geom_tag_collision_o`'s one-bit law must be RE-AUTHORED with
                it. Sizing this as "16 -> 17" leaves a collision counter that
                silently stops meaning anything, which is R88's own shape one
                block over. Route B, R68's instance-centre 1/w and R3's
                time-multiplex all want this widening; whoever takes it owns
                re-authoring the law and re-firing the counter in one commit.

Documents only. Register 21, unchanged.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

