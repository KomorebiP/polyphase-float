#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Generate and validate the Polyphase Float public semantic contract.

The contract deliberately separates facts parsed from the public header from
family-level policy and direct test evidence.  An explicit partial or
not-yet-verified classification is preferred to an inferred guarantee.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)

RETURN_KINDS = {
    "void", "ternary", "status", "predicate", "comparison", "sign",
    "count", "value", "pointer", "owned_pointer", "borrowed_pointer",
    "interval_status", "formatted_count"
}
ROUNDING_KINDS = {
    "explicit", "fixed_by_operation", "context_default", "not_applicable"
}
CONTEXT_POLICIES = {
    "explicit_required", "explicit_or_thread_default", "explicit_object",
    "thread_default", "none"
}
ALIASING_CLASSES = {
    "not_applicable", "verified_in_place", "expected_in_place_unverified",
    "verified_distinct_outputs", "verified_same_index_overlap",
    "source_must_be_distinct", "lifecycle_destination",
    "mutable_object", "direct_storage", "application_managed"
}
THREAD_CLASSES = {
    "thread_local", "caller_owned_context", "read_only_shared",
    "distinct_objects", "global_synchronized", "application_lock_required",
    "immutable_plan", "process_metadata", "not_applicable"
}
OWNERSHIP_CLASSES = {
    "no_transfer", "borrowed_thread_local", "borrowed_static",
    "borrowed_object_storage", "context_owned", "caller_owned_string",
    "caller_owned_output", "plan_owned", "application_owned_storage",
    "releases_resource", "value_copy"
}
ATOMICITY_CLASSES = {
    "not_applicable", "verified_transactional", "documented_partial",
    "not_yet_verified", "initialization_safe_zero", "reviewed_allocation_free",
    "verified_initialization_lifecycle", "verified_prefix_cleanup"
}

STATUS_FUNCTIONS = {
    "ppf_context_init", "ppf_context_init_custom", "ppf_context_clone",
    "ppf_context_set_default_precision", "ppf_context_set_range",
    "ppf_context_set_rounding", "ppf_set_emin", "ppf_set_emax",
    "ppf_init", "ppf_init2", "ppf_init2_ctx", "ppf_inits2", "ppf_inits",
    "ppf_reserve", "ppf_set_prec", "ppf_set_default_prec",
    "ppf_commit_regular", "ppf_set_default_rounding_mode", "ppf_set_str",
    "ppf_init_set_str", "ppf_fpif_export", "ppf_fpif_import",
    "ppf_get_abi_info", "ppf_mp_memory_cleanup", "ppf_custom_init_set",
    "ppf_plan_product", "ppf_divisor_plan_create",
    "ppf_divisor_plan_create_ex", "ppf_precompute_pi",
    "ppf_precompute_log2", "ppf_precompute_euler",
    "ppf_precompute_catalan", "ppf_precompute_constant",
    "ppf_get_last_constant_stats", "ppf_set_exp"
}
FORMATTED_COUNT_FUNCTIONS = {
    "ppf_fprintf", "ppf_vfprintf", "ppf_printf", "ppf_vprintf",
    "ppf_sprintf", "ppf_vsprintf", "ppf_snprintf", "ppf_vsnprintf",
    "ppf_asprintf", "ppf_vasprintf"
}
COMPARISON_FUNCTIONS = {
    "ppf_cmp", "ppf_cmp_ui", "ppf_cmp_si", "ppf_cmp_d", "ppf_cmp_ld",
    "ppf_cmp_z", "ppf_cmp_q", "ppf_cmp_f", "ppf_cmp_ui_2exp",
    "ppf_cmp_si_2exp", "ppf_cmpabs", "ppf_cmpabs_ui"
}
SIGN_FUNCTIONS = {"ppf_sgn"}
PREDICATE_EXTRA = {
    "ppf_can_round", "ppf_validate", "ppf_integer_p", "ppf_equal_p",
    "ppf_greater_p", "ppf_greaterequal_p", "ppf_less_p",
    "ppf_lessequal_p", "ppf_lessgreater_p", "ppf_unordered_p",
    "ppf_total_order_p"
}
COUNT_FUNCTIONS = {"ppf_out_str", "ppf_inp_str", "ppf_get_str_ndigits"}
OWNED_POINTER_FUNCTIONS = {"ppf_get_str"}
BORROWED_POINTER_FUNCTIONS = {
    "ppf_default_context", "ppf_limbs_read", "ppf_limbs_write",
    "ppf_custom_get_significand"
}
BORROWED_STRING_FUNCTIONS = {
    "ppf_print_rnd_mode", "ppf_get_version", "ppf_abi_tag",
    "ppf_abi_contract_sha256", "ppf_get_patches",
    "ppf_buildopt_tune_case", "ppf_product_profile_name",
    "ppf_product_profile_sha256", "ppf_rounding_name", "ppf_kind_name"
}
FIXED_ROUNDING_FUNCTIONS = {
    "ppf_ceil", "ppf_floor", "ppf_round", "ppf_roundeven", "ppf_trunc",
    "ppf_rint_ceil", "ppf_rint_floor", "ppf_rint_round",
    "ppf_rint_roundeven", "ppf_rint_trunc"
}
THREAD_LOCAL_FUNCTIONS = {
    "ppf_default_context", "ppf_default_context_cleanup", "ppf_thread_cleanup",
    "ppf_set_default_prec", "ppf_get_default_prec",
    "ppf_set_default_rounding_mode", "ppf_get_default_rounding_mode",
    "ppf_get_emin", "ppf_get_emax", "ppf_set_emin", "ppf_set_emax",
    "ppf_clear_underflow", "ppf_clear_overflow", "ppf_clear_divby0",
    "ppf_clear_nanflag", "ppf_clear_inexflag", "ppf_clear_erangeflag",
    "ppf_clear_allocflag", "ppf_clear_flags", "ppf_set_underflow",
    "ppf_set_overflow", "ppf_set_divby0", "ppf_set_nanflag",
    "ppf_set_inexflag", "ppf_set_erangeflag", "ppf_set_allocflag",
    "ppf_underflow_p", "ppf_overflow_p", "ppf_divby0_p", "ppf_nanflag_p",
    "ppf_inexflag_p", "ppf_erangeflag_p", "ppf_allocflag_p",
    "ppf_flags_clear", "ppf_flags_set", "ppf_flags_test", "ppf_flags_save",
    "ppf_flags_restore", "ppf_free_pool"
}
GLOBAL_SYNCHRONIZED_PREFIXES = (
    "ppf_const_", "ppf_precompute_", "ppf_clear_constant_cache",
    "ppf_free_constant_caches", "ppf_constant_cache_precision",
    "ppf_set_constant_threads", "ppf_get_constant_threads",
    "ppf_get_last_constant_stats", "ppf_get_constant_interval"
)
DISTINCT_OUTPUT_FUNCTIONS = {
    "ppf_modf", "ppf_grandom", "ppf_sin_cos", "ppf_sinh_cosh",
    "ppf_get_constant_interval"
}
SAME_INDEX_OVERLAP_FUNCTIONS = {"ppf_div_with_plan_many"}
SOURCE_MUST_BE_DISTINCT_FUNCTIONS = {
    "ppf_init_set", "ppf_round_nearest_away_finish"
}
LIFECYCLE_DESTINATION_FUNCTIONS = {
    "ppf_init", "ppf_init2", "ppf_init2_ctx", "ppf_inits",
    "ppf_inits2", "ppf_clears", "ppf_init_set", "ppf_init_set_ui",
    "ppf_init_set_si", "ppf_init_set_uj", "ppf_init_set_sj",
    "ppf_init_set_flt", "ppf_init_set_d", "ppf_init_set_ld",
    "ppf_init_set_z", "ppf_init_set_q", "ppf_init_set_f",
    "ppf_init_set_str"
}

