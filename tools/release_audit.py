#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Audit a Polyphase Float source tree before public release."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

from library_surface import enabled_core_declarations

SPDX = "SPDX-License-Identifier: LGPL-3.0-or-later"
CURRENT_RELEASE = "0.20.0"
PREVIOUS_RELEASE = "0.19.0"
REVIEWED_BASELINE_MANIFEST_SHA256 = (
    "561b8f906a7c8b7d9a2cf5fd4e4c1d4df67ec27a1a301557f585e81d3a94b805"
)
REVIEWED_CHANGED_SOURCE_FILES = (
    "assignment.c",
    "conditional_formats.c",
    "io.c",
    "limb_round.c",
    "polyphase_chudnovsky_core.c",
    "range_flags.c",
    "round.c",
    "special_bessel.c",
    "special_common.c",
    "special_gamma.c",
    "special_misc.c",
    "transcendental.c",
)
REQUIRED_FILES = (
    "abi/0.19.0-to-0.20.0-comparison.json",
    "docs/PRODUCTION_CHANGESET_0.20.0.json",
    "tools/qualify_release.py",
    "docs/QUALIFICATION.md",
    "docs/BRANCH_REVIEW_0.20.0.md",
    "docs/ALLOCATION_COMPLETION_0.20.0.md",
    "docs/FPIF_RESOURCE_CONTRACT.md",
    "tests/test_release_boundaries.c",
    "tests/test_allocation_completion.c",
    "tests/test_fpif_resource_bounds.c",
    "tools/tests/test_release_api_audit.py",
    "LICENSE",
    "COPYING",
    "LICENSE.md",
    "THIRD_PARTY_NOTICES.md",
    "README.md",
    "CONTRIBUTING.md",
    "CODE_OF_CONDUCT.md",
    "SECURITY.md",
    "GOVERNANCE.md",
    "SUPPORT.md",
    "ROADMAP.md",
    "CITATION.cff",
    "CMakeLists.txt",
    "CMakePresets.json",
    "polyphase-float.pc.in",
    "polyphase-float-mpfr.pc.in",
    "docs/BUILDING.md",
    "docs/TESTING.md",
    "docs/CLAIMS_AND_EVIDENCE.md",
    "docs/BENCHMARK_EVIDENCE_PIPELINE.md",
    "docs/BENCHMARK_EVIDENCE_VALIDATION_0.10.5.md",
    "docs/BENCHMARK_EVIDENCE_VALIDATION_0.10.6.md",
    "docs/BENCHMARK_COUNTERBALANCING.md",
    "docs/BENCHMARK_REGRESSION_GATES.md",
    "docs/PRODUCT_PLANNER_PROFILES.md",
    "docs/PRODUCT_AND_FUSED_PLANNER.md",
    "docs/PRODUCT_PLANNER_CHANGESET_0.10.7.json",
    "docs/CANONICAL_BASELINE.md",
    "docs/PRODUCTION_SOURCE_PRESERVATION_0.10.5.json",
    "docs/PRODUCTION_SOURCE_PRESERVATION_0.10.6.json",
    "docs/PUBLIC_RELEASE_STATUS.md",
    "docs/PUBLIC_RELEASE_HARDENING_REPORT.md",
    "docs/MPFR_OPTIONAL_FORMAT_COMPATIBILITY_0.10.4.md",
    "MPFR_OPTIONAL_FORMAT_COMPATIBILITY_VALIDATION.txt",
    "docs/MPFR_PRECISION_TYPE_COMPATIBILITY_0.10.3.md",
    "MPFR_PRECISION_TYPE_COMPATIBILITY_VALIDATION.txt",
    "cmake/PolyphaseMPFROptionalFormats.cmake",
    "tools/probes/mpfr_float128.c",
    "tools/probes/mpfr_decimal.c",
    ".github/workflows/ci.yml",
    ".github/workflows/extended.yml",
    ".github/workflows/codeql.yml",
    "include/polyphase_float.h",
    "tools/audit_independence.py",
    "tools/generate_product_profile.py",
    "scripts/calibrate_product_profile.py",
    "scripts/validate_product_profile.py",
    "scripts/check_api_coverage.py",
    "benchmarks/benchmark_samples.h",
    "tests/test_benchmark_json_schema.py",
    "tests/test_product_profile.c",
    "tests/test_generate_product_profile.py",
    "src/product_planner.c",
    "src/ppf_product_profile_build.h",
    "src/ppf_openmp_compat.h",
    "cmake/PolyphaseFuzzers.cmake",
    "fuzz/fuzz_text_parser.c",
    "fuzz/fuzz_fpif_import.c",
    "fuzz/corpus/text/zero.txt",
    "fuzz/corpus/fpif/valid-zero.ppfif",
    "tests/test_allocation_failure.c",
    "docs/ALLOCATION_FAILURE_CONTRACT.md",
    "docs/ALLOCATION_FAILURE_VALIDATION_0.10.9.md",
    "docs/PORTABILITY_AND_FUZZING_0.10.9.md",
    "docs/COMPLETION_STATUS_0.10.9.md",
    "docs/PRODUCTION_CHANGESET_0.10.9.json",
    "tests/test_thread_safety.c",
    "docs/THREAD_SAFETY.md",
    "docs/THREAD_SAFETY_BASELINE_RACE_0.10.9.md",
    "docs/THREAD_SAFETY_VALIDATION_0.11.0.md",
    "docs/COMPLETION_STATUS_0.11.0.md",
    "docs/PRODUCTION_CHANGESET_0.11.0.json",
    "src/abi.c",
    "abi/contract-v1.json",
    "abi/0.11.0-to-0.12.0-comparison.json",
    "abi/baselines/0.11.0-contract.json",
    "abi/baselines/0.11.0-linux-x86_64-lp64-gmp64-layout.json",
    "abi/polyphase_float.map",
    "abi/polyphase_float_mpfr.map",
    "abi/symbols/core-0.11.txt",
    "abi/symbols/core-0.12.txt",
    "abi/symbols/adapter-0.11.txt",
    "tools/abi_contract.py",
    "tools/abi_exports.py",
    "tools/abi_layout_probe.c",
    "tools/abi_layout_compare.py",
    "tools/run_abi_binary_compat.py",
    "tests/test_abi_runtime.c",
    "tests/abi_0_11_consumer.c",
    "docs/ABI_POLICY.md",
    "docs/ABI_COMPATIBILITY_0.12.0.md",
    "docs/ABI_STABILITY_VALIDATION_0.12.0.md",
    "docs/ABI_STABILITY_RELEASE_REPORT_0.12.0.md",
    "docs/COMPLETION_STATUS_0.12.0.md",
    "docs/PRODUCTION_CHANGESET_0.12.0.json",
    "abi/0.12.0-to-0.13.0-comparison.json",
    "abi/baselines/0.12.0-contract.json",
    "semantics/function-purposes.csv",
    "semantics/contract-v1.json",
    "semantics/function-matrix-v1.csv",
    "tools/semantic_contract.py",
    "tests/test_semantic_contract.py",
    "tests/test_semantic_surface.c",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.13.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.13.0.md",
    "docs/ABI_COMPATIBILITY_0.13.0.md",
    "docs/COMPLETION_STATUS_0.13.0.md",
    "docs/PRODUCTION_CHANGESET_0.13.0.json",
    "abi/0.13.0-to-0.14.0-comparison.json",
    "abi/baselines/0.13.0-contract.json",
    "semantics/aliasing-contract-v1.json",
    "semantics/aliasing-matrix-v1.csv",
    "tools/aliasing_contract.py",
    "tests/test_aliasing_contract.py",
    "tests/test_aliasing_matrix.c",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.14.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.14.0.md",
    "docs/ALIASING_CONTRACT_0.14.0.md",
    "docs/ALIASING_CONTRACT_VALIDATION_0.14.0.md",
    "docs/ABI_COMPATIBILITY_0.14.0.md",
    "docs/COMPLETION_STATUS_0.14.0.md",
    "docs/PRODUCTION_CHANGESET_0.14.0.json",
    "tests/test_io_allocation_failure.c",
    "abi/baselines/0.14.0-contract.json",
    "abi/0.14.0-to-0.15.0-comparison.json",
    "docs/ABI_COMPATIBILITY_0.15.0.md",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.15.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.15.0.md",
    "docs/ALIASING_CONTRACT_0.15.0.md",
    "docs/ALIASING_CONTRACT_VALIDATION_0.15.0.md",
    "docs/IO_ALLOCATION_CONTRACT_0.15.0.md",
    "docs/IO_ALLOCATION_HARDENING_VALIDATION_0.15.0.md",
    "docs/COMPLETION_STATUS_0.15.0.md",
    "docs/PRODUCTION_CHANGESET_0.15.0.json",
    "tests/test_constant_allocation_failure.c",
    "abi/baselines/0.15.0-contract.json",
    "abi/0.15.0-to-0.16.0-comparison.json",
    "docs/ABI_COMPATIBILITY_0.16.0.md",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.16.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.16.0.md",
    "docs/ALIASING_CONTRACT_0.16.0.md",
    "docs/ALIASING_CONTRACT_VALIDATION_0.16.0.md",
    "docs/CONSTANT_ALLOCATION_CONTRACT_0.16.0.md",
    "docs/CONSTANT_ALLOCATION_HARDENING_VALIDATION_0.16.0.md",
    "docs/COMPLETION_STATUS_0.16.0.md",
    "docs/PRODUCTION_CHANGESET_0.16.0.json",
    "tests/test_transcendental_special_allocation_failure.c",
    "src/result_transaction.c",
    "src/transcendental_special_transaction.c",
    "src/transactional_impl_prototypes.h",
    "src/transactional_special_names.h",
    "src/transactional_transcendental_names.h",
    "src/transactional_trig_names.h",
    "abi/baselines/0.16.0-contract.json",
    "abi/0.16.0-to-0.17.0-comparison.json",
    "docs/ABI_COMPATIBILITY_0.17.0.md",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.17.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.17.0.md",
    "docs/ALIASING_CONTRACT_0.17.0.md",
    "docs/ALIASING_CONTRACT_VALIDATION_0.17.0.md",
    "docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_CONTRACT_0.17.0.md",
    "docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_HARDENING_VALIDATION_0.17.0.md",
    "docs/COMPLETION_STATUS_0.17.0.md",
    "docs/PRODUCTION_CHANGESET_0.17.0.json",
    "tests/test_arithmetic_root_allocation_failure.c",
    "src/arithmetic_root_transaction.c",
    "src/transactional_arithmetic_root_names.h",
    "abi/baselines/0.17.0-contract.json",
    "abi/0.17.0-to-0.18.0-comparison.json",
    "docs/ABI_COMPATIBILITY_0.18.0.md",
    "docs/PUBLIC_SEMANTIC_CONTRACT_0.18.0.md",
    "docs/SEMANTIC_CONTRACT_VALIDATION_0.18.0.md",
    "docs/ALIASING_CONTRACT_0.18.0.md",
    "docs/ALIASING_CONTRACT_VALIDATION_0.18.0.md",
    "docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md",
    "docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_HARDENING_VALIDATION_0.18.0.md",
    "docs/COMPLETION_STATUS_0.18.0.md",
    "docs/PRODUCTION_CHANGESET_0.18.0.json",
    "docs/PRODUCTION_CHANGESET_0.19.0.json",
    "abi/0.18.0-to-0.19.0-comparison.json",
    "abi/baselines/0.18.0-contract.json",
    "tests/test_install_packaging.py",
    "tools/library_surface.py",
    ".github/workflows/abi.yml",
)

