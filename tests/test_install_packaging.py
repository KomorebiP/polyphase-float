#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Build, relocate, and consume a nested-libdir static installation."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def run(command: list[str], *, env: dict[str, str] | None = None,
        expected_success: bool = True) -> str:
    result = subprocess.run(command, text=True, capture_output=True, env=env)
    if (result.returncode == 0) != expected_success:
        raise RuntimeError(
            f"unexpected exit status {result.returncode}: {shlex.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--ctest", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--gmp-include", required=True)
    parser.add_argument("--gmp-library", required=True)
    parser.add_argument("--mpfr-include", default="")
    parser.add_argument("--mpfr-library", default="")
    parser.add_argument("--pkg-config", default="")
    parser.add_argument("--with-mpfr", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()

    with tempfile.TemporaryDirectory(prefix="ppf-install-packaging-") as temp:
        work = Path(temp)
        build = work / "build"
        initial_prefix = work / "original-prefix"
        prefix = work / "relocated-prefix"
        libdir = "lib/x86_64-linux-gnu"
        configure = [
            args.cmake, "-S", str(root), "-B", str(build),
            "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_C_COMPILER={args.cc}",
            f"-DCMAKE_INSTALL_PREFIX={initial_prefix}",
            f"-DCMAKE_INSTALL_LIBDIR={libdir}",
            "-DPPF_BUILD_SHARED=OFF", "-DPPF_BUILD_STATIC=ON",
            "-DBUILD_TESTING=OFF", "-DPPF_BUILD_EXAMPLES=OFF",
            "-DPPF_BUILD_BENCHMARKS=OFF", "-DPPF_OPENMP=OFF",
            f"-DPPF_MPFR_SUPPORT={'ON' if args.with_mpfr else 'OFF'}",
            f"-DGMP_INCLUDE_DIR={args.gmp_include}",
            f"-DGMP_LIBRARY={args.gmp_library}",
        ]
        if args.with_mpfr:
            configure += [f"-DMPFR_INCLUDE_DIR={args.mpfr_include}",
                          f"-DMPFR_LIBRARY={args.mpfr_library}"]
        run(configure)
        run([args.cmake, "--build", str(build), "--parallel", "2"])
        run([args.cmake, "--install", str(build)])
        shutil.move(str(initial_prefix), str(prefix))

        # The installed reader route must retain the same relative paths as
        # the source tree, including the first example and machine contracts.
        documentation = prefix / "share/doc/PolyphaseFloat"
        for relative in (
            "README.md", "README_FIRST.txt", "docs/BUILDING.md",
            "docs/TESTING.md", "docs/ABI_POLICY.md",
            "docs/COMPLETION_SCOPE_0.20.0.md", "examples/basic.c",
            "docs/STABILITY_POLICY.md", "docs/FUZZING_POLICY.md",
            "docs/NUMERICAL_LIMITS.md", "docs/manual/README.md",
            "docs/manual/Polyphase_Float_Manual.pdf",
            "docs/manual/Polyphase_Float_Manual.html",
            "abi/contract-v1.json", "semantics/contract-v1.json",
        ):
            installed = documentation / relative
            if not installed.is_file() or installed.read_bytes() != (root / relative).read_bytes():
                raise RuntimeError(f"installed documentation missing or changed: {relative}")

        # The manual's local links depend on its chapters, figures, and reader
        # resources keeping their source-relative locations after relocation.
        for source in (root / "docs/manual").rglob("*"):
            if not source.is_file():
                continue
            relative = source.relative_to(root)
            installed = documentation / relative
            if not installed.is_file() or installed.read_bytes() != source.read_bytes():
                raise RuntimeError(f"installed manual resource missing or changed: {relative}")

        def consume(name: str, *, components: str = "",
                    disable_mpfr: bool = False,
                    optional_components: str = "",
                    missing_mpfr_header: bool = False,
                    minimum_version: str = "",
                    expected_success: bool = True) -> None:
            consumer_build = work / name
            command = [
                args.cmake, "-S", str(root / "tests/install_consumer"),
                "-B", str(consumer_build), f"-DCMAKE_C_COMPILER={args.cc}",
                f"-DCMAKE_PREFIX_PATH={prefix}",
                f"-DGMP_INCLUDE_DIR={args.gmp_include}",
                f"-DGMP_LIBRARY={args.gmp_library}",
                "-DPPF_CONSUMER_FORCE_STATIC=ON",
            ]
            if components:
                command.append(f"-DPPF_CONSUMER_COMPONENTS={components}")
            if optional_components:
                command.append(f"-DPPF_CONSUMER_OPTIONAL_COMPONENTS={optional_components}")
            if minimum_version:
                command.append(f"-DPPF_CONSUMER_MIN_VERSION={minimum_version}")
            if disable_mpfr:
                command.append("-DCMAKE_DISABLE_FIND_PACKAGE_MPFR=TRUE")
            elif args.with_mpfr:
                command += [f"-DMPFR_INCLUDE_DIR={args.mpfr_include}",
                            f"-DMPFR_LIBRARY={args.mpfr_library}"]
            if missing_mpfr_header:
                command.append(f"-DMPFR_INCLUDE_DIR={work / 'missing-mpfr-header'}")
            run(command, expected_success=expected_success)
            if expected_success:
                run([args.cmake, "--build", str(consumer_build), "--parallel", "2"])
                run([args.ctest, "--test-dir", str(consumer_build), "--output-on-failure"])

        consume("consumer-core", components="core", disable_mpfr=True)
        consume("consumer-optional-mpfr", components="core",
                optional_components="mpfr", disable_mpfr=True)
        consume("consumer-epoch-baseline", components="core",
                disable_mpfr=True, minimum_version="0.11.0")
        consume("consumer-before-epoch", components="core",
                disable_mpfr=True, minimum_version="0.10.0",
                expected_success=False)
        consume("consumer-default")
        consume("consumer-mpfr", components="mpfr",
                expected_success=args.with_mpfr)
        if args.with_mpfr:
            consume("consumer-mpfr-missing-header", components="mpfr",
                    missing_mpfr_header=True, expected_success=False)
            consume("consumer-optional-missing-header", components="core",
                    optional_components="mpfr", missing_mpfr_header=True)

        pkg_config = args.pkg_config or shutil.which("pkg-config")
        if pkg_config:
            env = os.environ.copy()
            pkg_dir = str(prefix / libdir / "pkgconfig")
            existing_pkg_path = env.get("PKG_CONFIG_PATH", "")
            env["PKG_CONFIG_PATH"] = (pkg_dir + os.pathsep + existing_pkg_path
                                      if existing_pkg_path else pkg_dir)
            for variable, expected in (
                ("prefix", prefix), ("libdir", prefix / libdir),
                ("includedir", prefix / "include"),
            ):
                actual = run([pkg_config, f"--variable={variable}",
                              "polyphase-float"], env=env).strip()
                if Path(actual).resolve() != expected.resolve():
                    raise RuntimeError(f"pkg-config {variable}: {actual} != {expected}")
            flags = run([pkg_config, "--static", "--cflags", "--libs",
                         "polyphase-float"], env=env)
            executable = work / "pkg-config-consumer"
            run([args.cc, str(root / "tests/install_consumer/main.c"),
                 "-o", str(executable), *shlex.split(flags)], env=env)
            run([str(executable)])
            if args.with_mpfr:
                flags = run([pkg_config, "--static", "--cflags", "--libs",
                             "polyphase-float-mpfr"], env=env)
                executable = work / "pkg-config-mpfr-consumer"
                run([args.cc, str(root / "tests/install_consumer/mpfr_main.c"),
                     "-o", str(executable), *shlex.split(flags)], env=env)
                run([str(executable)])
        else:
            print("pkg-config executable unavailable: CMake consumption tested only")
    print("nested-libdir relocation, installed documentation, and component consumption: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
