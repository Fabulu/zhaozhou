# MiSTer core PLL provenance

The MiSTer framework's `sys/pll_q17.qip` expects the core PLL at `rtl/pll.qip`.
The following files are vendored verbatim from
[MiSTer-devel/Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer)
commit `3ea1134cf05d62c2b1db30362277a823d739ced2`:

| Path | Upstream Git blob |
|---|---|
| `fpga/rtl/pll.qip` | `22278c8f7c5338a3b70fb43ddad19d9ca3a86f80` |
| `fpga/rtl/pll.v` | `6446867ebec35c20f136ce5f321344a6c559b156` |
| `fpga/rtl/pll/pll_0002.qip` | `aec45eb73ea83ceab9b5ad1b7d71b5869c92036f` |
| `fpga/rtl/pll/pll_0002.v` | `c599468749fd26ad95fca4593c869fbeace4572f` |

The generated IP consumes the canonical 50 MHz MiSTer core clock and emits a
20 MHz core/video clock plus `locked`. `.gitattributes` marks these paths
`-text` so Git for Windows cannot alter the pinned blobs.

Do not customise these generated files. A future Zhaozhou PLL belongs under a
separately named platform module and must replace the QIP reference as an
explicit board-target change.
