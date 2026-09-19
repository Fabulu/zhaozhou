# Owner rulings, 2026-09-19 evening

Answers to `reports/OWNER-DOCKET-20260919.md` and handover §8/§12, given by
Fabian in session RUN-20260919-1656-gaps-to-zero. Items marked **(owner, explicit)**
were chosen by the owner directly. The owner then said *"go with your recommended
answers for now and don't stop to quiz me"*, so items marked **(provisional,
coordinator's recommendation)** stand until the owner revises them. Each is
cheap to reverse and is cited where it is used.

| # | question | ruling |
|---|---|---|
| R1 | PART.COLLIDE terrain normal (I6) — spec §4.4 finite differences vs TERRAIN.NORMALS.md:194 face normals | **Face normal (owner, explicit).** The collision normal is `normalize3_approx(face_normal(...))` of the triangle `spec/terrain_rules.md` §4.3 already picks for the height. `spec/terrain_rules.md` §4.4 is to be amended to say so for collision. `zhao_terrain_heighttap` grows the normal; I6 closes. |
| R2 | GEOM.LIGHT's owner (docket §5) | **`zhao_light_stream` owns vertex light (owner, explicit).** `zhao_geom_light` is superseded; `sky_and_beams.md` §4a is amended to match. Record the supersession in `design/console_inventory.yml` and the ledger. |
| R3 | Third projector port for particles / FORGE.SHADOW (I24, docket §2) | **Keep the time-multiplex (owner, explicit).** No third port in v1. Owed: a written schedule proof that geometry, particles and FORGE.SHADOW's instance-centre 1/w share client A's bandwidth within the frame at the guaranteed content tier. |
| R4 | Third client on `zhao_hps_arbiter` (I26/I27, MEM.UPLOAD) | **Widen to N clients (owner, explicit)**, preserving and re-proving the existing starvation law. Then compose MEM.UPLOAD and the terrain clients. |
| R5 | TERRAIN.NORMALS/SHADE third ModeTri pass (docket §7) | **Change the schedule (owner, explicit).** Keep lit terrain normals. Restructure the sequencer schedule so the third pass fits, with the frame deadline proven. |
| R6 | Island handle width, T10 50 bits vs RTL 32 (docket §6) | **Widen the RTL to 50 bits (owner, explicit).** T10 stands as written. |
| R7 | INPUT.SNAC, GEOM.WARP, POST.ECHO (docket §9–11) | **Build all three (owner, explicit).** They stay mandatory; the 2026-09-18 revocation stands. |
| R8 | TERRAIN.LOD deviation law (I21/I44, docket §3) | **The packet renders both readings side by side and the owner picks by eye (owner, explicit).** Until that happens (provisional, coordinator's recommendation) implement the reading that keeps T8's nested decimation bit-identical on shared vertices. Keep the choice in ONE named, editable constant/selector so the owner's pick is a one-line change, and produce the comparison render. |
| R9 | TEXTURE.TMU / `texture_samples` owner (docket §8) | **(provisional, coordinator's recommendation)** Retire TEXTURE.TMU as a single module in favour of the v3 path, and give `texture_samples` ONE owner: the v3 block that retires a filtered sample to the fragment. It counts samples it actually delivered. The ledger names that block, and the register resolves the capability to it. |
| R10 | Depth-profile port (docket §1), directory key (docket §4) | Already landed (`ac4f293d`, `db46a73e`). |
