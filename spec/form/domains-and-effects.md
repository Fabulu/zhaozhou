# Form Domains and Effects — Phase 1 scope

**Status:** stub, content-scoped to Phase 1 (Phase 3 owns the expansion).

## 1. Truth/presentation law (frozen now)

Field programs compute **truth** (terrain height, velocity, material,
nav_cost) — never presentation. A field program is a pure function
(field-ir.md §1.4); its only effects are its output record lanes (§7.1) and
the sticky Status word (§3.19). Presentation (colour, emissive, shake) is
derived downstream from truth by raster/presentation blocks, never baked into
a field program. This is the seam that lets the same .zprog replay on
C++/RTL/board byte-identically.

## 2. The five profiles (frozen v1)

`earth` (terrain height/velocity/material/nav at a column), `warp`
(displacement + normal bending), `flow` (advection velocity fields),
`formation` (instance transforms), `stamp` (surface decals/tags) — I/O records
and provisional ceilings in field-ir.md §7. Profiles restrict *shape*, never
*semantics*: one opcode table, one interpreter, five envelopes.

## 3. Phase 3 expands

Per-domain effect contracts (what a warp program may legally read/write in the
particle/stream state); footprint declaration and bake-ability (FORM §8);
interaction with TerrainField/SurfaceStamp command records; the material and
nav_cost vocabularies; hazard/tag semantics consumed by stamp outputs.
