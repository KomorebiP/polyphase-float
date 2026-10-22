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


def check_special_classifications(generator, data: dict, root: Path) -> None:
    """Guard result channels that C signatures alone cannot distinguish."""
    functions = entry_map(data)
    packed = {"ppf_modf", "ppf_sin_cos", "ppf_sinh_cosh", "ppf_grandom"}
    assert {name for name, entry in functions.items()
            if entry["return_contract"]["kind"] == "packed_ternary"} == packed
    for name in packed:
        result = functions[name]["return_contract"]
        assert result["error_sentinel"] == "PPF_TERNARY_ERROR", name
        assert "code(first) + 4 * code(second)" in result["note"], name
        assert "0 means exact, 1 means above" in result["note"], name
        assert "2 means below" in result["note"], name
    assert "integral output first" in functions["ppf_modf"]["return_contract"]["note"]
    assert "rop2 is NULL its component code is 0" in functions["ppf_grandom"]["return_contract"]["note"]
    for name, error in {
        "ppf_div_with_plan_many": "-1",
        "ppf_get_constant_interval": "-1",
        "ppf_urandomb": "1",
    }.items():
        result = functions[name]["return_contract"]
        assert result["kind"] == "status", name
        assert result["error_sentinel"] == error, name
        assert f"Returns 0 on success and {error} on failure" in result["note"], name
    assert "ternaries array" in functions["ppf_div_with_plan_many"]["return_contract"]["note"]
    for name in {"ppf_get_z", "ppf_get_f"}:
        result = functions[name]["return_contract"]
        assert result["kind"] == "ternary", name
        assert result["error_sentinel"] == "PPF_TERNARY_ERROR", name
    assert functions["ppf_eq"]["return_contract"]["kind"] == "predicate"
    assert functions["ppf_signbit"]["return_contract"]["kind"] == "predicate"
    assert "NaN maps to zero with ERANGE and return 0" in functions["ppf_get_f"]["return_contract"]["note"]
    for name in {"ppf_context_set_rounding", "ppf_set_default_rounding_mode"}:
        rounding = functions[name]["rounding"]
        assert rounding["kind"] == "explicit", name
        assert rounding["parameter"] is not None, name
        assert rounding["accepts_default"] is False, name
        assert "PPF_RND_DEFAULT is rejected" in rounding["note"], name
    assert functions["ppf_add"]["rounding"]["accepts_default"] is True
    assert "supplied context or the calling thread default context" in functions["ppf_add"]["rounding"]["note"]
    # No-context rounding arguments do not all imply thread-default policy.
    # The two name helpers inspect enumeration values rather than select a mode.
    rounding_name = functions["ppf_rounding_name"]
    assert rounding_name["rounding"]["kind"] == "not_applicable"
    assert rounding_name["rounding"]["accepts_default"] is True
    assert "without selecting or resolving" in rounding_name["rounding"]["note"]
    assert rounding_name["return_contract"]["error_sentinel"] is None
    assert "PPF_RND_INVALID" in rounding_name["return_contract"]["note"]
    print_mode = functions["ppf_print_rnd_mode"]
    assert print_mode["rounding"]["kind"] == "not_applicable"
    assert print_mode["rounding"]["accepts_default"] is False
    assert print_mode["return_contract"]["error_sentinel"] == "NULL"
    assert "PPF_RND_DEFAULT and invalid" in print_mode["return_contract"]["note"]
    can_round = functions["ppf_can_round"]["rounding"]
    assert can_round["kind"] == "explicit"
    assert can_round["parameter"] == "source_rounding"
    assert can_round["accepts_default"] is False
    assert "source_rounding and target_rounding must both be concrete" in can_round["note"]
    assert "either parameter is rejected with return 0" in can_round["note"]
    assert len(generator.PRIVATE_CONTEXT_FITS_FUNCTIONS) == 8
    for name in generator.PRIVATE_CONTEXT_FITS_FUNCTIONS:
        rounding = functions[name]["rounding"]
        assert rounding["kind"] == "explicit", name
        assert rounding["accepts_default"] is True, name
        assert "PPF_RND_DEFAULT resolves to PPF_RNDN" in rounding["note"], name
        assert "newly initialized private context" in rounding["note"], name
        assert functions[name]["context"]["policy"] == "none", name

    bounded_roots = {
        "ppf_rootn_ui", "ppf_rootn_si", "ppf_root", "ppf_cbrt",
        "ppf_rec_sqrt", "ppf_rec_sqrt_threads", "ppf_rec_sqrt_algorithm_threads",
        "ppf_hypot", "ppf_sqrt", "ppf_sqrt_ui",
    }
    for name in bounded_roots:
        assert functions[name]["practical_limits"]["classification"] == "implementation_bound", name
    for name in {"ppf_rec_sqrt", "ppf_rec_sqrt_threads", "ppf_rec_sqrt_algorithm_threads"}:
        assert "not a shared global total of 10" in functions[name]["practical_limits"]["summary"], name
    for name in {"ppf_sqrt", "ppf_sqrt_ui"}:
        assert "24 attempts" in functions[name]["practical_limits"]["summary"], name
    for name in {"ppf_add", "ppf_sum"}:
        assert functions[name]["practical_limits"]["classification"] == "none_declared", name

    # Every non-numerical allocation transaction needs its own output-channel
    # qualification. In particular, NULL publication is not pointer rollback.
    nonnumerical = {
        name for name, entry in functions.items()
        if entry["failure_atomicity"]["classification"] == "verified_transactional"
        and not entry["arguments"]["destinations"]
        and not entry["arguments"]["inout_numbers"]
    }
    assert len(nonnumerical) == 21
    for name in nonnumerical:
        assert functions[name]["failure_atomicity"].get("note"), name
    for name in {"ppf_asprintf", "ppf_vasprintf"}:
        note = functions[name]["failure_atomicity"]["note"]
        assert "leaves *text NULL" in note, name
        assert "%n writes" in note and "not rolled back" in note, name
    for name in {"ppf_fprintf", "ppf_vfprintf", "ppf_printf", "ppf_vprintf"}:
        note = functions[name]["failure_atomicity"]["note"]
        assert "stream write failure may leave partial output" in note, name
    for name in {"ppf_inp_str", "ppf_fpif_import"}:
        assert "bytes already consumed" in functions[name]["failure_atomicity"]["note"], name
    for name in {"ppf_get_constant_interval", "ppf_precompute_constant"}:
        assert "GMP allocation" in functions[name]["failure_atomicity"]["note"], name
    # Source notes qualify evidence; they cannot grant the tested status when
    # the operation has no direct failure-injection evidence.
    for function in generator.parse_header(root / "include/polyphase_float.h"):
        if function["name"] in nonnumerical:
            function["section"] = functions[function["name"]]["section"]
            contract = generator.atomicity_contract(function, [])
            assert contract["classification"] != "verified_transactional"
            assert "note" not in contract


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    generator = load_generator(root)
    data = generator.generate(root)
    failures = generator.validate(data)
    assert not failures, "\n".join(failures)

    assert data["release_version"] == "0.20.0"
    assert data["contract_version"] == 1
    assert data["summary"]["function_count"] == 437
    assert data["summary"]["functions_with_direct_test_reference"] == 437
    assert data["summary"]["conditional_functions"] == 6
    assert "allocation_failure_atomicity_incomplete" not in data["summary"]["explicit_gap_counts"]
    assert data["summary"]["explicit_gap_counts"]["bounded_implementation_path"] == 115
    assert data["summary"]["explicit_gap_counts"]["conditional_build_surface"] == 6
    assert data["summary"]["atomicity_classes"]["verified_transactional"] == 192
    assert data["summary"]["atomicity_classes"]["reviewed_allocation_free"] == 63
    assert data["summary"]["atomicity_classes"]["verified_initialization_lifecycle"] == 12
    assert data["summary"]["atomicity_classes"]["verified_prefix_cleanup"] == 2
    assert "no_direct_test_reference" not in data["summary"]["explicit_gap_counts"]

    transactional_family = generator.TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_FUNCTIONS
    arithmetic_root_family = generator.ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_FUNCTIONS
    assert len(transactional_family) == 79
    assert len(arithmetic_root_family) == 56

    functions = entry_map(data)
    check_special_classifications(generator, data, root)
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
        "ppf_set_str", "ppf_get_str", "ppf_free_str",
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
    for path, names in generator.ALLOCATION_ATOMICITY_TEST_FUNCTIONS.items():
        assert (root / path).exists(), path
        for name in names:
            assert functions[name]["failure_atomicity"]["classification"] == \
                "verified_transactional", name
            assert path in functions[name]["evidence"]["tests"], (path, name)

    # Setup/cleanup helpers mentioned in failure suites do not thereby gain
    # an independently verified allocation-failure contract.
    for name in {"ppf_set_ui", "ppf_set_d", "ppf_set_ui_2exp",
                 "ppf_validate", "ppf_context_init", "ppf_clear"}:
        assert functions[name]["failure_atomicity"]["classification"] != \
            "verified_transactional", name
    assert functions["ppf_strtofr"]["failure_atomicity"]["classification"] == \
        "verified_transactional"
    for name in generator.REVIEWED_ALLOCATION_FREE_FUNCTIONS:
        assert functions[name]["failure_atomicity"]["classification"] == "reviewed_allocation_free", name
        assert generator.ALLOCATION_COMPLETION_TEST in functions[name]["evidence"]["tests"], name
        assert "GMP" in functions[name]["failure_atomicity"]["note"], name
    for name in generator.INITIALIZATION_LIFECYCLE_FUNCTIONS:
        assert functions[name]["failure_atomicity"]["classification"] == "verified_initialization_lifecycle", name
        assert generator.ALLOCATION_COMPLETION_TEST in functions[name]["evidence"]["tests"], name
    for name in generator.PREFIX_CLEANUP_FUNCTIONS:
        assert functions[name]["failure_atomicity"]["classification"] == "verified_prefix_cleanup", name
        assert "unvisited" in functions[name]["failure_atomicity"]["note"], name
        assert "libc" in functions[name]["failure_atomicity"]["note"], name
    # A token in an unrelated test must not grant a reviewed classification.
    for function in generator.parse_header(root / "include" / "polyphase_float.h"):
        if function["name"] in generator.REVIEWED_ALLOCATION_FREE_FUNCTIONS:
            function["section"] = functions[function["name"]]["section"]
            assert generator.atomicity_contract(function, ["tests/test_object.c"])["classification"] != "reviewed_allocation_free"
    assert functions["ppf_fpif_import"]["practical_limits"]["classification"] == "implementation_bound"
    assert "4096" in functions["ppf_fpif_import"]["practical_limits"]["summary"]
    for name in {"ppf_set_trig_threads", "ppf_get_trig_threads",
                 "ppf_clear_constant_cache", "ppf_free_constant_caches",
                 "ppf_constant_cache_precision", "ppf_set_constant_threads",
                 "ppf_get_constant_threads", "ppf_get_last_constant_stats"}:
        assert functions[name]["practical_limits"]["classification"] == \
            "none_declared", name
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