MUTABLE_OBJECT_FUNCTIONS = {
    "ppf_clear", "ppf_reserve", "ppf_set_prec", "ppf_prec_round",
    "ppf_swap", "ppf_set_prec_raw", "ppf_limbs_write", "ppf_commit_regular",
    "ppf_set_zero", "ppf_set_inf", "ppf_set_nan", "ppf_nexttoward",
    "ppf_nextabove", "ppf_nextbelow", "ppf_set_exp", "ppf_custom_move",
    "ppf_check_range", "ppf_subnormalize"
}
DIRECT_STORAGE_FUNCTIONS = {
    "ppf_limb_count", "ppf_limbs_read", "ppf_limbs_write",
    "ppf_commit_regular", "ppf_custom_get_size", "ppf_custom_init",
    "ppf_custom_init_set", "ppf_custom_get_kind",
    "ppf_custom_get_significand", "ppf_custom_get_exp", "ppf_custom_move"
}
PLAN_FUNCTIONS = {
    "ppf_divisor_plan_create", "ppf_divisor_plan_create_ex",
    "ppf_divisor_plan_destroy", "ppf_divisor_plan_precision",
    "ppf_divisor_plan_applications", "ppf_div_with_plan",
    "ppf_div_with_plan_many"
}

CONTEXT_REQUIRED_FUNCTIONS = {
    "ppf_context_init", "ppf_context_init_custom", "ppf_context_clone",
    "ppf_context_clear", "ppf_init2_ctx"
}
IMPLICIT_DEFAULT_CONTEXT_FUNCTIONS = {
    "ppf_init", "ppf_init2", "ppf_inits", "ppf_inits2"
}
DIRECT_FLAG_CONTROL_FUNCTIONS = {
    "ppf_context_flags", "ppf_context_flags_clear", "ppf_context_flags_set",
    "ppf_context_flags_save", "ppf_context_flags_restore",
    "ppf_clear_underflow", "ppf_clear_overflow", "ppf_clear_divby0",
    "ppf_clear_nanflag", "ppf_clear_inexflag", "ppf_clear_erangeflag",
    "ppf_clear_allocflag", "ppf_clear_flags", "ppf_set_underflow",
    "ppf_set_overflow", "ppf_set_divby0", "ppf_set_nanflag",
    "ppf_set_inexflag", "ppf_set_erangeflag", "ppf_set_allocflag",
    "ppf_underflow_p", "ppf_overflow_p", "ppf_divby0_p", "ppf_nanflag_p",
    "ppf_inexflag_p", "ppf_erangeflag_p", "ppf_allocflag_p",
    "ppf_flags_clear", "ppf_flags_set", "ppf_flags_test", "ppf_flags_save",
    "ppf_flags_restore"
}
NO_FLAG_SIDE_EFFECT_FUNCTIONS = {
    "ppf_default_allocator", "ppf_context_init", "ppf_context_init_custom",
    "ppf_context_clone", "ppf_context_clear", "ppf_default_context_cleanup",
    "ppf_thread_cleanup", "ppf_context_set_default_precision",
    "ppf_context_set_range", "ppf_context_set_rounding", "ppf_arena_mark",
    "ppf_arena_release", "ppf_arena_reset", "ppf_set_default_prec",
    "ppf_get_default_prec", "ppf_set_default_rounding_mode",
    "ppf_get_default_rounding_mode", "ppf_get_emin", "ppf_get_emax",
    "ppf_set_emin", "ppf_set_emax", "ppf_get_emin_min",
    "ppf_get_emin_max", "ppf_get_emax_min", "ppf_get_emax_max",
    "ppf_get_version", "ppf_get_patches", "ppf_buildopt_tls_p",
    "ppf_buildopt_decimal_p", "ppf_buildopt_gmpinternals_p",
    "ppf_buildopt_sharedcache_p", "ppf_buildopt_thread_safe_p",
    "ppf_buildopt_float128_p", "ppf_buildopt_thread_cleanup_p",
    "ppf_buildopt_tune_case", "ppf_abi_epoch", "ppf_abi_revision",
    "ppf_abi_tag", "ppf_abi_contract_sha256", "ppf_abi_info_size",
    "ppf_rounding_name", "ppf_kind_name", "ppf_print_rnd_mode"
}

DOMAIN_OVERRIDES = {
    "ppf_log": "finite regular inputs require x > 0; zero and negative inputs follow documented exceptional-value semantics",
    "ppf_log2": "finite regular inputs require x > 0",
    "ppf_log10": "finite regular inputs require x > 0",
    "ppf_log1p": "finite regular inputs require x > -1",
    "ppf_log2p1": "finite regular inputs require x > -1",
    "ppf_log10p1": "finite regular inputs require x > -1",
    "ppf_sqrt": "finite regular inputs require x >= 0",
    "ppf_sqrt_ui": "defined for every unsigned input",
    "ppf_rec_sqrt": "finite regular inputs require x > 0",
    "ppf_rec_sqrt_threads": "finite regular inputs require x > 0",
    "ppf_rec_sqrt_algorithm_threads": "finite regular inputs require x > 0",
    "ppf_cbrt": "defined for finite real inputs",
    "ppf_rootn_ui": "degree must be nonzero; even degrees require nonnegative finite input",
    "ppf_rootn_si": "degree must be nonzero; even degrees require nonnegative finite input; negative degrees additionally exclude zero",
    "ppf_root": "degree must be nonzero; even degrees require nonnegative finite input",
    "ppf_powr": "real-valued power function requiring a nonnegative base, with zero cases handled separately",
    "ppf_acos": "finite regular inputs require -1 <= x <= 1",
    "ppf_asin": "finite regular inputs require -1 <= x <= 1",
    "ppf_acosu": "finite regular inputs require -1 <= x <= 1 and u != 0",
    "ppf_asinu": "finite regular inputs require -1 <= x <= 1 and u != 0",
    "ppf_atanu": "u must be nonzero",
    "ppf_acospi": "finite regular inputs require -1 <= x <= 1",
    "ppf_asinpi": "finite regular inputs require -1 <= x <= 1",
    "ppf_atanh": "finite real result requires -1 < x < 1; endpoints and exterior values follow exceptional semantics",
    "ppf_acosh": "finite real inputs require x >= 1",
    "ppf_gamma": "real input excluding nonpositive integer poles",
    "ppf_lngamma": "real input excluding nonpositive integer poles when a finite real logarithm is required",
    "ppf_lgamma": "real input; sign output records the sign of Gamma away from poles",
    "ppf_digamma": "real input excluding nonpositive integer poles",
    "ppf_beta": "real arguments where the Gamma quotient is finite; singular combinations follow exceptional semantics",
    "ppf_zeta": "real input with a pole at x = 1",
    "ppf_y0": "finite real series path requires x > 0",
    "ppf_y1": "finite real series path requires x > 0",
    "ppf_yn": "finite real series path requires x > 0",
    "ppf_ai": "implemented real-domain path is restricted to |x| <= 500; infinities are handled separately",
    "ppf_divisor_plan_create": "denominator must be finite and nonzero; destination precision must be valid",
    "ppf_divisor_plan_create_ex": "denominator must be finite and nonzero; destination precision must be valid",
    "ppf_div_with_plan": "plan must be complete and numerator valid",
    "ppf_div_with_plan_many": "plan and every output/numerator entry must be valid",
    "ppf_set_dyadic_interval": "lower and upper are exact integers with lower <= upper and share the supplied binary exponent",
}

OWNERSHIP_OVERRIDES = {
    "ppf_default_allocator": ("value_copy", "returns allocator callbacks by value"),
    "ppf_default_context": ("borrowed_thread_local", "pointer remains owned by the calling thread and is invalid after thread cleanup"),
    "ppf_arena_alloc": ("context_owned", "storage remains owned by the context until release, reset, or clear"),
    "ppf_get_str": ("caller_owned_string", "when the caller supplies no buffer, the returned string retains the resolved context allocator provenance and must be released only with ppf_free_str"),
    "ppf_free_str": ("releases_resource", "releases a string allocated by ppf_get_str"),
    "ppf_asprintf": ("caller_owned_output", "on success *text is allocated with the process allocator and must be released with free"),
    "ppf_vasprintf": ("caller_owned_output", "on success *text is allocated with the process allocator and must be released with free"),
    "ppf_limbs_read": ("borrowed_object_storage", "pointer is invalidated by mutation, reallocation, or clear"),
    "ppf_limbs_write": ("borrowed_object_storage", "pointer is writable internal storage and must be followed by a valid commit when representation fields change"),
    "ppf_custom_get_significand": ("borrowed_object_storage", "returns the currently attached application-owned significand storage"),
    "ppf_custom_init": ("application_owned_storage", "initializes caller-provided storage without taking ownership"),
    "ppf_custom_init_set": ("application_owned_storage", "attaches caller-provided storage without taking ownership"),
    "ppf_custom_move": ("application_owned_storage", "moves the object view to replacement caller-owned storage"),
    "ppf_divisor_plan_create": ("plan_owned", "successful creation returns a plan released by ppf_divisor_plan_destroy"),
    "ppf_divisor_plan_create_ex": ("plan_owned", "successful creation returns a plan released by ppf_divisor_plan_destroy"),
    "ppf_divisor_plan_destroy": ("releases_resource", "releases a completed or partially constructed divisor plan"),
}

