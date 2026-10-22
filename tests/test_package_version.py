#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Exercise the installed version policy through real CMake find_package calls.

Fixtures use the production generator but need no compiler or arithmetic
library.  Future 1.x fixtures catch package compatibility regressions before a
project-version change.  Actual installation/relocation is tested separately
by test_install_packaging.py.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def bracket(value: str) -> str:
    if "]==]" in value:
        raise ValueError("unsupported CMake bracket delimiter in path")
    return "[==[" + value + "]==]"


def run(cmake: str, script: Path, expect_success: bool = True) -> str:
    result = subprocess.run(
        [cmake, "-P", str(script)], text=True, capture_output=True, check=False
    )
    if (result.returncode == 0) != expect_success:
        raise AssertionError(
            f"{script.name}: exit {result.returncode}\n"
            + result.stdout + result.stderr
        )
    return result.stdout + result.stderr


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    root = args.root.resolve()
    helper = root / "cmake" / "PolyphasePackageVersion.cmake"
    if not helper.is_file():
        raise AssertionError(f"missing production generator: {helper}")

    # provider, request (including optional EXACT), expected acceptance.
    # Deliberately test full three-component EXACT versions: CMake's standard
    # version helpers compare the requested and provided strings for EXACT.
    cases = [
        ("0.20.0", "", True),
        ("0.20.0", "0.11.0", True),
        ("0.20.0", "0.19.0", True),
        ("0.20.0", "0.20.0", True),
        ("0.20.0", "0.20.1", False),
        ("0.20.0", "0.10.0", False),
        ("0.20.0", "1.0.0", False),
        ("0.20.0", "2.0.0", False),
        ("0.20.0", "0.20.0 EXACT", True),
        ("0.20.0", "0.19.0 EXACT", False),
        ("0.20.0", "0.11.0...0.20.0", True),
        ("0.20.0", "0.11.0...<0.20.0", False),
        ("0.20.0", "0.11.0...<1.0.0", True),
        ("0.20.0", "0.10.0...0.20.0", False),
        ("1.0.0", "", True),
        ("1.0.0", "0.11.0", True),
        ("1.0.0", "0.20.0", True),
        ("1.0.0", "1.0.0", True),
        ("1.0.0", "0.10.0", False),
        ("1.0.0", "1.0.1", False),
        ("1.0.0", "1.1.0", False),
        ("1.0.0", "2.0.0", False),
        ("1.0.0", "1.0.0 EXACT", True),
        ("1.0.0", "0.20.0 EXACT", False),
        ("1.0.0", "0.11.0...0.20.0", False),
        ("1.0.0", "0.11.0...<1.0.0", False),
        ("1.0.0", "0.11.0...1.0.0", True),
        ("1.0.0", "0.11.0...<2.0.0", True),
        ("1.0.0", "1.0.0...<2.0.0", True),
        ("1.0.0", "1.1.0...<2.0.0", False),
        ("1.0.0", "0.10.0...<2.0.0", False),
        ("1.4.0", "0.11.0", True),
        ("1.4.0", "0.20.0", True),
        ("1.4.0", "1.0.0", True),
        ("1.4.0", "1.4.0", True),
        ("1.4.0", "1.4.1", False),
        ("1.4.0", "2.0.0", False),
        ("1.4.0", "1.4.0 EXACT", True),
        ("1.4.0", "1.0.0 EXACT", False),
        ("1.4.0", "0.20.0 EXACT", False),
        ("1.4.0", "0.11.0...<1.0.0", False),
        ("1.4.0", "0.11.0...1.4.0", True),
        ("1.4.0", "1.0.0...<1.4.0", False),
        ("1.4.0", "1.0.0...<2.0.0", True),
    ]
    count = 0
    with tempfile.TemporaryDirectory(prefix="ppf-package-version-") as temp:
        work = Path(temp)
        for version in ("0.20.0", "1.0.0", "1.4.0", "2.0.0"):
            fixture = work / version
            fixture.mkdir()
            script = work / f"generate-{version}.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.23)\n"
                "set(CMAKE_SIZEOF_VOID_P 8)\n"
                f"include({bracket(str(helper))})\n"
                f"ppf_write_package_version_files({bracket(str(fixture))} "
                f'"{version}")\n', encoding="utf-8"
            )
            output = run(args.cmake, script, version != "2.0.0")
            if version == "2.0.0":
                if "needs an explicit package compatibility policy" not in output:
                    raise AssertionError("unsupported-major failure was unrelated")
                continue
            (fixture / "PolyphaseFloatConfig.cmake").write_text(
                "set(PolyphaseFloat_POLICY_FIXTURE TRUE)\n", encoding="utf-8"
            )

        # No pointer-size variable models LANGUAGES NONE.  The governed floor
        # still applies even when CMake cannot perform an architecture check.
        for pointer_size in (8, None):
            for provider, request, expected in cases:
                count += 1
                script = work / f"case-{count}.cmake"
                pointer_line = (f"set(CMAKE_SIZEOF_VOID_P {pointer_size})\n"
                                if pointer_size else "unset(CMAKE_SIZEOF_VOID_P)\n")
                script.write_text(
                    "cmake_minimum_required(VERSION 3.23)\n" + pointer_line
                    + f"find_package(PolyphaseFloat {request} CONFIG QUIET "
                    + f"PATHS {bracket(str(work / provider))} NO_DEFAULT_PATH)\n"
                    + f"if(NOT PolyphaseFloat_FOUND STREQUAL \"{int(expected)}\")\n"
                    + f'  message(FATAL_ERROR "provider={provider}, request={request}, '
                    + f'pointer={pointer_size}: expected {int(expected)}, '
                    + 'got ${PolyphaseFloat_FOUND}")\nendif()\n', encoding="utf-8"
                )
                run(args.cmake, script)

        # A requested version must not admit an incompatible pointer size.
        # Unversioned find_package does not consult a ConfigVersion file.
        for provider in ("0.20.0", "1.0.0", "1.4.0"):
            count += 1
            script = work / f"case-{count}.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.23)\n"
                "set(CMAKE_SIZEOF_VOID_P 4)\n"
                "find_package(PolyphaseFloat 0.11.0 CONFIG QUIET "
                f"PATHS {bracket(str(work / provider))} NO_DEFAULT_PATH)\n"
                "if(PolyphaseFloat_FOUND)\n"
                '  message(FATAL_ERROR "architecture mismatch accepted")\n'
                "endif()\n", encoding="utf-8"
            )
            run(args.cmake, script)

    print(f"package version policy: {count} find_package cases passed; "
          "unsupported future provider rejected")


if __name__ == "__main__":
    main()
