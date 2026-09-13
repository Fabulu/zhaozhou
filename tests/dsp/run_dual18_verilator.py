#!/usr/bin/env python3
"""Build and run the dual18 calibration directly with the pinned Verilator.

No CMake, shared build tree, or CTest state is touched.  Every model is built in
a temporary directory and removed.  The vendor backend is intentionally not
attempted here: it requires an Intel-supported encrypted-library simulator setup
which this machine does not provide.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORKSPACE = REPO.parent
WRAPPER = REPO / "fpga" / "rtl" / "common" / "zhao_dual18_mul.sv"
DISCRIMINATOR = REPO / "tests" / "rtl" / "dual18_physical_pack_discriminator.sv"
CE_MUTANT = REPO / "tests" / "mutants" / "dual18_ce_ignore_mutant.sv"
CPP = REPO / "tests" / "dsp" / "dual18_physical_pack_directed.cpp"


@dataclass(frozen=True)
class Case:
    name: str
    top: str
    cpp_define: str
    sources: tuple[Path, ...]
    signs: tuple[int, int, int, int] | None = None
    run_args: tuple[str, ...] = ()


CASES = (
    Case(
        "inferred_uu",
        "dual18_inferred_pair",
        "DUAL18_TOP_INFERRED",
        (DISCRIMINATOR,),
        (0, 0, 0, 0),
    ),
    Case(
        "explicit_uu",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (0, 0, 0, 0),
    ),
    Case(
        "explicit_ss",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (1, 1, 1, 1),
    ),
    Case(
        "explicit_su",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (1, 0, 1, 0),
    ),
    Case(
        "explicit_us",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (0, 1, 0, 1),
    ),
    Case(
        "explicit_uu_ss",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (0, 0, 1, 1),
    ),
    Case(
        "explicit_su_us",
        "dual18_explicit_pair",
        "DUAL18_TOP_EXPLICIT",
        (WRAPPER, DISCRIMINATOR),
        (1, 0, 0, 1),
    ),
    Case(
        "s32x18_exact",
        "dual18_s32x18_exact",
        "DUAL18_TOP_S32X18",
        (WRAPPER, DISCRIMINATOR),
    ),
    Case(
        "s32xu12_projector",
        "dual18_s32xu12_projector",
        "DUAL18_TOP_PROJECTOR",
        (WRAPPER, DISCRIMINATOR),
    ),
    Case(
        "ce_ignore_positive_control",
        "dual18_ce_ignore_mutant",
        "DUAL18_TOP_CE_MUTANT",
        (WRAPPER, CE_MUTANT),
        (0, 0, 0, 0),
        ("--control-only", "--expect-ce-failure"),
    ),
)


def tool_environment() -> tuple[dict[str, str], Path]:
    suite = WORKSPACE / ".tools" / "oss-cad-suite"
    verilator = suite / "bin" / ("verilator_bin.exe" if os.name == "nt" else "verilator_bin")
    if not verilator.is_file():
        found = shutil.which("verilator_bin.exe") or shutil.which("verilator_bin")
        if not found:
            raise RuntimeError("pinned verilator_bin executable was not found")
        verilator = Path(found)
        suite = verilator.parent.parent

    env = os.environ.copy()
    env["VERILATOR_ROOT"] = (suite / "share" / "verilator").as_posix()
    prefixes = [suite / "bin", suite / "lib"]
    winlibs = WORKSPACE.parent / "dsstuff" / "mingw64" / "bin"
    if winlibs.is_dir():
        # This order is load-bearing on Windows: the suite's archive tools and
        # winlibs C++ runtime are not an interchangeable ABI pair.
        prefixes.insert(0, winlibs)
        env["MAKE"] = "mingw32-make.exe"
    env["PATH"] = os.pathsep.join(str(path) for path in prefixes) + os.pathsep + env.get("PATH", "")
    return env, verilator


def run_checked(command: list[str], env: dict[str, str], context: str) -> str:
    completed = subprocess.run(
        command,
        cwd=REPO,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode:
        print(completed.stdout, file=sys.stderr)
        raise RuntimeError("%s failed with exit code %d" % (context, completed.returncode))
    return completed.stdout


def backend_selection_fire_tests(verilator: Path, env: dict[str, str]) -> None:
    base = [
        str(verilator),
        "--lint-only",
        "-Wno-fatal",
        "--top-module",
        "zhao_dual18_mul",
        str(WRAPPER),
    ]
    cases = [
        ("missing", [], "zhao_dual18_backend_not_selected"),
        (
            "conflict",
            ["-DZHAO_DUAL18_CYCLONEV=1", "-DZHAO_DUAL18_BEHAVIORAL=1"],
            "zhao_dual18_backend_conflict",
        ),
    ]
    for name, defines, detector in cases:
        completed = subprocess.run(
            base[:-1] + defines + base[-1:],
            cwd=REPO,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if completed.returncode == 0 or detector not in completed.stdout:
            print(completed.stdout, file=sys.stderr)
            raise RuntimeError("backend %s selection detector did not fire" % name)
        print("DUAL18_POSITIVE_CONTROL backend_%s_detector=FIRED" % name)


def build_case(case: Case, root: Path, verilator: Path, env: dict[str, str]) -> tuple[Path, str]:
    mdir = root / case.name
    mdir.mkdir()
    cflags = ["-O2", "-std=c++17", "-D%s=1" % case.cpp_define]
    command = [
        str(verilator),
        "--cc",
        "--exe",
        "--build",
        "-j",
        "4",
        "--assert",
        "-Wno-fatal",
        "-Wno-DECLFILENAME",
        "-DZHAO_DUAL18_BEHAVIORAL=1",
        "--top-module",
        case.top,
        "--Mdir",
        str(mdir),
        "-o",
        "dual18_test",
    ]
    if case.signs is not None:
        names = ("AX_SIGNED", "AY_SIGNED", "BX_SIGNED", "BY_SIGNED")
        test_names = ("TEST_AX_SIGNED", "TEST_AY_SIGNED", "TEST_BX_SIGNED", "TEST_BY_SIGNED")
        for name, value in zip(names, case.signs):
            command.append("-G%s=%d" % (name, value))
        for name, value in zip(test_names, case.signs):
            cflags.append("-D%s=%d" % (name, value))
    command += ["-CFLAGS", " ".join(cflags)]
    command += [str(source) for source in case.sources]
    command.append(str(CPP))
    build_output = run_checked(command, env, "Verilating %s" % case.name)
    executable = mdir / ("dual18_test.exe" if os.name == "nt" else "dual18_test")
    if not executable.is_file():
        # Windows may expose the generated PE path without showing its suffix to
        # a POSIX compatibility shell; accept only one exact fallback.
        fallback = mdir / "dual18_test"
        if not fallback.is_file():
            raise RuntimeError("built executable is missing for %s" % case.name)
        executable = fallback
    return executable, build_output


def run_model(executable: Path, args: tuple[str, ...], env: dict[str, str], name: str) -> str:
    output = run_checked([str(executable), *args], env, "running %s" % name)
    if "DUAL18_RESULT PASS" not in output:
        raise RuntimeError("%s did not emit its PASS marker" % name)
    for line in output.splitlines():
        if line.startswith("DUAL18_"):
            print("%s %s" % (name, line))
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--quick",
        action="store_true",
        help="run flow controls only (the default executes exhaustive/random arithmetic)",
    )
    args = parser.parse_args(argv)

    try:
        env, verilator = tool_environment()
        version = run_checked([str(verilator), "--version"], env, "reading Verilator version").strip()
        print("DUAL18_TOOL %s" % version)
        backend_selection_fire_tests(verilator, env)
        with tempfile.TemporaryDirectory(prefix="dual18-verilator-") as temp:
            root = Path(temp)
            executables: dict[str, Path] = {}
            for case in CASES:
                executable, _ = build_case(case, root, verilator, env)
                executables[case.name] = executable
                run_args = case.run_args
                if args.quick and not run_args:
                    if case.signs is not None:
                        run_args = ("--control-only",)
                    else:
                        # Wide-product modes have no control-only switch; their
                        # boundary/random corpus is already bounded.
                        run_args = ()
                run_model(executable, run_args, env, case.name)

            # A legal-vector lane swap is the functional checker's positive
            # control.  Reuse the already-built correct UU model; this mode
            # passes only if independent output comparisons detect the swap.
            run_model(
                executables["explicit_uu"],
                ("--control-only", "--inject-lane-swap"),
                env,
                "lane_swap_positive_control",
            )
    except RuntimeError as exc:
        print("DUAL18_DIRECT_REJECT: %s" % exc, file=sys.stderr)
        return 1

    print("DUAL18_DIRECT_RESULT PASS")
    print("DUAL18_VENDOR_GATE HOLD: no supported encrypted-library simulator was invoked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
