# Manafold Pass 17 solid-body contour-mist ladder

**Direction:** Owner Direction 15
**Binary:** Pass-17 scale baseline, same executable for every rung
**Subject:** `manafold-inspect`, every rung rendered through all 600 frames

## Evidence plates

- `PASS17-SHELL-LADDER-NATIVE.png` / `PASS17-SHELL-LADDER-4X.png` — Pass-16 control plus first three solid-body candidates at f300.
- `PASS17-SHELL-LADDER-VIEWS.png` — f150/f450 multi-view comparison.
- `PASS17-SHELL-FINALISTS-NATIVE.png` / `PASS17-SHELL-FINALISTS-4X.png` — Pass-16, B, D and E finalists at f300.

Every render returned RC 0. Unlisted controls retain shipping defaults: scatter/alpha `450`, outward reach `140`, rise gamma `1050`, core floor `0`, ink handling and tint unchanged.

| Rung | Peak | Inward decay | Transmission | By-eye read |
|---|---:|---:|---:|---|
| Pass 16 control | 180 | 800 | 750 | Broad body transmission; interior reads washed/see-through rather than predominantly solid. Rejected by Direction 15. |
| A | 160 | 300 | 300 | Strong improvement, but the interior edge of the band still reaches farther inward than requested. |
| B | 120 | 220 | 220 | Solid interior with a visible contour mist; close, but still slightly broad at 4×. |
| C | 140 | 260 | 120 | Solid/dark core, but the broader decay weakens the “only a little inside” read. |
| D | 100 | 140 | 180 | **Selected.** Most of the ball remains solid; mist occupies a narrow contour-centred ridge, reaches only slightly inward and visibly thins outside. |
| E | 120 | 160 | 220 | Similar to B; more luminous/transmissive than needed beside D. |

## Decision

Select **D: peak 100 / decay 140 / transmission 180** as the current authored Pass-17 body-optics values. This is a visual choice from final-resolution scenes, not a value generated from the old shell measurements. Keep the other independent defaults named and unchanged unless the integrated target bank shows a concrete problem.

The same-binary legacy control is explicit `180/800/750`; it restores the rejected broad transparency. `manafold_shellgate` is re-aimed to nine structural checks: solid untouched core, contour-adjacent peak, monotone outward thinning, body-scaled band, limited inward reach and real scene transmission among them. Normal, nine-leg selftest, Pass-16 legacy control and historical Pass-15 regression control all return RC 0. The new limited-inward-reach check goes red on 180/800/750 specifically. The gate preserves the selected picture; it does not choose these values.

Final targeted review still covers Inspect plus Hover/Rest views before the full bank. If D loses visible contour mist in those scenes, adjust the named ladder by eye rather than broadening the interior mechanically.
