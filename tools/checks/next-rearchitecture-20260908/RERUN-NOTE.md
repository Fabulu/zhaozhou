# Re-run of the bundled checks, in this tree

`bash run_checks.sh` on 2026-09-08, this machine:

```
N1 normalization/index: 16777216 denominators PASS; zero-convention mutant detected
F1 fog channel: 16777216 combinations PASS; truncation mutant mismatches=8042336; /255 mutant mismatches=2796160
I1 identity layouts: 262144 encodings PASS (including invalid sample=3); stale slot mutant=196608
L1 row-local ISSUE plus simultaneous AUX: 4096 scalar truth cases PASS
J1 credited 3-cycle read join: 32 schedules x 10000 cycles PASS; retired=133137; max reserved=4
A1 admission fork: 8 truth cases PASS; ungated valid mutant detected
Z1 abstract zero-work lease witness: NOT an RTL reachability proof
E1 barrier conjunction: 512 truth cases PASS; one withheld ACK cannot reopen
ALL INDEPENDENT CHECKS PASSED.
```

`sha256sum -c SHA256SUMS.txt` verifies all six files, and the bundled brief is
byte-identical to the copy delivered separately.

**Every check carries a mutant it detects.** That is the standard this
repository asks for -- a detector that has not been shown to fire has not been
tested -- and it is worth noting that the bundle applies it to itself.

**What these are not.** Arithmetic and protocol models. No RTL simulation, no
Quartus, no board. The bundle says so itself and Z1 labels its own limit in its
output line. They do not corroborate any resource or timing claim.