SOURCE_SUFFIXES = {".c", ".h", ".py", ".ps1", ".cmake", ".yml", ".yaml"}
SOURCE_ROOTS = (
    "src",
    "include",
    "adapter",
    "compat",
    "tests",
    "benchmarks",
    "examples",
    "experiments",
    "scripts",
    "tools",
    "cmake",
    "fuzz",
    ".github",
)

EXCLUDED_DIRS = {
    ".git",
    "build",
    "bin",
    "lib",
    "stage",
    "dist",
    "__pycache__",
}

DEPRECATED_MPFR_CALLS = ("mpfr_root", "mpfr_grandom")
DEPRECATED_MPFR_SCAN_ROOTS = (
    "src",
    "adapter",
    "tests",
    "benchmarks",
    "examples",
    "experiments",
)


def fail(message: str, failures: list[str]) -> None:
    failures.append(message)


def parse_version(root: Path, failures: list[str]) -> str | None:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    citation = (root / "CITATION.cff").read_text(encoding="utf-8")

    cmake_match = re.search(
        r"project\s*\(\s*PolyphaseFloat.*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
        cmake,
        re.S,
    )
    if not cmake_match:
        fail("unable to parse project version from CMakeLists.txt", failures)
        return None
    version = cmake_match.group(1)
    major, minor, patch = version.split(".")

    expected_macros = {
        "PPF_VERSION_MAJOR": major,
        "PPF_VERSION_MINOR": minor,
        "PPF_VERSION_PATCHLEVEL": patch,
    }
    for macro, expected in expected_macros.items():
        match = re.search(rf"#define\s+{macro}\s+([0-9]+)", header)
        if not match or match.group(1) != expected:
            fail(f"{macro} does not match CMake version {version}", failures)

    string_match = re.search(r'#define\s+PPF_VERSION_STRING\s+"([^"]+)"', header)
    if not string_match or string_match.group(1) != version:
        fail(f"PPF_VERSION_STRING does not equal {version}", failures)

    cff_match = re.search(r'^version:\s*["\']?([^"\'\n]+)', citation, re.M)
    if not cff_match or cff_match.group(1).strip() != version:
        fail(f"CITATION.cff version does not equal {version}", failures)

    return version


def audit_spdx(root: Path, failures: list[str]) -> int:
    checked = 0
    for relative_root in SOURCE_ROOTS:
        base = root / relative_root
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            if any(part in EXCLUDED_DIRS for part in path.parts):
                continue
            checked += 1
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                fail(f"source-like file is not UTF-8: {path.relative_to(root)}", failures)
                continue
            if SPDX not in text[:1200]:
                fail(f"missing SPDX identifier: {path.relative_to(root)}", failures)
            for line_number, line in enumerate(text.splitlines(), 1):
                if line.rstrip(" \t") != line:
                    fail(
                        f"trailing whitespace: {path.relative_to(root)}:{line_number}",
                        failures,
                    )
                    break
            if path.suffix.lower() == ".py":
                try:
                    compile(text, str(path), "exec")
                except SyntaxError as exc:
                    fail(
                        f"Python syntax error in {path.relative_to(root)}: {exc}",
                        failures,
                    )

    for relative in ("CMakeLists.txt", "Makefile"):
        path = root / relative
        checked += 1
        if SPDX not in path.read_text(encoding="utf-8")[:1200]:
            fail(f"missing SPDX identifier: {relative}", failures)
    return checked


def audit_hygiene(root: Path, failures: list[str]) -> None:
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if any(part in EXCLUDED_DIRS for part in relative.parts):
            continue
        if path.is_file() and (path.suffix == ".tmp" or path.name.endswith("~")):
            fail(f"temporary file in source release: {relative}", failures)
        if path.is_file() and path.suffix in {".o", ".a", ".so", ".dll", ".dylib", ".pyc"}:
            fail(f"compiled artifact in source release: {relative}", failures)

    for forbidden in ("build-cmake", "cmake-build", "stage", "dist"):
        if (root / forbidden).exists():
            fail(f"local build/package directory present: {forbidden}", failures)

    generated_root_files = {
        "polyphase-float.pc": "generated pkg-config metadata",
        "SHA256SUMS.txt": "obsolete source-tree checksum manifest",
    }
    for relative, description in generated_root_files.items():
        if (root / relative).exists():
            fail(f"{description} present at repository root: {relative}", failures)


