#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Validate every generated planner rule against a tuned library build."""

from __future__ import annotations

import argparse
import csv
import json
import os
import statistics
import subprocess
from pathlib import Path


def run(command: list[str], env: dict[str, str] | None = None,
        timeout: int = 600) -> str:
    completed = subprocess.run(
        command, env=env, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, timeout=timeout, check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout.strip()


def parse_benchmark(text: str) -> list[dict[str, str]]:
    lines = [line for line in text.splitlines() if line.strip()]
    index = next((i for i, line in enumerate(lines)
                  if line.startswith("bits,operation,method,")), None)
    if index is None:
        raise RuntimeError("benchmark output contained no CSV header")
    return list(csv.DictReader(lines[index:]))


def paired_ratio(rows: list[dict[str, str]], operation: str,
                 first: str, second: str) -> list[float]:
    first_rows = {
        int(row["repeat"]): float(row["seconds"])
        for row in rows if row["operation"] == operation and row["method"] == first
    }
    second_rows = {
        int(row["repeat"]): float(row["seconds"])
        for row in rows if row["operation"] == operation and row["method"] == second
    }
    repeats = sorted(set(first_rows) & set(second_rows))
    if not repeats:
        raise RuntimeError(
            f"no paired rows for operation={operation} methods={first},{second}"
        )
    return [first_rows[repeat] / second_rows[repeat] for repeat in repeats]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile-report", required=True)
    parser.add_argument("--inspector", required=True)
    parser.add_argument("--benchmark", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bindings", default="close,spread")
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--inner-repetitions", type=int, default=1)
    parser.add_argument("--maximum-auto-slowdown", type=float, default=1.25)
    args = parser.parse_args()

    report_path = Path(args.profile_report).resolve()
    inspector = Path(args.inspector).resolve()
    benchmark = Path(args.benchmark).resolve()
    output = Path(args.output).resolve()
    for path in (report_path, inspector, benchmark):
        if not path.exists():
            parser.error(f"required path is missing: {path}")
    if args.repetitions < 3 or args.inner_repetitions < 1:
        parser.error("invalid repetition count")
    if args.maximum_auto_slowdown <= 1.0:
        parser.error("--maximum-auto-slowdown must exceed 1")

    profile = json.loads(report_path.read_text(encoding="utf-8"))
    bindings = [item.strip() for item in args.bindings.split(",") if item.strip()]
    results: list[dict] = []
    failures: list[str] = []
    method_for = {"rank3": "ppf_rank3", "rank5": "ppf_rank5"}
    expected_effective = {"rank3": 3, "rank5": 5}

    for rule in profile["rules"]:
        bits = int(rule["min_precision"])
        workers = int(rule["min_workers"])
        operation = rule["operation"]
        algorithm = rule["algorithm"]
        inspection = json.loads(run([
            str(inspector), str(bits), operation, str(workers)
        ]))
        expected_workers = min(workers, expected_effective[algorithm])
        inspection_ok = (
            inspection.get("profile_name") == profile["profile_name"]
            and inspection.get("profile_sha256") == profile["profile_sha256"]
            and inspection.get("algorithm") == algorithm
            and inspection.get("effective_workers") == expected_workers
            and inspection.get("profile_rule_matched") is True
        )
        if not inspection_ok:
            failures.append(
                f"inspection mismatch for {operation} bits={bits} workers={workers}: "
                f"{inspection}"
            )
        for binding in bindings:
            env = os.environ.copy()
            env.update({
                "LC_ALL": "C", "LANG": "C", "OMP_DYNAMIC": "FALSE",
                "OMP_PROC_BIND": binding, "OMP_PLACES": "cores",
            })
            rows = parse_benchmark(run([
                str(benchmark), str(bits), str(args.repetitions),
                str(workers), str(args.inner_repetitions)
            ], env=env))
            ratios = paired_ratio(
                rows, operation, "ppf_auto", method_for[algorithm]
            )
            median_slowdown = float(statistics.median(ratios))
            passed = median_slowdown <= args.maximum_auto_slowdown
            if not passed:
                failures.append(
                    f"AUTO slowdown {median_slowdown:.3f} exceeds "
                    f"{args.maximum_auto_slowdown:.3f} for {operation} "
                    f"bits={bits} workers={workers} binding={binding}"
                )
            results.append({
                "bits": bits,
                "operation": operation,
                "workers": workers,
                "binding": binding,
                "algorithm": algorithm,
                "inspection_ok": inspection_ok,
                "paired_auto_over_forced": ratios,
                "median_auto_over_forced": median_slowdown,
                "maximum_allowed": args.maximum_auto_slowdown,
                "passed": inspection_ok and passed,
            })

    output.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": 1,
        "profile_name": profile["profile_name"],
        "profile_sha256": profile["profile_sha256"],
        "rule_count": len(profile["rules"]),
        "results": results,
        "failures": failures,
        "passed": not failures,
    }
    (output / "profile_validation.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    lines = [
        "# Generated product-profile validation",
        "",
        f"Profile: `{profile['profile_name']}`",
        "",
        f"Profile SHA-256: `{profile['profile_sha256']}`",
        "",
        f"Rules: **{len(profile['rules'])}**",
        "",
        f"Status: **{'PASS' if not failures else 'FAIL'}**",
        "",
        "| Bits | Operation | Workers | Binding | Algorithm | "
        "Median AUTO/forced | Status |",
        "|---:|---|---:|---|---|---:|---|",
    ]
    for result in results:
        lines.append(
            f"| {result['bits']:,} | {result['operation']} | "
            f"{result['workers']} | {result['binding']} | "
            f"{result['algorithm']} | "
            f"{result['median_auto_over_forced']:.3f} | "
            f"{'PASS' if result['passed'] else 'FAIL'} |"
        )
    if failures:
        lines.extend(["", "## Failures", ""])
        lines.extend(f"- {failure}" for failure in failures)
    (output / "PROFILE_VALIDATION.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )
    print(json.dumps({
        "passed": not failures,
        "rules": len(profile["rules"]),
        "output": str(output),
    }, sort_keys=True))
    return 0 if not failures else 2


if __name__ == "__main__":
    raise SystemExit(main())