PRACTICAL_LIMITS = {
    "assignment_text": {
        "summary": "absolute parsed exponent magnitudes above 2,000,000 are constructed only when the result is already unambiguously outside the configured binary range",
        "source": "src/assignment.c:PPF_TEXT_POWER_LIMIT"
    },
    "transcendental": {
        "summary": "certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject",
        "source": "src/transcendental.c:PPF_TRANS_MAX_ATTEMPTS,PPF_TRANS_MAX_STEPS,PPF_TRANS_MAX_ALIGNMENT_BITS"
    },
    "trigonometric": {
        "summary": "general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps",
        "source": "src/trig_hyperbolic.c:PPF_TRIG_MAX_ATTEMPTS,PPF_TRIG_MAX_REDUCTION_EXTRA,PPF_TRIG_MAX_STEPS"
    },
    "special": {
        "summary": "special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates",
        "source": "src/special_common.c,src/special_gamma.c,src/special_bessel.c"
    },
    "constants": {
        "summary": "constant certification uses at most 16 precision-escalation attempts per request",
        "source": "src/constants.c:PPF_CONSTANT_MAX_ATTEMPTS"
    },
    "random": {
        "summary": "adaptive random-distribution rounding uses at most 12 precision-escalation attempts",
        "source": "src/random.c:PPF_RANDOM_MAX_ATTEMPTS"
    }
}


def strip_comments(text: str) -> str:
    return COMMENT_RE.sub("", text)


