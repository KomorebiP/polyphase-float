#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Generate and verify the public Polyphase Float aliasing contract."""
from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
from collections import Counter
from pathlib import Path
from typing import Any


def load_semantic_generator(root: Path):
    path = root / "tools" / "semantic_contract.py"
    spec = importlib.util.spec_from_file_location("ppf_semantic_contract", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def canonical_payload(data: dict[str, Any]) -> bytes:
    value = json.loads(json.dumps(data))
    value.pop("contract_sha256", None)
    value.pop("release_version", None)
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def contract_hash(data: dict[str, Any]) -> str:
    return hashlib.sha256(canonical_payload(data)).hexdigest()


def generate(root: Path) -> dict[str, Any]:
    semantic = load_semantic_generator(root).generate(root)
    functions = []
    for entry in semantic["functions"]:
        alias = entry["aliasing"]
        functions.append({
            "name": entry["name"],
            "declaration": entry["declaration"],
            "availability": entry["availability"],
            "destinations": entry["arguments"]["destinations"],
            "inout_numbers": entry["arguments"]["inout_numbers"],
            "source_numbers": entry["arguments"]["source_numbers"],
            "classification": alias["classification"],
            "allowed_patterns": alias.get("allowed_patterns", []),
            "forbidden_patterns": alias.get("forbidden_patterns", []),
            "invalid_overlap_behavior": alias.get("invalid_overlap_behavior"),
            "evidence": alias.get("evidence", []),
        })
    result = {
        "schema_version": 1,
        "contract_version": 1,
        "library": "Polyphase Float",
        "release_version": semantic["release_version"],
        "scope": "overlap and lifecycle rules for the public core C API",
        "semantic_contract_sha256": semantic["contract_sha256"],
        "summary": {
            "function_count": len(functions),
            "classification_counts": dict(sorted(Counter(x["classification"] for x in functions).items())),
            "functions_with_aliasing_evidence": sum(bool(x["evidence"]) for x in functions),
            "unverified_aliasing_functions": sum(x["classification"] == "expected_in_place_unverified" for x in functions),
            "governed_distinct_output_functions": sum(x["classification"] == "verified_distinct_outputs" for x in functions),
            "governed_same_index_batch_functions": sum(x["classification"] == "verified_same_index_overlap" for x in functions),
            "source_distinct_preconditions": sum(x["classification"] == "source_must_be_distinct" for x in functions),
        },
        "functions": functions,
    }
    result["contract_sha256"] = contract_hash(result)
    return result


def validate(data: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    functions = data.get("functions", [])
    names = [x.get("name") for x in functions]
    if len(names) != len(set(names)):
        failures.append("duplicate function names")
    if data.get("summary", {}).get("function_count") != len(functions):
        failures.append("summary function count mismatch")
    for entry in functions:
        name = entry.get("name", "<unnamed>")
        if not entry.get("classification"):
            failures.append(f"{name}: missing classification")
        if entry.get("classification") == "expected_in_place_unverified":
            failures.append(f"{name}: unresolved aliasing classification")
        if entry.get("classification") in {
            "verified_in_place", "verified_distinct_outputs",
            "verified_same_index_overlap", "source_must_be_distinct"
        } and not entry.get("evidence"):
            failures.append(f"{name}: governed aliasing class lacks direct evidence")
        if entry.get("classification") == "verified_distinct_outputs" and not entry.get("forbidden_patterns"):
            failures.append(f"{name}: missing distinct-output prohibition")
    if data.get("contract_sha256") != contract_hash(data):
        failures.append("contract SHA-256 mismatch")
    return failures


def render_csv(data: dict[str, Any], output: Path) -> None:
    fields = [
        "function", "prototype", "availability", "destinations", "inout_numbers",
        "sources", "classification", "allowed_patterns", "forbidden_patterns",
        "invalid_overlap_behavior", "evidence"
    ]
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for entry in data["functions"]:
            writer.writerow({
                "function": entry["name"],
                "prototype": entry["declaration"],
                "availability": entry["availability"]["classification"],
                "destinations": ",".join(entry["destinations"]),
                "inout_numbers": ",".join(entry["inout_numbers"]),
                "sources": ",".join(entry["source_numbers"]),
                "classification": entry["classification"],
                "allowed_patterns": " | ".join(entry["allowed_patterns"]),
                "forbidden_patterns": " | ".join(entry["forbidden_patterns"]),
                "invalid_overlap_behavior": entry["invalid_overlap_behavior"],
                "evidence": ",".join(entry["evidence"]),
            })


def render_markdown(data: dict[str, Any], output: Path) -> None:
    s = data["summary"]
    lines = [
        "# Polyphase Float aliasing contract",
        "",
        f"Release: `{data['release_version']}`  ",
        f"Contract version: `{data['contract_version']}`  ",
        f"Contract SHA-256: `{data['contract_sha256']}`",
        "",
        "## Contract boundary",
        "",
        "This contract governs overlap among public Polyphase floating objects and the lifecycle preconditions that can make an apparent in-place call invalid. It does not authorize overlapping raw buffers in advanced direct-storage interfaces unless those interfaces state their own storage rule.",
        "",
        "A verified in-place classification means the destination/source positions named by the contract were compared with out-of-place execution, including numerical value, ternary relation, and context flags. The systematic matrix exercises all six public rounding modes. Multi-source functions additionally exercise each destination/source position and the all-sources-identical case.",
        "",
        "## Summary",
        "",
        "| Item | Count |",
        "|---|---:|",
        f"| Public core functions | **{s['function_count']}** |",
        f"| Unverified aliasing entries | **{s['unverified_aliasing_functions']}** |",
        f"| Governed distinct-output functions | **{s['governed_distinct_output_functions']}** |",
        f"| Governed same-index batch functions | **{s['governed_same_index_batch_functions']}** |",
        f"| Source-distinct preconditions | **{s['source_distinct_preconditions']}** |",
        "",
        "## Classification counts",
        "",
        "| Classification | Functions |",
        "|---|---:|",
    ]
    for key, value in s["classification_counts"].items():
        lines.append(f"| `{key}` | {value} |")
    lines += [
        "",
        "## Governed multi-output rules",
        "",
        "| Function | Allowed overlap | Forbidden overlap | Invalid-overlap behavior |",
        "|---|---|---|---|",
    ]
    for entry in data["functions"]:
        if entry["classification"] in {"verified_distinct_outputs", "verified_same_index_overlap"}:
            lines.append(
                f"| `{entry['name']}` | {'; '.join(entry['allowed_patterns'])} | "
                f"{'; '.join(entry['forbidden_patterns'])} | {entry['invalid_overlap_behavior']} |"
            )
    lines += [
        "",
        "## Reproduction",
        "",
        "```sh",
        "python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json",
        "python3 tests/test_aliasing_contract.py --root .",
        "ctest --test-dir build/validation -R 'aliasing_(matrix|contract)' --output-on-failure",
        "make aliasing-check",
        "```",
        "",
        "The JSON file is canonical. The CSV is a flattened review surface generated from the same data.",
        "",
    ]
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--root", type=Path, default=Path("."))
    g.add_argument("--json-output", type=Path, required=True)
    g.add_argument("--csv-output", type=Path)
    g.add_argument("--markdown-output", type=Path)
    v = sub.add_parser("verify")
    v.add_argument("--root", type=Path, default=Path("."))
    v.add_argument("--contract", type=Path, required=True)
    c = sub.add_parser("validate")
    c.add_argument("--contract", type=Path, required=True)
    h = sub.add_parser("hash")
    h.add_argument("--contract", type=Path, required=True)
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
            print(json.dumps({
                "expected_sha256": expected.get("contract_sha256"),
                "actual_sha256": actual.get("contract_sha256"),
            }, indent=2, sort_keys=True))
            raise SystemExit(1)
        print(f"aliasing contract verified: {actual['contract_sha256']}")
    elif args.command == "validate":
        data = json.loads(args.contract.read_text(encoding="utf-8"))
        failures = validate(data)
        if failures:
            raise SystemExit("\n".join(failures))
        print(json.dumps(data["summary"], indent=2, sort_keys=True))
    else:
        data = json.loads(args.contract.read_text(encoding="utf-8"))
        print(contract_hash(data))


if __name__ == "__main__":
    main()
