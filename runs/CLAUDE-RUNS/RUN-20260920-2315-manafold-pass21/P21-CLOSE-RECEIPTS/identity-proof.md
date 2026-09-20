# The two instrument repairs moved NO rendered byte

The proof is a full 22-subject CRC walk from TWO binaries, one built from
`528cc15f` in a detached worktree and one from the repaired tree, both with
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` and the shipping `rods` rig.

    zhao-reel-cel --crc <the 22 kU02LiveSiteSubjects>

Both runs produced **8,062 lines**. `diff` reports exactly ONE differing line,
and it is the marker this worker appended to tell the two files apart
(`FIXED` vs `BASE`). Every frame count, every unique-colour count, every
`sequence_crc32c` and every live-history line is identical.

    8062c8062
    < FIXED
    ---
    > BASE

`--crc` renders in memory and writes no frames, so this costs no disk -- the
mechanism the pass-21 implementation added for exactly this check.

⚠ ONE LINE IS VISUALLY MANGLED IN BOTH FILES AND IT IS NOT A FAULT.
`manafold-death-drop`'s CRC line is interleaved with a `projected_radius_q8`
diagnostic written to the other stream, so it reads
`sequence_crccelmain projected_radius_q8 ...`. It is mangled IDENTICALLY in both
files, which is why the diff is still clean; the subject's digest is recorded
properly in the bank manifest, from the frames themselves.

Frame totals add up to the bank's 7,992 across the 22 subjects, so no subject
was silently skipped by either binary.