def normalize(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def split_parameters(text: str) -> list[str]:
    if normalize(text) in {"", "void"}:
        return []
    out: list[str] = []
    start = 0
    depth = 0
    for i, char in enumerate(text):
        if char in "([":
            depth += 1
        elif char in ")]":
            depth -= 1
        elif char == "," and depth == 0:
            out.append(normalize(text[start:i]))
            start = i + 1
    out.append(normalize(text[start:]))
    return out


def parameter_record(raw: str) -> dict[str, Any]:
    if raw == "...":
        return {"name": "...", "type": "variadic", "declaration": raw, "role": "variadic"}
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$", raw)
    if not m:
        raise RuntimeError(f"cannot parse parameter: {raw}")
    name = m.group(1)
    type_text = normalize(raw[:m.start(1)] + raw[m.end(1):])
    type_text = type_text.replace(" [", "[")
    role = "input"
    if "ppf_context_t" in raw:
        role = "context"
    elif "ppf_rnd_t" in raw:
        role = "rounding"
    elif re.search(r"\bppf_t\s*\*", raw) and "const ppf_t" not in raw:
        role = "mutable_number"
    elif "const ppf_t" in raw:
        role = "source_number"
    elif "*" in raw and "const" not in raw and not raw.startswith("FILE *"):
        role = "output_or_inout"
    elif raw.startswith("FILE *"):
        role = "stream"
    return {"name": name, "type": type_text, "declaration": raw, "role": role}


def parse_header(path: Path) -> list[dict[str, Any]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    stack: list[str] = []
    buffer: list[str] = []
    condition = ""
    functions: list[dict[str, Any]] = []
    ignored = {"POLYPHASE_FLOAT_H", "__cplusplus"}
    for raw in lines:
        line = raw.strip()
        if line.startswith("#if "):
            stack.append(line[4:].strip()); continue
        if line.startswith("#ifdef "):
            name = line[7:].strip(); stack.append("" if name in ignored else f"defined({name})"); continue
        if line.startswith("#ifndef "):
            name = line[8:].strip(); stack.append("" if name in ignored else f"!defined({name})"); continue
        if line.startswith("#elif "):
            if stack: stack[-1] = line[6:].strip()
            continue
        if line == "#else":
            if stack: stack[-1] = f"!({stack[-1]})"
            continue
        if line == "#endif":
            if stack: stack.pop()
            continue
        if not buffer and raw.lstrip().startswith("PPF_API "):
            buffer = [raw.strip()]
            condition = " && ".join(x for x in stack if x)
        elif buffer:
            buffer.append(raw.strip())
        if buffer and ";" in raw:
            declaration = normalize(strip_comments(" ".join(buffer))).replace("PPF_API ", "", 1)
            m = re.match(r"(.+?)\b(ppf_[A-Za-z0-9_]+)\s*\((.*)\);$", declaration)
            if not m:
                raise RuntimeError(f"cannot parse declaration: {declaration}")
            return_type = normalize(m.group(1))
            name = m.group(2)
            params = [parameter_record(x) for x in split_parameters(m.group(3))]
            functions.append({
                "name": name,
                "declaration": declaration,
                "return_type": return_type,
                "parameters": params,
                "condition": condition,
            })
            buffer = []
            condition = ""
    return functions


def load_purposes(path: Path) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    result = {row["function"]: row for row in rows}
    if len(result) != len(rows):
        raise RuntimeError("duplicate function names in function-purposes.csv")
    return result


def direct_references(root: Path, names: list[str], subdir: str) -> dict[str, list[str]]:
    refs = {name: [] for name in names}
    base = root / subdir
    if not base.exists():
        return refs
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in {".c", ".h", ".py", ".md"}:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for name in names:
            if re.search(rf"\b{re.escape(name)}\b", text):
                refs[name].append(str(path.relative_to(root)))
    return refs


def return_contract(function: dict[str, Any]) -> dict[str, Any]:
    name = function["name"]
    ret = function["return_type"]
    if ret == "void":
        kind, sentinel = "void", None
    elif ret == "ppf_interval_status_t":
        kind, sentinel = "interval_status", "PPF_INTERVAL_ERROR"
    elif name in FORMATTED_COUNT_FUNCTIONS:
        kind, sentinel = "formatted_count", "negative value"
    elif name in COUNT_FUNCTIONS or ret == "size_t":
        kind, sentinel = "count", "0 when the operation cannot produce a count"
    elif name in COMPARISON_FUNCTIONS:
        kind, sentinel = "comparison", None
    elif name in SIGN_FUNCTIONS:
        kind, sentinel = "sign", None
    elif name.endswith("_p") or name in PREDICATE_EXTRA:
        kind, sentinel = "predicate", None
    elif name in STATUS_FUNCTIONS:
        kind, sentinel = "status", "function-specific nonzero failure status"
    elif name in OWNED_POINTER_FUNCTIONS:
        kind, sentinel = "owned_pointer", "NULL"
    elif name in BORROWED_POINTER_FUNCTIONS or name in BORROWED_STRING_FUNCTIONS:
        kind, sentinel = "borrowed_pointer", "NULL where documented"
    elif "*" in ret:
        kind, sentinel = "pointer", "NULL where documented"
    elif ret == "int" and any(p["role"] == "mutable_number" for p in function["parameters"]):
        kind, sentinel = "ternary", "PPF_TERNARY_ERROR"
    else:
        kind, sentinel = "value", None
    assert kind in RETURN_KINDS
    return {"kind": kind, "error_sentinel": sentinel}


def rounding_contract(function: dict[str, Any]) -> dict[str, Any]:
    name = function["name"]
    params = function["parameters"]
    explicit = next((p["name"] for p in params if p["role"] == "rounding"), None)
    if explicit:
        return {"kind": "explicit", "parameter": explicit,
                "accepts_default": True,
                "note": "PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context"}
    if name in FIXED_ROUNDING_FUNCTIONS:
        return {"kind": "fixed_by_operation", "parameter": None,
                "accepts_default": False,
                "note": "the function name selects the integer-rounding rule"}
    if name in {"ppf_urandomb"}:
        return {"kind": "fixed_by_operation", "parameter": None,
                "accepts_default": False,
                "note": "the generated significand is exact at the destination precision"}
    if any(p["role"] == "mutable_number" for p in params) and name in {
        "ppf_init", "ppf_init2", "ppf_init2_ctx", "ppf_inits", "ppf_inits2",
        "ppf_set_zero", "ppf_set_inf", "ppf_set_nan", "ppf_custom_init_set",
        "ppf_nextabove", "ppf_nextbelow", "ppf_nexttoward", "ppf_set_exp"
    }:
        return {"kind": "not_applicable", "parameter": None,
                "accepts_default": False, "note": "no significand rounding mode is selected"}
    return {"kind": "not_applicable", "parameter": None,
            "accepts_default": False, "note": "the operation does not select a floating-point rounding mode"}


def context_contract(function: dict[str, Any]) -> dict[str, Any]:
    name = function["name"]
    params = function["parameters"]
    context = next((p for p in params if p["role"] == "context"), None)
    if context:
        if name in CONTEXT_REQUIRED_FUNCTIONS:
            return {"policy": "explicit_required", "parameter": context["name"],
                    "null_uses_thread_default": False}
        const = "const ppf_context_t" in context["declaration"]
        return {"policy": "explicit_object" if const else "explicit_or_thread_default",
                "parameter": context["name"],
                "null_uses_thread_default": not const}
    if name in THREAD_LOCAL_FUNCTIONS or name in IMPLICIT_DEFAULT_CONTEXT_FUNCTIONS:
        return {"policy": "thread_default", "parameter": None,
                "null_uses_thread_default": True}
    return {"policy": "none", "parameter": None,
            "null_uses_thread_default": False}


def classify_arguments(function: dict[str, Any]) -> dict[str, Any]:
    name = function["name"]
    mutable = [p["name"] for p in function["parameters"] if p["role"] == "mutable_number"]
    sources = [p["name"] for p in function["parameters"] if p["role"] == "source_number"]
    outputs = [p["name"] for p in function["parameters"] if p["role"] == "output_or_inout"]
    if name in MUTABLE_OBJECT_FUNCTIONS:
        destinations: list[str] = []
        inout = mutable
    else:
        destinations = mutable
        inout = []
    return {
        "destinations": destinations,
        "inout_numbers": inout,
        "source_numbers": sources,
        "other_outputs_or_inout": outputs,
    }


def flag_contract(function: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    section = function["section"]
    name = function["name"]
    if name in DIRECT_FLAG_CONTROL_FUNCTIONS:
        return {"may_set": [], "classification": "direct_flag_control",
                "note": "the function directly queries or modifies the selected flag register"}
    if name in NO_FLAG_SIDE_EFFECT_FUNCTIONS or context["policy"] == "none":
        return {"may_set": [], "classification": "none_or_not_applicable",
                "note": "no caller-visible numerical flag side effect is declared"}
    if section in {
        "Assignment and parsing", "Initialize-and-set shortcuts",
        "Conversion to C and GMP types", "Arithmetic and product planning",
        "Roots and hypotenuse", "Reusable division, fused operations, powers of two, sums, and dot products",
        "Logarithms, exponentials, and powers", "Trigonometric and hyperbolic functions",
        "Special functions", "Certified constants and constant caches",
        "Certified interval assignment", "Integer rounding, fractional parts, and remainders"
    }:
        return {"may_set": ["underflow", "overflow", "divide_by_zero", "nan", "inexact", "erange", "allocation"],
                "classification": "family_upper_bound",
                "note": "conservative family-level upper bound; not every listed flag is reachable for every input"}
    if section in {"Initialization and precision", "Contexts, allocators, flags, and scratch memory", "Text I/O, PPFIF interchange, formatting, and fit tests", "Random values, build information, cache control, and custom storage"}:
        return {"may_set": ["nan", "inexact", "erange", "allocation"],
                "classification": "family_upper_bound",
                "note": "conservative family-level upper bound; not every listed flag is reachable for every input"}
    if section in {"Comparison, ordering, sign, and absolute value", "Core comparison, validation, and diagnostics", "Neighboring values, min/max, and exponent access", "Special values, signs, and rounding control"}:
        return {"may_set": ["nan", "inexact", "erange", "allocation"],
                "classification": "family_upper_bound",
                "note": "conservative family-level upper bound; not every listed flag is reachable for every input"}
    return {"may_set": [], "classification": "none_or_not_applicable",
            "note": "no caller-visible numerical flag side effect is declared"}


def aliasing_contract(function: dict[str, Any], test_refs: list[str]) -> dict[str, Any]:
    name = function["name"]
    args = classify_arguments(function)
    sources = args["source_numbers"]
    mutable_count = len(args["destinations"]) + len(args["inout_numbers"])
    evidence = [p for p in test_refs if "alias" in p or "allocation_failure" in p]
    allowed: list[str] = []
    forbidden: list[str] = []
    invalid = "not applicable"

    if name in DIRECT_STORAGE_FUNCTIONS:
        cls = "direct_storage"
        allowed = ["overlap is governed by the advanced direct-storage preconditions"]
        forbidden = ["overlap that violates the documented storage-size, commit, or ownership preconditions"]
        invalid = "application-managed; invalid direct-storage use is outside the ordinary arithmetic aliasing contract"
    elif name in DISTINCT_OUTPUT_FUNCTIONS:
        cls = "verified_distinct_outputs"
        if name in {"ppf_modf", "ppf_sin_cos", "ppf_sinh_cosh"}:
            allowed = ["either floating output may alias the floating input individually"]
            forbidden = ["the two floating outputs must not be the same object"]
            invalid = "returns the function error sentinel before modifying the shared output object"
        elif name == "ppf_grandom":
            allowed = ["the optional second output may be NULL"]
            forbidden = ["the two floating outputs must not be the same object"]
            invalid = "returns PPF_TERNARY_ERROR, raises PPF_FLAG_ERANGE, and preserves the shared object"
        else:
            allowed = ["lower and upper may refer to distinct caller-owned integers"]
            forbidden = ["lower and upper must not be the same mpz object"]
            invalid = "returns failure before modifying either endpoint"
    elif name in SAME_INDEX_OVERLAP_FUNCTIONS:
        cls = "verified_same_index_overlap"
        allowed = ["outputs[i] may equal numerators[i] for each index"]
        forbidden = ["duplicate outputs and cross-index output/input overlap"]
        invalid = "returns failure and raises PPF_FLAG_ERANGE before starting the batch"
    elif name in SOURCE_MUST_BE_DISTINCT_FUNCTIONS:
        cls = "source_must_be_distinct"
        if name == "ppf_init_set":
            allowed = ["destination is uninitialized and distinct from the initialized source"]
            forbidden = ["destination and source are the same object"]
            invalid = "same-object use violates the initialization precondition and is not a supported call"
        else:
            allowed = ["destination precision is exactly one bit below the extended source precision"]
            forbidden = ["destination and extended source are the same object"]
            invalid = "returns PPF_TERNARY_ERROR without modifying the source object"
    elif name in LIFECYCLE_DESTINATION_FUNCTIONS:
        cls = "lifecycle_destination"
        allowed = ["destination storage satisfies the function's initialization or destruction precondition"]
        forbidden = ["using an already initialized object as an initialization destination unless explicitly documented"]
        invalid = "lifecycle misuse is outside the arithmetic in-place contract"
    elif name in MUTABLE_OBJECT_FUNCTIONS:
        cls = "mutable_object"
        allowed = ["the designated object is mutated in place"]
        forbidden = []
        invalid = "not applicable"
    elif mutable_count == 0:
        cls = "not_applicable"
        allowed = ["the function has no mutable ppf_t destination subject to arithmetic aliasing"]
    elif not sources:
        cls = "not_applicable"
        allowed = ["no public ppf_t source can overlap the destination"]
        forbidden = []
        invalid = "not applicable"
    elif any("alias" in path for path in test_refs) or name in {
        "ppf_set", "ppf_add", "ppf_sub", "ppf_mul", "ppf_div", "ppf_sqr",
        "ppf_sqrt", "ppf_fma", "ppf_fms", "ppf_sum", "ppf_dot"
    }:
        cls = "verified_in_place"
        allowed = ["the floating destination may alias each floating source position exercised by the aliasing matrix"]
        forbidden = []
        invalid = "not applicable for the governed in-place patterns"
    else:
        cls = "expected_in_place_unverified"
        allowed = []
        forbidden = []
        invalid = "the exact overlap behavior is not yet promoted to a public guarantee"
    return {
        "classification": cls,
        "allowed_patterns": allowed,
        "forbidden_patterns": forbidden,
        "invalid_overlap_behavior": invalid,
        "evidence": evidence,
    }


def thread_contract(function: dict[str, Any], test_refs: list[str]) -> dict[str, Any]:
    name = function["name"]
    section = function["section"]
    if name in THREAD_LOCAL_FUNCTIONS:
        cls = "thread_local"
    elif name in PLAN_FUNCTIONS:
        cls = "immutable_plan" if not name.startswith("ppf_divisor_plan_create") and name != "ppf_divisor_plan_destroy" else "application_lock_required"
    elif any(name.startswith(prefix) for prefix in GLOBAL_SYNCHRONIZED_PREFIXES) or name in {"ppf_free_cache", "ppf_free_cache2"}:
        cls = "global_synchronized"
    elif section == "Build and ABI information" or name in BORROWED_STRING_FUNCTIONS:
        cls = "process_metadata"
    elif section == "Random values, build information, cache control, and custom storage" and (name.startswith("ppf_urandom") or name in {"ppf_nrandom", "ppf_grandom", "ppf_erandom"}):
        cls = "application_lock_required"
    elif any(p["role"] == "context" for p in function["parameters"]):
        cls = "caller_owned_context"
    elif all(p["role"] != "mutable_number" for p in function["parameters"]):
        cls = "read_only_shared"
    else:
        cls = "distinct_objects"
    return {"classification": cls,
            "direct_thread_test": any("thread_safety" in path for path in test_refs)}


def ownership_contract(name: str) -> dict[str, Any]:
    if name in OWNERSHIP_OVERRIDES:
        cls, note = OWNERSHIP_OVERRIDES[name]
    elif name in BORROWED_STRING_FUNCTIONS:
        cls, note = "borrowed_static", "returned string is library-owned static storage"
    elif name in BORROWED_POINTER_FUNCTIONS:
        cls, note = "borrowed_object_storage", "returned pointer remains owned by the referenced library object"
    else:
        cls, note = "no_transfer", "the function does not transfer ownership of caller-visible storage"
    return {"classification": cls, "note": note}


# These allowlists describe operations actually subjected to injected
# allocation failures. A token reference to a setup, cleanup, or assertion
# helper in the same file is not evidence that its failure path was tested.
ALLOCATION_ATOMICITY_TEST_FUNCTIONS = {
    "tests/test_allocation_completion.c": {"ppf_strtofr", "ppf_dim"},
    "tests/test_allocation_failure.c": {
        "ppf_reserve", "ppf_set_prec", "ppf_arena_alloc",
        "ppf_divisor_plan_create_ex", "ppf_div_with_plan", "ppf_sum",
        "ppf_dot", "ppf_mul_algorithm_threads", "ppf_sqr", "ppf_div",
        "ppf_sqrt_ui", "ppf_rec_sqrt", "ppf_fpif_import",
    },
    "tests/test_io_allocation_failure.c": {
        "ppf_set_str", "ppf_get_str", "ppf_out_str", "ppf_inp_str",
        "ppf_fpif_export", "ppf_asprintf", "ppf_vasprintf",
        "ppf_snprintf", "ppf_vsnprintf", "ppf_sprintf", "ppf_vsprintf",
        "ppf_fprintf", "ppf_vfprintf", "ppf_printf", "ppf_vprintf",
    },
    "tests/test_constant_allocation_failure.c": {
        "ppf_const_pi", "ppf_const_pi_threads", "ppf_const_pi_uncached",
        "ppf_const_log2", "ppf_const_log2_threads", "ppf_const_log2_uncached",
        "ppf_const_euler", "ppf_const_euler_threads", "ppf_const_euler_uncached",
        "ppf_const_catalan", "ppf_const_catalan_threads", "ppf_const_catalan_uncached",
        "ppf_precompute_pi", "ppf_precompute_log2", "ppf_precompute_euler",
        "ppf_precompute_catalan", "ppf_precompute_constant",
        "ppf_get_constant_interval",
    },
    "tests/test_precision_remainder_failure.c": {
        "ppf_prec_round", "ppf_fmod_ui", "ppf_set_dyadic_interval",
    },
    "tests/test_api_lifecycle_audit.c": {
        "ppf_urandomb", "ppf_urandom", "ppf_erandom", "ppf_nrandom",
        "ppf_grandom", "ppf_reldiff",
    },
}

TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_TEST = \
    "tests/test_transcendental_special_allocation_failure.c"
TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_FUNCTIONS = {
    "ppf_log",
    "ppf_log_ui",
    "ppf_log2",
    "ppf_log10",
    "ppf_log1p",
    "ppf_log2p1",
    "ppf_log10p1",
    "ppf_exp",
    "ppf_exp2",
    "ppf_exp10",
    "ppf_expm1",
    "ppf_exp2m1",
    "ppf_exp10m1",
    "ppf_pow",
    "ppf_powr",
    "ppf_pow_ui",
    "ppf_pow_si",
    "ppf_pow_uj",
    "ppf_pow_sj",
    "ppf_pown",
    "ppf_pow_z",
    "ppf_ui_pow_ui",
    "ppf_ui_pow",
    "ppf_compound_si",
    "ppf_cos",
    "ppf_sin",
    "ppf_tan",
    "ppf_cosu",
    "ppf_sinu",
    "ppf_tanu",
    "ppf_cospi",
    "ppf_sinpi",
    "ppf_tanpi",
    "ppf_sin_cos",
    "ppf_sec",
    "ppf_csc",
    "ppf_cot",
    "ppf_acos",
    "ppf_asin",
    "ppf_atan",
    "ppf_acosu",
    "ppf_asinu",
    "ppf_atanu",
    "ppf_acospi",
    "ppf_asinpi",
    "ppf_atanpi",
    "ppf_atan2",
    "ppf_atan2u",
    "ppf_atan2pi",
    "ppf_cosh",
    "ppf_sinh",
    "ppf_tanh",
    "ppf_sinh_cosh",
    "ppf_sech",
    "ppf_csch",
    "ppf_coth",
    "ppf_acosh",
    "ppf_asinh",
    "ppf_atanh",
    "ppf_eint",
    "ppf_li2",
    "ppf_gamma",
    "ppf_gamma_inc",
    "ppf_lngamma",
    "ppf_lgamma",
    "ppf_digamma",
    "ppf_beta",
    "ppf_zeta",
    "ppf_zeta_ui",
    "ppf_erf",
    "ppf_erfc",
    "ppf_j0",
    "ppf_j1",
    "ppf_jn",
    "ppf_y0",
    "ppf_y1",
    "ppf_yn",
    "ppf_agm",
    "ppf_ai",
}


ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_TEST = \
    "tests/test_arithmetic_root_allocation_failure.c"
ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_FUNCTIONS = {
    "ppf_add", "ppf_add_ui", "ppf_add_si", "ppf_add_d", "ppf_add_z",
    "ppf_add_q", "ppf_sub", "ppf_ui_sub", "ppf_sub_ui", "ppf_si_sub",
    "ppf_sub_si", "ppf_d_sub", "ppf_sub_d", "ppf_z_sub", "ppf_sub_z",
    "ppf_sub_q", "ppf_mul", "ppf_mul_ui", "ppf_mul_si", "ppf_mul_d",
    "ppf_mul_z", "ppf_mul_q", "ppf_mul_threads", "ppf_sqr_threads",
    "ppf_sqr_algorithm_threads", "ppf_ui_div", "ppf_div_ui",
    "ppf_si_div", "ppf_div_si", "ppf_d_div", "ppf_div_d", "ppf_div_z",
    "ppf_div_q", "ppf_div_with_plan_many", "ppf_fma",
    "ppf_fma_threads", "ppf_fma_algorithm_threads", "ppf_fms",
    "ppf_fmma", "ppf_fmms", "ppf_fms_threads",
    "ppf_fms_algorithm_threads", "ppf_mul_2ui", "ppf_mul_2si",
    "ppf_div_2ui", "ppf_div_2si", "ppf_fac_ui", "ppf_dot_threads",
    "ppf_sqrt", "ppf_rec_sqrt_threads",
    "ppf_rec_sqrt_algorithm_threads", "ppf_cbrt", "ppf_rootn_ui",
    "ppf_rootn_si", "ppf_root", "ppf_hypot",
}


# Exact endpoint sets reviewed in docs/ALLOCATION_COMPLETION_0.20.0.md.
# Allocation-free means no project allocator is reachable for valid initialized
# storage and existing contexts. It never means GMP has no private allocation.
ALLOCATION_COMPLETION_TEST = "tests/test_allocation_completion.c"
REVIEWED_ALLOCATION_FREE_FUNCTIONS = {
    "ppf_mul_2exp",
    "ppf_div_2exp",
    'ppf_abs',
    'ppf_ceil',
    'ppf_check_range',
    'ppf_clear',
    'ppf_clears',
    'ppf_commit_regular',
    'ppf_copysign',
    'ppf_custom_init_set',
    'ppf_custom_move',
    'ppf_floor',
    'ppf_fmod',
    'ppf_fmodquo',
    'ppf_frac',
    'ppf_frexp',
    'ppf_limbs_write',
    'ppf_max',
    'ppf_min',
    'ppf_modf',
    'ppf_neg',
    'ppf_nextabove',
    'ppf_nextbelow',
    'ppf_nexttoward',
    'ppf_remainder',
    'ppf_remquo',
    'ppf_rint',
    'ppf_rint_ceil',
    'ppf_rint_floor',
    'ppf_rint_round',
    'ppf_rint_roundeven',
    'ppf_rint_trunc',
    'ppf_round',
    'ppf_round_nearest_away_finish',
    'ppf_roundeven',
    'ppf_set',
    'ppf_set_d',
    'ppf_set_decimal128',
    'ppf_set_decimal64',
    'ppf_set_exp',
    'ppf_set_f',
    'ppf_set_float128',
    'ppf_set_flt',
    'ppf_set_inf',
    'ppf_set_ld',
    'ppf_set_nan',
    'ppf_set_prec_raw',
    'ppf_set_q',
    'ppf_set_si',
    'ppf_set_si_2exp',
    'ppf_set_sj',
    'ppf_set_sj_2exp',
    'ppf_set_ui',
    'ppf_set_ui_2exp',
    'ppf_set_uj',
    'ppf_set_uj_2exp',
    'ppf_set_z',
    'ppf_set_z_2exp',
    'ppf_set_zero',
    'ppf_setsign',
    'ppf_subnormalize',
    'ppf_swap',
    'ppf_trunc',
}
INITIALIZATION_LIFECYCLE_FUNCTIONS = {
    'ppf_init_set',
    'ppf_init_set_d',
    'ppf_init_set_f',
    'ppf_init_set_flt',
    'ppf_init_set_ld',
    'ppf_init_set_q',
    'ppf_init_set_si',
    'ppf_init_set_sj',
    'ppf_init_set_str',
    'ppf_init_set_ui',
    'ppf_init_set_uj',
    'ppf_init_set_z',
}
PREFIX_CLEANUP_FUNCTIONS = {"ppf_inits", "ppf_inits2"}

def atomicity_contract(function: dict[str, Any], test_refs: list[str]) -> dict[str, Any]:
    name = function["name"]
    args = classify_arguments(function)
    if ALLOCATION_COMPLETION_TEST in test_refs:
        if name in REVIEWED_ALLOCATION_FREE_FUNCTIONS:
            return {"classification": "reviewed_allocation_free",
                    "note": "reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim"}
        if name in INITIALIZATION_LIFECYCLE_FUNCTIONS:
            return {"classification": "verified_initialization_lifecycle",
                    "note": "every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome"}
        if name in PREFIX_CLEANUP_FUNCTIONS:
            return {"classification": "verified_prefix_cleanup",
                    "note": "on failure the initialized prefix is cleared and unvisited variadic destinations are untouched; every observed project allocation is tested; libc prefix-tracking realloc remains outside the recoverable project-allocator domain"}
    if name in PREFIX_CLEANUP_FUNCTIONS:
        return {"classification": "documented_partial",
                "note": "prefix cleanup and unvisited destinations require direct evidence"}
    if name in {"ppf_init", "ppf_init2", "ppf_init2_ctx"}:
        cls = "initialization_safe_zero"
    elif (any(name in ALLOCATION_ATOMICITY_TEST_FUNCTIONS.get(path, set())
              for path in test_refs) or
          (name in TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_FUNCTIONS and
           TRANSCENDENTAL_SPECIAL_ALLOCATION_ATOMICITY_TEST in test_refs) or
          (name in ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_FUNCTIONS and
           ARITHMETIC_ROOT_ALLOCATION_ATOMICITY_TEST in test_refs)):
        cls = "verified_transactional"
    elif not args["destinations"] and not args["inout_numbers"]:
        cls = "not_applicable"
    elif function["section"] in {
        "Arithmetic and product planning", "Roots and hypotenuse",
        "Reusable division, fused operations, powers of two, sums, and dot products",
        "Initialization and precision", "Text I/O, PPFIF interchange, formatting, and fit tests"
    }:
        cls = "documented_partial"
    else:
        cls = "not_yet_verified"
    return {"classification": cls}


def practical_limit(section: str, name: str) -> dict[str, Any]:
    # These control/query/cleanup operations do not execute the bounded
    # numerical backend of the family in which their header places them.
    if name in {
        "ppf_set_trig_threads", "ppf_get_trig_threads",
        "ppf_clear_constant_cache", "ppf_free_constant_caches",
        "ppf_constant_cache_precision", "ppf_set_constant_threads",
        "ppf_get_constant_threads", "ppf_get_last_constant_stats",
    }:
        return {"classification": "none_declared",
                "summary": "metadata or cleanup operation; no numerical certification loop is executed",
                "source": None}
    if name == "ppf_fpif_import":
        return {"classification": "implementation_bound",
                "summary": "default 64 MiB cap independently bounds imported payload and rounded significand storage (536870912 ordinary precision bits on GMP64); PPF_FPIF_MAX_IMPORT_BYTES may configure another positive cap; initial staging is at most 4096 bytes, growing only after reserved bytes arrive; range rejection returns -1, raises ERANGE and preserves the destination",
                "source": "src/io.c; docs/FPIF_RESOURCE_CONTRACT.md"}
    if name in {"ppf_set_str", "ppf_strtofr", "ppf_init_set_str"}:
        return {"classification": "implementation_bound", **PRACTICAL_LIMITS["assignment_text"]}
    if section == "Logarithms, exponentials, and powers":
        return {"classification": "implementation_bound", **PRACTICAL_LIMITS["transcendental"]}
    if section == "Trigonometric and hyperbolic functions":
        return {"classification": "implementation_bound", **PRACTICAL_LIMITS["trigonometric"]}
    if section == "Special functions":
        result = {"classification": "implementation_bound", **PRACTICAL_LIMITS["special"]}
        if name == "ppf_ai":
            result["summary"] += "; the current Airy Ai path additionally rejects finite |x| > 500"
        return result
    if section == "Certified constants and constant caches":
        return {"classification": "implementation_bound", **PRACTICAL_LIMITS["constants"]}
    if name in {"ppf_urandom", "ppf_nrandom", "ppf_grandom", "ppf_erandom"}:
        return {"classification": "implementation_bound", **PRACTICAL_LIMITS["random"]}
    return {"classification": "none_declared",
            "summary": "no additional function-specific practical limit is declared beyond type, precision, memory, and configured exponent-range limits",
            "source": None}


def contract_status(entry: dict[str, Any]) -> tuple[str, list[str]]:
    gaps: list[str] = []
    if entry["aliasing"]["classification"] == "expected_in_place_unverified":
        gaps.append("aliasing_not_individually_verified")
    if entry["failure_atomicity"]["classification"] in {"documented_partial", "not_yet_verified"}:
        gaps.append("allocation_failure_atomicity_incomplete")
    if entry["practical_limits"]["classification"] == "implementation_bound":
        gaps.append("bounded_implementation_path")
    if entry["availability"]["classification"] == "conditional":
        gaps.append("conditional_build_surface")
    if not entry["evidence"]["tests"]:
        gaps.append("no_direct_test_reference")
    return ("indexed_without_recorded_gap" if not gaps else "indexed_with_explicit_gaps", gaps)


def canonical_payload(data: dict[str, Any]) -> bytes:
    copy = json.loads(json.dumps(data))
    copy.pop("contract_sha256", None)
    copy.pop("release_version", None)
    copy.pop("summary", None)
    return json.dumps(copy, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def contract_hash(data: dict[str, Any]) -> str:
    return hashlib.sha256(canonical_payload(data)).hexdigest()


def parse_version(header: str) -> str:
    values = []
    for macro in ("PPF_VERSION_MAJOR", "PPF_VERSION_MINOR", "PPF_VERSION_PATCHLEVEL"):
        m = re.search(rf"#define\s+{macro}\s+(\d+)", header)
        values.append(m.group(1) if m else "0")
    return ".".join(values)


def generate(root: Path) -> dict[str, Any]:
    header_path = root / "include/polyphase_float.h"
    header = header_path.read_text(encoding="utf-8")
    functions = parse_header(header_path)
    purposes = load_purposes(root / "semantics/function-purposes.csv")
    names = [f["name"] for f in functions]
    if set(names) != set(purposes):
        missing = sorted(set(names) - set(purposes))
        stale = sorted(set(purposes) - set(names))
        raise RuntimeError(f"purpose inventory mismatch: missing={missing} stale={stale}")
    tests = direct_references(root, names, "tests")
    docs = direct_references(root, names, "docs")
    entries: list[dict[str, Any]] = []
    for function in functions:
        name = function["name"]
        purpose = purposes[name]
        function["section"] = purpose["section"]
        args = classify_arguments(function)
        context = context_contract(function)
        entry = {
            "name": name,
            "declaration": function["declaration"],
            "section": purpose["section"],
            "purpose": purpose["purpose"],
            "availability": {
                "classification": "conditional" if function["condition"] else "always",
                "condition": function["condition"] or None,
            },
            "signature": {
                "return_type": function["return_type"],
                "parameters": function["parameters"],
            },
            "arguments": args,
            "return_contract": return_contract(function),
            "rounding": rounding_contract(function),
            "context": context,
            "flags": flag_contract(function, context),
            "aliasing": aliasing_contract(function, tests[name]),
            "thread_safety": thread_contract(function, tests[name]),
            "ownership": ownership_contract(name),
            "failure_atomicity": atomicity_contract(function, tests[name]),
            "domain": {
                "summary": DOMAIN_OVERRIDES.get(name, purpose["purpose"]),
                "classification": "function_specific" if name in DOMAIN_OVERRIDES else "purpose_level",
            },
            "practical_limits": practical_limit(function["section"], name),
            "evidence": {
                "tests": tests[name],
                "documentation": docs[name],
            },
        }
        status, gaps = contract_status(entry)
        entry["contract_status"] = status
        entry["explicit_gaps"] = gaps
        entries.append(entry)
    summary = {
        "function_count": len(entries),
        "functions_with_direct_test_reference": sum(bool(x["evidence"]["tests"]) for x in entries),
        "functions_with_documentation_reference": sum(bool(x["evidence"]["documentation"]) for x in entries),
        "conditional_functions": sum(x["availability"]["classification"] == "conditional" for x in entries),
        "entries_without_recorded_gap": sum(not x["explicit_gaps"] for x in entries),
        "entries_with_explicit_gaps": sum(bool(x["explicit_gaps"]) for x in entries),
        "section_counts": dict(sorted(Counter(x["section"] for x in entries).items())),
        "contract_status_counts": dict(sorted(Counter(x["contract_status"] for x in entries).items())),
        "return_kinds": dict(sorted(Counter(x["return_contract"]["kind"] for x in entries).items())),
        "rounding_kinds": dict(sorted(Counter(x["rounding"]["kind"] for x in entries).items())),
        "context_policies": dict(sorted(Counter(x["context"]["policy"] for x in entries).items())),
        "flag_classes": dict(sorted(Counter(x["flags"]["classification"] for x in entries).items())),
        "ownership_classes": dict(sorted(Counter(x["ownership"]["classification"] for x in entries).items())),
        "practical_limit_classes": dict(sorted(Counter(x["practical_limits"]["classification"] for x in entries).items())),
        "aliasing_classes": dict(sorted(Counter(x["aliasing"]["classification"] for x in entries).items())),
        "atomicity_classes": dict(sorted(Counter(x["failure_atomicity"]["classification"] for x in entries).items())),
        "thread_classes": dict(sorted(Counter(x["thread_safety"]["classification"] for x in entries).items())),
        "explicit_gap_counts": dict(sorted(Counter(g for x in entries for g in x["explicit_gaps"]).items())),
    }
    result = {
        "schema_version": 1,
        "contract_version": 1,
        "library": "Polyphase Float",
        "release_version": parse_version(header),
        "scope": "public core C API declared in include/polyphase_float.h",
        "field_provenance": {
            "declaration_signature_availability": "parsed from the public header",
            "purpose_section": "curated function-purpose inventory",
            "test_evidence": "direct token references in tests",
            "documentation_evidence": "direct token references in docs",
            "semantic_classifications": "family rules plus explicit function overrides",
            "gap_policy": "unknown or unverified behavior is recorded as an explicit gap rather than inferred as a guarantee",
        },
        "summary": summary,
        "functions": entries,
    }
    result["contract_sha256"] = contract_hash(result)
    return result


def validate(data: dict[str, Any], require_all_tested: bool = True) -> list[str]:
    failures: list[str] = []
    functions = data.get("functions", [])
    names = [x.get("name") for x in functions]
    if len(names) != len(set(names)):
        failures.append("duplicate function names")
    if data.get("summary", {}).get("function_count") != len(functions):
        failures.append("summary function count mismatch")
    for entry in functions:
        name = entry.get("name", "<unnamed>")
        if not entry.get("purpose"):
            failures.append(f"{name}: missing purpose")
        if entry.get("return_contract", {}).get("kind") not in RETURN_KINDS:
            failures.append(f"{name}: invalid return kind")
        if entry.get("rounding", {}).get("kind") not in ROUNDING_KINDS:
            failures.append(f"{name}: invalid rounding kind")
        if entry.get("context", {}).get("policy") not in CONTEXT_POLICIES:
            failures.append(f"{name}: invalid context policy")
        if entry.get("aliasing", {}).get("classification") not in ALIASING_CLASSES:
            failures.append(f"{name}: invalid aliasing class")
        if entry.get("thread_safety", {}).get("classification") not in THREAD_CLASSES:
            failures.append(f"{name}: invalid thread class")
        if entry.get("ownership", {}).get("classification") not in OWNERSHIP_CLASSES:
            failures.append(f"{name}: invalid ownership class")
        if entry.get("failure_atomicity", {}).get("classification") not in ATOMICITY_CLASSES:
            failures.append(f"{name}: invalid atomicity class")
        if require_all_tested and not entry.get("evidence", {}).get("tests"):
            failures.append(f"{name}: no direct test reference")
    expected_hash = contract_hash(data)
    if data.get("contract_sha256") != expected_hash:
        failures.append("contract SHA-256 mismatch")
    return failures


def render_csv(data: dict[str, Any], output: Path) -> None:
    fields = [
        "section", "function", "purpose", "prototype", "availability",
        "return_kind", "rounding", "context", "destinations", "sources",
        "flags", "aliasing", "thread_safety", "ownership",
        "failure_atomicity", "practical_limit", "direct_tests",
        "contract_status", "explicit_gaps"
    ]
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for entry in data["functions"]:
            writer.writerow({
                "section": entry["section"],
                "function": entry["name"],
                "purpose": entry["purpose"],
                "prototype": entry["declaration"],
                "availability": entry["availability"]["classification"],
                "return_kind": entry["return_contract"]["kind"],
                "rounding": entry["rounding"]["kind"],
                "context": entry["context"]["policy"],
                "destinations": ",".join(entry["arguments"]["destinations"] + entry["arguments"]["inout_numbers"]),
                "sources": ",".join(entry["arguments"]["source_numbers"]),
                "flags": ",".join(entry["flags"]["may_set"]),
                "aliasing": entry["aliasing"]["classification"],
                "thread_safety": entry["thread_safety"]["classification"],
                "ownership": entry["ownership"]["classification"],
                "failure_atomicity": entry["failure_atomicity"]["classification"],
                "practical_limit": entry["practical_limits"]["classification"],
                "direct_tests": ",".join(entry["evidence"]["tests"]),
                "contract_status": entry["contract_status"],
                "explicit_gaps": ",".join(entry["explicit_gaps"]),
            })


def render_markdown(data: dict[str, Any], output: Path) -> None:
    summary = data["summary"]
    lines = [
        "# Polyphase Float public semantic contract",
        "",
        f"Release: `{data['release_version']}`  ",
        f"Contract version: `{data['contract_version']}`  ",
        f"Contract SHA-256: `{data['contract_sha256']}`",
        "",
        "## Purpose and boundary",
        "",
        "This document is the human-readable summary of the generated public semantic ledger. Every core function declared in `include/polyphase_float.h` has exactly one machine-readable record in `semantics/contract-v1.json` and one review row in `semantics/function-matrix-v1.csv`.",
        "",
        "The ledger distinguishes mechanically parsed facts, curated descriptions, conservative family-level classifications, direct test references, and explicit unresolved gaps. A direct test reference means that the function is named in a test source. It does not by itself prove every branch, input class, rounding mode, alias pattern, or allocation-failure point.",
        "",
        "An entry without a currently recorded gap is not a universal correctness proof. It means that none of the gap categories modeled by contract version 1 applies to that entry.",
        "",
        "## Coverage summary",
        "",
        "| Item | Count |",
        "|---|---:|",
        f"| Public core functions | **{summary['function_count']}** |",
        f"| Functions with a direct test reference | **{summary['functions_with_direct_test_reference']}** |",
        f"| Functions with a pre-existing documentation reference | **{summary['functions_with_documentation_reference']}** |",
        f"| Conditional functions | **{summary['conditional_functions']}** |",
        f"| Entries without a currently recorded gap | **{summary['entries_without_recorded_gap']}** |",
        f"| Entries with one or more explicit gaps | **{summary['entries_with_explicit_gaps']}** |",
        "",
        "## Explicit gap counts",
        "",
        "A function may appear in more than one row below.",
        "",
        "| Gap | Functions | Meaning |",
        "|---|---:|---|",
    ]
    meanings = {
        "aliasing_not_individually_verified": "The exact legal and illegal overlap combinations have not been promoted to an individual function guarantee.",
        "allocation_failure_atomicity_incomplete": "Fail-at-every-allocation transactional behavior has not been demonstrated for this exact entry.",
        "bounded_implementation_path": "The current source contains a real domain, attempt, iteration, or reduction bound.",
        "conditional_build_surface": "The declaration depends on a compiler or build capability and must be validated in both enabled and disabled configurations.",
        "no_direct_test_reference": "No test source names the public function.",
    }
    for key, value in summary["explicit_gap_counts"].items():
        lines.append(f"| `{key}` | {value} | {meanings.get(key, '')} |")
    lines += [
        "",
        "## Classification rules",
        "",
        "### Return values",
        "",
        "`ternary` records the floating-point relation to the exact result and uses `PPF_TERNARY_ERROR` for an unrecovered failure. `status` records an ordinary success or failure code. Predicates, comparisons, counts, borrowed pointers, and caller-owned pointers are classified separately.",
        "",
        "### Rounding",
        "",
        "`explicit` means a public rounding parameter is present. `fixed_by_operation` means the operation name selects the rule. `not_applicable` means the function does not perform a floating-point settlement step governed by a public rounding argument.",
        "",
        "### Contexts and flags",
        "",
        "`explicit_required` means a null context is invalid for that lifecycle call. `explicit_or_thread_default` means a null context resolves to the calling thread's default context. Flag lists marked `family_upper_bound` are conservative: they identify flags reachable somewhere in the family, not a claim that every flag is reachable for every input to every function.",
        "",
        "### Aliasing",
        "",
        "`verified_in_place` means the destination/source overlap positions named by the systematic matrix have direct evidence. `verified_distinct_outputs` governs paired outputs that must remain distinct while permitting documented input overlap. `verified_same_index_overlap` governs batch interfaces whose output may replace only its corresponding input. `source_must_be_distinct` and `lifecycle_destination` record real preconditions rather than implying unsupported in-place use. Direct-storage functions remain application-managed advanced interfaces.",
        "",
        "### Failure atomicity",
        "",
        "`verified_transactional` means deterministic allocation-failure injection directly exercises the entry and verifies destination preservation on unrecovered failure or an exact successful fallback. `reviewed_allocation_free` means reviewed valid initialized-storage paths contain no project-allocation site, with direct fixed-storage tests. `verified_initialization_lifecycle` and `verified_prefix_cleanup` state tested lifecycle outcomes, not preservation of an uninitialized old value. GMP, libc and kernel allocation domains remain separate. `documented_partial` and `not_yet_verified` are not promoted to stronger guarantees.",
        "",
        "### Practical limits",
        "",
        "`implementation_bound` records a bound found in the current source, such as a maximum escalation count, iteration count, parser exponent policy, or a finite domain restriction. These are implementation properties, not mathematical limitations of the represented function.",
        "",
        "## Function families",
        "",
        "| Family | Functions |",
        "|---|---:|",
    ]
    for key, value in summary["section_counts"].items():
        lines.append(f"| {key} | {value} |")
    lines += [
        "",
        "## Reproduction",
        "",
        "```sh",
        "python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json",
        "python3 tests/test_semantic_contract.py --root .",
        "cmake --build build/validation --target semantic-contract-audit",
        "make semantic-check",
        "```",
        "",
        "The JSON contract is canonical. The CSV is a flattened review surface generated from the same data. This Markdown summary is generated from the same contract and is not a substitute for the per-function records.",
        "",
        "## Installed artifact layout",
        "",
        "Installed packages preserve the semantic and ABI contracts at distinct paths:",
        "",
        "```text",
        "share/doc/PolyphaseFloat/semantics/contract-v1.json",
        "share/doc/PolyphaseFloat/semantics/function-matrix-v1.csv",
        "share/doc/PolyphaseFloat/abi/contract-v1.json",
        "```",
        "",
        "This separation prevents the two independently governed `contract-v1.json` files from overwriting one another.",
        "",
    ]
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    generate_parser = sub.add_parser("generate")
    generate_parser.add_argument("--root", type=Path, default=Path("."))
    generate_parser.add_argument("--json-output", type=Path, required=True)
    generate_parser.add_argument("--csv-output", type=Path)
    generate_parser.add_argument("--markdown-output", type=Path)
    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("--root", type=Path, default=Path("."))
    verify_parser.add_argument("--contract", type=Path, required=True)
    validate_parser = sub.add_parser("validate")
    validate_parser.add_argument("--contract", type=Path, required=True)
    validate_parser.add_argument("--allow-untested", action="store_true")
    hash_parser = sub.add_parser("hash")
    hash_parser.add_argument("--contract", type=Path, required=True)
    args = parser.parse_args()

    if args.command == "generate":
        data = generate(args.root.resolve())
        failures = validate(data)
        if failures:
            raise SystemExit("\n".join(failures))
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if args.csv_output:
            args.csv_output.parent.mkdir(parents=True, exist_ok=True)
            render_csv(data, args.csv_output)
        if args.markdown_output:
            args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
            render_markdown(data, args.markdown_output)
        print(data["contract_sha256"])
    elif args.command == "verify":
        expected = json.loads(args.contract.read_text(encoding="utf-8"))
        actual = generate(args.root.resolve())
        failures = validate(actual)
        if failures:
            raise SystemExit("\n".join(failures))
        if expected != actual:
            expected_names = {x["name"] for x in expected.get("functions", [])}
            actual_names = {x["name"] for x in actual.get("functions", [])}
            detail = {
                "expected_sha256": expected.get("contract_sha256"),
                "actual_sha256": actual.get("contract_sha256"),
                "missing_functions": sorted(actual_names - expected_names),
                "stale_functions": sorted(expected_names - actual_names),
            }
            print(json.dumps(detail, indent=2, sort_keys=True))
            raise SystemExit(1)
        print(f"semantic contract verified: {actual['contract_sha256']}")
    elif args.command == "validate":
        data = json.loads(args.contract.read_text(encoding="utf-8"))
        failures = validate(data, require_all_tested=not args.allow_untested)
        if failures:
            print("\n".join(failures))
            raise SystemExit(1)
        print(json.dumps(data["summary"], indent=2, sort_keys=True))
    else:
        data = json.loads(args.contract.read_text(encoding="utf-8"))
        print(contract_hash(data))


if __name__ == "__main__":
    main()
