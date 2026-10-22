#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Compile and run the manual's complete examples, preserving commands and results."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time

sys.dont_write_bytecode = True


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def production_snapshot(source: Path) -> dict[str, str]:
    return {path.relative_to(source).as_posix(): sha256(path)
            for directory in ("src", "include", "compat", "adapter")
            for path in sorted((source / directory).rglob("*"))
            if path.is_file() and path.suffix in {".c", ".h"}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True,
                        help="New evidence/build directory outside the source tree")
    parser.add_argument("--cc", nargs="+", default=["cc"], help="C compilers to check")
    parser.add_argument("--core-library", type=Path, help="Reuse an existing static core")
    parser.add_argument("--mpfr-library", type=Path, help="Matching static MPFR adapter")
    parser.add_argument("--baseline-snapshot", type=Path,
                        help="Require all production C/header bytes to match this prior source map")
    parser.add_argument("--baseline-receipt", type=Path,
                        help="Record, without relabeling, the prior qualification receipt")
    parser.add_argument("--expected-core-sha256", help="Require this hash before reusing the core")
    parser.add_argument("--core-only", action="store_true", help="Explicitly omit MPFR examples")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--report", type=Path, help="Write a readable Markdown output record")
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output == source or source in output.parents:
        parser.error("--output must be outside the source tree")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    output.mkdir(parents=True, exist_ok=False)
    (output / "logs").mkdir()
    manifest_path = source / "examples/manual/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    examples = manifest["examples"]
    registered = [entry["source"] for entry in examples]
    actual = [path.relative_to(source).as_posix()
              for path in sorted((source / "examples/manual").glob("*.c"))]
    if len(registered) != len(set(registered)) or sorted(registered) != actual:
        raise RuntimeError("The manifest must list every complete C example exactly once")
    inputs = production_snapshot(source)
    inputs.update({name: sha256(source / name) for name in registered})
    inputs["examples/manual/manifest.json"] = sha256(manifest_path)
    inputs["tools/check_manual_examples.py"] = sha256(Path(__file__).resolve())
    receipt = {"schema": "polyphase-float.manual-examples/1", "status": "RUNNING",
               "started_utc": datetime.now(timezone.utc).isoformat(),
               "source": str(source), "input_sha256": inputs, "steps": [], "examples": [],
               "limits": ["These executable examples check their stated cases; they do not replace full library qualification."]}
    env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1", "LC_ALL": "C"}

    def save() -> None:
        write_json(output / "manual_examples.json", receipt)

    def run(name: str, command: list[str], timeout: int = 180) -> dict[str, object]:
        start = time.monotonic()
        completed = subprocess.run(command, cwd=output, env=env, capture_output=True,
                                   text=True, timeout=timeout, check=False)
        stdout, stderr = output / "logs" / (name + ".stdout"), output / "logs" / (name + ".stderr")
        stdout.write_text(completed.stdout, encoding="utf-8")
        stderr.write_text(completed.stderr, encoding="utf-8")
        step = {"name": name, "command": command, "cwd": str(output),
                "exit_code": completed.returncode, "elapsed_seconds": round(time.monotonic() - start, 6),
                "stdout": str(stdout.relative_to(output)), "stderr": str(stderr.relative_to(output)),
                "stdout_sha256": sha256(stdout), "stderr_sha256": sha256(stderr)}
        receipt["steps"].append(step)
        save()
        if completed.returncode:
            raise RuntimeError(name + " failed; see " + str(stderr))
        return step

    save()
    try:
        if args.baseline_snapshot:
            baseline = json.loads(args.baseline_snapshot.read_text(encoding="utf-8"))
            production = production_snapshot(source)
            previous = {name: digest for name, digest in baseline.items()
                        if name.split("/")[0] in {"src", "include", "compat", "adapter"}
                        and Path(name).suffix in {".c", ".h"}}
            different = sorted(name for name in production.keys() | previous.keys()
                               if production.get(name) != previous.get(name))
            receipt["reuse_delta"] = {"baseline_snapshot": str(args.baseline_snapshot.resolve()),
                "baseline_snapshot_sha256": sha256(args.baseline_snapshot),
                "production_files_compared": len(production), "different_production_paths": different,
                "production_and_header_bytes_unchanged": not different}
            if different:
                raise RuntimeError("Existing-library reuse rejected: production/header bytes differ")
        if args.baseline_receipt:
            receipt["prior_evidence"] = {"path": str(args.baseline_receipt.resolve()),
                                         "sha256": sha256(args.baseline_receipt),
                                         "meaning": "Prior qualification is retained at its original source identity; this run validates only the new example inputs."}
        selected = [entry for entry in examples if not (args.core_only and entry.get("mpfr", False))]
        receipt["omitted_examples"] = [entry["source"] for entry in examples if entry not in selected]
        core = args.core_library.resolve() if args.core_library else None
        adapter = args.mpfr_library.resolve() if args.mpfr_library else None
        if core is None:
            build = output / "core"
            needs_mpfr = any(entry.get("mpfr", False) for entry in selected)
            run("configure_core", ["cmake", "-S", str(source), "-B", str(build),
                "-DCMAKE_C_COMPILER=" + args.cc[0], "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_C_EXTENSIONS=OFF", "-DPPF_WARNINGS_AS_ERRORS=ON",
                "-DPPF_ENABLE_NATIVE_OPTIMIZATIONS=OFF", "-DPPF_OPENMP=OFF",
                "-DPPF_MPFR_SUPPORT=" + ("ON" if needs_mpfr else "OFF"),
                "-DPPF_BUILD_SHARED=OFF", "-DPPF_BUILD_STATIC=ON", "-DBUILD_TESTING=OFF",
                "-DPPF_BUILD_EXAMPLES=OFF", "-DPPF_BUILD_BENCHMARKS=OFF"])
            run("build_core", ["cmake", "--build", str(build), "-j", str(args.jobs)])
            core = build / "libpolyphase_float.a"
            adapter = build / "libpolyphase_float_mpfr.a" if needs_mpfr else None
        if not core.is_file() or (args.expected_core_sha256 and sha256(core) != args.expected_core_sha256):
            raise RuntimeError("Core library is absent or does not match the required SHA-256")
        libraries = {str(core): sha256(core)}
        if any(entry.get("mpfr", False) for entry in selected):
            if adapter is None or not adapter.is_file():
                raise RuntimeError("The selected MPFR example requires --mpfr-library or a fresh adapter build")
            libraries[str(adapter)] = sha256(adapter)
        receipt["libraries_before"] = libraries
        receipt["compiler_versions"] = {}
        for index, compiler in enumerate(args.cc):
            version = run("compiler_" + str(index), [compiler, "--version"])
            receipt["compiler_versions"][compiler] = (output / version["stdout"]).read_text().splitlines()[0]
            directory = output / ("compiler_" + str(index))
            directory.mkdir()
            for entry in selected:
                name = Path(entry["source"]).stem
                executable = directory / name
                command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                           "-I" + str(source / "include"), str(source / entry["source"]), "-o", str(executable)]
                if entry.get("mpfr", False):
                    command.append(str(adapter))
                command.append(str(core))
                if entry.get("mpfr", False):
                    command.append("-lmpfr")
                command.extend(["-lgmp", "-lm", "-pthread"])
                run(str(index) + "_compile_" + name, command)
                result = run(str(index) + "_run_" + name, [str(executable)], timeout=60)
                observed = (output / result["stdout"]).read_text(encoding="utf-8")
                if observed != entry["stdout"]:
                    raise RuntimeError(compiler + " output differs from the reviewed expectation for " + name)
                receipt["examples"].append({"source": entry["source"], "compiler": compiler,
                    "status": "PASS", "stdout": observed, "executable_sha256": sha256(executable),
                    "source_sha256": inputs[entry["source"]], "mpfr": entry.get("mpfr", False)})
                save()
        receipt["libraries_after"] = {path: sha256(Path(path)) for path in libraries}
        if receipt["libraries_after"] != libraries:
            raise RuntimeError("A reused or newly built library changed during example validation")
        changed = [name for name, digest in inputs.items() if sha256(source / name) != digest]
        receipt["changed_input_paths"] = changed
        if changed:
            raise RuntimeError("Example validation inputs changed while the checks ran")
        receipt["status"] = "PASS_SELECTED_EXAMPLES" if receipt["omitted_examples"] else "PASS"
        if args.report:
            lines = ["# Executable example results", "", "The complete programs in this manual were compiled as strict C11 with warnings treated as errors. Each program returned success and produced the expected output shown below.", "", "Reproduce these checks from the source directory:", "", "```sh", "python3 tools/check_manual_examples.py --output ../manual-check --cc gcc clang", "```", "", "The full run requires GMP and the official MPFR development headers and libraries. Add `--core-only` to explicitly omit the MPFR bridge. The output directory must be new and outside the source tree.", "", "Compilers used:", ""]
            lines.extend("- " + version for version in receipt["compiler_versions"].values())
            lines.extend(["", "These examples validate their demonstrated cases. They supplement the library's broader qualification and numerical contracts.", ""])
            for entry in selected:
                lines.extend(["## " + Path(entry["source"]).stem, "", "Source: [" + Path(entry["source"]).name + "](" + "../../" + entry["source"] + ").", "", "```text", entry["stdout"].rstrip("\n"), "```", ""])
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text("\n".join(lines), encoding="utf-8")
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        receipt["status"] = "FAIL"
        receipt["error"] = str(error)
    finally:
        receipt["completed_utc"] = datetime.now(timezone.utc).isoformat()
        save()
    print(json.dumps({"status": receipt["status"], "compiled_and_executed": len(receipt["examples"]),
                      "receipt": str(output / "manual_examples.json")}, indent=2))
    return 0 if receipt["status"] in {"PASS", "PASS_SELECTED_EXAMPLES"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
