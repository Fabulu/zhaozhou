# Manafold Pass 15 archive integrity

Before Pass 16 overwrites the live Manafold media, the 28 Pass 15 WebM files are preserved under `website/public/renders/archive-pass15-manafold-*.webm` and declared by the `Pass 15 — 2026-09-10` archive group in `website/creatures.json`.

2026-09-18 verification:

- archive declarations: **28**;
- corresponding live Pass 15 sources present: **28**;
- archive files present: **28**;
- SHA-256-identical source/archive pairs: **28/28**;
- total archived source bytes checked: **66,390,717**;
- archive commit: Upheaval `830c46c` (`Archive Manafold Pass 15 and prepare Pass 16 card`);
- remote containment: `origin/manafold-pass16` contains `830c46c`.

This establishes that the historical generation is preserved byte-for-byte before the Pass 16 encode replaces live media.
