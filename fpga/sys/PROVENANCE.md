# MiSTer `sys/` provenance

The 57 upstream framework files in this directory were vendored verbatim from
[MiSTer-devel/Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer)
on 2026-09-13 for the SuperStation One hardware lane.

- upstream commit: `3ea1134cf05d62c2b1db30362277a823d739ced2`
- upstream `sys` tree: `9f95eddd65ebfca9b8dd94ed1a48e3f867165aa8`
- upstream path: `sys/`
- target selected by upstream `sys.tcl`: `5CSEBA6U23I7`
- accompanying upstream root license copied to: `fpga/sys/LICENSE`

`PROVENANCE.md` and `LICENSE` are local companion files and are not part of the
recorded upstream `sys` tree. Every other file below this directory must remain
byte-identical to that tree. Framework updates replace the upstream files; board
or Zhaozhou customisation belongs outside `fpga/sys/`.

The import was checked file-by-file with SHA-256 against a detached checkout of
the pinned commit: 57 expected files, zero missing or mismatched files.

## SuperStation build-copy safety overlay

The vendor tree above remains unchanged. `build_superstation_bringup.ps1` and
`build_superstation_specs.ps1` copy it into their owned workspace and then run
`tools/board/patch_mister_sys_top.py` against that copy only.

- pinned upstream `sys_top.v` SHA-256:
  `9bc5562bcc9d923aa3bff1a9c976c52492edb9ef981f428b7a4101c919a711b8`;
- exact repaired build-copy SHA-256:
  `24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9`;
- repair 1: all seven USER/SNAC assignments are unconditional high-impedance;
- repair 2: the 4-bit scaler-mode expression receives a leading zero for the
  5-bit `ascal.mode` port, preserving all four existing bit meanings.

Both replacement anchors and both digests are exact and mutation-tested. Any
upstream drift refuses before writing the build copy. Post-fit verification must
also report all seven USER_IO output enables permanently disabled and zero
Critical Warnings of any spelling.

Historical 2026-09-13 RBFs predate this overlay. Their observed color-bar/green
results remain historical evidence, but independent review invalidated them for
future loading.

This closes the original `ZH-000` source-availability block for the
SuperStation/MiSTer lane. It does not by itself close PLL/reset behaviour or
board-load safety; those are established by the board-specific project,
constraints, final Quartus pin report, and staged volatile bring-up receipts.