def audit_configuration_syntax(root: Path, failures: list[str]) -> None:
    try:
        json.loads((root / "CMakePresets.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid CMakePresets.json: {exc}", failures)


def audit_deprecated_mpfr_calls(root: Path, failures: list[str]) -> int:
    checked = 0
    for relative_root in DEPRECATED_MPFR_SCAN_ROOTS:
        base = root / relative_root
        if not base.exists():
            continue
        for path in sorted(base.rglob("*.c")):
            if any(part in EXCLUDED_DIRS for part in path.parts):
                continue
            checked += 1
            text = path.read_text(encoding="utf-8")
            for symbol in DEPRECATED_MPFR_CALLS:
                match = re.search(rf"\b{re.escape(symbol)}\s*\(", text)
                if match:
                    line = text.count("\n", 0, match.start()) + 1
                    fail(
                        f"deprecated MPFR call in validation source: "
                        f"{path.relative_to(root)}:{line}: {symbol}",
                        failures,
                    )
    return checked


def audit_mpfr_precision_model(root: Path, failures: list[str]) -> None:
    header = (root / "compat/mpfr_minimal.h").read_text(encoding="utf-8")
    test = (root / "tests/test_mpfr_type_model.c").read_text(encoding="utf-8")
    divisor = (root / "tests/test_divisor_plan.c").read_text(encoding="utf-8")

    if "typedef mp_size_t mpfr_prec_t;" not in header:
        fail("minimal MPFR header does not derive mpfr_prec_t from signed GMP type", failures)
    if "typedef mp_exp_t mpfr_exp_t;" not in header:
        fail("minimal MPFR header does not derive mpfr_exp_t from signed GMP type", failures)
    if re.search(r"typedef\s+unsigned[^;]*\s+mpfr_prec_t\s*;", header):
        fail("minimal MPFR header models mpfr_prec_t as unsigned", failures)
    if "_Static_assert((mpfr_prec_t)-1 < (mpfr_prec_t)0" not in test:
        fail("missing compile-time signed mpfr_prec_t regression", failures)
    if "_Static_assert((mpfr_exp_t)-1 < (mpfr_exp_t)0" not in test:
        fail("missing compile-time signed mpfr_exp_t regression", failures)
    if "ppf_prec_from_mpfr" not in divisor:
        fail("divisor-plan test lacks checked MPFR precision conversion", failures)


def audit_mpfr_optional_formats(root: Path, failures: list[str]) -> None:
    header = (root / "compat/mpfr_minimal.h").read_text(encoding="utf-8")
    test = (root / "tests/test_conditional_formats.c").read_text(encoding="utf-8")
    benchmark = (root / "benchmarks/benchmark_io_formatting.c").read_text(encoding="utf-8")
    detection = (root / "cmake/PolyphaseMPFROptionalFormats.cmake").read_text(encoding="utf-8")
    test_cmake = (root / "cmake/PolyphaseTests.cmake").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    float_probe = (root / "tools/probes/mpfr_float128.c").read_text(encoding="utf-8")
    decimal_probe = (root / "tools/probes/mpfr_decimal.c").read_text(encoding="utf-8")

    if "#ifdef MPFR_WANT_FLOAT128" not in header:
        fail("minimal MPFR header exposes binary128 without the official opt-in", failures)
    if "#ifdef MPFR_WANT_DECIMAL_FLOATS" not in header:
        fail("minimal MPFR header exposes decimal conversions without the official opt-in", failures)
    if re.search(r"typedef\s+[^;]+\s+mpfr_decimal(?:64|128)\s*;", header):
        fail("minimal MPFR header defines nonstandard decimal typedef aliases", failures)
    if "int __gmpfr_fpif_export(FILE *, mpfr_ptr);" not in header:
        fail("minimal MPFR header does not match the mutable FPIF export prototype", failures)

    include_pos = test.find('#include "polyphase_float_mpfr.h"')
    float_opt_pos = test.find("# define MPFR_WANT_FLOAT128 1")
    decimal_opt_pos = test.find("# define MPFR_WANT_DECIMAL_FLOATS 1")
    if include_pos < 0 or float_opt_pos < 0 or float_opt_pos > include_pos:
        fail("conditional-format test does not request MPFR binary128 before inclusion", failures)
    if include_pos < 0 or decimal_opt_pos < 0 or decimal_opt_pos > include_pos:
        fail("conditional-format test does not request MPFR decimal formats before inclusion", failures)
    if re.search(r"\bmpfr_decimal(?:64|128)\b", test):
        fail("conditional-format test relies on nonstandard MPFR decimal aliases", failures)

    if "static int mpfr_roundtrip(mpfr_ptr source, mpfr_ptr target)" not in benchmark:
        fail("FPIF benchmark does not honor MPFR's mutable export-source prototype", failures)

    required_detection_tokens = (
        "check_c_source_compiles",
        "MPFR_WANT_FLOAT128",
        "MPFR_WANT_DECIMAL_FLOATS",
        "PPF_MPFR_HAVE_FLOAT128_API",
        "PPF_MPFR_HAVE_DECIMAL_API",
    )
    for token in required_detection_tokens:
        if token not in detection:
            fail(f"optional MPFR format detection is missing {token}", failures)

    if "PPF_MPFR_HAVE_FLOAT128_API=${PPF_MPFR_HAVE_FLOAT128_API}" not in test_cmake:
        fail("conditional-format target lacks binary128 feature result", failures)
    if "PPF_MPFR_HAVE_DECIMAL_API=${PPF_MPFR_HAVE_DECIMAL_API}" not in test_cmake:
        fail("conditional-format target lacks decimal feature result", failures)

    required_make_tokens = (
        "tools/probes/mpfr_float128.c",
        "tools/probes/mpfr_decimal.c",
        "PPF_MPFR_HAVE_FLOAT128_API :=",
        "PPF_MPFR_HAVE_DECIMAL_API :=",
        "MPFR_OPTIONAL_FORMAT_CPPFLAGS",
    )
    for token in required_make_tokens:
        if token not in makefile:
            fail(f"Make optional MPFR format detection is missing {token}", failures)
    if "mpfr_buildopt_float128_p()" not in float_probe:
        fail("Make binary128 probe does not verify the MPFR build option", failures)
    if "mpfr_buildopt_decimal_p()" not in decimal_probe:
        fail("Make decimal probe does not verify the MPFR build option", failures)



def audit_benchmark_evidence(root: Path, failures: list[str]) -> None:
    helper = (root / "benchmarks/benchmark_samples.h").read_text(encoding="utf-8")
    speedup = (root / "benchmarks/benchmark_speedup_method.c").read_text(
        encoding="utf-8"
    )
    constants = (root / "benchmarks/benchmark_constants.c").read_text(
        encoding="utf-8"
    )
    schema_test = (root / "tests/test_benchmark_json_schema.py").read_text(
        encoding="utf-8"
    )
    benchmark_cmake = (root / "cmake/PolyphaseBenchmarks.cmake").read_text(
        encoding="utf-8"
    )

    if not re.search(r"static\s+inline\s+int\s+ppf_benchmark_median", helper):
        fail("benchmark median helper does not report failure explicitly", failures)
    if "return values[count / 2u]" in helper:
        fail("benchmark median helper retains an unsorted fallback", failures)
    if "memcpy(copy, values" not in helper:
        fail("benchmark median helper does not preserve acquisition order", failures)

    for relative, text in (
        ("benchmarks/benchmark_speedup_method.c", speedup),
        ("benchmarks/benchmark_constants.c", constants),
    ):
        for token in (
            '\\"schema_version\\":2',
            '\\"sample_statistic\\":\\"median\\"',
            '\\"sample_count\\"',
            '\\"samples_seconds\\"',
            '\\"verified\\":true',
        ):
            if token not in text:
                fail(f"{relative} does not emit required evidence token {token}", failures)
        if "ppf_benchmark_median" not in text:
            fail(f"{relative} does not use the common median helper", failures)

    for token in (
        "schema_version",
        "samples_seconds",
        "sample_count",
        "statistics.median",
        "verified",
    ):
        if token not in schema_test:
            fail(f"benchmark schema regression lacks {token}", failures)
    if "benchmark_json_schema_smoke" not in benchmark_cmake:
        fail("benchmark JSON schema regression is not registered with CTest", failures)
    if "tests/test_benchmark_json_schema.py" not in benchmark_cmake:
        fail("CTest schema regression does not invoke the public test script", failures)


def audit_product_planner_changeset(root: Path, failures: list[str]) -> None:
    """Validate the frozen historical 0.10.7 record without comparing it to
    the current production tree."""
    path = root / "docs/PRODUCT_PLANNER_CHANGESET_0.10.7.json"
    try:
        record = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid product-planner changeset record: {exc}", failures)
        return
    if record.get("version") != "0.10.7" or record.get("baseline_version") != "0.10.6":
        fail("product-planner changeset version boundary is incorrect", failures)
    if record.get("production_source_unchanged") is not False:
        fail("product-planner changeset incorrectly claims unchanged source", failures)
    if record.get("changed_files") != ["arithmetic.c", "ppf_internal.h", "roots.c"]:
        fail("product-planner changeset has an unexpected changed-file list", failures)
    if record.get("added_files") != ["ppf_product_profile_build.h", "product_planner.c"]:
        fail("product-planner changeset has an unexpected added-file list", failures)
    if record.get("removed_files") != []:
        fail("product-planner changeset reports removed production files", failures)
    frozen = record.get("files")
    if not isinstance(frozen, dict) or not frozen:
        fail("product-planner changeset has no frozen file hash map", failures)
        return
    for relative, item in frozen.items():
        digest = item.get("sha256") if isinstance(item, dict) else None
        if not isinstance(relative, str) or not re.fullmatch(r"[0-9a-f]{64}", digest or ""):
            fail("product-planner changeset contains an invalid frozen hash", failures)
            break


def audit_current_production_changeset(root: Path, failures: list[str]) -> tuple[int, int, int]:
    path = root / f"docs/PRODUCTION_CHANGESET_{CURRENT_RELEASE}.json"
    try:
        record = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid current production changeset record: {exc}", failures)
        return 0, 0, 0
    if record.get("version") != CURRENT_RELEASE or record.get("baseline_version") != PREVIOUS_RELEASE:
        fail("current production changeset version boundary is incorrect", failures)
    if record.get("production_source_unchanged") is not False:
        fail("current production changeset incorrectly claims unchanged source", failures)
    if record.get("numerical_formula_changes") is not False or record.get("numerical_behavior_changes") is not True:
        fail("current changeset must distinguish numerical implementation corrections from unchanged mathematical definitions", failures)
    changed = record.get("changed_files")
    added = record.get("added_files")
    removed = record.get("removed_files")
    if not all(isinstance(value, list) for value in (changed, added, removed)):
        fail("current production changeset has invalid file lists", failures)
        return 0, 0, 0
    if changed != list(REVIEWED_CHANGED_SOURCE_FILES) or added != [] or removed != []:
        fail("current production changeset has an unexpected production-source boundary", failures)
    boundary = record.get("change_boundary")
    if not isinstance(boundary, dict) or set(boundary) != set(REVIEWED_CHANGED_SOURCE_FILES):
        fail("current changeset does not describe every reviewed production change", failures)
    elif any(not isinstance(value, str) or not value.strip() for value in boundary.values()):
        fail("current changeset contains an empty production-change description", failures)
    expected = record.get("files")
    if not isinstance(expected, dict):
        fail("current production changeset has no file hash map", failures)
        return len(changed), len(added), len(removed)
    actual_paths = {
        source.relative_to(root / "src").as_posix(): source
        for source in (root / "src").rglob("*")
        if source.is_file()
    }
    if set(expected) != set(actual_paths):
        fail("current production changeset inventory does not match src/", failures)
        return len(changed), len(added), len(removed)
    for relative, source in actual_paths.items():
        item = expected.get(relative)
        wanted = item.get("sha256") if isinstance(item, dict) else None
        wanted_bytes = item.get("bytes") if isinstance(item, dict) else None
        actual = hashlib.sha256(source.read_bytes()).hexdigest()
        if wanted != actual:
            fail(f"production source hash differs from current record: src/{relative}", failures)
        if wanted_bytes != source.stat().st_size:
            fail(f"production source size differs from current record: src/{relative}", failures)
    # A new self-consistent manifest must not conceal changes outside the
    # reviewed boundary. Compare against the frozen prior-release hash map.
    baseline_path = root / f"docs/PRODUCTION_CHANGESET_{PREVIOUS_RELEASE}.json"
    try:
        baseline_bytes = baseline_path.read_bytes()
        baseline = json.loads(baseline_bytes)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid frozen prior-release source manifest: {exc}", failures)
    else:
        if hashlib.sha256(baseline_bytes).hexdigest() != REVIEWED_BASELINE_MANIFEST_SHA256:
            fail("frozen prior-release source manifest hash changed", failures)
        baseline_files = baseline.get("files", {})
        if not isinstance(baseline_files, dict) or set(baseline_files) != set(actual_paths):
            fail("prior-release source inventory does not match reviewed no-addition boundary", failures)
        else:
            actual_changed = sorted(
                relative for relative, source in actual_paths.items()
                if baseline_files[relative].get("sha256") != hashlib.sha256(source.read_bytes()).hexdigest()
            )
            if actual_changed != list(REVIEWED_CHANGED_SOURCE_FILES):
                fail("actual production-source changes differ from reviewed prior-release boundary", failures)
    return len(changed), len(added), len(removed)

def audit_install_packaging(root: Path, failures: list[str]) -> None:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in ("add_test(NAME install_packaging", "tests/test_install_packaging.py"):
        if token not in cmake:
            fail(f"installed-consumer packaging regression is not registered: {token}", failures)


def audit_product_profile(root: Path, failures: list[str]) -> None:
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    planner = (root / "src/product_planner.c").read_text(encoding="utf-8")
    default_profile = (root / "src/ppf_product_profile_build.h").read_text(
        encoding="utf-8"
    )
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    generator = (root / "tools/generate_product_profile.py").read_text(
        encoding="utf-8"
    )
    calibrator = (root / "scripts/calibrate_product_profile.py").read_text(
        encoding="utf-8"
    )
    validator = (root / "scripts/validate_product_profile.py").read_text(
        encoding="utf-8"
    )
    tests = cmake + "\n" + (root / "cmake/PolyphaseTests.cmake").read_text(
        encoding="utf-8"
    )
    benchmarks = cmake + "\n" + (root / "cmake/PolyphaseBenchmarks.cmake").read_text(
        encoding="utf-8"
    )
    for token in (
        "ppf_product_plan_t",
        "ppf_plan_product(",
        "ppf_product_profile_name(",
        "ppf_product_profile_sha256(",
    ):
        if token not in header:
            fail(f"public planner introspection lacks {token}", failures)
    for token in (
        "ppf_product_plan_internal",
        "profile_lookup",
        "effective_workers_for",
        "PPF_PRODUCT_PROFILE_RULE_COUNT",
    ):
        if token not in planner:
            fail(f"central product planner lacks {token}", failures)
    for token in (
        'PPF_PRODUCT_PROFILE_NAME "portable"',
        "PPF_PRODUCT_PROFILE_RULE_COUNT 0u",
    ):
        if token not in default_profile:
            fail(f"portable product profile lacks {token}", failures)
    if "PPF_PRODUCT_PROFILE_HEADER" not in cmake or \
            "PPF_PRODUCT_PROFILE_HEADER" not in makefile:
        fail("build systems do not expose the product-profile header option", failures)
    for token in (
        "min_processes", "min_bindings", "min_speedup",
        "min_win_fraction", "machine_fingerprint_sha256",
        "software_fingerprint_sha256", "bootstrap_median_interval",
    ):
        if token not in generator:
            fail(f"profile generator lacks evidence gate {token}", failures)
    for token in (
        "process_repetitions", "binding_order", "rank_raw.csv",
        "resolve_campaign_id", "allowed_cpu_list",
    ):
        if token not in calibrator:
            fail(f"profile calibrator lacks {token}", failures)
    for token in ("profile_rule_matched", "maximum_auto_slowdown"):
        if token not in validator:
            fail(f"profile validator lacks {token}", failures)
    if "product_profile_generator" not in tests:
        fail("profile generator regression is not registered with CTest", failures)
    if "product_plan_inspection_smoke" not in benchmarks:
        fail("product-plan inspection is not registered as a smoke test", failures)

def audit_allocation_hardening(root: Path, failures: list[str]) -> None:
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    context = (root / "src/context.c").read_text(encoding="utf-8")
    flags = (root / "src/range_flags.c").read_text(encoding="utf-8")
    arithmetic = (root / "src/arithmetic.c").read_text(encoding="utf-8")
    divisor = (root / "src/divisor_plan.c").read_text(encoding="utf-8")
    roots = (root / "src/roots.c").read_text(encoding="utf-8")
    io = (root / "src/io.c").read_text(encoding="utf-8")
    test = (root / "tests/test_allocation_failure.c").read_text(encoding="utf-8")
    io_test = (root / "tests/test_io_allocation_failure.c").read_text(encoding="utf-8")
    constant_test = (root / "tests/test_constant_allocation_failure.c").read_text(encoding="utf-8")
    transcendental_special_test = (
        root / "tests/test_transcendental_special_allocation_failure.c"
    ).read_text(encoding="utf-8")
    result_transaction = (root / "src/result_transaction.c").read_text(encoding="utf-8")
    transaction_wrappers = (
        root / "src/transcendental_special_transaction.c"
    ).read_text(encoding="utf-8")
    arithmetic_root_test = (
        root / "tests/test_arithmetic_root_allocation_failure.c"
    ).read_text(encoding="utf-8")
    arithmetic_root_wrappers = (
        root / "src/arithmetic_root_transaction.c"
    ).read_text(encoding="utf-8")
    arithmetic_root_names = (
        root / "src/transactional_arithmetic_root_names.h"
    ).read_text(encoding="utf-8")
    transcendental_fast = (root / "src/transcendental_fast.c").read_text(encoding="utf-8")
    trig_fast = (root / "src/trig_fast.c").read_text(encoding="utf-8")
    special_gamma = (root / "src/special_gamma.c").read_text(encoding="utf-8")
    special_misc = (root / "src/special_misc.c").read_text(encoding="utf-8")
    constants = (root / "src/constants.c").read_text(encoding="utf-8")
    constant_log2 = (root / "src/constant_log2.c").read_text(encoding="utf-8")
    constant_pi = (root / "src/constant_pi.c").read_text(encoding="utf-8")
    chudnovsky_core = (root / "src/polyphase_chudnovsky_core.c").read_text(encoding="utf-8")
    special_common = (root / "src/special_common.c").read_text(encoding="utf-8")
    assignment = (root / "src/assignment.c").read_text(encoding="utf-8")
    formatting = (root / "src/format.c").read_text(encoding="utf-8")
    internal = (root / "src/ppf_internal.h").read_text(encoding="utf-8")
    tests_cmake = (root / "cmake/PolyphaseTests.cmake").read_text(encoding="utf-8")

    for token in (
        "PPF_FLAG_ALLOC", "ppf_clear_allocflag(", "ppf_set_allocflag(",
        "ppf_allocflag_p(", "ppf_context_t *ctx);",
    ):
        if token not in header:
            fail(f"public allocation contract lacks {token}", failures)
    for token in (
        "ppf_context_malloc_internal", "ppf_context_calloc_internal",
        "ppf_context_realloc_internal", "ppf_context_free_internal",
    ):
        if token not in context:
            fail(f"context allocation layer lacks {token}", failures)
    for token in (
        "DEFINE_CLEAR_FLAG(allocflag, PPF_FLAG_ALLOC)",
        "DEFINE_SET_FLAG(allocflag, PPF_FLAG_ALLOC)",
        "DEFINE_TEST_FLAG(allocflag, PPF_FLAG_ALLOC)",
    ):
        if token not in flags:
            fail(f"allocation flag implementation lacks {token}", failures)
    for relative, text, tokens in (
        ("src/arithmetic.c", arithmetic,
         ("arithmetic_memory_error", "ppf_context_calloc_internal")),
        ("src/divisor_plan.c", divisor,
         ("plan_memory_or_error", "ppf_context_malloc_internal")),
        ("src/roots.c", roots,
         ("root_memory_or_error", "PPF_FLAG_ALLOC")),
        ("src/io.c", io,
         ("ppf_context_malloc_internal(ctx, payload_capacity)",
          "PPF_FPIF_MAX_IMPORT_BYTES", "PPF_FPIF_READ_CHUNK_BYTES",
          "ppf_context_free_internal(ctx, payload")),
    ):
        for token in tokens:
            if token not in text:
                fail(f"{relative} lacks allocation-hardening token {token}", failures)
    for token in (
        "fail_at", "object_snapshot", "live_count",
        "test_fpif_import_failure_sweep", "transactional-state tests passed",
    ):
        if token not in test:
            fail(f"allocation-failure regression lacks {token}", failures)
    if "allocation_failure hardening" not in tests_cmake:
        fail("allocation-failure test is not registered under hardening", failures)
    for relative, text, tokens in (
        ("src/assignment.c", assignment,
         ("ppf_strtofr_parse", "commit_parsed_value",
          "ppf_owned_string_alloc_internal", "ppf_owned_string_free_internal")),
        ("src/format.c", formatting,
         ("ppf_context_realloc_internal", "builder_clear", "ppf_vasprintf")),
        ("src/io.c", io,
         ("record_bytes", "ppf_context_realloc_internal", "ppf_fpif_export")),
        ("src/ppf_internal.h", internal,
         ("ppf_owned_string_alloc_internal", "ppf_owned_string_free_internal")),
        ("tests/test_io_allocation_failure.c", io_test,
         ("test_get_str_allocator_provenance", "test_parsing_transactions",
          "test_fpif_export_staging", "test_formatting_failure_sweep",
          "I/O allocation-failure and commit-boundary tests passed")),
    ):
        for token in tokens:
            if token not in text:
                fail(f"{relative} lacks I/O allocation-hardening token {token}", failures)
    if 'io_allocation_failure \"hardening;semantic\"' not in tests_cmake:
        fail("I/O allocation-failure matrix is not registered under hardening and semantic", failures)
    for relative, text, tokens in (
        ("src/constants.c", constants,
         ("ppf_swap(rop, &staged)", "ppf_constant_interval_swap",
          "allocation_failed_since", "store_rounded_result")),
        ("src/constant_log2.c", constant_log2,
         ("ppf_owned_buffer_calloc_internal", "scratch_all")),
        ("src/constant_pi.c", constant_pi,
         ("PPF_CORE_MALLOC", "g_pi_core_context", "pi_core_set_context",
          "g_adic_factors = 0")),
        ("src/polyphase_chudnovsky_core.c", chudnovsky_core,
         ("ppf_core_free_gmp_string", "mp_get_memory_functions",
          "ppf_core_strdup")),
        ("src/special_common.c", special_common,
         ("ppf_context_clone(&wide, ctx)", "PPF_FLAG_ALLOC")),
        ("tests/test_constant_allocation_failure.c", constant_test,
         ("test_evaluation_atomicity", "test_precompute_cache_atomicity",
          "test_interval_output_atomicity", "test_public_constant_controls",
          "constant allocation failure atomicity: PASS")),
    ):
        for token in tokens:
            if token not in text:
                fail(f"{relative} lacks certified-constant allocation token {token}", failures)
    if 'constant_allocation_failure "hardening;semantic"' not in tests_cmake:
        fail("constant allocation-failure matrix is not registered under hardening and semantic", failures)
    for relative, text, tokens in (
        ("src/result_transaction.c", result_transaction,
         ("ppf_result_transaction_begin_internal", "commit_same_precision",
          "ppf_result_transaction_pair_finish_internal",
          "tls_result_transaction_context", "saved_alloc_flag")),
        ("src/transcendental_special_transaction.c", transaction_wrappers,
         ("ppf_log_uncommitted_internal", "ppf_sin_cos_uncommitted_internal",
          "ppf_lgamma_uncommitted_internal", "ppf_ai_uncommitted_internal")),
        ("src/transcendental_fast.c", transcendental_fast,
         ("ppf_owned_buffer_calloc_internal",
          "ppf_result_transaction_context_internal")),
        ("src/trig_fast.c", trig_fast,
         ("tbs_workspace_array_init", "workspace_count",
          "ppf_owned_buffer_calloc_internal")),
        ("src/special_gamma.c", special_gamma,
         ("ppf_owned_buffer_calloc_internal", "ppf_owned_buffer_free_internal")),
        ("src/special_misc.c", special_misc,
         ("ppf_owned_buffer_calloc_internal", "ppf_owned_buffer_free_internal")),
        ("tests/test_transcendental_special_allocation_failure.c",
         transcendental_special_test,
         ("function_cases", "test_public_endpoint_atomicity",
          "test_alias_failure_atomicity", "test_external_storage_atomicity",
          "test_high_precision_workspaces",
          "test_sticky_allocation_flag_recovery",
          "transcendental and special allocation atomicity: PASS")),
    ):
        for token in tokens:
            if token not in text:
                fail(
                    f"{relative} lacks transcendental/special allocation token {token}",
                    failures,
                )
    if transaction_wrappers.count("ppf_result_transaction_begin_internal") < 79:
        fail("transactional wrapper surface does not cover all 79 governed endpoints", failures)
    if 'transcendental_special_allocation_failure "hardening;semantic"' not in tests_cmake:
        fail(
            "transcendental/special allocation matrix is not registered under hardening and semantic",
            failures,
        )
    for relative, text, tokens in (
        ("src/result_transaction.c", result_transaction,
         ("ppf_result_transaction_begin_inline_internal",
          "PPF_RESULT_TRANSACTION_INLINE_LIMBS",
          "ppf_result_transaction_many_finish_internal")),
        ("src/arithmetic_root_transaction.c", arithmetic_root_wrappers,
         ("ppf_add_uncommitted_internal",
          "ppf_fma_algorithm_threads_uncommitted_internal",
          "ppf_hypot_uncommitted_internal",
          "ppf_div_with_plan_many_uncommitted_internal",
          "ppf_result_transaction_many_finish_internal")),
        ("src/transactional_arithmetic_root_names.h", arithmetic_root_names,
         ("#define ppf_add ppf_add_uncommitted_internal",
          "#define ppf_div_with_plan_many ppf_div_with_plan_many_uncommitted_internal",
          "#define ppf_hypot ppf_hypot_uncommitted_internal")),
        ("tests/test_arithmetic_root_allocation_failure.c", arithmetic_root_test,
         ("function_cases", "test_all_public_endpoints",
          "test_inplace_failure_and_sticky_recovery",
          "test_external_storage_transaction", "test_batch_same_index_alias",
          "atomic_flag", "FAULT_STATE_INITIALIZER",
          "arithmetic, fused-operation, and root allocation transactions passed")),
    ):
        for token in tokens:
            if token not in text:
                fail(
                    f"{relative} lacks arithmetic/fused/root allocation token {token}",
                    failures,
                )
    if len(re.findall(r"^int ppf_[A-Za-z0-9_]+\(", arithmetic_root_wrappers, re.M)) != 56:
        fail("arithmetic/fused/root transactional wrapper surface is not exactly 56 endpoints", failures)
    if arithmetic_root_names.count("_uncommitted_internal") != 56:
        fail("arithmetic/fused/root private-name mapping is not exactly 56 endpoints", failures)
    if 'arithmetic_root_allocation_failure "hardening;semantic"' not in tests_cmake:
        fail(
            "arithmetic/fused/root allocation matrix is not registered under hardening and semantic",
            failures,
        )


def audit_optional_openmp_and_fuzzers(root: Path, failures: list[str]) -> None:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    compat = (root / "src/ppf_openmp_compat.h").read_text(encoding="utf-8")
    config = (root / "cmake/PolyphaseFloatConfig.cmake.in").read_text(encoding="utf-8")
    fuzz_cmake = (root / "cmake/PolyphaseFuzzers.cmake").read_text(encoding="utf-8")

    for token in (
        'set(PPF_OPENMP "AUTO"', 'STRINGS AUTO ON OFF',
        "PPF_OPENMP_ENABLED", "PPF_BUILD_FUZZERS",
        "requires an isolated static-core build",
    ):
        if token not in cmake:
            fail(f"CMake optional-OpenMP/fuzzer policy lacks {token}", failures)
    for token in ("PPF_OPENMP ?= AUTO", "PPF_OPENMP_PROBE", "PPF_OPENMP_ENABLED"):
        if token not in makefile:
            fail(f"Make optional-OpenMP policy lacks {token}", failures)
    for token in ("#ifdef _OPENMP", "PPF_OMP_PRAGMA", "omp_get_max_threads"):
        if token not in compat:
            fail(f"OpenMP compatibility header lacks {token}", failures)
    if "if(@PPF_INSTALLED_WITH_OPENMP@)" not in config:
        fail("installed CMake package does not condition OpenMP dependency", failures)

    allowed = root / "src/ppf_openmp_compat.h"
    for base in (root / "src", root / "benchmarks", root / "experiments"):
        for source in base.rglob("*"):
            if not source.is_file() or source.suffix not in {".c", ".h"}:
                continue
            text = source.read_text(encoding="utf-8")
            if source != allowed and "#include <omp.h>" in text:
                fail(f"direct omp.h inclusion outside compatibility layer: {source.relative_to(root)}", failures)
            if "#pragma omp" in text:
                fail(f"raw OpenMP pragma bypasses compatibility layer: {source.relative_to(root)}", failures)

    for token in ("fuzz_text_parser", "fuzz_fpif_import", "-fsanitize=fuzzer,address,undefined"):
        if token not in fuzz_cmake:
            fail(f"fuzzer build lacks {token}", failures)
    for relative in (
        "fuzz/fuzz_text_parser.c", "fuzz/fuzz_fpif_import.c",
        "fuzz/corpus/text/zero.txt", "fuzz/corpus/fpif/valid-zero.ppfif",
    ):
        if not (root / relative).is_file():
            fail(f"missing fuzzer source or seed: {relative}", failures)

    expected_corpora = {
        "fuzz/corpus/text": {
            "decimal-extreme.txt", "hex-extreme.txt", "nan.txt", "zero.txt"
        },
        "fuzz/corpus/fpif": {
            "bad-magic.bin", "truncated-header.bin", "valid-zero.ppfif"
        },
    }
    for relative, expected_names in expected_corpora.items():
        directory = root / relative
        actual_names = {path.name for path in directory.iterdir() if path.is_file()}
        if actual_names != expected_names:
            fail(
                f"immutable release corpus contains unexpected files: {relative}: "
                f"{sorted(actual_names - expected_names)}",
                failures,
            )


def audit_thread_safety(root: Path, failures: list[str]) -> None:
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    context = (root / "src/context.c").read_text(encoding="utf-8")
    internal = (root / "src/ppf_internal.h").read_text(encoding="utf-8")
    special = (root / "src/special_common.c").read_text(encoding="utf-8")
    toom = (root / "src/toom3.c").read_text(encoding="utf-8")
    test = (root / "tests/test_thread_safety.c").read_text(encoding="utf-8")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    presets = (root / "CMakePresets.json").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    tests_cmake = (root / "cmake/PolyphaseTests.cmake").read_text(encoding="utf-8")
    ci = (root / ".github/workflows/ci.yml").read_text(encoding="utf-8")

    for token in ("ppf_thread_cleanup(", "ppf_buildopt_thread_cleanup_p("):
        if token not in header:
            fail(f"public thread-lifecycle API lacks {token}", failures)
    for token in (
        "pthread_key_create", "ppf_thread_cleanup_destructor",
        "ppf_thread_state_touch", "tls_cleanup_registered",
        "ctx == &tls_context", "block->allocation",
        "block->allocation_bytes", "alignment_slack",
    ):
        if token not in context:
            fail(f"thread-lifecycle implementation lacks {token}", failures)
    if "ppf_thread_state_touch" not in internal:
        fail("internal thread-state registration declaration is missing", failures)
    for token in (
        "static ppf_constant_spec_t euler_spec, catalan_spec",
        "pthread_once_t special_once", "special_cache_init_once",
        "pthread_once(&special_once",
    ):
        if token not in special:
            fail(f"special-constant descriptor hardening lacks {token}", failures)
    if special.count("euler_spec.id =") != 1 or special.count("catalan_spec.id =") != 1:
        fail("special-constant descriptors are not initialized exactly once", failures)
    if toom.count("ppf_thread_state_touch();") < 2:
        fail("direct-limb Toom entry points do not register TLS cleanup", failures)

    for token in (
        "run_context_isolation", "run_thread_exit_cleanup",
        "run_explicit_context_toom_cleanup", "run_concurrent_cache_cleanup",
        "run_shared_read_only_state", "run_arena_alignment",
        "PPF_THREAD_STRESS_MULTIPLIER", "live_blocks", "live_bytes",
        "ppf_divisor_plan_applications",
        "thread safety and cache lifecycle tests passed",
    ):
        if token not in test:
            fail(f"thread-safety regression lacks {token}", failures)
    if 'thread_safety "threading;hardening"' not in tests_cmake:
        fail("thread-safety test is not registered under threading and hardening", failures)
    if "RUN_SERIAL TRUE" not in tests_cmake:
        fail("thread-safety CTest is not marked process-serial", failures)
    if "PPF_THREAD_STRESS_MULTIPLIER=4" not in tests_cmake:
        fail("ThreadSanitizer CTest lacks the stress multiplier", failures)
    for token in (
        "PPF_ENABLE_THREAD_SANITIZER", "-fsanitize=thread",
        "ThreadSanitizer validation requires PPF_OPENMP=OFF",
    ):
        if token not in cmake:
            fail(f"CMake ThreadSanitizer policy lacks {token}", failures)
    if presets.count('"name": "tsan"') < 2:
        fail("CMake presets lack configure, build, or test tsan entries", failures)
    if "tsan:" not in makefile or "PPF_THREAD_STRESS_MULTIPLIER=4" not in makefile:
        fail("Make ThreadSanitizer target or stress policy is missing", failures)
    for token in ("thread-sanitizer:", "PPF_THREAD_STRESS_MULTIPLIER=8"):
        if token not in ci:
            fail(f"CI ThreadSanitizer job lacks {token}", failures)


def audit_abi_stability(root: Path, failures: list[str]) -> None:
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    core_map = (root / "abi/polyphase_float.map").read_text(encoding="utf-8")
    adapter_map = (root / "abi/polyphase_float_mpfr.map").read_text(encoding="utf-8")
    policy = (root / "docs/ABI_POLICY.md").read_text(encoding="utf-8")

    for token in (
        "#define PPF_ABI_EPOCH 1",
        "#define PPF_ABI_REVISION 0",
        '#define PPF_ABI_TAG "polyphase-float-abi-1"',
        "ppf_get_abi_info(",
        "ppf_abi_contract_sha256(",
    ):
        if token not in header:
            fail(f"public ABI interface lacks {token}", failures)

    contract = root / "abi/contract-v1.json"
    epoch_baseline = root / "abi/baselines/0.11.0-contract.json"
    previous_baseline = root / f"abi/baselines/{PREVIOUS_RELEASE}-contract.json"
    compare = root / f"abi/{PREVIOUS_RELEASE}-to-{CURRENT_RELEASE}-comparison.json"
    commands = (
        [sys.executable, str(root / "tools/abi_contract.py"), "verify",
         "--contract", str(contract), "--root", str(root)],
        [sys.executable, str(root / "tools/abi_contract.py"), "compare",
         "--baseline", str(epoch_baseline), "--current", str(contract)],
        [sys.executable, str(root / "tools/abi_contract.py"), "compare",
         "--baseline", str(previous_baseline), "--current", str(contract)],
    )
    for command in commands:
        result = subprocess.run(command, cwd=root, text=True, capture_output=True)
        if result.returncode != 0:
            fail("ABI contract audit failed:\n" + result.stdout + result.stderr, failures)

    try:
        comparison = json.loads(compare.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid ABI comparison report: {exc}", failures)
    else:
        if comparison.get("compatible") is not True or comparison.get("breaking") != []:
            fail(f"{CURRENT_RELEASE} previous-release ABI comparison is not additive", failures)
        additions = {
            (item.get("category"), item.get("name"))
            for item in comparison.get("compatible_additions", [])
        }
        if additions != set():
            fail(f"{CURRENT_RELEASE} ABI comparison has unexpected additions", failures)
        metadata = {
            (item.get("category"), item.get("name"))
            for item in comparison.get("compatible_metadata_changes", [])
        }
        if metadata != {
            ("constants", "PPF_SEMANTIC_CONTRACT_SHA256"),
            ("constants", "PPF_ALIASING_CONTRACT_SHA256"),
        }:
            fail(f"{CURRENT_RELEASE} ABI comparison has unexpected metadata changes", failures)

    contract_data = json.loads(contract.read_text(encoding="utf-8"))
    contract_data.pop("contract_sha256", None)
    contract_data.pop("release_version", None)
    expected_hash = hashlib.sha256(
        json.dumps(contract_data, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    ).hexdigest()
    macro = re.search(
        r'PPF_ABI_CONTRACT_SHA256\s+\\?\s*\n?\s*"([0-9a-f]{64})"',
        header,
    )
    if not macro or macro.group(1) != expected_hash:
        fail("public ABI contract hash macro does not match abi/contract-v1.json", failures)

    previous_contract_token = f"abi/baselines/{PREVIOUS_RELEASE}-contract.json"
    if previous_contract_token not in cmake or previous_contract_token not in makefile:
        fail(f"configured ABI checks do not target the {PREVIOUS_RELEASE} previous release", failures)

    for token in ("PPF_0.11", "PPF_0.12", "local:", "*;"):
        if token not in core_map:
            fail(f"core ELF version map lacks {token}", failures)
    if "PPF_MPFR_0.11" not in adapter_map or "local:" not in adapter_map or "*;" not in adapter_map:
        fail("adapter ELF version map is incomplete", failures)
    for token in ("PPF_ELF_SYMBOL_VERSIONING", "--version-script"):
        if token not in cmake or token not in makefile:
            fail(f"build systems do not both govern {token}", failures)
    if "rm -rf build bin lib" in makefile or "rm -rf build polyphase-float.pc" in makefile:
        fail("Make clean still removes unrelated CMake build trees", failures)
    for token in ("ABI epoch 1", "libpolyphase_float.so.0", "PPF_0.11", "PPF_0.12"):
        if token not in policy:
            fail(f"ABI policy lacks {token}", failures)

def audit_semantic_contract(root: Path, failures: list[str]) -> tuple[int, int, int]:
    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    contract_path = root / "semantics/contract-v1.json"
    matrix_path = root / "semantics/function-matrix-v1.csv"
    alias_contract_path = root / "semantics/aliasing-contract-v1.json"
    alias_matrix_path = root / "semantics/aliasing-matrix-v1.csv"
    commands = (
        [sys.executable, str(root / "tools/semantic_contract.py"), "verify",
         "--root", str(root), "--contract", str(contract_path)],
        [sys.executable, str(root / "tests/test_semantic_contract.py"),
         "--root", str(root)],
        [sys.executable, str(root / "tools/aliasing_contract.py"), "verify",
         "--root", str(root), "--contract", str(alias_contract_path)],
        [sys.executable, str(root / "tests/test_aliasing_contract.py"),
         "--root", str(root)],
    )
    for command in commands:
        result = subprocess.run(command, cwd=root, text=True, capture_output=True)
        if result.returncode != 0:
            fail("semantic or aliasing contract audit failed:\n" + result.stdout + result.stderr, failures)

    try:
        data = json.loads(contract_path.read_text(encoding="utf-8"))
        alias_data = json.loads(alias_contract_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid semantic or aliasing contract: {exc}", failures)
        return 0, 0, 0
    summary = data.get("summary", {})
    alias_summary = alias_data.get("summary", {})
    function_count = summary.get("function_count", 0)
    direct_tests = summary.get("functions_with_direct_test_reference", 0)
    gap_entries = summary.get("entries_with_explicit_gaps", 0)
    if data.get("release_version") != CURRENT_RELEASE or data.get("contract_version") != 1:
        fail("semantic contract version boundary is incorrect", failures)
    if alias_data.get("release_version") != CURRENT_RELEASE or alias_data.get("contract_version") != 1:
        fail("aliasing contract version boundary is incorrect", failures)
    if function_count != 437 or direct_tests != 437 or alias_summary.get("function_count") != 437:
        fail("semantic and aliasing contracts do not cover all 437 public functions", failures)
    if summary.get("conditional_functions") != 6:
        fail("semantic contract conditional-function count is not six", failures)
    if summary.get("explicit_gap_counts") != {
        "bounded_implementation_path": 115,
        "conditional_build_surface": 6,
    }:
        fail(f"semantic contract gap counts differ from the reviewed {CURRENT_RELEASE} ledger", failures)
    if gap_entries != 121 or alias_summary.get("unverified_aliasing_functions") != 0:
        fail(f"semantic or aliasing gap summary differs from the reviewed {CURRENT_RELEASE} ledger", failures)
    if summary.get("atomicity_classes", {}).get("verified_transactional") != 192:
        fail(f"semantic contract transactional-function count differs from the reviewed {CURRENT_RELEASE} ledger", failures)

    macro_version = re.search(r"#define\s+PPF_SEMANTIC_CONTRACT_VERSION\s+(\d+)", header)
    macro_hash = re.search(
        r'PPF_SEMANTIC_CONTRACT_SHA256\s+\\?\s*\n?\s*"([0-9a-f]{64})"', header)
    alias_version = re.search(r"#define\s+PPF_ALIASING_CONTRACT_VERSION\s+(\d+)", header)
    alias_hash = re.search(
        r'PPF_ALIASING_CONTRACT_SHA256\s+\\?\s*\n?\s*"([0-9a-f]{64})"', header)
    if not macro_version or int(macro_version.group(1)) != data.get("contract_version"):
        fail("public semantic contract version macro is inconsistent", failures)
    if not macro_hash or macro_hash.group(1) != data.get("contract_sha256"):
        fail("public semantic contract hash macro is inconsistent", failures)
    if not alias_version or int(alias_version.group(1)) != alias_data.get("contract_version"):
        fail("public aliasing contract version macro is inconsistent", failures)
    if not alias_hash or alias_hash.group(1) != alias_data.get("contract_sha256"):
        fail("public aliasing contract hash macro is inconsistent", failures)

    try:
        with matrix_path.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        with alias_matrix_path.open(newline="", encoding="utf-8") as handle:
            alias_rows = list(csv.DictReader(handle))
    except OSError as exc:
        fail(f"unable to read semantic or aliasing matrix: {exc}", failures)
        rows, alias_rows = [], []
    contract_names = {entry.get("name") for entry in data.get("functions", [])}
    matrix_names = {row.get("function") for row in rows}
    alias_names = {row.get("function") for row in alias_rows}
    if len(rows) != 437 or len(alias_rows) != 437 or matrix_names != contract_names or alias_names != contract_names:
        fail("semantic and aliasing CSV/JSON function inventories differ", failures)

    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    tests_cmake = (root / "cmake/PolyphaseTests.cmake").read_text(encoding="utf-8")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    for token in (
        "semantic_contract_current", "semantic_contract_policy",
        "aliasing_contract_current", "aliasing_contract_policy",
        "semantic-contract-audit", "semantics/function-matrix-v1.csv",
        "semantics/aliasing-matrix-v1.csv",
    ):
        if token not in cmake:
            fail(f"CMake semantic/aliasing integration lacks {token}", failures)
    for token in ("test_semantic_surface", "test_aliasing_matrix"):
        if token not in tests_cmake:
            fail(f"CMake does not register {token}", failures)
    for token in (
        "semantic-check:", "aliasing-check:", "bin/test_semantic_surface",
        "bin/test_aliasing_matrix", "semantics/contract-v1.json",
        "semantics/aliasing-contract-v1.json",
    ):
        if token not in makefile:
            fail(f"Make semantic/aliasing integration lacks {token}", failures)
    return int(function_count), int(direct_tests), int(gap_entries)

def audit_current_claims(root: Path, failures: list[str]) -> None:
    current_files = (
        "README.md",
        "RELEASE_NOTES.md",
        "STATUS.md",
        "README_FIRST.txt",
        "CMakeLists.txt",
        "CITATION.cff",
        "docs/PUBLIC_RELEASE_STATUS.md",
        "docs/PUBLIC_RELEASE_HARDENING_REPORT.md",
    )
    stale_patterns = (
        "No redistribution license has been selected",
        "208/349",
        "243/349",
        "265/349",
        "279/349",
        "311/349",
        "0.10.0-runtime-complete",
        "example.invalid",
        "CHANGE_ME",
        "PLACEHOLDER_URL",
    )
    for relative in current_files:
        text = (root / relative).read_text(encoding="utf-8")
        for pattern in stale_patterns:
            if pattern in text:
                fail(f"stale top-level claim {pattern!r} in {relative}", failures)


def audit_api(root: Path, library: Path | None, failures: list[str]) -> tuple[int, int]:
    command = [sys.executable, str(root / "scripts/check_api_coverage.py")]
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    if result.returncode != 0:
        fail("API coverage script failed:\n" + result.stdout + result.stderr, failures)

    header = (root / "include/polyphase_float.h").read_text(encoding="utf-8")
    declarations = set(
        re.findall(r"PPF_API\s+[^;]*?\b(ppf_[a-z0-9_]+)\s*\(", header, re.S)
    )
    export_count = 0

    if library is not None:
        if not library.exists():
            fail(f"requested library does not exist: {library}", failures)
            return len(declarations), export_count
        independence = subprocess.run(
            [sys.executable, str(root / "tools/audit_independence.py"), str(library)],
            cwd=root,
            text=True,
            capture_output=True,
        )
        if independence.returncode != 0:
            fail("independence audit failed:\n" + independence.stdout + independence.stderr, failures)

        try:
            nm = subprocess.run(
                ["nm", "-D", "--defined-only", str(library)],
                check=True,
                text=True,
                capture_output=True,
            )
        except (OSError, subprocess.CalledProcessError) as exc:
            fail(f"unable to inspect shared-library exports: {exc}", failures)
        else:
            exports = set(re.findall(r"\b(ppf_[a-z0-9_]+)(?:@@?[A-Za-z0-9_.-]+)?$", nm.stdout, re.M))
            export_count = len(exports)
            try:
                expected, features = enabled_core_declarations(library, declarations)
            except (OSError, AttributeError, RuntimeError) as exc:
                fail(f"unable to query native core-library conditional capabilities: {exc}", failures)
                expected = declarations
            else:
                print("core_conditional_capabilities=" + json.dumps(features, sort_keys=True))
                print(f"enabled_core_declarations={len(expected)}")
            if exports != expected:
                missing = sorted(expected - exports)
                extra = sorted(exports - expected)
                fail(
                    "header/export mismatch: "
                    f"indexed={len(declarations)} enabled={len(expected)} exported={len(exports)} "
                    f"missing={missing[:10]} extra={extra[:10]}",
                    failures,
                )
    return len(declarations), export_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="source-tree root",
    )
    parser.add_argument(
        "--library",
        type=Path,
        help="built production shared library to inspect",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    failures: list[str] = []

    for relative in REQUIRED_FILES:
        if not (root / relative).is_file():
            fail(f"missing required public-release file: {relative}", failures)

    version = parse_version(root, failures) if not failures else None
    spdx_count = audit_spdx(root, failures)
    audit_hygiene(root, failures)
    audit_configuration_syntax(root, failures)
    deprecated_scan_count = audit_deprecated_mpfr_calls(root, failures)
    audit_mpfr_precision_model(root, failures)
    audit_mpfr_optional_formats(root, failures)
    audit_benchmark_evidence(root, failures)
    audit_product_planner_changeset(root, failures)
    changed_count, added_count, removed_count = audit_current_production_changeset(root, failures)
    audit_install_packaging(root, failures)
    audit_product_profile(root, failures)
    audit_allocation_hardening(root, failures)
    audit_optional_openmp_and_fuzzers(root, failures)
    audit_thread_safety(root, failures)
    audit_abi_stability(root, failures)
    semantic_functions, semantic_direct_tests, semantic_gap_entries = audit_semantic_contract(root, failures)
    audit_current_claims(root, failures)
    declaration_count, export_count = audit_api(
        root,
        args.library.resolve() if args.library else None,
        failures,
    )

    if failures:
        print("PUBLIC RELEASE AUDIT: FAILED", file=sys.stderr)
        for item in failures:
            print(f"- {item}", file=sys.stderr)
        return 1

    print("PUBLIC RELEASE AUDIT: PASS")
    print(f"version={version}")
    print("license=LGPL-3.0-or-later")
    print(f"spdx_files_checked={spdx_count}")
    print(f"deprecated_mpfr_source_files_checked={deprecated_scan_count}")
    print("deprecated_mpfr_call_sites=0")
    print("mpfr_precision_type_model=signed")
    print("checked_precision_conversion=present")
    print("mpfr_optional_format_opt_ins=present")
    print("mpfr_optional_format_link_probes=present")
    print("mpfr_optional_format_make_probes=present")
    print("mpfr_fpif_export_signature=official")
    print("benchmark_record_schema=2")
    print("benchmark_raw_sample_retention=required")
    print("benchmark_schema_smoke_test=present")
    print("product_planner_changeset=verified_historical")
    print("current_production_changeset=verified")
    print("relocated_installed_consumer_regression=registered")
    print(f"production_source_files={len([p for p in (root / 'src').rglob('*') if p.is_file()])}")
    baseline_token = PREVIOUS_RELEASE.replace(".", "_")
    print(f"production_source_files_changed_from_{baseline_token}={changed_count}")
    print(f"production_source_files_added_from_{baseline_token}={added_count}")
    print(f"production_source_files_removed_from_{baseline_token}={removed_count}")
    print("allocation_failure_flag=present")
    print("allocation_transaction_test=present")
    print("io_allocation_commit_matrix=present")
    print("constant_allocation_commit_matrix=present")
    print("transcendental_special_allocation_commit_matrix=present")
    print("arithmetic_fused_root_allocation_commit_matrix=present")
    print("openmp_policy=AUTO_ON_OFF")
    print("serial_execution_path=present")
    print("parser_and_fpif_fuzzers=present")
    print("thread_ownership_contract=present")
    print("automatic_thread_cleanup=present")
    print("thread_sanitizer_configuration=present")
    print("concurrent_cache_lifecycle_test=present")
    print("abi_epoch=1")
    print("abi_baseline=0.11.0")
    print("abi_contract_comparison=additive")
    print("elf_symbol_versioning=governed")
    print("old_binary_compatibility_test=present")
    print("semantic_contract_version=1")
    print("aliasing_contract_version=1")
    print("aliasing_contract_unverified_functions=0")
    print(f"semantic_contract_functions={semantic_functions}")
    print(f"semantic_contract_direct_test_references={semantic_direct_tests}")
    print(f"semantic_contract_entries_with_explicit_gaps={semantic_gap_entries}")
    print(f"public_function_declarations={declaration_count}")
    if args.library:
        print(f"shared_library_exports={export_count}")
        print("production_mpfr_imports=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
