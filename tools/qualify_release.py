#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run bounded, offline release qualification and preserve exact execution records."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCOPES = {
    "gcc-serial": "Required strict GCC build and complete MPFR-enabled CTest suite",
    "clang-serial": "Required strict Clang build and complete MPFR-enabled CTest suite",
    "gcc-openmp": "Required strict GCC/OpenMP build and complete MPFR-enabled CTest suite",
    "asan": "Optional GCC AddressSanitizer/UndefinedBehaviorSanitizer hardening suite",
    "fuzz": "Optional isolated Clang static-core build and two concurrent libFuzzer campaigns",
}
REQUIRED = {"gcc-serial", "clang-serial", "gcc-openmp"}
ALIASES = {"gcc": "gcc-serial", "clang": "clang-serial", "openmp": "gcc-openmp"}
EXCLUDED = {".git", "build", "bin", "lib", "stage", "dist", "__pycache__", ".cache"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def snapshot(root: Path) -> dict[str, str]:
    return {
        p.relative_to(root).as_posix(): digest(p)
        for p in sorted(root.rglob("*"))
        if p.is_file()
        and not any(x in EXCLUDED or x.startswith("build-") for x in p.relative_to(root).parts)
        and p.suffix not in {".pyc", ".o", ".a", ".so"}
    }


def write_json(path: Path, data: object) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def describe_tool(name: str, args: list[str]) -> dict[str, object]:
    executable = shutil.which(name)
    result: dict[str, object] = {"executable": executable, "command": [name, *args]}
    if not executable:
        result["status"] = "UNAVAILABLE"
        return result
    try:
        completed = subprocess.run(
            [executable, *args], capture_output=True, text=True, timeout=20, check=False
        )
        result.update(
            status="PASS" if completed.returncode == 0 else "FAIL",
            exit_code=completed.returncode,
            stdout=completed.stdout.strip(),
            stderr=completed.stderr.strip(),
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        result.update(status="UNAVAILABLE", error=str(error))
    return result


def parse_cmake_cache(text: str) -> dict[str, str]:
    """Read each CMake cache assignment without allowing blank lines in keys."""
    result = {}
    for line in text.splitlines():
        match = re.match(r"^([^/#][^:=]*):[^=]*=(.*)$", line)
        if match:
            result[match.group(1)] = match.group(2)
    return result


def configure_command(scope: str, source: Path, build: Path, args: argparse.Namespace) -> list[str]:
    compiler = args.clang if scope in {"clang-serial", "fuzz"} else args.gcc
    command = [
        args.cmake, "-S", str(source), "-B", str(build),
        "-DCMAKE_C_COMPILER=" + compiler,
        "-DCMAKE_BUILD_TYPE=" + ("RelWithDebInfo" if scope in {"asan", "fuzz"} else "Release"),
        "-DCMAKE_C_EXTENSIONS=OFF", "-DPPF_WARNINGS_AS_ERRORS=ON",
        "-DPPF_ENABLE_NATIVE_OPTIMIZATIONS=OFF",
        "-DPPF_OPENMP=" + ("ON" if scope == "gcc-openmp" else "OFF"),
        "-DPPF_MPFR_SUPPORT=" + ("OFF" if scope == "fuzz" else "ON"),
        "-DPPF_BUILD_BENCHMARKS=OFF",
    ]
    if scope == "fuzz":
        command.extend([
            "-DBUILD_TESTING=OFF", "-DPPF_BUILD_EXAMPLES=OFF",
            "-DPPF_BUILD_SHARED=OFF", "-DPPF_BUILD_STATIC=ON", "-DPPF_BUILD_FUZZERS=ON",
        ])
    else:
        command.extend([
            "-DBUILD_TESTING=ON", "-DPPF_BUILD_EXAMPLES=ON",
            "-DPPF_BUILD_SHARED=ON", "-DPPF_BUILD_STATIC=ON", "-DPPF_BUILD_FUZZERS=OFF",
        ])
    if scope == "asan":
        command.extend([
            "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer",
            "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O1 -g",
            "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined",
            "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined",
        ])
    return command


def test_command(scope: str, build: Path, args: argparse.Namespace) -> list[str]:
    command = [args.ctest, "--test-dir", str(build), "--output-on-failure", "--no-tests=error", "-j1"]
    if scope == "asan":
        command.extend(["-L", "hardening", "-E", "^abi_exports$"])
    return command


def corpus_paths(run: Path, source: Path, name: str) -> tuple[Path, Path, Path]:
    return run / "corpus" / name, source / "fuzz" / "corpus" / name, run / "artifacts" / name


def fuzz_command(run: Path, source: Path, build: Path, name: str, args: argparse.Namespace) -> list[str]:
    target, seed = ("fuzz_text_parser", args.text_seed) if name == "text" else ("fuzz_fpif_import", args.fpif_seed)
    mutable, immutable, artifacts = corpus_paths(run, source, name)
    return [
        str(build / target), str(mutable), str(immutable),
        "-seed=" + str(seed), "-max_total_time=" + str(args.fuzz_seconds),
        "-timeout=" + str(args.fuzz_case_timeout), "-max_len=" + str(args.fuzz_max_len),
        "-artifact_prefix=" + str(artifacts) + os.sep,
    ]


def plan(source: Path, run: Path, selected: list[str], args: argparse.Namespace) -> dict[str, object]:
    scopes = {}
    for scope in selected:
        build = run / "build" / scope
        steps = [configure_command(scope, source, build, args), [args.cmake, "--build", str(build), "-j", str(args.jobs)]]
        if scope == "fuzz":
            steps.extend(fuzz_command(run, source, build, name, args) for name in ("text", "fpif"))
        else:
            steps.append(test_command(scope, build, args))
        scopes[scope] = {"required": scope in REQUIRED, "commands": steps}
    return {"source": str(source), "run_directory": str(run), "scopes": scopes, "executes_commands": False}


class Qualification:
    def __init__(self, source: Path, run: Path, args: argparse.Namespace):
        self.source, self.run, self.args = source, run, args
        self.environment = {"PYTHONDONTWRITEBYTECODE": "1"}
        self.receipt: dict[str, object] = {
            "schema": "polyphase-float.qualification/1", "started_utc": utc_now(),
            "source": str(source), "run_directory": str(run),
            "platform": {"system": platform.platform(), "machine": platform.machine(), "python": sys.version},
            "scope": "Local execution of the selected bounded checks on this source snapshot; no independent external replication or universal correctness claim.",
            "source_snapshot": "source_snapshot.json", "scopes": {},
            "limits": ["Tests cover their exercised cases and recorded contracts.", "GMP allocation behavior is outside project allocator rollback."],
        }
        if not args.detect_leaks:
            self.receipt["limits"].append("LeakSanitizer is disabled by explicit ASAN_OPTIONS=detect_leaks=0.")
        self.receipt["limits"].append("The ASan hardening scope excludes the Python ctypes-based abi_exports check. The three ordinary compiler suites retain that check; sanitizer C ABI/runtime tests remain enabled.")

    def step(self, scope: str, name: str, command: list[str], *, environment: dict[str, str] | None = None, timeout: int | None = None) -> dict[str, object]:
        directory = self.run / "logs" / scope
        directory.mkdir(parents=True, exist_ok=True)
        log = directory / (name + ".log")
        override = {**self.environment, **(environment or {})}
        entry: dict[str, object] = {
            "name": name, "command": command, "cwd": str(self.run),
            "environment_overrides": override, "started_utc": utc_now(),
            "log": str(log.relative_to(self.run)),
        }
        started = time.monotonic()
        print("[" + scope + "] " + shlex.join(command), flush=True)
        try:
            with log.open("wb") as stream:
                completed = subprocess.run(command, cwd=self.run, env={**os.environ, **override}, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout, check=False)
            entry.update(status="PASS" if completed.returncode == 0 else "FAIL", exit_code=completed.returncode)
        except subprocess.TimeoutExpired:
            entry.update(status="FAIL", exit_code=None, error="Command exceeded timeout", timeout_seconds=timeout)
        except OSError as error:
            entry.update(status="FAIL", exit_code=None, error=str(error))
        entry.update(elapsed_seconds=round(time.monotonic() - started, 6), completed_utc=utc_now())
        if log.exists():
            entry["log_sha256"] = digest(log)
            if name == "ctest":
                text = log.read_text(encoding="utf-8", errors="replace")
                summary = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", text)
                if summary:
                    failed, total = int(summary.group(2)), int(summary.group(3))
                    entry["test_counts"] = {"total": total, "passed": total - failed, "failed": failed}
        return entry

    def scope(self, scope: str) -> dict[str, object]:
        entry: dict[str, object] = {"required": scope in REQUIRED, "status": "NOT_RUN", "steps": []}
        compiler = self.args.clang if scope in {"clang-serial", "fuzz"} else self.args.gcc
        needed = [self.args.cmake, compiler] + ([] if scope == "fuzz" else [self.args.ctest])
        missing = [name for name in needed if shutil.which(name) is None]
        if missing:
            entry.update(status="FAIL" if scope in REQUIRED else "NOT_RUN", reason="Unavailable prerequisites: " + ", ".join(missing))
            return entry
        if scope == "fuzz" and platform.system() != "Linux":
            entry["reason"] = "Current PPFIF fuzzer requires Linux fmemopen"
            return entry
        sanitizer_env = {"ASAN_OPTIONS": "detect_leaks=" + str(self.args.detect_leaks) + ":abort_on_error=1", "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
        if scope == "fuzz":
            probe = self.run / "fuzzer_runtime_probe.c"
            probe.write_text("#include <stddef.h>\n#include <stdint.h>\nint LLVMFuzzerTestOneInput(const uint8_t *p, size_t n) {(void)p; (void)n; return 0;}\n", encoding="utf-8")
            check = self.step(scope, "runtime_probe", [compiler, str(probe), "-fsanitize=fuzzer,address,undefined", "-o", str(self.run / "fuzzer_runtime_probe")])
            entry["steps"].append(check)
            if check["status"] != "PASS":
                entry["reason"] = "Optional Clang/libFuzzer sanitizer runtime probe failed; see recorded compiler output"
                return entry
        build = self.run / "build" / scope
        configure = self.step(scope, "configure", configure_command(scope, self.source, build, self.args))
        entry["steps"].append(configure)
        if configure["status"] != "PASS":
            entry["status"] = "FAIL"
            return entry
        cache = parse_cmake_cache((build / "CMakeCache.txt").read_text(encoding="utf-8"))
        entry["dependencies"] = {key: cache.get(key) for key in ("GMP_INCLUDE_DIR", "GMP_LIBRARY", "MPFR_INCLUDE_DIR", "MPFR_LIBRARY", "PC_GMP_VERSION", "PC_MPFR_VERSION")}
        if scope != "fuzz" and not (Path(cache.get("MPFR_INCLUDE_DIR", "")) / "mpfr.h").is_file():
            entry.update(status="FAIL", reason="Qualification requires the official MPFR development header, not the minimal compatibility ABI")
            return entry
        built = self.step(scope, "build", [self.args.cmake, "--build", str(build), "-j", str(self.args.jobs)])
        entry["steps"].append(built)
        if built["status"] != "PASS":
            entry["status"] = "FAIL"
            return entry
        if scope == "fuzz":
            for name in ("text", "fpif"):
                mutable, immutable, artifacts = corpus_paths(self.run, self.source, name)
                if not immutable.is_dir() or not any(immutable.iterdir()):
                    entry.update(status="FAIL", reason="Missing nonempty immutable seed corpus: " + str(immutable))
                    return entry
                mutable.mkdir(parents=True, exist_ok=True)
                artifacts.mkdir(parents=True, exist_ok=True)
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                futures = [pool.submit(self.step, scope, name, fuzz_command(self.run, self.source, build, name, self.args), environment=sanitizer_env, timeout=self.args.fuzz_seconds + self.args.fuzz_case_timeout + 120) for name in ("text", "fpif")]
                entry["steps"].extend(future.result() for future in futures)
            entry["campaign_seconds_per_target"] = self.args.fuzz_seconds
            entry["seeds"] = {"text": self.args.text_seed, "fpif": self.args.fpif_seed}
        else:
            environment = sanitizer_env if scope == "asan" else {}
            if scope == "gcc-openmp":
                environment["OMP_NUM_THREADS"] = str(self.args.openmp_threads)
            entry["steps"].append(self.step(scope, "ctest", test_command(scope, build, self.args), environment=environment))
        entry["status"] = "PASS" if all(step["status"] == "PASS" for step in entry["steps"]) else "FAIL"
        return entry


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, help="Evidence/build parent directory outside the source tree")
    parser.add_argument("--list", action="store_true", help="List scopes without executing or writing files")
    parser.add_argument("--plan", action="store_true", help="Print command plan without executing or writing files")
    parser.add_argument("--only", nargs="+", choices=[*SCOPES, *ALIASES], help="Execute only these scopes; gcc/clang/openmp are accepted aliases")
    parser.add_argument("--with-sanitizers", action="store_true", help="Add optional GCC ASan/UBSan hardening checks")
    parser.add_argument("--skip-fuzz", action="store_true", help="Do not run fuzzers; record that scope as NOT_RUN")
    parser.add_argument("--fuzz-seconds", type=int, default=600)
    parser.add_argument("--fuzz-case-timeout", type=int, default=30)
    parser.add_argument("--fuzz-max-len", type=int, default=8192)
    parser.add_argument("--text-seed", type=int, default=2026102201)
    parser.add_argument("--fpif-seed", type=int, default=2026102202)
    parser.add_argument("--detect-leaks", type=int, choices=(0, 1), default=0)
    parser.add_argument("--openmp-threads", type=int, default=4)
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--gcc", default="gcc")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ctest", default="ctest")
    args = parser.parse_args()
    if args.list:
        print(json.dumps(SCOPES, indent=2))
        return 0
    if args.output is None:
        parser.error("--output is required except with --list")
    source, output = args.source.resolve(), args.output.resolve()
    if not (source / "CMakeLists.txt").is_file():
        parser.error("--source must be a Polyphase Float source directory")
    if output == source or source in output.parents:
        parser.error("--output must be outside the source tree")
    if min(args.jobs, args.openmp_threads, args.fuzz_seconds, args.fuzz_case_timeout, args.fuzz_max_len) < 1:
        parser.error("jobs, thread counts, fuzz duration, timeout and maximum length must be positive")
    if args.text_seed == args.fpif_seed or not all(0 < x < 2**32 for x in (args.text_seed, args.fpif_seed)):
        parser.error("fuzzer seeds must be distinct positive 32-bit integers")
    if args.only and args.skip_fuzz and "fuzz" in args.only:
        parser.error("--only fuzz conflicts with --skip-fuzz")
    selected = list(dict.fromkeys(ALIASES.get(scope, scope) for scope in (args.only or ["gcc-serial", "clang-serial", "gcc-openmp", *(["asan"] if args.with_sanitizers else []), *([] if args.skip_fuzz else ["fuzz"])])))
    run = output / ("plan" if args.plan else datetime.now(timezone.utc).strftime("qualification_%Y%m%dT%H%M%S_%fZ"))
    if args.plan:
        print(json.dumps(plan(source, run, selected, args), indent=2))
        return 0
    run.mkdir(parents=True, exist_ok=False)
    qualification = Qualification(source, run, args)
    receipt = qualification.receipt
    version = re.search(r"project\s*\(\s*PolyphaseFloat.*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", (source / "CMakeLists.txt").read_text(encoding="utf-8"), re.S)
    receipt["source_version"] = version.group(1) if version else None
    receipt["selected_scopes"] = selected
    receipt["tools"] = {name: describe_tool(tool, ["--version"]) for name, tool in (("gcc", args.gcc), ("clang", args.clang), ("cmake", args.cmake), ("ctest", args.ctest))}
    before = snapshot(source)
    receipt["source_snapshot_files"] = len(before)
    write_json(run / "source_snapshot.json", before)
    receipt["source_snapshot_sha256"] = digest(run / "source_snapshot.json")
    for scope in SCOPES:
        receipt["scopes"][scope] = {"status": "NOT_RUN", "required": scope in REQUIRED, "reason": "Not selected for this invocation"}
    interrupted = False
    try:
        for scope in selected:
            receipt["scopes"][scope] = qualification.scope(scope)
            write_json(run / "qualification.json", receipt)
    except KeyboardInterrupt:
        interrupted = True
        receipt["interrupted"] = True
    except Exception as error:
        receipt["runner_error"] = str(error)
    after = snapshot(source)
    changes = sorted(key for key in before.keys() | after.keys() if before.get(key) != after.get(key))
    receipt["source_unchanged"] = not changes
    receipt["changed_source_paths"] = changes
    completed = [scope for scope in selected if receipt["scopes"][scope]["status"] == "PASS"]
    failed = [scope for scope in selected if receipt["scopes"][scope]["status"] == "FAIL"]
    receipt["completed_scopes"] = completed
    receipt["required_matrix_complete"] = all(receipt["scopes"][scope]["status"] == "PASS" for scope in REQUIRED)
    receipt["status"] = "FAIL" if failed or changes or interrupted or "runner_error" in receipt else ("PASS_COMPLETED_SCOPES" if completed else "NOT_RUN")
    receipt["completed_utc"] = utc_now()
    write_json(run / "qualification.json", receipt)
    print(json.dumps({"status": receipt["status"], "completed_scopes": completed, "required_matrix_complete": receipt["required_matrix_complete"], "receipt": str(run / "qualification.json")}, indent=2))
    return 130 if interrupted else (1 if receipt["status"] == "FAIL" else 0)


if __name__ == "__main__":
    raise SystemExit(main())
