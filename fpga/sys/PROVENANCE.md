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

This closes the original `ZH-000` source-availability block for the
SuperStation/MiSTer lane. It does not by itself close PLL/reset behaviour or
board-load safety; those are established by the board-specific project,
constraints, final Quartus pin report, and staged volatile bring-up receipts.
