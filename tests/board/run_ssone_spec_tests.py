#!/usr/bin/env python3
"""Build and run the SuperStation real-block spec vectors in isolation."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORKSPACE = REPO.parent
SOURCES = (
    REPO / "fpga" / "rtl" / "common" / "zhao_crc32c_fold.sv",
    REPO / "fpga" / "rtl" / "raster" / "zhao_raster_fill.sv",
    REPO / "fpga" / "rtl" / "common" / "zhao_dual18_mul.sv",
    REPO / "fpga" / "rtl" / "platform" / "zhao_ssone_spec_tests.sv",
    REPO / "tests" / "board" / "ssone_spec_tests_tb.sv",
)


def tool_environment() -> tuple[dict[str, str], Path]:
    suite = WORKSPACE / ".tools" / "oss-cad-suite"
    verilator = suite / "bin" / ("verilator_bin.exe" if os.name == "nt" else "verilator_bin")
    if not verilator.is_file():
        raise RuntimeError(f"pinned verilator_bin executable was not found: {verilator}")

    env = os.environ.copy()
    env["VERILATOR_ROOT"] = (suite / "share" / "verilator").as_posix()
    prefixes = [suite / "bin", suite / "lib"]
    winlibs = WORKSPACE.parent / "dsstuff" / "mingw64" / "bin"
    if winlibs.is_dir():
        # The generated makefile needs winlibs C++/archive tools together, then
        # Git sh for uname and POSIX recipe syntax. This is the calibrated order
        # used by tests/dsp/run_dual18_verilator.py.
        prefixes.insert(0, winlibs)
        env["MAKE"] = "mingw32-make.exe"
        git_bash = Path(env.get("CLAUDE_CODE_GIT_BASH_PATH", ""))
        git_usr_bin = git_bash.parent.parent / "usr" / "bin"
        git_sh = git_usr_bin / "sh.exe"
        if git_sh.is_file():
            prefixes.append(git_usr_bin)
            env["SHELL"] = str(git_sh)
    env["PATH"] = os.pathsep.join(str(path) for path in prefixes) + os.pathsep + env.get("PATH", "")
    return env, verilator


def main() -> int:
    try:
        env, verilator = tool_environment()
        version = subprocess.run(
            [str(verilator), "--version"],
            cwd=REPO,
            env=env,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ).stdout.strip()
        print(f"SSONE_SPEC_TOOL {version}")

        with tempfile.TemporaryDirectory(prefix="ssone-spec-verilator-") as temp:
            mdir = Path(temp) / "obj"
            command = [
                str(verilator),
                "--binary",
                "--timing",
                "--assert",
                "-j",
                "4",
                "-Wno-fatal",
                "-Wno-DECLFILENAME",
                "-Wno-TIMESCALEMOD",
                "-Wno-PROCASSINIT",
                "-DZHAO_DUAL18_BEHAVIORAL=1",
                "--top-module",
                "tb_ssone_spec_tests",
                "--Mdir",
                str(mdir),
                "-CFLAGS",
                "-O2 -std=c++17",
                *map(str, SOURCES),
            ]
            build = subprocess.run(
                command,
                cwd=REPO,
                env=env,
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if build.returncode:
                print(build.stdout, file=sys.stderr)
                raise RuntimeError(f"Verilator build failed with exit code {build.returncode}")

            executable = mdir / ("Vtb_ssone_spec_tests.exe" if os.name == "nt" else "Vtb_ssone_spec_tests")
            run = subprocess.run(
                [str(executable)],
                cwd=REPO,
                env=env,
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            print(run.stdout, end="")
            if (
                run.returncode
                or "%Fatal" in run.stdout
                or "SSONE_SPEC_TESTS_PASS vectors=16 mutant_fail=1 signature=" not in run.stdout
            ):
                raise RuntimeError(f"spec-vector run failed with exit code {run.returncode}")
    except (OSError, subprocess.CalledProcessError, RuntimeError) as exc:
        print(f"SSONE_SPEC_RESULT FAIL: {exc}", file=sys.stderr)
        return 1

    print("SSONE_SPEC_RESULT PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
