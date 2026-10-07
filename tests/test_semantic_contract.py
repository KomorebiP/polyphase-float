#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Regression checks for the generated public semantic contract."""
from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def load_generator(root: Path):
    path = root / "tools" / "semantic_contract.py"
    spec = importlib.util.spec_from_file_location("ppf_semantic_contract", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def entry_map(data: dict) -> dict[str, dict]:
    return {entry["name"]: entry for entry in data["functions"]}


def macro_string(header: str, name: str) -> str:
    pattern = rf'#define\s+{re.escape(name)}\s+(?:\\\s*)?"([0-9a-f]+)"'
    match = re.search(pattern, header)
    if not match:
        raise AssertionError(f"missing string macro {name}")
    return match.group(1)


def macro_integer(header: str, name: str) -> int:
    match = re.search(rf"#define\s+{re.escape(name)}\s+(\d+)", header)
    if not match:
        raise AssertionError(f"missing integer macro {name}")
    return int(match.group(1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    generator = load_generator(root)
    data = generator.generate(root)
    failures = generator.validate(data)
    assert not failures, "\n".join(failures)

    assert data["release_version"] == "0.18.0"
    assert data["contract_version"] == 1
    assert data["summary"]["function_count"] == 437
    assert data["summary"]["functions_with_direct_test_reference"] == 437
    assert data["summary"]["conditional_functions"] == 6
    assert data["summary"]["explicit_gap_counts"] == {
        "allocation_failure_atomicity_incomplete": 81,
        "bounded_implementation_path": 112,
        "conditional_build_surface": 6,
    }
    assert data["summary"]["atomicity_classes"]["verified_transactional"] == 213
    assert "no_direct_test_reference" not in data["summary"]["explicit_gap_counts"]

    transactional_family = generator.TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_FUNCTIONS
    arithmetic_root_family = generator.ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_FUNCTIONS
    assert len(transactional_family) == 79
    assert len(arithmetic_root_family) == 56

    functions = entry_map(data)
    expected_conditional = {
        "ppf_set_float128", "ppf_get_float128",
        "ppf_set_decimal64", "ppf_get_decimal64",
        "ppf_set_decimal128", "ppf_get_decimal128",
    }
    actual_conditional = {
        name for name, entry in functions.items()
        if entry["availability"]["classification"] == "conditional"
    }
    assert actual_conditional == expected_conditional

    assert functions["ppf_context_init"]["context"]["policy"] == "explicit_required"
    assert functions["ppf_init2_ctx"]["context"]["policy"] == "explicit_required"
    assert functions["ppf_context_set_range"]["context"]["policy"] == "explicit_or_thread_default"
    assert functions["ppf_init"]["context"]["policy"] == "thread_default"
    assert functions["ppf_default_allocator"]["flags"]["may_set"] == []
    assert functions["ppf_add"]["return_contract"]["kind"] == "ternary"
    assert functions["ppf_add"]["aliasing"]["classification"] == "verified_in_place"
    assert functions["ppf_sin_cos"]["aliasing"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_div_with_plan_many"]["aliasing"]["classification"] == "verified_same_index_overlap"
    assert functions["ppf_init_set"]["aliasing"]["classification"] == "source_must_be_distinct"
    assert "aliasing_not_individually_verified" not in data["summary"]["explicit_gap_counts"]
    assert functions["ppf_get_str"]["ownership"]["classification"] == "caller_owned_string"
    assert functions["ppf_get_str"]["return_contract"]["kind"] == "owned_pointer"
    io_atomic = {
        "ppf_set_str", "ppf_strtofr", "ppf_get_str", "ppf_free_str",
        "ppf_out_str", "ppf_inp_str", "ppf_fpif_export", "ppf_fpif_import",
        "ppf_asprintf", "ppf_vasprintf", "ppf_snprintf", "ppf_vsnprintf",
        "ppf_sprintf", "ppf_vsprintf", "ppf_fprintf", "ppf_vfprintf",
        "ppf_printf", "ppf_vprintf",
    }
    for name in io_atomic:
        assert functions[name]["failure_atomicity"]["classification"] in {
            "verified_transactional", "not_applicable"
        }, name
    for name in transactional_family:
        assert functions[name]["failure_atomicity"]["classification"] == \
            "verified_transactional", name
        assert "tests/test_transcendental_special_allocation_failure.c" in \
            functions[name]["evidence"]["tests"], name
    for name in arithmetic_root_family:
        assert functions[name]["failure_atomicity"]["classification"] == \
            "verified_transactional", name
        assert "tests/test_arithmetic_root_allocation_failure.c" in \
            functions[name]["evidence"]["tests"], name
    assert functions["ppf_ai"]["practical_limits"]["classification"] == "implementation_bound"
    assert "500" in functions["ppf_ai"]["practical_limits"]["summary"]

    surface_test = root / "tests" / "test_semantic_surface.c"
    assert surface_test.exists()
    surface_text = surface_test.read_text(encoding="utf-8")
    previously_unreferenced = {
        "ppf_default_allocator", "ppf_context_flags_save", "ppf_context_flags_restore",
        "ppf_get_emin_max", "ppf_get_emax_min", "ppf_set_default_rounding_mode",
        "ppf_round_nearest_away_finish", "ppf_set_flt", "ppf_init_set_q",
        "ppf_get_ld_2exp", "ppf_fits_sshort_p", "ppf_mul_threads",
        "ppf_fms_algorithm_threads", "ppf_dot_threads", "ppf_set_trig_threads",
        "ppf_const_euler_uncached", "ppf_const_catalan_uncached",
        "ppf_precompute_constant", "ppf_get_constant_threads", "ppf_dump",
        "ppf_rounding_name", "ppf_kind_name",
    }
    for name in previously_unreferenced:
        assert re.search(rf"\b{re.escape(name)}\b", surface_text), name

    cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    make_text = (root / "Makefile").read_text(encoding="utf-8")
    assert 'doc/PolyphaseFloat/semantics' in cmake_text
    assert 'doc/PolyphaseFloat/abi' in cmake_text
    assert '$(DOCDIR)/semantics' in make_text
    assert '$(DOCDIR)/abi' in make_text
    assert 'semantics/contract-v1.json semantics/function-matrix-v1.csv semantics/aliasing-contract-v1.json semantics/aliasing-matrix-v1.csv $(DESTDIR)$(DOCDIR)/semantics/' in make_text
    assert 'abi/contract-v1.json $(DESTDIR)$(DOCDIR)/abi/' in make_text

    header = (root / "include" / "polyphase_float.h").read_text(encoding="utf-8")
    assert macro_integer(header, "PPF_SEMANTIC_CONTRACT_VERSION") == data["contract_version"]
    assert macro_string(header, "PPF_SEMANTIC_CONTRACT_SHA256") == data["contract_sha256"]

    print(
        "semantic contract policy passed: "
        f"functions={data['summary']['function_count']} "
        f"direct_tests={data['summary']['functions_with_direct_test_reference']} "
        f"explicit_gap_entries={data['summary']['function_count'] - data['summary']['entries_without_recorded_gap']}"
    )


if __name__ == "__main__":
    main()
