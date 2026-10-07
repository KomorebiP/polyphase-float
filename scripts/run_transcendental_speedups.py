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
from pathlib import Path
import platform
import random
import statistics
import subprocess
from typing import Iterable

OPS = [
    "log", "log1p", "log2", "log10", "log2p1", "log10p1",
    "exp", "expm1", "exp2", "exp10", "exp2m1", "exp10m1",
    "pow", "pow_ui",
]


def median(values: list[float]) -> float:
    return float(statistics.median(values))


def mad(values: list[float]) -> float:
    m = median(values)
    return median([abs(x - m) for x in values])


def percentile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return float("nan")
    if len(ordered) == 1:
        return ordered[0]
    position = q * (len(ordered) - 1)
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high:
        return ordered[low]
    return ordered[low] * (high - position) + ordered[high] * (position - low)


def bootstrap_ci(values: list[float], draws: int, seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    n = len(values)
    samples = [median([values[rng.randrange(n)] for _ in range(n)]) for _ in range(draws)]
    return percentile(samples, 0.025), percentile(samples, 0.975)


def sign_p(wins: int, losses: int) -> float:
    n = wins + losses
    if n == 0:
        return 1.0
    k = min(wins, losses)
    tail = sum(math.comb(n, i) for i in range(k + 1)) / (2 ** n)
    return min(1.0, 2.0 * tail)


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


def loops_for_bits(bits: int, override: int) -> int:
    if override > 0:
        return override
    if bits <= 1000:
        return 20
    if bits <= 5000:
        return 5
    if bits <= 10000:
        return 3
    return 1


def invoke(binary: Path, method: str, bits: int, loops: int, env: dict[str, str]) -> dict:
    completed = subprocess.run(
        [str(binary), method, str(bits), str(loops)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        env=env,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{method} failed with {completed.returncode}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    for line in reversed(completed.stdout.splitlines()):
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(payload, dict):
            return payload
    raise RuntimeError(f"No JSON output from {method}\n{completed.stdout}\n{completed.stderr}")


def parse_ops(text: str) -> list[str]:
    if not text.strip():
        return OPS[:]
    requested = [part.strip() for part in text.split(",") if part.strip()]
    unknown = [op for op in requested if op not in OPS]
    if unknown:
        raise ValueError(f"unknown operations: {', '.join(unknown)}")
    return requested


def command_line(command: list[str]) -> str:
    try:
        completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL, check=False)
    except OSError:
        return ""
    if completed.returncode != 0:
        return ""
    return completed.stdout.splitlines()[0].strip() if completed.stdout else ""


def main() -> int:
    parser = argparse.ArgumentParser(description="Correctness-gated transcendental speed benchmark")
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/transcendental_speedups")
    parser.add_argument("--bits", default="1000,5000,10000,20000")
    parser.add_argument("--ops", default="")
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--loops", type=int, default=0,
                        help="calls per process; zero selects a precision-dependent count")
    parser.add_argument("--bootstrap-draws", type=int, default=20000)
    parser.add_argument("--seed", type=int, default=20260811)
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    raw_path = out / "raw.csv"
    if args.fresh and raw_path.exists():
        raw_path.unlink()

    binary = root / "bin" / "benchmark_transcendental_method"
    if not binary.exists():
        raise FileNotFoundError(binary)
    bit_values = [int(x) for x in args.bits.split(",") if x.strip()]
    operations = parse_ops(args.ops)
    repetitions = max(3, args.repetitions)
    methods = [f"{family}_{op}" for op in operations for family in ("ppf", "mpfr")]
    rng = random.Random(args.seed)

    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(root / "lib") + (
        ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else ""
    )
    env["OMP_NUM_THREADS"] = "1"

    raw: list[dict] = []
    if raw_path.exists():
        with raw_path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                try:
                    parsed = {
                        "bits": int(row["bits"]),
                        "repeat": int(row["repeat"]),
                        "order": int(row["order"]),
                        "method": row["method"],
                        "seconds_per_call": float(row["seconds_per_call"]),
                        "loops": int(row["loops"]),
                        "ternary": int(row["ternary"]),
                    }
                except (KeyError, ValueError):
                    continue
                if parsed["bits"] in bit_values and parsed["method"] in methods:
                    raw.append(parsed)
    completed_keys = {(r["bits"], r["repeat"], r["method"]) for r in raw}

    print("Correctness-gated independent transcendental speed benchmark", flush=True)
    print(f"bits={bit_values}, operations={operations}, repetitions={repetitions}", flush=True)

    # Warm each implementation outside measured rounds.  The C binary also
    # verifies value and ternary agreement against the opposite implementation.
    for bits in bit_values:
        order = methods[:]
        rng.shuffle(order)
        for method in order:
            invoke(binary, method, bits, 1, env)

    for bits in bit_values:
        loops = loops_for_bits(bits, args.loops)
        print(f"\n{bits:,} bits, loops={loops}", flush=True)
        for repeat in range(1, repetitions + 1):
            order = methods[:]
            rng.shuffle(order)
            for position, method in enumerate(order, 1):
                key = (bits, repeat, method)
                if key in completed_keys:
                    continue
                payload = invoke(binary, method, bits, loops, env)
                row = {
                    "bits": bits,
                    "repeat": repeat,
                    "order": position,
                    "method": method,
                    "seconds_per_call": float(payload["seconds_per_call"]),
                    "loops": int(payload["loops"]),
                    "ternary": int(payload["ternary"]),
                }
                raw.append(row)
                completed_keys.add(key)
                raw.sort(key=lambda r: (r["bits"], r["repeat"], r["order"]))
                write_csv(raw_path, raw)
                print(f"  round={repeat} {method}: {row['seconds_per_call']:.9f}s", flush=True)
    write_csv(raw_path, raw)

    summary: list[dict] = []
    for bits in bit_values:
        for method in methods:
            values = [
                r["seconds_per_call"] for r in raw
                if r["bits"] == bits and r["method"] == method
            ]
            summary.append({
                "bits": bits,
                "method": method,
                "runs": len(values),
                "median_seconds": median(values),
                "mad_seconds": mad(values),
                "min_seconds": min(values),
                "max_seconds": max(values),
            })
    write_csv(out / "summary.csv", summary)

    lookup = {
        (r["bits"], r["repeat"], r["method"]): r["seconds_per_call"]
        for r in raw
    }
    paired: list[dict] = []
    for bits in bit_values:
        for operation in operations:
            ratios: list[float] = []
            wins = losses = ties = 0
            for repeat in range(1, repetitions + 1):
                ppf_seconds = lookup[(bits, repeat, f"ppf_{operation}")]
                mpfr_seconds = lookup[(bits, repeat, f"mpfr_{operation}")]
                ratios.append(mpfr_seconds / ppf_seconds)
                if ppf_seconds < mpfr_seconds:
                    wins += 1
                elif ppf_seconds > mpfr_seconds:
                    losses += 1
                else:
                    ties += 1
            digest = int.from_bytes(hashlib.sha256(operation.encode()).digest()[:8], "big")
            low, high = bootstrap_ci(ratios, args.bootstrap_draws,
                                     args.seed ^ bits ^ digest)
            paired.append({
                "bits": bits,
                "operation": operation,
                "pairs": len(ratios),
                "paired_median_speedup": median(ratios),
                "bootstrap_95_low": low,
                "bootstrap_95_high": high,
                "wins": wins,
                "losses": losses,
                "ties": ties,
                "sign_test_two_sided_p": sign_p(wins, losses),
            })
    write_csv(out / "paired_comparisons.csv", paired)

    smap = {(r["bits"], r["method"]): r for r in summary}
    pmap = {(r["bits"], r["operation"]): r for r in paired}
    lines = [
        "# Independent transcendental speed benchmark",
        "",
        "Every timed row passes an untimed value-and-ternary correctness gate against MPFR.",
        "Both implementations use one worker, identical destination precision, and nearest-even rounding.",
        "A speedup above 1 favors Polyphase.",
        "",
        "Benchmark inputs are fixed and reproducible: log-family calls use 1.25,",
        "log1p- and exp-family calls use 0.125, general power uses 1.25^3.25,",
        "and integer power uses 1.25^17. Natural exp therefore includes a sparse",
        "dyadic case, while exp2/exp10 exercise dense reduced arguments.",
        "",
        "| Bits | Operation | Polyphase median | MPFR median | Paired speedup | Bootstrap 95% | W-L-T |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    for bits in bit_values:
        for operation in operations:
            ppf_row = smap[(bits, f"ppf_{operation}")]
            mpfr_row = smap[(bits, f"mpfr_{operation}")]
            comparison = pmap[(bits, operation)]
            lines.append(
                f"| {bits:,} | {operation} | {ppf_row['median_seconds']:.9f} s | "
                f"{mpfr_row['median_seconds']:.9f} s | "
                f"{comparison['paired_median_speedup']:.3f}x | "
                f"[{comparison['bootstrap_95_low']:.3f}, "
                f"{comparison['bootstrap_95_high']:.3f}] | "
                f"{comparison['wins']}-{comparison['losses']}-{comparison['ties']} |"
            )
    lines += [
        "",
        "## Interpretation",
        "",
        "The fixed-point backend removes repeated floating-object allocation and general",
        "division from the series loops. It uses measured root/series policies for logarithms",
        "and a sparse/dense split between bit-burst and blocked Taylor reconstruction for",
        "exponentials. The original interval implementation remains a native correctness",
        "fallback; no implemented operation delegates to MPFR.",
        "",
        "Natural exp on 0.125 is a favorable sparse-dyadic workload. The exp2, exp10,",
        "and general-power rows are the more conservative dense-reduction checks.",
    ]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    environment = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "compiler": command_line(["gcc", "--version"]),
        "gmp_version": command_line(["pkg-config", "--modversion", "gmp"]),
        "mpfr_version": command_line(["pkg-config", "--modversion", "mpfr"]),
        "cpu_model": command_line(["bash", "-lc", "lscpu | sed -n 's/^Model name:[[:space:]]*//p' | head -1"]),
        "visible_cpus": os.cpu_count(),
        "bits": bit_values,
        "operations": operations,
        "repetitions": repetitions,
        "loops_override": args.loops,
        "bootstrap_draws": args.bootstrap_draws,
        "omp_num_threads": env["OMP_NUM_THREADS"],
    }
    (out / "environment.json").write_text(
        json.dumps(environment, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"\nReport: {out / 'REPORT.md'}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
