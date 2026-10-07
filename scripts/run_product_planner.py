#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import platform
import random
import statistics
import subprocess
import time
from pathlib import Path
from typing import Iterable, Sequence


def median(values: Sequence[float]) -> float:
    return float(statistics.median(values))


def mad(values: Sequence[float]) -> float:
    center = median(values)
    return median([abs(value - center) for value in values])


def percentile(values: Sequence[float], probability: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    return float(
        ordered[lower] * (upper - position)
        + ordered[upper] * (position - lower)
    )


def bootstrap_ci(values: Sequence[float], seed: int,
                 draws: int) -> tuple[float, float]:
    rng = random.Random(seed)
    count = len(values)
    samples = []
    for _ in range(draws):
        samples.append(median([values[rng.randrange(count)]
                               for _ in range(count)]))
    return percentile(samples, 0.025), percentile(samples, 0.975)


def sign_test_p(wins: int, losses: int) -> float:
    count = wins + losses
    if count == 0:
        return 1.0
    tail = min(wins, losses)
    probability = sum(math.comb(count, index)
                      for index in range(tail + 1)) / (2 ** count)
    return min(1.0, 2.0 * probability)


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields: list[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def parse_benchmark_output(text: str, binding: str) -> list[dict]:
    lines = [line for line in text.splitlines() if line.strip()]
    header_index = next((index for index, line in enumerate(lines)
                         if line.startswith("bits,operation,method,")), None)
    if header_index is None:
        raise RuntimeError(f"benchmark produced no CSV header:\n{text}")
    reader = csv.DictReader(lines[header_index:])
    rows: list[dict] = []
    for row in reader:
        rows.append({
            "bits": int(row["bits"]),
            "operation": row["operation"],
            "method": row["method"],
            "method_workers": int(row["method_workers"]),
            "repeat": int(row["repeat"]),
            "order": int(row["order"]),
            "binding": binding,
            "seconds": float(row["seconds"]),
        })
    return rows


def run_case(binary: Path, bits: int, repetitions: int,
             workers: int, inner: int, binding: str,
             environment: dict[str, str]) -> list[dict]:
    env = environment.copy()
    env.update({
        "LC_ALL": "C",
        "LANG": "C",
        "OMP_DYNAMIC": "FALSE",
        "OMP_PROC_BIND": binding,
        "OMP_PLACES": "cores",
    })
    command = [str(binary), str(bits), str(repetitions),
               str(workers), str(inner)]
    completed = subprocess.run(command, env=env, text=True,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, check=False)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}")
    return parse_benchmark_output(completed.stdout, binding)


def run_json_output(command: list[str], environment: dict[str, str],
                    timeout_seconds: int) -> dict:
    try:
        completed = subprocess.run(
            command, env=environment, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            check=False, timeout=timeout_seconds)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f"benchmark timed out after {timeout_seconds}s: "
            f"{' '.join(command)}") from error
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}")
    for line in reversed([line.strip() for line in completed.stdout.splitlines()
                          if line.strip()]):
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            pass
    raise RuntimeError(f"no JSON result from {' '.join(command)}")


def paired_ratios(rows: list[dict], first: str, second: str) -> list[float]:
    first_rows = {row["repeat"]: row["seconds"] for row in rows
                  if row["method"] == first}
    second_rows = {row["repeat"]: row["seconds"] for row in rows
                   if row["method"] == second}
    repeats = sorted(set(first_rows) & set(second_rows))
    return [second_rows[repeat] / first_rows[repeat] for repeat in repeats]


def comparison_row(rows: list[dict], first: str, second: str,
                   seed: int, draws: int) -> dict:
    ratios = paired_ratios(rows, first, second)
    wins = losses = ties = 0
    first_rows = {row["repeat"]: row["seconds"] for row in rows
                  if row["method"] == first}
    second_rows = {row["repeat"]: row["seconds"] for row in rows
                   if row["method"] == second}
    for repeat in sorted(set(first_rows) & set(second_rows)):
        a = first_rows[repeat]
        b = second_rows[repeat]
        if a < b:
            wins += 1
        elif a > b:
            losses += 1
        else:
            ties += 1
    low, high = bootstrap_ci(ratios, seed, draws)
    return {
        "first_method": first,
        "second_method": second,
        "pairs": len(ratios),
        "paired_median_speedup": median(ratios),
        "bootstrap_95_low": low,
        "bootstrap_95_high": high,
        "wins": wins,
        "losses": losses,
        "ties": ties,
        "sign_test_two_sided_p": sign_test_p(wins, losses),
    }


