ZHAOZHOU TEXTURE ISLAND — NEXT REARCHITECTURE RESEARCH AND CHECKS
8 September 2026

CONTENTS
  ZHAOZHOU_TEXTURE_ISLAND_NEXT_REARCHITECTURE_2026-09-08.txt
    Architecture decisions, source findings, implementation packets,
    verification/fitting gates, source index and closing branch refresh.
  source_manifest.json
    Immutable repository pins/paths and primary vendor references.
  independent_checks.cpp
    Self-contained C++17 finite-domain arithmetic and abstract protocol models.
  independent_checks.log
    Output of the independently executed checks in this session.
  run_checks.sh
    Compile and run the checks in a temporary directory; no repo needed.
  SHA256SUMS.txt
    File hashes, excluding this hash manifest itself.

RUN THE CHECKS
  Requires a C++17 compiler (g++ by default) and Bash:
    bash run_checks.sh
  An alternative compiler can be selected with CXX, for example:
    CXX=clang++ bash run_checks.sh

  Or compile directly on another platform with equivalent C++17 options:
    g++ -std=c++17 -O2 -Wall -Wextra -Werror independent_checks.cpp -o checks
    ./checks

WHAT WAS EXECUTED
  Exhaustive 24-bit normalization/index comparison (16,777,216 inputs),
  exhaustive fog channel arithmetic (16,777,216 triples), full encoded
  identity-space checks (262,144 combinations), row-local ISSUE truth cases,
  credited read-join schedules, admission-fork and barrier truth tables.
  Deliberately broken variants exercise the corresponding detectors.
  The C++ models were rebuilt and rerun successfully before packaging.

WHAT WAS NOT EXECUTED
  No repository RTL simulation, Quartus build, FPGA programming, hardware test
  or unbounded formal proof. Repository test and fit results in the brief are
  read from the checked-in evidence and explicitly distinguished from these
  independent models.

  The zero-work example is an ABSTRACT protocol witness. It demonstrates why
  a front-end-completion obligation matters; it does NOT prove that the actual
  repository reaches the same destructive reuse trace. The brief requires an
  RTL proof/test or a justified lease before production acceptance.

PINS AND SCOPE
  Primary RTL: 7cbeec13ec9efe649087a0f85186ac4e95b2e31b (06:02:28 CEST).
  Gate/fit refresh: e1622b2b1dada677cbcf3591431413721af49cf9 (06:15:25 CEST).
  Closing report: 3e666f8348f38469ae05b87e940e8db662155755 (06:34:32 CEST).
  All times 8 September 2026. The refresh commits do not change the inspected
  V3 datapath. No completed V3 physical fit was supplied in those refreshes.

No repository files were modified. No compiler binaries or font files are
included. This bundle contains recommendations and check models, not a ready-
to-merge RTL implementation or a guarantee of resource/timing closure.
