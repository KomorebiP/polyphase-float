#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Policy checks for the generated Polyphase Float aliasing contract."""
from __future__ import annotations

import argparse
import importlib.util
import json
import re
from pathlib import Path


def load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def macro_string(header: str, name: str) -> str:
    match = re.search(rf'#define\s+{re.escape(name)}\s+(?:\\\s*)?"([0-9a-f]+)"', header)
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
    generator = load(root / "tools" / "aliasing_contract.py", "ppf_aliasing_contract")
    data = generator.generate(root)
    failures = generator.validate(data)
    assert not failures, "\n".join(failures)
    assert data["release_version"] == "0.18.0"
    assert data["contract_version"] == 1
    assert data["summary"]["function_count"] == 437
    assert data["summary"]["unverified_aliasing_functions"] == 0

    functions = {entry["name"]: entry for entry in data["functions"]}
    assert functions["ppf_add"]["classification"] == "verified_in_place"
    assert functions["ppf_fmma"]["classification"] == "verified_in_place"
    assert functions["ppf_sin_cos"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_sinh_cosh"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_modf"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_grandom"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_get_constant_interval"]["classification"] == "verified_distinct_outputs"
    assert functions["ppf_div_with_plan_many"]["classification"] == "verified_same_index_overlap"
    assert functions["ppf_init_set"]["classification"] == "source_must_be_distinct"
    assert functions["ppf_round_nearest_away_finish"]["classification"] == "source_must_be_distinct"
    assert functions["ppf_set_ui"]["classification"] == "not_applicable"
    assert functions["ppf_get_d"]["classification"] == "not_applicable"
    assert functions["ppf_init2"]["classification"] == "lifecycle_destination"
    assert functions["ppf_check_range"]["classification"] == "mutable_object"

    matrix = (root / "tests" / "test_aliasing_matrix.c").read_text(encoding="utf-8")
    for name in {
        "ppf_setsign", "ppf_copysign", "ppf_fma_threads", "ppf_fmma",
        "ppf_rec_sqrt", "ppf_rootn_ui", "ppf_log", "ppf_pow",
        "ppf_sin_cos", "ppf_sinh_cosh", "ppf_modf", "ppf_grandom",
        "ppf_get_constant_interval", "ppf_div_with_plan_many"
    }:
        assert re.search(rf"\b{re.escape(name)}\b", matrix), name

    semantic = json.loads((root / "semantics" / "contract-v1.json").read_text(encoding="utf-8"))
    assert "aliasing_not_individually_verified" not in semantic["summary"]["explicit_gap_counts"]
    assert semantic["summary"]["aliasing_classes"] == data["summary"]["classification_counts"]

    header = (root / "include" / "polyphase_float.h").read_text(encoding="utf-8")
    assert macro_integer(header, "PPF_ALIASING_CONTRACT_VERSION") == data["contract_version"]
    assert macro_string(header, "PPF_ALIASING_CONTRACT_SHA256") == data["contract_sha256"]

    print(
        "aliasing contract policy passed: "
        f"functions={data['summary']['function_count']} "
        f"unverified={data['summary']['unverified_aliasing_functions']}"
    )


if __name__ == "__main__":
    main()
