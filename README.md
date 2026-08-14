# Zhaozhou

A game console. FPGA-based hardware.

## Status

Scaffold. The hardware architecture spec is being written separately and will land in
`docs/`. Nothing here is designed yet — no ISA, no memory map, no RTL. Those come from
the spec, not from guesses.

## Layout

```
docs/            architecture and hardware specs (pending)
rtl/             HDL sources
sim/             testbenches
constraints/     board pin assignments
tools/           build and image helpers
```

## Related

- [Nanquan](../nanquan) — the programming language for this console
- [untitled-game](../untitled-game) — a game that runs on it
