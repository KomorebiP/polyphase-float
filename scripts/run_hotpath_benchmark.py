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
import random
import statistics
import subprocess
import time
from typing import Iterable

OPS = [
    "set_z", "get_z", "get_str", "fmod", "remainder",
    "cbrt", "root7", "rec_sqrt", "hypot", "div",
]


def med(values: Iterable[float]) -> float:
    return float(statistics.median(list(values)))


def mad(values: list[float]) -> float:
    m = med(values)
    return med([abs(x - m) for x in values])


def percentile(values: list[float], q: float) -> float:
    x = sorted(values)
    if not x:
        return float("nan")
    if len(x) == 1:
        return x[0]
    p = q * (len(x) - 1)
    lo = int(math.floor(p)); hi = int(math.ceil(p))
    if lo == hi:
        return x[lo]
    return x[lo] * (hi - p) + x[hi] * (p - lo)


def bootstrap_median_ci(values: list[float], draws: int, seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    n = len(values)
    samples = []
    for _ in range(draws):
        samples.append(med([values[rng.randrange(n)] for _ in range(n)]))
    return percentile(samples, 0.025), percentile(samples, 0.975)


def sign_p(wins: int, losses: int) -> float:
    n = wins + losses
    if n == 0:
        return 1.0
    k = min(wins, losses)
    tail = sum(math.comb(n, i) for i in range(k + 1)) / (2 ** n)
    return min(1.0, 2.0 * tail)


def run_method(binary: Path, method: str, bits: int, inner: int, env: dict[str, str]) -> dict:
    proc = subprocess.run(
        [str(binary), method, str(bits), str(inner)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"{method} failed ({proc.returncode})\n{proc.stdout}\n{proc.stderr}")
    for line in reversed(proc.stdout.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            return value
    raise RuntimeError(f"No JSON from {method}:\n{proc.stdout}\n{proc.stderr}")


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields: list[str] = []
    seen: set[str] = set()
    for row in rows:
        for key in row:
            if key not in seen:
                seen.add(key); fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader(); writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/hotpath_speedups")
    parser.add_argument("--bits", default="100000,1000000,5000000")
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--inner", type=int, default=1)
    parser.add_argument("--bootstrap", type=int, default=20000)
    parser.add_argument("--seed", type=int, default=20260810)
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    if args.fresh and (out / "raw.csv").exists():
        (out / "raw.csv").unlink()
    binary = root / "bin/benchmark_hotpath_method"
    bit_values = [int(x) for x in args.bits.split(",") if x.strip()]
    reps = max(3, args.repetitions)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(root / "lib") + (":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
    env.setdefault("OMP_PROC_BIND", "close")
    env.setdefault("OMP_PLACES", "cores")
    rng = random.Random(args.seed)
    methods = [f"{family}_{op}" for op in OPS for family in ("ppf", "mpfr")]

    print("Correctness-gated hot-path benchmark", flush=True)
    print(f"Precisions: {bit_values}; measured repetitions: {reps}", flush=True)

    # Untimed warmups.
    for bits in bit_values:
        warm = methods[:]; rng.shuffle(warm)
        for method in warm:
            run_method(binary, method, bits, 1, env)

    raw: list[dict] = []
    raw_path = out / "raw.csv"
    if raw_path.exists():
        with raw_path.open(newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
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
                except (KeyError, TypeError, ValueError):
                    continue
                if parsed["bits"] in bit_values and 1 <= parsed["repeat"] <= reps and parsed["method"] in methods:
                    raw.append(parsed)
        print(f"Resuming from {len(raw)} completed timing rows.", flush=True)
    completed = {(r["bits"], r["repeat"], r["method"]) for r in raw}

    for bits in bit_values:
        print(f"\n{bits:,} bits", flush=True)
        for repeat in range(1, reps + 1):
            order = methods[:]; rng.shuffle(order)
            for position, method in enumerate(order, 1):
                if (bits, repeat, method) in completed:
                    continue
                result = run_method(binary, method, bits, args.inner, env)
                row = {
                    "bits": bits, "repeat": repeat, "order": position,
                    "method": method,
                    "seconds_per_call": float(result["seconds_per_call"]),
                    "loops": int(result["loops"]),
                    "ternary": int(result["ternary"]),
                }
                raw.append(row)
                completed.add((bits, repeat, method))
                raw.sort(key=lambda r: (r["bits"], r["repeat"], r["order"]))
                write_csv(raw_path, raw)
                print(f"  round={repeat} {method}: {row['seconds_per_call']:.9f}s", flush=True)
    write_csv(raw_path, raw)

    summary: list[dict] = []
    for bits in bit_values:
        for method in methods:
            values = [r["seconds_per_call"] for r in raw if r["bits"] == bits and r["method"] == method]
            summary.append({
                "bits": bits, "method": method, "runs": len(values),
                "median_seconds": med(values), "mad_seconds": mad(values),
                "min_seconds": min(values), "max_seconds": max(values),
            })
    write_csv(out / "summary.csv", summary)

    lookup = {(r["bits"], r["repeat"], r["method"]): r["seconds_per_call"] for r in raw}
    paired: list[dict] = []
    for bits in bit_values:
        for op in OPS:
            ratios = []
            wins = losses = ties = 0
            for repeat in range(1, reps + 1):
                p = lookup[(bits, repeat, f"ppf_{op}")]
                m = lookup[(bits, repeat, f"mpfr_{op}")]
                ratios.append(m / p)
                if p < m: wins += 1
                elif p > m: losses += 1
                else: ties += 1
            seed = args.seed ^ bits ^ int.from_bytes(hashlib.sha256(op.encode()).digest()[:8], "big")
            lo, hi = bootstrap_median_ci(ratios, args.bootstrap, seed)
            paired.append({
                "bits": bits, "operation": op, "pairs": len(ratios),
                "paired_median_speedup": med(ratios),
                "bootstrap_95_low": lo, "bootstrap_95_high": hi,
                "wins": wins, "losses": losses, "ties": ties,
                "sign_test_p": sign_p(wins, losses),
            })
    write_csv(out / "paired_comparisons.csv", paired)

    smap = {(r["bits"], r["method"]): r for r in summary}
    lines = [
        "# Independent floating-point hot-path benchmark", "",
        "Every timed implementation is checked against MPFR before the timing row is retained.",
        "A speedup above 1 favors Polyphase. Timings are medians of one operation, not cumulative suite time.", "",
        "| Bits | Operation | Polyphase median | MPFR median | Paired speedup | Bootstrap 95% | W-L-T |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    pmap = {(r["bits"], r["operation"]): r for r in paired}
    for bits in bit_values:
        for op in OPS:
            p = smap[(bits, f"ppf_{op}")]["median_seconds"]
            m = smap[(bits, f"mpfr_{op}")]["median_seconds"]
            c = pmap[(bits, op)]
            lines.append(
                f"| {bits:,} | {op} | {p:.9f} s | {m:.9f} s | "
                f"{c['paired_median_speedup']:.3f}x | "
                f"[{c['bootstrap_95_low']:.3f}, {c['bootstrap_95_high']:.3f}] | "
                f"{c['wins']}-{c['losses']}-{c['ties']} |"
            )
    lines += ["", "## Interpretation", "",
              "The benchmark uses one worker for both libraries. It does not include process startup, input construction, or correctness checking in the timed region.",
              "Short decimal output requests use 128 significant digits. Root and remainder inputs are deterministic full-precision dyadic values."]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    environment = {
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "bits": bit_values, "repetitions": reps, "inner_repetitions": args.inner,
        "seed": args.seed,
        "uname": subprocess.run(["uname", "-a"], text=True, stdout=subprocess.PIPE).stdout.strip(),
        "compiler": subprocess.run(["cc", "--version"], text=True, stdout=subprocess.PIPE).stdout.splitlines()[0],
        "contract": "same destination precision, nearest-even rounding, correctness-gated against MPFR",
    }
    (out / "environment.json").write_text(json.dumps(environment, indent=2) + "\n", encoding="utf-8")
    print("\n" + (out / "REPORT.md").read_text(encoding="utf-8"), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