def stable_recommendation(group: list[dict], operation: str,
                          seed: int, draws: int) -> tuple[str, str]:
    available = {row["method"] for row in group}
    candidates = [method for method in
                  ["ppf_full_1t", "ppf_rank3", "ppf_rank5", "ppf_window"]
                  if method in available]
    medians = {
        method: median([row["seconds"] for row in group
                        if row["method"] == method])
        for method in candidates
    }
    baseline = "ppf_rank3"
    fastest = min(candidates, key=lambda method: medians[method])
    if fastest == baseline:
        return baseline, "rank 3 was fastest"
    ratios = paired_ratios(group, fastest, baseline)
    improvement = median(ratios)
    low, high = bootstrap_ci(ratios, seed, draws)
    required = 1.03 if fastest == "ppf_rank5" else 1.05
    if improvement >= required and low > 1.0:
        return fastest, (
            f"paired {improvement:.3f}x gain over rank 3; "
            f"bootstrap interval [{low:.3f}, {high:.3f}]"
        )
    if operation == "square" and fastest == "ppf_rank5" and improvement >= 1.05:
        return fastest, (
            f"paired {improvement:.3f}x gain over rank 3; "
            "square-specific rank-5 preference"
        )
    return baseline, (
        f"raw fastest was {fastest}, but the paired {improvement:.3f}x "
        f"gain with interval [{low:.3f}, {high:.3f}] was not stable "
        "enough for an automatic recommendation"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/product_planner")
    parser.add_argument("--bits", default="200000,1000000,5000000,10000000")
    parser.add_argument("--workers", default="3,5")
    parser.add_argument("--bindings", default="close")
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--inner-repetitions", type=int, default=1)
    parser.add_argument("--bootstrap-draws", type=int, default=20000)
    parser.add_argument("--case-timeout-seconds", type=int, default=300)
    parser.add_argument("--seed", type=int, default=20260811)
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if args.fresh:
        for path in out.iterdir():
            if path.is_file():
                path.unlink()

    bits_list = [int(value.strip()) for value in args.bits.split(",")
                 if value.strip()]
    workers_list = [int(value.strip()) for value in args.workers.split(",")
                    if value.strip()]
    bindings = [value.strip() for value in args.bindings.split(",")
                if value.strip()]
    repetitions = max(3, args.repetitions)
    inner = max(1, args.inner_repetitions)
    binary = root / "bin" / "benchmark_product_planner"
    recsqrt_binary = root / "bin" / "benchmark_recsqrt_planner"
    speed_binary = root / "bin" / "benchmark_speedup_method"
    recsqrt_matched_binary = root / "bin" / "benchmark_recsqrt_matched"
    required = [binary, recsqrt_binary, speed_binary, recsqrt_matched_binary]
    if any(not path.exists() for path in required):
        raise RuntimeError("planner benchmark executables are missing; rebuild first")

    raw_path = out / "raw.csv"
    rows: list[dict] = []
    if raw_path.exists() and not args.fresh:
        with raw_path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                rows.append({
                    "bits": int(row["bits"]),
                    "operation": row["operation"],
                    "method": row["method"],
                    "method_workers": int(row["method_workers"]),
                    "repeat": int(row["repeat"]),
                    "order": int(row["order"]),
                    "binding": row["binding"],
                    "seconds": float(row["seconds"]),
                })
    completed = {(row["bits"], row["method_workers"], row["binding"],
                  row["operation"])
                 for row in rows if row["method"] == "ppf_auto"}

    environment = os.environ.copy()
    for binding in bindings:
        for workers in workers_list:
            for bits in bits_list:
                product_keys = [
                    (bits, workers, binding, operation)
                    for operation in ("multiply", "square", "fma")
                ]
                if any(key not in completed for key in product_keys):
                    new_rows = run_case(binary, bits, repetitions, workers,
                                        inner, binding, environment)
                    rows.extend(new_rows)
                    write_csv(raw_path, rows)
                    for added in new_rows:
                        if added["method"] == "ppf_auto":
                            completed.add((added["bits"], workers, binding,
                                           added["operation"]))
                    print(f"completed products bits={bits:,} workers={workers} "
                          f"binding={binding}", flush=True)
                root_key = (bits, workers, binding, "rec_sqrt")
                if root_key not in completed:
                    new_rows = run_case(recsqrt_binary, bits, repetitions,
                                        workers, inner, binding, environment)
                    rows.extend(new_rows)
                    write_csv(raw_path, rows)
                    completed.add(root_key)
                    print(f"completed reciprocal root bits={bits:,} "
                          f"workers={workers} binding={binding}", flush=True)

    write_csv(raw_path, rows)

    matched_path = out / "matched_workers_raw.csv"
    matched_rows: list[dict] = []
    if matched_path.exists() and not args.fresh:
        with matched_path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                matched_rows.append({
                    "bits": int(row["bits"]),
                    "operation": row["operation"],
                    "method": row["method"],
                    "workers": int(row["workers"]),
                    "jobs": int(row["jobs"]),
                    "repeat": int(row["repeat"]),
                    "order": int(row["order"]),
                    "binding": row["binding"],
                    "seconds": float(row["seconds"]),
                })
    matched_done = {(row["bits"], row["operation"], row["workers"],
                     row["binding"], row["repeat"], row["method"])
                    for row in matched_rows}
    matched_rng = random.Random(args.seed ^ 0x5A17B47C)
    for binding in bindings:
        matched_env = environment.copy()
        matched_env.update({
            "LC_ALL": "C", "LANG": "C", "OMP_DYNAMIC": "FALSE",
            "OMP_PROC_BIND": binding, "OMP_PLACES": "cores",
        })
        for workers in workers_list:
            jobs = workers
            for bits in bits_list:
                for operation in ("multiply", "fma", "rec_sqrt"):
                    if operation == "rec_sqrt":
                        methods = ["ppf_outer", "mpfr_outer"]
                    elif operation == "fma":
                        methods = ["ppf_fma_outer", "mpfr_fma_outer"]
                    else:
                        methods = ["ppf_mul_outer", "mpfr_mul_outer"]
                    for repeat in range(1, repetitions + 1):
                        order = list(methods)
                        matched_rng.shuffle(order)
                        for position, method in enumerate(order, 1):
                            key = (bits, operation, workers, binding,
                                   repeat, method)
                            if key in matched_done:
                                continue
                            if operation in {"multiply", "fma"}:
                                payload = run_json_output(
                                    [str(speed_binary), method, str(bits),
                                     str(workers), str(jobs), "1"], matched_env, args.case_timeout_seconds)
                                seconds = float(payload["seconds"])
                            else:
                                payload = run_json_output(
                                    [str(recsqrt_matched_binary), method,
                                     str(bits), str(workers), str(jobs), "1"],
                                    matched_env, args.case_timeout_seconds)
                                seconds = float(payload["seconds_per_batch"])
                            matched_rows.append({
                                "bits": bits, "operation": operation,
                                "method": method, "workers": workers,
                                "jobs": jobs, "repeat": repeat,
                                "order": position, "binding": binding,
                                "seconds": seconds,
                            })
                            matched_done.add(key)
                            write_csv(matched_path, matched_rows)
                    print(f"completed matched workers operation={operation} "
                          f"bits={bits:,} workers={workers} binding={binding}",
                          flush=True)
    write_csv(matched_path, matched_rows)

    summaries: list[dict] = []
    group_keys = sorted({
        (row["bits"], row["operation"], row["method"],
         row["method_workers"], row["binding"])
        for row in rows
    })
    for bits, operation, method, method_workers, binding in group_keys:
        values = [row["seconds"] for row in rows
                  if row["bits"] == bits
                  and row["operation"] == operation
                  and row["method"] == method
                  and row["method_workers"] == method_workers
                  and row["binding"] == binding]
        summaries.append({
            "bits": bits,
            "operation": operation,
            "method": method,
            "method_workers": method_workers,
            "binding": binding,
            "runs": len(values),
            "median_seconds": median(values),
            "mad_seconds": mad(values),
            "minimum_seconds": min(values),
            "maximum_seconds": max(values),
        })
    write_csv(out / "summary.csv", summaries)

    comparisons: list[dict] = []
    recommendations: list[dict] = []
    case_keys = sorted({
        (row["bits"], row["operation"], row["binding"],
         max(r["method_workers"] for r in rows
             if r["bits"] == row["bits"]
             and r["operation"] == row["operation"]
             and r["binding"] == row["binding"]
             and r["method"] in {"ppf_rank3", "ppf_rank5", "ppf_window", "ppf_auto"}))
        for row in rows if row["method"] == "ppf_auto"
    })
    # The prior set-comprehension can merge cases with different requested
    # worker counts. Reconstruct them explicitly from the auto rows.
    case_keys = sorted({
        (row["bits"], row["operation"], row["binding"],
         row["method_workers"])
        for row in rows if row["method"] == "ppf_auto"
    })

    for bits, operation, binding, workers in case_keys:
        group = [row for row in rows
                 if row["bits"] == bits
                 and row["operation"] == operation
                 and row["binding"] == binding
                 and (row["method_workers"] == workers
                      or row["method"] in {"ppf_full_1t", "mpfr_1t"})]
        digest = hashlib.sha256(
            f"{bits}:{operation}:{binding}:{workers}".encode()).digest()
        seed = args.seed ^ int.from_bytes(digest[:8], "big")
        available = {row["method"] for row in group}
        pair_candidates = [
            ("ppf_rank5", "ppf_rank3"),
            ("ppf_window", "ppf_rank3"),
            ("ppf_auto", "ppf_rank3"),
            ("ppf_full_1t", "mpfr_1t"),
            ("ppf_rank3", "mpfr_1t"),
            ("ppf_rank5", "mpfr_1t"),
            ("ppf_auto", "mpfr_1t"),
        ]
        pair_candidates = [pair for pair in pair_candidates
                           if pair[0] in available and pair[1] in available]
        for index, (first, second) in enumerate(pair_candidates):
            row = comparison_row(group, first, second,
                                 seed + index, args.bootstrap_draws)
            row.update({
                "bits": bits,
                "operation": operation,
                "binding": binding,
                "requested_workers": workers,
            })
            comparisons.append(row)
        selected, reason = stable_recommendation(
            group, operation, seed + 101, args.bootstrap_draws)
        recommendations.append({
            "bits": bits,
            "operation": operation,
            "binding": binding,
            "workers": workers,
            "recommended_method": selected,
            "reason": reason,
        })

    write_csv(out / "paired_comparisons.csv", comparisons)
    write_csv(out / "recommendations.csv", recommendations)
    (out / "recommendations.json").write_text(
        json.dumps(recommendations, indent=2) + "\n", encoding="utf-8")

    matched_summaries: list[dict] = []
    matched_comparisons: list[dict] = []
    matched_cases = sorted({
        (row["bits"], row["operation"], row["workers"], row["binding"])
        for row in matched_rows
    })
    for bits, operation, workers, binding in matched_cases:
        group = [row for row in matched_rows
                 if row["bits"] == bits and row["operation"] == operation
                 and row["workers"] == workers and row["binding"] == binding]
        methods = sorted({row["method"] for row in group})
        for method in methods:
            values = [row["seconds"] for row in group if row["method"] == method]
            matched_summaries.append({
                "bits": bits, "operation": operation,
                "workers": workers, "jobs": workers,
                "binding": binding, "method": method,
                "runs": len(values), "median_seconds": median(values),
                "mad_seconds": mad(values),
            })
        if operation == "rec_sqrt":
            first, second = "ppf_outer", "mpfr_outer"
        elif operation == "fma":
            first, second = "ppf_fma_outer", "mpfr_fma_outer"
        else:
            first, second = "ppf_mul_outer", "mpfr_mul_outer"
        digest = hashlib.sha256(
            f"matched:{bits}:{operation}:{workers}:{binding}".encode()).digest()
        comparison = comparison_row(
            group, first, second,
            args.seed ^ int.from_bytes(digest[:8], "big"),
            args.bootstrap_draws)
        comparison.update({
            "bits": bits, "operation": operation,
            "workers": workers, "jobs": workers, "binding": binding,
        })
        matched_comparisons.append(comparison)
    write_csv(out / "matched_workers_summary.csv", matched_summaries)
    write_csv(out / "matched_workers_comparisons.csv", matched_comparisons)

    summary_index = {
        (row["bits"], row["operation"], row["method"],
         row["method_workers"], row["binding"]): row
        for row in summaries
    }
    comparison_index = {
        (row["bits"], row["operation"], row["binding"],
         row["requested_workers"], row["first_method"],
         row["second_method"]): row
        for row in comparisons
    }

    lines = [
        "# Rank-adaptive product and fused planner",
        "",
        "Every timed result is checked against MPFR before it is recorded. "
        "A speedup above 1 favors the first method named.",
        "",
        "## Product-rank comparison",
        "",
        "| Bits | Op | Workers | Binding | Rank 3 | Rank 5 | "
        "Rank-5 / rank-3 | 95% interval | W-L-T | Recommendation |",
        "|---:|---|---:|---|---:|---:|---:|---:|---:|---|",
    ]
    for recommendation in recommendations:
        bits = recommendation["bits"]
        operation = recommendation["operation"]
        if operation not in {"multiply", "square", "fma"}:
            continue
        binding = recommendation["binding"]
        workers = recommendation["workers"]
        rank3 = summary_index[(bits, operation, "ppf_rank3",
                               workers, binding)]
        rank5 = summary_index[(bits, operation, "ppf_rank5",
                               workers, binding)]
        comparison = comparison_index[(bits, operation, binding, workers,
                                       "ppf_rank5", "ppf_rank3")]
        lines.append(
            f"| {bits:,} | {operation} | {workers} | {binding} | "
            f"{rank3['median_seconds']:.6f} s | "
            f"{rank5['median_seconds']:.6f} s | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} | "
            f"{recommendation['recommended_method']} |"
        )

    lines += [
        "",
        "## Equal-core one-thread arithmetic",
        "",
        "Both implementations receive one worker. This is the strict "
        "equal-resource arithmetic comparison.",
        "",
        "| Bits | Operation | Polyphase 1T / MPFR 1T | 95% interval | W-L-T |",
        "|---:|---|---:|---:|---:|",
    ]
    seen_equal_core = set()
    for recommendation in recommendations:
        if recommendation["operation"] not in {"multiply", "square", "fma"}:
            continue
        equal_key = (recommendation["bits"], recommendation["operation"],
                     recommendation["binding"])
        if equal_key in seen_equal_core:
            continue
        seen_equal_core.add(equal_key)
        comparison = comparison_index[(
            recommendation["bits"], recommendation["operation"],
            recommendation["binding"], recommendation["workers"],
            "ppf_full_1t", "mpfr_1t")]
        lines.append(
            f"| {recommendation['bits']:,} | {recommendation['operation']} | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} |"
        )

    lines += [
        "",
        "## Recommended product latency versus MPFR",
        "",
        "These rows compare one internally threaded Polyphase operation with "
        "one ordinary MPFR call. They are latency comparisons, not equal-worker "
        "batch comparisons.",
        "",
        "| Bits | Op | Workers | Selected method | Paired speedup vs MPFR | "
        "95% interval | W-L-T |",
        "|---:|---|---:|---|---:|---:|---:|",
    ]
    for recommendation in recommendations:
        if recommendation["operation"] not in {"multiply", "square", "fma"}:
            continue
        bits = recommendation["bits"]
        operation = recommendation["operation"]
        binding = recommendation["binding"]
        workers = recommendation["workers"]
        selected = recommendation["recommended_method"]
        comparison = comparison_index[(bits, operation, binding, workers,
                                       selected, "mpfr_1t")]
        lines.append(
            f"| {bits:,} | {operation} | {workers} | {selected} | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} |"
        )

    lines += [
        "",
        "## Reciprocal-square-root product rank",
        "",
        "| Bits | Workers | Binding | Rank 3 | Rank 5 | "
        "Rank-5 / rank-3 | 95% interval | W-L-T | Recommendation |",
        "|---:|---:|---|---:|---:|---:|---:|---:|---|",
    ]
    for recommendation in recommendations:
        if recommendation["operation"] != "rec_sqrt":
            continue
        bits = recommendation["bits"]
        binding = recommendation["binding"]
        workers = recommendation["workers"]
        rank3 = summary_index[(bits, "rec_sqrt", "ppf_rank3",
                               workers, binding)]
        rank5 = summary_index[(bits, "rec_sqrt", "ppf_rank5",
                               workers, binding)]
        comparison = comparison_index[(bits, "rec_sqrt", binding, workers,
                                       "ppf_rank5", "ppf_rank3")]
        lines.append(
            f"| {bits:,} | {workers} | {binding} | "
            f"{rank3['median_seconds']:.6f} s | "
            f"{rank5['median_seconds']:.6f} s | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} | "
            f"{recommendation['recommended_method']} |"
        )

    lines += [
        "",
        "## Recommended reciprocal-root latency versus MPFR",
        "",
        "| Bits | Workers | Selected method | Paired speedup vs MPFR | "
        "95% interval | W-L-T |",
        "|---:|---:|---|---:|---:|---:|",
    ]
    for recommendation in recommendations:
        if recommendation["operation"] != "rec_sqrt":
            continue
        bits = recommendation["bits"]
        binding = recommendation["binding"]
        workers = recommendation["workers"]
        selected = recommendation["recommended_method"]
        comparison = comparison_index[(bits, "rec_sqrt", binding, workers,
                                       selected, "mpfr_1t")]
        lines.append(
            f"| {bits:,} | {workers} | {selected} | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} |"
        )

    lines += [
        "",
        "## Equal-worker batch throughput",
        "",
        "Both libraries receive the same outer worker count and the same "
        "number of independent jobs. These are the fair equal-resource "
        "throughput rows.",
        "",
        "| Bits | Operation | Workers/jobs | Paired Polyphase speedup | "
        "95% interval | W-L-T |",
        "|---:|---|---:|---:|---:|---:|",
    ]
    for comparison in matched_comparisons:
        lines.append(
            f"| {comparison['bits']:,} | {comparison['operation']} | "
            f"{comparison['workers']} | "
            f"{comparison['paired_median_speedup']:.3f}x | "
            f"[{comparison['bootstrap_95_low']:.3f}, "
            f"{comparison['bootstrap_95_high']:.3f}] | "
            f"{comparison['wins']}-{comparison['losses']}-"
            f"{comparison['ties']} |"
        )

    lines += [
        "",
        "## Portable policy",
        "",
        "The portable automatic policy uses rank 5 for sufficiently large "
        "squares and keeps rank 3 for general multiplication and fused "
        "multiply-add. Multiplication and FMA "
        "crossovers were non-monotone because the child products cross GMP's "
        "own internal thresholds. Explicit rank selection and the tuner are "
        "therefore retained.",
        "",
        "The certified window candidate is retained as a correctness and "
        "future high-product experiment. It is not selected automatically "
        "unless target-machine evidence establishes a stable advantage.",
    ]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n",
                                    encoding="utf-8")

    environment_record = {
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "bits": bits_list,
        "workers": workers_list,
        "bindings": bindings,
        "repetitions": repetitions,
        "inner_repetitions": inner,
        "bootstrap_draws": args.bootstrap_draws,
        "case_timeout_seconds": args.case_timeout_seconds,
        "seed": args.seed,
        "platform": platform.platform(),
        "uname": subprocess.run(["uname", "-a"], text=True,
                                capture_output=True).stdout.strip(),
        "gcc": subprocess.run(["gcc", "--version"], text=True,
                              capture_output=True).stdout.splitlines()[0],
    }
    (out / "environment.json").write_text(
        json.dumps(environment_record, indent=2) + "\n", encoding="utf-8")
    print("\n" + (out / "REPORT.md").read_text(encoding="utf-8"),
          flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
